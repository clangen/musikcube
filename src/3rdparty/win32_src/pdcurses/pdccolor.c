/* PDCurses */

/* Palette management code used by VT,  WinGUI,  SDL1/2,  and X11 for
'full color' (24-bit).  It may eventually be used by DOSVGA,  WinCon,
and/or the Plan9 platform,  all of which have full color capability.
It will presumably never be useful for the DOS or OS/2 platforms.
See 'pdccolor.txt' for a rationale of how this works. */

#ifdef NO_STDINT_H
   #define uint64_t unsigned long long
   #define uint32_t unsigned long
   #define uint16_t unsigned short
#else
   #include <stdint.h>
#endif
   #include <stdlib.h>
   #include <assert.h>

#define PACKED_RGB uint32_t

#ifndef PACK_RGB
   #define PACK_RGB( red, green, blue) ((red) | ((green)<<8) | ((PACKED_RGB)(blue) << 16))
#endif

#include <curspriv.h>
#include "pdccolor.h"

int PDC_blink_state = 0;

static PACKED_RGB *rgbs;   /* the 'standard' 256-color palette,  plus any allocated */
static int palette_size;

PACKED_RGB PDC_default_color( int idx)
{
    PACKED_RGB rval;

    assert( idx >= 0);
    if( idx < 16)
    {
        if( idx == 8)
            rval = PACK_RGB( 0x80, 0x80, 0x80);
        else
        {
            const int intensity = ((idx & 8) ? 0xff : 0xc0);

            rval = PACK_RGB( ((idx & COLOR_RED) ? intensity : 0),
                             ((idx & COLOR_GREEN) ? intensity : 0),
                             ((idx & COLOR_BLUE) ? intensity : 0));
        }
    }
    else if( idx < 216 + 16)
    {                    /* colors 16-231 are a 6x6x6 color cube */
        int r, g, b;

        idx -= 16;
        r = idx / 36;
        g = (idx / 6) % 6;
        b = idx % 6;
        rval = PACK_RGB( r ? r * 40 + 55 : 0,
                         g ? g * 40 + 55 : 0,
                         b ? b * 40 + 55 : 0);
    }
    else if( idx < 256)    /* colors 232-255 are 24 shades of gray */
    {
        const int intensity = (idx - 232) * 10 + 8;

        rval = PACK_RGB( intensity, intensity, intensity);
    }
    else             /* colors 256 to 256+2^24 are RGB values */
        rval = idx - 256;
    return( rval);
}

int PDC_init_palette( void)
{
    return( 0);
}

void PDC_free_palette( void)
{
   if( rgbs)
      free( rgbs);
   rgbs = NULL;
}

PACKED_RGB PDC_get_palette_entry( const int idx)
{
   PACKED_RGB rval;

   if( idx < palette_size)
   {
      assert( idx >= 0);
      rval = rgbs[idx];
   }
   else
      rval = PDC_default_color( idx);
   return( rval);
}

/* Return value is -1 if no palette could be allocated,  0 if the color
didn't change (new RGB matched the old one),  and 1 if the color changed. */

int PDC_set_palette_entry( const int idx, const PACKED_RGB rgb)
{
   int rval, i;

   if( idx >= palette_size)
      {
      int new_size = palette_size;
      const int initial_palette_size = 8;

      if( !new_size)
         new_size = initial_palette_size;
      while( new_size <= idx)
         new_size *= 2;
      rgbs = realloc( rgbs, new_size * sizeof( PACKED_RGB));
      assert( rgbs);
      if( !rgbs)
         return( -1);
      for( i = palette_size; i < new_size; i++)
         rgbs[i] = PDC_default_color( i);
      palette_size = new_size;
      }
   rval = (rgbs[idx] == rgb ? 1 : 0);
   rgbs[idx] = rgb;
   return( rval);
}

    /* This function 'intensifies' a color by shifting it toward white. */
    /* It used to average the input color with white.  Then it did a    */
    /* weighted average:  2/3 of the input color,  1/3 white,   for a   */
    /* lower "intensification" level.                                   */
    /*    Then Mark Hessling suggested that the output level should     */
    /* remap zero to 85 (= 255 / 3, so one-third intensity),  and input */
    /* of 192 or greater should be remapped to 255 (full intensity).    */
    /* Assuming we want a linear response between zero and 192,  that   */
    /* leads to output = 85 + input * (255-85)/192.                     */
    /*    This should lead to proper handling of bold text in legacy    */
    /* apps,  where "bold" means "high intensity".                      */

static PACKED_RGB intensified_color( PACKED_RGB ival)
{
    int rgb, i;
    PACKED_RGB oval = 0;

    for( i = 0; i < 3; i++, ival >>= 8)
    {
        rgb = (int)( ival & 0xff);
        if( rgb >= 192)
            rgb = 255;
        else
            rgb = 85 + rgb * (255 - 85) / 192;
        oval |= ((PACKED_RGB)rgb << (i * 8));
    }
    return( oval);
}

   /* For use in adjusting colors for A_DIMmed characters.  Just */
   /* knocks down the intensity of R, G, and B by 1/3.           */

static PACKED_RGB dimmed_color( PACKED_RGB ival)
{
    unsigned i;
    PACKED_RGB oval = 0;

    for( i = 0; i < 3; i++, ival >>= 8)
    {
        unsigned rgb = (unsigned)( ival & 0xff);

        rgb -= (rgb / 3);
        oval |= ((PACKED_RGB)rgb << (i * 8));
    }
    return( oval);
}


void PDC_get_rgb_values( const chtype srcp,
            PACKED_RGB *foreground_rgb, PACKED_RGB *background_rgb)
{
    const int color = (int)(( srcp & A_COLOR) >> PDC_COLOR_SHIFT);
    bool reverse_colors = ((srcp & A_REVERSE) ? TRUE : FALSE);
    bool intensify_backgnd = FALSE;
    bool default_foreground = FALSE, default_background = FALSE;
    int foreground_index, background_index;

    extended_pair_content( color, &foreground_index, &background_index);
    if( foreground_index < 0 && SP->orig_attr)
        default_foreground = TRUE;
    else
        *foreground_rgb = PDC_get_palette_entry( foreground_index);
    if( background_index < 0 && SP->orig_attr)
        default_background = TRUE;
    else
        *background_rgb = PDC_get_palette_entry( background_index);

    if( srcp & A_BLINK)
    {
        if( !(SP->termattrs & A_BLINK))   /* convert 'blinking' to 'bold' */
            intensify_backgnd = TRUE;
        else if( PDC_blink_state)
            reverse_colors ^= 1;
    }
    if( default_foreground)
        *foreground_rgb = (PACKED_RGB)-1;
    else if( srcp & A_BOLD & ~SP->termattrs)
        *foreground_rgb = intensified_color( *foreground_rgb);

    if( default_background)
        *background_rgb = (PACKED_RGB)-1;
    else if( intensify_backgnd)
        *background_rgb = intensified_color( *background_rgb);
    if( srcp & A_DIM)
    {
        if( !default_foreground)
           *foreground_rgb = dimmed_color( *foreground_rgb);
        if( !default_background)
           *background_rgb = dimmed_color( *background_rgb);
    }
    if( reverse_colors)
    {
        const PACKED_RGB temp = *foreground_rgb;

        *foreground_rgb = *background_rgb;
        *background_rgb = temp;
    }
}
