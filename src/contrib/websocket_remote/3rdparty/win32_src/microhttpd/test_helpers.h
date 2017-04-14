/*
  This file is part of libmicrohttpd
  Copyright (C) 2016 Karlson2k (Evgeny Grin)

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
 * @file microhttpd/test_helpers.h
 * @brief Static functions and macros helpers for testsuite.
 * @author Karlson2k (Evgeny Grin)
 */

#include <string.h>

/**
 * Check whether program name contains specific @a marker string.
 * Only last component in pathname is checked for marker presence,
 * all leading directories names (if any) are ignored. Directories
 * separators are handled correctly on both non-W32 and W32
 * platforms.
 * @param prog_name program name, may include path
 * @param marker    marker to look for.
 * @return zero if any parameter is NULL or empty string or
 *         @prog_name ends with slash or @marker is not found in
 *         program name, non-zero if @maker is found in program
 *         name.
 */
static int
has_in_name(const char *prog_name, const char *marker)
{
  size_t name_pos;
  size_t pos;

  if (!prog_name || !marker || !prog_name[0] || !marker[0])
    return 0;

  pos = 0;
  name_pos = 0;
  while (prog_name[pos])
    {
      if ('/' == prog_name[pos])
        name_pos = pos + 1;
#ifdef _WIN32
      else if ('\\' == prog_name[pos])
        name_pos = pos + 1;
#endif /* _WIN32 */
      pos++;
    }
  if (name_pos == pos)
    return 0;
  return strstr(prog_name + name_pos, marker) != (char*)0;
}

/**
 * Check whether one of strings in array is equal to @a param.
 * String @a argv[0] is ignored.
 * @param argc number of strings in @a argv, as passed to main function
 * @param argv array of strings, as passed to main function
 * @param param parameter to look for.
 * @return zero if @a argv is NULL, @a param is NULL or empty string,
 *         @a argc is less then 2 or @a param is not found in @a argv,
 *         non-zero if one of strings in @a argv is equal to @a param.
 */
static int
has_param(int argc, char * const argv[], const char * param)
{
  int i;
  if (!argv || !param || !param[0])
    return 0;

  for(i = 1; i < argc; i++)
    {
      if(argv[i] && strcmp(argv[i], param) == 0)
        return !0;
    }

  return 0;
}
