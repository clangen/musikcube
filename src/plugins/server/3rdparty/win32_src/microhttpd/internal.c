/*
     This file is part of libmicrohttpd
     Copyright (C) 2007 Daniel Pittman and Christian Grothoff

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
 * @file microhttpd/internal.c
 * @brief  internal shared structures
 * @author Daniel Pittman
 * @author Christian Grothoff
 */

#include "internal.h"
#include "mhd_str.h"

#ifdef HAVE_MESSAGES
#if DEBUG_STATES
/**
 * State to string dictionary.
 */
const char *
MHD_state_to_string (enum MHD_CONNECTION_STATE state)
{
  switch (state)
    {
    case MHD_CONNECTION_INIT:
      return "connection init";
    case MHD_CONNECTION_URL_RECEIVED:
      return "connection url received";
    case MHD_CONNECTION_HEADER_PART_RECEIVED:
      return "header partially received";
    case MHD_CONNECTION_HEADERS_RECEIVED:
      return "headers received";
    case MHD_CONNECTION_HEADERS_PROCESSED:
      return "headers processed";
    case MHD_CONNECTION_CONTINUE_SENDING:
      return "continue sending";
    case MHD_CONNECTION_CONTINUE_SENT:
      return "continue sent";
    case MHD_CONNECTION_BODY_RECEIVED:
      return "body received";
    case MHD_CONNECTION_FOOTER_PART_RECEIVED:
      return "footer partially received";
    case MHD_CONNECTION_FOOTERS_RECEIVED:
      return "footers received";
    case MHD_CONNECTION_HEADERS_SENDING:
      return "headers sending";
    case MHD_CONNECTION_HEADERS_SENT:
      return "headers sent";
    case MHD_CONNECTION_NORMAL_BODY_READY:
      return "normal body ready";
    case MHD_CONNECTION_NORMAL_BODY_UNREADY:
      return "normal body unready";
    case MHD_CONNECTION_CHUNKED_BODY_READY:
      return "chunked body ready";
    case MHD_CONNECTION_CHUNKED_BODY_UNREADY:
      return "chunked body unready";
    case MHD_CONNECTION_BODY_SENT:
      return "body sent";
    case MHD_CONNECTION_FOOTERS_SENDING:
      return "footers sending";
    case MHD_CONNECTION_FOOTERS_SENT:
      return "footers sent";
    case MHD_CONNECTION_CLOSED:
      return "closed";
    case MHD_TLS_CONNECTION_INIT:
      return "secure connection init";
    default:
      return "unrecognized connection state";
    }
}
#endif
#endif


#ifdef HAVE_MESSAGES
/**
 * fprintf-like helper function for logging debug
 * messages.
 */
void
MHD_DLOG (const struct MHD_Daemon *daemon,
          const char *format,
          ...)
{
  va_list va;

  if (0 == (daemon->options & MHD_USE_ERROR_LOG))
    return;
  va_start (va, format);
  daemon->custom_error_log (daemon->custom_error_log_cls,
                            format,
                            va);
  va_end (va);
}
#endif


/**
 * Convert all occurrences of '+' to ' '.
 *
 * @param arg string that is modified (in place), must be 0-terminated
 */
void
MHD_unescape_plus (char *arg)
{
  char *p;

  for (p=strchr (arg, '+'); NULL != p; p = strchr (p + 1, '+'))
    *p = ' ';
}


/**
 * Process escape sequences ('%HH') Updates val in place; the
 * result should be UTF-8 encoded and cannot be larger than the input.
 * The result must also still be 0-terminated.
 *
 * @param val value to unescape (modified in the process)
 * @return length of the resulting val (strlen(val) maybe
 *  shorter afterwards due to elimination of escape sequences)
 */
size_t
MHD_http_unescape (char *val)
{
  char *rpos = val;
  char *wpos = val;

  while ('\0' != *rpos)
    {
      uint32_t num;
      switch (*rpos)
	{
	case '%':
          if (2 == MHD_strx_to_uint32_n_ (rpos + 1,
                                          2,
                                          &num))
	    {
	      *wpos = (char)((unsigned char) num);
	      wpos++;
	      rpos += 3;
	      break;
	    }
          /* TODO: add bad sequence handling */
	  /* intentional fall through! */
	default:
	  *wpos = *rpos;
	  wpos++;
	  rpos++;
	}
    }
  *wpos = '\0'; /* add 0-terminator */
  return wpos - val; /* = strlen(val) */
}


/**
 * Parse and unescape the arguments given by the client
 * as part of the HTTP request URI.
 *
 * @param kind header kind to pass to @a cb
 * @param connection connection to add headers to
 * @param[in|out] args argument URI string (after "?" in URI),
 *        clobbered in the process!
 * @param cb function to call on each key-value pair found
 * @param[out] num_headers set to the number of headers found
 * @return #MHD_NO on failure (@a cb returned #MHD_NO),
 *         #MHD_YES for success (parsing succeeded, @a cb always
 *                               returned #MHD_YES)
 */
int
MHD_parse_arguments_ (struct MHD_Connection *connection,
		      enum MHD_ValueKind kind,
		      char *args,
		      MHD_ArgumentIterator_ cb,
		      unsigned int *num_headers)
{
  struct MHD_Daemon *daemon = connection->daemon;
  char *equals;
  char *amper;

  *num_headers = 0;
  while ( (NULL != args) &&
	  ('\0' != args[0]) )
    {
      equals = strchr (args, '=');
      amper = strchr (args, '&');
      if (NULL == amper)
	{
	  /* last argument */
	  if (NULL == equals)
	    {
	      /* last argument, without '=' */
              MHD_unescape_plus (args);
	      daemon->unescape_callback (daemon->unescape_callback_cls,
					 connection,
					 args);
	      if (MHD_YES != cb (connection,
				 args,
				 NULL,
				 kind))
		return MHD_NO;
	      (*num_headers)++;
	      break;
	    }
	  /* got 'foo=bar' */
	  equals[0] = '\0';
	  equals++;
          MHD_unescape_plus (args);
	  daemon->unescape_callback (daemon->unescape_callback_cls,
				     connection,
				     args);
          MHD_unescape_plus (equals);
	  daemon->unescape_callback (daemon->unescape_callback_cls,
				     connection,
				     equals);
	  if (MHD_YES != cb (connection,
			     args,
			     equals,
			     kind))
	    return MHD_NO;
	  (*num_headers)++;
	  break;
	}
      /* amper is non-NULL here */
      amper[0] = '\0';
      amper++;
      if ( (NULL == equals) ||
	   (equals >= amper) )
	{
	  /* got 'foo&bar' or 'foo&bar=val', add key 'foo' with NULL for value */
          MHD_unescape_plus (args);
	  daemon->unescape_callback (daemon->unescape_callback_cls,
				     connection,
				     args);
	  if (MHD_YES != cb (connection,
			     args,
			     NULL,
			     kind))
	    return MHD_NO;
	  /* continue with 'bar' */
	  (*num_headers)++;
	  args = amper;
	  continue;
	}
      /* equals and amper are non-NULL here, and equals < amper,
	 so we got regular 'foo=value&bar...'-kind of argument */
      equals[0] = '\0';
      equals++;
      MHD_unescape_plus (args);
      daemon->unescape_callback (daemon->unescape_callback_cls,
				 connection,
				 args);
      MHD_unescape_plus (equals);
      daemon->unescape_callback (daemon->unescape_callback_cls,
				 connection,
				 equals);
      if (MHD_YES != cb (connection,
			 args,
			 equals,
			 kind))
        return MHD_NO;
      (*num_headers)++;
      args = amper;
    }
  return MHD_YES;
}

/* end of internal.c */
