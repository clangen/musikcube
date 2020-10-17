/*
     This file is part of libmicrohttpd
     Copyright (C) 2007, 2009, 2010, 2016, 2017 Daniel Pittman and Christian Grothoff

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
 * @file response.c
 * @brief  Methods for managing response objects
 * @author Daniel Pittman
 * @author Christian Grothoff
 * @author Karlson2k (Evgeny Grin)
 */

#define MHD_NO_DEPRECATION 1

#include "mhd_options.h"
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */
#ifdef _WIN32
#include <windows.h>
#endif /* _WIN32 */

#include "internal.h"
#include "response.h"
#include "mhd_limits.h"
#include "mhd_sockets.h"
#include "mhd_itc.h"
#include "connection.h"
#include "memorypool.h"
#include "mhd_compat.h"


#if defined(_WIN32) && defined(MHD_W32_MUTEX_)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif /* !WIN32_LEAN_AND_MEAN */
#include <windows.h>
#endif /* _WIN32 && MHD_W32_MUTEX_ */
#if defined(_WIN32)
#include <io.h> /* for lseek(), read() */
#endif /* _WIN32 */


/**
 * Add a header or footer line to the response.
 *
 * @param response response to add a header to
 * @param kind header or footer
 * @param header the header to add
 * @param content value to add
 * @return #MHD_NO on error (i.e. invalid header or content format).
 */
static int
add_response_entry (struct MHD_Response *response,
		    enum MHD_ValueKind kind,
		    const char *header,
		    const char *content)
{
  struct MHD_HTTP_Header *hdr;

  if ( (NULL == response) ||
       (NULL == header) ||
       (NULL == content) ||
       (0 == header[0]) ||
       (0 == content[0]) ||
       (NULL != strchr (header, '\t')) ||
       (NULL != strchr (header, '\r')) ||
       (NULL != strchr (header, '\n')) ||
       (NULL != strchr (content, '\t')) ||
       (NULL != strchr (content, '\r')) ||
       (NULL != strchr (content, '\n')) )
    return MHD_NO;
  if (NULL == (hdr = malloc (sizeof (struct MHD_HTTP_Header))))
    return MHD_NO;
  if (NULL == (hdr->header = strdup (header)))
    {
      free (hdr);
      return MHD_NO;
    }
  if (NULL == (hdr->value = strdup (content)))
    {
      free (hdr->header);
      free (hdr);
      return MHD_NO;
    }
  hdr->kind = kind;
  hdr->next = response->first_header;
  response->first_header = hdr;
  return MHD_YES;
}


/**
 * Add a header line to the response.
 *
 * @param response response to add a header to
 * @param header the header to add
 * @param content value to add
 * @return #MHD_NO on error (i.e. invalid header or content format).
 * @ingroup response
 */
int
MHD_add_response_header (struct MHD_Response *response,
                         const char *header,
                         const char *content)
{
  return add_response_entry (response,
			     MHD_HEADER_KIND,
			     header,
			     content);
}


/**
 * Add a footer line to the response.
 *
 * @param response response to remove a header from
 * @param footer the footer to delete
 * @param content value to delete
 * @return #MHD_NO on error (i.e. invalid footer or content format).
 * @ingroup response
 */
int
MHD_add_response_footer (struct MHD_Response *response,
                         const char *footer,
                         const char *content)
{
  return add_response_entry (response,
			     MHD_FOOTER_KIND,
			     footer,
			     content);
}


/**
 * Delete a header (or footer) line from the response.
 *
 * @param response response to remove a header from
 * @param header the header to delete
 * @param content value to delete
 * @return #MHD_NO on error (no such header known)
 * @ingroup response
 */
int
MHD_del_response_header (struct MHD_Response *response,
                         const char *header,
			 const char *content)
{
  struct MHD_HTTP_Header *pos;
  struct MHD_HTTP_Header *prev;

  if ( (NULL == header) ||
  (NULL == content) )
    return MHD_NO;
  prev = NULL;
  pos = response->first_header;
  while (pos != NULL)
    {
      if ((0 == strcmp (header,
                        pos->header)) &&
          (0 == strcmp (content,
                        pos->value)))
        {
          free (pos->header);
          free (pos->value);
          if (NULL == prev)
            response->first_header = pos->next;
          else
            prev->next = pos->next;
          free (pos);
          return MHD_YES;
        }
      prev = pos;
      pos = pos->next;
    }
  return MHD_NO;
}


/**
 * Get all of the headers (and footers) added to a response.
 *
 * @param response response to query
 * @param iterator callback to call on each header;
 *        maybe NULL (then just count headers)
 * @param iterator_cls extra argument to @a iterator
 * @return number of entries iterated over
 * @ingroup response
 */
int
MHD_get_response_headers (struct MHD_Response *response,
                          MHD_KeyValueIterator iterator,
                          void *iterator_cls)
{
  struct MHD_HTTP_Header *pos;
  int numHeaders = 0;

  for (pos = response->first_header; NULL != pos; pos = pos->next)
    {
      numHeaders++;
      if ((NULL != iterator) &&
          (MHD_YES != iterator (iterator_cls,
                                pos->kind,
                                pos->header,
                                pos->value)))
        break;
    }
  return numHeaders;
}


/**
 * Get a particular header (or footer) from the response.
 *
 * @param response response to query
 * @param key which header to get
 * @return NULL if header does not exist
 * @ingroup response
 */
const char *
MHD_get_response_header (struct MHD_Response *response,
			 const char *key)
{
  struct MHD_HTTP_Header *pos;

  if (NULL == key)
    return NULL;
  for (pos = response->first_header; NULL != pos; pos = pos->next)
    if (0 == strcmp (key,
                     pos->header))
      return pos->value;
  return NULL;
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response, #MHD_SIZE_UNKNOWN for unknown
 * @param block_size preferred block size for querying crc (advisory only,
 *                   MHD may still call @a crc using smaller chunks); this
 *                   is essentially the buffer size used for IO, clients
 *                   should pick a value that is appropriate for IO and
 *                   memory performance requirements
 * @param crc callback to use to obtain response data
 * @param crc_cls extra argument to @a crc
 * @param crfc callback to call to free @a crc_cls resources
 * @return NULL on error (i.e. invalid arguments, out of memory)
 * @ingroup response
 */
struct MHD_Response *
MHD_create_response_from_callback (uint64_t size,
                                   size_t block_size,
                                   MHD_ContentReaderCallback crc,
                                   void *crc_cls,
                                   MHD_ContentReaderFreeCallback crfc)
{
  struct MHD_Response *response;

  if ((NULL == crc) || (0 == block_size))
    return NULL;
  if (NULL == (response = MHD_calloc_ (1, sizeof (struct MHD_Response) + block_size)))
    return NULL;
  response->fd = -1;
  response->data = (void *) &response[1];
  response->data_buffer_size = block_size;
  if (! MHD_mutex_init_ (&response->mutex))
    {
      free (response);
      return NULL;
    }
  response->crc = crc;
  response->crfc = crfc;
  response->crc_cls = crc_cls;
  response->reference_count = 1;
  response->total_size = size;
  return response;
}


/**
 * Set special flags and options for a response.
 *
 * @param response the response to modify
 * @param flags to set for the response
 * @param ... #MHD_RO_END terminated list of options
 * @return #MHD_YES on success, #MHD_NO on error
 */
int
MHD_set_response_options (struct MHD_Response *response,
                          enum MHD_ResponseFlags flags,
                          ...)
{
  va_list ap;
  int ret;
  enum MHD_ResponseOptions ro;

  ret = MHD_YES;
  response->flags = flags;
  va_start (ap, flags);
  while (MHD_RO_END != (ro = va_arg (ap, enum MHD_ResponseOptions)))
  {
    switch (ro)
    {
    default:
      ret = MHD_NO;
      break;
    }
  }
  va_end (ap);
  return ret;
}


/**
 * Given a file descriptor, read data from the file
 * to generate the response.
 *
 * @param cls pointer to the response
 * @param pos offset in the file to access
 * @param buf where to write the data
 * @param max number of bytes to write at most
 * @return number of bytes written
 */
static ssize_t
file_reader (void *cls,
             uint64_t pos,
             char *buf,
             size_t max)
{
  struct MHD_Response *response = cls;
#ifndef _WIN32
  ssize_t n;
#else  /* _WIN32 */
  const HANDLE fh = (HANDLE) _get_osfhandle (response->fd);
#endif /* _WIN32 */
  const int64_t offset64 = (int64_t)(pos + response->fd_off);

  if (offset64 < 0)
    return MHD_CONTENT_READER_END_WITH_ERROR; /* seek to required position is not possible */

#ifndef _WIN32
  if (max > SSIZE_MAX)
    max = SSIZE_MAX; /* Clamp to maximum return value. */

#if defined(HAVE_PREAD64)
  n = pread64(response->fd, buf, max, offset64);
#elif defined(HAVE_PREAD)
  if ( (sizeof(off_t) < sizeof (uint64_t)) &&
       (offset64 > (uint64_t)INT32_MAX) )
    return MHD_CONTENT_READER_END_WITH_ERROR; /* Read at required position is not possible. */

  n = pread(response->fd, buf, max, (off_t) offset64);
#else  /* ! HAVE_PREAD */
#if defined(HAVE_LSEEK64)
  if (lseek64 (response->fd,
               offset64,
               SEEK_SET) != offset64)
    return MHD_CONTENT_READER_END_WITH_ERROR; /* can't seek to required position */
#else  /* ! HAVE_LSEEK64 */
  if ( (sizeof(off_t) < sizeof (uint64_t)) &&
       (offset64 > (uint64_t)INT32_MAX) )
    return MHD_CONTENT_READER_END_WITH_ERROR; /* seek to required position is not possible */

  if (lseek (response->fd,
             (off_t) offset64,
             SEEK_SET) != (off_t) offset64)
    return MHD_CONTENT_READER_END_WITH_ERROR; /* can't seek to required position */
#endif /* ! HAVE_LSEEK64 */
  n = read (response->fd,
            buf,
            max);

#endif /* ! HAVE_PREAD */
  if (0 == n)
    return MHD_CONTENT_READER_END_OF_STREAM;
  if (n < 0)
    return MHD_CONTENT_READER_END_WITH_ERROR;
  return n;
#else /* _WIN32 */
  if (INVALID_HANDLE_VALUE == fh)
    return MHD_CONTENT_READER_END_WITH_ERROR; /* Value of 'response->fd' is not valid. */
  else
    {
      OVERLAPPED f_ol = {0, 0, {{0, 0}}, 0}; /* Initialize to zero. */
      ULARGE_INTEGER pos_uli;
      DWORD toRead = (max > INT32_MAX) ? INT32_MAX : (DWORD) max;
      DWORD resRead;

      pos_uli.QuadPart = (uint64_t) offset64; /* Simple transformation 64bit -> 2x32bit. */
      f_ol.Offset = pos_uli.LowPart;
      f_ol.OffsetHigh = pos_uli.HighPart;
      if (! ReadFile(fh, (void*)buf, toRead, &resRead, &f_ol))
        return MHD_CONTENT_READER_END_WITH_ERROR; /* Read error. */
      if (0 == resRead)
        return MHD_CONTENT_READER_END_OF_STREAM;
      return (ssize_t) resRead;
    }
#endif /* _WIN32 */
}


/**
 * Destroy file reader context.  Closes the file
 * descriptor.
 *
 * @param cls pointer to file descriptor
 */
static void
free_callback (void *cls)
{
  struct MHD_Response *response = cls;

  (void) close (response->fd);
  response->fd = -1;
}

#undef MHD_create_response_from_fd_at_offset

/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response
 * @param fd file descriptor referring to a file on disk with the
 *        data; will be closed when response is destroyed;
 *        fd should be in 'blocking' mode
 * @param offset offset to start reading from in the file;
 *        Be careful! `off_t` may have been compiled to be a
 *        64-bit variable for MHD, in which case your application
 *        also has to be compiled using the same options! Read
 *        the MHD manual for more details.
 * @return NULL on error (i.e. invalid arguments, out of memory)
 * @ingroup response
 */
struct MHD_Response *
MHD_create_response_from_fd_at_offset (size_t size,
				       int fd,
				       off_t offset)
{
  return MHD_create_response_from_fd_at_offset64 (size,
                                                  fd,
                                                  offset);
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response;
 *        sizes larger than 2 GiB may be not supported by OS or
 *        MHD build; see ::MHD_FEATURE_LARGE_FILE
 * @param fd file descriptor referring to a file on disk with the
 *        data; will be closed when response is destroyed;
 *        fd should be in 'blocking' mode
 * @param offset offset to start reading from in the file;
 *        reading file beyond 2 GiB may be not supported by OS or
 *        MHD build; see ::MHD_FEATURE_LARGE_FILE
 * @return NULL on error (i.e. invalid arguments, out of memory)
 * @ingroup response
 */
_MHD_EXTERN struct MHD_Response *
MHD_create_response_from_fd_at_offset64 (uint64_t size,
                                         int fd,
                                         uint64_t offset)
{
  struct MHD_Response *response;

#if !defined(HAVE___LSEEKI64) && !defined(HAVE_LSEEK64)
  if ( (sizeof(uint64_t) > sizeof(off_t)) &&
       ( (size > (uint64_t)INT32_MAX) ||
         (offset > (uint64_t)INT32_MAX) ||
         ((size + offset) >= (uint64_t)INT32_MAX) ) )
    return NULL;
#endif
  if ( ((int64_t)size < 0) ||
       ((int64_t)offset < 0) ||
       ((int64_t)(size + offset) < 0) )
    return NULL;

  response = MHD_create_response_from_callback (size,
						4 * 1024,
						&file_reader,
						NULL,
						&free_callback);
  if (NULL == response)
    return NULL;
  response->fd = fd;
  response->fd_off = offset;
  response->crc_cls = response;
  return response;
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response
 * @param fd file descriptor referring to a file on disk with the data
 * @return NULL on error (i.e. invalid arguments, out of memory)
 * @ingroup response
 */
struct MHD_Response *
MHD_create_response_from_fd (size_t size,
			     int fd)
{
  return MHD_create_response_from_fd_at_offset64 (size,
                                                  fd,
                                                  0);
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response;
 *        sizes larger than 2 GiB may be not supported by OS or
 *        MHD build; see ::MHD_FEATURE_LARGE_FILE
 * @param fd file descriptor referring to a file on disk with the
 *        data; will be closed when response is destroyed;
 *        fd should be in 'blocking' mode
 * @return NULL on error (i.e. invalid arguments, out of memory)
 * @ingroup response
 */
_MHD_EXTERN struct MHD_Response *
MHD_create_response_from_fd64 (uint64_t size,
                               int fd)
{
  return MHD_create_response_from_fd_at_offset64 (size,
                                                  fd,
                                                  0);
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the @a data portion of the response
 * @param data the data itself
 * @param must_free libmicrohttpd should free data when done
 * @param must_copy libmicrohttpd must make a copy of @a data
 *        right away, the data maybe released anytime after
 *        this call returns
 * @return NULL on error (i.e. invalid arguments, out of memory)
 * @deprecated use #MHD_create_response_from_buffer instead
 * @ingroup response
 */
struct MHD_Response *
MHD_create_response_from_data (size_t size,
                               void *data,
                               int must_free,
                               int must_copy)
{
  struct MHD_Response *response;
  void *tmp;

  if ((NULL == data) && (size > 0))
    return NULL;
  if (NULL == (response = MHD_calloc_ (1, sizeof (struct MHD_Response))))
    return NULL;
  response->fd = -1;
  if (! MHD_mutex_init_ (&response->mutex))
    {
      free (response);
      return NULL;
    }
  if ((must_copy) && (size > 0))
    {
      if (NULL == (tmp = malloc (size)))
        {
          MHD_mutex_destroy_chk_ (&response->mutex);
          free (response);
          return NULL;
        }
      memcpy (tmp, data, size);
      must_free = MHD_YES;
      data = tmp;
    }
  if (must_free)
    {
      response->crfc = &free;
      response->crc_cls = data;
    }
  response->reference_count = 1;
  response->total_size = size;
  response->data = data;
  response->data_size = size;
  return response;
}


/**
 * Create a response object.  The response object can be extended with
 * header information and then be used any number of times.
 *
 * @param size size of the data portion of the response
 * @param buffer size bytes containing the response's data portion
 * @param mode flags for buffer management
 * @return NULL on error (i.e. invalid arguments, out of memory)
 * @ingroup response
 */
struct MHD_Response *
MHD_create_response_from_buffer (size_t size,
				 void *buffer,
				 enum MHD_ResponseMemoryMode mode)
{
  return MHD_create_response_from_data (size,
					buffer,
					mode == MHD_RESPMEM_MUST_FREE,
					mode == MHD_RESPMEM_MUST_COPY);
}


#ifdef UPGRADE_SUPPORT
/**
 * This connection-specific callback is provided by MHD to
 * applications (unusual) during the #MHD_UpgradeHandler.
 * It allows applications to perform 'special' actions on
 * the underlying socket from the upgrade.
 *
 * @param urh the handle identifying the connection to perform
 *            the upgrade @a action on.
 * @param action which action should be performed
 * @param ... arguments to the action (depends on the action)
 * @return #MHD_NO on error, #MHD_YES on success
 */
_MHD_EXTERN int
MHD_upgrade_action (struct MHD_UpgradeResponseHandle *urh,
                    enum MHD_UpgradeAction action,
                    ...)
{
  struct MHD_Connection *connection;
  struct MHD_Daemon *daemon;

  if (NULL == urh)
    return MHD_NO;
  connection = urh->connection;

  /* Precaution checks on external data. */
  if (NULL == connection)
    return MHD_NO;
  daemon = connection->daemon;
  if (NULL == daemon)
    return MHD_NO;

  switch (action)
  {
  case MHD_UPGRADE_ACTION_CLOSE:
    if (urh->was_closed)
      return MHD_NO; /* Already closed. */

    /* transition to special 'closed' state for start of cleanup */
#ifdef HTTPS_SUPPORT
    if (0 != (daemon->options & MHD_USE_TLS) )
      {
        /* signal that app is done by shutdown() of 'app' socket */
        /* Application will not use anyway this socket after this command. */
        shutdown (urh->app.socket,
                  SHUT_RDWR);
      }
#endif /* HTTPS_SUPPORT */
    EXTRA_CHECK (MHD_CONNECTION_UPGRADE == connection->state);
    urh->was_closed = true;
    /* As soon as connection will be marked with BOTH
     * 'urh->was_closed' AND 'urh->clean_ready', it will
     * be moved to cleanup list by MHD_resume_connection(). */
    MHD_resume_connection (connection);
    return MHD_YES;
  default:
    /* we don't understand this one */
    return MHD_NO;
  }
}


/**
 * We are done sending the header of a given response to the client.
 * Now it is time to perform the upgrade and hand over the connection
 * to the application.
 * @remark To be called only from thread that process connection's
 * recv(), send() and response. Must be called right after sending
 * response headers.
 *
 * @param response the response that was created for an upgrade
 * @param connection the specific connection we are upgrading
 * @return #MHD_YES on success, #MHD_NO on failure (will cause
 *        connection to be closed)
 */
int
MHD_response_execute_upgrade_ (struct MHD_Response *response,
                               struct MHD_Connection *connection)
{
  struct MHD_Daemon *daemon = connection->daemon;
  struct MHD_UpgradeResponseHandle *urh;
  size_t rbo;

  if (0 == (daemon->options & MHD_ALLOW_UPGRADE))
    return MHD_NO;

  if (NULL ==
      MHD_get_response_header (response,
                               MHD_HTTP_HEADER_UPGRADE))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Invalid response for upgrade: application failed to set the 'Upgrade' header!\n"));
#endif
      return MHD_NO;
    }

  urh = MHD_calloc_ (1, sizeof (struct MHD_UpgradeResponseHandle));
  if (NULL == urh)
    return MHD_NO;
  urh->connection = connection;
  rbo = connection->read_buffer_offset;
  connection->read_buffer_offset = 0;
#ifdef HTTPS_SUPPORT
  if (0 != (daemon->options & MHD_USE_TLS) )
  {
    struct MemoryPool *pool;
    size_t avail;
    char *buf;
    MHD_socket sv[2];
#if defined(MHD_socket_nosignal_) || !defined(MHD_socket_pair_nblk_)
    int res1;
    int res2;
#endif /* MHD_socket_nosignal_ || !MHD_socket_pair_nblk_ */

#ifdef MHD_socket_pair_nblk_
    if (! MHD_socket_pair_nblk_ (sv))
      {
        free (urh);
        return MHD_NO;
      }
#else  /* !MHD_socket_pair_nblk_ */
    if (! MHD_socket_pair_ (sv))
      {
        free (urh);
        return MHD_NO;
      }
    res1 = MHD_socket_nonblocking_(sv[0]);
    res2 = MHD_socket_nonblocking_(sv[1]);
    if ( (! res1) || (! res2) )
      {
#ifdef HAVE_MESSAGES
        MHD_DLOG (daemon,
		  _("Failed to make loopback sockets non-blocking.\n"));
#endif
        if (! res2)
          {
            /* Socketpair cannot be used. */
            MHD_socket_close_chk_ (sv[0]);
            MHD_socket_close_chk_ (sv[1]);
            free (urh);
            return MHD_NO;
          }
      }
#endif /* !MHD_socket_pair_nblk_ */
#ifdef MHD_socket_nosignal_
    res1 = MHD_socket_nosignal_(sv[0]);
    res2 = MHD_socket_nosignal_(sv[1]);
    if ( (! res1) || (! res2) )
      {
#ifdef HAVE_MESSAGES
        MHD_DLOG (daemon,
                  _("Failed to set SO_NOSIGPIPE on loopback sockets.\n"));
#endif
#ifndef MSG_NOSIGNAL
        if (!res2)
          {
            /* Socketpair cannot be used. */
            MHD_socket_close_chk_ (sv[0]);
            MHD_socket_close_chk_ (sv[1]);
            free (urh);
            return MHD_NO;
          }
#endif /* ! MSG_NOSIGNAL */
      }
#endif /* MHD_socket_nosignal_ */
    if ( (! MHD_SCKT_FD_FITS_FDSET_ (sv[1],
                                     NULL)) &&
         (0 == (daemon->options & (MHD_USE_POLL | MHD_USE_EPOLL))) )
      {
#ifdef HAVE_MESSAGES
        MHD_DLOG (daemon,
                  _("Socketpair descriptor larger than FD_SETSIZE: %d > %d\n"),
                  (int) sv[1],
                  (int) FD_SETSIZE);
#endif
        MHD_socket_close_chk_ (sv[0]);
        MHD_socket_close_chk_ (sv[1]);
        free (urh);
        return MHD_NO;
      }
    urh->app.socket = sv[0];
    urh->app.urh = urh;
    urh->app.celi = MHD_EPOLL_STATE_UNREADY;
    urh->mhd.socket = sv[1];
    urh->mhd.urh = urh;
    urh->mhd.celi = MHD_EPOLL_STATE_UNREADY;
    pool = connection->pool;
    avail = MHD_pool_get_free (pool);
    if (avail < RESERVE_EBUF_SIZE)
      {
        /* connection's pool is totally at the limit,
           use our 'emergency' buffer of #RESERVE_EBUF_SIZE bytes. */
        avail = RESERVE_EBUF_SIZE;
        buf = urh->e_buf;
      }
    else
      {
        /* Normal case: grab all remaining memory from the
           connection's pool for the IO buffers; the connection
           certainly won't need it anymore as we've upgraded
           to another protocol. */
        buf = MHD_pool_allocate (pool,
                                 avail,
                                 MHD_NO);
      }
    /* use half the buffer for inbound, half for outbound */
    urh->in_buffer_size = avail / 2;
    urh->out_buffer_size = avail - urh->in_buffer_size;
    urh->in_buffer = buf;
    urh->out_buffer = &buf[urh->in_buffer_size];
#ifdef EPOLL_SUPPORT
    /* Launch IO processing by the event loop */
    if (0 != (daemon->options & MHD_USE_EPOLL))
      {
        /* We're running with epoll(), need to add the sockets
           to the event set of the daemon's `epoll_upgrade_fd` */
        struct epoll_event event;

        EXTRA_CHECK (-1 != daemon->epoll_upgrade_fd);
        /* First, add network socket */
        event.events = EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLET;
        event.data.ptr = &urh->app;
        if (0 != epoll_ctl (daemon->epoll_upgrade_fd,
                            EPOLL_CTL_ADD,
                            connection->socket_fd,
                            &event))
	{
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Call to epoll_ctl failed: %s\n"),
                    MHD_socket_last_strerr_ ());
#endif
          MHD_socket_close_chk_ (sv[0]);
          MHD_socket_close_chk_ (sv[1]);
          free (urh);
          return MHD_NO;
	}

        /* Second, add our end of the UNIX socketpair() */
        event.events = EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLET;
        event.data.ptr = &urh->mhd;
        if (0 != epoll_ctl (daemon->epoll_upgrade_fd,
                            EPOLL_CTL_ADD,
                            urh->mhd.socket,
                            &event))
	{
          event.events = EPOLLIN | EPOLLOUT | EPOLLPRI;
          event.data.ptr = &urh->app;
          if (0 != epoll_ctl (daemon->epoll_upgrade_fd,
                              EPOLL_CTL_DEL,
                              connection->socket_fd,
                              &event))
            MHD_PANIC (_("Error cleaning up while handling epoll error"));
#ifdef HAVE_MESSAGES
          MHD_DLOG (daemon,
                    _("Call to epoll_ctl failed: %s\n"),
                    MHD_socket_last_strerr_ ());
#endif
          MHD_socket_close_chk_ (sv[0]);
          MHD_socket_close_chk_ (sv[1]);
          free (urh);
          return MHD_NO;
	}
	EDLL_insert (daemon->eready_urh_head,
		     daemon->eready_urh_tail,
		     urh);
	urh->in_eready_list = true;
      }
#endif /* EPOLL_SUPPORT */
    if (0 == (daemon->options & MHD_USE_THREAD_PER_CONNECTION) )
      {
        /* This takes care of further processing for most event loops:
           simply add to DLL for bi-direcitonal processing */
        DLL_insert (daemon->urh_head,
                    daemon->urh_tail,
                    urh);
      }
    /* In thread-per-connection mode, thread will switch to forwarding once
     * connection.urh is not NULL and connection.state == MHD_CONNECTION_UPGRADE.
     */
  }
  else
    {
      urh->app.socket = MHD_INVALID_SOCKET;
      urh->mhd.socket = MHD_INVALID_SOCKET;
      /* Non-TLS connection do not hold any additional resources. */
      urh->clean_ready = true;
    }
#else  /* ! HTTPS_SUPPORT */
  urh->clean_ready = true;
#endif /* ! HTTPS_SUPPORT */
  connection->urh = urh;
  /* As far as MHD's event loops are concerned, this connection is
     suspended; it will be resumed once application is done by the
     #MHD_upgrade_action() function */
  MHD_suspend_connection (connection);

  /* hand over socket to application */
  response->upgrade_handler (response->upgrade_handler_cls,
                             connection,
                             connection->client_context,
                             connection->read_buffer,
                             rbo,
#ifdef HTTPS_SUPPORT
                             (0 == (daemon->options & MHD_USE_TLS) ) ?
                             connection->socket_fd : urh->app.socket,
#else  /* ! HTTPS_SUPPORT */
                             connection->socket_fd,
#endif /* ! HTTPS_SUPPORT */
                             urh);
  return MHD_YES;
}


/**
 * Create a response object that can be used for 101 UPGRADE
 * responses, for example to implement WebSockets.  After sending the
 * response, control over the data stream is given to the callback (which
 * can then, for example, start some bi-directional communication).
 * If the response is queued for multiple connections, the callback
 * will be called for each connection.  The callback
 * will ONLY be called after the response header was successfully passed
 * to the OS; if there are communication errors before, the usual MHD
 * connection error handling code will be performed.
 *
 * Setting the correct HTTP code (i.e. MHD_HTTP_SWITCHING_PROTOCOLS)
 * and setting correct HTTP headers for the upgrade must be done
 * manually (this way, it is possible to implement most existing
 * WebSocket versions using this API; in fact, this API might be useful
 * for any protocol switch, not just WebSockets).  Note that
 * draft-ietf-hybi-thewebsocketprotocol-00 cannot be implemented this
 * way as the header "HTTP/1.1 101 WebSocket Protocol Handshake"
 * cannot be generated; instead, MHD will always produce "HTTP/1.1 101
 * Switching Protocols" (if the response code 101 is used).
 *
 * As usual, the response object can be extended with header
 * information and then be used any number of times (as long as the
 * header information is not connection-specific).
 *
 * @param upgrade_handler function to call with the 'upgraded' socket
 * @param upgrade_handler_cls closure for @a upgrade_handler
 * @return NULL on error (i.e. invalid arguments, out of memory)
 */
_MHD_EXTERN struct MHD_Response *
MHD_create_response_for_upgrade (MHD_UpgradeHandler upgrade_handler,
				 void *upgrade_handler_cls)
{
  struct MHD_Response *response;

  if (NULL == upgrade_handler)
    return NULL; /* invalid request */
  response = MHD_calloc_ (1, sizeof (struct MHD_Response));
  if (NULL == response)
    return NULL;
  if (! MHD_mutex_init_ (&response->mutex))
    {
      free (response);
      return NULL;
    }
  response->upgrade_handler = upgrade_handler;
  response->upgrade_handler_cls = upgrade_handler_cls;
  response->total_size = MHD_SIZE_UNKNOWN;
  response->reference_count = 1;
  if (MHD_NO ==
      MHD_add_response_header (response,
                               MHD_HTTP_HEADER_CONNECTION,
                               "Upgrade"))
    {
      MHD_destroy_response (response);
      return NULL;
    }
  return response;
}
#endif /* UPGRADE_SUPPORT */


/**
 * Destroy a response object and associated resources.  Note that
 * libmicrohttpd may keep some of the resources around if the response
 * is still in the queue for some clients, so the memory may not
 * necessarily be freed immediately.
 *
 * @param response response to destroy
 * @ingroup response
 */
void
MHD_destroy_response (struct MHD_Response *response)
{
  struct MHD_HTTP_Header *pos;

  if (NULL == response)
    return;
  MHD_mutex_lock_chk_ (&response->mutex);
  if (0 != --(response->reference_count))
    {
      MHD_mutex_unlock_chk_ (&response->mutex);
      return;
    }
  MHD_mutex_unlock_chk_ (&response->mutex);
  MHD_mutex_destroy_chk_ (&response->mutex);
  if (NULL != response->crfc)
    response->crfc (response->crc_cls);
  while (NULL != response->first_header)
    {
      pos = response->first_header;
      response->first_header = pos->next;
      free (pos->header);
      free (pos->value);
      free (pos);
    }
  free (response);
}


/**
 * Increments the reference counter for the @a response.
 *
 * @param response object to modify
 */
void
MHD_increment_response_rc (struct MHD_Response *response)
{
  MHD_mutex_lock_chk_ (&response->mutex);
  (response->reference_count)++;
  MHD_mutex_unlock_chk_ (&response->mutex);
}


/* end of response.c */
