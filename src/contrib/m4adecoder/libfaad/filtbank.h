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
** $Id: filtbank.h,v 1.27 2007/11/01 12:33:30 menno Exp $
**/

#ifndef __FILTBANK_H__
#define __FILTBANK_H__

#ifdef __cplusplus
extern "C" {
#endif


fb_info *filter_bank_init(uint16_t frame_len);
void filter_bank_end(fb_info *fb);

#ifdef LTP_DEC
void filter_bank_ltp(fb_info *fb,
                     uint8_t window_sequence,
                     uint8_t window_shape,
                     uint8_t window_shape_prev,
                     real_t *in_data,
                     real_t *out_mdct,
                     uint8_t object_type,
                     uint16_t frame_len);
#endif

void ifilter_bank(fb_info *fb, uint8_t window_sequence, uint8_t window_shape,
                  uint8_t window_shape_prev, real_t *freq_in,
                  real_t *time_out, real_t *overlap,
                  uint8_t object_type, uint16_t frame_len);

#ifdef __cplusplus
}
#endif
#endif
