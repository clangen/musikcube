/*
     This file is part of libmicrohttpd
     Copyright (C) 2016 Christian Grothoff

     libmicrohttpd is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 3, or (at your
     option) any later version.

     libmicrohttpd is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libmicrohttpd; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
     Boston, MA 02110-1301, USA.
*/

/**
 * @file test_upgrade.c
 * @brief  Testcase for libmicrohttpd upgrading a connection
 * @author Christian Grothoff
 * @author Karlson2k (Evgeny Grin)
 */

#include "mhd_options.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <stddef.h>
#ifndef WINDOWS
#include <unistd.h>
#endif
#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif /* HAVE_STDBOOL_H */

#include "mhd_sockets.h"
#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif /* HAVE_NETINET_IP_H */

#include "platform.h"
#include "microhttpd.h"

#include "test_helpers.h"

#ifdef HTTPS_SUPPORT
#include <gnutls/gnutls.h>
#include "../testcurl/https/tls_test_keys.h"

#if defined(HAVE_FORK) && defined(HAVE_WAITPID)
#include <sys/types.h>
#include <sys/wait.h>
#endif /* HAVE_FORK && HAVE_WAITPID */
#endif /* HTTPS_SUPPORT */

static int verbose = 0;

enum tls_tool
{
  TLS_CLI_NO_TOOL = 0,
  TLS_CLI_GNUTLS,
  TLS_CLI_OPENSSL,
  TLS_LIB_GNUTLS
};

enum tls_tool use_tls_tool;

#if defined(HTTPS_SUPPORT) && defined(HAVE_FORK) && defined(HAVE_WAITPID)
/**
 * Fork child that connects via GnuTLS-CLI to our @a port.  Allows us to
 * talk to our port over a socket in @a sp without having to worry
 * about TLS.
 *
 * @param location where the socket is returned
 * @return -1 on error, otherwise PID of TLS child process
 */
static pid_t
gnutlscli_connect (int *sock,
                 uint16_t port)
{
  pid_t chld;
  int sp[2];
  char destination[30];

  if (0 != socketpair (AF_UNIX,
                       SOCK_STREAM,
                       0,
                       sp))
    return -1;
  chld = fork ();
  if (0 != chld)
    {
      *sock = sp[1];
      MHD_socket_close_chk_ (sp[0]);
      return chld;
    }
  MHD_socket_close_chk_ (sp[1]);
  (void) close (0);
  (void) close (1);
  dup2 (sp[0], 0);
  dup2 (sp[0], 1);
  MHD_socket_close_chk_ (sp[0]);
  if (TLS_CLI_GNUTLS == use_tls_tool)
    {
      snprintf (destination,
                sizeof(destination),
                "%u",
                (unsigned int) port);
      execlp ("gnutls-cli",
              "gnutls-cli",
              "--insecure",
              "-p",
              destination,
              "127.0.0.1",
              (char *) NULL);
    }
  else if (TLS_CLI_OPENSSL == use_tls_tool)
    {
      snprintf (destination,
                sizeof(destination),
                "127.0.0.1:%u",
                (unsigned int) port);
      execlp ("openssl",
              "openssl",
              "s_client",
              "-connect",
              destination,
              "-verify",
              "1",
              (char *) NULL);
    }
  _exit (1);
}
#endif /* HTTPS_SUPPORT && HAVE_FORK && HAVE_WAITPID */


/**
 * Wrapper structure for plain&TLS sockets
 */
struct wr_socket_strc
{
  /**
   * Real network socket
   */
  MHD_socket fd;

  /**
   * Type of this socket
   */
  enum wr_type
  {
    wr_invalid = 0,
    wr_plain = 1,
    wr_tls = 2
  } t;
#ifdef HTTPS_SUPPORT
  /**
   * TLS credentials
   */
  gnutls_certificate_credentials_t tls_crd;

  /**
   * TLS session.
   */
  gnutls_session_t tls_s;

  /**
   * TLS handshake already succeed?
   */
  bool tls_connected;
#endif
};


/**
 * Pseudo type for plain&TLS sockets
 */
typedef struct wr_socket_strc* wr_socket;


/**
 * Invalid value of wr_socket
 */
#define WR_BAD (NULL)


/**
 * Get underlying real socket.
 * @return FD of real socket
 */
#define wr_fd(s) ((s)->fd)


/**
 * Create wr_socket with plain TCP underlying socket
 * @return created socket on success, WR_BAD otherwise
 */
static wr_socket wr_create_plain_sckt(void)
{
  wr_socket s = (wr_socket)malloc(sizeof(struct wr_socket_strc));
  if (WR_BAD == s)
    return WR_BAD;
  s->t = wr_plain;
  s->fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (MHD_INVALID_SOCKET != s->fd)
    return s;
  free(s);
  return WR_BAD;
}


/**
 * Create wr_socket with TLS TCP underlying socket
 * @return created socket on success, WR_BAD otherwise
 */
static wr_socket wr_create_tls_sckt(void)
{
#ifdef HTTPS_SUPPORT
  wr_socket s = (wr_socket)malloc(sizeof(struct wr_socket_strc));
  if (WR_BAD == s)
    return WR_BAD;
  s->t = wr_tls;
  s->tls_connected = 0;
  s->fd = socket (AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (MHD_INVALID_SOCKET != s->fd)
    {
      if (GNUTLS_E_SUCCESS == gnutls_init (&(s->tls_s), GNUTLS_CLIENT))
        {
          if (GNUTLS_E_SUCCESS == gnutls_set_default_priority (s->tls_s))
            {
              if (GNUTLS_E_SUCCESS == gnutls_certificate_allocate_credentials (&(s->tls_crd)))
                {
                  if (GNUTLS_E_SUCCESS == gnutls_credentials_set (s->tls_s, GNUTLS_CRD_CERTIFICATE, s->tls_crd))
                    {
#if GNUTLS_VERSION_NUMBER+0 >= 0x030109
                      gnutls_transport_set_int (s->tls_s, (int)(s->fd));
#else  /* GnuTLS before 3.1.9 */
                      gnutls_transport_set_ptr (s->tls_s, (gnutls_transport_ptr_t)(intptr_t)(s->fd));
#endif /* GnuTLS before 3.1.9 */
                      return s;
                    }
                  gnutls_certificate_free_credentials (s->tls_crd);
                }
            }
          gnutls_deinit (s->tls_s);
        }
      (void)MHD_socket_close_ (s->fd);
    }
  free(s);
#endif /* HTTPS_SUPPORT */
  return WR_BAD;
}


/**
 * Create wr_socket with plain TCP underlying socket
 * from already created TCP socket.
 * @param plain_sk real TCP socket
 * @return created socket on success, WR_BAD otherwise
 */
static wr_socket wr_create_from_plain_sckt(MHD_socket plain_sk)
{
  wr_socket s = (wr_socket)malloc(sizeof(struct wr_socket_strc));
  if (WR_BAD == s)
    return WR_BAD;
  s->t = wr_plain;
  s->fd = plain_sk;
  return s;
}


/**
 * Connect socket to specified address.
 * @param s socket to use
 * @param addr address to connect
 * @param length of sturcture pointed by @a addr
 * @return zero on success, -1 otherwise.
 */
static int wr_connect(wr_socket s, struct sockaddr * addr, int length)
{
  if (0 != connect(s->fd, addr, length))
    return -1;
  if (wr_plain == s->t)
    return 0;
#ifdef HTTPS_SUPPORT
  if (wr_tls == s->t)
    {
      s->tls_connected = 0;
      return 0;
      /* Do not try handshake here as
       * it require processing on MHD side and
       * when testing with "external" polling,
       * test will call MHD processing only
       * after return from wr_connect(). */
      /*
      int res = gnutls_handshake (s->tls_s);
      if (GNUTLS_E_SUCCESS == res)
        {
          s->tls_connected = !0;
          return 0;
        }
      if (GNUTLS_E_AGAIN == res)
        return 0;
       */
    }
#endif /* HTTPS_SUPPORT */
  return -1;
}

#ifdef HTTPS_SUPPORT
/* Only to be called from wr_send() and wr_recv() ! */
static bool wr_handshake(wr_socket s)
{
  int res = gnutls_handshake (s->tls_s);
  if (GNUTLS_E_SUCCESS == res)
    s->tls_connected = !0;
  else if (GNUTLS_E_AGAIN == res)
    MHD_socket_set_error_ (MHD_SCKT_EAGAIN_);
  else
    MHD_socket_set_error_ (MHD_SCKT_ECONNABORTED_); /* hard error */
  return s->tls_connected;
}
#endif /* HTTPS_SUPPORT */


/**
 * Send data to remote by socket.
 * @param s the socket to use
 * @param buf the buffer with data to send
 * @param len the length of data in @a buf
 * @return number of bytes were sent if succeed,
 *         -1 if failed. Use #MHD_socket_get_error_()
 *         to get socket error.
 */
static ssize_t wr_send(wr_socket s, const void *buf, size_t len)
{
  if (wr_plain == s->t)
    return MHD_send_(s->fd, buf, len);
#ifdef HTTPS_SUPPORT
  if (wr_tls == s->t)
    {
      ssize_t ret;
      if (!s->tls_connected && !wr_handshake (s))
        return -1;

      ret = gnutls_record_send (s->tls_s, buf, len);
      if (ret > 0)
        return ret;
      if (GNUTLS_E_AGAIN == ret)
        MHD_socket_set_error_ (MHD_SCKT_EAGAIN_);
      else
        MHD_socket_set_error_ (MHD_SCKT_ECONNABORTED_); /* hard error */
    }
#endif /* HTTPS_SUPPORT */
  return -1;
}


/**
 * Receive data from remote by socket.
 * @param s the socket to use
 * @param buf the buffer to store received data
 * @param len the length of @a buf
 * @return number of bytes were received if succeed,
 *         -1 if failed. Use #MHD_socket_get_error_()
 *         to get socket error.
 */
static ssize_t wr_recv(wr_socket s, void *buf, size_t len)
{
  if (wr_plain == s->t)
    return MHD_recv_ (s->fd, buf, len);
#ifdef HTTPS_SUPPORT
  if (wr_tls == s->t)
    {
      ssize_t ret;
      if (!s->tls_connected && !wr_handshake (s))
        return -1;

      ret = gnutls_record_recv (s->tls_s, buf, len);
      if (ret > 0)
        return ret;
      if (GNUTLS_E_AGAIN == ret)
        MHD_socket_set_error_ (MHD_SCKT_EAGAIN_);
      else
        MHD_socket_set_error_ (MHD_SCKT_ECONNABORTED_); /* hard error */
    }
#endif /* HTTPS_SUPPORT */
  return -1;
}


/**
 * Close socket and release allocated resourced
 * @param s the socket to close
 * @return zero on succeed, -1 otherwise
 */
static int
wr_close(wr_socket s)
{
  int ret = (MHD_socket_close_(s->fd)) ? 0 : -1;
#ifdef HTTPS_SUPPORT
  if (wr_tls == s->t)
    {
      gnutls_deinit (s->tls_s);
      gnutls_certificate_free_credentials (s->tls_crd);
    }
#endif /* HTTPS_SUPPORT */
  free(s);
  return ret;
}


/**
 * Thread we use to run the interaction with the upgraded socket.
 */
static pthread_t pt;

/**
 * Will be set to the upgraded socket.
 */
static wr_socket usock;

/**
 * Thread we use to run the interaction with the upgraded socket.
 */
static pthread_t pt_client;

/**
 * Flag set to 1 once the test is finished.
 */
static volatile bool done;


static void
notify_completed_cb (void *cls,
                     struct MHD_Connection *connection,
                     void **con_cls,
                     enum MHD_RequestTerminationCode toe)
{
  if ( (toe != MHD_REQUEST_TERMINATED_COMPLETED_OK) &&
       (toe != MHD_REQUEST_TERMINATED_CLIENT_ABORT) &&
       (toe != MHD_REQUEST_TERMINATED_DAEMON_SHUTDOWN) )
    abort ();
  if (! pthread_equal (**((pthread_t**)con_cls), pthread_self ()))
    abort ();
  free (*con_cls);
  *con_cls = NULL;
}


/**
 * Logging callback.
 *
 * @param cls logging closure (NULL)
 * @param uri access URI
 * @param connection connection handle
 * @return #TEST_PTR
 */
static void *
log_cb (void *cls,
        const char *uri,
        struct MHD_Connection *connection)
{
  pthread_t* ppth;
  if (0 != strcmp (uri,
                   "/"))
    abort ();
  ppth = (pthread_t*) malloc (sizeof(pthread_t));
  if (NULL == ppth)
    abort();
  *ppth = pthread_self ();
  return (void *) ppth;
}


/**
 * Function to check that MHD properly notifies about starting
 * and stopping.
 *
 * @param cls client-defined closure
 * @param connection connection handle
 * @param socket_context socket-specific pointer where the
 *                       client can associate some state specific
 *                       to the TCP connection; note that this is
 *                       different from the "con_cls" which is per
 *                       HTTP request.  The client can initialize
 *                       during #MHD_CONNECTION_NOTIFY_STARTED and
 *                       cleanup during #MHD_CONNECTION_NOTIFY_CLOSED
 *                       and access in the meantime using
 *                       #MHD_CONNECTION_INFO_SOCKET_CONTEXT.
 * @param toe reason for connection notification
 * @see #MHD_OPTION_NOTIFY_CONNECTION
 * @ingroup request
 */
static void
notify_connection_cb (void *cls,
                      struct MHD_Connection *connection,
                      void **socket_context,
                      enum MHD_ConnectionNotificationCode toe)
{
  static int started;

  switch (toe)
  {
  case MHD_CONNECTION_NOTIFY_STARTED:
    if (MHD_NO != started)
      abort ();
    started = MHD_YES;
    *socket_context = &started;
    break;
  case MHD_CONNECTION_NOTIFY_CLOSED:
    if (MHD_YES != started)
      abort ();
    if (&started != *socket_context)
      abort ();
    *socket_context = NULL;
    started = MHD_NO;
    break;
  }
}


/**
 * Change socket to blocking.
 *
 * @param fd the socket to manipulate
 * @return non-zero if succeeded, zero otherwise
 */
static void
make_blocking (MHD_socket fd)
{
#if defined(MHD_POSIX_SOCKETS)
  int flags;

  flags = fcntl (fd, F_GETFL);
  if (-1 == flags)
    return;
  if ((flags & ~O_NONBLOCK) != flags)
    if (-1 == fcntl (fd, F_SETFL, flags & ~O_NONBLOCK))
      abort ();
#elif defined(MHD_WINSOCK_SOCKETS)
  unsigned long flags = 1;

  ioctlsocket (fd, FIONBIO, &flags);
#endif /* MHD_WINSOCK_SOCKETS */

}


static void
send_all (wr_socket sock,
          const char *text)
{
  size_t len = strlen (text);
  ssize_t ret;

  make_blocking (wr_fd (sock));
  for (size_t off = 0; off < len; off += ret)
    {
      ret = wr_send (sock,
                       &text[off],
                       len - off);
      if (0 > ret)
        {
          if (MHD_SCKT_ERR_IS_EAGAIN_ (MHD_socket_get_error_ ()))
            {
              ret = 0;
              continue;
            }
          abort ();
        }
    }
}


/**
 * Read character-by-character until we
 * get '\r\n\r\n'.
 */
static void
recv_hdr (wr_socket sock)
{
  unsigned int i;
  char next;
  char c;
  ssize_t ret;

  make_blocking (wr_fd (sock));
  next = '\r';
  i = 0;
  while (i < 4)
    {
      ret = wr_recv (sock,
                       &c,
                       1);
      if (0 > ret)
        {
          if (MHD_SCKT_ERR_IS_EAGAIN_ (MHD_socket_get_error_ ()))
            continue;
          abort ();
        }
      if (0 == ret)
        continue;
      if (c == next)
        {
          i++;
          if (next == '\r')
            next = '\n';
          else
            next = '\r';
          continue;
        }
      if (c == '\r')
        {
          i = 1;
          next = '\n';
          continue;
        }
      i = 0;
      next = '\r';
    }
}


static void
recv_all (wr_socket sock,
          const char *text)
{
  size_t len = strlen (text);
  char buf[len];
  ssize_t ret;

  make_blocking (wr_fd (sock));
  for (size_t off = 0; off < len; off += ret)
    {
      ret = wr_recv (sock,
                       &buf[off],
                       len - off);
      if (0 > ret)
        {
          if (MHD_SCKT_ERR_IS_EAGAIN_ (MHD_socket_get_error_ ()))
            {
              ret = 0;
              continue;
            }
          abort ();
        }
    }
  if (0 != strncmp (text, buf, len))
    abort();
}


/**
 * Main function for the thread that runs the interaction with
 * the upgraded socket.
 *
 * @param cls the handle for the upgrade
 */
static void *
run_usock (void *cls)
{
  struct MHD_UpgradeResponseHandle *urh = cls;

  send_all (usock,
            "Hello");
  recv_all (usock,
            "World");
  send_all (usock,
            "Finished");
  MHD_upgrade_action (urh,
                      MHD_UPGRADE_ACTION_CLOSE);
  return NULL;
}


/**
 * Main function for the thread that runs the client-side of the
 * interaction with the upgraded socket.
 *
 * @param cls the client socket
 */
static void *
run_usock_client (void *cls)
{
  wr_socket *sock = cls;

  send_all (*sock,
            "GET / HTTP/1.1\r\nConnection: Upgrade\r\n\r\n");
  recv_hdr (*sock);
  recv_all (*sock,
            "Hello");
  send_all (*sock,
            "World");
  recv_all (*sock,
            "Finished");
  wr_close (*sock);
  done = true;
  return NULL;
}


/**
 * Function called after a protocol "upgrade" response was sent
 * successfully and the socket should now be controlled by some
 * protocol other than HTTP.
 *
 * Any data already received on the socket will be made available in
 * @e extra_in.  This can happen if the application sent extra data
 * before MHD send the upgrade response.  The application should
 * treat data from @a extra_in as if it had read it from the socket.
 *
 * Note that the application must not close() @a sock directly,
 * but instead use #MHD_upgrade_action() for special operations
 * on @a sock.
 *
 * Except when in 'thread-per-connection' mode, implementations
 * of this function should never block (as it will still be called
 * from within the main event loop).
 *
 * @param cls closure, whatever was given to #MHD_create_response_for_upgrade().
 * @param connection original HTTP connection handle,
 *                   giving the function a last chance
 *                   to inspect the original HTTP request
 * @param con_cls last value left in `con_cls` of the `MHD_AccessHandlerCallback`
 * @param extra_in if we happened to have read bytes after the
 *                 HTTP header already (because the client sent
 *                 more than the HTTP header of the request before
 *                 we sent the upgrade response),
 *                 these are the extra bytes already read from @a sock
 *                 by MHD.  The application should treat these as if
 *                 it had read them from @a sock.
 * @param extra_in_size number of bytes in @a extra_in
 * @param sock socket to use for bi-directional communication
 *        with the client.  For HTTPS, this may not be a socket
 *        that is directly connected to the client and thus certain
 *        operations (TCP-specific setsockopt(), getsockopt(), etc.)
 *        may not work as expected (as the socket could be from a
 *        socketpair() or a TCP-loopback).  The application is expected
 *        to perform read()/recv() and write()/send() calls on the socket.
 *        The application may also call shutdown(), but must not call
 *        close() directly.
 * @param urh argument for #MHD_upgrade_action()s on this @a connection.
 *        Applications must eventually use this callback to (indirectly)
 *        perform the close() action on the @a sock.
 */
static void
upgrade_cb (void *cls,
            struct MHD_Connection *connection,
            void *con_cls,
            const char *extra_in,
            size_t extra_in_size,
            MHD_socket sock,
            struct MHD_UpgradeResponseHandle *urh)
{
  usock = wr_create_from_plain_sckt (sock);
  if (0 != extra_in_size)
    abort ();
  if (0 != pthread_create (&pt,
                           NULL,
                           &run_usock,
                           urh))
    abort ();
}


/**
 * A client has requested the given url using the given method
 * (#MHD_HTTP_METHOD_GET, #MHD_HTTP_METHOD_PUT,
 * #MHD_HTTP_METHOD_DELETE, #MHD_HTTP_METHOD_POST, etc).  The callback
 * must call MHD callbacks to provide content to give back to the
 * client and return an HTTP status code (i.e. #MHD_HTTP_OK,
 * #MHD_HTTP_NOT_FOUND, etc.).
 *
 * @param cls argument given together with the function
 *        pointer when the handler was registered with MHD
 * @param url the requested url
 * @param method the HTTP method used (#MHD_HTTP_METHOD_GET,
 *        #MHD_HTTP_METHOD_PUT, etc.)
 * @param version the HTTP version string (i.e.
 *        #MHD_HTTP_VERSION_1_1)
 * @param upload_data the data being uploaded (excluding HEADERS,
 *        for a POST that fits into memory and that is encoded
 *        with a supported encoding, the POST data will NOT be
 *        given in upload_data and is instead available as
 *        part of #MHD_get_connection_values; very large POST
 *        data *will* be made available incrementally in
 *        @a upload_data)
 * @param upload_data_size set initially to the size of the
 *        @a upload_data provided; the method must update this
 *        value to the number of bytes NOT processed;
 * @param con_cls pointer that the callback can set to some
 *        address and that will be preserved by MHD for future
 *        calls for this request; since the access handler may
 *        be called many times (i.e., for a PUT/POST operation
 *        with plenty of upload data) this allows the application
 *        to easily associate some request-specific state.
 *        If necessary, this state can be cleaned up in the
 *        global #MHD_RequestCompletedCallback (which
 *        can be set with the #MHD_OPTION_NOTIFY_COMPLETED).
 *        Initially, `*con_cls` will be NULL.
 * @return #MHD_YES if the connection was handled successfully,
 *         #MHD_NO if the socket must be closed due to a serios
 *         error while handling the request
 */
static int
ahc_upgrade (void *cls,
             struct MHD_Connection *connection,
             const char *url,
             const char *method,
             const char *version,
             const char *upload_data,
             size_t *upload_data_size,
             void **con_cls)
{
  struct MHD_Response *resp;
  int ret;

  if (NULL == *con_cls)
    abort ();
  if (! pthread_equal (**((pthread_t**)con_cls), pthread_self ()))
    abort ();
  resp = MHD_create_response_for_upgrade (&upgrade_cb,
                                          NULL);
  MHD_add_response_header (resp,
                           MHD_HTTP_HEADER_UPGRADE,
                           "Hello World Protocol");
  ret = MHD_queue_response (connection,
                            MHD_HTTP_SWITCHING_PROTOCOLS,
                            resp);
  MHD_destroy_response (resp);
  return ret;
}


/**
 * Run the MHD external event loop using select.
 *
 * @param daemon daemon to run it for
 */
static void
run_mhd_select_loop (struct MHD_Daemon *daemon)
{
  fd_set rs;
  fd_set ws;
  fd_set es;
  MHD_socket max_fd;
  MHD_UNSIGNED_LONG_LONG to;
  struct timeval tv;

  while (! done)
    {
      FD_ZERO (&rs);
      FD_ZERO (&ws);
      FD_ZERO (&es);
      max_fd = -1;
      to = 1000;

      if (MHD_YES !=
          MHD_get_fdset (daemon,
                         &rs,
                         &ws,
                         &es,
                         &max_fd))
        abort ();
      (void) MHD_get_timeout (daemon,
                              &to);
      if (1000 < to)
        to = 1000;
      tv.tv_sec = to / 1000;
      tv.tv_usec = 1000 * (to % 1000);
      if (0 > MHD_SYS_select_ (max_fd + 1,
                               &rs,
                               &ws,
                               &es,
                               &tv))
        abort ();
      MHD_run_from_select (daemon,
                           &rs,
                           &ws,
                           &es);
    }
}

#ifdef HAVE_POLL

/**
 * Run the MHD external event loop using select.
 *
 * @param daemon daemon to run it for
 */
static void
run_mhd_poll_loop (struct MHD_Daemon *daemon)
{
  abort (); /* currently not implementable with existing MHD API */
}
#endif /* HAVE_POLL */


#ifdef EPOLL_SUPPORT
/**
 * Run the MHD external event loop using select.
 *
 * @param daemon daemon to run it for
 */
static void
run_mhd_epoll_loop (struct MHD_Daemon *daemon)
{
  const union MHD_DaemonInfo *di;
  MHD_socket ep;
  fd_set rs;
  MHD_UNSIGNED_LONG_LONG to;
  struct timeval tv;

  di = MHD_get_daemon_info (daemon,
                            MHD_DAEMON_INFO_EPOLL_FD);
  ep = di->listen_fd;
  while (! done)
    {
      FD_ZERO (&rs);
      to = 1000;

      FD_SET (ep, &rs);
      (void) MHD_get_timeout (daemon,
                              &to);
      if (1000 < to)
        to = 1000;
      tv.tv_sec = to / 1000;
      tv.tv_usec = 1000 * (to % 1000);
      select (ep + 1,
              &rs,
              NULL,
              NULL,
              &tv);
      MHD_run (daemon);
    }
}
#endif /* EPOLL_SUPPORT */

/**
 * Run the MHD external event loop using select.
 *
 * @param daemon daemon to run it for
 */
static void
run_mhd_loop (struct MHD_Daemon *daemon,
              int flags)
{
  if (0 == (flags & (MHD_USE_POLL | MHD_USE_EPOLL)))
    run_mhd_select_loop (daemon);
#ifdef HAVE_POLL
  else if (0 != (flags & MHD_USE_POLL))
    run_mhd_poll_loop (daemon);
#endif /* HAVE_POLL */
#if EPOLL_SUPPORT
  else if (0 != (flags & MHD_USE_EPOLL))
    run_mhd_epoll_loop (daemon);
#endif
  else
    abort ();
}

static bool test_tls;

/**
 * Test upgrading a connection.
 *
 * @param flags which event loop style should be tested
 * @param pool size of the thread pool, 0 to disable
 */
static int
test_upgrade (int flags,
              unsigned int pool)
{
  struct MHD_Daemon *d = NULL;
  wr_socket sock;
  struct sockaddr_in sa;
  const union MHD_DaemonInfo *real_flags;
#if defined(HTTPS_SUPPORT) && defined(HAVE_FORK) && defined(HAVE_WAITPID)
  pid_t pid = -1;
#endif /* HTTPS_SUPPORT && HAVE_FORK && HAVE_WAITPID */

  done = false;

  if (!test_tls)
    d = MHD_start_daemon (flags | MHD_USE_ERROR_LOG | MHD_ALLOW_UPGRADE,
			  1080,
			  NULL, NULL,
			  &ahc_upgrade, NULL,
			  MHD_OPTION_URI_LOG_CALLBACK, &log_cb, NULL,
			  MHD_OPTION_NOTIFY_COMPLETED, &notify_completed_cb, NULL,
			  MHD_OPTION_NOTIFY_CONNECTION, &notify_connection_cb, NULL,
			  MHD_OPTION_THREAD_POOL_SIZE, pool,
			  MHD_OPTION_END);
#ifdef HTTPS_SUPPORT
  else
    d = MHD_start_daemon (flags | MHD_USE_ERROR_LOG | MHD_ALLOW_UPGRADE | MHD_USE_TLS,
                          1080,
                          NULL, NULL,
                          &ahc_upgrade, NULL,
                          MHD_OPTION_URI_LOG_CALLBACK, &log_cb, NULL,
                          MHD_OPTION_NOTIFY_COMPLETED, &notify_completed_cb, NULL,
                          MHD_OPTION_NOTIFY_CONNECTION, &notify_connection_cb, NULL,
                          MHD_OPTION_HTTPS_MEM_KEY, srv_signed_key_pem,
                          MHD_OPTION_HTTPS_MEM_CERT, srv_signed_cert_pem,
                          MHD_OPTION_THREAD_POOL_SIZE, pool,
                          MHD_OPTION_END);
#endif /* HTTPS_SUPPORT */
  if (NULL == d)
    return 2;
  real_flags = MHD_get_daemon_info(d, MHD_DAEMON_INFO_FLAGS);
  if (NULL == real_flags)
    abort ();
  if (!test_tls || TLS_LIB_GNUTLS == use_tls_tool)
    {
      sock = test_tls ? wr_create_tls_sckt () : wr_create_plain_sckt ();
      if (WR_BAD == sock)
        abort ();
      sa.sin_family = AF_INET;
      sa.sin_port = htons (1080);
      sa.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
      if (0 != wr_connect (sock,
                        (struct sockaddr *) &sa,
                        sizeof (sa)))
        abort ();
    }
  else
    {
#if defined(HTTPS_SUPPORT) && defined(HAVE_FORK) && defined(HAVE_WAITPID)
      MHD_socket tls_fork_sock;
      if (-1 == (pid = gnutlscli_connect (&tls_fork_sock, 1080)))
        {
          MHD_stop_daemon (d);
          return 4;
        }
      sock =  wr_create_from_plain_sckt (tls_fork_sock);
      if (WR_BAD == sock)
        abort ();
#else  /* !HTTPS_SUPPORT || !HAVE_FORK || !HAVE_WAITPID */
      abort ();
#endif /* !HTTPS_SUPPORT || !HAVE_FORK || !HAVE_WAITPID */
    }

  if (0 != pthread_create (&pt_client,
                           NULL,
                           &run_usock_client,
                           &sock))
    abort ();
  if (0 == (flags & MHD_USE_INTERNAL_POLLING_THREAD) )
    run_mhd_loop (d, real_flags->flags);
  pthread_join (pt_client,
                NULL);
  pthread_join (pt,
                NULL);
#if defined(HTTPS_SUPPORT) && defined(HAVE_FORK) && defined(HAVE_WAITPID)
  if (test_tls && TLS_LIB_GNUTLS != use_tls_tool)
    waitpid (pid, NULL, 0);
#endif /* HTTPS_SUPPORT && HAVE_FORK && HAVE_WAITPID */
  MHD_stop_daemon (d);
  return 0;
}


int
main (int argc,
      char *const *argv)
{
  int error_count = 0;
  int res;

  use_tls_tool = TLS_CLI_NO_TOOL;
  test_tls = has_in_name(argv[0], "_tls");

  if (has_param(argc, argv, "-v") || has_param(argc, argv, "--verbose"))
    verbose = 1;

  if (test_tls)
    {
#ifdef HTTPS_SUPPORT
      if (has_param(argc, argv, "--use-gnutls-cli"))
        use_tls_tool = TLS_CLI_GNUTLS;
      else if (has_param(argc, argv, "--use-openssl"))
        use_tls_tool = TLS_CLI_OPENSSL;
      else if (has_param(argc, argv, "--use-gnutls-lib"))
        use_tls_tool = TLS_LIB_GNUTLS;
#if defined(HAVE_FORK) && defined(HAVE_WAITPID)
      else if (0 == system ("gnutls-cli --version 1> /dev/null"))
        use_tls_tool = TLS_CLI_GNUTLS;
      else if (0 == system ("openssl version 1> /dev/null"))
        use_tls_tool = TLS_CLI_OPENSSL;
#endif /* HAVE_FORK && HAVE_WAITPID */
      else
        use_tls_tool = TLS_LIB_GNUTLS; /* Should be available as MHD use it. */
      if (verbose)
        {
          switch (use_tls_tool)
          {
            case TLS_CLI_GNUTLS:
              printf ("GnuTLS-CLI will be used for testing.\n");
              break;
            case TLS_CLI_OPENSSL:
              printf ("Command line version of OpenSSL will be used for testing.\n");
              break;
            case TLS_LIB_GNUTLS:
              printf ("GnuTLS library will be used for testing.\n");
              break;
            default:
              abort ();
          }
        }
      if ( (TLS_LIB_GNUTLS == use_tls_tool) &&
           (GNUTLS_E_SUCCESS != gnutls_global_init()) )
        abort ();

#else  /* ! HTTPS_SUPPORT */
      fprintf (stderr, "HTTPS support was disabled by configure.\n");
      return 77;
#endif /* ! HTTPS_SUPPORT */
    }

  /* run tests */
  if (verbose)
    printf ("Starting HTTP \"Upgrade\" tests with %s connections.\n", test_tls ? "TLS" : "plain");
  /* try external select */
  res = test_upgrade (0,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with external select, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with external select.\n");

  /* Try external auto */
  res = test_upgrade (MHD_USE_AUTO,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with external 'auto', return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with external 'auto'.\n");

#ifdef EPOLL_SUPPORT
  res = test_upgrade (MHD_USE_EPOLL,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with external select with EPOLL, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with external select with EPOLL.\n");
#endif

  /* Test thread-per-connection */
  res = test_upgrade (MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with thread per connection, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with thread per connection.\n");

  res = test_upgrade (MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with thread per connection and 'auto', return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with thread per connection and 'auto'.\n");
#ifdef HAVE_POLL
  res = test_upgrade (MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION | MHD_USE_POLL,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with thread per connection and poll, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with thread per connection and poll.\n");
#endif /* HAVE_POLL */

  /* Test different event loops, with and without thread pool */
  res = test_upgrade (MHD_USE_INTERNAL_POLLING_THREAD,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with internal select, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with internal select.\n");
  res = test_upgrade (MHD_USE_INTERNAL_POLLING_THREAD,
                      2);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with internal select with thread pool, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with internal select with thread pool.\n");
  res = test_upgrade (MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with internal 'auto' return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with internal 'auto'.\n");
  res = test_upgrade (MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
                      2);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with internal 'auto' with thread pool, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with internal 'auto' with thread pool.\n");
#ifdef HAVE_POLL
  res = test_upgrade (MHD_USE_POLL_INTERNAL_THREAD,
                      0);
  error_count += res;
  if (res)
    fprintf (stderr, "FAILED: Upgrade with internal poll, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with internal poll.\n");
  res = test_upgrade (MHD_USE_POLL_INTERNAL_THREAD,
                      2);
  if (res)
    fprintf (stderr, "FAILED: Upgrade with internal poll with thread pool, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with internal poll with thread pool.\n");
#endif
#ifdef EPOLL_SUPPORT
  res = test_upgrade (MHD_USE_EPOLL_INTERNAL_THREAD,
                      0);
  if (res)
    fprintf (stderr, "FAILED: Upgrade with internal epoll, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with internal epoll.\n");
  res = test_upgrade (MHD_USE_EPOLL_INTERNAL_THREAD,
                      2);
  if (res)
    fprintf (stderr, "FAILED: Upgrade with internal epoll, return code %d.\n", res);
  else if (verbose)
    printf ("PASSED: Upgrade with internal epoll.\n");
#endif
  /* report result */
  if (0 != error_count)
    fprintf (stderr,
             "Error (code: %u)\n",
             error_count);
#ifdef HTTPS_SUPPORT
  if (test_tls && (TLS_LIB_GNUTLS == use_tls_tool))
    gnutls_global_deinit();
#endif /* HTTPS_SUPPORT */
  return error_count != 0;       /* 0 == pass */
}
