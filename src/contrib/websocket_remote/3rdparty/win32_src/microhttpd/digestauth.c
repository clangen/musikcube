/*
     This file is part of libmicrohttpd
     Copyright (C) 2010, 2011, 2012, 2015 Daniel Pittman and Christian Grothoff

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
 * @file digestauth.c
 * @brief Implements HTTP digest authentication
 * @author Amr Ali
 * @author Matthieu Speder
 */
#include "platform.h"
#include <limits.h>
#include "internal.h"
#include "md5.h"
#include "mhd_mono_clock.h"
#include "mhd_str.h"
#include "mhd_compat.h"

#if defined(_WIN32) && defined(MHD_W32_MUTEX_)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif /* !WIN32_LEAN_AND_MEAN */
#include <windows.h>
#endif /* _WIN32 && MHD_W32_MUTEX_ */

#define HASH_MD5_HEX_LEN (2 * MD5_DIGEST_SIZE)
/* 32 bit value is 4 bytes */
#define TIMESTAMP_BIN_SIZE 4
#define TIMESTAMP_HEX_LEN (2 * TIMESTAMP_BIN_SIZE)

/* Standard server nonce length, not including terminating null */
#define NONCE_STD_LEN (HASH_MD5_HEX_LEN + TIMESTAMP_HEX_LEN)

/**
 * Beginning string for any valid Digest authentication header.
 */
#define _BASE		"Digest "

/**
 * Maximum length of a username for digest authentication.
 */
#define MAX_USERNAME_LENGTH 128

/**
 * Maximum length of a realm for digest authentication.
 */
#define MAX_REALM_LENGTH 256

/**
 * Maximum length of the response in digest authentication.
 */
#define MAX_AUTH_RESPONSE_LENGTH 128


/**
 * convert bin to hex
 *
 * @param bin binary data
 * @param len number of bytes in bin
 * @param hex pointer to len*2+1 bytes
 */
static void
cvthex (const unsigned char *bin,
	size_t len,
	char *hex)
{
  size_t i;
  unsigned int j;

  for (i = 0; i < len; ++i)
    {
      j = (bin[i] >> 4) & 0x0f;
      hex[i * 2] = (char)((j <= 9) ? (j + '0') : (j - 10 + 'a'));
      j = bin[i] & 0x0f;
      hex[i * 2 + 1] = (char)((j <= 9) ? (j + '0') : (j - 10 + 'a'));
    }
  hex[len * 2] = '\0';
}


/**
 * calculate H(A1) as per RFC2617 spec and store the
 * result in 'sessionkey'.
 *
 * @param alg The hash algorithm used, can be "md5" or "md5-sess"
 * @param username A `char *' pointer to the username value
 * @param realm A `char *' pointer to the realm value
 * @param password A `char *' pointer to the password value
 * @param nonce A `char *' pointer to the nonce value
 * @param cnonce A `char *' pointer to the cnonce value
 * @param sessionkey pointer to buffer of HASH_MD5_HEX_LEN+1 bytes
 */
static void
digest_calc_ha1 (const char *alg,
		 const char *username,
		 const char *realm,
		 const char *password,
		 const char *nonce,
		 const char *cnonce,
		 char sessionkey[HASH_MD5_HEX_LEN + 1])
{
  struct MD5Context md5;
  unsigned char ha1[MD5_DIGEST_SIZE];

  MD5Init (&md5);
  MD5Update (&md5,
             (const unsigned char *) username,
             strlen (username));
  MD5Update (&md5,
             (const unsigned char *) ":",
             1);
  MD5Update (&md5,
             (const unsigned char *) realm,
             strlen (realm));
  MD5Update (&md5,
             (const unsigned char *) ":",
             1);
  MD5Update (&md5,
             (const unsigned char *) password,
             strlen (password));
  MD5Final (ha1,
            &md5);
  if (MHD_str_equal_caseless_(alg,
                              "md5-sess"))
    {
      MD5Init (&md5);
      MD5Update (&md5,
                 (const unsigned char *) ha1,
                 sizeof (ha1));
      MD5Update (&md5,
                 (const unsigned char *) ":",
                 1);
      MD5Update (&md5,
                 (const unsigned char *) nonce,
                 strlen (nonce));
      MD5Update (&md5,
                 (const unsigned char *) ":",
                 1);
      MD5Update (&md5,
                 (const unsigned char *) cnonce,
                 strlen (cnonce));
      MD5Final (ha1,
                &md5);
    }
  cvthex (ha1,
          sizeof (ha1),
          sessionkey);
}


/**
 * Calculate request-digest/response-digest as per RFC2617 spec
 *
 * @param ha1 H(A1)
 * @param nonce nonce from server
 * @param noncecount 8 hex digits
 * @param cnonce client nonce
 * @param qop qop-value: "", "auth" or "auth-int"
 * @param method method from request
 * @param uri requested URL
 * @param hentity H(entity body) if qop="auth-int"
 * @param response request-digest or response-digest
 */
static void
digest_calc_response (const char ha1[HASH_MD5_HEX_LEN + 1],
		      const char *nonce,
		      const char *noncecount,
		      const char *cnonce,
		      const char *qop,
		      const char *method,
		      const char *uri,
		      const char *hentity,
		      char response[HASH_MD5_HEX_LEN + 1])
{
  struct MD5Context md5;
  unsigned char ha2[MD5_DIGEST_SIZE];
  unsigned char resphash[MD5_DIGEST_SIZE];
  char ha2hex[HASH_MD5_HEX_LEN + 1];

  MD5Init (&md5);
  MD5Update (&md5,
             (const unsigned char *) method,
             strlen (method));
  MD5Update (&md5,
             (const unsigned char *) ":",
             1);
  MD5Update (&md5,
             (const unsigned char *) uri,
             strlen (uri));
#if 0
  if (0 == strcasecmp(qop,
                      "auth-int"))
    {
      /* This is dead code since the rest of this module does
	 not support auth-int. */
      MD5Update (&md5,
                 ":",
                 1);
      if (NULL != hentity)
	MD5Update (&md5,
                   hentity,
                   strlen (hentity));
    }
#endif
  MD5Final (ha2,
            &md5);
  cvthex (ha2,
          MD5_DIGEST_SIZE,
          ha2hex);
  MD5Init (&md5);
  /* calculate response */
  MD5Update (&md5,
             (const unsigned char *) ha1,
             HASH_MD5_HEX_LEN);
  MD5Update (&md5,
             (const unsigned char *) ":",
             1);
  MD5Update (&md5,
             (const unsigned char *) nonce,
             strlen (nonce));
  MD5Update (&md5,
             (const unsigned char*) ":",
             1);
  if ('\0' != *qop)
    {
      MD5Update (&md5,
                 (const unsigned char *) noncecount,
                 strlen (noncecount));
      MD5Update (&md5,
                 (const unsigned char *) ":",
                 1);
      MD5Update (&md5,
                 (const unsigned char *) cnonce,
                 strlen (cnonce));
      MD5Update (&md5,
                 (const unsigned char *) ":",
                 1);
      MD5Update (&md5,
                 (const unsigned char *) qop,
                 strlen (qop));
      MD5Update (&md5,
                 (const unsigned char *) ":",
                 1);
    }
  MD5Update (&md5,
             (const unsigned char *) ha2hex,
             HASH_MD5_HEX_LEN);
  MD5Final (resphash,
            &md5);
  cvthex (resphash,
          sizeof(resphash),
          response);
}


/**
 * Lookup subvalue off of the HTTP Authorization header.
 *
 * A description of the input format for 'data' is at
 * http://en.wikipedia.org/wiki/Digest_access_authentication
 *
 *
 * @param dest where to store the result (possibly truncated if
 *             the buffer is not big enough).
 * @param size size of dest
 * @param data pointer to the Authorization header
 * @param key key to look up in data
 * @return size of the located value, 0 if otherwise
 */
static size_t
lookup_sub_value (char *dest,
		  size_t size,
		  const char *data,
		  const char *key)
{
  size_t keylen;
  size_t len;
  const char *ptr;
  const char *eq;
  const char *q1;
  const char *q2;
  const char *qn;

  if (0 == size)
    return 0;
  keylen = strlen (key);
  ptr = data;
  while ('\0' != *ptr)
    {
      if (NULL == (eq = strchr (ptr,
                                '=')))
	return 0;
      q1 = eq + 1;
      while (' ' == *q1)
	q1++;
      if ('\"' != *q1)
	{
	  q2 = strchr (q1,
                       ',');
	  qn = q2;
	}
      else
	{
	  q1++;
	  q2 = strchr (q1,
                       '\"');
	  if (NULL == q2)
	    return 0; /* end quote not found */
	  qn = q2 + 1;
	}
      if ( (MHD_str_equal_caseless_n_(ptr,
                                      key,
                                      keylen)) &&
	   (eq == &ptr[keylen]) )
	{
	  if (NULL == q2)
	    {
	      len = strlen (q1) + 1;
	      if (size > len)
		size = len;
	      size--;
	      strncpy (dest,
		       q1,
		       size);
	      dest[size] = '\0';
	      return size;
	    }
	  else
	    {
	      if (size > (size_t) ((q2 - q1) + 1))
		size = (q2 - q1) + 1;
	      size--;
	      memcpy (dest,
		      q1,
		      size);
	      dest[size] = '\0';
	      return size;
	    }
	}
      if (NULL == qn)
	return 0;
      ptr = strchr (qn,
                    ',');
      if (NULL == ptr)
	return 0;
      ptr++;
      while (' ' == *ptr)
	ptr++;
    }
  return 0;
}


/**
 * Check nonce-nc map array with either new nonce counter
 * or a whole new nonce.
 *
 * @param connection The MHD connection structure
 * @param nonce A pointer that referenced a zero-terminated array of nonce
 * @param nc The nonce counter, zero to add the nonce to the array
 * @return #MHD_YES if successful, #MHD_NO if invalid (or we have no NC array)
 */
static int
check_nonce_nc (struct MHD_Connection *connection,
		const char *nonce,
		uint64_t nc)
{
  struct MHD_Daemon *daemon = connection->daemon;
  struct MHD_NonceNc *nn;
  uint32_t off;
  uint32_t mod;
  const char *np;

  if (MAX_NONCE_LENGTH <= strlen (nonce))
    return MHD_NO; /* This should be impossible, but static analysis
                      tools have a hard time with it *and* this also
                      protects against unsafe modifications that may
                      happen in the future... */
  mod = daemon->nonce_nc_size;
  if (0 == mod)
    return MHD_NO; /* no array! */
  /* super-fast xor-based "hash" function for HT lookup in nonce array */
  off = 0;
  np = nonce;
  while ('\0' != *np)
    {
      off = (off << 8) | (*np ^ (off >> 24));
      np++;
    }
  off = off % mod;
  /*
   * Look for the nonce, if it does exist and its corresponding
   * nonce counter is less than the current nonce counter by 1,
   * then only increase the nonce counter by one.
   */
  nn = &daemon->nnc[off];
  MHD_mutex_lock_chk_ (&daemon->nnc_lock);
  if (0 == nc)
    {
      /* Fresh nonce, reinitialize array */
      strcpy (nn->nonce,
              nonce);
      nn->nc = 0;
      nn->nmask = 0;
      MHD_mutex_unlock_chk_ (&daemon->nnc_lock);
      return MHD_YES;
    }
  /* Note that we use 64 here, as we do not store the
     bit for 'nn->nc' itself in 'nn->nmask' */
  if ( (nc < nn->nc) &&
       (nc + 64 > nc /* checking for overflow */) &&
       (nc + 64 >= nn->nc) &&
       (0 == ((1LLU << (nn->nc - nc - 1)) & nn->nmask)) )
    {
      /* Out-of-order nonce, but within 64-bit bitmask, set bit */
      nn->nmask |= (1LLU < (nn->nc - nc - 1));
      MHD_mutex_unlock_chk_ (&daemon->nnc_lock);
      return MHD_YES;
    }

  if ( (nc <= nn->nc) ||
       (0 != strcmp (nn->nonce,
                     nonce)) )
    {
      /* Nonce does not match, fail */
      MHD_mutex_unlock_chk_ (&daemon->nnc_lock);
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Stale nonce received.  If this happens a lot, you should probably increase the size of the nonce array.\n"));
#endif
      return MHD_NO;
    }
  /* Nonce is larger, shift bitmask and bump limit */
  if (64 > nc - nn->nc)
    nn->nmask <<= (nc - nn->nc); /* small jump, less than mask width */
  else
    nn->nmask = 0; /* big jump, unset all bits in the mask */
  nn->nc = nc;
  MHD_mutex_unlock_chk_ (&daemon->nnc_lock);
  return MHD_YES;
}


/**
 * Get the username from the authorization header sent by the client
 *
 * @param connection The MHD connection structure
 * @return NULL if no username could be found, a pointer
 * 			to the username if found
 * @ingroup authentication
 */
char *
MHD_digest_auth_get_username(struct MHD_Connection *connection)
{
  size_t len;
  char user[MAX_USERNAME_LENGTH];
  const char *header;

  if (NULL == (header =
               MHD_lookup_connection_value (connection,
                                            MHD_HEADER_KIND,
                                            MHD_HTTP_HEADER_AUTHORIZATION)))
    return NULL;
  if (0 != strncmp (header,
                    _BASE,
                    MHD_STATICSTR_LEN_ (_BASE)))
    return NULL;
  header += MHD_STATICSTR_LEN_ (_BASE);
  if (0 == (len = lookup_sub_value (user,
				    sizeof (user),
				    header,
				    "username")))
    return NULL;
  return strdup (user);
}


/**
 * Calculate the server nonce so that it mitigates replay attacks
 * The current format of the nonce is ...
 * H(timestamp ":" method ":" random ":" uri ":" realm) + Hex(timestamp)
 *
 * @param nonce_time The amount of time in seconds for a nonce to be invalid
 * @param method HTTP method
 * @param rnd A pointer to a character array for the random seed
 * @param rnd_size The size of the random seed array @a rnd
 * @param uri HTTP URI (in MHD, without the arguments ("?k=v")
 * @param realm A string of characters that describes the realm of auth.
 * @param nonce A pointer to a character array for the nonce to put in
 */
static void
calculate_nonce (uint32_t nonce_time,
		 const char *method,
		 const char *rnd,
		 size_t rnd_size,
		 const char *uri,
		 const char *realm,
		 char nonce[NONCE_STD_LEN + 1])
{
  struct MD5Context md5;
  unsigned char timestamp[TIMESTAMP_BIN_SIZE];
  unsigned char tmpnonce[MD5_DIGEST_SIZE];
  char timestamphex[TIMESTAMP_HEX_LEN + 1];

  MD5Init (&md5);
  timestamp[0] = (unsigned char)((nonce_time & 0xff000000) >> 0x18);
  timestamp[1] = (unsigned char)((nonce_time & 0x00ff0000) >> 0x10);
  timestamp[2] = (unsigned char)((nonce_time & 0x0000ff00) >> 0x08);
  timestamp[3] = (unsigned char)((nonce_time & 0x000000ff));
  MD5Update (&md5,
             timestamp,
             sizeof (timestamp));
  MD5Update (&md5,
             (const unsigned char *) ":",
             1);
  MD5Update (&md5,
             (const unsigned char *) method,
             strlen (method));
  MD5Update (&md5,
             (const unsigned char *) ":",
             1);
  if (rnd_size > 0)
    MD5Update (&md5,
               (const unsigned char *) rnd,
               rnd_size);
  MD5Update (&md5,
             (const unsigned char *) ":",
             1);
  MD5Update (&md5,
             (const unsigned char *) uri,
             strlen (uri));
  MD5Update (&md5,
             (const unsigned char *) ":",
             1);
  MD5Update (&md5,
             (const unsigned char *) realm,
             strlen (realm));
  MD5Final (tmpnonce,
            &md5);
  cvthex (tmpnonce,
          sizeof (tmpnonce),
          nonce);
  cvthex (timestamp,
          sizeof (timestamp),
          timestamphex);
  strncat (nonce,
           timestamphex,
           8);
}


/**
 * Test if the given key-value pair is in the headers for the
 * given connection.
 *
 * @param connection the connection
 * @param key the key
 * @param value the value, can be NULL
 * @param kind type of the header
 * @return #MHD_YES if the key-value pair is in the headers,
 *         #MHD_NO if not
 */
static int
test_header (struct MHD_Connection *connection,
	     const char *key,
	     const char *value,
	     enum MHD_ValueKind kind)
{
  struct MHD_HTTP_Header *pos;

  for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    {
      if (kind != pos->kind)
	continue;
      if (0 != strcmp (key,
                       pos->header))
	continue;
      if ( (NULL == value) &&
	   (NULL == pos->value) )
	return MHD_YES;
      if ( (NULL == value) ||
	   (NULL == pos->value) ||
	   (0 != strcmp (value,
                         pos->value)) )
	continue;
      return MHD_YES;
    }
  return MHD_NO;
}


/**
 * Check that the arguments given by the client as part
 * of the authentication header match the arguments we
 * got as part of the HTTP request URI.
 *
 * @param connection connections with headers to compare against
 * @param args argument URI string (after "?" in URI)
 * @return #MHD_YES if the arguments match,
 *         #MHD_NO if not
 */
static int
check_argument_match (struct MHD_Connection *connection,
		      const char *args)
{
  struct MHD_HTTP_Header *pos;
  char *argb;
  unsigned int num_headers;
  int ret;

  argb = strdup (args);
  if (NULL == argb)
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
		_("Failed to allocate memory for copy of URI arguments\n"));
#endif /* HAVE_MESSAGES */
      return MHD_NO;
    }
  ret = MHD_parse_arguments_ (connection,
			      MHD_GET_ARGUMENT_KIND,
			      argb,
			      &test_header,
			      &num_headers);
  free (argb);
  if (MHD_YES != ret)
    return MHD_NO;
  /* also check that the number of headers matches */
  for (pos = connection->headers_received; NULL != pos; pos = pos->next)
    {
      if (MHD_GET_ARGUMENT_KIND != pos->kind)
	continue;
      num_headers--;
    }
  if (0 != num_headers)
    {
      /* argument count mismatch */
      return MHD_NO;
    }
  return MHD_YES;
}


/**
 * Authenticates the authorization header sent by the client
 *
 * @param connection The MHD connection structure
 * @param realm The realm presented to the client
 * @param username The username needs to be authenticated
 * @param password The password used in the authentication
 * @param nonce_timeout The amount of time for a nonce to be
 * 			invalid in seconds
 * @return #MHD_YES if authenticated, #MHD_NO if not,
 * 			#MHD_INVALID_NONCE if nonce is invalid
 * @ingroup authentication
 */
int
MHD_digest_auth_check (struct MHD_Connection *connection,
		       const char *realm,
		       const char *username,
		       const char *password,
		       unsigned int nonce_timeout)
{
  struct MHD_Daemon *daemon = connection->daemon;
  size_t len;
  const char *header;
  char nonce[MAX_NONCE_LENGTH];
  char cnonce[MAX_NONCE_LENGTH];
  char qop[15]; /* auth,auth-int */
  char nc[20];
  char response[MAX_AUTH_RESPONSE_LENGTH];
  const char *hentity = NULL; /* "auth-int" is not supported */
  char ha1[HASH_MD5_HEX_LEN + 1];
  char respexp[HASH_MD5_HEX_LEN + 1];
  char noncehashexp[NONCE_STD_LEN + 1];
  uint32_t nonce_time;
  uint32_t t;
  size_t left; /* number of characters left in 'header' for 'uri' */
  uint64_t nci;

  header = MHD_lookup_connection_value (connection,
					MHD_HEADER_KIND,
					MHD_HTTP_HEADER_AUTHORIZATION);
  if (NULL == header)
    return MHD_NO;
  if (0 != strncmp (header,
                    _BASE,
                    MHD_STATICSTR_LEN_(_BASE)))
    return MHD_NO;
  header += MHD_STATICSTR_LEN_ (_BASE);
  left = strlen (header);

  {
    char un[MAX_USERNAME_LENGTH];

    len = lookup_sub_value (un,
			    sizeof (un),
			    header,
                            "username");
    if ( (0 == len) ||
	 (0 != strcmp (username,
                       un)) )
      return MHD_NO;
    left -= strlen ("username") + len;
  }

  {
    char r[MAX_REALM_LENGTH];

    len = lookup_sub_value (r,
                            sizeof (r),
                            header,
                            "realm");
    if ( (0 == len) ||
	 (0 != strcmp (realm,
                       r)) )
      return MHD_NO;
    left -= strlen ("realm") + len;
  }

  if (0 == (len = lookup_sub_value (nonce,
				    sizeof (nonce),
				    header,
                                    "nonce")))
    return MHD_NO;
  left -= strlen ("nonce") + len;
  if (left > 32 * 1024)
  {
    /* we do not permit URIs longer than 32k, as we want to
       make sure to not blow our stack (or per-connection
       heap memory limit).  Besides, 32k is already insanely
       large, but of course in theory the
       #MHD_OPTION_CONNECTION_MEMORY_LIMIT might be very large
       and would thus permit sending a >32k authorization
       header value. */
    return MHD_NO;
  }
  if (TIMESTAMP_HEX_LEN !=
      MHD_strx_to_uint32_n_ (nonce + len - TIMESTAMP_HEX_LEN,
                             TIMESTAMP_HEX_LEN,
                             &nonce_time))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
                _("Authentication failed, invalid timestamp format.\n"));
#endif
      return MHD_NO;
    }
  t = (uint32_t) MHD_monotonic_sec_counter();
  /*
   * First level vetting for the nonce validity: if the timestamp
   * attached to the nonce exceeds `nonce_timeout', then the nonce is
   * invalid.
   */
  if ( (t > nonce_time + nonce_timeout) ||
       (nonce_time + nonce_timeout < nonce_time) )
    {
      /* too old */
      return MHD_INVALID_NONCE;
    }

  calculate_nonce (nonce_time,
                   connection->method,
                   daemon->digest_auth_random,
                   daemon->digest_auth_rand_size,
                   connection->url,
                   realm,
                   noncehashexp);
  /*
   * Second level vetting for the nonce validity
   * if the timestamp attached to the nonce is valid
   * and possibly fabricated (in case of an attack)
   * the attacker must also know the random seed to be
   * able to generate a "sane" nonce, which if he does
   * not, the nonce fabrication process going to be
   * very hard to achieve.
   */

  if (0 != strcmp (nonce, noncehashexp))
    {
      return MHD_INVALID_NONCE;
    }
  if ( (0 == lookup_sub_value (cnonce,
                               sizeof (cnonce),
                               header,
                               "cnonce")) ||
       (0 == lookup_sub_value (qop,
                               sizeof (qop),
                               header,
                               "qop")) ||
       ( (0 != strcmp (qop,
                       "auth")) &&
         (0 != strcmp (qop,
                       "")) ) ||
       (0 == (len = lookup_sub_value (nc,
                                      sizeof (nc),
                                      header,
                                      "nc")) )  ||
       (0 == lookup_sub_value (response,
                               sizeof (response),
                               header,
                               "response")) )
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Authentication failed, invalid format.\n"));
#endif
      return MHD_NO;
    }
  if (len != MHD_strx_to_uint64_n_ (nc,
                                    len,
                                    &nci))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Authentication failed, invalid nc format.\n"));
#endif
      return MHD_NO; /* invalid nonce format */
    }

  /*
   * Checking if that combination of nonce and nc is sound
   * and not a replay attack attempt. Also adds the nonce
   * to the nonce-nc map if it does not exist there.
   */
  if (MHD_YES !=
      check_nonce_nc (connection,
                      nonce,
                      nci))
    {
      return MHD_NO;
    }

  {
    char *uri;

    uri = malloc (left + 1);
    if (NULL == uri)
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG(daemon,
               _("Failed to allocate memory for auth header processing\n"));
#endif /* HAVE_MESSAGES */
      return MHD_NO;
    }
    if (0 == lookup_sub_value (uri,
                               left + 1,
                               header,
                               "uri"))
    {
      free (uri);
      return MHD_NO;
    }

    digest_calc_ha1 ("md5",
                     username,
                     realm,
                     password,
                     nonce,
                     cnonce,
                     ha1);
    digest_calc_response (ha1,
			  nonce,
			  nc,
			  cnonce,
			  qop,
			  connection->method,
			  uri,
			  hentity,
			  respexp);

    /* Need to unescape URI before comparing with connection->url */
    daemon->unescape_callback (daemon->unescape_callback_cls,
                               connection,
                               uri);
    if (0 != strncmp (uri,
		      connection->url,
		      strlen (connection->url)))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (daemon,
		_("Authentication failed, URI does not match.\n"));
#endif
      free (uri);
      return MHD_NO;
    }

    {
      const char *args = strchr (uri,
                                 '?');

      if (NULL == args)
	args = "";
      else
	args++;
      if (MHD_YES !=
	  check_argument_match (connection,
				args) )
      {
#ifdef HAVE_MESSAGES
	MHD_DLOG (daemon,
		  _("Authentication failed, arguments do not match.\n"));
#endif
       free (uri);
       return MHD_NO;
      }
    }
    free (uri);
    return (0 == strcmp(response,
                        respexp))
      ? MHD_YES
      : MHD_NO;
  }
}


/**
 * Queues a response to request authentication from the client
 *
 * @param connection The MHD connection structure
 * @param realm the realm presented to the client
 * @param opaque string to user for opaque value
 * @param response reply to send; should contain the "access denied"
 *        body; note that this function will set the "WWW Authenticate"
 *        header and that the caller should not do this
 * @param signal_stale #MHD_YES if the nonce is invalid to add
 * 			'stale=true' to the authentication header
 * @return #MHD_YES on success, #MHD_NO otherwise
 * @ingroup authentication
 */
int
MHD_queue_auth_fail_response (struct MHD_Connection *connection,
			      const char *realm,
			      const char *opaque,
			      struct MHD_Response *response,
			      int signal_stale)
{
  int ret;
  int hlen;
  char nonce[NONCE_STD_LEN + 1];

  /* Generating the server nonce */
  calculate_nonce ((uint32_t) MHD_monotonic_sec_counter(),
		   connection->method,
		   connection->daemon->digest_auth_random,
		   connection->daemon->digest_auth_rand_size,
		   connection->url,
		   realm,
		   nonce);
  if (MHD_YES !=
      check_nonce_nc (connection,
                      nonce,
                      0))
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
		_("Could not register nonce (is the nonce array size zero?).\n"));
#endif
      return MHD_NO;
    }
  /* Building the authentication header */
  hlen = MHD_snprintf_ (NULL,
                        0,
                        "Digest realm=\"%s\",qop=\"auth\",nonce=\"%s\",opaque=\"%s\"%s",
                        realm,
                        nonce,
                        opaque,
                        signal_stale
                        ? ",stale=\"true\""
                        : "");
  if (hlen > 0)
    {
      char *header;

      header = MHD_calloc_ (1, hlen + 1);
      if (NULL == header)
        {
#ifdef HAVE_MESSAGES
          MHD_DLOG(connection->daemon,
                   _("Failed to allocate memory for auth response header\n"));
#endif /* HAVE_MESSAGES */
          return MHD_NO;
        }

      if (MHD_snprintf_ (header,
                         hlen + 1,
                         "Digest realm=\"%s\",qop=\"auth\",nonce=\"%s\",opaque=\"%s\"%s",
                         realm,
                         nonce,
                         opaque,
                         signal_stale
                         ? ",stale=\"true\""
                         : "") == hlen)
        ret = MHD_add_response_header(response,
                                      MHD_HTTP_HEADER_WWW_AUTHENTICATE,
                                      header);
      else
        ret = MHD_NO;
      free (header);
    }
  else
    ret = MHD_NO;

  if (MHD_YES == ret)
    {
      ret = MHD_queue_response (connection,
                                MHD_HTTP_UNAUTHORIZED,
                                response);
    }
  else
    {
#ifdef HAVE_MESSAGES
      MHD_DLOG (connection->daemon,
                _("Failed to add Digest auth header\n"));
#endif /* HAVE_MESSAGES */
    }
  return ret;
}


/* end of digestauth.c */
