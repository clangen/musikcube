/*
     This file is part of libmicrohttpd
     Copyright (C) 2007, 2008, 2010 Daniel Pittman and Christian Grothoff

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
 * @file connection_https.c
 * @brief  Methods for managing SSL/TLS connections. This file is only
 *         compiled if ENABLE_HTTPS is set.
 * @author Sagie Amir
 * @author Christian Grothoff
 */

#include "internal.h"
#include "connection.h"
#include "connection_https.h"
#include "memorypool.h"
#include "response.h"
#include "mhd_mono_clock.h"
#include <gnutls/gnutls.h>


/**
 * Give gnuTLS chance to work on the TLS handshake.
 *
 * @param connection connection to handshake on
 * @return #MHD_YES on error or if the handshake is progressing
 *         #MHD_NO if the handshake has completed successfully
 *         and we should start to read/write data
 */
static int
run_tls_handshake (struct MHD_Connection *connection)
{
  int ret;

  if (MHD_TLS_CONNECTION_INIT == connection->state)
    {
      ret = gnutls_handshake (connection->tls_session);
      if (ret == GNUTLS_E_SUCCESS)
	{
	  /* set connection state to enable HTTP processing */
	  connection->state = MHD_CONNECTION_INIT;
	  MHD_update_last_activity_ (connection);
	  return MHD_NO;
	}
      if ( (GNUTLS_E_AGAIN == ret) ||
	   (GNUTLS_E_INTERRUPTED == ret) )
	{
	  /* handshake not done */
	  return MHD_YES;
	}
      /* handshake failed */
#ifdef HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
		_("Error: received handshake message out of context\n"));
#endif
      MHD_connection_close_ (connection,
                             MHD_REQUEST_TERMINATED_WITH_ERROR);
      return MHD_YES;
    }
  return MHD_NO;
}


/**
 * This function handles a particular SSL/TLS connection when
 * it has been determined that there is data to be read off a
 * socket. Message processing is done by message type which is
 * determined by peeking into the first message type byte of the
 * stream.
 *
 * Error message handling: all fatal level messages cause the
 * connection to be terminated.
 *
 * Application data is forwarded to the underlying daemon for
 * processing.
 *
 * @param connection the source connection
 * @return always #MHD_YES (we should continue to process the connection)
 */
static int
MHD_tls_connection_handle_read (struct MHD_Connection *connection)
{
  if (MHD_YES == run_tls_handshake (connection))
    return MHD_YES;
  return MHD_connection_handle_read (connection);
}


/**
 * This function was created to handle writes to sockets when it has
 * been determined that the socket can be written to. This function
 * will forward all write requests to the underlying daemon unless
 * the connection has been marked for closing.
 *
 * @return always #MHD_YES (we should continue to process the connection)
 */
static int
MHD_tls_connection_handle_write (struct MHD_Connection *connection)
{
  if (MHD_YES == run_tls_handshake (connection))
    return MHD_YES;
  return MHD_connection_handle_write (connection);
}


/**
 * This function was created to handle per-connection processing that
 * has to happen even if the socket cannot be read or written to.  All
 * implementations (multithreaded, external select, internal select)
 * call this function.
 *
 * @param connection being handled
 * @return #MHD_YES if we should continue to process the
 *         connection (not dead yet), #MHD_NO if it died
 */
static int
MHD_tls_connection_handle_idle (struct MHD_Connection *connection)
{
  time_t timeout;

#if DEBUG_STATES
  MHD_DLOG (connection->daemon,
            _("In function %s handling connection at state: %s\n"),
            __FUNCTION__,
            MHD_state_to_string (connection->state));
#endif
  if (connection->suspended)
    return MHD_connection_handle_idle (connection);
  switch (connection->state)
    {
      /* on newly created connections we might reach here before any reply has been received */
    case MHD_TLS_CONNECTION_INIT:
      break;
      /* close connection if necessary */
    case MHD_CONNECTION_CLOSED:
      return MHD_connection_handle_idle (connection);
    default:
      return MHD_connection_handle_idle (connection);
    }
  timeout = connection->connection_timeout;
  if ( (timeout != 0) &&
       (timeout < (MHD_monotonic_sec_counter() - connection->last_activity)))
    MHD_connection_close_ (connection,
                           MHD_REQUEST_TERMINATED_TIMEOUT_REACHED);
#ifdef EPOLL_SUPPORT
  return MHD_connection_epoll_update_ (connection);
#else
  return MHD_YES;
#endif
}


/**
 * Set connection callback function to be used through out
 * the processing of this secure connection.
 *
 * @param connection which callbacks should be modified
 */
void
MHD_set_https_callbacks (struct MHD_Connection *connection)
{
  connection->read_handler = &MHD_tls_connection_handle_read;
  connection->write_handler = &MHD_tls_connection_handle_write;
  connection->idle_handler = &MHD_tls_connection_handle_idle;
}


/**
 * Initiate shutdown of TLS layer of connection.
 *
 * @param connection to use
 * @return #MHD_YES if succeed, #MHD_NO otherwise.
 */
int
MHD_tls_connection_shutdown (struct MHD_Connection *connection)
{
  if (connection->tls_closed)
    return MHD_NO;

  connection->tls_closed = true;
  return (GNUTLS_E_SUCCESS == gnutls_bye(connection->tls_session, GNUTLS_SHUT_WR)) ?
      MHD_YES : MHD_NO;
}

/* end of connection_https.c */
