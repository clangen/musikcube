/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: pns.h,v 1.27 2007/11/01 12:33:33 menno Exp $
**/

#ifndef __PNS_H__
#define __PNS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "syntax.h"

#define NOISE_OFFSET 90

void pns_decode(ic_stream *ics_left, ic_stream *ics_right,
                real_t *spec_left, real_t *spec_right, uint16_t frame_len,
                uint8_t channel_pair, uint8_t object_type,
                /* RNG states */ uint32_t *__r1, uint32_t *__r2);

static INLINE uint8_t is_noise(ic_stream *ics, uint8_t group, uint8_t sfb)
{
    if (ics->sfb_cb[group][sfb] == NOISE_HCB)
        return 1;
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif
