/*
 * Copyright 2008-2013 Various Authors
 * Copyright 2005 Timo Hirvonen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "id3.h"
#include "misc.h"

#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

enum {
	ID3_ENCODING_ISO_8859_1 = 0x00,
	ID3_ENCODING_UTF_16     = 0x01,
	ID3_ENCODING_UTF_16_BE  = 0x02,
	ID3_ENCODING_UTF_8      = 0x03,
	ID3_ENCODING_MAX        = 0x03
};

/*
 * position:
 *
 *    0 "ID3"
 *  -10 "3DI"
 * -128 "TAG"
 * -138 "3DI"
 *
 * if v2 is at beginning _and_ at end then there must be a seek tag at beginning
 */

struct v2_header {
	unsigned char ver_major;
	unsigned char ver_minor;
	unsigned char flags;
	uint32_t size;
};

struct v2_extended_header {
	uint32_t size;
};

struct v2_frame_header {
	char id[4];
	uint32_t size;
	uint16_t flags;
};

#define V2_HEADER_FOOTER	(1 << 4)
#define id3_debug(...) d_print(__VA_ARGS__)

static int is_v1(const char *buf)
{
	return buf[0] == 'T' && buf[1] == 'A' && buf[2] == 'G';
}

static int u32_unsync(const unsigned char *buf, uint32_t *up)
{
    uint32_t b, u = 0;
    int i;

    for (i = 0; i < 4; i++) {
        b = buf[i];
        if (b >= 0x80)
            return 0;
        u <<= 7;
        u |= b;
    }
    *up = u;
    return 1;
}

static int v2_header_footer_parse(struct v2_header *header, const char *buf)
{
	const unsigned char *b = (const unsigned char *)buf;

	header->ver_major = b[3];
	header->ver_minor = b[4];
	header->flags = b[5];
	if (header->ver_major == 0xff || header->ver_minor == 0xff)
		return 0;
	return u32_unsync(b + 6, &header->size);
}

static int v2_header_parse(struct v2_header *header, const char *buf)
{
	if (buf[0] != 'I' || buf[1] != 'D' || buf[2] != '3')
		return 0;
	return v2_header_footer_parse(header, buf);
}

static int v2_footer_parse(struct v2_header *header, const char *buf)
{
	if (buf[0] != '3' || buf[1] != 'D' || buf[2] != 'I')
		return 0;
	return v2_header_footer_parse(header, buf);
}

int id3_tag_size(const char *buf, int buf_size)
{
	struct v2_header header;

	if (buf_size < 10)
		return 0;
	if (v2_header_parse(&header, buf)) {
		if (header.flags & V2_HEADER_FOOTER) {
			/* header + data + footer */
			id3_debug("v2.%d.%d with footer\n", header.ver_major, header.ver_minor);
			return 10 + header.size + 10;
		}
		/* header */
		id3_debug("v2.%d.%d\n", header.ver_major, header.ver_minor);
		return 10 + header.size;
	}
	if (buf_size >= 3 && is_v1(buf)) {
		id3_debug("v1\n");
		return 128;
	}
	return 0;
}
