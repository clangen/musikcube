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
** $Id: rvlc.h,v 1.17 2007/11/01 12:33:34 menno Exp $
**/

#ifndef __RVLC_SCF_H__
#define __RVLC_SCF_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int8_t index;
    uint8_t len;
    uint32_t cw;
} rvlc_huff_table;


#define ESC_VAL 7


uint8_t rvlc_scale_factor_data(ic_stream *ics, bitfile *ld);
uint8_t rvlc_decode_scale_factors(ic_stream *ics, bitfile *ld);


#ifdef __cplusplus
}
#endif
#endif
