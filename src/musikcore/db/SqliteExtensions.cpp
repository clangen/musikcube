//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "pch.hpp"

#pragma warning(push, 0)
#include <sqlite/sqlite3.h>
#include <musikcore/db/SqliteExtensions.h>
#pragma warning(pop)

#include <unordered_map>
#include <regex>

/* NOTE: nearly all of the source here was pulled in from sqlite3.c except for the
extension registration and accentToChar mapping; this is basically a copy/paste of
the default LIKE operator that supports coercing accented characters to standard
english characters to make it easier to search. */

#ifndef UINT32_TYPE
# ifdef HAVE_UINT32_T
#  define UINT32_TYPE uint32_t
# else
#  define UINT32_TYPE unsigned int
# endif
#endif
#ifndef UINT8_TYPE
# ifdef HAVE_UINT8_T
#  define UINT8_TYPE uint8_t
# else
#  define UINT8_TYPE unsigned char
# endif
#endif

typedef UINT8_TYPE u8;             /* 1-byte unsigned integer */
typedef UINT32_TYPE u32;           /* 4-byte unsigned integer */

/* mappings from ASCII characters to a list of their respective accented
characters. both case sensitive and insensitive versions are supplied here. */
static std::unordered_map<u32, const char*> charToAccentsInsensitive = {
    { (u32)'a', u8"AaàáâãäåāăąÀÁÂÃÄÅĀĂĄ" },
    { (u32)'c', u8"CcçćĉċčÇĆĈĊČ" },
    { (u32)'d', u8"DdďđĎĐ" },
    { (u32)'e', u8"EeèéêëēĕėęěÈÉÊËĒĔĖĘĚ" },
    { (u32)'g', u8"GgĝğġģĜĞĠĢ" },
    { (u32)'h', u8"HhĥħĤĦ" },
    { (u32)'i', u8"IiìíîïĩīĭįıÌÍÎÏĨĪĬĮİ" },
    { (u32)'j', u8"JjĵĴ" },
    { (u32)'k', u8"KkķĸĶ" },
    { (u32)'l', u8"LlĺļľŀłĹĻĽĿŁ" },
    { (u32)'n', u8"NnñńņňŊÑŃŅŇŉŋ" },
    { (u32)'o', u8"OoòóôõöùúûüōŏőÒÓÔÕÖŌŎŐ" },
    { (u32)'r', u8"RrŕŗřŔŖŘ" },
    { (u32)'s', u8"SsśŝşšſŚŜŞŠßŚŜŞŠß" },
    { (u32)'t', u8"TtţťŧŢŤŦ" },
    { (u32)'u', u8"UuũūŭůűųÙÚÛÜŨŪŬŮŰŲ" },
    { (u32)'w', u8"WwŵŴ" },
    { (u32)'y', u8"YyýÿŷÝŶŸ" },
    { (u32)'z', u8"ZzŹŻŽ" }
};

static std::unordered_map<u32, const char*> charToAccentsSensitive = {
    { (u32)'A', u8"AÀÁÂÃÄÅĀĂĄ" },
    { (u32)'C', u8"CÇĆĈĊČ" },
    { (u32)'D', u8"DĎĐ" },
    { (u32)'E', u8"EÈÉÊËĒĔĖĘĚ" },
    { (u32)'G', u8"GĜĞĠĢ" },
    { (u32)'H', u8"HĤĦ" },
    { (u32)'I', u8"IÌÍÎÏĨĪĬĮİ" },
    { (u32)'J', u8"JĴ" },
    { (u32)'K', u8"KĶ" },
    { (u32)'L', u8"LĹĻĽĿŁ" },
    { (u32)'N', u8"NÑŃŅŇŉŋ" },
    { (u32)'O', u8"OÒÓÔÕÖŌŎŐ" },
    { (u32)'R', u8"RŔŖŘ" },
    { (u32)'S', u8"SŚŜŞŠßŚŜŞŠß" },
    { (u32)'T', u8"TŢŤŦ" },
    { (u32)'U', u8"UÙÚÛÜŨŪŬŮŰŲ" },
    { (u32)'W', u8"WŴ" },
    { (u32)'Y', u8"YÝŶŸ" },
    { (u32)'Z', u8"ZŹŻŽ" },
    { (u32)'a', u8"aàáâãäåāăą" },
    { (u32)'c', u8"cçćĉċč" },
    { (u32)'d', u8"dďđ" },
    { (u32)'e', u8"eèéêëēĕėęě" },
    { (u32)'g', u8"gĝğġģ" },
    { (u32)'h', u8"hĥħ" },
    { (u32)'i', u8"iìíîïĩīĭįı" },
    { (u32)'j', u8"jĵ" },
    { (u32)'k', u8"kķĸ" },
    { (u32)'l', u8"lĺļľŀł" },
    { (u32)'n', u8"nñńņňŊ" },
    { (u32)'o', u8"oòóôõöùúûüōŏő" },
    { (u32)'r', u8"rŕŗř" },
    { (u32)'s', u8"sśŝşšſ" },
    { (u32)'t', u8"tţťŧ" },
    { (u32)'u', u8"uũūŭůűų" },
    { (u32)'w', u8"wŵ" },
    { (u32)'y', u8"yýÿŷ" },
};

/*
** Mapping from accented UTF8 sequence to a regular english char
*/
static std::unordered_map<u32, u32> accentToChar = {
    /* A */
    { (u32)0xc0  /* À */ , (u32)'A' },
    { (u32)0xc1  /* Á */ , (u32)'A' },
    { (u32)0xc2  /* Â */ , (u32)'A' },
    { (u32)0xc3  /* Ã */ , (u32)'A' },
    { (u32)0xc4  /* Ä */ , (u32)'A' },
    { (u32)0xc5  /* Å */ , (u32)'A' },
    { (u32)0x100 /* Ā */ , (u32)'A' },
    { (u32)0x102 /* Ă */ , (u32)'A' },
    { (u32)0x104 /* Ą */ , (u32)'A' },
    /* C */
    { (u32)0xc7  /* Ç */ , (u32)'C' },
    { (u32)0x106 /* Ć */ , (u32)'C' },
    { (u32)0x108 /* Ĉ */ , (u32)'C' },
    { (u32)0x10a /* Ċ */ , (u32)'C' },
    { (u32)0x10c /* Č */ , (u32)'C' },
    /* D */
    { (u32)0x10e /* Ď */ , (u32)'D' },
    { (u32)0x110 /* Đ */ , (u32)'D' },
    /* E */
    { (u32)0xc8  /* È */ , (u32)'E' },
    { (u32)0xc9  /* É */ , (u32)'E' },
    { (u32)0xca  /* Ê */ , (u32)'E' },
    { (u32)0xcb  /* Ë */ , (u32)'E' },
    { (u32)0x112 /* Ē */ , (u32)'E' },
    { (u32)0x114 /* Ĕ */ , (u32)'E' },
    { (u32)0x116 /* Ė */ , (u32)'E' },
    { (u32)0x118 /* Ę */ , (u32)'E' },
    { (u32)0x11a /* Ě */ , (u32)'E' },
    /* G */
    { (u32)0x11c /* Ĝ */ , (u32)'G' },
    { (u32)0x11e /* Ğ */ , (u32)'G' },
    { (u32)0x120 /* Ġ */ , (u32)'G' },
    { (u32)0x122 /* Ģ */ , (u32)'G' },
    /* H */
    { (u32)0x124 /* Ĥ */ , (u32)'H' },
    { (u32)0x126 /* Ħ */ , (u32)'H' },
    /* I */
    { (u32)0xcc  /* Ì */ , (u32)'I' },
    { (u32)0xcd  /* Í */ , (u32)'I' },
    { (u32)0xce  /* Î */ , (u32)'I' },
    { (u32)0xcf  /* Ï */ , (u32)'I' },
    { (u32)0x128 /* Ĩ */ , (u32)'I' },
    { (u32)0x12a /* Ī */ , (u32)'I' },
    { (u32)0x12c /* Ĭ */ , (u32)'I' },
    { (u32)0x12e /* Į */ , (u32)'I' },
    { (u32)0x130 /* İ */ , (u32)'I' },
    /* J */
    { (u32)0x134 /* Ĵ */ , (u32)'J' },
    /* K */
    { (u32)0x136 /* Ķ */ , (u32)'K' },
    /* L */
    { (u32)0x139 /* Ĺ */ , (u32)'L' },
    { (u32)0x13b /* Ļ */ , (u32)'L' },
    { (u32)0x13d /* Ľ */ , (u32)'L' },
    { (u32)0x13f /* Ŀ */ , (u32)'L' },
    { (u32)0x141 /* Ł */ , (u32)'L' },
    /* N */
    { (u32)0xd1  /* Ñ */ , (u32)'N' },
    { (u32)0x143 /* Ń */ , (u32)'N' },
    { (u32)0x145 /* Ņ */ , (u32)'N' },
    { (u32)0x147 /* Ň */ , (u32)'N' },
    { (u32)0x149 /* ŉ */ , (u32)'N' },
    { (u32)0x14b /* ŋ */ , (u32)'N' },
    /* O */
    { (u32)0xd2  /* Ò */ , (u32)'O' },
    { (u32)0xd3  /* Ó */ , (u32)'O' },
    { (u32)0xd4  /* Ô */ , (u32)'O' },
    { (u32)0xd5  /* Õ */ , (u32)'O' },
    { (u32)0xd6  /* Ö */ , (u32)'O' },
    { (u32)0x14c /* Ō */ , (u32)'O' },
    { (u32)0x14e /* Ŏ */ , (u32)'O' },
    { (u32)0x150 /* Ő */ , (u32)'O' },
    /* R */
    { (u32)0x154 /* Ŕ */ , (u32)'R' },
    { (u32)0x156 /* Ŗ */ , (u32)'R' },
    { (u32)0x158 /* Ř */ , (u32)'R' },
    /* S */
    { (u32)0x15a /* Ś */ , (u32)'S' },
    { (u32)0x15c /* Ŝ */ , (u32)'S' },
    { (u32)0x15e /* Ş */ , (u32)'S' },
    { (u32)0x160 /* Š */ , (u32)'S' },
    /* T */
    { (u32)0x162 /* Ţ */ , (u32)'T' },
    { (u32)0x164 /* Ť */ , (u32)'T' },
    { (u32)0x166 /* Ŧ */ , (u32)'T' },
    /* U */
    { (u32)0xd9  /* Ù */ , (u32)'U' },
    { (u32)0xda  /* Ú */ , (u32)'U' },
    { (u32)0xdb  /* Û */ , (u32)'U' },
    { (u32)0xdc  /* Ü */ , (u32)'U' },
    { (u32)0x168 /* Ũ */ , (u32)'U' },
    { (u32)0x16a /* Ū */ , (u32)'U' },
    { (u32)0x16c /* Ŭ */ , (u32)'U' },
    { (u32)0x16e /* Ů */ , (u32)'U' },
    { (u32)0x170 /* Ű */ , (u32)'U' },
    { (u32)0x172 /* Ų */ , (u32)'U' },
    /* W */
    { (u32)0x174 /* Ŵ */ , (u32)'W' },
    /* Y */
    { (u32)0xdd  /* Ý */ , (u32)'Y' },
    { (u32)0x176 /* Ŷ */ , (u32)'Y' },
    { (u32)0x178 /* Ÿ */ , (u32)'Y' },
    /* S */
    { (u32)0xdf  /* ß */ , (u32)'s' },
    /* Z */
    { (u32)0x179 /* Ź */ , (u32)'Z' },
    { (u32)0x17b /* Ż */ , (u32)'Z' },
    { (u32)0x17d /* Ž */ , (u32)'Z' },
    /* a */
    { (u32)0xe0  /* à */ , (u32)'a' },
    { (u32)0xe1  /* á */ , (u32)'a' },
    { (u32)0xe2  /* â */ , (u32)'a' },
    { (u32)0xe3  /* ã */ , (u32)'a' },
    { (u32)0xe4  /* ä */ , (u32)'a' },
    { (u32)0xe5  /* å */ , (u32)'a' },
    { (u32)0x101 /* ā */ , (u32)'a' },
    { (u32)0x103 /* ă */ , (u32)'a' },
    { (u32)0x105 /* ą */ , (u32)'a' },
    /* c */
    { (u32)0xe7  /* ç */ , (u32)'c' },
    { (u32)0x107 /* ć */ , (u32)'c' },
    { (u32)0x109 /* ĉ */ , (u32)'c' },
    { (u32)0x10b /* ċ */ , (u32)'c' },
    { (u32)0x10d /* č */ , (u32)'c' },
    /* d */
    { (u32)0x10f /* ď */ , (u32)'d' },
    { (u32)0x111 /* đ */ , (u32)'d' },
    /* e */
    { (u32)0xe8  /* è */ , (u32)'e' },
    { (u32)0xe9  /* é */ , (u32)'e' },
    { (u32)0xea  /* ê */ , (u32)'e' },
    { (u32)0xeb  /* ë */ , (u32)'e' },
    { (u32)0x113 /* ē */ , (u32)'e' },
    { (u32)0x115 /* ĕ */ , (u32)'e' },
    { (u32)0x117 /* ė */ , (u32)'e' },
    { (u32)0x119 /* ę */ , (u32)'e' },
    { (u32)0x11b /* ě */ , (u32)'e' },
    /* g */
    { (u32)0x11d /* ĝ */ , (u32)'g' },
    { (u32)0x11f /* ğ */ , (u32)'g' },
    { (u32)0x121 /* ġ */ , (u32)'g' },
    { (u32)0x123 /* ģ */ , (u32)'g' },
    /* h */
    { (u32)0x125 /* ĥ */ , (u32)'h' },
    { (u32)0x127 /* ħ */ , (u32)'h' },
    /* i */
    { (u32)0xec  /* ì */ , (u32)'i' },
    { (u32)0xed  /* í */ , (u32)'i' },
    { (u32)0xee  /* î */ , (u32)'i' },
    { (u32)0xef  /* ï */ , (u32)'i' },
    { (u32)0x129 /* ĩ */ , (u32)'i' },
    { (u32)0x12b /* ī */ , (u32)'i' },
    { (u32)0x12d /* ĭ */ , (u32)'i' },
    { (u32)0x12f /* į */ , (u32)'i' },
    { (u32)0x131 /* ı */ , (u32)'i' },
    /* j */
    { (u32)0x135 /* ĵ */ , (u32)'j' },
    /* k */
    { (u32)0x137 /* ķ */ , (u32)'k' },
    { (u32)0x138 /* ĸ */ , (u32)'k' },
    /* l */
    { (u32)0x13a /* ĺ */ , (u32)'l' },
    { (u32)0x13c /* ļ */ , (u32)'l' },
    { (u32)0x13e /* ľ */ , (u32)'l' },
    { (u32)0x140 /* ŀ */ , (u32)'l' },
    { (u32)0x142 /* ł */ , (u32)'l' },
    /* n */
    { (u32)0xf1  /* ñ */ , (u32)'n' },
    { (u32)0x144 /* ń */ , (u32)'n' },
    { (u32)0x146 /* ņ */ , (u32)'n' },
    { (u32)0x148 /* ň */ , (u32)'n' },
    { (u32)0x14a /* Ŋ */ , (u32)'n' },
    /* o */
    { (u32)0xf2  /* ò */ , (u32)'o' },
    { (u32)0xf3  /* ó */ , (u32)'o' },
    { (u32)0xf4  /* ô */ , (u32)'o' },
    { (u32)0xf5  /* õ */ , (u32)'o' },
    { (u32)0xf6  /* ö */ , (u32)'o' },
    { (u32)0xf9  /* ù */ , (u32)'u' },
    { (u32)0xfa  /* ú */ , (u32)'u' },
    { (u32)0xfb  /* û */ , (u32)'u' },
    { (u32)0xfc  /* ü */ , (u32)'u' },
    { (u32)0x14d /* ō */ , (u32)'o' },
    { (u32)0x14f /* ŏ */ , (u32)'o' },
    { (u32)0x151 /* ő */ , (u32)'o' },
    /* r */
    { (u32)0x155 /* ŕ */ , (u32)'r' },
    { (u32)0x157 /* ŗ */ , (u32)'r' },
    { (u32)0x159 /* ř */ , (u32)'r' },
    /* s */
    { (u32)0x15b /* ś */ , (u32)'s' },
    { (u32)0x15d /* ŝ */ , (u32)'s' },
    { (u32)0x15f /* ş */ , (u32)'s' },
    { (u32)0x161 /* š */ , (u32)'s' },
    { (u32)0x17f /* ſ */ , (u32)'s' },
    /* t */
    { (u32)0x163 /* ţ */ , (u32)'t' },
    { (u32)0x165 /* ť */ , (u32)'t' },
    { (u32)0x167 /* ŧ */ , (u32)'t' },
    /* u */
    { (u32)0x169 /* ũ */ , (u32)'u' },
    { (u32)0x16b /* ū */ , (u32)'u' },
    { (u32)0x16d /* ŭ */ , (u32)'u' },
    { (u32)0x16f /* ů */ , (u32)'u' },
    { (u32)0x171 /* ű */ , (u32)'u' },
    { (u32)0x173 /* ų */ , (u32)'u' },
    /* w */
    { (u32)0x175 /* ŵ */ , (u32)'w' },
    /* y */
    { (u32)0xfd  /* ý */ , (u32)'y' },
    { (u32)0xff  /* ÿ */ , (u32)'y' },
    { (u32)0x177 /* ŷ */ , (u32)'y' },
    /* z */
    { (u32)0x17a /* ź */ , (u32)'z' },
    { (u32)0x17c /* ż */ , (u32)'z' },
    { (u32)0x17e /* ž */ , (u32)'z' },
};

/*
** Check to see if this machine uses EBCDIC.  (Yes, believe it or
** not, there are still machines out there that use EBCDIC.)
*/
#if 'A' == '\301'
# define SQLITE_EBCDIC 1
#else
# define SQLITE_ASCII 1
#endif

/* An array to map all upper-case characters into their corresponding
** lower-case character.
**
** SQLite only considers US-ASCII (or EBCDIC) characters.  We do not
** handle case conversions for the UTF character set since the tables
** involved are nearly as big or bigger than SQLite itself.
*/
const unsigned char sqlite3UpperToLower[] = {
#ifdef SQLITE_ASCII
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17,
     18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35,
     36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53,
     54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 97, 98, 99,100,101,102,103,
    104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,
    122, 91, 92, 93, 94, 95, 96, 97, 98, 99,100,101,102,103,104,105,106,107,
    108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,
    126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,
    162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,
    180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,
    198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,
    216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,
    234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,
    252,253,254,255
#endif
#ifdef SQLITE_EBCDIC
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, /* 0x */
     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, /* 1x */
     32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, /* 2x */
     48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, /* 3x */
     64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, /* 4x */
     80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, /* 5x */
     96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111, /* 6x */
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127, /* 7x */
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143, /* 8x */
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159, /* 9x */
    160,161,162,163,164,165,166,167,168,169,170,171,140,141,142,175, /* Ax */
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191, /* Bx */
    192,129,130,131,132,133,134,135,136,137,202,203,204,205,206,207, /* Cx */
    208,145,146,147,148,149,150,151,152,153,218,219,220,221,222,223, /* Dx */
    224,225,162,163,164,165,166,167,168,169,234,235,236,237,238,239, /* Ex */
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255, /* Fx */
#endif
};

/*
** Maximum length (in bytes) of the pattern in a LIKE or GLOB
** operator.
*/
#ifndef SQLITE_MAX_LIKE_PATTERN_LENGTH
# define SQLITE_MAX_LIKE_PATTERN_LENGTH 50000
#endif

/*
** Possible error returns from patternMatch()
*/
#define SQLITE_MATCH             0
#define SQLITE_NOMATCH           1
#define SQLITE_NOWILDCARDMATCH   2

/*
** A structure defining how to do GLOB-style comparisons.
*/
struct compareInfo {
    u8 matchAll; /* "*" or "%" */
    u8 matchOne; /* "?" or "_" */
    u8 matchSet; /* "[" or 0 */
    u8 noCase; /* true to ignore case differences */
};

static const struct compareInfo likeInfoNorm = { '%', '_',   0, 1 };

/*
** Assuming zIn points to the first byte of a UTF-8 character,
** advance zIn to point to the first byte of the next UTF-8 character.
*/
#define SQLITE_SKIP_UTF8(zIn) {                        \
  if( (*(zIn++))>=0xc0 ){                              \
    while( (*zIn & 0xc0)==0x80 ){ zIn++; }             \
  }                                                    \
}

/*
** The following macros mimic the standard library functions toupper(),
** isspace(), isalnum(), isdigit() and isxdigit(), respectively. The
** sqlite versions only work for ASCII characters, regardless of locale.
*/
#ifdef SQLITE_ASCII
# define sqlite3Toupper(x)  ((x)&~(sqlite3CtypeMap[(unsigned char)(x)]&0x20))
# define sqlite3Isspace(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x01)
# define sqlite3Isalnum(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x06)
# define sqlite3Isalpha(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x02)
# define sqlite3Isdigit(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x04)
# define sqlite3Isxdigit(x)  (sqlite3CtypeMap[(unsigned char)(x)]&0x08)
# define sqlite3Tolower(x)   (sqlite3UpperToLower[(unsigned char)(x)])
# define sqlite3Isquote(x)   (sqlite3CtypeMap[(unsigned char)(x)]&0x80)
#else
# define sqlite3Toupper(x)   toupper((unsigned char)(x))
# define sqlite3Isspace(x)   isspace((unsigned char)(x))
# define sqlite3Isalnum(x)   isalnum((unsigned char)(x))
# define sqlite3Isalpha(x)   isalpha((unsigned char)(x))
# define sqlite3Isdigit(x)   isdigit((unsigned char)(x))
# define sqlite3Isxdigit(x)  isxdigit((unsigned char)(x))
# define sqlite3Tolower(x)   tolower((unsigned char)(x))
# define sqlite3Isquote(x)   ((x)=='"'||(x)=='\''||(x)=='['||(x)=='`')
#endif

/*
** For LIKE and GLOB matching on EBCDIC machines, assume that every
** character is exactly one byte in size.  Also, provde the Utf8Read()
** macro for fast reading of the next character in the common case where
** the next character is ASCII.
*/
#if defined(SQLITE_EBCDIC)
# define sqlite3Utf8Read(A)        (*((*A)++))
# define Utf8Read(A)               (*(A++))
#else
# define Utf8Read(A)               (A[0]<0x80?*(A++):sqlite3Utf8Read(&A))
#endif

/*
** This lookup table is used to help decode the first byte of
** a multi-byte UTF8 character.
*/
static const unsigned char sqlite3Utf8Trans1[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
  0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
  0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x00, 0x00,
};

/*
** The following 256 byte lookup table is used to support SQLites built-in
** equivalents to the following standard library functions:
**
**   isspace()                        0x01
**   isalpha()                        0x02
**   isdigit()                        0x04
**   isalnum()                        0x06
**   isxdigit()                       0x08
**   toupper()                        0x20
**   SQLite identifier character      0x40
**   Quote character                  0x80
**
** Bit 0x20 is set if the mapped character requires translation to upper
** case. i.e. if the character is a lower-case ASCII character.
** If x is a lower-case ASCII character, then its upper-case equivalent
** is (x - 0x20). Therefore toupper() can be implemented as:
**
**   (x & ~(map[x]&0x20))
**
** The equivalent of tolower() is implemented using the sqlite3UpperToLower[]
** array. tolower() is used more often than toupper() by SQLite.
**
** Bit 0x40 is set if the character is non-alphanumeric and can be used in an
** SQLite identifier.  Identifiers are alphanumerics, "_", "$", and any
** non-ASCII UTF character. Hence the test for whether or not a character is
** part of an identifier is 0x46.
*/
const unsigned char sqlite3CtypeMap[256] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 00..07    ........ */
  0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,  /* 08..0f    ........ */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 10..17    ........ */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 18..1f    ........ */
  0x01, 0x00, 0x80, 0x00, 0x40, 0x00, 0x00, 0x80,  /* 20..27     !"#$%&' */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 28..2f    ()*+,-./ */
  0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c, 0x0c,  /* 30..37    01234567 */
  0x0c, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 38..3f    89:;<=>? */

  0x00, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a, 0x02,  /* 40..47    @ABCDEFG */
  0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,  /* 48..4f    HIJKLMNO */
  0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,  /* 50..57    PQRSTUVW */
  0x02, 0x02, 0x02, 0x80, 0x00, 0x00, 0x00, 0x40,  /* 58..5f    XYZ[\]^_ */
  0x80, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x2a, 0x22,  /* 60..67    `abcdefg */
  0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,  /* 68..6f    hijklmno */
  0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,  /* 70..77    pqrstuvw */
  0x22, 0x22, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 78..7f    xyz{|}~. */

  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 80..87    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 88..8f    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 90..97    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* 98..9f    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* a0..a7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* a8..af    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* b0..b7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* b8..bf    ........ */

  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* c0..c7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* c8..cf    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* d0..d7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* d8..df    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* e0..e7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* e8..ef    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,  /* f0..f7    ........ */
  0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40   /* f8..ff    ........ */
};

/*
** pZ is a UTF-8 encoded unicode string. If nByte is less than zero,
** return the number of unicode characters in pZ up to (but not including)
** the first 0x00 byte. If nByte is not less than zero, return the
** number of unicode characters in the first nByte of pZ (or up to
** the first 0x00, whichever comes first).
*/
int sqlite3Utf8CharLen(const char* zIn, int nByte) {
    int r = 0;
    const u8* z = (const u8*)zIn;
    const u8* zTerm;
    if (nByte >= 0) {
        zTerm = &z[nByte];
    }
    else {
        zTerm = (const u8*)(-1);
    }
    assert(z <= zTerm);
    while (*z != 0 && z < zTerm) {
        SQLITE_SKIP_UTF8(z);
        r++;
    }
    return r;
}

u32 sqlite3Utf8Read(
    const unsigned char** pz    /* Pointer to string from which to read char */
) {
    unsigned int c;

    /* Same as READ_UTF8() above but without the zTerm parameter.
    ** For this routine, we assume the UTF8 string is always zero-terminated.
    */
    c = *((*pz)++);
    if (c >= 0xc0) {
        c = sqlite3Utf8Trans1[c - 0xc0];
        while ((*(*pz) & 0xc0) == 0x80) {
            c = (c << 6) + (0x3f & *((*pz)++));
        }
        if (c < 0x80
            || (c & 0xFFFFF800) == 0xD800
            || (c & 0xFFFFFFFE) == 0xFFFE) {
            c = 0xFFFD;
        }
    }
    auto it = accentToChar.find(c);
    return it != accentToChar.end() ? it->second : c;
}

/*
** Compare two UTF-8 strings for equality where the first string is
** a GLOB or LIKE expression.  Return values:
**
**    SQLITE_MATCH:            Match
**    SQLITE_NOMATCH:          No match
**    SQLITE_NOWILDCARDMATCH:  No match in spite of having * or % wildcards.
**
** Globbing rules:
**
**      '*'       Matches any sequence of zero or more characters.
**
**      '?'       Matches exactly one character.
**
**     [...]      Matches one character from the enclosed list of
**                characters.
**
**     [^...]     Matches one character not in the enclosed list.
**
** With the [...] and [^...] matching, a ']' character can be included
** in the list by making it the first character after '[' or '^'.  A
** range of characters can be specified using '-'.  Example:
** "[a-z]" matches any single lower-case letter.  To match a '-', make
** it the last character in the list.
**
** Like matching rules:
**
**      '%'       Matches any sequence of zero or more characters
**
***     '_'       Matches any one character
**
**      Ec        Where E is the "esc" character and c is any other
**                character, including '%', '_', and esc, match exactly c.
**
** The comments within this routine usually assume glob matching.
**
** This routine is usually quick, but can be N**2 in the worst case.
*/
static int patternCompare(
    const u8* zPattern,              /* The glob pattern */
    const u8* zString,               /* The string to compare against the glob */
    const struct compareInfo* pInfo, /* Information about how to do the compare */
    u32 matchOther                   /* The escape char (LIKE) or '[' (GLOB) */
) {
    u32 c, c2;                       /* Next pattern and input string chars */
    u32 matchOne = pInfo->matchOne;  /* "?" or "_" */
    u32 matchAll = pInfo->matchAll;  /* "*" or "%" */
    u8 noCase = pInfo->noCase;       /* True if uppercase==lowercase */
    const u8* zEscaped = 0;          /* One past the last escaped input char */

    while ((c = Utf8Read(zPattern)) != 0) {
        if (c == matchAll) {  /* Match "*" */
          /* Skip over multiple "*" characters in the pattern.  If there
          ** are also "?" characters, skip those as well, but consume a
          ** single character of the input string for each "?" skipped */
            while ((c = Utf8Read(zPattern)) == matchAll || c == matchOne) {
                if (c == matchOne && sqlite3Utf8Read(&zString) == 0) {
                    return SQLITE_NOWILDCARDMATCH;
                }
            }
            if (c == 0) {
                return SQLITE_MATCH;   /* "*" at the end of the pattern matches */
            }
            else if (c == matchOther) {
                if (pInfo->matchSet == 0) {
                    c = sqlite3Utf8Read(&zPattern);
                    if (c == 0) return SQLITE_NOWILDCARDMATCH;
                }
                else {
                    /* "[...]" immediately follows the "*".  We have to do a slow
                    ** recursive search in this case, but it is an unusual case. */
                    assert(matchOther < 0x80);  /* '[' is a single-byte character */
                    while (*zString) {
                        int bMatch = patternCompare(&zPattern[-1], zString, pInfo, matchOther);
                        if (bMatch != SQLITE_NOMATCH) return bMatch;
                        SQLITE_SKIP_UTF8(zString);
                    }
                    return SQLITE_NOWILDCARDMATCH;
                }
            }

            /* At this point variable c contains the first character of the
            ** pattern string past the "*".  Search in the input string for the
            ** first matching character and recursively continue the match from
            ** that point.
            **
            ** For a case-insensitive search, set variable cx to be the same as
            ** c but in the other case and search the input string for either
            ** c or cx.
            */
            if (c <= 0x80) {
                std::unordered_map<u32, const char*>& charToAccentsMap = charToAccentsInsensitive;
                char zStop[3];
                int bMatch;
                if (noCase) {
                    zStop[0] = sqlite3Toupper(c);
                    zStop[1] = sqlite3Tolower(c);
                    zStop[2] = 0;
                }
                else {
                    charToAccentsMap = charToAccentsSensitive;
                    zStop[0] = c;
                    zStop[1] = 0;
                }
                while (1) {
                    /* see if we have a mapping from input character to a list of their
                    respective accented characters; if we do, use that mapping*/
                    auto it = charToAccentsMap.find(c);
                    if (it != charToAccentsMap.end()) {
                        const char* stop = it->second;
                        zString += strcspn((const char*)zString, it->second);
                    }
                    /* otherwise we fall back to the default sqlite implementation that
                    does simple ascii matching */
                    else {
                        zString += strcspn((const char*)zString, zStop);
                    }
                    if (zString[0] == 0) break;
                    zString++;
                    bMatch = patternCompare(zPattern, zString, pInfo, matchOther);
                    if (bMatch != SQLITE_NOMATCH) return bMatch;
                }
            }
            else {
                int bMatch;
                while ((c2 = Utf8Read(zString)) != 0) {
                    if (c2 != c) continue;
                    bMatch = patternCompare(zPattern, zString, pInfo, matchOther);
                    if (bMatch != SQLITE_NOMATCH) return bMatch;
                }
            }
            return SQLITE_NOWILDCARDMATCH;
        }
        if (c == matchOther) {
            if (pInfo->matchSet == 0) {
                c = sqlite3Utf8Read(&zPattern);
                if (c == 0) return SQLITE_NOMATCH;
                zEscaped = zPattern;
            }
            else {
                u32 prior_c = 0;
                int seen = 0;
                int invert = 0;
                c = sqlite3Utf8Read(&zString);
                if (c == 0) return SQLITE_NOMATCH;
                c2 = sqlite3Utf8Read(&zPattern);
                if (c2 == '^') {
                    invert = 1;
                    c2 = sqlite3Utf8Read(&zPattern);
                }
                if (c2 == ']') {
                    if (c == ']') seen = 1;
                    c2 = sqlite3Utf8Read(&zPattern);
                }
                while (c2 && c2 != ']') {
                    if (c2 == '-' && zPattern[0] != ']' && zPattern[0] != 0 && prior_c > 0) {
                        c2 = sqlite3Utf8Read(&zPattern);
                        if (c >= prior_c && c <= c2) seen = 1;
                        prior_c = 0;
                    }
                    else {
                        if (c == c2) {
                            seen = 1;
                        }
                        prior_c = c2;
                    }
                    c2 = sqlite3Utf8Read(&zPattern);
                }
                if (c2 == 0 || (seen ^ invert) == 0) {
                    return SQLITE_NOMATCH;
                }
                continue;
            }
        }
        c2 = Utf8Read(zString);
        if (c == c2) continue;
        if (noCase && sqlite3Tolower(c) == sqlite3Tolower(c2) && c < 0x80 && c2 < 0x80) {
            continue;
        }
        if (c == matchOne && zPattern != zEscaped && c2 != 0) continue;
        return SQLITE_NOMATCH;
    }
    return *zString == 0 ? SQLITE_MATCH : SQLITE_NOMATCH;
}

/*
** Implementation of the like() SQL function.  This function implements
** the built-in LIKE operator.  The first argument to the function is the
** pattern and the second argument is the string.  So, the SQL statements:
**
**       A LIKE B
**
** is implemented as like(B,A).
**
** This same function (with a different compareInfo structure) computes
** the GLOB operator.
*/
static void likeFunc(
    sqlite3_context* context,
    int argc,
    sqlite3_value** argv
) {
    const unsigned char* zA, * zB;
    u32 escape;
    sqlite3* db = sqlite3_context_db_handle(context);
    struct compareInfo* pInfo = (compareInfo*) sqlite3_user_data(context);
    struct compareInfo backupInfo;

#ifdef SQLITE_LIKE_DOESNT_MATCH_BLOBS
    if (sqlite3_value_type(argv[0]) == SQLITE_BLOB
        || sqlite3_value_type(argv[1]) == SQLITE_BLOB
        ) {
        sqlite3_result_int(context, 0);
        return;
    }
#endif

    /* Limit the length of the LIKE or GLOB pattern to avoid problems
    ** of deep recursion and N*N behavior in patternCompare().
    */

    if (sqlite3_value_bytes(argv[0]) > SQLITE_MAX_LIKE_PATTERN_LENGTH) {
        sqlite3_result_error(context, "LIKE or GLOB pattern too complex", -1);
        return;
    }

    if (argc == 3) {
        /* The escape character string must consist of a single UTF-8 character.
        ** Otherwise, return an error.
        */
        const unsigned char* zEsc = sqlite3_value_text(argv[2]);
        if (zEsc == 0) return;
        if (sqlite3Utf8CharLen((char*)zEsc, -1) != 1) {
            sqlite3_result_error(context,
                "ESCAPE expression must be a single character", -1);
            return;
        }
        escape = sqlite3Utf8Read(&zEsc);
        if (escape == pInfo->matchAll || escape == pInfo->matchOne) {
            memcpy(&backupInfo, pInfo, sizeof(backupInfo));
            pInfo = &backupInfo;
            if (escape == pInfo->matchAll) pInfo->matchAll = 0;
            if (escape == pInfo->matchOne) pInfo->matchOne = 0;
        }
    }
    else {
        escape = pInfo->matchSet;
    }
    zB = sqlite3_value_text(argv[0]);
    zA = sqlite3_value_text(argv[1]);
    if (zA && zB) {
        sqlite3_result_int(context,
            patternCompare(zB, zA, pInfo, escape) == SQLITE_MATCH);
    }
}

/*
** Function to delete compiled regexp objects. Registered as
** a destructor function with sqlite3_set_auxdata().
*/
static void regexpDelete(void* p) {
    std::regex* regex = (std::regex*) p;
    delete regex;
}

/*
** Implementation of SQLite REGEXP operator. This scalar function takes
** two arguments. The first is a regular expression pattern to compile
** the second is a string to match against that pattern. If either
** argument is an SQL NULL, then NULL Is returned. Otherwise, the result
** is 1 if the string matches the pattern, or 0 otherwise.
**
** SQLite maps the regexp() function to the regexp() operator such
** that the following two are equivalent:
**
**     zString REGEXP zPattern
**     regexp(zPattern, zString)
**
** Uses standard C++ std::regex
*/
static void regexpFunc(sqlite3_context* context, int nArg, sqlite3_value** apArg) {
    static auto kRegexFlags =
        std::regex::icase |
        std::regex::optimize |
        std::regex::collate |
        std::regex::ECMAScript;

    static auto kMatchFlags =
        std::regex_constants::match_any;

    const char* matchAgainst = (const char*) sqlite3_value_text(apArg[1]);

    if (!matchAgainst) {
        return;
    }

    std::regex* regex = (std::regex*) sqlite3_get_auxdata(context, 0);
    if (!regex) {
        const char* pattern = (const char*) sqlite3_value_text(apArg[0]);
        if (!pattern) {
            return;
        }

        try {
            regex = new std::regex(pattern, kRegexFlags);
        }
        catch (std::regex_error) {
            return;
        }

        sqlite3_set_auxdata(context, 0, regex, regexpDelete);
    }

    /* Return 1 or 0. */
    sqlite3_result_int(context, std::regex_search(matchAgainst, *regex, kMatchFlags) ? 1 : 0);
}

namespace musik { namespace core { namespace db {

    namespace SqliteExtensions {

        int Register(sqlite3* db) {
            static const struct Scalar {
                const char* zName; /* Function name */
                unsigned char nArg; /* Number of arguments */
                unsigned int enc; /* Optimal text encoding */
                void (*xFunc)(sqlite3_context*, int, sqlite3_value**);
            } scalars[] = {
                {"regexp", 2, SQLITE_ANY | SQLITE_DETERMINISTIC| SQLITE_INNOCUOUS, regexpFunc},
                {"like", 2, SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS, likeFunc},
                {"like", 3, SQLITE_UTF8 | SQLITE_DETERMINISTIC | SQLITE_INNOCUOUS, likeFunc},
            };
            int rc = SQLITE_OK;
            for (int i = 0; rc == SQLITE_OK && i < (int)(sizeof(scalars) / sizeof(scalars[0])); i++) {
                const struct Scalar* p = &scalars[i];
                rc = sqlite3_create_function(
                    db,
                    p->zName,
                    p->nArg,
                    p->enc,
                    (void*)&likeInfoNorm,
                    p->xFunc,
                    0,
                    0);
            }
            return rc;
        }

    }

} } }

