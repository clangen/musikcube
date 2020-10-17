/*
  This file is part of libmicrohttpd
  Copyright (C) 2015, 2016 Karlson2k (Evgeny Grin)

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
 * @file microhttpd/mhd_str.c
 * @brief  Functions implementations for string manipulating
 * @author Karlson2k (Evgeny Grin)
 */

#include "mhd_str.h"

#ifdef HAVE_STDBOOL_H
#include <stdbool.h>
#endif

#include "mhd_limits.h"

#ifdef MHD_FAVOR_SMALL_CODE
#ifdef _MHD_inline
#undef _MHD_inline
#endif /* _MHD_inline */
/* Do not force inlining and do not use macro functions, use normal static
   functions instead.
   This may give more flexibility for size optimizations. */
#define _MHD_inline static
#ifndef INLINE_FUNC
#define INLINE_FUNC 1
#endif /* !INLINE_FUNC */
#endif /* MHD_FAVOR_SMALL_CODE */

/*
 * Block of functions/macros that use US-ASCII charset as required by HTTP
 * standards. Not affected by current locale settings.
 */

#ifdef INLINE_FUNC
/**
 * Check whether character is lower case letter in US-ASCII
 *
 * @param c character to check
 * @return non-zero if character is lower case letter, zero otherwise
 */
_MHD_inline bool
isasciilower (char c)
{
  return (c >= 'a') && (c <= 'z');
}


/**
 * Check whether character is upper case letter in US-ASCII
 *
 * @param c character to check
 * @return non-zero if character is upper case letter, zero otherwise
 */
_MHD_inline bool
isasciiupper (char c)
{
  return (c >= 'A') && (c <= 'Z');
}


/**
 * Check whether character is letter in US-ASCII
 *
 * @param c character to check
 * @return non-zero if character is letter in US-ASCII, zero otherwise
 */
_MHD_inline bool
isasciialpha (char c)
{
  return isasciilower (c) || isasciiupper (c);
}


/**
 * Check whether character is decimal digit in US-ASCII
 *
 * @param c character to check
 * @return non-zero if character is decimal digit, zero otherwise
 */
_MHD_inline bool
isasciidigit (char c)
{
  return (c >= '0') && (c <= '9');
}


/**
 * Check whether character is hexadecimal digit in US-ASCII
 *
 * @param c character to check
 * @return non-zero if character is decimal digit, zero otherwise
 */
_MHD_inline bool
isasciixdigit (char c)
{
  return isasciidigit (c) ||
    ( (c >= 'A') && (c <= 'F') ) ||
    ( (c >= 'a') && (c <= 'f') );
}


/**
 * Check whether character is decimal digit or letter in US-ASCII
 *
 * @param c character to check
 * @return non-zero if character is decimal digit or letter, zero otherwise
 */
_MHD_inline bool
isasciialnum (char c)
{
  return isasciialpha (c) || isasciidigit (c);
}


/**
 * Convert US-ASCII character to lower case.
 * If character is upper case letter in US-ASCII than it's converted to lower
 * case analog. If character is NOT upper case letter than it's returned
 * unmodified.
 *
 * @param c character to convert
 * @return converted to lower case character
 */
_MHD_inline char
toasciilower (char c)
{
  return isasciiupper (c) ? (c - 'A' + 'a') : c;
}


/**
 * Convert US-ASCII character to upper case.
 * If character is lower case letter in US-ASCII than it's converted to upper
 * case analog. If character is NOT lower case letter than it's returned
 * unmodified.
 *
 * @param c character to convert
 * @return converted to upper case character
 */
_MHD_inline char
toasciiupper (char c)
{
  return isasciilower (c) ? (c - 'a' + 'A') : c;
}


/**
 * Convert US-ASCII decimal digit to its value.
 *
 * @param c character to convert
 * @return value of hexadecimal digit or -1 if @ c is not hexadecimal digit
 */
_MHD_inline int
todigitvalue (char c)
{
  if (isasciidigit (c))
    return (unsigned char)(c - '0');

  return -1;
}


/**
 * Convert US-ASCII hexadecimal digit to its value.
 *
 * @param c character to convert
 * @return value of hexadecimal digit or -1 if @ c is not hexadecimal digit
 */
_MHD_inline int
toxdigitvalue (char c)
{
  if (isasciidigit (c))
    return (unsigned char)(c - '0');
  if ( (c >= 'A') && (c <= 'F') )
    return (unsigned char)(c - 'A' + 10);
  if ( (c >= 'a') && (c <= 'f') )
    return (unsigned char)(c - 'a' + 10);

  return -1;
}
#else  /* !INLINE_FUNC */


/**
 * Checks whether character is lower case letter in US-ASCII
 *
 * @param c character to check
 * @return boolean true if character is lower case letter,
 *         boolean false otherwise
 */
#define isasciilower(c) (((char)(c)) >= 'a' && ((char)(c)) <= 'z')


/**
 * Checks whether character is upper case letter in US-ASCII
 *
 * @param c character to check
 * @return boolean true if character is upper case letter,
 *         boolean false otherwise
 */
#define isasciiupper(c) (((char)(c)) >= 'A' && ((char)(c)) <= 'Z')


/**
 * Checks whether character is letter in US-ASCII
 *
 * @param c character to check
 * @return boolean true if character is letter, boolean false
 *         otherwise
 */
#define isasciialpha(c) (isasciilower(c) || isasciiupper(c))


/**
 * Check whether character is decimal digit in US-ASCII
 *
 * @param c character to check
 * @return boolean true if character is decimal digit, boolean false
 *         otherwise
 */
#define isasciidigit(c) (((char)(c)) >= '0' && ((char)(c)) <= '9')


/**
 * Check whether character is hexadecimal digit in US-ASCII
 *
 * @param c character to check
 * @return boolean true if character is hexadecimal digit,
 *         boolean false otherwise
 */
#define isasciixdigit(c) (isasciidigit((c)) || \
                          (((char)(c)) >= 'A' && ((char)(c)) <= 'F') || \
                          (((char)(c)) >= 'a' && ((char)(c)) <= 'f') )


/**
 * Check whether character is decimal digit or letter in US-ASCII
 *
 * @param c character to check
 * @return boolean true if character is decimal digit or letter,
 *         boolean false otherwise
 */
#define isasciialnum(c) (isasciialpha(c) || isasciidigit(c))


/**
 * Convert US-ASCII character to lower case.
 * If character is upper case letter in US-ASCII than it's converted to lower
 * case analog. If character is NOT upper case letter than it's returned
 * unmodified.
 *
 * @param c character to convert
 * @return converted to lower case character
 */
#define toasciilower(c) ((isasciiupper(c)) ? (((char)(c)) - 'A' + 'a') : ((char)(c)))


/**
 * Convert US-ASCII character to upper case.
 * If character is lower case letter in US-ASCII than it's converted to upper
 * case analog. If character is NOT lower case letter than it's returned
 * unmodified.
 *
 * @param c character to convert
 * @return converted to upper case character
 */
#define toasciiupper(c) ((isasciilower(c)) ? (((char)(c)) - 'a' + 'A') : ((char)(c)))


/**
 * Convert US-ASCII decimal digit to its value.
 *
 * @param c character to convert
 * @return value of hexadecimal digit or -1 if @ c is not hexadecimal digit
 */
#define todigitvalue(c) (isasciidigit(c) ? (int)(((char)(c)) - '0') : (int)(-1))


/**
 * Convert US-ASCII hexadecimal digit to its value.
 * @param c character to convert
 * @return value of hexadecimal digit or -1 if @ c is not hexadecimal digit
 */
#define toxdigitvalue(c) ( isasciidigit(c) ? (int)(((char)(c)) - '0') : \
                           ( (((char)(c)) >= 'A' && ((char)(c)) <= 'F') ? \
                             (int)(((unsigned char)(c)) - 'A' + 10) : \
                             ( (((char)(c)) >= 'a' && ((char)(c)) <= 'f') ? \
                               (int)(((unsigned char)(c)) - 'a' + 10) : (int)(-1) )))
#endif /* !INLINE_FUNC */


#ifndef MHD_FAVOR_SMALL_CODE
/**
 * Check two string for equality, ignoring case of US-ASCII letters.
 *
 * @param str1 first string to compare
 * @param str2 second string to compare
 * @return non-zero if two strings are equal, zero otherwise.
 */
int
MHD_str_equal_caseless_ (const char * str1,
                         const char * str2)
{
  while (0 != (*str1))
    {
      const char c1 = *str1;
      const char c2 = *str2;
      if ( (c1 != c2) &&
           (toasciilower (c1) != toasciilower (c2)) )
        return 0;
      str1++;
      str2++;
    }
  return 0 == (*str2);
}
#endif /* ! MHD_FAVOR_SMALL_CODE */


/**
 * Check two string for equality, ignoring case of US-ASCII letters and
 * checking not more than @a maxlen characters.
 * Compares up to first terminating null character, but not more than
 * first @a maxlen characters.
 *
 * @param str1 first string to compare
 * @param str2 second string to compare
 * @param maxlen maximum number of characters to compare
 * @return non-zero if two strings are equal, zero otherwise.
 */
int
MHD_str_equal_caseless_n_ (const char * const str1,
                           const char * const str2,
                           size_t maxlen)
{
  size_t i;

  for (i = 0; i < maxlen; ++i)
    {
      const char c1 = str1[i];
      const char c2 = str2[i];
      if (0 == c2)
        return 0 == c1;
      if ( (c1 != c2) &&
           (toasciilower (c1) != toasciilower (c2)) )
        return 0;
    }
  return !0;
}

#ifndef MHD_FAVOR_SMALL_CODE
/* Use individual function for each case */

/**
 * Convert decimal US-ASCII digits in string to number in uint64_t.
 * Conversion stopped at first non-digit character.
 *
 * @param str string to convert
 * @param[out] out_val pointer to uint64_t to store result of conversion
 * @return non-zero number of characters processed on succeed,
 *         zero if no digit is found, resulting value is larger
 *         then possible to store in uint64_t or @a out_val is NULL
 */
size_t
MHD_str_to_uint64_ (const char *str,
                    uint64_t *out_val)
{
  const char * const start = str;
  uint64_t res;

  if (!str || !out_val || !isasciidigit(str[0]))
    return 0;

  res = 0;
  do
    {
      const int digit = (unsigned char)(*str) - '0';
      if ( (res > (UINT64_MAX / 10)) ||
           ( (res == (UINT64_MAX / 10)) &&
             ((uint64_t)digit > (UINT64_MAX % 10)) ) )
        return 0;

      res *= 10;
      res += digit;
      str++;
    } while (isasciidigit (*str));

  *out_val = res;
  return str - start;
}


/**
 * Convert not more then @a maxlen decimal US-ASCII digits in string to
 * number in uint64_t.
 * Conversion stopped at first non-digit character or after @a maxlen
 * digits.
 *
 * @param str string to convert
 * @param maxlen maximum number of characters to process
 * @param[out] out_val pointer to uint64_t to store result of conversion
 * @return non-zero number of characters processed on succeed,
 *         zero if no digit is found, resulting value is larger
 *         then possible to store in uint64_t or @a out_val is NULL
 */
size_t
MHD_str_to_uint64_n_ (const char * str,
                      size_t maxlen,
                      uint64_t *out_val)
{
  uint64_t res;
  size_t i;

  if (!str || !maxlen || !out_val || !isasciidigit (str[0]))
    return 0;

  res = 0;
  i = 0;
  do
    {
      const int digit = (unsigned char)str[i] - '0';

      if ( (res > (UINT64_MAX / 10)) ||
           ( (res == (UINT64_MAX / 10)) &&
             ((uint64_t)digit > (UINT64_MAX % 10)) ) )
        return 0;

      res *= 10;
      res += digit;
      i++;
    } while ( (i < maxlen) &&
              isasciidigit (str[i]) );

  *out_val= res;
  return i;
}


/**
 * Convert hexadecimal US-ASCII digits in string to number in uint32_t.
 * Conversion stopped at first non-digit character.
 *
 * @param str string to convert
 * @param[out] out_val pointer to uint32_t to store result of conversion
 * @return non-zero number of characters processed on succeed,
 *         zero if no digit is found, resulting value is larger
 *         then possible to store in uint32_t or @a out_val is NULL
 */
size_t
MHD_strx_to_uint32_ (const char * str,
                     uint32_t *out_val)
{
  const char * const start = str;
  uint32_t res;
  int digit;

  if (!str || !out_val)
    return 0;

  res = 0;
  digit = toxdigitvalue (*str);
  while (digit >= 0)
    {
      if ( (res < (UINT32_MAX / 16)) ||
           (res == (UINT32_MAX / 16) && (uint32_t)digit <= (UINT32_MAX % 16)) )
        {
          res *= 16;
          res += digit;
        }
      else
        return 0;
      str++;
      digit = toxdigitvalue (*str);
    }

  if (str - start > 0)
    *out_val = res;
  return str - start;
}


/**
 * Convert not more then @a maxlen hexadecimal US-ASCII digits in string
 * to number in uint32_t.
 * Conversion stopped at first non-digit character or after @a maxlen
 * digits.
 *
 * @param str string to convert
 * @param maxlen maximum number of characters to process
 * @param[out] out_val pointer to uint32_t to store result of conversion
 * @return non-zero number of characters processed on succeed,
 *         zero if no digit is found, resulting value is larger
 *         then possible to store in uint32_t or @a out_val is NULL
 */
size_t
MHD_strx_to_uint32_n_ (const char *str,
                       size_t maxlen,
                       uint32_t *out_val)
{
  size_t i;
  uint32_t res;
  int digit;
  if (!str || !out_val)
    return 0;

  res = 0;
  i = 0;
  while (i < maxlen && (digit = toxdigitvalue (str[i])) >= 0)
    {
      if ( (res > (UINT32_MAX / 16)) ||
           (res == (UINT32_MAX / 16) && (uint32_t)digit > (UINT32_MAX % 16)) )
        return 0;

      res *= 16;
      res += digit;
      i++;
    }

  if (i)
    *out_val = res;
  return i;
}


/**
 * Convert hexadecimal US-ASCII digits in string to number in uint64_t.
 * Conversion stopped at first non-digit character.
 *
 * @param str string to convert
 * @param[out] out_val pointer to uint64_t to store result of conversion
 * @return non-zero number of characters processed on succeed,
 *         zero if no digit is found, resulting value is larger
 *         then possible to store in uint64_t or @a out_val is NULL
 */
size_t
MHD_strx_to_uint64_ (const char *str,
                     uint64_t *out_val)
{
  const char * const start = str;
  uint64_t res;
  int digit;
  if (!str || !out_val)
    return 0;

  res = 0;
  digit = toxdigitvalue (*str);
  while (digit >= 0)
    {
      if ( (res < (UINT64_MAX / 16)) ||
           (res == (UINT64_MAX / 16) && (uint64_t)digit <= (UINT64_MAX % 16)) )
        {
          res *= 16;
          res += digit;
        }
      else
        return 0;
      str++;
      digit = toxdigitvalue (*str);
    }

  if (str - start > 0)
    *out_val = res;
  return str - start;
}


/**
 * Convert not more then @a maxlen hexadecimal US-ASCII digits in string
 * to number in uint64_t.
 * Conversion stopped at first non-digit character or after @a maxlen
 * digits.
 *
 * @param str string to convert
 * @param maxlen maximum number of characters to process
 * @param[out] out_val pointer to uint64_t to store result of conversion
 * @return non-zero number of characters processed on succeed,
 *         zero if no digit is found, resulting value is larger
 *         then possible to store in uint64_t or @a out_val is NULL
 */
size_t
MHD_strx_to_uint64_n_ (const char * str,
                       size_t maxlen,
                       uint64_t *out_val)
{
  size_t i;
  uint64_t res;
  int digit;
  if (!str || !out_val)
    return 0;

  res = 0;
  i = 0;
  while (i < maxlen && (digit = toxdigitvalue (str[i])) >= 0)
    {
      if ( (res > (UINT64_MAX / 16)) ||
           (res == (UINT64_MAX / 16) && (uint64_t)digit > (UINT64_MAX % 16)) )
        return 0;

      res *= 16;
      res += digit;
      i++;
    }

  if (i)
    *out_val = res;
  return i;
}

#else  /* MHD_FAVOR_SMALL_CODE */

/**
 * Generic function for converting not more then @a maxlen
 * hexadecimal or decimal US-ASCII digits in string to number.
 * Conversion stopped at first non-digit character or after @a maxlen
 * digits.
 * To be used only within macro.
 *
 * @param str the string to convert
 * @param maxlen the maximum number of characters to process
 * @param out_val the pointer to variable to store result of conversion
 * @param val_size the size of variable pointed by @a out_val, in bytes, 4 or 8
 * @param max_val the maximum decoded number
 * @param base the numeric base, 10 or 16
 * @return non-zero number of characters processed on succeed,
 *         zero if no digit is found, resulting value is larger
 *         then @max_val, @val_size is not 16/32 or @a out_val is NULL
 */
size_t
MHD_str_to_uvalue_n_ (const char *str,
                      size_t maxlen,
                      void * out_val,
                      size_t val_size,
                      uint64_t max_val,
                      int base)
{
  size_t i;
  uint64_t res;
  int digit;
  const uint64_t max_v_div_b = max_val / base;
  const uint64_t max_v_mod_b = max_val % base;
  /* 'digit->value' must be function, not macro */
  int (*const dfunc)(char) = (base == 16) ?
                              toxdigitvalue : todigitvalue;

  if ( !str || !out_val ||
       (base != 16 && base != 10) )
    return 0;

  res = 0;
  i = 0;
  while (maxlen > i && 0 <= (digit = dfunc (str[i])))
    {
      if ( ((max_v_div_b) < res) ||
          ((max_v_div_b) == res && (max_v_mod_b) < (uint64_t)digit) )
        return 0;

      res *= base;
      res += digit;
      i++;
    }

  if (i)
    {
      if (8 == val_size)
        *(uint64_t*)out_val = res;
      else if (4 == val_size)
        *(uint32_t*)out_val = (uint32_t)res;
      else
        return 0;
    }
  return i;
}
#endif /* MHD_FAVOR_SMALL_CODE */
