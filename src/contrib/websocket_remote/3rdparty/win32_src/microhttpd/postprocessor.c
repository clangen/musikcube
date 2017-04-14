/*
     This file is part of libmicrohttpd
     Copyright (C) 2007-2013 Daniel Pittman and Christian Grothoff

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
 * @file postprocessor.c
 * @brief  Methods for parsing POST data
 * @author Christian Grothoff
 */

#include "internal.h"
#include "mhd_str.h"
#include "mhd_compat.h"

/**
 * Size of on-stack buffer that we use for un-escaping of the value.
 * We use a pretty small value to be nice to the stack on embedded
 * systems.
 */
#define XBUF_SIZE 512

/**
 * States in the PP parser's state machine.
 */
enum PP_State
{
  /* general states */
  PP_Error,
  PP_Done,
  PP_Init,
  PP_NextBoundary,

  /* url encoding-states */
  PP_ProcessValue,
  PP_ExpectNewLine,

  /* post encoding-states  */
  PP_ProcessEntryHeaders,
  PP_PerformCheckMultipart,
  PP_ProcessValueToBoundary,
  PP_PerformCleanup,

  /* nested post-encoding states */
  PP_Nested_Init,
  PP_Nested_PerformMarking,
  PP_Nested_ProcessEntryHeaders,
  PP_Nested_ProcessValueToBoundary,
  PP_Nested_PerformCleanup

};


enum RN_State
{
  /**
   * No RN-preprocessing in this state.
   */
  RN_Inactive = 0,

  /**
   * If the next character is CR, skip it.  Otherwise,
   * just go inactive.
   */
  RN_OptN = 1,

  /**
   * Expect LFCR (and only LFCR).  As always, we also
   * expect only LF or only CR.
   */
  RN_Full = 2,

  /**
   * Expect either LFCR or '--'LFCR.  If '--'LFCR, transition into dash-state
   * for the main state machine
   */
  RN_Dash = 3,

  /**
   * Got a single dash, expect second dash.
   */
  RN_Dash2 = 4
};


/**
 * Bits for the globally known fields that
 * should not be deleted when we exit the
 * nested state.
 */
enum NE_State
{
  NE_none = 0,
  NE_content_name = 1,
  NE_content_type = 2,
  NE_content_filename = 4,
  NE_content_transfer_encoding = 8
};


/**
 * Internal state of the post-processor.  Note that the fields
 * are sorted by type to enable optimal packing by the compiler.
 */
struct MHD_PostProcessor
{

  /**
   * The connection for which we are doing
   * POST processing.
   */
  struct MHD_Connection *connection;

  /**
   * Function to call with POST data.
   */
  MHD_PostDataIterator ikvi;

  /**
   * Extra argument to ikvi.
   */
  void *cls;

  /**
   * Encoding as given by the headers of the
   * connection.
   */
  const char *encoding;

  /**
   * Primary boundary (points into encoding string)
   */
  const char *boundary;

  /**
   * Nested boundary (if we have multipart/mixed encoding).
   */
  char *nested_boundary;

  /**
   * Pointer to the name given in disposition.
   */
  char *content_name;

  /**
   * Pointer to the (current) content type.
   */
  char *content_type;

  /**
   * Pointer to the (current) filename.
   */
  char *content_filename;

  /**
   * Pointer to the (current) encoding.
   */
  char *content_transfer_encoding;

  /**
   * Unprocessed value bytes due to escape
   * sequences (URL-encoding only).
   */
  char xbuf[8];

  /**
   * Size of our buffer for the key.
   */
  size_t buffer_size;

  /**
   * Current position in the key buffer.
   */
  size_t buffer_pos;

  /**
   * Current position in @e xbuf.
   */
  size_t xbuf_pos;

  /**
   * Current offset in the value being processed.
   */
  uint64_t value_offset;

  /**
   * strlen(boundary) -- if boundary != NULL.
   */
  size_t blen;

  /**
   * strlen(nested_boundary) -- if nested_boundary != NULL.
   */
  size_t nlen;

  /**
   * Do we have to call the 'ikvi' callback when processing the
   * multipart post body even if the size of the payload is zero?
   * Set to #MHD_YES whenever we parse a new multiparty entry header,
   * and to #MHD_NO the first time we call the 'ikvi' callback.
   * Used to ensure that we do always call 'ikvi' even if the
   * payload is empty (but not more than once).
   */
  int must_ikvi;

  /**
   * State of the parser.
   */
  enum PP_State state;

  /**
   * Side-state-machine: skip LRCR (or just LF).
   * Set to 0 if we are not in skip mode.  Set to 2
   * if a LFCR is expected, set to 1 if a CR should
   * be skipped if it is the next character.
   */
  enum RN_State skip_rn;

  /**
   * If we are in skip_rn with "dash" mode and
   * do find 2 dashes, what state do we go into?
   */
  enum PP_State dash_state;

  /**
   * Which headers are global? (used to tell which
   * headers were only valid for the nested multipart).
   */
  enum NE_State have;

};


/**
 * Create a `struct MHD_PostProcessor`.
 *
 * A `struct MHD_PostProcessor` can be used to (incrementally) parse
 * the data portion of a POST request.  Note that some buggy browsers
 * fail to set the encoding type.  If you want to support those, you
 * may have to call #MHD_set_connection_value with the proper encoding
 * type before creating a post processor (if no supported encoding
 * type is set, this function will fail).
 *
 * @param connection the connection on which the POST is
 *        happening (used to determine the POST format)
 * @param buffer_size maximum number of bytes to use for
 *        internal buffering (used only for the parsing,
 *        specifically the parsing of the keys).  A
 *        tiny value (256-1024) should be sufficient.
 *        Do NOT use a value smaller than 256.  For good
 *        performance, use 32 or 64k (i.e. 65536).
 * @param iter iterator to be called with the parsed data,
 *        Must NOT be NULL.
 * @param iter_cls first argument to @a iter
 * @return NULL on error (out of memory, unsupported encoding),
 *         otherwise a PP handle
 * @ingroup request
 */
struct MHD_PostProcessor *
MHD_create_post_processor (struct MHD_Connection *connection,
                           size_t buffer_size,
                           MHD_PostDataIterator iter,
                           void *iter_cls)
{
  struct MHD_PostProcessor *ret;
  const char *encoding;
  const char *boundary;
  size_t blen;

  if ( (buffer_size < 256) ||
       (NULL == connection) ||
       (NULL == iter))
    mhd_panic (mhd_panic_cls,
               __FILE__,
               __LINE__,
               NULL);
  encoding = MHD_lookup_connection_value (connection,
                                          MHD_HEADER_KIND,
                                          MHD_HTTP_HEADER_CONTENT_TYPE);
  if (NULL == encoding)
    return NULL;
  boundary = NULL;
  if (! MHD_str_equal_caseless_n_ (MHD_HTTP_POST_ENCODING_FORM_URLENCODED,
                                   encoding,
                                   MHD_STATICSTR_LEN_ (MHD_HTTP_POST_ENCODING_FORM_URLENCODED)))
    {
      if (! MHD_str_equal_caseless_n_ (MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA,
                                       encoding,
                                       MHD_STATICSTR_LEN_ (MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA)))
        return NULL;
      boundary =
        &encoding[MHD_STATICSTR_LEN_ (MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA)];
      /* Q: should this be "strcasestr"? */
      boundary = strstr (boundary, "boundary=");
      if (NULL == boundary)
	return NULL; /* failed to determine boundary */
      boundary += MHD_STATICSTR_LEN_ ("boundary=");
      blen = strlen (boundary);
      if ( (blen == 0) ||
           (blen * 2 + 2 > buffer_size) )
        return NULL;            /* (will be) out of memory or invalid boundary */
      if ( (boundary[0] == '"') &&
           (boundary[blen - 1] == '"') )
	{
	  /* remove enclosing quotes */
	  ++boundary;
	  blen -= 2;
	}
    }
  else
    blen = 0;
  buffer_size += 4; /* round up to get nice block sizes despite boundary search */

  /* add +1 to ensure we ALWAYS have a zero-termination at the end */
  if (NULL == (ret = MHD_calloc_ (1, sizeof (struct MHD_PostProcessor) + buffer_size + 1)))
    return NULL;
  ret->connection = connection;
  ret->ikvi = iter;
  ret->cls = iter_cls;
  ret->encoding = encoding;
  ret->buffer_size = buffer_size;
  ret->state = PP_Init;
  ret->blen = blen;
  ret->boundary = boundary;
  ret->skip_rn = RN_Inactive;
  return ret;
}


/**
 * Process url-encoded POST data.
 *
 * @param pp post processor context
 * @param post_data upload data
 * @param post_data_len number of bytes in @a post_data
 * @return #MHD_YES on success, #MHD_NO if there was an error processing the data
 */
static int
post_process_urlencoded (struct MHD_PostProcessor *pp,
                         const char *post_data,
			 size_t post_data_len)
{
  size_t equals;
  size_t amper;
  size_t poff;
  size_t xoff;
  size_t delta;
  int end_of_value_found;
  char *buf;
  char xbuf[XBUF_SIZE + 1];

  buf = (char *) &pp[1];
  poff = 0;
  while (poff < post_data_len)
    {
      switch (pp->state)
        {
        case PP_Error:
          return MHD_NO;
        case PP_Done:
          /* did not expect to receive more data */
          pp->state = PP_Error;
          return MHD_NO;
        case PP_Init:
          equals = 0;
          while ((equals + poff < post_data_len) &&
                 (post_data[equals + poff] != '='))
            equals++;
          if (equals + pp->buffer_pos > pp->buffer_size)
            {
              pp->state = PP_Error;     /* out of memory */
              return MHD_NO;
            }
          memcpy (&buf[pp->buffer_pos], &post_data[poff], equals);
          pp->buffer_pos += equals;
          if (equals + poff == post_data_len)
            return MHD_YES;     /* no '=' yet */
          buf[pp->buffer_pos] = '\0';   /* 0-terminate key */
          pp->buffer_pos = 0;   /* reset for next key */
	  MHD_unescape_plus (buf);
          MHD_http_unescape (buf);
          poff += equals + 1;
          pp->state = PP_ProcessValue;
          pp->value_offset = 0;
          break;
        case PP_ProcessValue:
          /* obtain rest of value from previous iteration */
          memcpy (xbuf, pp->xbuf, pp->xbuf_pos);
          xoff = pp->xbuf_pos;
          pp->xbuf_pos = 0;

          /* find last position in input buffer that is part of the value */
          amper = 0;
          while ((amper + poff < post_data_len) &&
                 (amper < XBUF_SIZE) &&
                 (post_data[amper + poff] != '&') &&
                 (post_data[amper + poff] != '\n') &&
                 (post_data[amper + poff] != '\r'))
            amper++;
          end_of_value_found = ((amper + poff < post_data_len) &&
                                ((post_data[amper + poff] == '&') ||
                                 (post_data[amper + poff] == '\n') ||
                                 (post_data[amper + poff] == '\r')));
          /* compute delta, the maximum number of bytes that we will be able to
             process right now (either amper-limited of xbuf-size limited) */
          delta = amper;
          if (delta > XBUF_SIZE - xoff)
            delta = XBUF_SIZE - xoff;

          /* move input into processing buffer */
          memcpy (&xbuf[xoff], &post_data[poff], delta);
          xoff += delta;
          poff += delta;

          /* find if escape sequence is at the end of the processing buffer;
             if so, exclude those from processing (reduce delta to point at
             end of processed region) */
          delta = xoff;
          if ((delta > 0) &&
              ('%' == xbuf[delta - 1]))
            delta--;
          else if ((delta > 1) &&
                   ('%' == xbuf[delta - 2]))
            delta -= 2;

          /* if we have an incomplete escape sequence, save it to
             pp->xbuf for later */
          if (delta < xoff)
            {
              memcpy (pp->xbuf,
                      &xbuf[delta],
                      xoff - delta);
              pp->xbuf_pos = xoff - delta;
              xoff = delta;
            }

          /* If we have nothing to do (delta == 0) and
             not just because the value is empty (are
             waiting for more data), go for next iteration */
          if ( (0 == xoff) &&
               (poff == post_data_len))
            continue;

          /* unescape */
          xbuf[xoff] = '\0';    /* 0-terminate in preparation */
	  MHD_unescape_plus (xbuf);
          xoff = MHD_http_unescape (xbuf);
          /* finally: call application! */
	  pp->must_ikvi = MHD_NO;
          if (MHD_NO == pp->ikvi (pp->cls,
                                  MHD_POSTDATA_KIND,
                                  (const char *) &pp[1],    /* key */
                                  NULL,
                                  NULL,
                                  NULL,
                                  xbuf,
                                  pp->value_offset,
                                  xoff))
            {
              pp->state = PP_Error;
              return MHD_NO;
            }
          pp->value_offset += xoff;

          /* are we done with the value? */
          if (end_of_value_found)
            {
              /* we found the end of the value! */
              if ( ('\n' == post_data[poff]) ||
                   ('\r' == post_data[poff]) )
                {
                  pp->state = PP_ExpectNewLine;
                }
              else if ('&' == post_data[poff])
                {
                  poff++;       /* skip '&' */
                  pp->state = PP_Init;
                }
            }
          break;
        case PP_ExpectNewLine:
          if ( ('\n' == post_data[poff]) ||
               ('\r' == post_data[poff]) )
            {
              poff++;
              /* we are done, report error if we receive any more... */
              pp->state = PP_Done;
              return MHD_YES;
            }
          return MHD_NO;
        default:
          mhd_panic (mhd_panic_cls,
                     __FILE__,
                     __LINE__,
                     NULL);          /* should never happen! */
        }
    }
  return MHD_YES;
}


/**
 * If the given line matches the prefix, strdup the
 * rest of the line into the suffix ptr.
 *
 * @param prefix prefix to match
 * @param line line to match prefix in
 * @param suffix set to a copy of the rest of the line, starting at the end of the match
 * @return #MHD_YES if there was a match, #MHD_NO if not
 */
static int
try_match_header (const char *prefix,
                  char *line,
                  char **suffix)
{
  if (NULL != *suffix)
    return MHD_NO;
  while (0 != *line)
    {
      if (MHD_str_equal_caseless_n_ (prefix,
                                     line,
                                     strlen (prefix)))
        {
          *suffix = strdup (&line[strlen (prefix)]);
          return MHD_YES;
        }
      ++line;
    }
  return MHD_NO;
}


/**
 *
 * @param pp post processor context
 * @param boundary boundary to look for
 * @param blen number of bytes in boundary
 * @param ioffptr set to the end of the boundary if found,
 *                otherwise incremented by one (FIXME: quirky API!)
 * @param next_state state to which we should advance the post processor
 *                   if the boundary is found
 * @param next_dash_state dash_state to which we should advance the
 *                   post processor if the boundary is found
 * @return #MHD_NO if the boundary is not found, #MHD_YES if we did find it
 */
static int
find_boundary (struct MHD_PostProcessor *pp,
               const char *boundary,
               size_t blen,
               size_t *ioffptr,
               enum PP_State next_state,
               enum PP_State next_dash_state)
{
  char *buf = (char *) &pp[1];
  const char *dash;

  if (pp->buffer_pos < 2 + blen)
    {
      if (pp->buffer_pos == pp->buffer_size)
        pp->state = PP_Error;   /* out of memory */
      /* ++(*ioffptr); */
      return MHD_NO;            /* not enough data */
    }
  if ( (0 != memcmp ("--",
                     buf,
                     2)) ||
       (0 != memcmp (&buf[2],
                     boundary,
                     blen)))
    {
      if (pp->state != PP_Init)
        {
          /* garbage not allowed */
          pp->state = PP_Error;
        }
      else
        {
          /* skip over garbage (RFC 2046, 5.1.1) */
          dash = memchr (buf,
                         '-',
                         pp->buffer_pos);
          if (NULL == dash)
            (*ioffptr) += pp->buffer_pos; /* skip entire buffer */
          else
            if (dash == buf)
              (*ioffptr)++; /* at least skip one byte */
            else
              (*ioffptr) += dash - buf; /* skip to first possible boundary */
        }
      return MHD_NO;            /* expected boundary */
    }
  /* remove boundary from buffer */
  (*ioffptr) += 2 + blen;
  /* next: start with headers */
  pp->skip_rn = RN_Dash;
  pp->state = next_state;
  pp->dash_state = next_dash_state;
  return MHD_YES;
}


/**
 * In buf, there maybe an expression '$key="$value"'.  If that is the
 * case, copy a copy of $value to destination.
 *
 * If destination is already non-NULL, do nothing.
 */
static void
try_get_value (const char *buf,
	       const char *key,
	       char **destination)
{
  const char *spos;
  const char *bpos;
  const char *endv;
  size_t klen;
  size_t vlen;

  if (NULL != *destination)
    return;
  bpos = buf;
  klen = strlen (key);
  while (NULL != (spos = strstr (bpos, key)))
    {
      if ( (spos[klen] != '=') ||
           ( (spos != buf) &&
             (spos[-1] != ' ') ) )
        {
          /* no match */
          bpos = spos + 1;
          continue;
        }
      if (spos[klen + 1] != '"')
        return;                 /* not quoted */
      if (NULL == (endv = strchr (&spos[klen + 2],
                                  '\"')))
        return;                 /* no end-quote */
      vlen = endv - spos - klen - 1;
      *destination = malloc (vlen);
      if (NULL == *destination)
        return;                 /* out of memory */
      (*destination)[vlen - 1] = '\0';
      memcpy (*destination,
              &spos[klen + 2],
              vlen - 1);
      return;                   /* success */
    }
}


/**
 * Go over the headers of the part and update
 * the fields in "pp" according to what we find.
 * If we are at the end of the headers (as indicated
 * by an empty line), transition into next_state.
 *
 * @param pp post processor context
 * @param ioffptr set to how many bytes have been
 *                processed
 * @param next_state state to which the post processor should
 *                be advanced if we find the end of the headers
 * @return #MHD_YES if we can continue processing,
 *         #MHD_NO on error or if we do not have
 *                enough data yet
 */
static int
process_multipart_headers (struct MHD_PostProcessor *pp,
                           size_t *ioffptr,
                           enum PP_State next_state)
{
  char *buf = (char *) &pp[1];
  size_t newline;

  newline = 0;
  while ( (newline < pp->buffer_pos) &&
          (buf[newline] != '\r') &&
          (buf[newline] != '\n') )
    newline++;
  if (newline == pp->buffer_size)
    {
      pp->state = PP_Error;
      return MHD_NO;            /* out of memory */
    }
  if (newline == pp->buffer_pos)
    return MHD_NO;              /* will need more data */
  if (0 == newline)
    {
      /* empty line - end of headers */
      pp->skip_rn = RN_Full;
      pp->state = next_state;
      return MHD_YES;
    }
  /* got an actual header */
  if (buf[newline] == '\r')
    pp->skip_rn = RN_OptN;
  buf[newline] = '\0';
  if (MHD_str_equal_caseless_n_ ("Content-disposition: ",
                                 buf,
                                 MHD_STATICSTR_LEN_ ("Content-disposition: ")))
    {
      try_get_value (&buf[MHD_STATICSTR_LEN_ ("Content-disposition: ")],
                     "name",
                     &pp->content_name);
      try_get_value (&buf[MHD_STATICSTR_LEN_ ("Content-disposition: ")],
                     "filename",
                     &pp->content_filename);
    }
  else
    {
      try_match_header ("Content-type: ",
                        buf,
                        &pp->content_type);
      try_match_header ("Content-Transfer-Encoding: ",
                        buf,
                        &pp->content_transfer_encoding);
    }
  (*ioffptr) += newline + 1;
  return MHD_YES;
}


/**
 * We have the value until we hit the given boundary;
 * process accordingly.
 *
 * @param pp post processor context
 * @param ioffptr incremented based on the number of bytes processed
 * @param boundary the boundary to look for
 * @param blen strlen(boundary)
 * @param next_state what state to go into after the
 *        boundary was found
 * @param next_dash_state state to go into if the next
 *        boundary ends with "--"
 * @return #MHD_YES if we can continue processing,
 *         #MHD_NO on error or if we do not have
 *                enough data yet
 */
static int
process_value_to_boundary (struct MHD_PostProcessor *pp,
                           size_t *ioffptr,
                           const char *boundary,
                           size_t blen,
                           enum PP_State next_state,
                           enum PP_State next_dash_state)
{
  char *buf = (char *) &pp[1];
  size_t newline;
  const char *r;

  /* all data in buf until the boundary
     (\r\n--+boundary) is part of the value */
  newline = 0;
  while (1)
    {
      while (newline + 4 < pp->buffer_pos)
        {
          r = memchr (&buf[newline],
                      '\r',
                      pp->buffer_pos - newline - 4);
          if (NULL == r)
          {
            newline = pp->buffer_pos - 4;
            break;
          }
          newline = r - buf;
          if (0 == memcmp ("\r\n--",
                           &buf[newline],
                           4))
            break;
          newline++;
        }
      if (newline + pp->blen + 4 <= pp->buffer_pos)
        {
          /* can check boundary */
          if (0 != memcmp (&buf[newline + 4],
                           boundary,
                           pp->blen))
            {
              /* no boundary, "\r\n--" is part of content, skip */
              newline += 4;
              continue;
            }
          else
            {
              /* boundary found, process until newline then
                 skip boundary and go back to init */
              pp->skip_rn = RN_Dash;
              pp->state = next_state;
              pp->dash_state = next_dash_state;
              (*ioffptr) += pp->blen + 4;       /* skip boundary as well */
              buf[newline] = '\0';
              break;
            }
        }
      else
        {
          /* cannot check for boundary, process content that
             we have and check again later; except, if we have
             no content, abort (out of memory) */
          if ( (0 == newline) &&
               (pp->buffer_pos == pp->buffer_size) )
            {
              pp->state = PP_Error;
              return MHD_NO;
            }
          break;
        }
    }
  /* newline is either at beginning of boundary or
     at least at the last character that we are sure
     is not part of the boundary */
  if ( ( (MHD_YES == pp->must_ikvi) ||
	 (0 != newline) ) &&
       (MHD_NO == pp->ikvi (pp->cls,
			    MHD_POSTDATA_KIND,
			    pp->content_name,
			    pp->content_filename,
			    pp->content_type,
			    pp->content_transfer_encoding,
			    buf,
                            pp->value_offset,
                            newline)) )
    {
      pp->state = PP_Error;
      return MHD_NO;
    }
  pp->must_ikvi = MHD_NO;
  pp->value_offset += newline;
  (*ioffptr) += newline;
  return MHD_YES;
}


/**
 *
 * @param pp post processor context
 */
static void
free_unmarked (struct MHD_PostProcessor *pp)
{
  if ( (NULL != pp->content_name) &&
       (0 == (pp->have & NE_content_name)) )
    {
      free (pp->content_name);
      pp->content_name = NULL;
    }
  if ( (NULL != pp->content_type) &&
       (0 == (pp->have & NE_content_type)) )
    {
      free (pp->content_type);
      pp->content_type = NULL;
    }
  if ( (NULL != pp->content_filename) &&
       (0 == (pp->have & NE_content_filename)) )
    {
      free (pp->content_filename);
      pp->content_filename = NULL;
    }
  if ( (NULL != pp->content_transfer_encoding) &&
       (0 == (pp->have & NE_content_transfer_encoding)) )
    {
      free (pp->content_transfer_encoding);
      pp->content_transfer_encoding = NULL;
    }
}


/**
 * Decode multipart POST data.
 *
 * @param pp post processor context
 * @param post_data data to decode
 * @param post_data_len number of bytes in @a post_data
 * @return #MHD_NO on error,
 */
static int
post_process_multipart (struct MHD_PostProcessor *pp,
                        const char *post_data,
			size_t post_data_len)
{
  char *buf;
  size_t max;
  size_t ioff;
  size_t poff;
  int state_changed;

  buf = (char *) &pp[1];
  ioff = 0;
  poff = 0;
  state_changed = 1;
  while ( (poff < post_data_len) ||
          ( (pp->buffer_pos > 0) &&
            (0 != state_changed) ) )
    {
      /* first, move as much input data
         as possible to our internal buffer */
      max = pp->buffer_size - pp->buffer_pos;
      if (max > post_data_len - poff)
        max = post_data_len - poff;
      memcpy (&buf[pp->buffer_pos],
              &post_data[poff],
              max);
      poff += max;
      pp->buffer_pos += max;
      if ( (0 == max) &&
           (0 == state_changed) &&
           (poff < post_data_len) )
        {
          pp->state = PP_Error;
          return MHD_NO;        /* out of memory */
        }
      state_changed = 0;

      /* first state machine for '\r'-'\n' and '--' handling */
      switch (pp->skip_rn)
        {
        case RN_Inactive:
          break;
        case RN_OptN:
          if (buf[0] == '\n')
            {
              ioff++;
              pp->skip_rn = RN_Inactive;
              goto AGAIN;
            }
          /* fall-through! */
        case RN_Dash:
          if (buf[0] == '-')
            {
              ioff++;
              pp->skip_rn = RN_Dash2;
              goto AGAIN;
            }
          pp->skip_rn = RN_Full;
          /* fall-through! */
        case RN_Full:
          if (buf[0] == '\r')
            {
              if ( (pp->buffer_pos > 1) &&
                   ('\n' == buf[1]) )
                {
                  pp->skip_rn = RN_Inactive;
                  ioff += 2;
                }
              else
                {
                  pp->skip_rn = RN_OptN;
                  ioff++;
                }
              goto AGAIN;
            }
          if (buf[0] == '\n')
            {
              ioff++;
              pp->skip_rn = RN_Inactive;
              goto AGAIN;
            }
          pp->skip_rn = RN_Inactive;
          pp->state = PP_Error;
          return MHD_NO;        /* no '\r\n' */
        case RN_Dash2:
          if (buf[0] == '-')
            {
              ioff++;
              pp->skip_rn = RN_Full;
              pp->state = pp->dash_state;
              goto AGAIN;
            }
          pp->state = PP_Error;
          break;
        }

      /* main state engine */
      switch (pp->state)
        {
        case PP_Error:
          return MHD_NO;
        case PP_Done:
          /* did not expect to receive more data */
          pp->state = PP_Error;
          return MHD_NO;
        case PP_Init:
          /**
           * Per RFC2046 5.1.1 NOTE TO IMPLEMENTORS, consume anything
           * prior to the first multipart boundary:
           *
           * > There appears to be room for additional information prior
           * > to the first boundary delimiter line and following the
           * > final boundary delimiter line.  These areas should
           * > generally be left blank, and implementations must ignore
           * > anything that appears before the first boundary delimiter
           * > line or after the last one.
           */
          (void) find_boundary (pp,
				pp->boundary,
				pp->blen,
				&ioff,
				PP_ProcessEntryHeaders,
                                PP_Done);
          break;
        case PP_NextBoundary:
          if (MHD_NO == find_boundary (pp,
                                       pp->boundary,
                                       pp->blen,
                                       &ioff,
                                       PP_ProcessEntryHeaders,
                                       PP_Done))
            {
              if (pp->state == PP_Error)
                return MHD_NO;
              goto END;
            }
          break;
        case PP_ProcessEntryHeaders:
	  pp->must_ikvi = MHD_YES;
          if (MHD_NO ==
              process_multipart_headers (pp,
                                         &ioff,
                                         PP_PerformCheckMultipart))
            {
              if (pp->state == PP_Error)
                return MHD_NO;
              else
                goto END;
            }
          state_changed = 1;
          break;
        case PP_PerformCheckMultipart:
          if ( (NULL != pp->content_type) &&
               (MHD_str_equal_caseless_n_ (pp->content_type,
                                           "multipart/mixed",
                                           MHD_STATICSTR_LEN_ ("multipart/mixed"))))
            {
              pp->nested_boundary = strstr (pp->content_type,
                                            "boundary=");
              if (NULL == pp->nested_boundary)
                {
                  pp->state = PP_Error;
                  return MHD_NO;
                }
              pp->nested_boundary =
                strdup (&pp->nested_boundary[MHD_STATICSTR_LEN_ ("boundary=")]);
              if (NULL == pp->nested_boundary)
                {
                  /* out of memory */
                  pp->state = PP_Error;
                  return MHD_NO;
                }
              /* free old content type, we will need that field
                 for the content type of the nested elements */
              free (pp->content_type);
              pp->content_type = NULL;
              pp->nlen = strlen (pp->nested_boundary);
              pp->state = PP_Nested_Init;
              state_changed = 1;
              break;
            }
          pp->state = PP_ProcessValueToBoundary;
          pp->value_offset = 0;
          state_changed = 1;
          break;
        case PP_ProcessValueToBoundary:
          if (MHD_NO == process_value_to_boundary (pp,
                                                   &ioff,
                                                   pp->boundary,
                                                   pp->blen,
                                                   PP_PerformCleanup,
                                                   PP_Done))
            {
              if (pp->state == PP_Error)
                return MHD_NO;
              break;
            }
          break;
        case PP_PerformCleanup:
          /* clean up state of one multipart form-data element! */
          pp->have = NE_none;
          free_unmarked (pp);
          if (NULL != pp->nested_boundary)
            {
              free (pp->nested_boundary);
              pp->nested_boundary = NULL;
            }
          pp->state = PP_ProcessEntryHeaders;
          state_changed = 1;
          break;
        case PP_Nested_Init:
          if (NULL == pp->nested_boundary)
            {
              pp->state = PP_Error;
              return MHD_NO;
            }
          if (MHD_NO == find_boundary (pp,
                                       pp->nested_boundary,
                                       pp->nlen,
                                       &ioff,
                                       PP_Nested_PerformMarking,
                                       PP_NextBoundary /* or PP_Error? */ ))
            {
              if (pp->state == PP_Error)
                return MHD_NO;
              goto END;
            }
          break;
        case PP_Nested_PerformMarking:
          /* remember what headers were given
             globally */
          pp->have = NE_none;
          if (NULL != pp->content_name)
            pp->have |= NE_content_name;
          if (NULL != pp->content_type)
            pp->have |= NE_content_type;
          if (NULL != pp->content_filename)
            pp->have |= NE_content_filename;
          if (NULL != pp->content_transfer_encoding)
            pp->have |= NE_content_transfer_encoding;
          pp->state = PP_Nested_ProcessEntryHeaders;
          state_changed = 1;
          break;
        case PP_Nested_ProcessEntryHeaders:
          pp->value_offset = 0;
          if (MHD_NO ==
              process_multipart_headers (pp,
                                         &ioff,
                                         PP_Nested_ProcessValueToBoundary))
            {
              if (pp->state == PP_Error)
                return MHD_NO;
              else
                goto END;
            }
          state_changed = 1;
          break;
        case PP_Nested_ProcessValueToBoundary:
          if (MHD_NO == process_value_to_boundary (pp,
                                                   &ioff,
                                                   pp->nested_boundary,
                                                   pp->nlen,
                                                   PP_Nested_PerformCleanup,
                                                   PP_NextBoundary))
            {
              if (pp->state == PP_Error)
                return MHD_NO;
              break;
            }
          break;
        case PP_Nested_PerformCleanup:
          free_unmarked (pp);
          pp->state = PP_Nested_ProcessEntryHeaders;
          state_changed = 1;
          break;
        default:
          mhd_panic (mhd_panic_cls,
                     __FILE__,
                     __LINE__,
                     NULL);          /* should never happen! */
        }
    AGAIN:
      if (ioff > 0)
        {
          memmove (buf,
                   &buf[ioff],
                   pp->buffer_pos - ioff);
          pp->buffer_pos -= ioff;
          ioff = 0;
          state_changed = 1;
        }
    }
END:
  if (0 != ioff)
    {
      memmove (buf,
               &buf[ioff],
               pp->buffer_pos - ioff);
      pp->buffer_pos -= ioff;
    }
  if (poff < post_data_len)
    {
      pp->state = PP_Error;
      return MHD_NO;            /* serious error */
    }
  return MHD_YES;
}


/**
 * Parse and process POST data.  Call this function when POST data is
 * available (usually during an #MHD_AccessHandlerCallback) with the
 * "upload_data" and "upload_data_size".  Whenever possible, this will
 * then cause calls to the #MHD_PostDataIterator.
 *
 * @param pp the post processor
 * @param post_data @a post_data_len bytes of POST data
 * @param post_data_len length of @a post_data
 * @return #MHD_YES on success, #MHD_NO on error
 *         (out-of-memory, iterator aborted, parse error)
 * @ingroup request
 */
int
MHD_post_process (struct MHD_PostProcessor *pp,
                  const char *post_data,
                  size_t post_data_len)
{
  if (0 == post_data_len)
    return MHD_YES;
  if (NULL == pp)
    return MHD_NO;
  if (MHD_str_equal_caseless_n_ (MHD_HTTP_POST_ENCODING_FORM_URLENCODED,
                                 pp->encoding,
                                 MHD_STATICSTR_LEN_(MHD_HTTP_POST_ENCODING_FORM_URLENCODED)))
    return post_process_urlencoded (pp,
                                    post_data,
                                    post_data_len);
  if (MHD_str_equal_caseless_n_ (MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA,
                                 pp->encoding,
                                 MHD_STATICSTR_LEN_ (MHD_HTTP_POST_ENCODING_MULTIPART_FORMDATA)))
    return post_process_multipart (pp,
                                   post_data,
                                   post_data_len);
  /* this should never be reached */
  return MHD_NO;
}


/**
 * Release PostProcessor resources.
 *
 * @param pp post processor context to destroy
 * @return #MHD_YES if processing completed nicely,
 *         #MHD_NO if there were spurious characters / formatting
 *                problems; it is common to ignore the return
 *                value of this function
 * @ingroup request
 */
int
MHD_destroy_post_processor (struct MHD_PostProcessor *pp)
{
  int ret;

  if (NULL == pp)
    return MHD_YES;
  if (PP_ProcessValue == pp->state)
  {
    /* key without terminated value left at the end of the
       buffer; fake receiving a termination character to
       ensure it is also processed */
    post_process_urlencoded (pp,
                             "\n",
                             1);
  }
  /* These internal strings need cleaning up since
     the post-processing may have been interrupted
     at any stage */
  if ( (pp->xbuf_pos > 0) ||
       ( (pp->state != PP_Done) &&
         (pp->state != PP_ExpectNewLine) ) )
    ret = MHD_NO;
  else
    ret = MHD_YES;
  pp->have = NE_none;
  free_unmarked (pp);
  if (NULL != pp->nested_boundary)
    free (pp->nested_boundary);
  free (pp);
  return ret;
}

/* end of postprocessor.c */
