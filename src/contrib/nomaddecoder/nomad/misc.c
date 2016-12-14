/*
 * Copyright 2008-2013 Various Authors
 * Copyright 2004-2005 Timo Hirvonen
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

#include "misc.h"

int replaygain_decode(unsigned int field, int *gain)
{
	unsigned int name_code, originator_code, sign_bit, val;

	name_code = (field >> 13) & 0x7;
	if (!name_code || name_code > 2)
		return 0;
	originator_code = (field >> 10) & 0x7;
	if (!originator_code)
		return 0;
	sign_bit = (field >> 9) & 0x1;
	val = field & 0x1ff;
	if (sign_bit && !val)
		return 0;
	*gain = (sign_bit ? -1 : 1) * val;
	return name_code;
}