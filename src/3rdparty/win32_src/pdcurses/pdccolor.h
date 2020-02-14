#ifndef PDCCOLOR_H

#define PDCCOLOR_H
extern int PDC_blink_state;

typedef uint32_t PACKED_RGB;

int PDC_init_palette( void);
void PDC_get_rgb_values( const chtype srcp,
            PACKED_RGB *foreground_rgb, PACKED_RGB *background_rgb);
int PDC_set_palette_entry( const int idx, const PACKED_RGB rgb);
PACKED_RGB PDC_get_palette_entry( const int idx);
void PDC_free_palette( void);
#endif
