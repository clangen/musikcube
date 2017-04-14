/*
  This file is part of libmicrohttpd
  Copyright (C) 2007-2017 Daniel Pittman and Christian Grothoff

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

*/

/**
 * @file microhttpd/daemon.c
 * @brief  A minimal-HTTP server library
 * @author Daniel Pittman
 * @author Christian Grothoff
 * @author Karlson2k (Evgeny Grin)
 */
#include "platform.h"
#include "mhd_threads.h"
#include "internal.h"
#include "response.h"
#include "connection.h"
#include "memorypool.h"
#include "mhd_limits.h"
#include "autoinit_funcs.h"
#include "mhd_mono_clock.h"
#include "mhd_locks.h"
#include "mhd_sockets.h"
#include "mhd_itc.h"
#include "mhd_compat.h"

#if HAVE_SEARCH_H
#include <search.h>
#else
#include "tsearch.h"
#endif

#ifdef HTTPS_SUPPORT
#include "connection_https.h"
#include <gcrypt.h>
#endif /* HTTPS_SUPPORT */

#ifdef LINUX
#include <sys/sendfile.h>
#endif

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif /* !WIN32_LEAN_AND_MEAN */
#include <windows.h>
#endif

/**
 * Default connection limit.
 */
#ifdef MHD_POSIX_SOCKETS
#define MHD_MAX_CONNECTIONS_DEFAULT (FD_SETSIZE - 4)
#else
#define MHD_MAX_CONNECTIONS_DEFAULT (FD_SETSIZE - 2)
#endif

/**
 * Default memory allowed per connection.
 */
#define MHD_POOL_SIZE_DEFAULT (32 * 1024)

/**
 * Print extra messages with reasons for closing
 * sockets? (only adds non-error messages).
 */
#define DEBUG_CLOSE MHD_NO

/**
 * Print extra messages when establishing
 * connections? (only adds non-error messages).
 */
#define DEBUG_CONNECT MHD_NO


/* Forward declarations. */

/**
 * Close all connections for the daemon.
 * Must only be called when MHD_Daemon::shutdown was set to #MHD_YES.
 * @remark To be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param daemon daemon to close down
 */
static void
close_all_connections (struct MHD_Daemon *daemon);

#ifdef EPOLL_SUPPORT

/**
 * Do epoll()-based processing (this function is allowed to
 * block if @a may_block is set to #MHD_YES).
 *
 * @param daemon daemon to run poll loop for
 * @param may_block #MHD_YES if blocking, #MHD_NO if non-blocking
 * @return #MHD_NO on serious errors, #MHD_YES on success
 */
static int
MHD_epoll (struct MHD_Daemon *daemon,
	   int may_block);

#endif /* EPOLL_SUPPORT */

/**
 * Default implementation of the panic function,
 * prints an error message and aborts.
 *
 * @param cls unused
 * @param file name of the file with the problem
 * @param line line number with the problem
 * @param reason error message with details
 */
static void
mhd_panic_std (void *cls,
	       const char *file,
	       unsigned int line,
	       const char *reason)
{
#ifdef HAVE_MESSAGES
  fprintf (stderr,
           _("Fatal error in GNU libmicrohttpd %s:%u: %s\n"),
	   file,
           line,
           reason);
#endif
  abort ();
}


/**
 * Handler for fatal errors.
 */
MHD_PanicCallback mhd_panic;

/**
 * Closure argument for #mhd_panic.
 */
void *mhd_panic_cls;

#ifdef _WIN32
/**
 * Track initialization of winsock
 */
static int mhd_winsock_inited_ = 0;
#endif


/**
 * Trace up to and return master daemon. If the supplied daemon
 * is a master, then return the daemon itself.
 *
 * @param daemon handle to a daemon
 * @return master daemon handle
 */
static struct MHD_Daemon*
MHD_get_master (struct MHD_Daemon *daemon)
{
  while (NULL != daemon->master)
    daemon = daemon->master;
  return daemon;
}


/**
 * Maintain connection count for single address.
 */
struct MHD_IPCount
{
  /**
   * Address family. AF_INET or AF_INET6 for now.
   */
  int family;

  /**
   * Actual address.
   */
  union
  {
    /**
     * IPv4 address.
     */
    struct in_addr ipv4;
#if HAVE_INET6
    /**
     * IPv6 address.
     */
    struct in6_addr ipv6;
#endif
  } addr;

  /**
   * Counter.
   */
  unsigned int count;
};


/**
 * Lock shared structure for IP connection counts and connection DLLs.
 *
 * @param daemon handle to daemon where lock is
 */
static void
MHD_ip_count_lock (struct MHD_Daemon *daemon)
{
  MHD_mutex_lock_chk_(&daemon->per_ip_connection_mutex);
}


/**
 * Unlock shared structure for IP connection counts and connection DLLs.
 *
 * @param daemon handle to daemon where lock is
 */
static void
MHD_ip_count_unlock (struct MHD_Daemon *daemon)
{
  MHD_mutex_unlock_chk_(&daemon->per_ip_connection_mutex);
}


/**
 * Tree comparison function for IP addresses (supplied to tsearch() family).
 * We compare everything in the struct up through the beginning of the
 * 'count' field.
 *
 * @param a1 first address to compare
 * @param a2 second address to compare
 * @return -1, 0 or 1 depending on result of compare
 */
static int
MHD_ip_addr_compare (const void *a1,
                     const void *a2)
{
  return memcmp (a1,
                 a2,
                 offsetof (struct MHD_IPCount,
                           count));
}


/**
 * Parse address and initialize @a key using the address.
 *
 * @param addr address to parse
 * @param addrlen number of bytes in @a addr
 * @param key where to store the parsed address
 * @return #MHD_YES on success and #MHD_NO otherwise (e.g., invalid address type)
 */
static int
MHD_ip_addr_to_key (const struct sockaddr *addr,
		    socklen_t addrlen,
		    struct MHD_IPCount *key)
{
  memset(key,
         0,
         sizeof(*key));

  /* IPv4 addresses */
  if (sizeof (struct sockaddr_in) == addrlen)
    {
      const struct sockaddr_in *addr4 = (const struct sockaddr_in*) addr;

      key->family = AF_INET;
      memcpy (&key->addr.ipv4,
              &addr4->sin_addr,
              sizeof(addr4->sin_addr));
      return MHD_YES;
    }

#if HAVE_INET6
  /* IPv6 addresses */
  if (sizeof (struct sockaddr_in6) == addrlen)
    {
      const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6*) addr;

      key->family = AF_INET6;
      memcpy (&key->addr.ipv6,
              &addr6->sin6_addr,
              sizeof(addr6->sin6_addr));
      return MHD_YES;
    }
#endif

  /* Some other address */
  return MHD_NO;
}


/**
 * Check if IP address is over its limit in terms of the number
 * of allowed concurrent connections.  If the IP is still allowed,
 * increments the connection counter.
 *
 * @param daemon handle to daemon where connection counts are tracked
 * @param addr address to add (or increment counter)
 * @param addrlen number of bytes in @a addr
 * @return Return #MHD_YES if IP below limit, #MHD_NO if IP has surpassed limit.
 *   Also returns #MHD_NO if fails to allocate memory.
 */
static int
MHD_ip_limit_add (struct MHD_Daemon *daemon,
		  const struct sockaddr *addr,
		  socklen_t addrlen)
{
  struct MHD_IPCount *key;
  void **nodep;
  void *node;
  int result;

  daemon = MHD_get_master (daemon);
  /* Ignore if no connection limit assigned */
  if (0 == daemon->per_ip_connection_limit)
    return MHD_YES;

  if (NULL == (key = malloc (sizeof(*key))))
    return MHD_NO;

  /* Initialize key */
  if (MHD_NO == MHD_ip_addr_to_key (addr,
                                    addrlen,
                                    key))
    {
      /* Allow unhandled address types through */
      free (key);
      return MHD_YES;
    }
  MHD_ip_count_lock (daemon);

  /* Search for the IP address */
  if (NULL == (nodep = tsearch (key,
				&daemon->per_ip_connection_count,
				&MHD_ip_addr_compare)))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Failed to add IP connection count node\n"));
#endif
      MHD_ip_count_unlock (daemon);
      free (key);
      return MHD_NO;
    }
  node = *nodep;
  /* If we got an existing node back, free the one we created */
  if (node != key)
    free(key);
  key = (struct MHD_IPCount *) node;
  /* Test if there is room for another connection; if so,
   * increment count */
  result = (key->count < daemon->per_ip_connection_limit) ? MHD_YES : MHD_NO;
  if (MHD_YES == result)
    ++key->count;

  MHD_ip_count_unlock (daemon);
  return result;
}


/**
 * Decrement connection count for IP address, removing from table
 * count reaches 0.
 *
 * @param daemon handle to daemon where connection counts are tracked
 * @param addr address to remove (or decrement counter)
 * @param addrlen number of bytes in @a addr
 */
static void
MHD_ip_limit_del (struct MHD_Daemon *daemon,
		  const struct sockaddr *addr,
		  socklen_t addrlen)
{
  struct MHD_IPCount search_key;
  struct MHD_IPCount *found_key;
  void **nodep;

  daemon = MHD_get_master (daemon);
  /* Ignore if no connection limit assigned */
  if (0 == daemon->per_ip_connection_limit)
    return;
  /* Initialize search key */
  if (MHD_NO == MHD_ip_addr_to_key (addr,
                                    addrlen,
                                    &search_key))
    return;

  MHD_ip_count_lock (daemon);

  /* Search for the IP address */
  if (NULL == (nodep = tfind (&search_key,
			      &daemon->per_ip_connection_count,
			      &MHD_ip_addr_compare)))
    {
      /* Something's wrong if we couldn't find an IP address
       * that was previously added */
      MHD_PANIC (_("Failed to find previously-added IP address\n"));
    }
  found_key = (struct MHD_IPCount *) *nodep;
  /* Validate existing count for IP address */
  if (0 == found_key->count)
    {
      MHD_PANIC (_("Previously-added IP address had counter of zero\n"));
    }
  /* Remove the node entirely if count reduces to 0 */
  if (0 == --found_key->count)
    {
      tdelete (found_key,
	       &daemon->per_ip_connection_count,
	       &MHD_ip_addr_compare);
      free (found_key);
    }

  MHD_ip_count_unlock (daemon);
}


#ifdef HTTPS_SUPPORT
/**
 * Callback for receiving data from the socket.
 *
 * @param connection the MHD_Connection structure
 * @param other where to write received data to
 * @param i maximum size of other (in bytes)
 * @return number of bytes actually received
 */
static ssize_t
recv_tls_adapter (struct MHD_Connection *connection,
                  void *other,
                  size_t i)
{
  ssize_t res;

  if (i > SSIZE_MAX)
    i = SSIZE_MAX;

  res = gnutls_record_recv (connection->tls_session,
                            other,
                            i);
  if ( (GNUTLS_E_AGAIN == res) ||
       (GNUTLS_E_INTERRUPTED == res) )
    {
      MHD_socket_set_error_ (MHD_SCKT_EINTR_);
#ifdef EPOLL_SUPPORT
      if (GNUTLS_E_AGAIN == res)
        connection->epoll_state &= ~MHD_EPOLL_STATE_READ_READY;
#endif
      return -1;
    }
  if (res < 0)
    {
      /* Likely 'GNUTLS_E_INVALID_SESSION' (client communication
	 disrupted); set errno to something caller will interpret
	 correctly as a hard error */
      MHD_socket_set_error_ (MHD_SCKT_ECONNRESET_);
      connection->tls_read_ready = false;
      return res;
    }

#ifdef EPOLL_SUPPORT
  /* If data not available to fill whole buffer - socket is not read ready anymore. */
  if (i > (size_t)res)
    connection->epoll_state &= ~MHD_EPOLL_STATE_READ_READY;
#endif /* EPOLL_SUPPORT */

  /* Check whether TLS buffers still have some unread data. */
  connection->tls_read_ready = ( ((size_t)res == i) &&
                                 (0 != gnutls_record_check_pending (connection->tls_session)) );
  return res;
}


/**
 * Callback for writing data to the socket.
 *
 * @param connection the MHD connection structure
 * @param other data to write
 * @param i number of bytes to write
 * @return actual number of bytes written
 */
static ssize_t
send_tls_adapter (struct MHD_Connection *connection,
                  const void *other,
                  size_t i)
{
  ssize_t res;

  if (i > SSIZE_MAX)
    i = SSIZE_MAX;

  res = gnutls_record_send (connection->tls_session,
                            other,
                            i);
  if ( (GNUTLS_E_AGAIN == res) ||
       (GNUTLS_E_INTERRUPTED == res) )
    {
      MHD_socket_set_error_ (MHD_SCKT_EINTR_);
#ifdef EPOLL_SUPPORT
      if (GNUTLS_E_AGAIN == res)
        connection->epoll_state &= ~MHD_EPOLL_STATE_WRITE_READY;
#endif
      return -1;
    }
  if (res < 0)
    {
      /* some other GNUTLS error, should set 'errno'; as we do not
         really understand the error (not listed in GnuTLS
         documentation explicitly), we set 'errno' to something that
         will cause the connection to fail. */
      MHD_socket_set_error_ (MHD_SCKT_ECONNRESET_);
      return -1;
    }
#ifdef EPOLL_SUPPORT
  /* If NOT all available data was sent - socket is not write ready anymore. */
  if (i > (size_t)res)
    connection->epoll_state &= ~MHD_EPOLL_STATE_WRITE_READY;
#endif /* EPOLL_SUPPORT */
  return res;
}


/**
 * Read and setup our certificate and key.
 *
 * @param daemon handle to daemon to initialize
 * @return 0 on success
 */
static int
MHD_init_daemon_certificate (struct MHD_Daemon *daemon)
{
  gnutls_datum_t key;
  gnutls_datum_t cert;
  int ret;

#if GNUTLS_VERSION_MAJOR >= 3
  if (NULL != daemon->cert_callback)
    {
      gnutls_certificate_set_retrieve_function2 (daemon->x509_cred,
                                                 daemon->cert_callback);
    }
#endif
  if (NULL != daemon->https_mem_trust)
    {
      cert.data = (unsigned char *) daemon->https_mem_trust;
      cert.size = strlen (daemon->https_mem_trust);
      if (gnutls_certificate_set_x509_trust_mem (daemon->x509_cred,
                                                 &cert,
						 GNUTLS_X509_FMT_PEM) < 0)
	{
#ifdef HAVE_MESSAGES
	  MHD_DLOG(daemon,
		   "Bad trust certificate format\n");
#endif
	  return -1;
	}
    }

  if (daemon->have_dhparams)
    {
      gnutls_certificate_set_dh_params (daemon->x509_cred,
                                        daemon->https_mem_dhparams);
    }
  /* certificate & key loaded from memory */
  if ( (NULL != daemon->https_mem_cert) &&
       (NULL != daemon->https_mem_key) )
    {
      key.data = (unsigned char *) daemon->https_mem_key;
      key.size = strlen (daemon->https_mem_key);
      cert.data = (unsigned char *) daemon->https_mem_cert;
      cert.size = strlen (daemon->https_mem_cert);

      if (NULL != daemon->https_key_password) {
#if GNUTLS_VERSION_NUMBER >= 0x030111
        ret = gnutls_certificate_set_x509_key_mem2 (daemon->x509_cred,
                                                    &cert,
                                                    &key,
                                                    GNUTLS_X509_FMT_PEM,
                                                    daemon->https_key_password,
                                                    0);
#else
#ifdef HAVE_MESSAGES
	MHD_DLOG (daemon,
                  _("Failed to setup x509 certificate/key: pre 3.X.X version " \
                    "of GnuTLS does not support setting key password"));
#endif
	return -1;
#endif
      }
      else
        ret = gnutls_certificate_set_x509_key_mem (daemon->x509_cred,
                                                   &cert,
                                                   &key,
                                                   GNUTLS_X509_FMT_PEM);
#ifdef HAVE_MESSAGES
      if (0 != ret)
        MHD_DLOG (daemon,
                  "GnuTLS failed to setup x509 certificate/key: %s\n",
                  gnutls_strerror (ret));
#endif
      return ret;
    }
#if GNUTLS_VERSION_MAJOR >= 3
  if (NULL != daemon->cert_callback)
    return 0;
#endif
#ifdef HAVE_MESSAGES
  MHD_DLOG (daemon,
            "You need to specify a certificate and key location\n");
#endif
  return -1;
}


/**
 * Initialize security aspects of the HTTPS daemon
 *
 * @param daemon handle to daemon to initialize
 * @return 0 on success
 */
static int
MHD_TLS_init (struct MHD_Daemon *daemon)
{
  switch (daemon->cred_type)
    {
    case GNUTLS_CRD_CERTIFICATE:
      if (0 !=
          gnutls_certificate_allocate_credentials (&daemon->x509_cred))
        return GNUTLS_E_MEMORY_ERROR;
      return MHD_init_daemon_certificate (daemon);
    default:
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Error: invalid credentials type %d specified.\n"),
                daemon->cred_type);
#endif
      return -1;
    }
}
#endif /* HTTPS_SUPPORT */


#undef MHD_get_fdset

/**
 * Obtain the `select()` sets for this daemon.
 * Daemon's FDs will be added to fd_sets. To get only
 * daemon FDs in fd_sets, call FD_ZERO for each fd_set
 * before calling this function. FD_SETSIZE is assumed
 * to be platform's default.
 *
 * @param daemon daemon to get sets from
 * @param read_fd_set read set
 * @param write_fd_set write set
 * @param except_fd_set except set
 * @param max_fd increased to largest FD added (if larger
 *               than existing value); can be NULL
 * @return #MHD_YES on success, #MHD_NO if this
 *         daemon was not started with the right
 *         options for this call or any FD didn't
 *         fit fd_set.
 * @ingroup event
 */
int
MHD_get_fdset (struct MHD_Daemon *daemon,
               fd_set *read_fd_set,
               fd_set *write_fd_set,
	       fd_set *except_fd_set,
	       MHD_socket *max_fd)
{
  return MHD_get_fdset2 (daemon,
                         read_fd_set,
                         write_fd_set,
                         except_fd_set,
                         max_fd,
                         _MHD_SYS_DEFAULT_FD_SETSIZE);
}


#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
/**
 * Obtain the select() file descriptor sets for the
 * given @a urh.
 *
 * @param urh upgrade handle to wait for
 * @param[out] rs read set to initialize
 * @param[out] ws write set to initialize
 * @param[out] es except set to initialize
 * @param[out] max_fd maximum FD to update
 * @param fd_setsize value of FD_SETSIZE
 * @return true on success, false on error
 */
static bool
urh_to_fdset (struct MHD_UpgradeResponseHandle *urh,
              fd_set *rs,
              fd_set *ws,
              fd_set *es,
              MHD_socket *max_fd,
              unsigned int fd_setsize)
{
  const MHD_socket conn_sckt = urh->connection->socket_fd;
  const MHD_socket mhd_sckt = urh->mhd.socket;
  bool res = true;

  /* Do not add to 'es' only if socket is closed
   * or not used anymore. */
  if (MHD_INVALID_SOCKET != conn_sckt)
    {
      if ( (urh->in_buffer_used < urh->in_buffer_size) &&
           (! MHD_add_to_fd_set_ (conn_sckt,
                                  rs,
                                  max_fd,
                                  fd_setsize)) )
        res = false;
      if ( (0 != urh->out_buffer_used) &&
           (! MHD_add_to_fd_set_ (conn_sckt,
                                  ws,
                                  max_fd,
                                  fd_setsize)) )
        res = false;
      /* Do not monitor again for errors if error was detected before as
       * error state is remembered. */
      if ((0 == (urh->app.celi & MHD_EPOLL_STATE_ERROR)) &&
          ((0 != urh->in_buffer_size) ||
           (0 != urh->out_buffer_size) ||
           (0 != urh->out_buffer_used)))
        MHD_add_to_fd_set_ (conn_sckt,
                            es,
                            max_fd,
                            fd_setsize);
    }
  if (MHD_INVALID_SOCKET != mhd_sckt)
    {
      if ( (urh->out_buffer_used < urh->out_buffer_size) &&
           (! MHD_add_to_fd_set_ (mhd_sckt,
                                  rs,
                                  max_fd,
                                  fd_setsize)) )
        res = false;
      if ( (0 != urh->in_buffer_used) &&
           (! MHD_add_to_fd_set_ (mhd_sckt,
                                  ws,
                                  max_fd,
                                  fd_setsize)) )
        res = false;
      /* Do not monitor again for errors if error was detected before as
       * error state is remembered. */
      if ((0 == (urh->mhd.celi & MHD_EPOLL_STATE_ERROR)) &&
          ((0 != urh->out_buffer_size) ||
           (0 != urh->in_buffer_size) ||
           (0 != urh->in_buffer_used)))
        MHD_add_to_fd_set_ (mhd_sckt,
                            es,
                            max_fd,
                            fd_setsize);
    }

  return res;
}


/**
 * Update the @a urh based on the ready FDs in
 * the @a rs, @a ws, and @a es.
 *
 * @param urh upgrade handle to update
 * @param rs read result from select()
 * @param ws write result from select()
 * @param ws except result from select()
 */
static void
urh_from_fdset (struct MHD_UpgradeResponseHandle *urh,
                const fd_set *rs,
                const fd_set *ws,
                const fd_set *es)
{
  const MHD_socket conn_sckt = urh->connection->socket_fd;
  const MHD_socket mhd_sckt = urh->mhd.socket;

  /* Reset read/write ready, preserve error state. */
  urh->app.celi &= (~MHD_EPOLL_STATE_READ_READY & ~MHD_EPOLL_STATE_WRITE_READY);
  urh->mhd.celi &= (~MHD_EPOLL_STATE_READ_READY & ~MHD_EPOLL_STATE_WRITE_READY);

  if (MHD_INVALID_SOCKET != conn_sckt)
    {
      if (FD_ISSET (conn_sckt, rs))
        urh->app.celi |= MHD_EPOLL_STATE_READ_READY;
      if (FD_ISSET (conn_sckt, ws))
        urh->app.celi |= MHD_EPOLL_STATE_WRITE_READY;
      if (FD_ISSET (conn_sckt, es))
        urh->app.celi |= MHD_EPOLL_STATE_ERROR;
    }
  if ((MHD_INVALID_SOCKET != mhd_sckt))
    {
      if (FD_ISSET (mhd_sckt, rs))
        urh->mhd.celi |= MHD_EPOLL_STATE_READ_READY;
      if (FD_ISSET (mhd_sckt, ws))
        urh->mhd.celi |= MHD_EPOLL_STATE_WRITE_READY;
      if (FD_ISSET (mhd_sckt, es))
        urh->mhd.celi |= MHD_EPOLL_STATE_ERROR;
    }
}

#ifdef HAVE_POLL

/**
 * Set required 'event' members in 'pollfd' elements,
 * assuming that @a p[0].fd is MHD side of socketpair
 * and @a p[1].fd is TLS connected socket.
 *
 * @param urh upgrade handle to watch for
 * @param p pollfd array to update
 */
static void
urh_update_pollfd(struct MHD_UpgradeResponseHandle *urh,
                  struct pollfd p[2])
{
  p[0].events = 0;
  p[1].events = 0;

  if (urh->in_buffer_used < urh->in_buffer_size)
    p[0].events |= POLLIN;
  if (0 != urh->out_buffer_used)
    p[0].events |= POLLOUT;

  /* Do not monitor again for errors if error was detected before as
   * error state is remembered. */
  if ((0 == (urh->app.celi & MHD_EPOLL_STATE_ERROR)) &&
      ((0 != urh->in_buffer_size) ||
       (0 != urh->out_buffer_size) ||
       (0 != urh->out_buffer_used)))
    p[0].events |= MHD_POLL_EVENTS_ERR_DISC;

  if (urh->out_buffer_used < urh->out_buffer_size)
    p[1].events |= POLLIN;
  if (0 != urh->in_buffer_used)
    p[1].events |= POLLOUT;

  /* Do not monitor again for errors if error was detected before as
   * error state is remembered. */
  if ((0 == (urh->mhd.celi & MHD_EPOLL_STATE_ERROR)) &&
      ((0 != urh->out_buffer_size) ||
       (0 != urh->in_buffer_size) ||
       (0 != urh->in_buffer_used)))
    p[1].events |= MHD_POLL_EVENTS_ERR_DISC;
}


/**
 * Set @a p to watch for @a urh.
 *
 * @param urh upgrade handle to watch for
 * @param p pollfd array to set
 */
static void
urh_to_pollfd(struct MHD_UpgradeResponseHandle *urh,
              struct pollfd p[2])
{
  p[0].fd = urh->connection->socket_fd;
  p[1].fd = urh->mhd.socket;
  urh_update_pollfd(urh, p);
}


/**
 * Update ready state in @a urh based on pollfd.
 * @param urh upgrade handle to update
 * @param p 'poll()' processed pollfd.
 */
static void
urh_from_pollfd(struct MHD_UpgradeResponseHandle *urh,
                struct pollfd p[2])
{
  /* Reset read/write ready, preserve error state. */
  urh->app.celi &= (~MHD_EPOLL_STATE_READ_READY & ~MHD_EPOLL_STATE_WRITE_READY);
  urh->mhd.celi &= (~MHD_EPOLL_STATE_READ_READY & ~MHD_EPOLL_STATE_WRITE_READY);

  if (0 != (p[0].revents & POLLIN))
    urh->app.celi |= MHD_EPOLL_STATE_READ_READY;
  if (0 != (p[0].revents & POLLOUT))
    urh->app.celi |= MHD_EPOLL_STATE_WRITE_READY;
  if (0 != (p[0].revents & POLLHUP))
    urh->app.celi |= MHD_EPOLL_STATE_READ_READY | MHD_EPOLL_STATE_WRITE_READY;
  if (0 != (p[0].revents & MHD_POLL_REVENTS_ERRROR))
    urh->app.celi |= MHD_EPOLL_STATE_ERROR;
  if (0 != (p[1].revents & POLLIN))
    urh->mhd.celi |= MHD_EPOLL_STATE_READ_READY;
  if (0 != (p[1].revents & POLLOUT))
    urh->mhd.celi |= MHD_EPOLL_STATE_WRITE_READY;
  if (0 != (p[1].revents & POLLHUP))
    urh->mhd.celi |= MHD_EPOLL_STATE_ERROR;
  if (0 != (p[1].revents & MHD_POLL_REVENTS_ERRROR))
    urh->mhd.celi |= MHD_EPOLL_STATE_READ_READY | MHD_EPOLL_STATE_WRITE_READY;
}
#endif /* HAVE_POLL */
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */


/**
 * Internal version of #MHD_get_fdset2().
 *
 * @param daemon daemon to get sets from
 * @param read_fd_set read set
 * @param write_fd_set write set
 * @param except_fd_set except set
 * @param max_fd increased to largest FD added (if larger
 *               than existing value); can be NULL
 * @param fd_setsize value of FD_SETSIZE
 * @return #MHD_YES on success, #MHD_NO if any FD didn't
 *         fit fd_set.
 * @ingroup event
 */
int
internal_get_fdset2 (struct MHD_Daemon *daemon,
                     fd_set *read_fd_set,
                     fd_set *write_fd_set,
                     fd_set *except_fd_set,
                     MHD_socket *max_fd,
                     unsigned int fd_setsize)

{
  struct MHD_Connection *pos;
  struct MHD_Connection *posn;
  int result = MHD_YES;
  MHD_socket ls;

  if (daemon->shutdown)
    return MHD_NO;

  ls = daemon->listen_fd;
  if ( (MHD_INVALID_SOCKET != ls) &&
       (! daemon->was_quiesced) &&
       (! MHD_add_to_fd_set_ (ls,
                              read_fd_set,
                              max_fd,
                              fd_setsize)) )
    result = MHD_NO;

  /* Add all sockets to 'except_fd_set' as well to watch for
   * out-of-band data. However, ignore errors if INFO_READ
   * or INFO_WRITE sockets will not fit 'except_fd_set'. */
  /* Start from oldest connections. Make sense for W32 FDSETs. */
  for (pos = daemon->connections_tail; NULL != pos; pos = posn)
    {
      posn = pos->prev;

      switch (pos->event_loop_info)
	{
	case MHD_EVENT_LOOP_INFO_READ:
	  if (! MHD_add_to_fd_set_ (pos->socket_fd,
                                    read_fd_set,
                                    max_fd,
                                    fd_setsize))
	    result = MHD_NO;
#ifdef MHD_POSIX_SOCKETS
          MHD_add_to_fd_set_ (pos->socket_fd,
                              except_fd_set,
                              max_fd,
                              fd_setsize);
#endif /* MHD_POSIX_SOCKETS */
	  break;
	case MHD_EVENT_LOOP_INFO_WRITE:
	  if (! MHD_add_to_fd_set_ (pos->socket_fd,
                                    write_fd_set,
                                    max_fd,
                                    fd_setsize))
	    result = MHD_NO;
#ifdef MHD_POSIX_SOCKETS
          MHD_add_to_fd_set_ (pos->socket_fd,
                              except_fd_set,
                              max_fd,
                              fd_setsize);
#endif /* MHD_POSIX_SOCKETS */
	  break;
	case MHD_EVENT_LOOP_INFO_BLOCK:
	  if ( (NULL == except_fd_set) ||
	      ! MHD_add_to_fd_set_ (pos->socket_fd,
	                            except_fd_set,
                                    max_fd,
                                    fd_setsize))
            result = MHD_NO;
	  break;
	case MHD_EVENT_LOOP_INFO_CLEANUP:
	  /* this should never happen */
	  break;
	}
    }
#ifdef MHD_WINSOCK_SOCKETS
  /* W32 use limited array for fd_set so add INFO_READ/INFO_WRITE sockets
   * only after INFO_BLOCK sockets to ensure that INFO_BLOCK sockets will
   * not be pushed out. */
  for (pos = daemon->connections_tail; NULL != pos; pos = posn)
    {
      posn = pos->prev;
      MHD_add_to_fd_set_ (pos->socket_fd,
                          except_fd_set,
                          max_fd,
                          fd_setsize);
    }
#endif /* MHD_WINSOCK_SOCKETS */
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  {
    struct MHD_UpgradeResponseHandle *urh;

    for (urh = daemon->urh_tail; NULL != urh; urh = urh->prev)
      {
        if (MHD_NO ==
            urh_to_fdset (urh,
                          read_fd_set,
                          write_fd_set,
                          except_fd_set,
                          max_fd,
                          fd_setsize))
          result = MHD_NO;
      }
  }
#endif
#if DEBUG_CONNECT
#ifdef HAVE_MESSAGES
  if (NULL != max_fd)
    MHD_DLOG (daemon,
              _("Maximum socket in select set: %d\n"),
              *max_fd);
#endif
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
  return result;
}


/**
 * Obtain the `select()` sets for this daemon.
 * Daemon's FDs will be added to fd_sets. To get only
 * daemon FDs in fd_sets, call FD_ZERO for each fd_set
 * before calling this function. Passing custom FD_SETSIZE
 * as @a fd_setsize allow usage of larger/smaller than
 * platform's default fd_sets.
 *
 * This function must be called only for daemon started
 * without #MHD_USE_INTERNAL_POLLING_THREAD flag.
 *
 * @param daemon daemon to get sets from
 * @param read_fd_set read set
 * @param write_fd_set write set
 * @param except_fd_set except set
 * @param max_fd increased to largest FD added (if larger
 *               than existing value); can be NULL
 * @param fd_setsize value of FD_SETSIZE
 * @return #MHD_YES on success, #MHD_NO if this
 *         daemon was not started with the right
 *         options for this call or any FD didn't
 *         fit fd_set.
 * @ingroup event
 */
int
MHD_get_fdset2 (struct MHD_Daemon *daemon,
               fd_set *read_fd_set,
               fd_set *write_fd_set,
               fd_set *except_fd_set,
               MHD_socket *max_fd,
               unsigned int fd_setsize)
{
  fd_set es;

  if ( (NULL == daemon) ||
       (NULL == read_fd_set) ||
       (NULL == write_fd_set) ||
       (0 != (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD)) ||
       (0 != (daemon->options & MHD_USE_POLL)))
    return MHD_NO;

  if (NULL == except_fd_set)
    { /* Workaround to maintain backward compatibility. */
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("MHD_get_fdset2() called with except_fd_set "
                  "set to NULL. Such behavior is unsupported.\n"));
#endif
      FD_ZERO (&es);
      except_fd_set = &es;
    }

#ifdef EPOLL_SUPPORT
  if (0 != (daemon->options & MHD_USE_EPOLL))
    {
      if (daemon->shutdown)
        return MHD_NO;

      /* we're in epoll mode, use the epoll FD as a stand-in for
         the entire event set */

      return MHD_add_to_fd_set_ (daemon->epoll_fd,
                                 read_fd_set,
                                 max_fd,
                                 fd_setsize) ? MHD_YES : MHD_NO;
    }
#endif

  return internal_get_fdset2 (daemon,
			      read_fd_set,
                              write_fd_set,
			      except_fd_set,
                              max_fd,
			      fd_setsize);
}


/**
 * Call the handlers for a connection in the appropriate order based
 * on the readiness as detected by the event loop.
 *
 * @param con connection to handle
 * @param read_ready set if the socket is ready for reading
 * @param write_ready set if the socket is ready for writing
 * @param force_close set if a hard error was detected on the socket;
 *        if this information is not available, simply pass #MHD_NO
 * @return #MHD_YES to continue normally,
 *         #MHD_NO if a serious error was encountered and the
 *         connection is to be closed.
 */
static int
call_handlers (struct MHD_Connection *con,
               bool read_ready,
               bool write_ready,
               bool force_close)
{
  int ret;
  bool states_info_processed = false;
  /* Fast track flag */
  bool on_fasttrack = (con->state == MHD_CONNECTION_INIT);

#ifdef HTTPS_SUPPORT
  if (con->tls_read_ready)
    read_ready = true;
#endif /* HTTPS_SUPPORT */
  if (!force_close)
    {
      if ( (MHD_EVENT_LOOP_INFO_READ == con->event_loop_info) &&
	   read_ready)
        {
          con->read_handler (con);
          ret = con->idle_handler (con);
          states_info_processed = true;
        }
      /* No need to check value of 'ret' here as closed connection
       * cannot be in MHD_EVENT_LOOP_INFO_WRITE state. */
      if ( (MHD_EVENT_LOOP_INFO_WRITE == con->event_loop_info) &&
	   write_ready)
        {
          con->write_handler (con);
          ret = con->idle_handler (con);
          states_info_processed = true;
        }
    }
  else
    {
      MHD_connection_close_ (con,
                             MHD_REQUEST_TERMINATED_WITH_ERROR);
      return con->idle_handler (con);
    }

  if (!states_info_processed)
    { /* Connection is not read or write ready, but external conditions
       * may be changed and need to be processed. */
      ret = con->idle_handler (con);
    }
  /* Fast track for fast connections. */
  /* If full request was read by single read_handler() invocation
     and headers were completely prepared by single idle_handler()
     then try not to wait for next sockets polling and send response
     immediately.
     As writeability of socket was not checked and it may have
     some data pending in system buffers, use this optimization
     only for non-blocking sockets. */
  /* No need to check 'ret' as connection is always in
   * MHD_CONNECTION_CLOSED state if 'ret' is equal 'MHD_NO'. */
  else if (on_fasttrack && con->sk_nonblck)
    {
      if (MHD_CONNECTION_HEADERS_SENDING == con->state)
        {
          con->write_handler (con);
          /* Always call 'idle_handler()' after each read/write. */
          ret = con->idle_handler (con);
        }
      /* If all headers were sent by single write_handler() and
       * response body is prepared by single idle_handler()
       * call - continue. */
      if ((MHD_CONNECTION_NORMAL_BODY_READY == con->state) ||
          (MHD_CONNECTION_CHUNKED_BODY_READY == con->state))
        {
          con->write_handler (con);
          ret = con->idle_handler (con);
        }
    }

  /* All connection's data and states are processed for this turn.
   * If connection already has more data to be processed - use
   * zero timeout for next select()/poll(). */
  /* Thread-per-connection do not need global zero timeout as
   * connections are processed individually. */
  /* Note: no need to check for read buffer availability for
   * TLS read-ready connection in 'read info' state as connection
   * without space in read buffer will be market as 'info block'. */
  if ( (!con->daemon->data_already_pending) &&
       (0 == (con->daemon->options & MHD_USE_THREAD_PER_CONNECTION)) )
    {
      if (MHD_EVENT_LOOP_INFO_BLOCK == con->event_loop_info)
        con->daemon->data_already_pending = true;
#ifdef HTTPS_SUPPORT
      else if ( (con->tls_read_ready) &&
                (MHD_EVENT_LOOP_INFO_READ == con->event_loop_info) )
        con->daemon->data_already_pending = true;
#endif /* HTTPS_SUPPORT */
    }
  return ret;
}


#ifdef UPGRADE_SUPPORT
/**
 * Finally cleanup upgrade-related resources. It should
 * be called when TLS buffers have been drained and
 * application signaled MHD by #MHD_UPGRADE_ACTION_CLOSE.
 *
 * @param connection handle to the upgraded connection to clean
 */
static void
cleanup_upgraded_connection (struct MHD_Connection *connection)
{
  struct MHD_UpgradeResponseHandle *urh = connection->urh;

  if (NULL == urh)
    return;
#ifdef HTTPS_SUPPORT
  /* Signal remote client the end of TLS connection by
   * gracefully closing TLS session. */
  if (0 != (connection->daemon->options & MHD_USE_TLS))
    gnutls_bye (connection->tls_session,
                GNUTLS_SHUT_WR);

  if (MHD_INVALID_SOCKET != urh->mhd.socket)
    MHD_socket_close_chk_ (urh->mhd.socket);

  if (MHD_INVALID_SOCKET != urh->app.socket)
    MHD_socket_close_chk_ (urh->app.socket);
#endif /* HTTPS_SUPPORT */
  connection->urh = NULL;
  free (urh);
}
#endif /* UPGRADE_SUPPORT */


#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
/**
 * Performs bi-directional forwarding on upgraded HTTPS connections
 * based on the readyness state stored in the @a urh handle.
 * @remark To be called only from thread that process
 * connection's recv(), send() and response.
 *
 * @param urh handle to process
 */
static void
process_urh (struct MHD_UpgradeResponseHandle *urh)
{
  /* Help compiler to optimize:
   * pointers to 'connection' and 'daemon' are not changed
   * during this processing, so no need to chain dereference
   * each time. */
  struct MHD_Connection * const connection = urh->connection;
  struct MHD_Daemon * const daemon = connection->daemon;
  /* Prevent data races: use same value of 'was_closed' throughout
   * this function. If 'was_closed' changed externally in the middle
   * of processing - it will be processed on next iteration. */
  bool was_closed;
  if (daemon->shutdown)
    {
      /* Daemon shutting down, application will not receive any more data. */
#ifdef HAVE_MESSAGES
      if (! urh->was_closed)
        {
          MHD_DLOG (daemon,
                    _("Initiated daemon shutdown while \"upgraded\" connection was not closed.\n"));
        }
#endif
      urh->was_closed = true;
    }
  was_closed = urh->was_closed;
  if (was_closed)
    {
      /* Application was closed connections: no more data
       * can be forwarded to application socket. */
      if (0 < urh->in_buffer_used)
        {
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Failed to forward to application " MHD_UNSIGNED_LONG_LONG_PRINTF \
                        " bytes of data received from remote side: application shut down socket\n"),
                    (MHD_UNSIGNED_LONG_LONG) urh->in_buffer_used);
#endif

        }
      /* If application signaled MHD about socket closure then
       * check for any pending data even if socket is not marked
       * as 'ready' (signal may arrive after poll()/select()).
       * Socketpair for forwarding is always in non-blocking mode
       * so no risk that recv() will block the thread. */
      if (0 != urh->out_buffer_size)
        urh->mhd.celi |= MHD_EPOLL_STATE_READ_READY;
      /* Discard any data received form remote. */
      urh->in_buffer_used = 0;
      /* Do not try to push data to application. */
      urh->mhd.celi &= ~MHD_EPOLL_STATE_WRITE_READY;
      /* Reading from remote client is not required anymore. */
      urh->in_buffer_size = 0;
      urh->app.celi &= ~MHD_EPOLL_STATE_READ_READY;
      connection->tls_read_ready = false;
    }

  /* On some platforms (W32, possibly Darwin) failed send() (send() will always
   * fail after remote disconnect was detected) may discard data in system
   * buffers received by system but not yet read by recv().
   * So, before trying send() on any socket, recv() must be performed at first
   * otherwise last part of incoming data may be lost. */

  /* If disconnect or error was detected - try to read from socket
   * to dry data possibly pending is system buffers. */
  if (0 != (MHD_EPOLL_STATE_ERROR & urh->app.celi))
    urh->app.celi |= MHD_EPOLL_STATE_READ_READY;
  if (0 != (MHD_EPOLL_STATE_ERROR & urh->mhd.celi))
    urh->mhd.celi |= MHD_EPOLL_STATE_READ_READY;

  /*
   * handle reading from remote TLS client
   */
  if ( ( (0 != (MHD_EPOLL_STATE_READ_READY & urh->app.celi)) ||
         (connection->tls_read_ready) ) &&
       (urh->in_buffer_used < urh->in_buffer_size) )
    {
      ssize_t res;
      size_t buf_size;

      buf_size = urh->in_buffer_size - urh->in_buffer_used;
      if (buf_size > SSIZE_MAX)
        buf_size = SSIZE_MAX;

      connection->tls_read_ready = false;
      res = gnutls_record_recv (connection->tls_session,
                                &urh->in_buffer[urh->in_buffer_used],
                                buf_size);
      if (0 >= res)
        {
          if (GNUTLS_E_INTERRUPTED != res)
            {
              urh->app.celi &= ~MHD_EPOLL_STATE_READ_READY;
              if (GNUTLS_E_AGAIN != res)
                {
                  /* Unrecoverable error on socket was detected or
                   * socket was disconnected/shut down. */
                  /* Stop trying to read from this TLS socket. */
                  urh->in_buffer_size = 0;
                }
            }
        }
      else /* 0 < res */
        {
          urh->in_buffer_used += res;
          if (buf_size > (size_t)res)
            urh->app.celi &= ~MHD_EPOLL_STATE_READ_READY;
          else if (0 < gnutls_record_check_pending (connection->tls_session))
            connection->tls_read_ready = true;
        }
      if (MHD_EPOLL_STATE_ERROR ==
          ((MHD_EPOLL_STATE_ERROR | MHD_EPOLL_STATE_READ_READY) & urh->app.celi))
        {
          /* Unrecoverable error on socket was detected and all
           * pending data was read from system buffers. */
          /* Stop trying to read from this TLS socket. */
          urh->in_buffer_size = 0;
        }
    }

  /*
   * handle reading from application
   */
  if ( (0 != (MHD_EPOLL_STATE_READ_READY & urh->mhd.celi)) &&
       (urh->out_buffer_used < urh->out_buffer_size) )
    {
      ssize_t res;
      size_t buf_size;

      buf_size = urh->out_buffer_size - urh->out_buffer_used;
      if (buf_size > MHD_SCKT_SEND_MAX_SIZE_)
        buf_size = MHD_SCKT_SEND_MAX_SIZE_;

      res = MHD_recv_ (urh->mhd.socket,
                       &urh->out_buffer[urh->out_buffer_used],
                       buf_size);
      if (0 >= res)
        {
          const int err = MHD_socket_get_error_ ();
          if ((0 == res) ||
              ((! MHD_SCKT_ERR_IS_EINTR_ (err)) &&
               (! MHD_SCKT_ERR_IS_LOW_RESOURCES_(err))))
            {
              urh->mhd.celi &= ~MHD_EPOLL_STATE_READ_READY;
              if ((0 == res) ||
                  (was_closed) ||
                  (0 != (MHD_EPOLL_STATE_ERROR & urh->mhd.celi)) ||
                  (! MHD_SCKT_ERR_IS_EAGAIN_ (err)))
                {
                  /* Socket disconnect/shutdown was detected;
                   * Application signaled about closure of 'upgraded' socket;
                   * or persistent / unrecoverable error. */
                  /* Do not try to pull more data from application. */
                  urh->out_buffer_size = 0;
                }
            }
        }
      else /* 0 < res */
        {
          urh->out_buffer_used += res;
          if (buf_size > (size_t)res)
            urh->mhd.celi &= ~MHD_EPOLL_STATE_READ_READY;
        }
      if ( (0 == (MHD_EPOLL_STATE_READ_READY & urh->mhd.celi)) &&
           ( (0 != (MHD_EPOLL_STATE_ERROR & urh->mhd.celi)) ||
             (was_closed) ) )
        {
          /* Unrecoverable error on socket was detected and all
           * pending data was read from system buffers. */
          /* Do not try to pull more data from application. */
          urh->out_buffer_size = 0;
        }
    }

  /*
   * handle writing to remote HTTPS client
   */
  if ( (0 != (MHD_EPOLL_STATE_WRITE_READY & urh->app.celi)) &&
       (urh->out_buffer_used > 0) )
    {
      ssize_t res;
      size_t data_size;

      data_size = urh->out_buffer_used;
      if (data_size > SSIZE_MAX)
        data_size = SSIZE_MAX;

      res = gnutls_record_send (connection->tls_session,
                                urh->out_buffer,
                                data_size);
      if (0 >= res)
        {
          if (GNUTLS_E_INTERRUPTED != res)
            {
              urh->app.celi &= ~MHD_EPOLL_STATE_WRITE_READY;
              if (GNUTLS_E_INTERRUPTED != res)
                {
                  /* TLS connection shut down or
                   * persistent / unrecoverable error. */
#ifdef HAVE_MESSAGES
                  MHD_DLOG (daemon,
                            _("Failed to forward to remote client " MHD_UNSIGNED_LONG_LONG_PRINTF \
                                " bytes of data received from application: %s\n"),
                            (MHD_UNSIGNED_LONG_LONG) urh->out_buffer_used,
                            gnutls_strerror(res));
#endif
                  /* Discard any data unsent to remote. */
                  urh->out_buffer_used = 0;
                  /* Do not try to pull more data from application. */
                  urh->out_buffer_size = 0;
                  urh->mhd.celi &= ~MHD_EPOLL_STATE_READ_READY;
                }
            }
        }
      else /* 0 < res */
        {
          const size_t next_out_buffer_used = urh->out_buffer_used - res;
          if (0 != next_out_buffer_used)
            {
              memmove (urh->out_buffer,
                       &urh->out_buffer[res],
                       next_out_buffer_used);
              if (data_size > (size_t)res)
                urh->app.celi &= ~MHD_EPOLL_STATE_WRITE_READY;
            }
          urh->out_buffer_used = next_out_buffer_used;
        }
      if ( (0 == urh->out_buffer_used) &&
           (0 != (MHD_EPOLL_STATE_ERROR & urh->app.celi)) )
        {
          /* Unrecoverable error on socket was detected and all
           * pending data was sent to remote. */
          /* Do not try to send to remote anymore. */
          urh->app.celi &= ~MHD_EPOLL_STATE_WRITE_READY;
          /* Do not try to pull more data from application. */
          urh->out_buffer_size = 0;
          urh->mhd.celi &= ~MHD_EPOLL_STATE_READ_READY;
        }
    }

  /*
   * handle writing to application
   */
  if ( (0 != (MHD_EPOLL_STATE_WRITE_READY & urh->mhd.celi)) &&
         (urh->in_buffer_used > 0) )
    {
      ssize_t res;
      size_t data_size;

      data_size = urh->in_buffer_used;
      if (data_size > MHD_SCKT_SEND_MAX_SIZE_)
        data_size = MHD_SCKT_SEND_MAX_SIZE_;

      res = MHD_send_ (urh->mhd.socket,
                       urh->in_buffer,
                       data_size);
      if (0 >= res)
        {
          const int err = MHD_socket_get_error_ ();
          if ( (! MHD_SCKT_ERR_IS_EINTR_ (err)) &&
               (! MHD_SCKT_ERR_IS_LOW_RESOURCES_(err)) )
            {
              urh->mhd.celi &= ~MHD_EPOLL_STATE_WRITE_READY;
              if (! MHD_SCKT_ERR_IS_EAGAIN_ (err))
                {
                  /* Socketpair connection shut down or
                   * persistent / unrecoverable error. */
#ifdef HAVE_MESSAGES
                  MHD_DLOG (daemon,
                            _("Failed to forward to application " MHD_UNSIGNED_LONG_LONG_PRINTF \
                                " bytes of data received from remote side: %s\n"),
                            (MHD_UNSIGNED_LONG_LONG) urh->in_buffer_used,
                            MHD_socket_strerr_ (err));
#endif
                  /* Discard any data received form remote. */
                  urh->in_buffer_used = 0;
                  /* Reading from remote client is not required anymore. */
                  urh->in_buffer_size = 0;
                  urh->app.celi &= ~MHD_EPOLL_STATE_READ_READY;
                  connection->tls_read_ready = false;
                }
            }
        }
      else /* 0 < res */
        {
          const size_t next_in_buffer_used = urh->in_buffer_used - res;
          if (0 != next_in_buffer_used)
            {
              memmove (urh->in_buffer,
                       &urh->in_buffer[res],
                       next_in_buffer_used);
              if (data_size > (size_t)res)
                urh->mhd.celi &= ~MHD_EPOLL_STATE_WRITE_READY;
            }
          urh->in_buffer_used = next_in_buffer_used;
        }
      if ( (0 == urh->in_buffer_used) &&
           (0 != (MHD_EPOLL_STATE_ERROR & urh->mhd.celi)) )
        {
          /* Do not try to push data to application. */
          urh->mhd.celi &= ~MHD_EPOLL_STATE_WRITE_READY;
          /* Reading from remote client is not required anymore. */
          urh->in_buffer_size = 0;
          urh->app.celi &= ~MHD_EPOLL_STATE_READ_READY;
          connection->tls_read_ready = false;
        }
    }

  /* Check whether data is present in TLS buffers
   * and incoming forward buffer have some space. */
  if ( (connection->tls_read_ready) &&
       (urh->in_buffer_used < urh->in_buffer_size) &&
       (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) )
    daemon->data_already_pending = true;

  if ( (daemon->shutdown) &&
       ( (0 != urh->out_buffer_size) ||
         (0 != urh->out_buffer_used) ) )
    {
      /* Daemon shutting down, discard any remaining forward data. */
#ifdef HAVE_MESSAGES
      if (0 < urh->out_buffer_used)
        MHD_DLOG (daemon,
                  _("Failed to forward to remote client " MHD_UNSIGNED_LONG_LONG_PRINTF \
                      " bytes of data received from application: daemon shut down\n"),
                  (MHD_UNSIGNED_LONG_LONG) urh->out_buffer_used);
#endif
      /* Discard any data unsent to remote. */
      urh->out_buffer_used = 0;
      /* Do not try to sent to remote anymore. */
      urh->app.celi &= ~MHD_EPOLL_STATE_WRITE_READY;
      /* Do not try to pull more data from application. */
      urh->out_buffer_size = 0;
      urh->mhd.celi &= ~MHD_EPOLL_STATE_READ_READY;
    }
}
#endif /* HTTPS_SUPPORT  && UPGRADE_SUPPORT */


#ifdef UPGRADE_SUPPORT
/**
 * Main function of the thread that handles an individual connection
 * after it was "upgraded" when #MHD_USE_THREAD_PER_CONNECTION is set.
 * @remark To be called only from thread that process
 * connection's recv(), send() and response.
 *
 * @param con the connection this thread will handle
 */
static void
thread_main_connection_upgrade (struct MHD_Connection *con)
{
#ifdef HTTPS_SUPPORT
  struct MHD_UpgradeResponseHandle *urh = con->urh;
  struct MHD_Daemon *daemon = con->daemon;

  /* Here, we need to bi-directionally forward
     until the application tells us that it is done
     with the socket; */
  if ( (0 != (daemon->options & MHD_USE_TLS)) &&
      (0 == (daemon->options & MHD_USE_POLL)))
    {
      while ( (0 != urh->in_buffer_size) ||
              (0 != urh->out_buffer_size) ||
              (0 != urh->in_buffer_used) ||
              (0 != urh->out_buffer_used) )
        {
          /* use select */
          fd_set rs;
          fd_set ws;
          fd_set es;
          MHD_socket max_fd;
          int num_ready;
          bool result;

          FD_ZERO (&rs);
          FD_ZERO (&ws);
          FD_ZERO (&es);
          max_fd = MHD_INVALID_SOCKET;
          result = urh_to_fdset (urh,
                                 &rs,
                                 &ws,
                                 &es,
                                 &max_fd,
                                 FD_SETSIZE);
          if (! result)
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (con->daemon,
                        _("Error preparing select\n"));
#endif
              break;
            }
          /* FIXME: does this check really needed? */
          if (MHD_INVALID_SOCKET != max_fd)
            {
              struct timeval* tvp;
              struct timeval tv;
              if ( (con->tls_read_ready) &&
                   (urh->in_buffer_used < urh->in_buffer_size))
                { /* No need to wait if incoming data is already pending in TLS buffers. */
                  tv.tv_sec = 0;
                  tv.tv_usec = 0;
                  tvp = &tv;
                }
              else
                tvp = NULL;
              num_ready = MHD_SYS_select_ (max_fd + 1,
                                           &rs,
                                           &ws,
                                           &es,
                                           tvp);
            }
          else
            num_ready = 0;
          if (num_ready < 0)
            {
              const int err = MHD_socket_get_error_();

              if (MHD_SCKT_ERR_IS_EINTR_(err))
                continue;
#ifdef HAVE_MESSAGES
              MHD_DLOG (con->daemon,
                        _("Error during select (%d): `%s'\n"),
                        err,
                        MHD_socket_strerr_ (err));
#endif
              break;
            }
          urh_from_fdset (urh,
                          &rs,
                          &ws,
                          &es);
          process_urh (urh);
        }
    }
#ifdef HAVE_POLL
  else if (0 != (daemon->options & MHD_USE_TLS))
    {
      /* use poll() */
      struct pollfd p[2];
      memset (p,
              0,
              sizeof (p));
      p[0].fd = urh->connection->socket_fd;
      p[1].fd = urh->mhd.socket;

      while ( (0 != urh->in_buffer_size) ||
              (0 != urh->out_buffer_size) ||
              (0 != urh->in_buffer_used) ||
              (0 != urh->out_buffer_used) )
        {
          int timeout;

          urh_update_pollfd(urh, p);

          if ( (con->tls_read_ready) &&
               (urh->in_buffer_used < urh->in_buffer_size))
            timeout = 0; /* No need to wait if incoming data is already pending in TLS buffers. */
          else
            timeout = -1;

          if (MHD_sys_poll_ (p,
                             2,
                             timeout) < 0)
            {
              const int err = MHD_socket_get_error_ ();

              if (MHD_SCKT_ERR_IS_EINTR_ (err))
                continue;
#ifdef HAVE_MESSAGES
              MHD_DLOG (con->daemon,
                        _("Error during poll: `%s'\n"),
                        MHD_socket_strerr_ (err));
#endif
              break;
            }
          urh_from_pollfd(urh, p);
          process_urh (urh);
        }
    }
  /* end POLL */
#endif
  /* end HTTPS */
#endif /* HTTPS_SUPPORT */
  /* TLS forwarding was finished. Cleanup socketpair. */
  MHD_connection_finish_forward_ (con);
  /* Do not set 'urh->clean_ready' yet as 'urh' will be used
   * in connection thread for a little while. */
}
#endif /* UPGRADE_SUPPORT */


/**
 * Main function of the thread that handles an individual
 * connection when #MHD_USE_THREAD_PER_CONNECTION is set.
 *
 * @param data the `struct MHD_Connection` this thread will handle
 * @return always 0
 */
static MHD_THRD_RTRN_TYPE_ MHD_THRD_CALL_SPEC_
thread_main_handle_connection (void *data)
{
  struct MHD_Connection *con = data;
  struct MHD_Daemon *daemon = con->daemon;
  int num_ready;
  fd_set rs;
  fd_set ws;
  fd_set es;
  MHD_socket maxsock;
  struct timeval tv;
  struct timeval *tvp;
  time_t now;
#if WINDOWS
#ifdef HAVE_POLL
  int extra_slot;
#endif /* HAVE_POLL */
#define EXTRA_SLOTS 1
#else  /* !WINDOWS */
#define EXTRA_SLOTS 0
#endif /* !WINDOWS */
#ifdef HAVE_POLL
  struct pollfd p[1 + EXTRA_SLOTS];
#endif
#undef EXTRA_SLOTS
#ifdef HAVE_POLL
  const bool use_poll = (0 != (daemon->options & MHD_USE_POLL));
#else  /* ! HAVE_POLL */
  const bool use_poll = 0;
#endif /* ! HAVE_POLL */
  bool was_suspended = false;

  while ( (! daemon->shutdown) &&
	  (MHD_CONNECTION_CLOSED != con->state) )
    {
      const time_t timeout = daemon->connection_timeout;
#ifdef UPGRADE_SUPPORT
      struct MHD_UpgradeResponseHandle * const urh = con->urh;
#else  /* ! UPGRADE_SUPPORT */
      static const void * const urh = NULL;
#endif /* ! UPGRADE_SUPPORT */

      if ( (con->suspended) &&
           (NULL == urh) )
        {
          /* Connection was suspended, wait for resume. */
          was_suspended = true;
          if (! use_poll)
            {
              FD_ZERO (&rs);
              if (! MHD_add_to_fd_set_ (MHD_itc_r_fd_ (daemon->itc),
                                        &rs,
                                        NULL,
                                        FD_SETSIZE))
                {
  #ifdef HAVE_MESSAGES
                  MHD_DLOG (con->daemon,
                            _("Failed to add FD to fd_set\n"));
  #endif
                  goto exit;
                }
              if (0 > MHD_SYS_select_ (MHD_itc_r_fd_ (daemon->itc) + 1,
                                       &rs,
                                       NULL,
                                       NULL,
                                       NULL))
                {
                  const int err = MHD_socket_get_error_();

                  if (MHD_SCKT_ERR_IS_EINTR_(err))
                    continue;
#ifdef HAVE_MESSAGES
                  MHD_DLOG (con->daemon,
                            _("Error during select (%d): `%s'\n"),
                            err,
                            MHD_socket_strerr_ (err));
#endif
                  break;
                }
            }
#ifdef HAVE_POLL
          else /* use_poll */
            {
              p[0].events = POLLIN;
              p[0].fd = MHD_itc_r_fd_ (daemon->itc);
              p[0].revents = 0;
              if (0 > MHD_sys_poll_ (p,
                                     1,
                                     -1))
                {
                  if (MHD_SCKT_LAST_ERR_IS_(MHD_SCKT_EINTR_))
                    continue;
#ifdef HAVE_MESSAGES
                  MHD_DLOG (con->daemon,
                            _("Error during poll: `%s'\n"),
                            MHD_socket_last_strerr_ ());
#endif
                  break;
                }
            }
#endif /* HAVE_POLL */
          MHD_itc_clear_ (daemon->itc);
          continue; /* Check again for resume. */
        } /* End of "suspended" branch. */

      if (was_suspended)
        {
          MHD_update_last_activity_ (con); /* Reset timeout timer. */
          was_suspended = false;
        }

      tvp = NULL;

      if ( (MHD_EVENT_LOOP_INFO_BLOCK == con->event_loop_info)
#ifdef HTTPS_SUPPORT
           || ( (con->tls_read_ready) &&
                (MHD_EVENT_LOOP_INFO_READ == con->event_loop_info) )
#endif /* HTTPS_SUPPORT */
         )
	{
	  /* do not block: more data may be inside of TLS buffers waiting or
	   * application must provide response data */
	  tv.tv_sec = 0;
	  tv.tv_usec = 0;
	  tvp = &tv;
	}
      if ( (NULL == tvp) &&
           (timeout > 0) )
	{
	  now = MHD_monotonic_sec_counter();
	  if (now - con->last_activity > timeout)
	    tv.tv_sec = 0;
          else
            {
              const time_t seconds_left = timeout - (now - con->last_activity);
#ifndef _WIN32
              tv.tv_sec = seconds_left;
#else  /* _WIN32 */
              if (seconds_left > TIMEVAL_TV_SEC_MAX)
                tv.tv_sec = TIMEVAL_TV_SEC_MAX;
              else
                tv.tv_sec = (_MHD_TIMEVAL_TV_SEC_TYPE) seconds_left;
#endif /* _WIN32 */
            }
	  tv.tv_usec = 0;
	  tvp = &tv;
	}
      if (! use_poll)
	{
	  /* use select */
	  bool err_state = false;

	  FD_ZERO (&rs);
	  FD_ZERO (&ws);
          FD_ZERO (&es);
	  maxsock = MHD_INVALID_SOCKET;
	  switch (con->event_loop_info)
	    {
	    case MHD_EVENT_LOOP_INFO_READ:
	      if (! MHD_add_to_fd_set_ (con->socket_fd,
                                        &rs,
                                        &maxsock,
                                        FD_SETSIZE))
	        err_state = true;
	      break;
	    case MHD_EVENT_LOOP_INFO_WRITE:
	      if (! MHD_add_to_fd_set_ (con->socket_fd,
                                        &ws,
                                        &maxsock,
                                        FD_SETSIZE))
                err_state = true;
	      break;
	    case MHD_EVENT_LOOP_INFO_BLOCK:
	      if (! MHD_add_to_fd_set_ (con->socket_fd,
                                        &es,
                                        &maxsock,
                                        FD_SETSIZE))
	        err_state = true;
	      break;
	    case MHD_EVENT_LOOP_INFO_CLEANUP:
	      /* how did we get here!? */
	      goto exit;
	    }
#if WINDOWS
          if (MHD_ITC_IS_VALID_(daemon->itc) )
            {
              if (! MHD_add_to_fd_set_ (MHD_itc_r_fd_ (daemon->itc),
                                        &rs,
                                        &maxsock,
                                        FD_SETSIZE))
                err_state = 1;
            }
#endif
            if (err_state)
              {
#ifdef HAVE_MESSAGES
                MHD_DLOG (con->daemon,
                          _("Failed to add FD to fd_set\n"));
#endif
                goto exit;
              }

	  num_ready = MHD_SYS_select_ (maxsock + 1,
                                       &rs,
                                       &ws,
                                       NULL,
                                       tvp);
	  if (num_ready < 0)
	    {
	      const int err = MHD_socket_get_error_();

	      if (MHD_SCKT_ERR_IS_EINTR_(err))
		continue;
#ifdef HAVE_MESSAGES
	      MHD_DLOG (con->daemon,
			_("Error during select (%d): `%s'\n"),
			err,
			MHD_socket_strerr_ (err));
#endif
	      break;
	    }
#if WINDOWS
          /* Clear ITC before other processing so additional
           * signals will trigger select() again */
          if ( (MHD_ITC_IS_VALID_(daemon->itc)) &&
               (FD_ISSET (MHD_itc_r_fd_ (daemon->itc),
                          &rs)) )
            MHD_itc_clear_ (daemon->itc);
#endif
          if (MHD_NO ==
              call_handlers (con,
                             FD_ISSET (con->socket_fd,
                                       &rs),
                             FD_ISSET (con->socket_fd,
                                       &ws),
                             FD_ISSET (con->socket_fd,
                                       &es)) )
            goto exit;
	}
#ifdef HAVE_POLL
      else
	{
	  /* use poll */
	  memset (&p,
                  0,
                  sizeof (p));
	  p[0].fd = con->socket_fd;
	  switch (con->event_loop_info)
	    {
	    case MHD_EVENT_LOOP_INFO_READ:
	      p[0].events |= POLLIN | MHD_POLL_EVENTS_ERR_DISC;
	      break;
	    case MHD_EVENT_LOOP_INFO_WRITE:
	      p[0].events |= POLLOUT | MHD_POLL_EVENTS_ERR_DISC;
	      break;
	    case MHD_EVENT_LOOP_INFO_BLOCK:
	      p[0].events |= MHD_POLL_EVENTS_ERR_DISC;
	      break;
	    case MHD_EVENT_LOOP_INFO_CLEANUP:
	      /* how did we get here!? */
	      goto exit;
	    }
#if WINDOWS
          extra_slot = 0;
          if (MHD_ITC_IS_VALID_(daemon->itc))
            {
              p[1].events |= POLLIN;
              p[1].fd = MHD_itc_r_fd_ (daemon->itc);
              p[1].revents = 0;
              extra_slot = 1;
            }
#endif
	  if (MHD_sys_poll_ (p,
#if WINDOWS
                             1 + extra_slot,
#else
                             1,
#endif
                             (NULL == tvp) ? -1 : tv.tv_sec * 1000) < 0)
	    {
	      if (MHD_SCKT_LAST_ERR_IS_(MHD_SCKT_EINTR_))
		continue;
#ifdef HAVE_MESSAGES
	      MHD_DLOG (con->daemon,
                        _("Error during poll: `%s'\n"),
			MHD_socket_last_strerr_ ());
#endif
	      break;
	    }
#if WINDOWS
          /* Clear ITC before other processing so additional
           * signals will trigger poll() again */
          if ( (MHD_ITC_IS_VALID_(daemon->itc)) &&
               (0 != (p[1].revents & (POLLERR | POLLHUP | POLLIN))) )
            MHD_itc_clear_ (daemon->itc);
#endif
          if (MHD_NO ==
              call_handlers (con,
                             0 != (p[0].revents & POLLIN),
                             0 != (p[0].revents & POLLOUT),
                             0 != (p[0].revents & (POLLERR | MHD_POLL_REVENTS_ERR_DISC))))
            goto exit;
	}
#endif
#ifdef UPGRADE_SUPPORT
      if (MHD_CONNECTION_UPGRADE == con->state)
        {
          /* Normal HTTP processing is finished,
           * notify application. */
          if ( (NULL != daemon->notify_completed) &&
               (con->client_aware) )
            daemon->notify_completed (daemon->notify_completed_cls,
                                      con,
                                      &con->client_context,
                                      MHD_REQUEST_TERMINATED_COMPLETED_OK);
          con->client_aware = false;

          thread_main_connection_upgrade (con);
          /* MHD_connection_finish_forward_() was called by thread_main_connection_upgrade(). */

          /* "Upgraded" data will not be used in this thread from this point. */
          con->urh->clean_ready = true;
          /* If 'urh->was_closed' set to true, connection will be
           * moved immediately to cleanup list. Otherwise connection
           * will stay in suspended list until 'urh' will be marked
           * with 'was_closed' by application. */
          MHD_resume_connection(con);

          /* skip usual clean up  */
          return (MHD_THRD_RTRN_TYPE_) 0;
        }
#endif /* UPGRADE_SUPPORT */
    }
  if (MHD_CONNECTION_IN_CLEANUP != con->state)
    {
#if DEBUG_CLOSE
#ifdef HAVE_MESSAGES
      MHD_DLOG (con->daemon,
                _("Processing thread terminating. Closing connection\n"));
#endif
#endif
      if (MHD_CONNECTION_CLOSED != con->state)
	MHD_connection_close_ (con,
                               (daemon->shutdown) ?
                               MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN:
                               MHD_REQUEST_TERMINATED_WITH_ERROR);
      con->idle_handler (con);
    }
exit:
  if (NULL != con->response)
    {
      MHD_destroy_response (con->response);
      con->response = NULL;
    }

  if (MHD_INVALID_SOCKET != con->socket_fd)
    {
      shutdown (con->socket_fd,
                SHUT_WR);
      /* 'socket_fd' can be used in other thread to signal shutdown.
       * To avoid data races, do not close socket here. Daemon will
       * use more connections only after cleanup anyway. */
    }
  return (MHD_THRD_RTRN_TYPE_) 0;
}


/**
 * Callback for receiving data from the socket.
 *
 * @param connection the MHD connection structure
 * @param other where to write received data to
 * @param i maximum size of other (in bytes)
 * @return number of bytes actually received
 */
static ssize_t
recv_param_adapter (struct MHD_Connection *connection,
		    void *other,
		    size_t i)
{
  ssize_t ret;

  if ( (MHD_INVALID_SOCKET == connection->socket_fd) ||
       (MHD_CONNECTION_CLOSED == connection->state) )
    {
      MHD_socket_set_error_ (MHD_SCKT_ENOTCONN_);
      return -1;
    }
  if (i > MHD_SCKT_SEND_MAX_SIZE_)
    i = MHD_SCKT_SEND_MAX_SIZE_; /* return value limit */

  ret = MHD_recv_ (connection->socket_fd,
                   other,
                   i);
#ifdef EPOLL_SUPPORT
  if (0 > ret)
    {
      /* Got EAGAIN --- no longer read-ready */
      if (MHD_SCKT_ERR_IS_EAGAIN_ (MHD_socket_get_error_ ()))
        connection->epoll_state &= ~MHD_EPOLL_STATE_READ_READY;
    }
  else if (i > (size_t)ret)
    connection->epoll_state &= ~MHD_EPOLL_STATE_READ_READY;
#endif
  return ret;
}


/**
 * Callback for writing data to the socket.
 *
 * @param connection the MHD connection structure
 * @param other data to write
 * @param i number of bytes to write
 * @return actual number of bytes written
 */
static ssize_t
send_param_adapter (struct MHD_Connection *connection,
                    const void *other,
		    size_t i)
{
  ssize_t ret;
  int err;

  if ( (MHD_INVALID_SOCKET == connection->socket_fd) ||
       (MHD_CONNECTION_CLOSED == connection->state) )
    {
      MHD_socket_set_error_ (MHD_SCKT_ENOTCONN_);
      return -1;
    }
  if (i > MHD_SCKT_SEND_MAX_SIZE_)
    i = MHD_SCKT_SEND_MAX_SIZE_; /* return value limit */

  if (0 != (connection->daemon->options & MHD_USE_TLS))
    return MHD_send_ (connection->socket_fd,
                      other,
                      i);
#if LINUX
  if ( (connection->write_buffer_append_offset ==
	connection->write_buffer_send_offset) &&
       (NULL != connection->response) &&
       (MHD_resp_sender_sendfile == connection->resp_sender) )
    {
      /* can use sendfile */
      int file_fd = connection->response->fd;
      uint64_t left;
      uint64_t offsetu64;
#ifndef HAVE_SENDFILE64
      off_t offset;
#else  /* HAVE_SENDFILE64 */
      off64_t offset;
#endif /* HAVE_SENDFILE64 */
      offsetu64 = connection->response_write_position + connection->response->fd_off;
      left = connection->response->total_size - connection->response_write_position;
      ret = 0;
#ifndef HAVE_SENDFILE64
      if ((uint64_t)OFF_T_MAX < offsetu64)
        MHD_socket_set_error_to_ENOMEM ();
      else
        {
          offset = (off_t) offsetu64;
          ret = sendfile (connection->socket_fd,
                          file_fd,
                          &offset,
                          left);
        }
#else  /* HAVE_SENDFILE64 */
      if ((uint64_t)OFF64_T_MAX < offsetu64)
        MHD_socket_set_error_to_ENOMEM ();
      else
        {
          offset = (off64_t) offsetu64;
          ret = sendfile64 (connection->socket_fd,
                            file_fd,
                            &offset,
                            left);
        }
#endif /* HAVE_SENDFILE64 */
      if (0 < ret)
        {
          /* write successful */
          if (left > (uint64_t)ret)
            connection->epoll_state &= ~MHD_EPOLL_STATE_WRITE_READY;
          return ret;
        }
      err = MHD_socket_get_error_();
#ifdef EPOLL_SUPPORT
      if ( (0 > ret) && (MHD_SCKT_ERR_IS_EAGAIN_(err)) )
        {
          /* EAGAIN --- no longer write-ready */
          connection->epoll_state &= ~MHD_EPOLL_STATE_WRITE_READY;
        }
#endif
      if (MHD_SCKT_ERR_IS_EINTR_ (err) ||
          MHD_SCKT_ERR_IS_EAGAIN_ (err))
	return 0;
      if (MHD_SCKT_ERR_IS_(err,
                           MHD_SCKT_EBADF_))
	return -1;
      /* sendfile() failed with EINVAL if mmap()-like operations are not
	 supported for FD or other 'unusual' errors occurred, so we should try
	 to fall back to 'SEND'; see also this thread for info on
	 odd libc/Linux behavior with sendfile:
	 http://lists.gnu.org/archive/html/libmicrohttpd/2011-02/msg00015.html */
      connection->resp_sender = MHD_resp_sender_std;
    }
#endif
  ret = MHD_send_ (connection->socket_fd,
                   other,
                   i);
  err = MHD_socket_get_error_();
#ifdef EPOLL_SUPPORT
  if (0 > ret)
    {
      /* EAGAIN --- no longer write-ready */
      if (MHD_SCKT_ERR_IS_EAGAIN_(err))
        connection->epoll_state &= ~MHD_EPOLL_STATE_WRITE_READY;
    }
  else if (i > (size_t)ret)
    connection->epoll_state &= ~MHD_EPOLL_STATE_WRITE_READY;
#endif
  /* Handle broken kernel / libc, returning -1 but not setting errno;
     kill connection as that should be safe; reported on mailinglist here:
     http://lists.gnu.org/archive/html/libmicrohttpd/2014-10/msg00023.html */
  if ( (0 > ret) &&
       (0 == err) )
    MHD_socket_set_error_ (MHD_SCKT_ECONNRESET_);
  return ret;
}


/**
 * Free resources associated with all closed connections.
 * (destroy responses, free buffers, etc.).  All closed
 * connections are kept in the "cleanup" doubly-linked list.
 *
 * @param daemon daemon to clean up
 */
static void
MHD_cleanup_connections (struct MHD_Daemon *daemon);


/**
 * Add another client connection to the set of connections
 * managed by MHD.  This API is usually not needed (since
 * MHD will accept inbound connections on the server socket).
 * Use this API in special cases, for example if your HTTP
 * server is behind NAT and needs to connect out to the
 * HTTP client.
 *
 * The given client socket will be managed (and closed!) by MHD after
 * this call and must no longer be used directly by the application
 * afterwards.
 *
 * Per-IP connection limits are ignored when using this API.
 *
 * @remark To be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param daemon daemon that manages the connection
 * @param client_socket socket to manage (MHD will expect
 *        to receive an HTTP request from this socket next).
 * @param addr IP address of the client
 * @param addrlen number of bytes in @a addr
 * @param external_add perform additional operations needed due
 *        to the application calling us directly
 * @param non_blck indicate that socket in non-blocking mode
 * @return #MHD_YES on success, #MHD_NO if this daemon could
 *        not handle the connection (i.e. malloc failed, etc).
 *        The socket will be closed in any case; 'errno' is
 *        set to indicate further details about the error.
 */
static int
internal_add_connection (struct MHD_Daemon *daemon,
			 MHD_socket client_socket,
			 const struct sockaddr *addr,
			 socklen_t addrlen,
			 int external_add,
			 bool non_blck)
{
  struct MHD_Connection *connection;
  unsigned int i;
  int eno;
  struct MHD_Daemon *worker;

  if (NULL != daemon->worker_pool)
    {
      /* have a pool, try to find a pool with capacity; we use the
	 socket as the initial offset into the pool for load
	 balancing */
      for (i=0;i<daemon->worker_pool_size;i++)
        {
          worker = &daemon->worker_pool[(i + client_socket) % daemon->worker_pool_size];
          if (worker->connections < worker->connection_limit)
            return internal_add_connection (worker,
                                            client_socket,
                                            addr,
                                            addrlen,
                                            external_add,
                                            non_blck);
        }
      /* all pools are at their connection limit, must refuse */
      MHD_socket_close_chk_ (client_socket);
#if ENFILE
      errno = ENFILE;
#endif
      return MHD_NO;
    }

  if ( (! MHD_SCKT_FD_FITS_FDSET_(client_socket,
                                  NULL)) &&
       (0 == (daemon->options & (MHD_USE_POLL | MHD_USE_EPOLL))) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Socket descriptor larger than FD_SETSIZE: %d > %d\n"),
		(int) client_socket,
		(int) FD_SETSIZE);
#endif
      MHD_socket_close_chk_ (client_socket);
#if EINVAL
      errno = EINVAL;
#endif
      return MHD_NO;
    }

#ifdef MHD_socket_nosignal_
  if (! MHD_socket_nosignal_ (client_socket))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to set SO_NOSIGPIPE on accepted socket: %s\n"),
                MHD_socket_last_strerr_());
#endif
#ifndef MSG_NOSIGNAL
      /* Cannot use socket as it can produce SIGPIPE. */
#ifdef ENOTSOCK
      errno = ENOTSOCK;
#endif /* ENOTSOCK */
      return MHD_NO;
#endif /* ! MSG_NOSIGNAL */
    }
#endif /* MHD_socket_nosignal_ */


#ifdef HAVE_MESSAGES
#if DEBUG_CONNECT
  MHD_DLOG (daemon,
            _("Accepted connection on socket %d\n"),
            client_socket);
#endif
#endif
  if ( (daemon->connections == daemon->connection_limit) ||
       (MHD_NO == MHD_ip_limit_add (daemon,
                                    addr,
                                    addrlen)) )
    {
      /* above connection limit - reject */
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Server reached connection limit. Closing inbound connection.\n"));
#endif
      MHD_socket_close_chk_ (client_socket);
#if ENFILE
      errno = ENFILE;
#endif
      return MHD_NO;
    }

  /* apply connection acceptance policy if present */
  if ( (NULL != daemon->apc) &&
       (MHD_NO == daemon->apc (daemon->apc_cls,
			       addr,
                               addrlen)) )
    {
#if DEBUG_CLOSE
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Connection rejected by application. Closing connection.\n"));
#endif
#endif
      MHD_socket_close_chk_ (client_socket);
      MHD_ip_limit_del (daemon,
                        addr,
                        addrlen);
#if EACCESS
      errno = EACCESS;
#endif
      return MHD_NO;
    }

  if (NULL == (connection = MHD_calloc_ (1, sizeof (struct MHD_Connection))))
    {
      eno = errno;
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		"Error allocating memory: %s\n",
		MHD_strerror_ (errno));
#endif
      MHD_socket_close_chk_ (client_socket);
      MHD_ip_limit_del (daemon,
                        addr,
                        addrlen);
      errno = eno;
      return MHD_NO;
    }
  connection->pool = MHD_pool_create (daemon->pool_size);
  if (NULL == connection->pool)
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Error allocating memory: %s\n"),
		MHD_strerror_ (errno));
#endif
      MHD_socket_close_chk_ (client_socket);
      MHD_ip_limit_del (daemon,
                        addr,
                        addrlen);
      free (connection);
#if ENOMEM
      errno = ENOMEM;
#endif
      return MHD_NO;
    }

  connection->connection_timeout = daemon->connection_timeout;
  if (NULL == (connection->addr = malloc (addrlen)))
    {
      eno = errno;
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Error allocating memory: %s\n"),
		MHD_strerror_ (errno));
#endif
      MHD_socket_close_chk_ (client_socket);
      MHD_ip_limit_del (daemon,
                        addr,
                        addrlen);
      MHD_pool_destroy (connection->pool);
      free (connection);
      errno = eno;
      return MHD_NO;
    }
  memcpy (connection->addr,
          addr,
          addrlen);
  connection->addr_len = addrlen;
  connection->socket_fd = client_socket;
  connection->sk_nonblck = non_blck;
  connection->daemon = daemon;
  connection->last_activity = MHD_monotonic_sec_counter();

  if (0 == (daemon->options & MHD_USE_TLS))
    {
      /* set default connection handlers  */
      MHD_set_http_callbacks_ (connection);
      connection->recv_cls = &recv_param_adapter;
      connection->send_cls = &send_param_adapter;
    }
  else
    {
#ifdef HTTPS_SUPPORT
      connection->recv_cls = &recv_tls_adapter;
      connection->send_cls = &send_tls_adapter;
      connection->state = MHD_TLS_CONNECTION_INIT;
      MHD_set_https_callbacks (connection);
      gnutls_init (&connection->tls_session,
                   GNUTLS_SERVER);
      gnutls_priority_set (connection->tls_session,
			   daemon->priority_cache);
      switch (daemon->cred_type)
        {
          /* set needed credentials for certificate authentication. */
        case GNUTLS_CRD_CERTIFICATE:
          gnutls_credentials_set (connection->tls_session,
				  GNUTLS_CRD_CERTIFICATE,
				  daemon->x509_cred);
          break;
        default:
#ifdef HAVE_MESSAGES
          MHD_DLOG (connection->daemon,
                    _("Failed to setup TLS credentials: unknown credential type %d\n"),
                    daemon->cred_type);
#endif
          MHD_socket_close_chk_ (client_socket);
          MHD_ip_limit_del (daemon,
                            addr,
                            addrlen);
          free (connection->addr);
          free (connection);
          MHD_PANIC (_("Unknown credential type"));
#if EINVAL
	  errno = EINVAL;
#endif
 	  return MHD_NO;
        }
      gnutls_transport_set_ptr (connection->tls_session,
				(gnutls_transport_ptr_t) connection);
      gnutls_transport_set_pull_function (connection->tls_session,
					  (gnutls_pull_func) &recv_param_adapter);
      gnutls_transport_set_push_function (connection->tls_session,
					  (gnutls_push_func) &send_param_adapter);

      if (daemon->https_mem_trust)
	  gnutls_certificate_server_set_request (connection->tls_session,
						 GNUTLS_CERT_REQUEST);
#else  /* ! HTTPS_SUPPORT */
      eno = EINVAL;
      goto cleanup;
#endif /* ! HTTPS_SUPPORT */
    }


  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
  {
    XDLL_insert (daemon->normal_timeout_head,
                 daemon->normal_timeout_tail,
                 connection);
  }
  DLL_insert (daemon->connections_head,
	      daemon->connections_tail,
	      connection);
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);

  if (NULL != daemon->notify_connection)
    daemon->notify_connection (daemon->notify_connection_cls,
                               connection,
                               &connection->socket_context,
                               MHD_CONNECTION_NOTIFY_STARTED);

  /* attempt to create handler thread */
  if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      if (! MHD_create_named_thread_ (&connection->pid,
                                      "MHD-connection",
                                      daemon->thread_stack_size,
                                      &thread_main_handle_connection,
                                      connection))
        {
	  eno = errno;
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    "Failed to create a thread: %s\n",
                    MHD_strerror_ (eno));
#endif
	  goto cleanup;
        }
    }
  else
    if ( (MHD_YES == external_add) &&
	 (MHD_ITC_IS_VALID_(daemon->itc)) &&
	 (! MHD_itc_activate_ (daemon->itc, "n")) )
      {
#ifdef HAVE_MESSAGES
	MHD_DLOG (daemon,
		  _("Failed to signal new connection via inter-thread communication channel."));
#endif
      }
#ifdef EPOLL_SUPPORT
  if (0 != (daemon->options & MHD_USE_EPOLL))
    {
      if (0 == (daemon->options & MHD_USE_TURBO))
	{
	  struct epoll_event event;

	  event.events = EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLET;
	  event.data.ptr = connection;
	  if (0 != epoll_ctl (daemon->epoll_fd,
			      EPOLL_CTL_ADD,
			      client_socket,
			      &event))
	    {
	      eno = errno;
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("Call to epoll_ctl failed: %s\n"),
                        MHD_socket_last_strerr_ ());
#endif
	      goto cleanup;
	    }
	  connection->epoll_state |= MHD_EPOLL_STATE_IN_EPOLL_SET;
	}
      else
	{
	  connection->epoll_state |= MHD_EPOLL_STATE_READ_READY | MHD_EPOLL_STATE_WRITE_READY
	    | MHD_EPOLL_STATE_IN_EREADY_EDLL;
	  EDLL_insert (daemon->eready_head,
		       daemon->eready_tail,
		       connection);
	}
    }
#endif
  daemon->connections++;
  return MHD_YES;
 cleanup:
  if (NULL != daemon->notify_connection)
    daemon->notify_connection (daemon->notify_connection_cls,
                               connection,
                               &connection->socket_context,
                               MHD_CONNECTION_NOTIFY_CLOSED);
  MHD_socket_close_chk_ (client_socket);
  MHD_ip_limit_del (daemon,
                    addr,
                    addrlen);
  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      XDLL_remove (daemon->normal_timeout_head,
                   daemon->normal_timeout_tail,
                   connection);
    }
  DLL_remove (daemon->connections_head,
	      daemon->connections_tail,
	      connection);
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
  MHD_pool_destroy (connection->pool);
  free (connection->addr);
  free (connection);
  errno = eno;
  return MHD_NO;
}


/**
 * Suspend handling of network data for a given connection.  This can
 * be used to dequeue a connection from MHD's event loop (external
 * select, internal select or thread pool; not applicable to
 * thread-per-connection!) for a while.
 *
 * If you use this API in conjunction with a internal select or a
 * thread pool, you must set the option #MHD_USE_ITC to
 * ensure that a resumed connection is immediately processed by MHD.
 *
 * Suspended connections continue to count against the total number of
 * connections allowed (per daemon, as well as per IP, if such limits
 * are set).  Suspended connections will NOT time out; timeouts will
 * restart when the connection handling is resumed.  While a
 * connection is suspended, MHD will not detect disconnects by the
 * client.
 *
 * The only safe time to suspend a connection is from the
 * #MHD_AccessHandlerCallback.
 *
 * Finally, it is an API violation to call #MHD_stop_daemon while
 * having suspended connections (this will at least create memory and
 * socket leaks or lead to undefined behavior).  You must explicitly
 * resume all connections before stopping the daemon.
 *
 * @remark In thread-per-connection mode: can be called from any thread,
 * in any other mode: to be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param connection the connection to suspend
 */
void
MHD_suspend_connection (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;

  if (0 == (daemon->options & MHD_TEST_ALLOW_SUSPEND_RESUME))
    MHD_PANIC (_("Cannot suspend connections without enabling MHD_ALLOW_SUSPEND_RESUME!\n"));
  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
  if (connection->resuming)
    {
      /* suspending again while we didn't even complete resuming yet */
      connection->resuming = false;
      MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
      return;
    }
  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      if (connection->connection_timeout == daemon->connection_timeout)
        XDLL_remove (daemon->normal_timeout_head,
                     daemon->normal_timeout_tail,
                     connection);
      else
        XDLL_remove (daemon->manual_timeout_head,
                     daemon->manual_timeout_tail,
                     connection);
    }
  DLL_remove (daemon->connections_head,
              daemon->connections_tail,
              connection);
  EXTRA_CHECK (! connection->suspended);
  DLL_insert (daemon->suspended_connections_head,
              daemon->suspended_connections_tail,
              connection);
  connection->suspended = true;
#ifdef EPOLL_SUPPORT
  if (0 != (daemon->options & MHD_USE_EPOLL))
    {
      if (0 != (connection->epoll_state & MHD_EPOLL_STATE_IN_EREADY_EDLL))
        {
          EDLL_remove (daemon->eready_head,
                       daemon->eready_tail,
                       connection);
          connection->epoll_state &= ~MHD_EPOLL_STATE_IN_EREADY_EDLL;
        }
      if (0 != (connection->epoll_state & MHD_EPOLL_STATE_IN_EPOLL_SET))
        {
          if (0 != epoll_ctl (daemon->epoll_fd,
                              EPOLL_CTL_DEL,
                              connection->socket_fd,
                              NULL))
            MHD_PANIC (_("Failed to remove FD from epoll set\n"));
          connection->epoll_state &= ~MHD_EPOLL_STATE_IN_EPOLL_SET;
        }
      connection->epoll_state |= MHD_EPOLL_STATE_SUSPENDED;
    }
#endif
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
}


/**
 * Resume handling of network data for suspended connection.  It is
 * safe to resume a suspended connection at any time.  Calling this function
 * on a connection that was not previously suspended will result
 * in undefined behavior.
 *
 * @param connection the connection to resume
 */
void
MHD_resume_connection (struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon;

  daemon = connection->daemon;
  if (0 == (daemon->options & MHD_TEST_ALLOW_SUSPEND_RESUME))
    MHD_PANIC (_("Cannot resume connections without enabling MHD_ALLOW_SUSPEND_RESUME!\n"));
  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
  connection->resuming = true;
  daemon->resuming = true;
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
  if ( (MHD_ITC_IS_VALID_(daemon->itc)) &&
       (! MHD_itc_activate_ (daemon->itc, "r")) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to signal resume via inter-thread communication channel."));
#endif
    }
}


/**
 * Run through the suspended connections and move any that are no
 * longer suspended back to the active state.
 * @remark To be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param daemon daemon context
 * @return #MHD_YES if a connection was actually resumed
 */
static int
resume_suspended_connections (struct MHD_Daemon *daemon)
{
  struct MHD_Connection *pos;
  struct MHD_Connection *prev = NULL;
  int ret;

  ret = MHD_NO;
  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);

  if (daemon->resuming)
    prev = daemon->suspended_connections_tail;

  EXTRA_CHECK(NULL != next);
  daemon->resuming = false;

  while (NULL != (pos = prev))
    {
#ifdef UPGRADE_SUPPORT
      struct MHD_UpgradeResponseHandle * const urh = pos->urh;
#else  /* ! UPGRADE_SUPPORT */
      static const void * const urh = NULL;
#endif /* ! UPGRADE_SUPPORT */
      prev = pos->prev;
      if ( (! pos->resuming)
#ifdef UPGRADE_SUPPORT
          || ( (NULL != urh) &&
               ( (! urh->was_closed) ||
                 (! urh->clean_ready) ) )
#endif /* UPGRADE_SUPPORT */
         )
        continue;
      ret = MHD_YES;
      EXTRA_CHECK (pos->suspended);
      DLL_remove (daemon->suspended_connections_head,
                  daemon->suspended_connections_tail,
                  pos);
      pos->suspended = false;
      if (NULL == urh)
        {
          DLL_insert (daemon->connections_head,
                      daemon->connections_tail,
                      pos);
          if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
            {
              /* Reset timeout timer on resume. */
              if (0 != pos->connection_timeout)
                pos->last_activity = MHD_monotonic_sec_counter();

              if (pos->connection_timeout == daemon->connection_timeout)
                XDLL_insert (daemon->normal_timeout_head,
                             daemon->normal_timeout_tail,
                             pos);
              else
                XDLL_insert (daemon->manual_timeout_head,
                             daemon->manual_timeout_tail,
                             pos);
            }
#ifdef EPOLL_SUPPORT
          if (0 != (daemon->options & MHD_USE_EPOLL))
            {
              if (0 != (pos->epoll_state & MHD_EPOLL_STATE_IN_EREADY_EDLL))
                MHD_PANIC ("Resumed connection was already in EREADY set\n");
              /* we always mark resumed connections as ready, as we
                 might have missed the edge poll event during suspension */
              EDLL_insert (daemon->eready_head,
                           daemon->eready_tail,
                           pos);
              pos->epoll_state |= MHD_EPOLL_STATE_IN_EREADY_EDLL;
              pos->epoll_state &= ~MHD_EPOLL_STATE_SUSPENDED;
            }
#endif
        }
#ifdef UPGRADE_SUPPORT
      else
        {
          /* Data forwarding was finished (for TLS connections) AND
           * application was closed upgraded connection.
           * Insert connection into cleanup list. */
          DLL_insert (daemon->cleanup_head,
                      daemon->cleanup_tail,
                      pos);

        }
#endif /* UPGRADE_SUPPORT */
      pos->resuming = false;
    }
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
  if ( (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
       (MHD_NO != ret) )
    { /* Wake up suspended connections. */
      if (! MHD_itc_activate_(daemon->itc,
                              "w"))
	{
#ifdef HAVE_MESSAGES
	  MHD_DLOG (daemon,
		    _("Failed to signal resume of connection via inter-thread communication channel."));
#endif
	}
    }
  return ret;
}


/**
 * Add another client connection to the set of connections managed by
 * MHD.  This API is usually not needed (since MHD will accept inbound
 * connections on the server socket).  Use this API in special cases,
 * for example if your HTTP server is behind NAT and needs to connect
 * out to the HTTP client, or if you are building a proxy.
 *
 * If you use this API in conjunction with a internal select or a
 * thread pool, you must set the option
 * #MHD_USE_ITC to ensure that the freshly added
 * connection is immediately processed by MHD.
 *
 * The given client socket will be managed (and closed!) by MHD after
 * this call and must no longer be used directly by the application
 * afterwards.
 *
 * Per-IP connection limits are ignored when using this API.
 *
 * @param daemon daemon that manages the connection
 * @param client_socket socket to manage (MHD will expect
 *        to receive an HTTP request from this socket next).
 * @param addr IP address of the client
 * @param addrlen number of bytes in @a addr
 * @return #MHD_YES on success, #MHD_NO if this daemon could
 *        not handle the connection (i.e. malloc() failed, etc).
 *        The socket will be closed in any case; `errno` is
 *        set to indicate further details about the error.
 * @ingroup specialized
 */
int
MHD_add_connection (struct MHD_Daemon *daemon,
		    MHD_socket client_socket,
		    const struct sockaddr *addr,
		    socklen_t addrlen)
{
  bool sk_nonbl;
  if (! MHD_socket_nonblocking_ (client_socket))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to set nonblocking mode on new client socket: %s\n"),
                MHD_socket_last_strerr_());
#endif
      sk_nonbl = 0;
    }
  else
    sk_nonbl = !0;

  if ( (0 != (daemon->options & MHD_USE_TURBO)) &&
       (! MHD_socket_noninheritable_ (client_socket)) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to set noninheritable mode on new client socket.\n"));
#endif
    }
  return internal_add_connection (daemon,
				  client_socket,
				  addr,
                                  addrlen,
				  MHD_YES,
				  sk_nonbl);
}


/**
 * Accept an incoming connection and create the MHD_Connection object for
 * it.  This function also enforces policy by way of checking with the
 * accept policy callback.
 * @remark To be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param daemon handle with the listen socket
 * @return #MHD_YES on success (connections denied by policy or due
 *         to 'out of memory' and similar errors) are still considered
 *         successful as far as #MHD_accept_connection() is concerned);
 *         a return code of #MHD_NO only refers to the actual
 *         accept() system call.
 */
static int
MHD_accept_connection (struct MHD_Daemon *daemon)
{
#if HAVE_INET6
  struct sockaddr_in6 addrstorage;
#else
  struct sockaddr_in addrstorage;
#endif
  struct sockaddr *addr = (struct sockaddr *) &addrstorage;
  socklen_t addrlen;
  MHD_socket s;
  MHD_socket fd;
  bool sk_nonbl;

  addrlen = sizeof (addrstorage);
  memset (addr,
          0,
          sizeof (addrstorage));
  if ( (MHD_INVALID_SOCKET == (fd = daemon->listen_fd)) ||
       (daemon->was_quiesced) )
    return MHD_NO;
#ifdef USE_ACCEPT4
  s = accept4 (fd,
               addr,
               &addrlen,
               MAYBE_SOCK_CLOEXEC | MAYBE_SOCK_NONBLOCK);
  sk_nonbl = (MAYBE_SOCK_NONBLOCK != 0);
#else  /* ! USE_ACCEPT4 */
  s = accept (fd,
              addr,
              &addrlen);
  sk_nonbl = 0;
#endif /* ! USE_ACCEPT4 */
  if ( (MHD_INVALID_SOCKET == s) ||
       (addrlen <= 0) )
    {
      const int err = MHD_socket_get_error_ ();

      /* This could be a common occurance with multiple worker threads */
      if (MHD_SCKT_ERR_IS_ (err,
                            MHD_SCKT_EINVAL_))
        return MHD_NO; /* can happen during shutdown */
      if (MHD_SCKT_ERR_IS_DISCNN_BEFORE_ACCEPT_(err))
        return MHD_NO; /* do not print error if client just disconnected early */
#ifdef HAVE_MESSAGES
      if (! MHD_SCKT_ERR_IS_EAGAIN_ (err) )
        MHD_DLOG (daemon,
		  _("Error accepting connection: %s\n"),
		  MHD_socket_strerr_(err));
#endif
      if (MHD_INVALID_SOCKET != s)
        {
          MHD_socket_close_chk_ (s);
        }
      if ( MHD_SCKT_ERR_IS_LOW_RESOURCES_ (err) )
        {
          /* system/process out of resources */
          if (0 == daemon->connections)
            {
#ifdef HAVE_MESSAGES
              /* Not setting 'at_limit' flag, as there is no way it
                 would ever be cleared.  Instead trying to produce
                 bit fat ugly warning. */
              MHD_DLOG (daemon,
                        _("Hit process or system resource limit at FIRST connection. This is really bad as there is no sane way to proceed. Will try busy waiting for system resources to become magically available.\n"));
#endif
            }
          else
            {
              daemon->at_limit = true;
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("Hit process or system resource limit at %u connections, temporarily suspending accept(). Consider setting a lower MHD_OPTION_CONNECTION_LIMIT.\n"),
                        (unsigned int) daemon->connections);
#endif
            }
        }
      return MHD_NO;
    }
#if !defined(USE_ACCEPT4) || !defined(HAVE_SOCK_NONBLOCK)
  if (! MHD_socket_nonblocking_ (s))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to set nonblocking mode on incoming connection socket: %s\n"),
                MHD_socket_last_strerr_());
#endif
    }
  else
    sk_nonbl = !0;
#endif /* !USE_ACCEPT4 || !HAVE_SOCK_NONBLOCK */
#if !defined(USE_ACCEPT4) || !defined(SOCK_CLOEXEC)
  if (! MHD_socket_noninheritable_ (s))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to set noninheritable mode on incoming connection socket.\n"));
#endif
    }
#endif /* !USE_ACCEPT4 || !SOCK_CLOEXEC */
#ifdef HAVE_MESSAGES
#if DEBUG_CONNECT
  MHD_DLOG (daemon,
            _("Accepted connection on socket %d\n"),
            s);
#endif
#endif
  (void) internal_add_connection (daemon,
                                  s,
				  addr,
                                  addrlen,
				  MHD_NO,
                                  sk_nonbl);
  return MHD_YES;
}


/**
 * Free resources associated with all closed connections.
 * (destroy responses, free buffers, etc.).  All closed
 * connections are kept in the "cleanup" doubly-linked list.
 * @remark To be called only from thread that
 * process daemon's select()/poll()/etc.
 *
 * @param daemon daemon to clean up
 */
static void
MHD_cleanup_connections (struct MHD_Daemon *daemon)
{
  struct MHD_Connection *pos;

  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
  while (NULL != (pos = daemon->cleanup_tail))
    {
      DLL_remove (daemon->cleanup_head,
		  daemon->cleanup_tail,
		  pos);
      MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);

      if ( (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
	   (! pos->thread_joined) &&
           (! MHD_join_thread_ (pos->pid)) )
        MHD_PANIC (_("Failed to join a thread\n"));
#ifdef UPGRADE_SUPPORT
      cleanup_upgraded_connection (pos);
#endif /* UPGRADE_SUPPORT */
      MHD_pool_destroy (pos->pool);
#ifdef HTTPS_SUPPORT
      if (NULL != pos->tls_session)
	gnutls_deinit (pos->tls_session);
#endif /* HTTPS_SUPPORT */
      daemon->connections--;
      daemon->at_limit = false;

      /* clean up the connection */
      if (NULL != daemon->notify_connection)
        daemon->notify_connection (daemon->notify_connection_cls,
                                   pos,
                                   &pos->socket_context,
                                   MHD_CONNECTION_NOTIFY_CLOSED);
      MHD_ip_limit_del (daemon,
                        pos->addr,
                        pos->addr_len);
#ifdef EPOLL_SUPPORT
      if (0 != (daemon->options & MHD_USE_EPOLL))
        {
          if (0 != (pos->epoll_state & MHD_EPOLL_STATE_IN_EREADY_EDLL))
            {
              EDLL_remove (daemon->eready_head,
                           daemon->eready_tail,
                           pos);
              pos->epoll_state &= ~MHD_EPOLL_STATE_IN_EREADY_EDLL;
            }
          if ( (-1 != daemon->epoll_fd) &&
               (0 != (pos->epoll_state & MHD_EPOLL_STATE_IN_EPOLL_SET)) )
            {
              /* epoll documentation suggests that closing a FD
                 automatically removes it from the epoll set; however,
                 this is not true as if we fail to do manually remove it,
                 we are still seeing an event for this fd in epoll,
                 causing grief (use-after-free...) --- at least on my
                 system. */
              if (0 != epoll_ctl (daemon->epoll_fd,
                                  EPOLL_CTL_DEL,
                                  pos->socket_fd,
                                  NULL))
                MHD_PANIC (_("Failed to remove FD from epoll set\n"));
              pos->epoll_state &= ~MHD_EPOLL_STATE_IN_EPOLL_SET;
            }
        }
#endif
      if (NULL != pos->response)
	{
	  MHD_destroy_response (pos->response);
	  pos->response = NULL;
	}
      if (MHD_INVALID_SOCKET != pos->socket_fd)
        MHD_socket_close_chk_ (pos->socket_fd);
      if (NULL != pos->addr)
	free (pos->addr);
      free (pos);

      MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
    }
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
}


/**
 * Obtain timeout value for `select()` for this daemon (only needed if
 * connection timeout is used).  The returned value is how long
 * `select()` or `poll()` should at most block, not the timeout value set
 * for connections.  This function MUST NOT be called if MHD is
 * running with #MHD_USE_THREAD_PER_CONNECTION.
 * @remark To be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param daemon daemon to query for timeout
 * @param timeout set to the timeout (in milliseconds)
 * @return #MHD_YES on success, #MHD_NO if timeouts are
 *        not used (or no connections exist that would
 *        necessitate the use of a timeout right now).
 * @ingroup event
 */
int
MHD_get_timeout (struct MHD_Daemon *daemon,
		 MHD_UNSIGNED_LONG_LONG *timeout)
{
  time_t earliest_deadline;
  time_t now;
  struct MHD_Connection *pos;
  bool have_timeout;

  if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Illegal call to MHD_get_timeout\n"));
#endif
      return MHD_NO;
    }

  if (daemon->data_already_pending)
    {
      /* Some data already waiting to be processed. */
      *timeout = 0;
      return MHD_YES;
    }

#ifdef EPOLL_SUPPORT
  if ( (0 != (daemon->options & MHD_USE_EPOLL)) &&
       ((NULL != daemon->eready_head)
#if defined(UPGRADE_SUPPORT) && defined(HTTPS_SUPPORT)
	 || (NULL != daemon->eready_urh_head)
#endif /* UPGRADE_SUPPORT && HTTPS_SUPPORT */
	 ) )
    {
	  /* Some connection(s) already have some data pending. */
      *timeout = 0;
      return MHD_YES;
    }
#endif /* EPOLL_SUPPORT */

  have_timeout = false;
  earliest_deadline = 0; /* avoid compiler warnings */
  for (pos = daemon->manual_timeout_tail; NULL != pos; pos = pos->prevX)
    {
      if (0 != pos->connection_timeout)
	{
	  if ( (! have_timeout) ||
	       (earliest_deadline - pos->last_activity > pos->connection_timeout) )
	    earliest_deadline = pos->last_activity + pos->connection_timeout;
	  have_timeout = true;
	}
    }
  /* normal timeouts are sorted, so we only need to look at the 'tail' (oldest) */
  pos = daemon->normal_timeout_tail;
  if ( (NULL != pos) &&
       (0 != pos->connection_timeout) )
    {
      if ( (! have_timeout) ||
	   (earliest_deadline - pos->connection_timeout > pos->last_activity) )
	earliest_deadline = pos->last_activity + pos->connection_timeout;
      have_timeout = true;
    }

  if (! have_timeout)
    return MHD_NO;
  now = MHD_monotonic_sec_counter();
  if (earliest_deadline < now)
    *timeout = 0;
  else
    {
      const time_t second_left = earliest_deadline - now;
      if (second_left > ULLONG_MAX / 1000) /* Ignore compiler warning: 'second_left' is always positive. */
        *timeout = ULLONG_MAX;
      else
        *timeout = 1000LL * second_left;
  }
  return MHD_YES;
}


/**
 * Internal version of #MHD_run_from_select().
 *
 * @param daemon daemon to run select loop for
 * @param read_fd_set read set
 * @param write_fd_set write set
 * @param except_fd_set except set (not used, can be NULL)
 * @return #MHD_NO on serious errors, #MHD_YES on success
 * @ingroup event
 */
static int
internal_run_from_select (struct MHD_Daemon *daemon,
                          const fd_set *read_fd_set,
                          const fd_set *write_fd_set,
                          const fd_set *except_fd_set)
{
  MHD_socket ds;
  struct MHD_Connection *pos;
  struct MHD_Connection *prev;
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  struct MHD_UpgradeResponseHandle *urh;
  struct MHD_UpgradeResponseHandle *urhn;
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
  /* Reset. New value will be set when connections are processed. */
  /* Note: no-op for thread-per-connection as it is always false in that mode. */
  daemon->data_already_pending = false;

  /* Clear ITC to avoid spinning select */
  /* Do it before any other processing so new signals
     will trigger select again and will be processed */
  if ( (MHD_ITC_IS_VALID_(daemon->itc)) &&
       (FD_ISSET (MHD_itc_r_fd_ (daemon->itc),
                  read_fd_set)) )
    MHD_itc_clear_ (daemon->itc);

#ifdef EPOLL_SUPPORT
  if (0 != (daemon->options & MHD_USE_EPOLL))
    {
      /* we're in epoll mode, the epoll FD stands for
	 the entire event set! */
      if (! MHD_SCKT_FD_FITS_FDSET_(daemon->epoll_fd,
                                    NULL))
	return MHD_NO; /* poll fd too big, fail hard */
      if (FD_ISSET (daemon->epoll_fd,
                    read_fd_set))
	return MHD_run (daemon);
      return MHD_YES;
    }
#endif

  /* select connection thread handling type */
  if ( (MHD_INVALID_SOCKET != (ds = daemon->listen_fd)) &&
       (! daemon->was_quiesced) &&
       (FD_ISSET (ds,
                  read_fd_set)) )
    (void) MHD_accept_connection (daemon);

  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      /* do not have a thread per connection, process all connections now */
      prev = daemon->connections_tail;
      while (NULL != (pos = prev))
        {
	  prev = pos->prev;
          ds = pos->socket_fd;
          if (MHD_INVALID_SOCKET == ds)
	    continue;
          call_handlers (pos,
                         FD_ISSET (ds,
                                   read_fd_set),
                         FD_ISSET (ds,
                                   write_fd_set),
                         FD_ISSET (ds,
                                   except_fd_set));
        }
    }

#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  /* handle upgraded HTTPS connections */
  for (urh = daemon->urh_tail; NULL != urh; urh = urhn)
    {
      urhn = urh->prev;
      /* update urh state based on select() output */
      urh_from_fdset (urh,
                      read_fd_set,
                      write_fd_set,
                      except_fd_set);
      /* call generic forwarding function for passing data */
      process_urh (urh);
      /* Finished forwarding? */
      if ( (0 == urh->in_buffer_size) &&
           (0 == urh->out_buffer_size) &&
           (0 == urh->in_buffer_used) &&
           (0 == urh->out_buffer_used) )
        {
          MHD_connection_finish_forward_ (urh->connection);
          urh->clean_ready = true;
          /* Resuming will move connection to cleanup list. */
          MHD_resume_connection(urh->connection);
        }
    }
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
  MHD_cleanup_connections (daemon);
  return MHD_YES;
}


/**
 * Run webserver operations. This method should be called by clients
 * in combination with #MHD_get_fdset if the client-controlled select
 * method is used.
 *
 * You can use this function instead of #MHD_run if you called
 * `select()` on the result from #MHD_get_fdset.  File descriptors in
 * the sets that are not controlled by MHD will be ignored.  Calling
 * this function instead of #MHD_run is more efficient as MHD will
 * not have to call `select()` again to determine which operations are
 * ready.
 *
 * This function cannot be used with daemon started with
 * MHD_USE_INTERNAL_POLLING_THREAD flag.
 *
 * @param daemon daemon to run select loop for
 * @param read_fd_set read set
 * @param write_fd_set write set
 * @param except_fd_set except set
 * @return #MHD_NO on serious errors, #MHD_YES on success
 * @ingroup event
 */
int
MHD_run_from_select (struct MHD_Daemon *daemon,
                     const fd_set *read_fd_set,
                     const fd_set *write_fd_set,
                     const fd_set *except_fd_set)
{
  fd_set es;
  if (0 != (daemon->options &
        (MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_POLL)) )
    return MHD_NO;
  if (NULL == read_fd_set || NULL == write_fd_set)
    return MHD_NO;
  if (NULL == except_fd_set)
    { /* Workaround to maintain backward compatibility. */
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("MHD_run_from_select() called with except_fd_set "
                  "set to NULL. Such behavior is deprecated.\n"));
#endif
      FD_ZERO (&es);
      except_fd_set = &es;
    }
  if (0 != (daemon->options & MHD_USE_EPOLL))
    {
#ifdef EPOLL_SUPPORT
      int ret;
      ret = MHD_epoll (daemon, MHD_NO);
      MHD_cleanup_connections (daemon);
      return ret;
#else  /* ! EPOLL_SUPPORT */
      return MHD_NO;
#endif /* ! EPOLL_SUPPORT */
    }

  /* Resuming external connections when using an extern mainloop  */
  resume_suspended_connections (daemon);

  return internal_run_from_select (daemon, read_fd_set,
                                   write_fd_set, except_fd_set);
}


/**
 * Main internal select() call.  Will compute select sets, call select()
 * and then #internal_run_from_select with the result.
 *
 * @param daemon daemon to run select() loop for
 * @param may_block #MHD_YES if blocking, #MHD_NO if non-blocking
 * @return #MHD_NO on serious errors, #MHD_YES on success
 */
static int
MHD_select (struct MHD_Daemon *daemon,
	    int may_block)
{
  int num_ready;
  fd_set rs;
  fd_set ws;
  fd_set es;
  MHD_socket maxsock;
  struct timeval timeout;
  struct timeval *tv;
  MHD_UNSIGNED_LONG_LONG ltimeout;
  int err_state;
  MHD_socket ls;

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  if (daemon->shutdown)
    return MHD_NO;
  FD_ZERO (&rs);
  FD_ZERO (&ws);
  FD_ZERO (&es);
  maxsock = MHD_INVALID_SOCKET;
  err_state = MHD_NO;
  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      if ( (0 != (daemon->options & MHD_TEST_ALLOW_SUSPEND_RESUME)) &&
           (MHD_YES == resume_suspended_connections (daemon)) )
        may_block = MHD_NO;

      /* single-threaded, go over everything */
      if (MHD_NO ==
          internal_get_fdset2 (daemon,
                               &rs,
                               &ws,
                               &es,
                               &maxsock,
                               FD_SETSIZE))
        {
#ifdef HAVE_MESSAGES
        MHD_DLOG (daemon,
                  _("Could not obtain daemon fdsets"));
#endif
          err_state = MHD_YES;
        }
    }
  else
    {
      /* accept only, have one thread per connection */
      if ( (MHD_INVALID_SOCKET != (ls = daemon->listen_fd)) &&
           (! daemon->was_quiesced) &&
           (! MHD_add_to_fd_set_ (ls,
                                  &rs,
                                  &maxsock,
                                  FD_SETSIZE)) )
        {
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Could not add listen socket to fdset"));
#endif
          return MHD_NO;
        }
    }
  if ( (MHD_ITC_IS_VALID_(daemon->itc)) &&
       (! MHD_add_to_fd_set_ (MHD_itc_r_fd_ (daemon->itc),
                              &rs,
                              &maxsock,
                              FD_SETSIZE)) )
    {
#if defined(MHD_WINSOCK_SOCKETS)
      /* fdset limit reached, new connections
         cannot be handled. Remove listen socket FD
         from fdset and retry to add ITC FD. */
      if ( (MHD_INVALID_SOCKET != (ls = daemon->listen_fd)) &&
           (! daemon->was_quiesced) )
        {
          FD_CLR (ls,
                  &rs);
          if (! MHD_add_to_fd_set_ (MHD_itc_r_fd_(daemon->itc),
                                    &rs,
                                    &maxsock,
                                    FD_SETSIZE))
            {
#endif /* MHD_WINSOCK_SOCKETS */
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("Could not add control inter-thread communication channel FD to fdset"));
#endif
              err_state = MHD_YES;
#if defined(MHD_WINSOCK_SOCKETS)
            }
        }
#endif /* MHD_WINSOCK_SOCKETS */
    }
  /* Stop listening if we are at the configured connection limit */
  /* If we're at the connection limit, no point in really
     accepting new connections; however, make sure we do not miss
     the shutdown OR the termination of an existing connection; so
     only do this optimization if we have a signaling ITC in
     place. */
  if ( (MHD_INVALID_SOCKET != (ls = daemon->listen_fd)) &&
       (MHD_ITC_IS_VALID_(daemon->itc)) &&
       ( (daemon->connections == daemon->connection_limit) ||
         (daemon->at_limit) ) )
    {
      FD_CLR (ls,
              &rs);
    }
  tv = NULL;
  if (MHD_YES == err_state)
    may_block = MHD_NO;
  if (MHD_NO == may_block)
    {
      timeout.tv_usec = 0;
      timeout.tv_sec = 0;
      tv = &timeout;
    }
  else if ( (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
	    (MHD_YES == MHD_get_timeout (daemon, &ltimeout)) )
    {
      /* ltimeout is in ms */
      timeout.tv_usec = (ltimeout % 1000) * 1000;
      if (ltimeout / 1000 > TIMEVAL_TV_SEC_MAX)
        timeout.tv_sec = TIMEVAL_TV_SEC_MAX;
      else
        timeout.tv_sec = (_MHD_TIMEVAL_TV_SEC_TYPE)(ltimeout / 1000);
      tv = &timeout;
    }
  num_ready = MHD_SYS_select_ (maxsock + 1,
                               &rs,
                               &ws,
                               &es,
                               tv);
  if (daemon->shutdown)
    return MHD_NO;
  if (num_ready < 0)
    {
      const int err = MHD_socket_get_error_ ();
      if (MHD_SCKT_ERR_IS_EINTR_(err))
        return (MHD_NO == err_state) ? MHD_YES : MHD_NO;
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("select failed: %s\n"),
                MHD_socket_strerr_ (err));
#endif
      return MHD_NO;
    }
  if (MHD_YES == internal_run_from_select (daemon,
                                           &rs,
                                           &ws,
                                           &es))
    return (MHD_NO == err_state) ? MHD_YES : MHD_NO;
  return MHD_NO;
}


#ifdef HAVE_POLL
/**
 * Process all of our connections and possibly the server
 * socket using poll().
 *
 * @param daemon daemon to run poll loop for
 * @param may_block #MHD_YES if blocking, #MHD_NO if non-blocking
 * @return #MHD_NO on serious errors, #MHD_YES on success
 */
static int
MHD_poll_all (struct MHD_Daemon *daemon,
	      int may_block)
{
  unsigned int num_connections;
  struct MHD_Connection *pos;
  struct MHD_Connection *prev;
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  struct MHD_UpgradeResponseHandle *urh;
  struct MHD_UpgradeResponseHandle *urhn;
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */

  if ( (0 != (daemon->options & MHD_TEST_ALLOW_SUSPEND_RESUME)) &&
       (MHD_YES == resume_suspended_connections (daemon)) )
    may_block = MHD_NO;

  /* count number of connections and thus determine poll set size */
  num_connections = 0;
  for (pos = daemon->connections_head; NULL != pos; pos = pos->next)
    num_connections++;
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  for (urh = daemon->urh_head; NULL != urh; urh = urh->next)
    num_connections += 2;
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
  {
    MHD_UNSIGNED_LONG_LONG ltimeout;
    unsigned int i;
    int timeout;
    unsigned int poll_server;
    int poll_listen;
    int poll_itc_idx;
    struct pollfd *p;
    MHD_socket ls;

    p = MHD_calloc_ ((2 + num_connections), sizeof (struct pollfd));
    if (NULL == p)
      {
#ifdef HAVE_MESSAGES
        MHD_DLOG (daemon,
                  _("Error allocating memory: %s\n"),
                  MHD_strerror_(errno));
#endif
        return MHD_NO;
      }
    poll_server = 0;
    poll_listen = -1;
    if ( (MHD_INVALID_SOCKET != (ls = daemon->listen_fd)) &&
         (! daemon->was_quiesced) &&
	 (daemon->connections < daemon->connection_limit) &&
         (! daemon->at_limit) )
      {
	/* only listen if we are not at the connection limit */
	p[poll_server].fd = ls;
	p[poll_server].events = POLLIN;
	p[poll_server].revents = 0;
	poll_listen = (int) poll_server;
	poll_server++;
      }
    poll_itc_idx = -1;
    if (MHD_ITC_IS_VALID_(daemon->itc))
      {
	p[poll_server].fd = MHD_itc_r_fd_ (daemon->itc);
	p[poll_server].events = POLLIN;
	p[poll_server].revents = 0;
        poll_itc_idx = (int) poll_server;
	poll_server++;
      }
    if (may_block == MHD_NO)
      timeout = 0;
    else if ( (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) ||
	      (MHD_YES != MHD_get_timeout (daemon,
                                           &ltimeout)) )
      timeout = -1;
    else
      timeout = (ltimeout > INT_MAX) ? INT_MAX : (int) ltimeout;

    i = 0;
    for (pos = daemon->connections_tail; NULL != pos; pos = pos->prev)
      {
	p[poll_server+i].fd = pos->socket_fd;
	switch (pos->event_loop_info)
	  {
	  case MHD_EVENT_LOOP_INFO_READ:
	    p[poll_server+i].events |= POLLIN | MHD_POLL_EVENTS_ERR_DISC;
	    break;
	  case MHD_EVENT_LOOP_INFO_WRITE:
	    p[poll_server+i].events |= POLLOUT | MHD_POLL_EVENTS_ERR_DISC;
	    break;
	  case MHD_EVENT_LOOP_INFO_BLOCK:
	    p[poll_server+i].events |=  MHD_POLL_EVENTS_ERR_DISC;
	    break;
	  case MHD_EVENT_LOOP_INFO_CLEANUP:
	    timeout = 0; /* clean up "pos" immediately */
	    break;
	  }
	i++;
      }
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
    for (urh = daemon->urh_tail; NULL != urh; urh = urh->prev)
      {
        urh_to_pollfd(urh, &(p[poll_server+i]));
        i += 2;
      }
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
    if (0 == poll_server + num_connections)
      {
        free(p);
        return MHD_YES;
      }
    if (MHD_sys_poll_(p,
                      poll_server + num_connections,
                      timeout) < 0)
      {
        const int err = MHD_socket_get_error_ ();
	if (MHD_SCKT_ERR_IS_EINTR_ (err))
      {
        free(p);
        return MHD_YES;
      }
#ifdef HAVE_MESSAGES
	MHD_DLOG (daemon,
		  _("poll failed: %s\n"),
		  MHD_socket_strerr_ (err));
#endif
        free(p);
	return MHD_NO;
      }

    /* Reset. New value will be set when connections are processed. */
    daemon->data_already_pending = false;

    /* handle ITC FD */
    /* do it before any other processing so
       new signals will be processed in next loop */
    if ( (-1 != poll_itc_idx) &&
         (0 != (p[poll_itc_idx].revents & POLLIN)) )
      MHD_itc_clear_ (daemon->itc);

    /* handle shutdown */
    if (daemon->shutdown)
      {
        free(p);
        return MHD_NO;
      }
    i = 0;
    prev = daemon->connections_tail;
    while (NULL != (pos = prev))
      {
	prev = pos->prev;
        /* first, sanity checks */
        if (i >= num_connections)
          break; /* connection list changed somehow, retry later ... */
        if (p[poll_server+i].fd != pos->socket_fd)
          continue; /* fd mismatch, something else happened, retry later ... */
        call_handlers (pos,
                       0 != (p[poll_server+i].revents & POLLIN),
                       0 != (p[poll_server+i].revents & POLLOUT),
                       0 != (p[poll_server+i].revents & MHD_POLL_REVENTS_ERR_DISC));
        i++;
      }
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
    for (urh = daemon->urh_tail; NULL != urh; urh = urhn)
      {
        if (i >= num_connections)
          break; /* connection list changed somehow, retry later ... */

        /* Get next connection here as connection can be removed
         * from 'daemon->urh_head' list. */
        urhn = urh->prev;
        /* Check for fd mismatch. FIXME: required for safety? */
        if ((p[poll_server+i].fd != urh->connection->socket_fd) ||
            (p[poll_server+i+1].fd != urh->mhd.socket))
          break;
        urh_from_pollfd(urh, &(p[poll_server+i]));
        i += 2;
        process_urh (urh);
        /* Finished forwarding? */
        if ( (0 == urh->in_buffer_size) &&
             (0 == urh->out_buffer_size) &&
             (0 == urh->in_buffer_used) &&
             (0 == urh->out_buffer_used) )
          {
            /* MHD_connection_finish_forward_() will remove connection from
             * 'daemon->urh_head' list. */
            MHD_connection_finish_forward_ (urh->connection);
            urh->clean_ready = true;
            /* If 'urh->was_closed' already was set to true, connection will be
             * moved immediately to cleanup list. Otherwise connection
             * will stay in suspended list until 'urh' will be marked
             * with 'was_closed' by application. */
            MHD_resume_connection(urh->connection);
          }
      }
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
    /* handle 'listen' FD */
    if ( (-1 != poll_listen) &&
	 (0 != (p[poll_listen].revents & POLLIN)) )
      (void) MHD_accept_connection (daemon);

    free(p);
  }
  return MHD_YES;
}


/**
 * Process only the listen socket using poll().
 *
 * @param daemon daemon to run poll loop for
 * @param may_block #MHD_YES if blocking, #MHD_NO if non-blocking
 * @return #MHD_NO on serious errors, #MHD_YES on success
 */
static int
MHD_poll_listen_socket (struct MHD_Daemon *daemon,
			int may_block)
{
  struct pollfd p[2];
  int timeout;
  unsigned int poll_count;
  int poll_listen;
  int poll_itc_idx;
  MHD_socket ls;

  memset (&p,
          0,
          sizeof (p));
  poll_count = 0;
  poll_listen = -1;
  poll_itc_idx = -1;
  if ( (MHD_INVALID_SOCKET != (ls = daemon->listen_fd)) &&
       (! daemon->was_quiesced) )

    {
      p[poll_count].fd = ls;
      p[poll_count].events = POLLIN;
      p[poll_count].revents = 0;
      poll_listen = poll_count;
      poll_count++;
    }
  if (MHD_ITC_IS_VALID_(daemon->itc))
    {
      p[poll_count].fd = MHD_itc_r_fd_ (daemon->itc);
      p[poll_count].events = POLLIN;
      p[poll_count].revents = 0;
      poll_itc_idx = poll_count;
      poll_count++;
    }
  if (MHD_NO == may_block)
    timeout = 0;
  else
    timeout = -1;
  if (0 == poll_count)
    return MHD_YES;
  if (MHD_sys_poll_(p,
                    poll_count,
                    timeout) < 0)
    {
      const int err = MHD_socket_get_error_ ();

      if (MHD_SCKT_ERR_IS_EINTR_ (err))
	return MHD_YES;
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("poll failed: %s\n"),
                MHD_socket_strerr_ (err));
#endif
      return MHD_NO;
    }
  if ( (-1 != poll_itc_idx) &&
       (0 != (p[poll_itc_idx].revents & POLLIN)) )
    MHD_itc_clear_ (daemon->itc);

  /* handle shutdown */
  if (daemon->shutdown)
    return MHD_NO;
  if ( (-1 != poll_listen) &&
       (0 != (p[poll_listen].revents & POLLIN)) )
    (void) MHD_accept_connection (daemon);
  return MHD_YES;
}
#endif


/**
 * Do poll()-based processing.
 *
 * @param daemon daemon to run poll()-loop for
 * @param may_block #MHD_YES if blocking, #MHD_NO if non-blocking
 * @return #MHD_NO on serious errors, #MHD_YES on success
 */
static int
MHD_poll (struct MHD_Daemon *daemon,
	  int may_block)
{
#ifdef HAVE_POLL
  if (daemon->shutdown)
    return MHD_NO;
  if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    return MHD_poll_all (daemon,
                         may_block);
  return MHD_poll_listen_socket (daemon,
                                 may_block);
#else
  return MHD_NO;
#endif
}


#ifdef EPOLL_SUPPORT

/**
 * How many events to we process at most per epoll() call?  Trade-off
 * between required stack-size and number of system calls we have to
 * make; 128 should be way enough to avoid more than one system call
 * for most scenarios, and still be moderate in stack size
 * consumption.  Embedded systems might want to choose a smaller value
 * --- but why use epoll() on such a system in the first place?
 */
#define MAX_EVENTS 128


#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)

/**
 * Checks whether @a urh has some data to process.
 *
 * @param urh upgrade handler to analyse
 * @return 'true' if @a urh has some data to process,
 *         'false' otherwise
 */
static bool
is_urh_ready(struct MHD_UpgradeResponseHandle * const urh)
{
  const struct MHD_Connection * const connection = urh->connection;

  if ( (0 == urh->in_buffer_size) &&
       (0 == urh->out_buffer_size) &&
       (0 == urh->in_buffer_used) &&
       (0 == urh->out_buffer_used) )
    return false;

  if (connection->daemon->shutdown)
    return true;

  if ( ( (0 != (MHD_EPOLL_STATE_READ_READY & urh->app.celi)) ||
         (connection->tls_read_ready) ) &&
       (urh->in_buffer_used < urh->in_buffer_size) )
    return true;

  if ( (0 != (MHD_EPOLL_STATE_READ_READY & urh->mhd.celi)) &&
       (urh->out_buffer_used < urh->out_buffer_size) )
    return true;

  if ( (0 != (MHD_EPOLL_STATE_WRITE_READY & urh->app.celi)) &&
       (urh->out_buffer_used > 0) )
    return true;

  if ( (0 != (MHD_EPOLL_STATE_WRITE_READY & urh->mhd.celi)) &&
         (urh->in_buffer_used > 0) )
    return true;

  return false;
}

/**
 * Do epoll()-based processing for TLS connections that have been
 * upgraded.  This requires a separate epoll() invocation as we
 * cannot use the `struct MHD_Connection` data structures for
 * the `union epoll_data` in this case.
 * @remark To be called only from thread that process
 * daemon's select()/poll()/etc.
 */
static int
run_epoll_for_upgrade (struct MHD_Daemon *daemon)
{
  struct epoll_event events[MAX_EVENTS];
  int num_events;
  struct MHD_UpgradeResponseHandle * pos;
  struct MHD_UpgradeResponseHandle * prev;

  num_events = MAX_EVENTS;
  while (MAX_EVENTS == num_events)
    {
      unsigned int i;
      /* update event masks */
      num_events = epoll_wait (daemon->epoll_upgrade_fd,
			       events,
                               MAX_EVENTS,
                               0);
      if (-1 == num_events)
	{
          const int err = MHD_socket_get_error_ ();
          if (MHD_SCKT_ERR_IS_EINTR_ (err))
	    return MHD_YES;
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Call to epoll_wait failed: %s\n"),
                    MHD_socket_strerr_ (err));
#endif
	  return MHD_NO;
	}
      for (i = 0; i < (unsigned int) num_events; i++)
	{
          struct UpgradeEpollHandle * const ueh = events[i].data.ptr;
          struct MHD_UpgradeResponseHandle * const urh = ueh->urh;
          bool new_err_state = false;

          if (urh->clean_ready)
            continue;

          /* Update ueh state based on what is ready according to epoll() */
          if (0 != (events[i].events & EPOLLIN))
            ueh->celi |= MHD_EPOLL_STATE_READ_READY;
          if (0 != (events[i].events & EPOLLOUT))
            ueh->celi |= MHD_EPOLL_STATE_WRITE_READY;
          if (0 != (events[i].events & EPOLLHUP))
            ueh->celi |= MHD_EPOLL_STATE_READ_READY | MHD_EPOLL_STATE_WRITE_READY;

          if ( (0 == (ueh->celi & MHD_EPOLL_STATE_ERROR)) &&
               (0 != (events[i].events & (EPOLLERR | EPOLLPRI))) )
	    {
              /* Process new error state only one time
               * and avoid continuously marking this connection
               * as 'ready'. */
              ueh->celi |= MHD_EPOLL_STATE_ERROR;
              new_err_state = true;
	    }

          if (! urh->in_eready_list)
            {
              if (new_err_state ||
        	  is_urh_ready(urh))
        	{
        	  EDLL_insert (daemon->eready_urh_head,
			       daemon->eready_urh_tail,
			       urh);
        	  urh->in_eready_list = true;
        	}
            }
        }
    }
  prev = daemon->eready_urh_tail;
  while (NULL != (pos = prev))
    {
      prev = pos->prevE;
      process_urh (pos);
      if (! is_urh_ready(pos))
      	{
      	  EDLL_remove (daemon->eready_urh_head,
      		       daemon->eready_urh_tail,
      		       pos);
      	  pos->in_eready_list = false;
      	}
      /* Finished forwarding? */
      if ( (0 == pos->in_buffer_size) &&
           (0 == pos->out_buffer_size) &&
           (0 == pos->in_buffer_used) &&
           (0 == pos->out_buffer_used) )
        {
          MHD_connection_finish_forward_ (pos->connection);
          pos->clean_ready = true;
          /* If 'pos->was_closed' already was set to true, connection
           * will be moved immediately to cleanup list. Otherwise
           * connection will stay in suspended list until 'pos' will
           * be marked with 'was_closed' by application. */
          MHD_resume_connection(pos->connection);
        }
    }

  return MHD_YES;
}
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */

/**
 * Pointer-marker to distinguish ITC slot in epoll sets.
 */
static const char * const epoll_itc_marker = "itc_marker";

/**
 * Do epoll()-based processing (this function is allowed to
 * block if @a may_block is set to #MHD_YES).
 *
 * @param daemon daemon to run poll loop for
 * @param may_block #MHD_YES if blocking, #MHD_NO if non-blocking
 * @return #MHD_NO on serious errors, #MHD_YES on success
 */
static int
MHD_epoll (struct MHD_Daemon *daemon,
	   int may_block)
{
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  static const char * const upgrade_marker = "upgrade_ptr";
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
  struct MHD_Connection *pos;
  struct MHD_Connection *prev;
  struct epoll_event events[MAX_EVENTS];
  struct epoll_event event;
  int timeout_ms;
  MHD_UNSIGNED_LONG_LONG timeout_ll;
  int num_events;
  unsigned int i;
  MHD_socket ls;
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  int run_upgraded = MHD_NO;
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */

  if (-1 == daemon->epoll_fd)
    return MHD_NO; /* we're down! */
  if (daemon->shutdown)
    return MHD_NO;
  if ( (MHD_INVALID_SOCKET != (ls = daemon->listen_fd)) &&
       (! daemon->was_quiesced) &&
       (daemon->connections < daemon->connection_limit) &&
       (! daemon->listen_socket_in_epoll) &&
       (! daemon->at_limit) )
    {
      event.events = EPOLLIN;
      event.data.ptr = daemon;
      if (0 != epoll_ctl (daemon->epoll_fd,
			  EPOLL_CTL_ADD,
			  ls,
			  &event))
	{
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Call to epoll_ctl failed: %s\n"),
                    MHD_socket_last_strerr_ ());
#endif
	  return MHD_NO;
	}
      daemon->listen_socket_in_epoll = true;
    }
  if ( (daemon->was_quiesced) &&
       (daemon->listen_socket_in_epoll) )
  {
    if (0 != epoll_ctl (daemon->epoll_fd,
                        EPOLL_CTL_DEL,
                        ls,
                        NULL))
      MHD_PANIC ("Failed to remove listen FD from epoll set\n");
    daemon->listen_socket_in_epoll = false;
  }

#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  if ( (! daemon->upgrade_fd_in_epoll) &&
       (-1 != daemon->epoll_upgrade_fd) )
    {
      event.events = EPOLLIN | EPOLLOUT;
      event.data.ptr = (void *) upgrade_marker;
      if (0 != epoll_ctl (daemon->epoll_fd,
			  EPOLL_CTL_ADD,
			  daemon->epoll_upgrade_fd,
			  &event))
	{
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Call to epoll_ctl failed: %s\n"),
                    MHD_socket_last_strerr_ ());
#endif
	  return MHD_NO;
	}
      daemon->upgrade_fd_in_epoll = true;
    }
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
  if ( (daemon->listen_socket_in_epoll) &&
       ( (daemon->connections == daemon->connection_limit) ||
         (daemon->at_limit) ||
         (daemon->was_quiesced) ) )
    {
      /* we're at the connection limit, disable listen socket
	 for event loop for now */
      if (0 != epoll_ctl (daemon->epoll_fd,
			  EPOLL_CTL_DEL,
			  ls,
			  NULL))
	MHD_PANIC (_("Failed to remove listen FD from epoll set\n"));
      daemon->listen_socket_in_epoll = false;
    }
  if (MHD_YES == may_block)
    {
      if (MHD_YES == MHD_get_timeout (daemon,
				      &timeout_ll))
	{
	  if (timeout_ll >= (MHD_UNSIGNED_LONG_LONG) INT_MAX)
	    timeout_ms = INT_MAX;
	  else
	    timeout_ms = (int) timeout_ll;
	}
      else
	timeout_ms = -1;
    }
  else
    timeout_ms = 0;

  /* Reset. New value will be set when connections are processed. */
  /* Note: Used mostly for uniformity here as same situation is
   * signaled in epoll mode by non-empty eready DLL. */
  daemon->data_already_pending = false;

  /* drain 'epoll' event queue; need to iterate as we get at most
     MAX_EVENTS in one system call here; in practice this should
     pretty much mean only one round, but better an extra loop here
     than unfair behavior... */
  num_events = MAX_EVENTS;
  while (MAX_EVENTS == num_events)
    {
      /* update event masks */
      num_events = epoll_wait (daemon->epoll_fd,
			       events,
                               MAX_EVENTS,
                               timeout_ms);
      if (-1 == num_events)
	{
          const int err = MHD_socket_get_error_ ();
          if (MHD_SCKT_ERR_IS_EINTR_ (err))
	    return MHD_YES;
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Call to epoll_wait failed: %s\n"),
                    MHD_socket_strerr_ (err));
#endif
	  return MHD_NO;
	}
      for (i=0;i<(unsigned int) num_events;i++)
	{
          /* First, check for the values of `ptr` that would indicate
             that this event is not about a normal connection. */
	  if (NULL == events[i].data.ptr)
	    continue; /* shutdown signal! */
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
          if (upgrade_marker == events[i].data.ptr)
            {
              /* activity on an upgraded connection, we process
                 those in a separate epoll() */
              run_upgraded = MHD_YES;
              continue;
            }
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
          if (epoll_itc_marker == events[i].data.ptr)
            {
              /* It's OK to clear ITC here as all external
                 conditions will be processed later. */
              MHD_itc_clear_ (daemon->itc);
              continue;
            }
	  if (daemon == events[i].data.ptr)
	    {
              /* Check for error conditions on listen socket. */
              /* FIXME: Initiate MHD_quiesce_daemon() to prevent busy waiting? */
              if (0 == (events[i].events & (EPOLLERR | EPOLLHUP)))
                {
                  unsigned int series_length = 0;
                  /* Run 'accept' until it fails or daemon at limit of connections.
                   * Do not accept more then 10 connections at once. The rest will
                   * be accepted on next turn (level trigger is used for listen
                   * socket). */
                  while ( (MHD_YES == MHD_accept_connection (daemon)) &&
                          (series_length < 10) &&
                          (daemon->connections < daemon->connection_limit) &&
                          (! daemon->at_limit) )
                    series_length++;
	        }
              continue;
	    }
          /* this is an event relating to a 'normal' connection,
             remember the event and if appropriate mark the
             connection as 'eready'. */
          pos = events[i].data.ptr;
          /* normal processing: update read/write data */
          if (0 != (events[i].events & (EPOLLPRI | EPOLLERR | EPOLLHUP)))
            {
              pos->epoll_state |= MHD_EPOLL_STATE_ERROR;
              if (0 == (pos->epoll_state & MHD_EPOLL_STATE_IN_EREADY_EDLL))
                {
                  EDLL_insert (daemon->eready_head,
                               daemon->eready_tail,
                               pos);
                  pos->epoll_state |= MHD_EPOLL_STATE_IN_EREADY_EDLL;
                }
            }
          else
            {
              if (0 != (events[i].events & EPOLLIN))
                {
                  pos->epoll_state |= MHD_EPOLL_STATE_READ_READY;
                  if ( ( (MHD_EVENT_LOOP_INFO_READ == pos->event_loop_info) ||
                         (pos->read_buffer_size > pos->read_buffer_offset) ) &&
                       (0 == (pos->epoll_state & MHD_EPOLL_STATE_IN_EREADY_EDLL) ) )
                    {
                      EDLL_insert (daemon->eready_head,
                                   daemon->eready_tail,
                                   pos);
                      pos->epoll_state |= MHD_EPOLL_STATE_IN_EREADY_EDLL;
                    }
                }
              if (0 != (events[i].events & EPOLLOUT))
                {
                  pos->epoll_state |= MHD_EPOLL_STATE_WRITE_READY;
                  if ( (MHD_EVENT_LOOP_INFO_WRITE == pos->event_loop_info) &&
                       (0 == (pos->epoll_state & MHD_EPOLL_STATE_IN_EREADY_EDLL) ) )
                    {
                      EDLL_insert (daemon->eready_head,
                                   daemon->eready_tail,
                                   pos);
                      pos->epoll_state |= MHD_EPOLL_STATE_IN_EREADY_EDLL;
                    }
                }
            }
        }
    }

#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  if (MHD_YES == run_upgraded)
    run_epoll_for_upgrade (daemon);
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */

  /* we handle resumes here because we may have ready connections
     that will not be placed into the epoll list immediately. */
  if (0 != (daemon->options & MHD_TEST_ALLOW_SUSPEND_RESUME))
    (void) resume_suspended_connections (daemon);

  /* process events for connections */
  prev = daemon->eready_tail;
  while (NULL != (pos = prev))
    {
	  prev = pos->prevE;
      call_handlers (pos,
                     0 != (pos->epoll_state & MHD_EPOLL_STATE_READ_READY),
                     0 != (pos->epoll_state & MHD_EPOLL_STATE_WRITE_READY),
                     0 != (pos->epoll_state & MHD_EPOLL_STATE_ERROR));
      if (MHD_EPOLL_STATE_IN_EREADY_EDLL ==
            (pos->epoll_state & (MHD_EPOLL_STATE_SUSPENDED | MHD_EPOLL_STATE_IN_EREADY_EDLL)))
        {
          if ( (MHD_EVENT_LOOP_INFO_READ == pos->event_loop_info &&
                  0 == (pos->epoll_state & MHD_EPOLL_STATE_READ_READY) ) ||
               (MHD_EVENT_LOOP_INFO_WRITE == pos->event_loop_info &&
                 0 == (pos->epoll_state & MHD_EPOLL_STATE_WRITE_READY) ) ||
               MHD_EVENT_LOOP_INFO_CLEANUP == pos->event_loop_info)
            {
              EDLL_remove (daemon->eready_head,
                           daemon->eready_tail,
                           pos);
              pos->epoll_state &= ~MHD_EPOLL_STATE_IN_EREADY_EDLL;
            }
        }
    }

  /* Finally, handle timed-out connections; we need to do this here
     as the epoll mechanism won't call the 'idle_handler' on everything,
     as the other event loops do.  As timeouts do not get an explicit
     event, we need to find those connections that might have timed out
     here.

     Connections with custom timeouts must all be looked at, as we
     do not bother to sort that (presumably very short) list. */
  prev = daemon->manual_timeout_tail;
  while (NULL != (pos = prev))
    {
      prev = pos->prevX;
      pos->idle_handler (pos);
    }
  /* Connections with the default timeout are sorted by prepending
     them to the head of the list whenever we touch the connection;
     thus it suffices to iterate from the tail until the first
     connection is NOT timed out */
  prev = daemon->normal_timeout_tail;
  while (NULL != (pos = prev))
    {
      prev = pos->prevX;
      pos->idle_handler (pos);
      if (MHD_CONNECTION_CLOSED != pos->state)
	break; /* sorted by timeout, no need to visit the rest! */
    }
  return MHD_YES;
}
#endif


/**
 * Run webserver operations (without blocking unless in client
 * callbacks).  This method should be called by clients in combination
 * with #MHD_get_fdset if the client-controlled select method is used.
 *
 * This function is a convenience method, which is useful if the
 * fd_sets from #MHD_get_fdset were not directly passed to `select()`;
 * with this function, MHD will internally do the appropriate `select()`
 * call itself again.  While it is always safe to call #MHD_run (in
 * external select mode), you should call #MHD_run_from_select if
 * performance is important (as it saves an expensive call to
 * `select()`).
 *
 * @param daemon daemon to run
 * @return #MHD_YES on success, #MHD_NO if this
 *         daemon was not started with the right
 *         options for this call.
 * @ingroup event
 */
int
MHD_run (struct MHD_Daemon *daemon)
{
  if ( (daemon->shutdown) ||
       (0 != (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD)) )
    return MHD_NO;
  if (0 != (daemon->options & MHD_USE_POLL))
  {
    MHD_poll (daemon, MHD_NO);
    MHD_cleanup_connections (daemon);
  }
#ifdef EPOLL_SUPPORT
  else if (0 != (daemon->options & MHD_USE_EPOLL))
  {
    MHD_epoll (daemon, MHD_NO);
    MHD_cleanup_connections (daemon);
  }
#endif
  else
  {
    MHD_select (daemon, MHD_NO);
    /* MHD_select does MHD_cleanup_connections already */
  }
  return MHD_YES;
}


/**
 * Close the given connection, remove it from all of its
 * DLLs and move it into the cleanup queue.
 * @remark To be called only from thread that
 * process daemon's select()/poll()/etc.
 *
 * @param pos connection to move to cleanup
 */
static void
close_connection (struct MHD_Connection *pos)
{
  struct MHD_Daemon *daemon = pos->daemon;

  if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
    {
      MHD_connection_mark_closed_ (pos);
      return; /* must let thread to do the rest */
    }
  MHD_connection_close_ (pos,
                         MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN);

  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);

  if (pos->connection_timeout == pos->daemon->connection_timeout)
    XDLL_remove (daemon->normal_timeout_head,
		 daemon->normal_timeout_tail,
		 pos);
  else
    XDLL_remove (daemon->manual_timeout_head,
		 daemon->manual_timeout_tail,
		 pos);
  DLL_remove (daemon->connections_head,
	      daemon->connections_tail,
	      pos);
  DLL_insert (daemon->cleanup_head,
	      daemon->cleanup_tail,
	      pos);

  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
}


/**
 * Thread that runs the select loop until the daemon
 * is explicitly shut down.
 *
 * @param cls `struct MHD_Deamon` to run select loop in a thread for
 * @return always 0 (on shutdown)
 */
static MHD_THRD_RTRN_TYPE_ MHD_THRD_CALL_SPEC_
MHD_select_thread (void *cls)
{
  struct MHD_Daemon *daemon = cls;

  while (! daemon->shutdown)
    {
      if (0 != (daemon->options & MHD_USE_POLL))
	MHD_poll (daemon, MHD_YES);
#ifdef EPOLL_SUPPORT
      else if (0 != (daemon->options & MHD_USE_EPOLL))
	MHD_epoll (daemon, MHD_YES);
#endif
      else
	MHD_select (daemon, MHD_YES);
      MHD_cleanup_connections (daemon);
    }

  /* Resume any pending for resume connections, join
   * all connection's threads (if any) and finally cleanup
   * everything. */
  close_all_connections (daemon);

  return (MHD_THRD_RTRN_TYPE_)0;
}


/**
 * Process escape sequences ('%HH') Updates val in place; the
 * result should be UTF-8 encoded and cannot be larger than the input.
 * The result must also still be 0-terminated.
 *
 * @param cls closure (use NULL)
 * @param connection handle to connection, not used
 * @param val value to unescape (modified in the process)
 * @return length of the resulting val (strlen(val) maybe
 *  shorter afterwards due to elimination of escape sequences)
 */
static size_t
unescape_wrapper (void *cls,
                  struct MHD_Connection *connection,
                  char *val)
{
  return MHD_http_unescape (val);
}


/**
 * Start a webserver on the given port.  Variadic version of
 * #MHD_start_daemon_va.
 *
 * @param flags combination of `enum MHD_FLAG` values
 * @param port port to bind to
 * @param apc callback to call to check which clients
 *        will be allowed to connect; you can pass NULL
 *        in which case connections from any IP will be
 *        accepted
 * @param apc_cls extra argument to @a apc
 * @param dh handler called for all requests (repeatedly)
 * @param dh_cls extra argument to @a dh
 * @return NULL on error, handle to daemon on success
 * @ingroup event
 */
struct MHD_Daemon *
MHD_start_daemon (unsigned int flags,
                  uint16_t port,
                  MHD_AcceptPolicyCallback apc,
                  void *apc_cls,
                  MHD_AccessHandlerCallback dh,
                  void *dh_cls,
                  ...)
{
  struct MHD_Daemon *daemon;
  va_list ap;

  va_start (ap,
            dh_cls);
  daemon = MHD_start_daemon_va (flags,
                                port,
                                apc,
                                apc_cls,
                                dh,
                                dh_cls,
                                ap);
  va_end (ap);
  return daemon;
}


/**
 * Stop accepting connections from the listening socket.  Allows
 * clients to continue processing, but stops accepting new
 * connections.  Note that the caller is responsible for closing the
 * returned socket; however, if MHD is run using threads (anything but
 * external select mode), socket will be removed from existing threads
 * with some delay and it must not be closed while it's in use. To make
 * sure that the socket is not used anymore, call #MHD_stop_daemon.
 *
 * Note that some thread modes require the caller to have passed
 * #MHD_USE_ITC when using this API.  If this daemon is
 * in one of those modes and this option was not given to
 * #MHD_start_daemon, this function will return #MHD_INVALID_SOCKET.
 *
 * @param daemon daemon to stop accepting new connections for
 * @return old listen socket on success, #MHD_INVALID_SOCKET if
 *         the daemon was already not listening anymore
 * @ingroup specialized
 */
MHD_socket
MHD_quiesce_daemon (struct MHD_Daemon *daemon)
{
  unsigned int i;
  MHD_socket ret;

  ret = daemon->listen_fd;
  if (MHD_INVALID_SOCKET == ret)
    return MHD_INVALID_SOCKET;
  if ( (0 == (daemon->options & (MHD_USE_ITC))) &&
       (0 != (daemon->options & (MHD_USE_INTERNAL_POLLING_THREAD))) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		"Using MHD_quiesce_daemon in this mode requires MHD_USE_ITC\n");
#endif
      return MHD_INVALID_SOCKET;
    }

  if (NULL != daemon->worker_pool)
    for (i = 0; i < daemon->worker_pool_size; i++)
      {
	daemon->worker_pool[i].was_quiesced = true;
#ifdef EPOLL_SUPPORT
	if ( (0 != (daemon->options & MHD_USE_EPOLL)) &&
	     (-1 != daemon->worker_pool[i].epoll_fd) &&
	     (daemon->worker_pool[i].listen_socket_in_epoll) )
	  {
	    if (0 != epoll_ctl (daemon->worker_pool[i].epoll_fd,
				EPOLL_CTL_DEL,
				ret,
				NULL))
	      MHD_PANIC (_("Failed to remove listen FD from epoll set\n"));
	    daemon->worker_pool[i].listen_socket_in_epoll = false;
	  }
        else
#endif
        if (MHD_ITC_IS_VALID_(daemon->worker_pool[i].itc))
          {
            if (! MHD_itc_activate_ (daemon->worker_pool[i].itc, "q"))
              MHD_PANIC (_("Failed to signal quiesce via inter-thread communication channel"));
          }
      }
  daemon->was_quiesced = true;
#ifdef EPOLL_SUPPORT
  if ( (0 != (daemon->options & MHD_USE_EPOLL)) &&
       (-1 != daemon->epoll_fd) &&
       (daemon->listen_socket_in_epoll) )
    {
      if (0 != epoll_ctl (daemon->epoll_fd,
			  EPOLL_CTL_DEL,
			  ret,
			  NULL))
	MHD_PANIC ("Failed to remove listen FD from epoll set\n");
      daemon->listen_socket_in_epoll = false;
    }
#endif
  if ( (MHD_ITC_IS_VALID_(daemon->itc)) &&
       (! MHD_itc_activate_ (daemon->itc, "q")) )
    MHD_PANIC (_("failed to signal quiesce via inter-thread communication channel"));
  return ret;
}


/**
 * Signature of the MHD custom logger function.
 *
 * @param cls closure
 * @param format format string
 * @param va arguments to the format string (fprintf-style)
 */
typedef void
(*VfprintfFunctionPointerType)(void *cls,
                               const char *format,
                               va_list va);


/**
 * Parse a list of options given as varargs.
 *
 * @param daemon the daemon to initialize
 * @param servaddr where to store the server's listen address
 * @param ap the options
 * @return #MHD_YES on success, #MHD_NO on error
 */
static int
parse_options_va (struct MHD_Daemon *daemon,
		  const struct sockaddr **servaddr,
		  va_list ap);


/**
 * Parse a list of options given as varargs.
 *
 * @param daemon the daemon to initialize
 * @param servaddr where to store the server's listen address
 * @param ... the options
 * @return #MHD_YES on success, #MHD_NO on error
 */
static int
parse_options (struct MHD_Daemon *daemon,
	       const struct sockaddr **servaddr,
	       ...)
{
  va_list ap;
  int ret;

  va_start (ap, servaddr);
  ret = parse_options_va (daemon,
                          servaddr,
                          ap);
  va_end (ap);
  return ret;
}


/**
 * Parse a list of options given as varargs.
 *
 * @param daemon the daemon to initialize
 * @param servaddr where to store the server's listen address
 * @param ap the options
 * @return #MHD_YES on success, #MHD_NO on error
 */
static int
parse_options_va (struct MHD_Daemon *daemon,
		  const struct sockaddr **servaddr,
		  va_list ap)
{
  enum MHD_OPTION opt;
  struct MHD_OptionItem *oa;
  unsigned int i;
  unsigned int uv;
#ifdef HTTPS_SUPPORT
  int ret;
  const char *pstr;
#endif /* HTTPS_SUPPORT */

  while (MHD_OPTION_END != (opt = (enum MHD_OPTION) va_arg (ap, int)))
    {
      switch (opt)
        {
        case MHD_OPTION_CONNECTION_MEMORY_LIMIT:
          daemon->pool_size = va_arg (ap,
                                      size_t);
          break;
        case MHD_OPTION_CONNECTION_MEMORY_INCREMENT:
          daemon->pool_increment= va_arg (ap,
                                          size_t);
          break;
        case MHD_OPTION_CONNECTION_LIMIT:
          daemon->connection_limit = va_arg (ap,
                                             unsigned int);
          break;
        case MHD_OPTION_CONNECTION_TIMEOUT:
          uv = va_arg (ap,
                       unsigned int);
          if (TIME_T_MAX < uv)
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("Warning: Too large timeout value, ignored.\n"));
#endif
              daemon->connection_timeout = 0;
            }
          else
            daemon->connection_timeout = (time_t)uv;
          break;
        case MHD_OPTION_NOTIFY_COMPLETED:
          daemon->notify_completed = va_arg (ap,
                                             MHD_RequestCompletedCallback);
          daemon->notify_completed_cls = va_arg (ap,
                                                 void *);
          break;
        case MHD_OPTION_NOTIFY_CONNECTION:
          daemon->notify_connection = va_arg (ap,
                                              MHD_NotifyConnectionCallback);
          daemon->notify_connection_cls = va_arg (ap,
                                                  void *);
          break;
        case MHD_OPTION_PER_IP_CONNECTION_LIMIT:
          daemon->per_ip_connection_limit = va_arg (ap,
                                                    unsigned int);
          break;
        case MHD_OPTION_SOCK_ADDR:
          *servaddr = va_arg (ap,
                              const struct sockaddr *);
          break;
        case MHD_OPTION_URI_LOG_CALLBACK:
          daemon->uri_log_callback = va_arg (ap,
                                             LogCallback);
          daemon->uri_log_callback_cls = va_arg (ap,
                                                 void *);
          break;
        case MHD_OPTION_THREAD_POOL_SIZE:
          daemon->worker_pool_size = va_arg (ap,
                                             unsigned int);
          if (0 == daemon->worker_pool_size)
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("Warning: Zero size, specified for thread pool size, is ignored. "
                          "Thread pool is not used.\n"));
#endif
            }
          else if (1 == daemon->worker_pool_size)
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("Warning: \"1\", specified for thread pool size, is ignored. "
                          "Thread pool is not used.\n"));
#endif
              daemon->worker_pool_size = 0;
            }
          else if (daemon->worker_pool_size >= (SIZE_MAX / sizeof (struct MHD_Daemon)))
	    {
#ifdef HAVE_MESSAGES
	      MHD_DLOG (daemon,
			_("Specified thread pool size (%u) too big\n"),
			daemon->worker_pool_size);
#endif
	      return MHD_NO;
	    }
	  else
	    {
	      if (0 == (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD))
		{
#ifdef HAVE_MESSAGES
		  MHD_DLOG (daemon,
			    _("MHD_OPTION_THREAD_POOL_SIZE option is specified but "
			      "MHD_USE_INTERNAL_POLLING_THREAD flag is not specified.\n"));
#endif
		  return MHD_NO;
		}
	      if (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION))
		{
#ifdef HAVE_MESSAGES
		  MHD_DLOG (daemon,
			    _("Both MHD_OPTION_THREAD_POOL_SIZE option and "
			      "MHD_USE_THREAD_PER_CONNECTION flag are specified.\n"));
#endif
		  return MHD_NO;
		}
	    }
          break;
#ifdef HTTPS_SUPPORT
        case MHD_OPTION_HTTPS_MEM_KEY:
	  if (0 != (daemon->options & MHD_USE_TLS))
	    daemon->https_mem_key = va_arg (ap,
                                            const char *);
#ifdef HAVE_MESSAGES
	  else
	    MHD_DLOG (daemon,
		      _("MHD HTTPS option %d passed to MHD but MHD_USE_TLS not set\n"),
		      opt);
#endif
          break;
        case MHD_OPTION_HTTPS_KEY_PASSWORD:
	  if (0 != (daemon->options & MHD_USE_TLS))
	    daemon->https_key_password = va_arg (ap,
                                                 const char *);
#ifdef HAVE_MESSAGES
	  else
	    MHD_DLOG (daemon,
		      _("MHD HTTPS option %d passed to MHD but MHD_USE_TLS not set\n"),
		      opt);
#endif
          break;
        case MHD_OPTION_HTTPS_MEM_CERT:
	  if (0 != (daemon->options & MHD_USE_TLS))
	    daemon->https_mem_cert = va_arg (ap,
                                             const char *);
#ifdef HAVE_MESSAGES
	  else
	    MHD_DLOG (daemon,
		      _("MHD HTTPS option %d passed to MHD but MHD_USE_TLS not set\n"),
		      opt);
#endif
          break;
        case MHD_OPTION_HTTPS_MEM_TRUST:
	  if (0 != (daemon->options & MHD_USE_TLS))
	    daemon->https_mem_trust = va_arg (ap,
                                              const char *);
#ifdef HAVE_MESSAGES
	  else
	    MHD_DLOG (daemon,
		      _("MHD HTTPS option %d passed to MHD but MHD_USE_TLS not set\n"),
		      opt);
#endif
          break;
	case MHD_OPTION_HTTPS_CRED_TYPE:
	  daemon->cred_type = (gnutls_credentials_type_t) va_arg (ap,
                                                                  int);
	  break;
        case MHD_OPTION_HTTPS_MEM_DHPARAMS:
          if (0 != (daemon->options & MHD_USE_TLS))
            {
              const char *arg = va_arg (ap,
                                        const char *);
              gnutls_datum_t dhpar;

              if (gnutls_dh_params_init (&daemon->https_mem_dhparams) < 0)
                {
#ifdef HAVE_MESSAGES
                  MHD_DLOG (daemon,
                            _("Error initializing DH parameters\n"));
#endif
                  return MHD_NO;
                }
              dhpar.data = (unsigned char *) arg;
              dhpar.size = strlen (arg);
              if (gnutls_dh_params_import_pkcs3 (daemon->https_mem_dhparams,
                                                 &dhpar,
                                                 GNUTLS_X509_FMT_PEM) < 0)
                {
#ifdef HAVE_MESSAGES
                  MHD_DLOG (daemon,
                            _("Bad Diffie-Hellman parameters format\n"));
#endif
                  gnutls_dh_params_deinit (daemon->https_mem_dhparams);
                  return MHD_NO;
                }
              daemon->have_dhparams = true;
            }
          else
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("MHD HTTPS option %d passed to MHD but MHD_USE_TLS not set\n"),
                        opt);
#endif
              return MHD_NO;
            }
          break;
        case MHD_OPTION_HTTPS_PRIORITIES:
	  if (0 != (daemon->options & MHD_USE_TLS))
	    {
	      gnutls_priority_deinit (daemon->priority_cache);
	      ret = gnutls_priority_init (&daemon->priority_cache,
					  pstr = va_arg (ap, const char*),
					  NULL);
	      if (GNUTLS_E_SUCCESS != ret)
	      {
#ifdef HAVE_MESSAGES
		MHD_DLOG (daemon,
			  _("Setting priorities to `%s' failed: %s\n"),
			  pstr,
			  gnutls_strerror (ret));
#endif
		daemon->priority_cache = NULL;
		return MHD_NO;
	      }
	    }
          break;
        case MHD_OPTION_HTTPS_CERT_CALLBACK:
#if GNUTLS_VERSION_MAJOR < 3
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("MHD_OPTION_HTTPS_CERT_CALLBACK requires building MHD with GnuTLS >= 3.0\n"));
#endif
          return MHD_NO;
#else
          if (0 != (daemon->options & MHD_USE_TLS))
            daemon->cert_callback = va_arg (ap,
                                            gnutls_certificate_retrieve_function2 *);
          break;
#endif
#endif /* HTTPS_SUPPORT */
#ifdef DAUTH_SUPPORT
	case MHD_OPTION_DIGEST_AUTH_RANDOM:
	  daemon->digest_auth_rand_size = va_arg (ap,
                                                  size_t);
	  daemon->digest_auth_random = va_arg (ap,
                                               const char *);
	  break;
	case MHD_OPTION_NONCE_NC_SIZE:
	  daemon->nonce_nc_size = va_arg (ap,
                                          unsigned int);
	  break;
#endif
	case MHD_OPTION_LISTEN_SOCKET:
          if (0 != (daemon->options & MHD_USE_NO_LISTEN_SOCKET))
            {
#ifdef HAVE_MESSAGES
                MHD_DLOG (daemon,
                          _("MHD_OPTION_LISTEN_SOCKET specified for daemon "
                            "with MHD_USE_NO_LISTEN_SOCKET flag set.\n"));
#endif
                return MHD_NO;
            }
          else
            daemon->listen_fd = va_arg (ap,
                                        MHD_socket);
	  break;
        case MHD_OPTION_EXTERNAL_LOGGER:
#ifdef HAVE_MESSAGES
          daemon->custom_error_log = va_arg (ap,
                                             VfprintfFunctionPointerType);
          daemon->custom_error_log_cls = va_arg (ap,
                                                 void *);
#else
          va_arg (ap,
                  VfprintfFunctionPointerType);
          va_arg (ap,
                  void *);
#endif
          break;
        case MHD_OPTION_THREAD_STACK_SIZE:
          daemon->thread_stack_size = va_arg (ap,
                                              size_t);
          break;
#ifdef TCP_FASTOPEN
        case MHD_OPTION_TCP_FASTOPEN_QUEUE_SIZE:
          daemon->fastopen_queue_size = va_arg (ap,
                                                unsigned int);
          break;
#endif
	case MHD_OPTION_LISTENING_ADDRESS_REUSE:
	  daemon->listening_address_reuse = va_arg (ap,
                                                    unsigned int) ? 1 : -1;
	  break;
	case MHD_OPTION_LISTEN_BACKLOG_SIZE:
	  daemon->listen_backlog_size = va_arg (ap,
                                                unsigned int);
	  break;
	case MHD_OPTION_ARRAY:
	  oa = va_arg (ap, struct MHD_OptionItem*);
	  i = 0;
	  while (MHD_OPTION_END != (opt = oa[i].option))
	    {
	      switch (opt)
		{
		  /* all options taking 'size_t' */
		case MHD_OPTION_CONNECTION_MEMORY_LIMIT:
		case MHD_OPTION_CONNECTION_MEMORY_INCREMENT:
		case MHD_OPTION_THREAD_STACK_SIZE:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(size_t) oa[i].value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* all options taking 'unsigned int' */
		case MHD_OPTION_NONCE_NC_SIZE:
		case MHD_OPTION_CONNECTION_LIMIT:
		case MHD_OPTION_CONNECTION_TIMEOUT:
		case MHD_OPTION_PER_IP_CONNECTION_LIMIT:
		case MHD_OPTION_THREAD_POOL_SIZE:
                case MHD_OPTION_TCP_FASTOPEN_QUEUE_SIZE:
		case MHD_OPTION_LISTENING_ADDRESS_REUSE:
		case MHD_OPTION_LISTEN_BACKLOG_SIZE:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(unsigned int) oa[i].value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* all options taking 'enum' */
		case MHD_OPTION_HTTPS_CRED_TYPE:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(int) oa[i].value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
                  /* all options taking 'MHD_socket' */
                case MHD_OPTION_LISTEN_SOCKET:
                  if (MHD_YES != parse_options (daemon,
                                                servaddr,
                                                opt,
                                                (MHD_socket) oa[i].value,
                                                MHD_OPTION_END))
                    return MHD_NO;
                  break;
		  /* all options taking one pointer */
		case MHD_OPTION_SOCK_ADDR:
		case MHD_OPTION_HTTPS_MEM_KEY:
		case MHD_OPTION_HTTPS_KEY_PASSWORD:
		case MHD_OPTION_HTTPS_MEM_CERT:
		case MHD_OPTION_HTTPS_MEM_TRUST:
	        case MHD_OPTION_HTTPS_MEM_DHPARAMS:
		case MHD_OPTION_HTTPS_PRIORITIES:
		case MHD_OPTION_ARRAY:
                case MHD_OPTION_HTTPS_CERT_CALLBACK:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						oa[i].ptr_value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* all options taking two pointers */
		case MHD_OPTION_NOTIFY_COMPLETED:
		case MHD_OPTION_NOTIFY_CONNECTION:
		case MHD_OPTION_URI_LOG_CALLBACK:
		case MHD_OPTION_EXTERNAL_LOGGER:
		case MHD_OPTION_UNESCAPE_CALLBACK:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(void *) oa[i].value,
						oa[i].ptr_value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		  /* options taking size_t-number followed by pointer */
		case MHD_OPTION_DIGEST_AUTH_RANDOM:
		  if (MHD_YES != parse_options (daemon,
						servaddr,
						opt,
						(size_t) oa[i].value,
						oa[i].ptr_value,
						MHD_OPTION_END))
		    return MHD_NO;
		  break;
		default:
		  return MHD_NO;
		}
	      i++;
	    }
	  break;
        case MHD_OPTION_UNESCAPE_CALLBACK:
          daemon->unescape_callback = va_arg (ap,
                                              UnescapeCallback);
          daemon->unescape_callback_cls = va_arg (ap,
                                                  void *);
          break;
        default:
#ifdef HAVE_MESSAGES
          if ( ( (opt >= MHD_OPTION_HTTPS_MEM_KEY) &&
                 (opt <= MHD_OPTION_HTTPS_PRIORITIES) ) ||
               (opt == MHD_OPTION_HTTPS_MEM_TRUST))
            {
              MHD_DLOG (daemon,
			_("MHD HTTPS option %d passed to MHD compiled without HTTPS support\n"),
			opt);
            }
          else
            {
              MHD_DLOG (daemon,
			_("Invalid option %d! (Did you terminate the list with MHD_OPTION_END?)\n"),
			opt);
            }
#endif
	  return MHD_NO;
        }
    }
  return MHD_YES;
}


#ifdef EPOLL_SUPPORT
static int
setup_epoll_fd (struct MHD_Daemon *daemon)
{
  int fd;

#ifdef USE_EPOLL_CREATE1
  fd = epoll_create1 (EPOLL_CLOEXEC);
#else  /* ! USE_EPOLL_CREATE1 */
  fd = epoll_create (MAX_EVENTS);
#endif /* ! USE_EPOLL_CREATE1 */
  if (MHD_INVALID_SOCKET == fd)
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Call to epoll_create1 failed: %s\n"),
                MHD_socket_last_strerr_ ());
#endif
      return MHD_INVALID_SOCKET;
    }
#if !defined(USE_EPOLL_CREATE1)
  if (! MHD_socket_noninheritable_ (fd))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to set noninheritable mode on epoll FD.\n"));
#endif
    }
#endif /* ! USE_EPOLL_CREATE1 */
  return fd;
}


/**
 * Setup epoll() FD for the daemon and initialize it to listen
 * on the listen FD.
 * @remark To be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param daemon daemon to initialize for epoll()
 * @return #MHD_YES on success, #MHD_NO on failure
 */
static int
setup_epoll_to_listen (struct MHD_Daemon *daemon)
{
  struct epoll_event event;
  MHD_socket ls;

  daemon->epoll_fd = setup_epoll_fd (daemon);
  if (-1 == daemon->epoll_fd)
    return MHD_NO;
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  if (0 != (MHD_ALLOW_UPGRADE & daemon->options))
    {
       daemon->epoll_upgrade_fd = setup_epoll_fd (daemon);
       if (MHD_INVALID_SOCKET == daemon->epoll_upgrade_fd)
         return MHD_NO;
    }
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
  if ( (MHD_INVALID_SOCKET == (ls = daemon->listen_fd)) ||
       (daemon->was_quiesced) )
    return MHD_YES; /* non-listening daemon */
  event.events = EPOLLIN;
  event.data.ptr = daemon;
  if (0 != epoll_ctl (daemon->epoll_fd,
		      EPOLL_CTL_ADD,
		      ls,
		      &event))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Call to epoll_ctl failed: %s\n"),
                MHD_socket_last_strerr_ ());
#endif
      return MHD_NO;
    }
  daemon->listen_socket_in_epoll = true;
  if (MHD_ITC_IS_VALID_(daemon->itc))
    {
      event.events = EPOLLIN;
      event.data.ptr = (void *) epoll_itc_marker;
      if (0 != epoll_ctl (daemon->epoll_fd,
                          EPOLL_CTL_ADD,
                          MHD_itc_r_fd_ (daemon->itc),
                          &event))
        {
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Call to epoll_ctl failed: %s\n"),
                    MHD_socket_last_strerr_ ());
#endif
          return MHD_NO;
        }
    }
  return MHD_YES;
}
#endif


/**
 * Start a webserver on the given port.
 *
 * @param flags combination of `enum MHD_FLAG` values
 * @param port port to bind to (in host byte order)
 * @param apc callback to call to check which clients
 *        will be allowed to connect; you can pass NULL
 *        in which case connections from any IP will be
 *        accepted
 * @param apc_cls extra argument to @a apc
 * @param dh handler called for all requests (repeatedly)
 * @param dh_cls extra argument to @a dh
 * @param ap list of options (type-value pairs,
 *        terminated with #MHD_OPTION_END).
 * @return NULL on error, handle to daemon on success
 * @ingroup event
 */
struct MHD_Daemon *
MHD_start_daemon_va (unsigned int flags,
                     uint16_t port,
                     MHD_AcceptPolicyCallback apc,
                     void *apc_cls,
                     MHD_AccessHandlerCallback dh,
                     void *dh_cls,
		     va_list ap)
{
  const MHD_SCKT_OPT_BOOL_ on = 1;
  struct MHD_Daemon *daemon;
  MHD_socket listen_fd;
  struct sockaddr_in servaddr4;
#if HAVE_INET6
  struct sockaddr_in6 servaddr6;
#endif
  const struct sockaddr *servaddr = NULL;
  socklen_t addrlen;
  unsigned int i;
  enum MHD_FLAG eflags; /* same type as in MHD_Daemon */
  enum MHD_FLAG *pflags;

  eflags = (enum MHD_FLAG) flags;
  pflags = &eflags;
#ifndef HAVE_INET6
  if (0 != (*pflags & MHD_USE_IPv6))
    return NULL;
#endif
#ifndef HAVE_POLL
  if (0 != (*pflags & MHD_USE_POLL))
    return NULL;
#endif
#ifndef EPOLL_SUPPORT
  if (0 != (*pflags & MHD_USE_EPOLL))
    return NULL;
#endif /* ! EPOLL_SUPPORT */
#ifndef HTTPS_SUPPORT
  if (0 != (*pflags & MHD_USE_TLS))
    return NULL;
#endif /* ! HTTPS_SUPPORT */
#ifndef TCP_FASTOPEN
  if (0 != (*pflags & MHD_USE_TCP_FASTOPEN))
    return NULL;
#endif
  if (0 != (*pflags & MHD_ALLOW_UPGRADE))
    {
#ifdef UPGRADE_SUPPORT
      *pflags |= MHD_ALLOW_SUSPEND_RESUME;
#else  /* ! UPGRADE_SUPPORT */
      return NULL;
#endif /* ! UPGRADE_SUPPORT */
    }
  if (NULL == dh)
    return NULL;

  /* Check for invalid combinations of flags. */
  if ( ((0 != (*pflags & MHD_USE_POLL)) && (0 != (*pflags & MHD_USE_EPOLL))) ||
       ((0 != (*pflags & MHD_USE_EPOLL)) && (0 != (*pflags & MHD_USE_THREAD_PER_CONNECTION))) ||
       ((0 != (*pflags & MHD_USE_POLL)) && (0 == (*pflags & MHD_USE_INTERNAL_POLLING_THREAD))) ||
       ((0 != (*pflags & MHD_USE_AUTO)) && (0 != (*pflags & (MHD_USE_POLL | MHD_USE_EPOLL)))) )
    return NULL;

  if (0 != (*pflags & MHD_USE_AUTO))
    {
      if (0 != (*pflags & MHD_USE_THREAD_PER_CONNECTION))
        {
          /* Thread per connection with internal polling thread. */
#ifdef HAVE_POLL
          *pflags |= MHD_USE_POLL;
#else  /* ! HAVE_POLL */
          /* use select() - do not modify flags */
#endif /* ! HAVE_POLL */
        }
      else if (0 != (*pflags & MHD_USE_INTERNAL_POLLING_THREAD))
        {
          /* Internal polling thread. */
#if defined(EPOLL_SUPPORT)
          *pflags |= MHD_USE_EPOLL;
#elif defined(HAVE_POLL)
          *pflags |= MHD_USE_POLL;
#else  /* !HAVE_POLL && !EPOLL_SUPPORT */
          /* use select() - do not modify flags */
#endif /* !HAVE_POLL && !EPOLL_SUPPORT */
        }
      else
        {
          /* Internal threads are not used - "external" polling mode. */
#if defined(EPOLL_SUPPORT)
          *pflags |= MHD_USE_EPOLL;
#else  /* ! EPOLL_SUPPORT */
          /* use select() - do not modify flags */
#endif /* ! EPOLL_SUPPORT */
        }
    }

  if (NULL == (daemon = MHD_calloc_ (1, sizeof (struct MHD_Daemon))))
    return NULL;
#ifdef EPOLL_SUPPORT
  daemon->epoll_fd = -1;
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  daemon->epoll_upgrade_fd = -1;
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
#endif
  /* try to open listen socket */
#ifdef HTTPS_SUPPORT
  daemon->priority_cache = NULL;
  if (0 != (*pflags & MHD_USE_TLS))
    {
      gnutls_priority_init (&daemon->priority_cache,
			    "NORMAL",
			    NULL);
    }
#endif /* HTTPS_SUPPORT */
  daemon->listen_fd = MHD_INVALID_SOCKET;
  daemon->listening_address_reuse = 0;
  daemon->options = *pflags;
  pflags = &daemon->options;
  daemon->port = port;
  daemon->apc = apc;
  daemon->apc_cls = apc_cls;
  daemon->default_handler = dh;
  daemon->default_handler_cls = dh_cls;
  daemon->connections = 0;
  daemon->connection_limit = MHD_MAX_CONNECTIONS_DEFAULT;
  daemon->pool_size = MHD_POOL_SIZE_DEFAULT;
  daemon->pool_increment = MHD_BUF_INC_SIZE;
  daemon->unescape_callback = &unescape_wrapper;
  daemon->connection_timeout = 0;       /* no timeout */
  MHD_itc_set_invalid_ (daemon->itc);
#ifdef SOMAXCONN
  daemon->listen_backlog_size = SOMAXCONN;
#else  /* !SOMAXCONN */
  daemon->listen_backlog_size = 511; /* should be safe value */
#endif /* !SOMAXCONN */
#ifdef HAVE_MESSAGES
  daemon->custom_error_log = (MHD_LogCallback) &vfprintf;
  daemon->custom_error_log_cls = stderr;
#endif
  if ( (0 != (*pflags & MHD_USE_THREAD_PER_CONNECTION)) &&
       (0 == (*pflags & MHD_USE_INTERNAL_POLLING_THREAD)) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Warning: MHD_USE_THREAD_PER_CONNECTION must be used only with "
                  "MHD_USE_INTERNAL_POLLING_THREAD. Flag MHD_USE_INTERNAL_POLLING_THREAD "
                   "was added. Consider setting MHD_USE_INTERNAL_POLLING_THREAD explicitly.\n"));
#endif
      *pflags |= MHD_USE_INTERNAL_POLLING_THREAD;
    }
  if (0 == (*pflags & MHD_USE_INTERNAL_POLLING_THREAD))
    *pflags &= ~MHD_USE_ITC; /* useless if we are using 'external' select */
  else
    {
#ifdef HAVE_LISTEN_SHUTDOWN
      if (0 != (*pflags & MHD_USE_NO_LISTEN_SOCKET))
#endif
        *pflags |= MHD_USE_ITC; /* yes, must use ITC to signal thread */
    }
#ifdef DAUTH_SUPPORT
  daemon->digest_auth_rand_size = 0;
  daemon->digest_auth_random = NULL;
  daemon->nonce_nc_size = 4; /* tiny */
#endif
#ifdef HTTPS_SUPPORT
  if (0 != (*pflags & MHD_USE_TLS))
    {
      daemon->cred_type = GNUTLS_CRD_CERTIFICATE;
    }
#endif /* HTTPS_SUPPORT */


  if (MHD_YES != parse_options_va (daemon,
                                   &servaddr,
                                   ap))
    {
#ifdef HTTPS_SUPPORT
      if ( (0 != (*pflags & MHD_USE_TLS)) &&
	   (NULL != daemon->priority_cache) )
	gnutls_priority_deinit (daemon->priority_cache);
#endif /* HTTPS_SUPPORT */
      free (daemon);
      return NULL;
    }
  if ( (0 != (*pflags & MHD_USE_ITC)) &&
       (0 == daemon->worker_pool_size) )
    {
      if (! MHD_itc_init_ (daemon->itc))
        {
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Failed to create inter-thread communication channel: %s\n"),
                    MHD_itc_last_strerror_ ());
#endif
#ifdef HTTPS_SUPPORT
          if (NULL != daemon->priority_cache)
            gnutls_priority_deinit (daemon->priority_cache);
#endif /* HTTPS_SUPPORT */
          free (daemon);
          return NULL;
        }
      if ( (0 == (*pflags & (MHD_USE_POLL | MHD_USE_EPOLL))) &&
           (! MHD_SCKT_FD_FITS_FDSET_(MHD_itc_r_fd_ (daemon->itc),
                                      NULL)) )
        {
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("file descriptor for inter-thread communication channel exceeds maximum value\n"));
#endif
          MHD_itc_destroy_chk_ (daemon->itc);
#ifdef HTTPS_SUPPORT
          if (NULL != daemon->priority_cache)
            gnutls_priority_deinit (daemon->priority_cache);
#endif /* HTTPS_SUPPORT */
          free (daemon);
          return NULL;
        }
    }

#ifdef DAUTH_SUPPORT
  if (daemon->nonce_nc_size > 0)
    {
      if ( ( (size_t) (daemon->nonce_nc_size * sizeof (struct MHD_NonceNc))) /
	   sizeof(struct MHD_NonceNc) != daemon->nonce_nc_size)
	{
#ifdef HAVE_MESSAGES
	  MHD_DLOG (daemon,
		    _("Specified value for NC_SIZE too large\n"));
#endif
#ifdef HTTPS_SUPPORT
	  if (0 != (*pflags & MHD_USE_TLS))
	    gnutls_priority_deinit (daemon->priority_cache);
#endif /* HTTPS_SUPPORT */
	  free (daemon);
	  return NULL;
	}
      daemon->nnc = malloc (daemon->nonce_nc_size * sizeof (struct MHD_NonceNc));
      if (NULL == daemon->nnc)
	{
#ifdef HAVE_MESSAGES
	  MHD_DLOG (daemon,
		    _("Failed to allocate memory for nonce-nc map: %s\n"),
		    MHD_strerror_ (errno));
#endif
#ifdef HTTPS_SUPPORT
	  if (0 != (*pflags & MHD_USE_TLS))
	    gnutls_priority_deinit (daemon->priority_cache);
#endif /* HTTPS_SUPPORT */
	  free (daemon);
	  return NULL;
	}
    }

  if (! MHD_mutex_init_ (&daemon->nnc_lock))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("MHD failed to initialize nonce-nc mutex\n"));
#endif
#ifdef HTTPS_SUPPORT
      if (0 != (*pflags & MHD_USE_TLS))
	gnutls_priority_deinit (daemon->priority_cache);
#endif /* HTTPS_SUPPORT */
      free (daemon->nnc);
      free (daemon);
      return NULL;
    }
#endif

  /* Thread pooling currently works only with internal select thread model */
  if ( (0 == (*pflags & MHD_USE_INTERNAL_POLLING_THREAD)) &&
       (daemon->worker_pool_size > 0) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("MHD thread pooling only works with MHD_USE_INTERNAL_POLLING_THREAD\n"));
#endif
      goto free_and_fail;
    }

#ifdef __SYMBIAN32__
  if (0 != (*pflags & (MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION)))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Threaded operations are not supported on Symbian.\n"));
#endif
      goto free_and_fail;
    }
#endif
  if ( (MHD_INVALID_SOCKET == daemon->listen_fd) &&
       (0 == (*pflags & MHD_USE_NO_LISTEN_SOCKET)) )
    {
      /* try to open listen socket */
      listen_fd = MHD_socket_create_listen_(*pflags & MHD_USE_IPv6);
      if (MHD_INVALID_SOCKET == listen_fd)
	{
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Failed to create socket for listening: %s\n"),
                    MHD_socket_last_strerr_ ());
#endif
	  goto free_and_fail;
	}

      /* Apply the socket options according to listening_address_reuse. */
      if (0 == daemon->listening_address_reuse)
        {
#ifndef _WIN32
          /* No user requirement, use "traditional" default SO_REUSEADDR
           * on non-W32 platforms, and do not fail if it doesn't work.
           * Don't use it on W32, because on W32 it will allow multiple
           * bind to the same address:port, like SO_REUSEPORT on others. */
          if (0 > setsockopt (listen_fd,
                              SOL_SOCKET,
                              SO_REUSEADDR,
                              (void*)&on, sizeof (on)))
          {
#ifdef HAVE_MESSAGES
            MHD_DLOG (daemon,
                      _("setsockopt failed: %s\n"),
                      MHD_socket_last_strerr_ ());
#endif
          }
#endif /* ! _WIN32 */
        }
      else if (daemon->listening_address_reuse > 0)
        {
          /* User requested to allow reusing listening address:port. */
#ifndef _WIN32
          /* Use SO_REUSEADDR on non-W32 platforms, and do not fail if
           * it doesn't work. */
          if (0 > setsockopt (listen_fd,
                              SOL_SOCKET,
                              SO_REUSEADDR,
                              (void*)&on, sizeof (on)))
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("setsockopt failed: %s\n"),
                        MHD_socket_last_strerr_ ());
#endif
            }
#endif /* ! _WIN32 */
          /* Use SO_REUSEADDR on Windows and SO_REUSEPORT on most platforms.
           * Fail if SO_REUSEPORT is not defined or setsockopt fails.
           */
          /* SO_REUSEADDR on W32 has the same semantics
             as SO_REUSEPORT on BSD/Linux */
#if defined(_WIN32) || defined(SO_REUSEPORT)
          if (0 > setsockopt (listen_fd,
                              SOL_SOCKET,
#ifndef _WIN32
                              SO_REUSEPORT,
#else  /* _WIN32 */
                              SO_REUSEADDR,
#endif /* _WIN32 */
                              (void *) &on,
                              sizeof (on)))
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("setsockopt failed: %s\n"),
                        MHD_socket_last_strerr_ ());
#endif
              goto free_and_fail;
            }
#else  /* !_WIN32 && !SO_REUSEPORT */
          /* we're supposed to allow address:port re-use, but
             on this platform we cannot; fail hard */
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Cannot allow listening address reuse: SO_REUSEPORT not defined\n"));
#endif
          goto free_and_fail;
#endif /* !_WIN32 && !SO_REUSEPORT */
        }
      else /* if (daemon->listening_address_reuse < 0) */
        {
          /* User requested to disallow reusing listening address:port.
           * Do nothing except for Windows where SO_EXCLUSIVEADDRUSE
           * is used and Solaris with SO_EXCLBIND.
           * Fail if MHD was compiled for W32 without SO_EXCLUSIVEADDRUSE
           * or setsockopt fails.
           */
#if (defined(_WIN32) && defined(SO_EXCLUSIVEADDRUSE)) || \
    (defined(__sun) && defined(SO_EXCLBIND))
          if (0 > setsockopt (listen_fd,
                              SOL_SOCKET,
#ifdef SO_EXCLUSIVEADDRUSE
                              SO_EXCLUSIVEADDRUSE,
#else  /* SO_EXCLBIND */
                              SO_EXCLBIND,
#endif /* SO_EXCLBIND */
                              (void *) &on,
                              sizeof (on)))
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("setsockopt failed: %s\n"),
                        MHD_socket_last_strerr_ ());
#endif
              goto free_and_fail;
            }
#elif defined(_WIN32) /* SO_EXCLUSIVEADDRUSE not defined on W32? */
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Cannot disallow listening address reuse: SO_EXCLUSIVEADDRUSE not defined\n"));
#endif
          goto free_and_fail;
#endif /* _WIN32 */
        }

      /* check for user supplied sockaddr */
#if HAVE_INET6
      if (0 != (*pflags & MHD_USE_IPv6))
	addrlen = sizeof (struct sockaddr_in6);
      else
#endif
	addrlen = sizeof (struct sockaddr_in);
      if (NULL == servaddr)
	{
#if HAVE_INET6
	  if (0 != (*pflags & MHD_USE_IPv6))
	    {
	      memset (&servaddr6,
                      0,
                      sizeof (struct sockaddr_in6));
	      servaddr6.sin6_family = AF_INET6;
	      servaddr6.sin6_port = htons (port);
#if HAVE_SOCKADDR_IN_SIN_LEN
	      servaddr6.sin6_len = sizeof (struct sockaddr_in6);
#endif
	      servaddr = (struct sockaddr *) &servaddr6;
	    }
	  else
#endif
	    {
	      memset (&servaddr4,
                      0,
                      sizeof (struct sockaddr_in));
	      servaddr4.sin_family = AF_INET;
	      servaddr4.sin_port = htons (port);
#if HAVE_SOCKADDR_IN_SIN_LEN
	      servaddr4.sin_len = sizeof (struct sockaddr_in);
#endif
	      servaddr = (struct sockaddr *) &servaddr4;
	    }
	}
      daemon->listen_fd = listen_fd;

      if (0 != (*pflags & MHD_USE_IPv6))
	{
#ifdef IPPROTO_IPV6
#ifdef IPV6_V6ONLY
	  /* Note: "IPV6_V6ONLY" is declared by Windows Vista ff., see "IPPROTO_IPV6 Socket Options"
	     (http://msdn.microsoft.com/en-us/library/ms738574%28v=VS.85%29.aspx);
	     and may also be missing on older POSIX systems; good luck if you have any of those,
	     your IPv6 socket may then also bind against IPv4 anyway... */
	  const MHD_SCKT_OPT_BOOL_ v6_only =
            (MHD_USE_DUAL_STACK != (*pflags & MHD_USE_DUAL_STACK));
	  if (0 > setsockopt (listen_fd,
                              IPPROTO_IPV6, IPV6_V6ONLY,
                              (const void *) &v6_only,
                              sizeof (v6_only)))
      {
#ifdef HAVE_MESSAGES
            MHD_DLOG (daemon,
                      _("setsockopt failed: %s\n"),
                      MHD_socket_last_strerr_ ());
#endif
      }
#endif
#endif
	}
      if (-1 == bind (listen_fd, servaddr, addrlen))
	{
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Failed to bind to port %u: %s\n"),
                    (unsigned int) port,
                    MHD_socket_last_strerr_ ());
#endif
	  MHD_socket_close_chk_ (listen_fd);
	  goto free_and_fail;
	}
#ifdef TCP_FASTOPEN
      if (0 != (*pflags & MHD_USE_TCP_FASTOPEN))
      {
        if (0 == daemon->fastopen_queue_size)
          daemon->fastopen_queue_size = MHD_TCP_FASTOPEN_QUEUE_SIZE_DEFAULT;
        if (0 != setsockopt (listen_fd,
                             IPPROTO_TCP,
                             TCP_FASTOPEN,
                             &daemon->fastopen_queue_size,
                             sizeof (daemon->fastopen_queue_size)))
        {
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("setsockopt failed: %s\n"),
                    MHD_socket_last_strerr_ ());
#endif
        }
      }
#endif
      if (listen (listen_fd,
                  daemon->listen_backlog_size) < 0)
	{
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Failed to listen for connections: %s\n"),
                    MHD_socket_last_strerr_ ());
#endif
	  MHD_socket_close_chk_ (listen_fd);
	  goto free_and_fail;
	}
    }
  else
    {
      listen_fd = daemon->listen_fd;
    }

  if ( (MHD_INVALID_SOCKET != listen_fd) &&
       (! MHD_socket_nonblocking_ (listen_fd)) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to set nonblocking mode on listening socket: %s\n"),
                MHD_socket_last_strerr_());
#endif
      if (0 != (*pflags & MHD_USE_EPOLL) ||
          daemon->worker_pool_size > 0)
        {
           /* Accept must be non-blocking. Multiple children may wake up
            * to handle a new connection, but only one will win the race.
            * The others must immediately return. */
          MHD_socket_close_chk_ (listen_fd);
          goto free_and_fail;
        }
    }
  if ( (MHD_INVALID_SOCKET != listen_fd) &&
       (! MHD_SCKT_FD_FITS_FDSET_(listen_fd,
                                  NULL)) &&
       (0 == (*pflags & (MHD_USE_POLL | MHD_USE_EPOLL)) ) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Socket descriptor larger than FD_SETSIZE: %d > %d\n"),
                listen_fd,
                FD_SETSIZE);
#endif
      MHD_socket_close_chk_ (listen_fd);
      goto free_and_fail;
    }

#ifdef EPOLL_SUPPORT
  if ( (0 != (*pflags & MHD_USE_EPOLL)) &&
       (0 == daemon->worker_pool_size) &&
       (0 == (*pflags & MHD_USE_NO_LISTEN_SOCKET)) )
    {
      if (0 != (*pflags & MHD_USE_THREAD_PER_CONNECTION))
	{
#ifdef HAVE_MESSAGES
	  MHD_DLOG (daemon,
		    _("Combining MHD_USE_THREAD_PER_CONNECTION and MHD_USE_EPOLL is not supported.\n"));
#endif
	  goto free_and_fail;
	}
      if (MHD_YES != setup_epoll_to_listen (daemon))
	goto free_and_fail;
    }
#endif /* EPOLL_SUPPORT */

  if (! MHD_mutex_init_ (&daemon->per_ip_connection_mutex))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("MHD failed to initialize IP connection limit mutex\n"));
#endif
      if (MHD_INVALID_SOCKET != listen_fd)
        MHD_socket_close_chk_ (listen_fd);
      goto free_and_fail;
    }
  if (! MHD_mutex_init_ (&daemon->cleanup_connection_mutex))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("MHD failed to initialize IP connection limit mutex\n"));
#endif
      MHD_mutex_destroy_chk_ (&daemon->cleanup_connection_mutex);
      if (MHD_INVALID_SOCKET != listen_fd)
        MHD_socket_close_chk_ (listen_fd);
      goto free_and_fail;
    }

#ifdef HTTPS_SUPPORT
  /* initialize HTTPS daemon certificate aspects & send / recv functions */
  if ( (0 != (*pflags & MHD_USE_TLS)) &&
       (0 != MHD_TLS_init (daemon)) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Failed to initialize TLS support\n"));
#endif
      if (MHD_INVALID_SOCKET != listen_fd)
        MHD_socket_close_chk_ (listen_fd);
      MHD_mutex_destroy_chk_ (&daemon->cleanup_connection_mutex);
      MHD_mutex_destroy_chk_ (&daemon->per_ip_connection_mutex);
      goto free_and_fail;
    }
#endif /* HTTPS_SUPPORT */
  if ( ( (0 != (*pflags & MHD_USE_INTERNAL_POLLING_THREAD)) &&
	 (0 == daemon->worker_pool_size) ) &&
       (0 == (*pflags & MHD_USE_NO_LISTEN_SOCKET)) &&
       (! MHD_create_named_thread_ (&daemon->pid,
                                    (*pflags & MHD_USE_THREAD_PER_CONNECTION) ?
                                    "MHD-listen" : "MHD-single",
                                    daemon->thread_stack_size,
                                    &MHD_select_thread,
                                    daemon) ) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Failed to create listen thread: %s\n"),
		MHD_strerror_ (errno));
#endif
      MHD_mutex_destroy_chk_ (&daemon->cleanup_connection_mutex);
      MHD_mutex_destroy_chk_ (&daemon->per_ip_connection_mutex);
      if (MHD_INVALID_SOCKET != listen_fd)
        MHD_socket_close_chk_ (listen_fd);
      goto free_and_fail;
    }
  if ( (daemon->worker_pool_size > 0) &&
       (0 == (*pflags & MHD_USE_NO_LISTEN_SOCKET)) )
    {
      /* Coarse-grained count of connections per thread (note error
       * due to integer division). Also keep track of how many
       * connections are leftover after an equal split. */
      unsigned int conns_per_thread = daemon->connection_limit
                                      / daemon->worker_pool_size;
      unsigned int leftover_conns = daemon->connection_limit
                                    % daemon->worker_pool_size;

      i = 0; /* we need this in case fcntl or malloc fails */

      /* Allocate memory for pooled objects */
      daemon->worker_pool = malloc (sizeof (struct MHD_Daemon)
                                    * daemon->worker_pool_size);
      if (NULL == daemon->worker_pool)
        goto thread_failed;

      /* Start the workers in the pool */
      for (i = 0; i < daemon->worker_pool_size; ++i)
        {
          /* Create copy of the Daemon object for each worker */
          struct MHD_Daemon *d = &daemon->worker_pool[i];

          memcpy (d, daemon, sizeof (struct MHD_Daemon));
          /* Adjust pooling params for worker daemons; note that memcpy()
             has already copied MHD_USE_INTERNAL_POLLING_THREAD thread model into
             the worker threads. */
          d->master = daemon;
          d->worker_pool_size = 0;
          d->worker_pool = NULL;

          if (0 != (*pflags & MHD_USE_ITC))
            {
              if (! MHD_itc_init_ (d->itc))
                {
#ifdef HAVE_MESSAGES
                  MHD_DLOG (daemon,
                            _("Failed to create worker inter-thread communication channel: %s\n"),
                            MHD_itc_last_strerror_() );
#endif
                  goto thread_failed;
                }
              if ( (0 == (*pflags & (MHD_USE_POLL | MHD_USE_EPOLL))) &&
                   (! MHD_SCKT_FD_FITS_FDSET_(MHD_itc_r_fd_ (d->itc),
                                              NULL)) )
                {
#ifdef HAVE_MESSAGES
                  MHD_DLOG (daemon,
                            _("File descriptor for worker inter-thread communication channel exceeds maximum value\n"));
#endif
                  MHD_itc_destroy_chk_ (d->itc);
                  goto thread_failed;
                }
            }
          else
            MHD_itc_set_invalid_ (d->itc);

          /* Divide available connections evenly amongst the threads.
           * Thread indexes in [0, leftover_conns) each get one of the
           * leftover connections. */
          d->connection_limit = conns_per_thread;
          if (i < leftover_conns)
            ++d->connection_limit;
#ifdef EPOLL_SUPPORT
	  if ( (0 != (*pflags & MHD_USE_EPOLL)) &&
	       (MHD_YES != setup_epoll_to_listen (d)) )
	    goto thread_failed;
#endif
          /* Must init cleanup connection mutex for each worker */
          if (! MHD_mutex_init_ (&d->cleanup_connection_mutex))
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("MHD failed to initialize cleanup connection mutex\n"));
#endif
              goto thread_failed;
            }

          /* Spawn the worker thread */
          if (! MHD_create_named_thread_ (&d->pid,
                                          "MHD-worker",
                                          daemon->thread_stack_size,
                                          &MHD_select_thread,
                                          d))
            {
#ifdef HAVE_MESSAGES
              MHD_DLOG (daemon,
                        _("Failed to create pool thread: %s\n"),
			MHD_strerror_ (errno));
#endif
              /* Free memory for this worker; cleanup below handles
               * all previously-created workers. */
              MHD_mutex_destroy_chk_ (&d->cleanup_connection_mutex);
              goto thread_failed;
            }
        }
    }
#ifdef HTTPS_SUPPORT
  /* API promises to never use the password after initialization,
     so we additionally NULL it here to not deref a dangling pointer. */
  daemon->https_key_password = NULL;
#endif /* HTTPS_SUPPORT */

  return daemon;

thread_failed:
  /* If no worker threads created, then shut down normally. Calling
     MHD_stop_daemon (as we do below) doesn't work here since it
     assumes a 0-sized thread pool means we had been in the default
     MHD_USE_INTERNAL_POLLING_THREAD mode. */
  if (0 == i)
    {
      if (MHD_INVALID_SOCKET != listen_fd)
        MHD_socket_close_chk_ (listen_fd);
      MHD_mutex_destroy_chk_ (&daemon->cleanup_connection_mutex);
      MHD_mutex_destroy_chk_ (&daemon->per_ip_connection_mutex);
      if (NULL != daemon->worker_pool)
        free (daemon->worker_pool);
      goto free_and_fail;
    }

  /* Shutdown worker threads we've already created. Pretend
     as though we had fully initialized our daemon, but
     with a smaller number of threads than had been
     requested. */
  daemon->worker_pool_size = i;
  MHD_stop_daemon (daemon);
  return NULL;

 free_and_fail:
  /* clean up basic memory state in 'daemon' and return NULL to
     indicate failure */
#ifdef EPOLL_SUPPORT
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  if (daemon->upgrade_fd_in_epoll)
    {
      if (0 != epoll_ctl (daemon->epoll_fd,
			  EPOLL_CTL_DEL,
			  daemon->epoll_upgrade_fd,
			  NULL))
	MHD_PANIC (_("Failed to remove FD from epoll set\n"));
      daemon->upgrade_fd_in_epoll = false;
    }
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
  if (-1 != daemon->epoll_fd)
    close (daemon->epoll_fd);
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  if (-1 != daemon->epoll_upgrade_fd)
    close (daemon->epoll_upgrade_fd);
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
#endif /* EPOLL_SUPPORT */
#ifdef DAUTH_SUPPORT
  free (daemon->nnc);
  MHD_mutex_destroy_chk_ (&daemon->nnc_lock);
#endif
#ifdef HTTPS_SUPPORT
  if (0 != (*pflags & MHD_USE_TLS))
    gnutls_priority_deinit (daemon->priority_cache);
#endif /* HTTPS_SUPPORT */
  if (MHD_ITC_IS_VALID_(daemon->itc))
    MHD_itc_destroy_chk_ (daemon->itc);
  free (daemon);
  return NULL;
}


/**
 * Close all connections for the daemon.
 * Must only be called when MHD_Daemon::shutdown was set to true.
 * @remark To be called only from thread that process
 * daemon's select()/poll()/etc.
 *
 * @param daemon daemon to close down
 */
static void
close_all_connections (struct MHD_Daemon *daemon)
{
  struct MHD_Connection *pos;
  const bool used_thr_p_c = (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION));
#ifdef UPGRADE_SUPPORT
  const bool upg_allowed = (0 != (daemon->options & MHD_ALLOW_UPGRADE));
#endif /* UPGRADE_SUPPORT */
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  struct MHD_UpgradeResponseHandle *urh;
  struct MHD_UpgradeResponseHandle *urhn;
  const bool used_tls = (0 != (daemon->options & MHD_USE_TLS));

  /* give upgraded HTTPS connections a chance to finish */
  /* 'daemon->urh_head' is not used in thread-per-connection mode. */
  for (urh = daemon->urh_tail; NULL != urh; urh = urhn)
    {
      urhn = urh->prev;
      /* call generic forwarding function for passing data
         with chance to detect that application is done. */
      process_urh (urh);
      MHD_connection_finish_forward_ (urh->connection);
      urh->clean_ready = true;
      /* Resuming will move connection to cleanup list. */
      MHD_resume_connection(urh->connection);
    }
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */

  /* Give suspended connections a chance to resume to avoid
     running into the check for there not being any suspended
     connections left in case of a tight race with a recently
     resumed connection. */
  if (0 != (MHD_TEST_ALLOW_SUSPEND_RESUME & daemon->options))
    {
      daemon->resuming = true; /* Force check for pending resume. */
      resume_suspended_connections (daemon);
    }
  /* first, make sure all threads are aware of shutdown; need to
     traverse DLLs in peace... */
  MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
#ifdef UPGRADE_SUPPORT
  if (upg_allowed)
    {
      struct MHD_Connection * susp;

      susp = daemon->suspended_connections_tail;
      while (NULL != susp)
        {
          if (NULL == susp->urh) /* "Upgraded" connection? */
            MHD_PANIC (_("MHD_stop_daemon() called while we have suspended connections.\n"));
#ifdef HTTPS_SUPPORT
          else if (used_tls &&
                   used_thr_p_c &&
                   (! susp->urh->clean_ready) )
            shutdown (susp->urh->app.socket,
                      SHUT_RDWR); /* Wake thread by shutdown of app socket. */
#endif /* HTTPS_SUPPORT */
          else
            {
#ifdef HAVE_MESSAGES
              if (! susp->urh->was_closed)
                MHD_DLOG (daemon,
                          _("Initiated daemon shutdown while \"upgraded\" connection was not closed.\n"));
#endif
              susp->urh->was_closed = true;
              /* If thread-per-connection is used, connection's thread
               * may still processing "upgrade" (exiting). */
              if (! used_thr_p_c)
                MHD_connection_finish_forward_ (susp);
              /* Do not use MHD_resume_connection() as mutex is
               * already locked. */
              susp->resuming = true;
              daemon->resuming = true;
            }
          susp = susp->prev;
        }
    }
  else /* This 'else' is combined with next 'if' */
#endif /* UPGRADE_SUPPORT */
  if (NULL != daemon->suspended_connections_head)
    MHD_PANIC (_("MHD_stop_daemon() called while we have suspended connections.\n"));
  for (pos = daemon->connections_tail; NULL != pos; pos = pos->prev)
    {
      shutdown (pos->socket_fd,
                SHUT_RDWR);
#if MHD_WINSOCK_SOCKETS
      if ( (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
           (MHD_ITC_IS_VALID_(daemon->itc)) &&
           (! MHD_itc_activate_ (daemon->itc, "e")) )
        MHD_PANIC (_("Failed to signal shutdown via inter-thread communication channel"));
#endif
    }

  /* now, collect per-connection threads */
  if (used_thr_p_c)
    {
      pos = daemon->connections_tail;
      while (NULL != pos)
      {
        if (! pos->thread_joined)
          {
            MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);
            if (! MHD_join_thread_ (pos->pid))
              MHD_PANIC (_("Failed to join a thread\n"));
            MHD_mutex_lock_chk_ (&daemon->cleanup_connection_mutex);
            pos->thread_joined = true;
            /* The thread may have concurrently modified the DLL,
               need to restart from the beginning */
            pos = daemon->connections_tail;
            continue;
          }
        pos = pos->prev;
      }
    }
  MHD_mutex_unlock_chk_ (&daemon->cleanup_connection_mutex);

#ifdef UPGRADE_SUPPORT
  /* Finished threads with "upgraded" connections need to be moved
   * to cleanup list by resume_suspended_connections(). */
  /* "Upgraded" connections that were not closed explicitly by
   * application should be moved to cleanup list too. */
  if (upg_allowed)
    {
      daemon->resuming = true; /* Force check for pending resume. */
      resume_suspended_connections (daemon);
    }
#endif /* UPGRADE_SUPPORT */

  /* now that we're alone, move everyone to cleanup */
  while (NULL != (pos = daemon->connections_tail))
  {
    if ( (0 != (daemon->options & MHD_USE_THREAD_PER_CONNECTION)) &&
         (! pos->thread_joined) )
      MHD_PANIC (_("Failed to join a thread\n"));
    close_connection (pos);
  }
  MHD_cleanup_connections (daemon);
}


/**
 * Shutdown an HTTP daemon.
 *
 * @param daemon daemon to stop
 * @ingroup event
 */
void
MHD_stop_daemon (struct MHD_Daemon *daemon)
{
  MHD_socket fd;
  unsigned int i;

  if (NULL == daemon)
    return;

  if (0 != (MHD_TEST_ALLOW_SUSPEND_RESUME & daemon->options))
    resume_suspended_connections (daemon);

  daemon->shutdown = true;
  fd = daemon->listen_fd;

  if (0 != (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD))
    {
      /* Separate thread(s) is used for select()/poll()/etc. */
      if (NULL != daemon->worker_pool)
        {
          /* Pool of workers is used.  */
          /* Initiate shutdown process in wokers. */
          for (i = 0; i < daemon->worker_pool_size; ++i)
            {
              daemon->worker_pool[i].shutdown = true;
              if (MHD_ITC_IS_VALID_(daemon->worker_pool[i].itc))
                {
                  if (! MHD_itc_activate_ (daemon->worker_pool[i].itc, "e"))
                    MHD_PANIC (_("Failed to signal shutdown via inter-thread communication channel."));
                }
#ifdef HAVE_LISTEN_SHUTDOWN
              else if (MHD_INVALID_SOCKET != fd)
                {
                  /* fd might be MHD_INVALID_SOCKET here due to 'MHD_quiesce_daemon' */
                  /* No problem if shutdown will be called several times for the same socket. */
                    (void) shutdown (fd,
                                     SHUT_RDWR);
                }
#endif
            }
          /* Start harvesting. */
          for (i = 0; i < daemon->worker_pool_size; ++i)
            {
              if (! MHD_join_thread_ (daemon->worker_pool[i].pid))
                MHD_PANIC (_("Failed to join a thread\n"));
#ifdef EPOLL_SUPPORT
              if (-1 != daemon->worker_pool[i].epoll_fd)
                MHD_fd_close_chk_ (daemon->worker_pool[i].epoll_fd);
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
              if (-1 != daemon->worker_pool[i].epoll_upgrade_fd)
                MHD_fd_close_chk_ (daemon->worker_pool[i].epoll_upgrade_fd);
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
#endif
              if (MHD_ITC_IS_VALID_ (daemon->worker_pool[i].itc) )
                MHD_itc_destroy_chk_ (daemon->worker_pool[i].itc);
              MHD_mutex_destroy_chk_ (&daemon->worker_pool[i].cleanup_connection_mutex);
            }
          free (daemon->worker_pool);
        }
      else
        {
          /* Single internal thread is used for select()/poll()/etc. */
          if (MHD_ITC_IS_VALID_(daemon->itc))
            {
              if (! MHD_itc_activate_ (daemon->itc, "e"))
                MHD_PANIC (_("Failed to signal shutdown via inter-thread communication channel"));
            }
#ifdef HAVE_LISTEN_SHUTDOWN
          else
            {
              /* fd might be MHD_INVALID_SOCKET here due to 'MHD_quiesce_daemon' */
              if ( (MHD_INVALID_SOCKET != fd) &&
                   (! daemon->was_quiesced) )
                (void) shutdown (fd,
                                 SHUT_RDWR);
            }
#endif
          if (! MHD_join_thread_ (daemon->pid))
            {
              MHD_PANIC (_("Failed to join a thread\n"));
            }
        }
    }
  else
    {
      /* Internal threads are not used for select()/poll()/etc. */
      close_all_connections (daemon);
    }

  if ( (MHD_INVALID_SOCKET != fd) &&
       (! daemon->was_quiesced) )
    MHD_socket_close_chk_ (fd);

  if (MHD_ITC_IS_VALID_ (daemon->itc))
    MHD_itc_destroy_chk_ (daemon->itc);

#ifdef EPOLL_SUPPORT
  if ( (0 != (daemon->options & MHD_USE_EPOLL)) &&
       (-1 != daemon->epoll_fd) )
    MHD_socket_close_chk_ (daemon->epoll_fd);
#if defined(HTTPS_SUPPORT) && defined(UPGRADE_SUPPORT)
  if ( (0 != (daemon->options & MHD_USE_EPOLL)) &&
       (-1 != daemon->epoll_upgrade_fd) )
    MHD_socket_close_chk_ (daemon->epoll_upgrade_fd);
#endif /* HTTPS_SUPPORT && UPGRADE_SUPPORT */
#endif

  /* TLS clean up */
#ifdef HTTPS_SUPPORT
  if (daemon->have_dhparams)
    {
      gnutls_dh_params_deinit (daemon->https_mem_dhparams);
      daemon->have_dhparams = false;
    }
  if (0 != (daemon->options & MHD_USE_TLS))
    {
      gnutls_priority_deinit (daemon->priority_cache);
      if (daemon->x509_cred)
        gnutls_certificate_free_credentials (daemon->x509_cred);
    }
#endif /* HTTPS_SUPPORT */

#ifdef DAUTH_SUPPORT
  free (daemon->nnc);
  MHD_mutex_destroy_chk_ (&daemon->nnc_lock);
#endif
  MHD_mutex_destroy_chk_ (&daemon->per_ip_connection_mutex);
  MHD_mutex_destroy_chk_ (&daemon->cleanup_connection_mutex);

  free (daemon);
}


/**
 * Obtain information about the given daemon
 * (not fully implemented!).
 *
 * @param daemon what daemon to get information about
 * @param info_type what information is desired?
 * @param ... depends on @a info_type
 * @return NULL if this information is not available
 *         (or if the @a info_type is unknown)
 * @ingroup specialized
 */
const union MHD_DaemonInfo *
MHD_get_daemon_info (struct MHD_Daemon *daemon,
		     enum MHD_DaemonInfoType info_type,
		     ...)
{
  if (NULL == daemon)
    return NULL;
  switch (info_type)
    {
    case MHD_DAEMON_INFO_KEY_SIZE:
      return NULL; /* no longer supported */
    case MHD_DAEMON_INFO_MAC_KEY_SIZE:
      return NULL; /* no longer supported */
    case MHD_DAEMON_INFO_LISTEN_FD:
      return (const union MHD_DaemonInfo *) &daemon->listen_fd;
#ifdef EPOLL_SUPPORT
    case MHD_DAEMON_INFO_EPOLL_FD:
      return (const union MHD_DaemonInfo *) &daemon->epoll_fd;
#endif
    case MHD_DAEMON_INFO_CURRENT_CONNECTIONS:
      if (0 == (daemon->options & MHD_USE_INTERNAL_POLLING_THREAD))
        {
          /* Assume that MHD_run() in not called in other thread
           * at the same time. */
          MHD_cleanup_connections (daemon);
        }
      else if (daemon->worker_pool)
        {
          unsigned int i;
          /* Collect the connection information stored in the workers. */
          daemon->connections = 0;
          for (i = 0; i < daemon->worker_pool_size; i++)
            {
              /* FIXME: next line is thread-safe only if read is atomic. */
              daemon->connections += daemon->worker_pool[i].connections;
            }
        }
      return (const union MHD_DaemonInfo *) &daemon->connections;
    case MHD_DAEMON_INFO_FLAGS:
      return (const union MHD_DaemonInfo *) &daemon->options;
    default:
      return NULL;
    };
}


/**
 * Sets the global error handler to a different implementation.  @a cb
 * will only be called in the case of typically fatal, serious
 * internal consistency issues.  These issues should only arise in the
 * case of serious memory corruption or similar problems with the
 * architecture.  While @a cb is allowed to return and MHD will then
 * try to continue, this is never safe.
 *
 * The default implementation that is used if no panic function is set
 * simply prints an error message and calls `abort()`.  Alternative
 * implementations might call `exit()` or other similar functions.
 *
 * @param cb new error handler
 * @param cls passed to @a cb
 * @ingroup logging
 */
void
MHD_set_panic_func (MHD_PanicCallback cb,
                    void *cls)
{
  mhd_panic = cb;
  mhd_panic_cls = cls;
}


/**
 * Obtain the version of this library
 *
 * @return static version string, e.g. "0.9.9"
 * @ingroup specialized
 */
const char *
MHD_get_version (void)
{
#ifdef PACKAGE_VERSION
  return PACKAGE_VERSION;
#else  /* !PACKAGE_VERSION */
  static char ver[12] = "\0\0\0\0\0\0\0\0\0\0\0";
  if (0 == ver[0])
  {
    int res = MHD_snprintf_(ver,
                            sizeof(ver),
                            "%x.%x.%x",
                            (((int)MHD_VERSION >> 24) & 0xFF),
                            (((int)MHD_VERSION >> 16) & 0xFF),
                            (((int)MHD_VERSION >> 8) & 0xFF));
    if (0 >= res || sizeof(ver) <= res)
      return "0.0.0"; /* Can't return real version*/
  }
  return ver;
#endif /* !PACKAGE_VERSION */
}


/**
 * Get information about supported MHD features.
 * Indicate that MHD was compiled with or without support for
 * particular feature. Some features require additional support
 * by kernel. Kernel support is not checked by this function.
 *
 * @param feature type of requested information
 * @return #MHD_YES if feature is supported by MHD, #MHD_NO if
 * feature is not supported or feature is unknown.
 * @ingroup specialized
 */
_MHD_EXTERN int
MHD_is_feature_supported(enum MHD_FEATURE feature)
{
  switch(feature)
    {
    case MHD_FEATURE_MESSAGES:
#ifdef HAVE_MESSAGES
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_TLS:
#ifdef HTTPS_SUPPORT
      return MHD_YES;
#else  /* ! HTTPS_SUPPORT */
      return MHD_NO;
#endif  /* ! HTTPS_SUPPORT */
    case MHD_FEATURE_HTTPS_CERT_CALLBACK:
#if defined(HTTPS_SUPPORT) && GNUTLS_VERSION_MAJOR >= 3
      return MHD_YES;
#else  /* !HTTPS_SUPPORT || GNUTLS_VERSION_MAJOR < 3 */
      return MHD_NO;
#endif /* !HTTPS_SUPPORT || GNUTLS_VERSION_MAJOR < 3 */
    case MHD_FEATURE_IPv6:
#ifdef HAVE_INET6
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_IPv6_ONLY:
#if defined(IPPROTO_IPV6) && defined(IPV6_V6ONLY)
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_POLL:
#ifdef HAVE_POLL
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_EPOLL:
#ifdef EPOLL_SUPPORT
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_SHUTDOWN_LISTEN_SOCKET:
#ifdef HAVE_LISTEN_SHUTDOWN
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_SOCKETPAIR:
#ifdef _MHD_ITC_SOCKETPAIR
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_TCP_FASTOPEN:
#ifdef TCP_FASTOPEN
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_BASIC_AUTH:
#ifdef BAUTH_SUPPORT
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_DIGEST_AUTH:
#ifdef DAUTH_SUPPORT
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_POSTPROCESSOR:
#ifdef HAVE_POSTPROCESSOR
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_HTTPS_KEY_PASSWORD:
#if defined(HTTPS_SUPPORT) && GNUTLS_VERSION_NUMBER >= 0x030111
      return MHD_YES;
#else  /* !HTTPS_SUPPORT || GNUTLS_VERSION_NUMBER < 0x030111 */
      return MHD_NO;
#endif /* !HTTPS_SUPPORT || GNUTLS_VERSION_NUMBER < 0x030111 */
    case MHD_FEATURE_LARGE_FILE:
#if defined(HAVE_PREAD64) || defined(_WIN32)
      return MHD_YES;
#elif defined(HAVE_PREAD)
      return (sizeof(uint64_t) > sizeof(off_t)) ? MHD_NO : MHD_YES;
#elif defined(HAVE_LSEEK64)
      return MHD_YES;
#else
      return (sizeof(uint64_t) > sizeof(off_t)) ? MHD_NO : MHD_YES;
#endif
    case MHD_FEATURE_THREAD_NAMES:
#if defined(MHD_USE_THREAD_NAME_)
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_UPGRADE:
#if defined(UPGRADE_SUPPORT)
      return MHD_YES;
#else
      return MHD_NO;
#endif
    case MHD_FEATURE_RESPONSES_SHARED_FD:
#if defined(HAVE_PREAD64) || defined(HAVE_PREAD) || defined(_WIN32)
      return MHD_YES;
#else
      return MHD_NO;
#endif
    }
  return MHD_NO;
}


#if defined(HTTPS_SUPPORT) && GCRYPT_VERSION_NUMBER < 0x010600
#if defined(MHD_USE_POSIX_THREADS)
GCRY_THREAD_OPTION_PTHREAD_IMPL;
#elif defined(MHD_W32_MUTEX_)

static int
gcry_w32_mutex_init (void **ppmtx)
{
  *ppmtx = malloc (sizeof (MHD_mutex_));

  if (NULL == *ppmtx)
    return ENOMEM;
  if (!MHD_mutex_init_ ((MHD_mutex_*)*ppmtx))
    {
      free (*ppmtx);
      *ppmtx = NULL;
      return EPERM;
    }

  return 0;
}


static int
gcry_w32_mutex_destroy (void **ppmtx)
{
  int res = (MHD_mutex_destroy_chk_ ((MHD_mutex_*)*ppmtx)) ? 0 : 1;
  free (*ppmtx);
  return res;
}


static int
gcry_w32_mutex_lock (void **ppmtx)
{
  MHD_mutex_lock_chk_ ((MHD_mutex_*)*ppmtx);
  return 0;
}


static int
gcry_w32_mutex_unlock (void **ppmtx)
{
  MHD_mutex_unlock_chk_ ((MHD_mutex_*)*ppmtx);
  return 0;
}


static struct gcry_thread_cbs gcry_threads_w32 = {
  (GCRY_THREAD_OPTION_USER | (GCRY_THREAD_OPTION_VERSION << 8)),
  NULL, gcry_w32_mutex_init, gcry_w32_mutex_destroy,
  gcry_w32_mutex_lock, gcry_w32_mutex_unlock,
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

#endif /* defined(MHD_W32_MUTEX_) */
#endif /* HTTPS_SUPPORT && GCRYPT_VERSION_NUMBER < 0x010600 */


/**
 * Initialize do setup work.
 */
void
MHD_init(void)
{
#ifdef _WIN32
  WSADATA wsd;
#endif /* _WIN32 */
  mhd_panic = &mhd_panic_std;
  mhd_panic_cls = NULL;

#ifdef _WIN32
  if (0 != WSAStartup(MAKEWORD(2, 2), &wsd))
    MHD_PANIC (_("Failed to initialize winsock\n"));
  mhd_winsock_inited_ = 1;
  if (2 != LOBYTE(wsd.wVersion) && 2 != HIBYTE(wsd.wVersion))
    MHD_PANIC (_("Winsock version 2.2 is not available\n"));
#endif
#ifdef HTTPS_SUPPORT
#if GCRYPT_VERSION_NUMBER < 0x010600
#if defined(MHD_USE_POSIX_THREADS)
  if (0 != gcry_control (GCRYCTL_SET_THREAD_CBS,
                         &gcry_threads_pthread))
    MHD_PANIC (_("Failed to initialise multithreading in libgcrypt\n"));
#elif defined(MHD_W32_MUTEX_)
  if (0 != gcry_control (GCRYCTL_SET_THREAD_CBS,
                         &gcry_threads_w32))
    MHD_PANIC (_("Failed to initialise multithreading in libgcrypt\n"));
#endif /* defined(MHD_W32_MUTEX_) */
  gcry_check_version (NULL);
#else
  if (NULL == gcry_check_version ("1.6.0"))
    MHD_PANIC (_("libgcrypt is too old. MHD was compiled for libgcrypt 1.6.0 or newer\n"));
#endif
  gnutls_global_init ();
#endif /* HTTPS_SUPPORT */
  MHD_monotonic_sec_counter_init();
}


void
MHD_fini(void)
{
#ifdef HTTPS_SUPPORT
  gnutls_global_deinit ();
#endif /* HTTPS_SUPPORT */
#ifdef _WIN32
  if (mhd_winsock_inited_)
    WSACleanup();
#endif
  MHD_monotonic_sec_counter_finish();
}

_SET_INIT_AND_DEINIT_FUNCS(MHD_init, MHD_fini);

/* end of daemon.c */
