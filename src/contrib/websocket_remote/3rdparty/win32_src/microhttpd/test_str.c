/*
  This file is part of libmicrohttpd
  Copyright (C) 2016 Karlson2k (Evgeny Grin)

  This test tool is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2, or
  (at your option) any later version.

  This test tool is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * @file microhttpd/unit_str_test.h
 * @brief  Unit tests for mhd_str functions
 * @author Karlson2k (Evgeny Grin)
 */

#include "mhd_options.h"
#include <stdio.h>
#include <locale.h>
#include <string.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else  /* ! HAVE_INTTYPES_H */
#define PRIu64  "llu"
#define PRIuPTR "u"
#define PRIX64 "llX"
#endif /* ! HAVE_INTTYPES_H */
#include <stdint.h>
#include "mhd_limits.h"
#include "mhd_str.h"
#include "test_helpers.h"


static int verbose = 0; /* verbose level (0-3)*/

/* Locale names to test.
 * Functions must not depend of current current locale,
 * so result must be the same in any locale.
 */
static const char * const locale_names[] = {
    "C",
    "",      /* System default locale */
#if defined(_WIN32) && !defined(__CYGWIN__)
    ".OCP",   /* W32 system default OEM code page */
    ".ACP",   /* W32 system default ANSI code page */
    ".65001", /* UTF-8 */
    ".437",
    ".850",
    ".857",
    ".866",
    ".1250",
    ".1251",
    ".1252",
    ".1254",
    ".20866", /* number for KOI8-R */
    ".28591", /* number for ISO-8859-1 */
    ".28595", /* number for ISO-8859-5 */
    ".28599", /* number for ISO-8859-9 */
    ".28605", /* number for ISO-8859-15 */
    "en",
    "english",
    "en-US",
    "English-US",
    "en-US.437",
    "English_United States.437",
    "en-US.1252",
    "English_United States.1252",
    "English_United States.28591",
    "English_United States.65001",
    "fra",
    "french",
    "fr-FR",
    "French_France",
    "fr-FR.850",
    "french_france.850",
    "fr-FR.1252",
    "French_france.1252",
    "French_france.28605",
    "French_France.65001",
    "de",
    "de-DE",
    "de-DE.850",
    "German_Germany.850",
    "German_Germany.1250",
    "de-DE.1252",
    "German_Germany.1252",
    "German_Germany.28605",
    "German_Germany.65001",
    "tr",
    "trk",
    "turkish",
    "tr-TR",
    "tr-TR.1254",
    "Turkish_Turkey.1254",
    "tr-TR.857",
    "Turkish_Turkey.857",
    "Turkish_Turkey.28599",
    "Turkish_Turkey.65001",
    "ru",
    "ru-RU",
    "Russian",
    "ru-RU.866",
    "Russian_Russia.866",
    "ru-RU.1251",
    "Russian_Russia.1251",
    "Russian_Russia.20866",
    "Russian_Russia.28595",
    "Russian_Russia.65001",
    "zh-Hans",
    "zh-Hans.936",
    "chinese-simplified"
#else /* ! _WIN32 || __CYGWIN__ */
    "C.UTF-8",
    "POSIX",
    "en",
    "en_US",
    "en_US.ISO-8859-1",
    "en_US.ISO_8859-1",
    "en_US.ISO8859-1",
    "en_US.iso88591",
    "en_US.ISO-8859-15",
    "en_US.DIS_8859-15",
    "en_US.ISO8859-15",
    "en_US.iso885915",
    "en_US.1252",
    "en_US.CP1252",
    "en_US.UTF-8",
    "en_US.utf8",
    "fr",
    "fr_FR",
    "fr_FR.850",
    "fr_FR.IBM850",
    "fr_FR.1252",
    "fr_FR.CP1252",
    "fr_FR.ISO-8859-1",
    "fr_FR.ISO_8859-1",
    "fr_FR.ISO8859-1",
    "fr_FR.iso88591",
    "fr_FR.ISO-8859-15",
    "fr_FR.DIS_8859-15",
    "fr_FR.ISO8859-15",
    "fr_FR.iso8859-15",
    "fr_FR.UTF-8",
    "fr_FR.utf8",
    "de",
    "de_DE",
    "de_DE.850",
    "de_DE.IBM850",
    "de_DE.1250",
    "de_DE.CP1250",
    "de_DE.1252",
    "de_DE.CP1252",
    "de_DE.ISO-8859-1",
    "de_DE.ISO_8859-1",
    "de_DE.ISO8859-1",
    "de_DE.iso88591",
    "de_DE.ISO-8859-15",
    "de_DE.DIS_8859-15",
    "de_DE.ISO8859-15",
    "de_DE.iso885915",
    "de_DE.UTF-8",
    "de_DE.utf8",
    "tr",
    "tr_TR",
    "tr_TR.1254",
    "tr_TR.CP1254",
    "tr_TR.857",
    "tr_TR.IBM857",
    "tr_TR.ISO-8859-9",
    "tr_TR.ISO8859-9",
    "tr_TR.iso88599",
    "tr_TR.UTF-8",
    "tr_TR.utf8",
    "ru",
    "ru_RU",
    "ru_RU.1251",
    "ru_RU.CP1251",
    "ru_RU.866",
    "ru_RU.IBM866",
    "ru_RU.KOI8-R",
    "ru_RU.koi8-r",
    "ru_RU.KOI8-RU",
    "ru_RU.ISO-8859-5",
    "ru_RU.ISO_8859-5",
    "ru_RU.ISO8859-5",
    "ru_RU.iso88595",
    "ru_RU.UTF-8",
    "zh_CN",
    "zh_CN.GB2312",
    "zh_CN.UTF-8",
#endif /* ! _WIN32 || __CYGWIN__ */
};

static const unsigned int locale_name_count = sizeof(locale_names) / sizeof(locale_names[0]);


/*
 *  Helper functions
 */

int set_test_locale(unsigned int num)
{
  if (num >= locale_name_count)
    return -1;
  if (verbose > 2)
    printf("Setting locale \"%s\":", locale_names[num]);
   if (setlocale(LC_ALL, locale_names[num]))
     {
       if (verbose > 2)
         printf(" succeed.\n");
       return 1;
     }
   if (verbose > 2)
     printf(" failed.\n");
   return 0;
}

const char * get_current_locale_str(void)
{
  char const * loc_str = setlocale(LC_ALL, NULL);
  return loc_str ? loc_str : "unknown";
}

static char tmp_bufs[4][4*1024]; /* should be enough for testing */
static size_t buf_idx = 0;

/* print non-printable chars as char codes */
char * n_prnt(const char * str)
{
  static char * buf; /* should be enough for testing */
  static const size_t buf_size = sizeof(tmp_bufs[0]);
  const unsigned char * p = (const unsigned char*)str;
  size_t w_pos = 0;
  if (++buf_idx > 3)
    buf_idx = 0;
  buf = tmp_bufs[buf_idx];

  while(*p && w_pos + 1 < buf_size)
    {
      const unsigned char c = *p;
      if (c == '\\' || c == '"')
        {
          if (w_pos + 2 >= buf_size)
            break;
          buf[w_pos++] = '\\';
          buf[w_pos++] = c;
        }
      else if (c >= 0x20 && c <= 0x7E)
          buf[w_pos++] = c;
      else
        {
          if (w_pos + 4 >= buf_size)
            break;
          if (snprintf(buf + w_pos, buf_size - w_pos, "\\x%02hX", (short unsigned int)c) != 4)
            break;
          w_pos += 4;
        }
      p++;
    }
  if (*p)
    { /* not full string is printed */
      /* enough space for "..." ? */
      if (w_pos + 3 > buf_size)
        w_pos = buf_size - 4;
      buf[w_pos++] = '.';
      buf[w_pos++] = '.';
      buf[w_pos++] = '.';
    }
  buf[w_pos] = 0;
  return buf;
}


struct str_with_len
{
  const char * const str;
  const size_t len;
};

#define D_STR_W_LEN(s) {(s), (sizeof((s)) / sizeof(char)) - 1}

/*
 * String caseless equality functions tests
 */

struct two_eq_strs
{
  const struct str_with_len s1;
  const struct str_with_len s2;
};

static const struct two_eq_strs eq_strings[] = {
    {D_STR_W_LEN("1234567890!@~%&$@#{}[]\\/!?`."), D_STR_W_LEN("1234567890!@~%&$@#{}[]\\/!?`.")},
    {D_STR_W_LEN("Simple string."), D_STR_W_LEN("Simple string.")},
    {D_STR_W_LEN("SIMPLE STRING."), D_STR_W_LEN("SIMPLE STRING.")},
    {D_STR_W_LEN("simple string."), D_STR_W_LEN("simple string.")},
    {D_STR_W_LEN("simple string."), D_STR_W_LEN("Simple String.")},
    {D_STR_W_LEN("sImPlE StRiNg."), D_STR_W_LEN("SiMpLe sTrInG.")},
    {D_STR_W_LEN("SIMPLE STRING."), D_STR_W_LEN("simple string.")},
    {D_STR_W_LEN("abcdefghijklmnopqrstuvwxyz"), D_STR_W_LEN("abcdefghijklmnopqrstuvwxyz")},
    {D_STR_W_LEN("ABCDEFGHIJKLMNOPQRSTUVWXYZ"), D_STR_W_LEN("ABCDEFGHIJKLMNOPQRSTUVWXYZ")},
    {D_STR_W_LEN("abcdefghijklmnopqrstuvwxyz"), D_STR_W_LEN("ABCDEFGHIJKLMNOPQRSTUVWXYZ")},
    {D_STR_W_LEN("zyxwvutsrqponMLKJIHGFEDCBA"), D_STR_W_LEN("ZYXWVUTSRQPONmlkjihgfedcba")},

    {D_STR_W_LEN("Cha\x8cne pour le test."),
     D_STR_W_LEN("Cha\x8cne pour le test.")},     /* "Chaîne pour le test." in CP850 */
    {D_STR_W_LEN("cha\x8cne pOur Le TEst."),
     D_STR_W_LEN("Cha\x8cne poUr Le teST.")},
    {D_STR_W_LEN("Cha\xeene pour le test."),
     D_STR_W_LEN("Cha\xeene pour le test.")},     /* "Chaîne pour le test." in CP1252/ISO-8859-1/ISO-8859-15 */
    {D_STR_W_LEN("CHa\xeene POUR le test."),
     D_STR_W_LEN("Cha\xeeNe pour lE TEST.")},
    {D_STR_W_LEN("Cha\xc3\xaene pour le Test."),
     D_STR_W_LEN("Cha\xc3\xaene pour le Test.")}, /* "Chaîne pour le test." in UTF-8 */
    {D_STR_W_LEN("ChA\xc3\xaene pouR lE TesT."),
     D_STR_W_LEN("Cha\xc3\xaeNe Pour le teSt.")},

    {D_STR_W_LEN(".Beispiel Zeichenfolge"),
     D_STR_W_LEN(".Beispiel Zeichenfolge")},
    {D_STR_W_LEN(".bEisPiel ZEIchenfoLgE"),
     D_STR_W_LEN(".BEiSpiEl zeIcheNfolge")},

    {D_STR_W_LEN("Do\xa7rulama \x87izgi!"),
     D_STR_W_LEN("Do\xa7rulama \x87izgi!")},      /* "Doğrulama çizgi!" in CP857 */
    {D_STR_W_LEN("Do\xa7rulama \x87IzgI!"),       /* Spelling intentionally incorrect here */
     D_STR_W_LEN("Do\xa7rulama \x87izgi!")},      /* Note: 'i' is not caseless equal to 'I' in Turkish */
    {D_STR_W_LEN("Do\xf0rulama \xe7izgi!"),
     D_STR_W_LEN("Do\xf0rulama \xe7izgi!")},      /* "Doğrulama çizgi!" in CP1254/ISO-8859-9 */
    {D_STR_W_LEN("Do\xf0rulamA \xe7Izgi!"),
     D_STR_W_LEN("do\xf0rulama \xe7izgi!")},
    {D_STR_W_LEN("Do\xc4\x9frulama \xc3\xa7izgi!"),
     D_STR_W_LEN("Do\xc4\x9frulama \xc3\xa7izgi!")},        /* "Doğrulama çizgi!" in UTF-8 */
    {D_STR_W_LEN("do\xc4\x9fruLAMA \xc3\xa7Izgi!"),         /* Spelling intentionally incorrect here */
     D_STR_W_LEN("DO\xc4\x9frulama \xc3\xa7izgI!")},        /* Spelling intentionally incorrect here */

    {D_STR_W_LEN("\x92\xa5\xe1\xe2\xae\xa2\xa0\xef \x91\xe2\xe0\xae\xaa\xa0."),
     D_STR_W_LEN("\x92\xa5\xe1\xe2\xae\xa2\xa0\xef \x91\xe2\xe0\xae\xaa\xa0.")}, /* "Тестовая Строка." in CP866 */
    {D_STR_W_LEN("\xd2\xe5\xf1\xf2\xee\xe2\xe0\xff \xd1\xf2\xf0\xee\xea\xe0."),
     D_STR_W_LEN("\xd2\xe5\xf1\xf2\xee\xe2\xe0\xff \xd1\xf2\xf0\xee\xea\xe0.")}, /* "Тестовая Строка." in CP1251 */
    {D_STR_W_LEN("\xf4\xc5\xd3\xd4\xcf\xd7\xc1\xd1 \xf3\xd4\xd2\xcf\xcb\xc1."),
     D_STR_W_LEN("\xf4\xc5\xd3\xd4\xcf\xd7\xc1\xd1 \xf3\xd4\xd2\xcf\xcb\xc1.")}, /* "Тестовая Строка." in KOI8-R */
    {D_STR_W_LEN("\xc2\xd5\xe1\xe2\xde\xd2\xd0\xef \xc1\xe2\xe0\xde\xda\xd0."),
     D_STR_W_LEN("\xc2\xd5\xe1\xe2\xde\xd2\xd0\xef \xc1\xe2\xe0\xde\xda\xd0.")}, /* "Тестовая Строка." in ISO-8859-5 */
    {D_STR_W_LEN("\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82\xd0\xbe\xd0\xb2\xd0\xb0\xd1"
                 "\x8f \xd0\xa1\xd1\x82\xd1\x80\xd0\xbe\xd0\xba\xd0\xb0."),
     D_STR_W_LEN("\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82\xd0\xbe\xd0\xb2\xd0\xb0\xd1"
                 "\x8f \xd0\xa1\xd1\x82\xd1\x80\xd0\xbe\xd0\xba\xd0\xb0.")},     /* "Тестовая Строка." in UTF-8 */

    {D_STR_W_LEN("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                 "\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !\"#$%&'()*+,-./0123456789:;<=>?@[\\]"
                 "^_`{|}~\x7f\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90"
                 "\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4"
                 "\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8"
                 "\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc"
                 "\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0"
                 "\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4"
                 "\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"),
     D_STR_W_LEN("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                 "\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !\"#$%&'()*+,-./0123456789:;<=>?@[\\]"
                 "^_`{|}~\x7f\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90"
                 "\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4"
                 "\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8"
                 "\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc"
                 "\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0"
                 "\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4"
                 "\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff")}, /* Full sequence without a-z */
    {D_STR_W_LEN("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                 "\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !\"#$%&'()*+,-./0123456789:;<=>?@AB"
                 "CDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7f\x80\x81\x82\x83"
                 "\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97"
                 "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab"
                 "\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
                 "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3"
                 "\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
                 "\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb"
                 "\xfc\xfd\xfe\xff"),
     D_STR_W_LEN("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                 "\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !\"#$%&'()*+,-./0123456789:;<=>?@AB"
                 "CDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\x7f\x80\x81\x82\x83"
                 "\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97"
                 "\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab"
                 "\xac\xad\xae\xaf\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
                 "\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3"
                 "\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7"
                 "\xe8\xe9\xea\xeb\xec\xed\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb"
                 "\xfc\xfd\xfe\xff")}, /* Full sequence */
    {D_STR_W_LEN("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                 "\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !\"#$%&'()*+,-./0123456789:;<=>?@AB"
                 "CDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`{|}~\x7f\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89"
                 "\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d"
                 "\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1"
                 "\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5"
                 "\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9"
                 "\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed"
                 "\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff"),
     D_STR_W_LEN("\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14"
                 "\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !\"#$%&'()*+,-./0123456789:;<=>?@ab"
                 "cdefghijklmnopqrstuvwxyz[\\]^_`{|}~\x7f\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89"
                 "\x8a\x8b\x8c\x8d\x8e\x8f\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d"
                 "\x9e\x9f\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf\xb0\xb1"
                 "\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf\xc0\xc1\xc2\xc3\xc4\xc5"
                 "\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9"
                 "\xda\xdb\xdc\xdd\xde\xdf\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed"
                 "\xee\xef\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff")} /* Full with A/a match */
};

struct two_neq_strs
{
  const struct str_with_len s1;
  const struct str_with_len s2;
  const size_t dif_pos;
};

static const struct two_neq_strs neq_strings[] = {
    {D_STR_W_LEN("1234567890!@~%&$@#{}[]\\/!?`."), D_STR_W_LEN("1234567890!@~%&$@#{}[]\\/!?`"), 27},
    {D_STR_W_LEN(".1234567890!@~%&$@#{}[]\\/!?`."), D_STR_W_LEN("1234567890!@~%&$@#{}[]\\/!?`"), 0},
    {D_STR_W_LEN("Simple string."), D_STR_W_LEN("Simple ctring."), 7},
    {D_STR_W_LEN("simple string."), D_STR_W_LEN("simple string"), 13},
    {D_STR_W_LEN("simple strings"), D_STR_W_LEN("Simple String."), 13},
    {D_STR_W_LEN("sImPlE StRiNg."), D_STR_W_LEN("SYMpLe sTrInG."), 1},
    {D_STR_W_LEN("SIMPLE STRING."), D_STR_W_LEN("simple string.2"), 14},
    {D_STR_W_LEN("abcdefghijklmnopqrstuvwxyz,"), D_STR_W_LEN("abcdefghijklmnopqrstuvwxyz."), 26},
    {D_STR_W_LEN("abcdefghijklmnopqrstuvwxyz!"), D_STR_W_LEN("ABCDEFGHIJKLMNOPQRSTUVWXYZ?"), 26},
    {D_STR_W_LEN("zyxwvutsrqponwMLKJIHGFEDCBA"), D_STR_W_LEN("ZYXWVUTSRQPON%mlkjihgfedcba"), 13},

    {D_STR_W_LEN("S\xbdur veulent plus d'\xbdufs."),    /* "Sœur veulent plus d'œufs." in ISO-8859-15 */
     D_STR_W_LEN("S\xbcUR VEULENT PLUS D'\xbcUFS."), 1},/* "SŒUR VEULENT PLUS D'ŒUFS." in ISO-8859-15 */
    {D_STR_W_LEN("S\x9cur veulent plus d'\x9cufs."),    /* "Sœur veulent plus d'œufs." in CP1252 */
     D_STR_W_LEN("S\x8cUR VEULENT PLUS D'\x8cUFS."), 1},/* "SŒUR VEULENT PLUS D'ŒUFS." in CP1252 */
    {D_STR_W_LEN("S\xc5\x93ur veulent plus d'\xc5\x93ufs."),    /* "Sœur veulent plus d'œufs." in UTF-8 */
     D_STR_W_LEN("S\xc5\x92UR VEULENT PLUS D'\xc5\x92UFS."), 2},/* "SŒUR VEULENT PLUS D'ŒUFS." in UTF-8 */

    {D_STR_W_LEN("Um ein sch\x94nes M\x84" "dchen zu k\x81ssen."),     /* "Um ein schönes Mädchen zu küssen." in CP850 */
     D_STR_W_LEN("UM EIN SCH\x99NES M\x8e" "DCHEN ZU K\x9aSSEN."), 10},/* "UM EIN SCHÖNES MÄDCHEN ZU KÜSSEN." in CP850 */
    {D_STR_W_LEN("Um ein sch\xf6nes M\xe4" "dchen zu k\xfcssen."),     /* "Um ein schönes Mädchen zu küssen." in ISO-8859-1/ISO-8859-15/CP1250/CP1252 */
     D_STR_W_LEN("UM EIN SCH\xd6NES M\xc4" "DCHEN ZU K\xdcSSEN."), 10},/* "UM EIN SCHÖNES MÄDCHEN ZU KÜSSEN." in ISO-8859-1/ISO-8859-15/CP1250/CP1252 */
    {D_STR_W_LEN("Um ein sch\xc3\xb6nes M\xc3\xa4" "dchen zu k\xc3\xbcssen."),     /* "Um ein schönes Mädchen zu küssen." in UTF-8 */
     D_STR_W_LEN("UM EIN SCH\xc3\x96NES M\xc3\x84" "DCHEN ZU K\xc3\x9cSSEN."), 11},/* "UM EIN SCHÖNES MÄDCHEN ZU KÜSSEN." in UTF-8 */

    {D_STR_W_LEN("\x98stanbul"),           /* "İstanbul" in CP857 */
     D_STR_W_LEN("istanbul"), 0},          /* "istanbul" in CP857 */
    {D_STR_W_LEN("\xddstanbul"),           /* "İstanbul" in ISO-8859-9/CP1254 */
     D_STR_W_LEN("istanbul"), 0},          /* "istanbul" in ISO-8859-9/CP1254 */
    {D_STR_W_LEN("\xc4\xb0stanbul"),       /* "İstanbul" in UTF-8 */
     D_STR_W_LEN("istanbul"), 0},          /* "istanbul" in UTF-8 */
    {D_STR_W_LEN("Diyarbak\x8dr"),         /* "Diyarbakır" in CP857 */
     D_STR_W_LEN("DiyarbakIR"), 8},        /* "DiyarbakIR" in CP857 */
    {D_STR_W_LEN("Diyarbak\xfdr"),         /* "Diyarbakır" in ISO-8859-9/CP1254 */
     D_STR_W_LEN("DiyarbakIR"), 8},        /* "DiyarbakIR" in ISO-8859-9/CP1254 */
    {D_STR_W_LEN("Diyarbak\xc4\xb1r"),     /* "Diyarbakır" in UTF-8 */
     D_STR_W_LEN("DiyarbakIR"), 8},        /* "DiyarbakIR" in UTF-8 */

    {D_STR_W_LEN("\x92\xa5\xe1\xe2\xae\xa2\xa0\xef \x91\xe2\xe0\xae\xaa\xa0."),     /* "Тестовая Строка." in CP866 */
     D_STR_W_LEN("\x92\x85\x91\x92\x8e\x82\x80\x9f \x91\x92\x90\x8e\x8a\x80."), 1}, /* "ТЕСТОВАЯ СТРОКА." in CP866 */
    {D_STR_W_LEN("\xd2\xe5\xf1\xf2\xee\xe2\xe0\xff \xd1\xf2\xf0\xee\xea\xe0."),     /* "Тестовая Строка." in CP1251 */
     D_STR_W_LEN("\xd2\xc5\xd1\xd2\xce\xc2\xc0\xdf \xd1\xd2\xd0\xce\xca\xc0."), 1}, /* "ТЕСТОВАЯ СТРОКА." in CP1251 */
    {D_STR_W_LEN("\xf4\xc5\xd3\xd4\xcf\xd7\xc1\xd1 \xf3\xd4\xd2\xcf\xcb\xc1."),     /* "Тестовая Строка." in KOI8-R */
     D_STR_W_LEN("\xf4\xe5\xf3\xf4\xef\xf7\xe1\xf1 \xf3\xf4\xf2\xef\xeb\xe1."), 1}, /* "ТЕСТОВАЯ СТРОКА." in KOI8-R */
    {D_STR_W_LEN("\xc2\xd5\xe1\xe2\xde\xd2\xd0\xef \xc1\xe2\xe0\xde\xda\xd0."),     /* "Тестовая Строка." in ISO-8859-5 */
     D_STR_W_LEN("\xc2\xb5\xc1\xc2\xbe\xb2\xb0\xcf \xc1\xc2\xc0\xbe\xba\xb0."), 1}, /* "ТЕСТОВАЯ СТРОКА." in ISO-8859-5 */
    {D_STR_W_LEN("\xd0\xa2\xd0\xb5\xd1\x81\xd1\x82\xd0\xbe\xd0\xb2\xd0\xb0\xd1"
                 "\x8f \xd0\xa1\xd1\x82\xd1\x80\xd0\xbe\xd0\xba\xd0\xb0."),         /* "Тестовая Строка." in UTF-8 */
     D_STR_W_LEN("\xd0\xa2\xd0\x95\xd0\xa1\xd0\xa2\xd0\x9e\xd0\x92\xd0\x90\xd0"
                 "\xaf \xd0\xa1\xd0\xa2\xd0\xa0\xd0\x9e\xd0\x9a\xd0\x90."), 3}      /* "ТЕСТОВАЯ СТРОКА." in UTF-8 */
};


int check_eq_strings(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(eq_strings) / sizeof(eq_strings[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          const struct two_eq_strs * const t = eq_strings + i;
          if (c_failed[i])
            continue; /* skip already failed checks */
          if (!MHD_str_equal_caseless_(t->s1.str, t->s2.str))
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_str_equal_caseless_(\"%s\", \"%s\") returned zero, while expected non-zero."
                      " Locale: %s\n", n_prnt(t->s1.str), n_prnt(t->s2.str), get_current_locale_str());
            }
          else if (!MHD_str_equal_caseless_(t->s2.str, t->s1.str))
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_str_equal_caseless_(\"%s\", \"%s\") returned zero, while expected non-zero."
                      " Locale: %s\n", n_prnt(t->s2.str), n_prnt(t->s1.str), get_current_locale_str());
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_equal_caseless_(\"%s\", \"%s\") != 0 && \\\n"
                   "        MHD_str_equal_caseless_(\"%s\", \"%s\") != 0\n", n_prnt(t->s1.str), n_prnt(t->s2.str),
                   n_prnt(t->s2.str), n_prnt(t->s1.str));
        }
    }
  return t_failed;
}

int check_neq_strings(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(neq_strings) / sizeof(neq_strings[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          const struct two_neq_strs * const t = neq_strings + i;
          if (c_failed[i])
            continue; /* skip already failed checks */
          if (MHD_str_equal_caseless_(t->s1.str, t->s2.str))
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_str_equal_caseless_(\"%s\", \"%s\") returned non-zero, while expected zero."
                      " Locale: %s\n", n_prnt(t->s1.str), n_prnt(t->s2.str), get_current_locale_str());
            }
          else if (MHD_str_equal_caseless_(t->s2.str, t->s1.str))
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_str_equal_caseless_(\"%s\", \"%s\") returned non-zero, while expected zero."
                      " Locale: %s\n", n_prnt(t->s2.str), n_prnt(t->s1.str), get_current_locale_str());
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_equal_caseless_(\"%s\", \"%s\") == 0 && \\\n"
                   "        MHD_str_equal_caseless_(\"%s\", \"%s\") == 0\n", n_prnt(t->s1.str), n_prnt(t->s2.str),
                   n_prnt(t->s2.str), n_prnt(t->s1.str));
        }
    }
  return t_failed;
}

int check_eq_strings_n(void)
{
  size_t t_failed = 0;
  size_t i, j, k;
  static const size_t n_checks = sizeof(eq_strings) / sizeof(eq_strings[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t m_len;
          const struct two_eq_strs * const t = eq_strings + i;
          m_len = (t->s1.len > t->s2.len) ? t->s1.len : t->s2.len;
          for(k = 0; k <= m_len + 1 && !c_failed[i]; k++)
            {
              if (!MHD_str_equal_caseless_n_(t->s1.str, t->s2.str, k))
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", %u) returned zero,"
                                  " while expected non-zero. Locale: %s\n",
                                  n_prnt(t->s1.str), n_prnt(t->s2.str), (unsigned int) k, get_current_locale_str());
                }
              else if (!MHD_str_equal_caseless_n_(t->s2.str, t->s1.str, k))
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", %u) returned zero,"
                                  " while expected non-zero. Locale: %s\n",
                                  n_prnt(t->s2.str), n_prnt(t->s1.str), (unsigned int) k, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", N) != 0 && \\\n"
                   "        MHD_str_equal_caseless_n_(\"%s\", \"%s\", N) != 0, where N is 0..%u\n",
                   n_prnt(t->s1.str), n_prnt(t->s2.str), n_prnt(t->s2.str), n_prnt(t->s1.str), (unsigned int) m_len + 1);
        }
    }
  return t_failed;
}

int check_neq_strings_n(void)
{
  size_t t_failed = 0;
  size_t i, j, k;
  static const size_t n_checks = sizeof(neq_strings) / sizeof(neq_strings[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t m_len;
          const struct two_neq_strs * const t = neq_strings + i;
          m_len = t->s1.len > t->s2.len ? t->s1.len : t->s2.len;
          if (t->dif_pos >= m_len)
            {
              fprintf(stderr, "ERROR: neq_strings[%u] has wrong dif_pos (%u): dif_pos is expected to be less than "
                              "s1.len (%u) or s2.len (%u).\n", (unsigned int) i, (unsigned int) t->dif_pos,
                              (unsigned int) t->s1.len, (unsigned int) t->s2.len);
              return -1;
            }
          if (t->dif_pos > t->s1.len)
            {
              fprintf(stderr, "ERROR: neq_strings[%u] has wrong dif_pos (%u): dif_pos is expected to be less or "
                              "equal to s1.len (%u).\n", (unsigned int) i, (unsigned int) t->dif_pos,
                              (unsigned int) t->s1.len);
              return -1;
            }
          if (t->dif_pos > t->s2.len)
            {
              fprintf(stderr, "ERROR: neq_strings[%u] has wrong dif_pos (%u): dif_pos is expected to be less or "
                              "equal to s2.len (%u).\n", (unsigned int) i, (unsigned int) t->dif_pos,
                              (unsigned int) t->s2.len);
              return -1;
            }
          for(k = 0; k <= m_len + 1 && !c_failed[i]; k++)
            {
              if (k <= t->dif_pos)
                {
                  if (!MHD_str_equal_caseless_n_(t->s1.str, t->s2.str, k))
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", %u) returned zero,"
                                      " while expected non-zero. Locale: %s\n",
                                      n_prnt(t->s1.str), n_prnt(t->s2.str), (unsigned int) k, get_current_locale_str());
                    }
                  else if (!MHD_str_equal_caseless_n_(t->s2.str, t->s1.str, k))
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", %u) returned zero,"
                                      " while expected non-zero. Locale: %s\n",
                                      n_prnt(t->s2.str), n_prnt(t->s1.str), (unsigned int) k, get_current_locale_str());
                    }
                }
              else
                {
                  if (MHD_str_equal_caseless_n_(t->s1.str, t->s2.str, k))
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", %u) returned non-zero,"
                                      " while expected zero. Locale: %s\n",
                                      n_prnt(t->s1.str), n_prnt(t->s2.str), (unsigned int) k, get_current_locale_str());
                    }
                  else if (MHD_str_equal_caseless_n_(t->s2.str, t->s1.str, k))
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", %u) returned non-zero,"
                                      " while expected zero. Locale: %s\n",
                                      n_prnt(t->s2.str), n_prnt(t->s1.str), (unsigned int) k, get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            {
              printf("PASSED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", N) != 0 && \\\n"
                     "        MHD_str_equal_caseless_n_(\"%s\", \"%s\", N) != 0, where N is 0..%u\n",
                     n_prnt(t->s1.str), n_prnt(t->s2.str), n_prnt(t->s2.str), n_prnt(t->s1.str),
                     (unsigned int) t->dif_pos);

              printf("PASSED: MHD_str_equal_caseless_n_(\"%s\", \"%s\", N) == 0 && \\\n"
                     "        MHD_str_equal_caseless_n_(\"%s\", \"%s\", N) == 0, where N is %u..%u\n",
                     n_prnt(t->s1.str), n_prnt(t->s2.str), n_prnt(t->s2.str), n_prnt(t->s1.str),
                     (unsigned int) t->dif_pos + 1, (unsigned int) m_len + 1);
            }
        }
    }
  return t_failed;
}

/*
 * Run eq/neq strings tests
 */
int run_eq_neq_str_tests(void)
{
  int str_equal_caseless_fails = 0;
  int str_equal_caseless_n_fails = 0;
  int res;

  res = check_eq_strings();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_eq_strings().\n");
          return 99;
        }
      str_equal_caseless_fails += res;
      fprintf(stderr, "FAILED: testcase check_eq_strings() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_eq_strings() successfully passed.\n\n");

  res = check_neq_strings();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_neq_strings().\n");
          return 99;
        }
      str_equal_caseless_fails += res;
      fprintf(stderr, "FAILED: testcase check_neq_strings() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_neq_strings() successfully passed.\n\n");

  if (str_equal_caseless_fails)
    fprintf(stderr, "FAILED: function MHD_str_equal_caseless_() failed %d time%s.\n\n",
                     str_equal_caseless_fails, str_equal_caseless_fails == 1 ? "" : "s");
  else if (verbose > 0)
    printf("PASSED: function MHD_str_equal_caseless_() successfully passed all checks.\n\n");

  res = check_eq_strings_n();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_eq_strings_n().\n");
          return 99;
        }
      str_equal_caseless_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_eq_strings_n() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_eq_strings_n() successfully passed.\n\n");

  res = check_neq_strings_n();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_neq_strings_n().\n");
          return 99;
        }
      str_equal_caseless_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_neq_strings_n() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_neq_strings_n() successfully passed.\n\n");

  if (str_equal_caseless_n_fails)
    fprintf(stderr, "FAILED: function MHD_str_equal_caseless_n_() failed %d time%s.\n\n",
                     str_equal_caseless_n_fails, str_equal_caseless_n_fails == 1 ? "" : "s");
  else if (verbose > 0)
    printf("PASSED: function MHD_str_equal_caseless_n_() successfully passed all checks.\n\n");

  if (str_equal_caseless_fails || str_equal_caseless_n_fails)
    {
      if (verbose > 0)
        printf("At least one test failed.\n");

      return 1;
    }

  if (verbose > 0)
    printf("All tests passed successfully.\n");

  return 0;
}

/*
 * Digits in string -> value tests
 */

struct str_with_value {
  const struct str_with_len str;
  const size_t num_of_digt;
  const uint64_t val;
};

/* valid string for conversion to unsigned integer value */
static const struct str_with_value dstrs_w_values[] = {
    /* simplest strings */
    {D_STR_W_LEN("1"), 1, 1},
    {D_STR_W_LEN("0"), 1, 0},
    {D_STR_W_LEN("10000"), 5, 10000},

    /* all digits */
    {D_STR_W_LEN("1234"), 4, 1234},
    {D_STR_W_LEN("4567"), 4, 4567},
    {D_STR_W_LEN("7890"), 4, 7890},
    {D_STR_W_LEN("8021"), 4, 8021},
    {D_STR_W_LEN("9754"), 4, 9754},
    {D_STR_W_LEN("6392"), 4, 6392},

    /* various prefixes */
    {D_STR_W_LEN("00000000"), 8, 0},
    {D_STR_W_LEN("0755"), 4, 755}, /* not to be interpreted as octal value! */
    {D_STR_W_LEN("002"), 3, 2},
    {D_STR_W_LEN("0001"), 4, 1},
    {D_STR_W_LEN("00000000000000000000000031295483"), 32, 31295483},

    /* numbers below and above limits */
    {D_STR_W_LEN("127"), 3, 127}, /* 0x7F, SCHAR_MAX */
    {D_STR_W_LEN("128"), 3, 128}, /* 0x80, SCHAR_MAX+1 */
    {D_STR_W_LEN("255"), 3, 255}, /* 0xFF, UCHAR_MAX */
    {D_STR_W_LEN("256"), 3, 256}, /* 0x100, UCHAR_MAX+1 */
    {D_STR_W_LEN("32767"), 5, 32767}, /* 0x7FFF, INT16_MAX */
    {D_STR_W_LEN("32768"), 5, 32768}, /* 0x8000, INT16_MAX+1 */
    {D_STR_W_LEN("65535"), 5, 65535}, /* 0xFFFF, UINT16_MAX */
    {D_STR_W_LEN("65536"), 5, 65536}, /* 0x10000, UINT16_MAX+1 */
    {D_STR_W_LEN("2147483647"), 10, 2147483647}, /* 0x7FFFFFFF, INT32_MAX */
    {D_STR_W_LEN("2147483648"), 10, UINT64_C(2147483648)}, /* 0x80000000, INT32_MAX+1 */
    {D_STR_W_LEN("4294967295"), 10, UINT64_C(4294967295)}, /* 0xFFFFFFFF, UINT32_MAX */
    {D_STR_W_LEN("4294967296"), 10, UINT64_C(4294967296)}, /* 0x100000000, UINT32_MAX+1 */
    {D_STR_W_LEN("9223372036854775807"), 19, UINT64_C(9223372036854775807)}, /* 0x7FFFFFFFFFFFFFFF, INT64_MAX */
    {D_STR_W_LEN("9223372036854775808"), 19, UINT64_C(9223372036854775808)}, /* 0x8000000000000000, INT64_MAX+1 */
    {D_STR_W_LEN("18446744073709551615"), 20, UINT64_C(18446744073709551615)},  /* 0xFFFFFFFFFFFFFFFF, UINT64_MAX */

    /* random numbers */
    {D_STR_W_LEN("10186753"), 8, 10186753},
    {D_STR_W_LEN("144402566"), 9, 144402566},
    {D_STR_W_LEN("151903144"), 9, 151903144},
    {D_STR_W_LEN("139264621"), 9, 139264621},
    {D_STR_W_LEN("730348"), 6, 730348},
    {D_STR_W_LEN("21584377"), 8, 21584377},
    {D_STR_W_LEN("709"), 3, 709},
    {D_STR_W_LEN("54"), 2, 54},
    {D_STR_W_LEN("8452"), 4, 8452},
    {D_STR_W_LEN("17745098750013624977"), 20, UINT64_C(17745098750013624977)},
    {D_STR_W_LEN("06786878769931678000"), 20, UINT64_C(6786878769931678000)},

    /* non-digit suffixes */
    {D_STR_W_LEN("1234oa"), 4, 1234},
    {D_STR_W_LEN("20h"), 2, 20}, /* not to be interpreted as hex value! */
    {D_STR_W_LEN("0x1F"), 1, 0}, /* not to be interpreted as hex value! */
    {D_STR_W_LEN("0564`~}"), 4, 564},
    {D_STR_W_LEN("7240146.724"), 7, 7240146},
    {D_STR_W_LEN("2,9"), 1, 2},
    {D_STR_W_LEN("200+1"), 3, 200},
    {D_STR_W_LEN("1a"), 1, 1},
    {D_STR_W_LEN("2E"), 1, 2},
    {D_STR_W_LEN("6c"), 1, 6},
    {D_STR_W_LEN("8F"), 1, 8},
    {D_STR_W_LEN("287416997! And the not too long string."), 9, 287416997}
};

/* strings that should overflow uint64_t */
const struct str_with_len str_ovflw[] = {
    D_STR_W_LEN("18446744073709551616"), /* 0x10000000000000000, UINT64_MAX+1 */
    D_STR_W_LEN("18446744073709551620"),
    D_STR_W_LEN("18446744083709551615"),
    D_STR_W_LEN("19234761020556472143"),
    D_STR_W_LEN("184467440737095516150"),
    D_STR_W_LEN("1844674407370955161500"),
    D_STR_W_LEN("000018446744073709551616"), /* 0x10000000000000000, UINT64_MAX+1 */
    D_STR_W_LEN("20000000000000000000"),
    D_STR_W_LEN("020000000000000000000"),
    D_STR_W_LEN("0020000000000000000000"),
    D_STR_W_LEN("100000000000000000000"),
    D_STR_W_LEN("434532891232591226417"),
    D_STR_W_LEN("99999999999999999999"),
    D_STR_W_LEN("18446744073709551616abcd"), /* 0x10000000000000000, UINT64_MAX+1 */
    D_STR_W_LEN("20000000000000000000 suffix"),
    D_STR_W_LEN("020000000000000000000x")
};

/* strings that should not be convertible to numeric value */
const struct str_with_len str_no_num[] = {
    D_STR_W_LEN("zero"),
    D_STR_W_LEN("one"),
    D_STR_W_LEN("\xb9\xb2\xb3"),                /* superscript "123" in ISO-8859-1/CP1252 */
    D_STR_W_LEN("\xc2\xb9\xc2\xb2\xc2\xb3"),    /* superscript "123" in UTF-8 */
    D_STR_W_LEN("\xd9\xa1\xd9\xa2\xd9\xa3"),    /* Arabic-Indic "١٢٣" in UTF-8 */
    D_STR_W_LEN("\xdb\xb1\xdb\xb2\xdb\xb3"),    /* Ext Arabic-Indic "۱۲۳" in UTF-8 */
    D_STR_W_LEN("\xe0\xa5\xa7\xe0\xa5\xa8\xe0\xa5\xa9"), /* Devanagari "१२३" in UTF-8 */
    D_STR_W_LEN("\xe4\xb8\x80\xe4\xba\x8c\xe4\xb8\x89"), /* Chinese "一二三" in UTF-8 */
    D_STR_W_LEN("\xd2\xbb\xb6\xfe\xc8\xfd"),    /* Chinese "一二三" in GB2312/CP936 */
    D_STR_W_LEN("\x1B\x24\x29\x41\x0E\x52\x3B\x36\x7E\x48\x7D\x0F") /* Chinese "一二三" in ISO-2022-CN */
};

/* valid hex string for conversion to unsigned integer value */
static const struct str_with_value xdstrs_w_values[] = {
    /* simplest strings */
    {D_STR_W_LEN("1"), 1, 0x1},
    {D_STR_W_LEN("0"), 1, 0x0},
    {D_STR_W_LEN("10000"), 5, 0x10000},

    /* all digits */
    {D_STR_W_LEN("1234"), 4, 0x1234},
    {D_STR_W_LEN("4567"), 4, 0x4567},
    {D_STR_W_LEN("7890"), 4, 0x7890},
    {D_STR_W_LEN("8021"), 4, 0x8021},
    {D_STR_W_LEN("9754"), 4, 0x9754},
    {D_STR_W_LEN("6392"), 4, 0x6392},
    {D_STR_W_LEN("abcd"), 4, 0xABCD},
    {D_STR_W_LEN("cdef"), 4, 0xCDEF},
    {D_STR_W_LEN("FEAB"), 4, 0xFEAB},
    {D_STR_W_LEN("BCED"), 4, 0xBCED},
    {D_STR_W_LEN("bCeD"), 4, 0xBCED},
    {D_STR_W_LEN("1A5F"), 4, 0x1A5F},
    {D_STR_W_LEN("12AB"), 4, 0x12AB},
    {D_STR_W_LEN("CD34"), 4, 0xCD34},
    {D_STR_W_LEN("56EF"), 4, 0x56EF},
    {D_STR_W_LEN("7a9f"), 4, 0x7A9F},

    /* various prefixes */
    {D_STR_W_LEN("00000000"), 8, 0x0},
    {D_STR_W_LEN("0755"), 4, 0x755}, /* not to be interpreted as octal value! */
    {D_STR_W_LEN("002"), 3, 0x2},
    {D_STR_W_LEN("0001"), 4, 0x1},
    {D_STR_W_LEN("00a"), 3, 0xA},
    {D_STR_W_LEN("0F"), 2, 0xF},
    {D_STR_W_LEN("0000000000000000000000003A29e4C3"), 32, 0x3A29E4C3},

    /* numbers below and above limits */
    {D_STR_W_LEN("7F"), 2, 127}, /* 0x7F, SCHAR_MAX */
    {D_STR_W_LEN("7f"), 2, 127}, /* 0x7F, SCHAR_MAX */
    {D_STR_W_LEN("80"), 2, 128}, /* 0x80, SCHAR_MAX+1 */
    {D_STR_W_LEN("fF"), 2, 255}, /* 0xFF, UCHAR_MAX */
    {D_STR_W_LEN("Ff"), 2, 255}, /* 0xFF, UCHAR_MAX */
    {D_STR_W_LEN("FF"), 2, 255}, /* 0xFF, UCHAR_MAX */
    {D_STR_W_LEN("ff"), 2, 255}, /* 0xFF, UCHAR_MAX */
    {D_STR_W_LEN("100"), 3, 256}, /* 0x100, UCHAR_MAX+1 */
    {D_STR_W_LEN("7fff"), 4, 32767}, /* 0x7FFF, INT16_MAX */
    {D_STR_W_LEN("7FFF"), 4, 32767}, /* 0x7FFF, INT16_MAX */
    {D_STR_W_LEN("7Fff"), 4, 32767}, /* 0x7FFF, INT16_MAX */
    {D_STR_W_LEN("8000"), 4, 32768}, /* 0x8000, INT16_MAX+1 */
    {D_STR_W_LEN("ffff"), 4, 65535}, /* 0xFFFF, UINT16_MAX */
    {D_STR_W_LEN("FFFF"), 4, 65535}, /* 0xFFFF, UINT16_MAX */
    {D_STR_W_LEN("FffF"), 4, 65535}, /* 0xFFFF, UINT16_MAX */
    {D_STR_W_LEN("10000"), 5, 65536}, /* 0x10000, UINT16_MAX+1 */
    {D_STR_W_LEN("7FFFFFFF"), 8, 2147483647}, /* 0x7FFFFFFF, INT32_MAX */
    {D_STR_W_LEN("7fffffff"), 8, 2147483647}, /* 0x7FFFFFFF, INT32_MAX */
    {D_STR_W_LEN("7FFffFff"), 8, 2147483647}, /* 0x7FFFFFFF, INT32_MAX */
    {D_STR_W_LEN("80000000"), 8, UINT64_C(2147483648)}, /* 0x80000000, INT32_MAX+1 */
    {D_STR_W_LEN("FFFFFFFF"), 8, UINT64_C(4294967295)}, /* 0xFFFFFFFF, UINT32_MAX */
    {D_STR_W_LEN("ffffffff"), 8, UINT64_C(4294967295)}, /* 0xFFFFFFFF, UINT32_MAX */
    {D_STR_W_LEN("FfFfFfFf"), 8, UINT64_C(4294967295)}, /* 0xFFFFFFFF, UINT32_MAX */
    {D_STR_W_LEN("100000000"), 9, UINT64_C(4294967296)}, /* 0x100000000, UINT32_MAX+1 */
    {D_STR_W_LEN("7fffffffffffffff"), 16, UINT64_C(9223372036854775807)}, /* 0x7FFFFFFFFFFFFFFF, INT64_MAX */
    {D_STR_W_LEN("7FFFFFFFFFFFFFFF"), 16, UINT64_C(9223372036854775807)}, /* 0x7FFFFFFFFFFFFFFF, INT64_MAX */
    {D_STR_W_LEN("7FfffFFFFffFFffF"), 16, UINT64_C(9223372036854775807)}, /* 0x7FFFFFFFFFFFFFFF, INT64_MAX */
    {D_STR_W_LEN("8000000000000000"), 16, UINT64_C(9223372036854775808)}, /* 0x8000000000000000, INT64_MAX+1 */
    {D_STR_W_LEN("ffffffffffffffff"), 16, UINT64_C(18446744073709551615)},  /* 0xFFFFFFFFFFFFFFFF, UINT64_MAX */
    {D_STR_W_LEN("FFFFFFFFFFFFFFFF"), 16, UINT64_C(18446744073709551615)},  /* 0xFFFFFFFFFFFFFFFF, UINT64_MAX */
    {D_STR_W_LEN("FffFffFFffFFfFFF"), 16, UINT64_C(18446744073709551615)},  /* 0xFFFFFFFFFFFFFFFF, UINT64_MAX */

    /* random numbers */
    {D_STR_W_LEN("10186753"), 8, 0x10186753},
    {D_STR_W_LEN("144402566"), 9, 0x144402566},
    {D_STR_W_LEN("151903144"), 9, 0x151903144},
    {D_STR_W_LEN("139264621"), 9, 0x139264621},
    {D_STR_W_LEN("730348"), 6, 0x730348},
    {D_STR_W_LEN("21584377"), 8, 0x21584377},
    {D_STR_W_LEN("709"), 3, 0x709},
    {D_STR_W_LEN("54"), 2, 0x54},
    {D_STR_W_LEN("8452"), 4, 0x8452},
    {D_STR_W_LEN("22353EC6"), 8, 0x22353EC6},
    {D_STR_W_LEN("307F1655"), 8, 0x307F1655},
    {D_STR_W_LEN("1FCB7226"), 8, 0x1FCB7226},
    {D_STR_W_LEN("82480560"), 8, 0x82480560},
    {D_STR_W_LEN("7386D95"), 7, 0x7386D95},
    {D_STR_W_LEN("EC3AB"), 5, 0xEC3AB},
    {D_STR_W_LEN("6DD05"), 5, 0x6DD05},
    {D_STR_W_LEN("C5DF"), 4, 0xC5DF},
    {D_STR_W_LEN("6CE"), 3, 0x6CE},
    {D_STR_W_LEN("CE6"), 3, 0xCE6},
    {D_STR_W_LEN("ce6"), 3, 0xCE6},
    {D_STR_W_LEN("F27"), 3, 0xF27},
    {D_STR_W_LEN("8497D54277D7E1"), 14, UINT64_C(37321639124785121)},
    {D_STR_W_LEN("8497d54277d7e1"), 14, UINT64_C(37321639124785121)},
    {D_STR_W_LEN("8497d54277d7E1"), 14, UINT64_C(37321639124785121)},
    {D_STR_W_LEN("8C8112D0A06"), 11, UINT64_C(9655374645766)},
    {D_STR_W_LEN("8c8112d0a06"), 11, UINT64_C(9655374645766)},
    {D_STR_W_LEN("8c8112d0A06"), 11, UINT64_C(9655374645766)},
    {D_STR_W_LEN("1774509875001362"), 16, UINT64_C(1690064375898968930)},
    {D_STR_W_LEN("0678687876998000"), 16, UINT64_C(466237428027981824)},

    /* non-digit suffixes */
    {D_STR_W_LEN("1234oa"), 4, 0x1234},
    {D_STR_W_LEN("20h"), 2, 0x20},
    {D_STR_W_LEN("2CH"), 2, 0x2C},
    {D_STR_W_LEN("2ch"), 2, 0x2C},
    {D_STR_W_LEN("0x1F"), 1, 0x0}, /* not to be interpreted as hex prefix! */
    {D_STR_W_LEN("0564`~}"), 4, 0x564},
    {D_STR_W_LEN("0A64`~}"), 4, 0xA64},
    {D_STR_W_LEN("056c`~}"), 4, 0X56C},
    {D_STR_W_LEN("7240146.724"), 7, 0x7240146},
    {D_STR_W_LEN("7E4c1AB.724"), 7, 0X7E4C1AB},
    {D_STR_W_LEN("F24B1B6.724"), 7, 0xF24B1B6},
    {D_STR_W_LEN("2,9"), 1, 0x2},
    {D_STR_W_LEN("a,9"), 1, 0xA},
    {D_STR_W_LEN("200+1"), 3, 0x200},
    {D_STR_W_LEN("2cc+1"), 3, 0x2CC},
    {D_STR_W_LEN("2cC+1"), 3, 0x2CC},
    {D_STR_W_LEN("27416997! And the not too long string."), 8, 0x27416997},
    {D_STR_W_LEN("27555416997! And the not too long string."), 11, 0x27555416997},
    {D_STR_W_LEN("416997And the not too long string."), 7, 0x416997A},
    {D_STR_W_LEN("0F4C3Dabstract addition to make string even longer"), 8, 0xF4C3DAB}
};

/* hex strings that should overflow uint64_t */
const struct str_with_len strx_ovflw[] = {
    D_STR_W_LEN("10000000000000000"),           /* 0x10000000000000000, UINT64_MAX+1 */
    D_STR_W_LEN("10000000000000001"),
    D_STR_W_LEN("10000000000000002"),
    D_STR_W_LEN("1000000000000000A"),
    D_STR_W_LEN("11000000000000000"),
    D_STR_W_LEN("010000000000000000"),          /* 0x10000000000000000, UINT64_MAX+1 */
    D_STR_W_LEN("000010000000000000000"),       /* 0x10000000000000000, UINT64_MAX+1 */
    D_STR_W_LEN("20000000000000000000"),
    D_STR_W_LEN("020000000000000000000"),
    D_STR_W_LEN("0020000000000000000000"),
    D_STR_W_LEN("20000000000000000"),
    D_STR_W_LEN("A0000000000000000"),
    D_STR_W_LEN("F0000000000000000"),
    D_STR_W_LEN("a0000000000000000"),
    D_STR_W_LEN("11111111111111111"),
    D_STR_W_LEN("CcCcCCccCCccCCccC"),
    D_STR_W_LEN("f0000000000000000"),
    D_STR_W_LEN("100000000000000000000"),
    D_STR_W_LEN("434532891232591226417"),
    D_STR_W_LEN("10000000000000000a"),
    D_STR_W_LEN("10000000000000000E"),
    D_STR_W_LEN("100000000000000000 and nothing"), /* 0x10000000000000000, UINT64_MAX+1 */
    D_STR_W_LEN("100000000000000000xx"),        /* 0x10000000000000000, UINT64_MAX+1 */
    D_STR_W_LEN("99999999999999999999"),
    D_STR_W_LEN("18446744073709551616abcd"),
    D_STR_W_LEN("20000000000000000000 suffix"),
    D_STR_W_LEN("020000000000000000000x")
};


int check_str_to_uint64_valid(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(dstrs_w_values) / sizeof(dstrs_w_values[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          uint64_t rv;
          size_t rs;
          const struct str_with_value * const t = dstrs_w_values + i;

          if (c_failed[i])
            continue; /* skip already failed checks */

          if (t->str.len < t->num_of_digt)
            {
              fprintf(stderr, "ERROR: dstrs_w_values[%u] has wrong num_of_digt (%u): num_of_digt is expected"
                              " to be less or equal to str.len (%u).\n",
                              (unsigned int) i, (unsigned int) t->num_of_digt, (unsigned int) t->str.len);
              return -1;
            }
          rv = 9435223; /* some random value */
          rs = MHD_str_to_uint64_(t->str.str, &rv);
          if (rs != t->num_of_digt)
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_str_to_uint64_(\"%s\", ->%" PRIu64 ") returned %" PRIuPTR ", while expecting %d."
                      " Locale: %s\n", n_prnt(t->str.str), rv, (intptr_t)rs, (int)t->num_of_digt, get_current_locale_str());
            }
          if (rv != t->val)
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_str_to_uint64_(\"%s\", ->%" PRIu64 ") converted string to value %" PRIu64 ","
                      " while expecting result %" PRIu64 ". Locale: %s\n", n_prnt(t->str.str), rv, rv,
                      t->val, get_current_locale_str());
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_to_uint64_(\"%s\", ->%" PRIu64 ") == %" PRIuPTR "\n",
                   n_prnt(t->str.str), rv, rs);
        }
    }
  return t_failed;
}


int check_str_to_uint64_all_chars(void)
{
  static const size_t n_checks = 256; /* from 0 to 255 */
  int c_failed[n_checks];
  size_t t_failed = 0;
  size_t j;

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      unsigned int c;
      uint64_t test_val;

      set_test_locale(j); /* setlocale() can be slow! */
      for(c = 0; c < n_checks; c++)
        {
          static const uint64_t rnd_val = 24941852;
          size_t rs;
          if (c >= '0' && c <= '9')
            continue; /* skip digits */
          for(test_val = 0; test_val <= rnd_val&& !c_failed[c]; test_val += rnd_val)
            {
              char test_str[] = "0123";
              uint64_t rv = test_val;

              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */
              rs = MHD_str_to_uint64_(test_str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[c] = !0;
                  fprintf(stderr, "FAILED: MHD_str_to_uint64_(\"%s\", ->%" PRIu64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(test_str), rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[c] = !0;
                  fprintf(stderr, "FAILED: MHD_str_to_uint64_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: %" PRIu64 ", after call %" PRIu64 "). Locale: %s\n",
                                  n_prnt(test_str), test_val, rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[c])
            {
              char test_str[] = "0123";
              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */

              printf("PASSED: MHD_str_to_uint64_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                     n_prnt(test_str));
            }
        }
    }
  return t_failed;
}


int check_str_to_uint64_overflow(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(str_ovflw) / sizeof(str_ovflw[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = str_ovflw + i;
          static const uint64_t rnd_val = 2;
          uint64_t test_val;

          for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
            {
              uint64_t rv = test_val;

              rs = MHD_str_to_uint64_(t->str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_str_to_uint64_(\"%s\", ->%" PRIu64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(t->str), rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_str_to_uint64_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: %" PRIu64 ", after call %" PRIu64 "). Locale: %s\n",
                                  n_prnt(t->str), test_val, rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_to_uint64_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                   n_prnt(t->str));
        }
    }
  return t_failed;
}


int check_str_to_uint64_no_val(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(str_no_num) / sizeof(str_no_num[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = str_no_num + i;
          static const uint64_t rnd_val = 74218431;
          uint64_t test_val;

          for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
            {
              uint64_t rv = test_val;

              rs = MHD_str_to_uint64_(t->str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_str_to_uint64_(\"%s\", ->%" PRIu64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(t->str), rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_str_to_uint64_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: %" PRIu64 ", after call %" PRIu64 "). Locale: %s\n",
                                  n_prnt(t->str), test_val, rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_to_uint64_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                   n_prnt(t->str));
        }
    }
  return t_failed;
}


int check_str_to_uint64_n_valid(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(dstrs_w_values) / sizeof(dstrs_w_values[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          uint64_t rv = 1235572; /* some random value */
          size_t rs = 0;
          size_t len;
          const struct str_with_value * const t = dstrs_w_values + i;

          if (t->str.len < t->num_of_digt)
            {
              fprintf(stderr, "ERROR: dstrs_w_values[%u] has wrong num_of_digt (%u): num_of_digt is expected"
                              " to be less or equal to str.len (%u).\n",
                              (unsigned int) i, (unsigned int) t->num_of_digt, (unsigned int) t->str.len);
              return -1;
            }
          for (len = t->num_of_digt; len <= t->str.len + 1 && !c_failed[i]; len++)
            {
              rs = MHD_str_to_uint64_n_(t->str.str, len, &rv);
              if (rs != t->num_of_digt)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR ", ->%" PRIu64 ")"
                          " returned %" PRIuPTR ", while expecting %d. Locale: %s\n",
                          n_prnt(t->str.str), (intptr_t)len, rv, (intptr_t)rs,
                          (int)t->num_of_digt, get_current_locale_str());
                }
              if (rv != t->val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR ", ->%" PRIu64 ")"
                          " converted string to value %" PRIu64 ", while expecting result %" PRIu64
                          ". Locale: %s\n", n_prnt(t->str.str), (intptr_t)len, rv, rv,
                          t->val, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR "..%" PRIuPTR ", ->%" PRIu64 ")"
                   " == %" PRIuPTR "\n", n_prnt(t->str.str), (intptr_t)t->num_of_digt,
                   (intptr_t)t->str.len + 1, rv, rs);
        }
    }
  return t_failed;
}


int check_str_to_uint64_n_all_chars(void)
{
  static const size_t n_checks = 256; /* from 0 to 255 */
  int c_failed[n_checks];
  size_t t_failed = 0;
  size_t j;

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      unsigned int c;
      uint64_t test_val;

      set_test_locale(j); /* setlocale() can be slow! */
      for(c = 0; c < n_checks; c++)
        {
          static const uint64_t rnd_val = 98372558;
          size_t rs;
          size_t len;

          if (c >= '0' && c <= '9')
            continue; /* skip digits */

          for (len = 0; len <= 5; len++)
            {
              for(test_val = 0; test_val <= rnd_val&& !c_failed[c]; test_val += rnd_val)
                {
                  char test_str[] = "0123";
                  uint64_t rv = test_val;

                  test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */
                  rs = MHD_str_to_uint64_n_(test_str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[c] = !0;
                      fprintf(stderr, "FAILED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR ", ->%" PRIu64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(test_str), (uintptr_t)len, rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[c] = !0;
                      fprintf(stderr, "FAILED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: %" PRIu64 ", after call %" PRIu64 ")."
                              " Locale: %s\n",
                              n_prnt(test_str), (uintptr_t)len, test_val, rv, get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[c])
            {
              char test_str[] = "0123";
              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */

              printf("PASSED: MHD_str_to_uint64_n_(\"%s\", 0..5, &ret_val) == 0, value of ret_val is unmodified\n",
                     n_prnt(test_str));
            }
        }
    }
  return t_failed;
}


int check_str_to_uint64_n_overflow(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(str_ovflw) / sizeof(str_ovflw[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = str_ovflw + i;
          static const uint64_t rnd_val = 3;
          size_t len;

          for (len = t->len; len <= t->len + 1; len++)
            {
              uint64_t test_val;
              for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
                {
                  uint64_t rv = test_val;

                  rs = MHD_str_to_uint64_n_(t->str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR ", ->%" PRIu64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(t->str), (uintptr_t)len, rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: %" PRIu64 ", after call %" PRIu64 ")."
                              " Locale: %s\n", n_prnt(t->str), (uintptr_t)len, test_val, rv,
                              get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR "..%" PRIuPTR ", &ret_val) == 0,"
                   " value of ret_val is unmodified\n", n_prnt(t->str), (uintptr_t)t->len,
                   (uintptr_t)t->len + 1);
        }
    }
  return t_failed;
}


int check_str_to_uint64_n_no_val(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(str_no_num) / sizeof(str_no_num[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = str_no_num + i;
          static const uint64_t rnd_val = 43255654342;
          size_t len;

          for (len = 0; len <= t->len + 1; len++)
            {
              uint64_t test_val;
              for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
                {
                  uint64_t rv = test_val;

                  rs = MHD_str_to_uint64_n_(t->str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR ", ->%" PRIu64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(t->str), (uintptr_t)len, rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_str_to_uint64_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: %" PRIu64 ", after call %" PRIu64 ")."
                              " Locale: %s\n", n_prnt(t->str), (uintptr_t)len, test_val, rv,
                              get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_str_to_uint64_n_(\"%s\", 0..%" PRIuPTR ", &ret_val) == 0,"
                   " value of ret_val is unmodified\n", n_prnt(t->str),
                   (uintptr_t)t->len + 1);
        }
    }
  return t_failed;
}


int check_strx_to_uint32_valid(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(xdstrs_w_values) / sizeof(xdstrs_w_values[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          uint32_t rv;
          size_t rs;
          const struct str_with_value * const t = xdstrs_w_values + i;

          if (t->val > UINT32_MAX)
            continue; /* number is too high for this function */

          if (c_failed[i])
            continue; /* skip already failed checks */

          if (t->str.len < t->num_of_digt)
            {
              fprintf(stderr, "ERROR: xdstrs_w_values[%u] has wrong num_of_digt (%u): num_of_digt is expected"
                              " to be less or equal to str.len (%u).\n",
                              (unsigned int) i, (unsigned int) t->num_of_digt, (unsigned int) t->str.len);
              return -1;
            }
          rv = 1458532; /* some random value */
          rs = MHD_strx_to_uint32_(t->str.str, &rv);
          if (rs != t->num_of_digt)
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_strx_to_uint32_(\"%s\", ->0x%" PRIX64 ") returned %" PRIuPTR ", while expecting %d."
                      " Locale: %s\n", n_prnt(t->str.str), (uint64_t)rv, (intptr_t)rs, (int)t->num_of_digt, get_current_locale_str());
            }
          if (rv != t->val)
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_strx_to_uint32_(\"%s\", ->0x%" PRIX64 ") converted string to value 0x%" PRIX64 ","
                      " while expecting result 0x%" PRIX64 ". Locale: %s\n", n_prnt(t->str.str), (uint64_t)rv, (uint64_t)rv,
                      t->val, get_current_locale_str());
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint32_(\"%s\", ->0x%" PRIX64 ") == %" PRIuPTR "\n",
                   n_prnt(t->str.str), (uint64_t)rv, rs);
        }
    }
  return t_failed;
}


int check_strx_to_uint32_all_chars(void)
{
  static const size_t n_checks = 256; /* from 0 to 255 */
  int c_failed[n_checks];
  size_t t_failed = 0;
  size_t j;

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      unsigned int c;
      uint32_t test_val;

      set_test_locale(j); /* setlocale() can be slow! */
      for(c = 0; c < n_checks; c++)
        {
          static const uint32_t rnd_val = 234234;
          size_t rs;
          if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') ||(c >= 'a' && c <= 'f'))
            continue; /* skip xdigits */
          for(test_val = 0; test_val <= rnd_val&& !c_failed[c]; test_val += rnd_val)
            {
              char test_str[] = "0123";
              uint32_t rv = test_val;

              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */
              rs = MHD_strx_to_uint32_(test_str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[c] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint32_(\"%s\", ->0x%" PRIX64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(test_str), (uint64_t)rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[c] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint32_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 "). Locale: %s\n",
                                  n_prnt(test_str), (uint64_t)test_val, (uint64_t)rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[c])
            {
              char test_str[] = "0123";
              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */

              printf("PASSED: MHD_strx_to_uint32_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                     n_prnt(test_str));
            }
        }
    }
  return t_failed;
}


int check_strx_to_uint32_overflow(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks1 = sizeof(strx_ovflw) / sizeof(strx_ovflw[0]);
  static const size_t n_checks = sizeof(strx_ovflw) / sizeof(strx_ovflw[0]) +
                                 sizeof(xdstrs_w_values) / sizeof(xdstrs_w_values[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          static const uint32_t rnd_val = 74218431;
          uint32_t test_val;
          const char * str;
          if (i < n_checks1)
            {
              const struct str_with_len * const t = strx_ovflw + i;
              str = t->str;
            }
          else
            {
              const struct str_with_value * const t = xdstrs_w_values + (i - n_checks1);
              if (t->val <= UINT32_MAX)
                continue; /* check only strings that should overflow uint32_t */
              str = t->str.str;
            }


          for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
            {
              uint32_t rv = test_val;

              rs = MHD_strx_to_uint32_(str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint32_(\"%s\", ->0x%" PRIX64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(str), (uint64_t)rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint32_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 "). Locale: %s\n",
                                  n_prnt(str), (uint64_t)test_val, (uint64_t)rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint32_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                   n_prnt(str));
        }
    }
  return t_failed;
}


int check_strx_to_uint32_no_val(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(str_no_num) / sizeof(str_no_num[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = str_no_num + i;
          static const uint32_t rnd_val = 74218431;
          uint32_t test_val;

          for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
            {
              uint32_t rv = test_val;

              rs = MHD_strx_to_uint32_(t->str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint32_(\"%s\", ->0x%" PRIX64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(t->str), (uint64_t)rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint32_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 "). Locale: %s\n",
                                  n_prnt(t->str), (uint64_t)test_val, (uint64_t)rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint32_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                   n_prnt(t->str));
        }
    }
  return t_failed;
}


int check_strx_to_uint32_n_valid(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(xdstrs_w_values) / sizeof(xdstrs_w_values[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          uint32_t rv = 2352932;  /* some random value */
          size_t rs = 0;
          size_t len;
          const struct str_with_value * const t = xdstrs_w_values + i;

          if (t->val > UINT32_MAX)
            continue; /* number is too high for this function */

          if (t->str.len < t->num_of_digt)
            {
              fprintf(stderr, "ERROR: xdstrs_w_values[%u] has wrong num_of_digt (%u): num_of_digt is expected"
                              " to be less or equal to str.len (%u).\n",
                              (unsigned int) i, (unsigned int) t->num_of_digt, (unsigned int) t->str.len);
              return -1;
            }
          for (len = t->num_of_digt; len <= t->str.len + 1 && !c_failed[i]; len++)
            {
              rs = MHD_strx_to_uint32_n_(t->str.str, len, &rv);
              if (rs != t->num_of_digt)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                          " returned %" PRIuPTR ", while expecting %d. Locale: %s\n",
                          n_prnt(t->str.str), (intptr_t)len, (uint64_t)rv, (intptr_t)rs,
                          (int)t->num_of_digt, get_current_locale_str());
                }
              if (rv != t->val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                          " converted string to value 0x%" PRIX64 ", while expecting result 0x%" PRIX64
                          ". Locale: %s\n", n_prnt(t->str.str), (intptr_t)len, (uint64_t)rv, (uint64_t)rv,
                          t->val, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR "..%" PRIuPTR ", ->0x%" PRIX64 ")"
                   " == %" PRIuPTR "\n", n_prnt(t->str.str), (intptr_t)t->num_of_digt,
                   (intptr_t)t->str.len + 1, (uint64_t)rv, rs);
        }
    }
  return t_failed;
}


int check_strx_to_uint32_n_all_chars(void)
{
  static const size_t n_checks = 256; /* from 0 to 255 */
  int c_failed[n_checks];
  size_t t_failed = 0;
  size_t j;

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      unsigned int c;
      uint32_t test_val;

      set_test_locale(j); /* setlocale() can be slow! */
      for(c = 0; c < n_checks; c++)
        {
          static const uint32_t rnd_val = 98372558;
          size_t rs;
          size_t len;

          if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
            continue; /* skip xdigits */

          for (len = 0; len <= 5; len++)
            {
              for(test_val = 0; test_val <= rnd_val&& !c_failed[c]; test_val += rnd_val)
                {
                  char test_str[] = "0123";
                  uint32_t rv = test_val;

                  test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */
                  rs = MHD_strx_to_uint32_n_(test_str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[c] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(test_str), (uintptr_t)len, (uint64_t)rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[c] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 ")."
                              " Locale: %s\n",
                              n_prnt(test_str), (uintptr_t)len, (uint64_t)test_val, (uint64_t)rv, get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[c])
            {
              char test_str[] = "0123";
              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */

              printf("PASSED: MHD_strx_to_uint32_n_(\"%s\", 0..5, &ret_val) == 0, value of ret_val is unmodified\n",
                     n_prnt(test_str));
            }
        }
    }
  return t_failed;
}


int check_strx_to_uint32_n_overflow(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks1 = sizeof(strx_ovflw) / sizeof(strx_ovflw[0]);
  static const size_t n_checks = sizeof(strx_ovflw) / sizeof(strx_ovflw[0]) +
                                 sizeof(xdstrs_w_values) / sizeof(xdstrs_w_values[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          static const uint32_t rnd_val = 4;
          size_t len;
          const char * str;
          size_t min_len, max_len;
          if (i < n_checks1)
            {
              const struct str_with_len * const t = strx_ovflw + i;
              str = t->str;
              min_len = t->len;
              max_len = t->len + 1;
            }
          else
            {
              const struct str_with_value * const t = xdstrs_w_values + (i - n_checks1);
              if (t->val <= UINT32_MAX)
                continue; /* check only strings that should overflow uint32_t */

              if (t->str.len < t->num_of_digt)
                {
                  fprintf(stderr, "ERROR: xdstrs_w_values[%u] has wrong num_of_digt (%u): num_of_digt is expected"
                                  " to be less or equal to str.len (%u).\n",
                                  (unsigned int) (i - n_checks1), (unsigned int) t->num_of_digt,
                                  (unsigned int) t->str.len);
                  return -1;
                }
              str = t->str.str;
              min_len = t->num_of_digt;
              max_len = t->str.len + 1;
            }

          for (len = min_len; len <= max_len; len++)
            {
              uint32_t test_val;
              for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
                {
                  uint32_t rv = test_val;

                  rs = MHD_strx_to_uint32_n_(str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(str), (uintptr_t)len, (uint64_t)rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 ")."
                              " Locale: %s\n", n_prnt(str), (uintptr_t)len, (uint64_t)test_val, (uint64_t)rv,
                              get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR "..%" PRIuPTR ", &ret_val) == 0,"
                   " value of ret_val is unmodified\n", n_prnt(str), (uintptr_t)min_len,
                   (uintptr_t)max_len);
        }
    }
  return t_failed;
}


int check_strx_to_uint32_n_no_val(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(str_no_num) / sizeof(str_no_num[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = str_no_num + i;
          static const uint32_t rnd_val = 3214314212L;
          size_t len;

          for (len = 0; len <= t->len + 1; len++)
            {
              uint32_t test_val;
              for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
                {
                  uint32_t rv = test_val;

                  rs = MHD_strx_to_uint32_n_(t->str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(t->str), (uintptr_t)len, (uint64_t)rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint32_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 ")."
                              " Locale: %s\n", n_prnt(t->str), (uintptr_t)len, (uint64_t)test_val, (uint64_t)rv,
                              get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint32_n_(\"%s\", 0..%" PRIuPTR ", &ret_val) == 0,"
                   " value of ret_val is unmodified\n", n_prnt(t->str),
                   (uintptr_t)t->len + 1);
        }
    }
  return t_failed;
}


int check_strx_to_uint64_valid(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(xdstrs_w_values) / sizeof(xdstrs_w_values[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          uint64_t rv;
          size_t rs;
          const struct str_with_value * const t = xdstrs_w_values + i;

          if (c_failed[i])
            continue; /* skip already failed checks */

          if (t->str.len < t->num_of_digt)
            {
              fprintf(stderr, "ERROR: xdstrs_w_values[%u] has wrong num_of_digt (%u): num_of_digt is expected"
                              " to be less or equal to str.len (%u).\n",
                              (unsigned int) i, (unsigned int) t->num_of_digt, (unsigned int) t->str.len);
              return -1;
            }
          rv = 1458532; /* some random value */
          rs = MHD_strx_to_uint64_(t->str.str, &rv);
          if (rs != t->num_of_digt)
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_strx_to_uint64_(\"%s\", ->0x%" PRIX64 ") returned %" PRIuPTR ", while expecting %d."
                      " Locale: %s\n", n_prnt(t->str.str), rv, (intptr_t)rs, (int)t->num_of_digt, get_current_locale_str());
            }
          if (rv != t->val)
            {
              t_failed++;
              c_failed[i] = !0;
              fprintf(stderr, "FAILED: MHD_strx_to_uint64_(\"%s\", ->0x%" PRIX64 ") converted string to value 0x%" PRIX64 ","
                      " while expecting result 0x%" PRIX64 ". Locale: %s\n", n_prnt(t->str.str), rv, rv,
                      t->val, get_current_locale_str());
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint64_(\"%s\", ->0x%" PRIX64 ") == %" PRIuPTR "\n",
                   n_prnt(t->str.str), rv, rs);
        }
    }
  return t_failed;
}


int check_strx_to_uint64_all_chars(void)
{
  static const size_t n_checks = 256; /* from 0 to 255 */
  int c_failed[n_checks];
  size_t t_failed = 0;
  size_t j;

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      unsigned int c;
      uint64_t test_val;

      set_test_locale(j); /* setlocale() can be slow! */
      for(c = 0; c < n_checks; c++)
        {
          static const uint64_t rnd_val = 234234;
          size_t rs;
          if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
            continue; /* skip xdigits */
          for(test_val = 0; test_val <= rnd_val&& !c_failed[c]; test_val += rnd_val)
            {
              char test_str[] = "0123";
              uint64_t rv = test_val;

              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */
              rs = MHD_strx_to_uint64_(test_str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[c] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint64_(\"%s\", ->0x%" PRIX64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(test_str), rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[c] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint64_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 "). Locale: %s\n",
                                  n_prnt(test_str), test_val, rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[c])
            {
              char test_str[] = "0123";
              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */

              printf("PASSED: MHD_strx_to_uint64_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                     n_prnt(test_str));
            }
        }
    }
  return t_failed;
}


int check_strx_to_uint64_overflow(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(strx_ovflw) / sizeof(strx_ovflw[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = strx_ovflw + i;
          static const uint64_t rnd_val = 74218431;
          uint64_t test_val;

          for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
            {
              uint64_t rv = test_val;

              rs = MHD_strx_to_uint64_(t->str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint64_(\"%s\", ->0x%" PRIX64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(t->str), rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint64_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 "). Locale: %s\n",
                                  n_prnt(t->str), test_val, rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint64_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                   n_prnt(t->str));
        }
    }
  return t_failed;
}


int check_strx_to_uint64_no_val(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(str_no_num) / sizeof(str_no_num[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = str_no_num + i;
          static const uint64_t rnd_val = 74218431;
          uint64_t test_val;

          for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
            {
              uint64_t rv = test_val;

              rs = MHD_strx_to_uint64_(t->str, &rv);
              if (rs != 0)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint64_(\"%s\", ->0x%" PRIX64 ") returned %" PRIuPTR ", while expecting zero."
                                  " Locale: %s\n", n_prnt(t->str), rv, (uintptr_t)rs, get_current_locale_str());
                }
              else if (rv != test_val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint64_(\"%s\", &ret_val) modified value of ret_val"
                                  " (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 "). Locale: %s\n",
                                  n_prnt(t->str), test_val, rv, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint64_(\"%s\", &ret_val) == 0, value of ret_val is unmodified\n",
                   n_prnt(t->str));
        }
    }
  return t_failed;
}


int check_strx_to_uint64_n_valid(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(xdstrs_w_values) / sizeof(xdstrs_w_values[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          uint64_t rv = 2352932; /* some random value */
          size_t rs = 0;
          size_t len;
          const struct str_with_value * const t = xdstrs_w_values + i;

          if (t->str.len < t->num_of_digt)
            {
              fprintf(stderr, "ERROR: xdstrs_w_values[%u] has wrong num_of_digt (%u): num_of_digt is expected"
                              " to be less or equal to str.len (%u).\n",
                              (unsigned int) i, (unsigned int) t->num_of_digt, (unsigned int) t->str.len);
              return -1;
            }
          for (len = t->num_of_digt; len <= t->str.len + 1 && !c_failed[i]; len++)
            {
              rs = MHD_strx_to_uint64_n_(t->str.str, len, &rv);
              if (rs != t->num_of_digt)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                          " returned %" PRIuPTR ", while expecting %d. Locale: %s\n",
                          n_prnt(t->str.str), (intptr_t)len, rv, (intptr_t)rs,
                          (int)t->num_of_digt, get_current_locale_str());
                }
              if (rv != t->val)
                {
                  t_failed++;
                  c_failed[i] = !0;
                  fprintf(stderr, "FAILED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                          " converted string to value 0x%" PRIX64 ", while expecting result 0x%" PRIX64
                          ". Locale: %s\n", n_prnt(t->str.str), (intptr_t)len, rv, rv,
                          t->val, get_current_locale_str());
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR "..%" PRIuPTR ", ->0x%" PRIX64 ")"
                   " == %" PRIuPTR "\n", n_prnt(t->str.str), (intptr_t)t->num_of_digt,
                   (intptr_t)t->str.len + 1, rv, rs);
        }
    }
  return t_failed;
}


int check_strx_to_uint64_n_all_chars(void)
{
  static const size_t n_checks = 256; /* from 0 to 255 */
  int c_failed[n_checks];
  size_t t_failed = 0;
  size_t j;

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      unsigned int c;
      uint64_t test_val;

      set_test_locale(j); /* setlocale() can be slow! */
      for(c = 0; c < n_checks; c++)
        {
          static const uint64_t rnd_val = 98372558;
          size_t rs;
          size_t len;

          if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
            continue; /* skip xdigits */

          for (len = 0; len <= 5; len++)
            {
              for(test_val = 0; test_val <= rnd_val&& !c_failed[c]; test_val += rnd_val)
                {
                  char test_str[] = "0123";
                  uint64_t rv = test_val;

                  test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */
                  rs = MHD_strx_to_uint64_n_(test_str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[c] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(test_str), (uintptr_t)len, rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[c] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 ")."
                              " Locale: %s\n",
                              n_prnt(test_str), (uintptr_t)len, test_val, rv, get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[c])
            {
              char test_str[] = "0123";
              test_str[0] = (char) (unsigned char)c; /* replace first char with non-digit char */

              printf("PASSED: MHD_strx_to_uint64_n_(\"%s\", 0..5, &ret_val) == 0, value of ret_val is unmodified\n",
                     n_prnt(test_str));
            }
        }
    }
  return t_failed;
}


int check_strx_to_uint64_n_overflow(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(strx_ovflw) / sizeof(strx_ovflw[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = strx_ovflw + i;
          static const uint64_t rnd_val = 4;
          size_t len;

          for (len = t->len; len <= t->len + 1; len++)
            {
              uint64_t test_val;
              for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
                {
                  uint64_t rv = test_val;

                  rs = MHD_strx_to_uint64_n_(t->str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(t->str), (uintptr_t)len, rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 ")."
                              " Locale: %s\n", n_prnt(t->str), (uintptr_t)len, test_val, rv,
                              get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR "..%" PRIuPTR ", &ret_val) == 0,"
                   " value of ret_val is unmodified\n", n_prnt(t->str), (uintptr_t)t->len,
                   (uintptr_t)t->len + 1);
        }
    }
  return t_failed;
}


int check_strx_to_uint64_n_no_val(void)
{
  size_t t_failed = 0;
  size_t i, j;
  static const size_t n_checks = sizeof(str_no_num) / sizeof(str_no_num[0]);
  int c_failed[n_checks];

  memset(c_failed, 0, sizeof(c_failed));

  for(j = 0; j < locale_name_count; j++)
    {
      set_test_locale(j); /* setlocale() can be slow! */
      for(i = 0; i < n_checks; i++)
        {
          size_t rs;
          const struct str_with_len * const t = str_no_num + i;
          static const uint64_t rnd_val = 3214314212L;
          size_t len;

          for (len = 0; len <= t->len + 1; len++)
            {
              uint64_t test_val;
              for(test_val = 0; test_val <= rnd_val && !c_failed[i]; test_val += rnd_val)
                {
                  uint64_t rv = test_val;

                  rs = MHD_strx_to_uint64_n_(t->str, len, &rv);
                  if (rs != 0)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR ", ->0x%" PRIX64 ")"
                              " returned %" PRIuPTR ", while expecting zero. Locale: %s\n",
                              n_prnt(t->str), (uintptr_t)len, rv, (uintptr_t)rs, get_current_locale_str());
                    }
                  else if (rv != test_val)
                    {
                      t_failed++;
                      c_failed[i] = !0;
                      fprintf(stderr, "FAILED: MHD_strx_to_uint64_n_(\"%s\", %" PRIuPTR ", &ret_val)"
                              " modified value of ret_val (before call: 0x%" PRIX64 ", after call 0x%" PRIX64 ")."
                              " Locale: %s\n", n_prnt(t->str), (uintptr_t)len, test_val, rv,
                              get_current_locale_str());
                    }
                }
            }
          if (verbose > 1 && j == locale_name_count - 1 && !c_failed[i])
            printf("PASSED: MHD_strx_to_uint64_n_(\"%s\", 0..%" PRIuPTR ", &ret_val) == 0,"
                   " value of ret_val is unmodified\n", n_prnt(t->str),
                   (uintptr_t)t->len + 1);
        }
    }
  return t_failed;
}


int run_str_to_X_tests(void)
{
  int str_to_uint64_fails = 0;
  int str_to_uint64_n_fails = 0;
  int strx_to_uint32_fails = 0;
  int strx_to_uint32_n_fails = 0;
  int strx_to_uint64_fails = 0;
  int strx_to_uint64_n_fails = 0;
  int res;

  res = check_str_to_uint64_valid();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_str_to_uint64_valid().\n");
          return 99;
        }
      str_to_uint64_fails += res;
      fprintf(stderr, "FAILED: testcase check_str_to_uint64_valid() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_str_to_uint64_valid() successfully passed.\n\n");

  res = check_str_to_uint64_all_chars();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_str_to_uint64_all_chars().\n");
          return 99;
        }
      str_to_uint64_fails += res;
      fprintf(stderr, "FAILED: testcase check_str_to_uint64_all_chars() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_str_to_uint64_all_chars() successfully passed.\n\n");

  res = check_str_to_uint64_overflow();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_str_to_uint64_overflow().\n");
          return 99;
        }
      str_to_uint64_fails += res;
      fprintf(stderr, "FAILED: testcase check_str_to_uint64_overflow() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_str_to_uint64_overflow() successfully passed.\n\n");

  res = check_str_to_uint64_no_val();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_str_to_uint64_no_val().\n");
          return 99;
        }
      str_to_uint64_fails += res;
      fprintf(stderr, "FAILED: testcase check_str_to_uint64_no_val() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_str_to_uint64_no_val() successfully passed.\n\n");

  if (str_to_uint64_fails)
    fprintf(stderr, "FAILED: function MHD_str_to_uint64_() failed %d time%s.\n\n",
                    str_to_uint64_fails, str_to_uint64_fails == 1 ? "" : "s");
  else if (verbose > 0)
    printf("PASSED: function MHD_str_to_uint64_() successfully passed all checks.\n\n");

  res = check_str_to_uint64_n_valid();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_str_to_uint64_n_valid().\n");
          return 99;
        }
      str_to_uint64_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_str_to_uint64_n_valid() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_str_to_uint64_n_valid() successfully passed.\n\n");

  res = check_str_to_uint64_n_all_chars();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_str_to_uint64_n_all_chars().\n");
          return 99;
        }
      str_to_uint64_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_str_to_uint64_n_all_chars() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_str_to_uint64_n_all_chars() successfully passed.\n\n");

  res = check_str_to_uint64_n_overflow();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_str_to_uint64_n_overflow().\n");
          return 99;
        }
      str_to_uint64_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_str_to_uint64_n_overflow() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_str_to_uint64_n_overflow() successfully passed.\n\n");

  res = check_str_to_uint64_n_no_val();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_str_to_uint64_n_no_val().\n");
          return 99;
        }
      str_to_uint64_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_str_to_uint64_n_no_val() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_str_to_uint64_n_no_val() successfully passed.\n\n");

  if (str_to_uint64_n_fails)
    fprintf(stderr, "FAILED: function MHD_str_to_uint64_n_() failed %d time%s.\n\n",
                    str_to_uint64_n_fails, str_to_uint64_n_fails == 1 ? "" : "s");
  else if (verbose > 0)
    printf("PASSED: function MHD_str_to_uint64_n_() successfully passed all checks.\n\n");

  res = check_strx_to_uint32_valid();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint32_valid().\n");
          return 99;
        }
      strx_to_uint32_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint32_valid() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint32_valid() successfully passed.\n\n");

  res = check_strx_to_uint32_all_chars();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint32_all_chars().\n");
          return 99;
        }
      strx_to_uint32_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint32_all_chars() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint32_all_chars() successfully passed.\n\n");

  res = check_strx_to_uint32_overflow();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint32_overflow().\n");
          return 99;
        }
      strx_to_uint32_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint32_overflow() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint32_overflow() successfully passed.\n\n");

  res = check_strx_to_uint32_no_val();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint32_no_val().\n");
          return 99;
        }
      strx_to_uint32_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint32_no_val() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint32_no_val() successfully passed.\n\n");

  if (strx_to_uint32_fails)
    fprintf(stderr, "FAILED: function MHD_strx_to_uint32_() failed %d time%s.\n\n",
                    strx_to_uint32_fails, strx_to_uint32_fails == 1 ? "" : "s");
  else if (verbose > 0)
    printf("PASSED: function MHD_strx_to_uint32_() successfully passed all checks.\n\n");

  res = check_strx_to_uint32_n_valid();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint32_n_valid().\n");
          return 99;
        }
      strx_to_uint32_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint32_n_valid() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint32_n_valid() successfully passed.\n\n");

  res = check_strx_to_uint32_n_all_chars();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint32_n_all_chars().\n");
          return 99;
        }
      strx_to_uint32_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint32_n_all_chars() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint32_n_all_chars() successfully passed.\n\n");

  res = check_strx_to_uint32_n_overflow();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint32_n_overflow().\n");
          return 99;
        }
      strx_to_uint32_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint32_n_overflow() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint32_n_overflow() successfully passed.\n\n");

  res = check_strx_to_uint32_n_no_val();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint32_n_no_val().\n");
          return 99;
        }
      strx_to_uint32_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint32_n_no_val() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint32_n_no_val() successfully passed.\n\n");

  if (strx_to_uint32_n_fails)
    fprintf(stderr, "FAILED: function MHD_strx_to_uint32_n_() failed %d time%s.\n\n",
                    strx_to_uint32_n_fails, strx_to_uint32_n_fails == 1 ? "" : "s");
  else if (verbose > 0)
    printf("PASSED: function MHD_strx_to_uint32_n_() successfully passed all checks.\n\n");

  res = check_strx_to_uint64_valid();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint64_valid().\n");
          return 99;
        }
      strx_to_uint64_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint64_valid() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint64_valid() successfully passed.\n\n");

  res = check_strx_to_uint64_all_chars();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint64_all_chars().\n");
          return 99;
        }
      strx_to_uint64_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint64_all_chars() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint64_all_chars() successfully passed.\n\n");

  res = check_strx_to_uint64_overflow();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint64_overflow().\n");
          return 99;
        }
      strx_to_uint64_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint64_overflow() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint64_overflow() successfully passed.\n\n");

  res = check_strx_to_uint64_no_val();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint64_no_val().\n");
          return 99;
        }
      strx_to_uint64_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint64_no_val() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint64_no_val() successfully passed.\n\n");

  if (strx_to_uint64_fails)
    fprintf(stderr, "FAILED: function MHD_strx_to_uint64_() failed %d time%s.\n\n",
                    strx_to_uint64_fails, strx_to_uint64_fails == 1 ? "" : "s");
  else if (verbose > 0)
    printf("PASSED: function MHD_strx_to_uint64_() successfully passed all checks.\n\n");

  res = check_strx_to_uint64_n_valid();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint64_n_valid().\n");
          return 99;
        }
      strx_to_uint64_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint64_n_valid() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint64_n_valid() successfully passed.\n\n");

  res = check_strx_to_uint64_n_all_chars();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint64_n_all_chars().\n");
          return 99;
        }
      strx_to_uint64_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint64_n_all_chars() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint64_n_all_chars() successfully passed.\n\n");

  res = check_strx_to_uint64_n_overflow();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint64_n_overflow().\n");
          return 99;
        }
      strx_to_uint64_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint64_n_overflow() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint64_n_overflow() successfully passed.\n\n");

  res = check_strx_to_uint64_n_no_val();
  if (res != 0)
    {
      if (res < 0)
        {
          fprintf(stderr, "ERROR: test internal error in check_strx_to_uint64_n_no_val().\n");
          return 99;
        }
      strx_to_uint64_n_fails += res;
      fprintf(stderr, "FAILED: testcase check_strx_to_uint64_n_no_val() failed.\n\n");
    }
  else if (verbose > 1)
    printf("PASSED: testcase check_strx_to_uint64_n_no_val() successfully passed.\n\n");

  if (strx_to_uint64_n_fails)
    fprintf(stderr, "FAILED: function MHD_strx_to_uint64_n_() failed %d time%s.\n\n",
                    strx_to_uint64_n_fails, strx_to_uint64_n_fails == 1 ? "" : "s");
  else if (verbose > 0)
    printf("PASSED: function MHD_strx_to_uint64_n_() successfully passed all checks.\n\n");

  if (str_to_uint64_fails || str_to_uint64_n_fails ||
      strx_to_uint32_fails || strx_to_uint32_n_fails ||
      strx_to_uint64_fails || strx_to_uint64_n_fails)
    {
      if (verbose > 0)
        printf("At least one test failed.\n");

      return 1;
    }

  if (verbose > 0)
    printf("All tests passed successfully.\n");

  return 0;
}


int main(int argc, char * argv[])
{
  if (has_param(argc, argv, "-v") || has_param(argc, argv, "--verbose") || has_param(argc, argv, "--verbose1"))
    verbose = 1;
  if (has_param(argc, argv, "-vv") || has_param(argc, argv, "--verbose2"))
    verbose = 2;
  if (has_param(argc, argv, "-vvv") || has_param(argc, argv, "--verbose3"))
    verbose = 3;

  if (has_in_name(argv[0], "_to_value"))
    return run_str_to_X_tests();

  return run_eq_neq_str_tests();
}
