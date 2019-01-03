/////////////////////////////////////////////////////////////////////////////
//
// qmix - QSound mixer
//
/////////////////////////////////////////////////////////////////////////////

#ifndef __Q_QMIX_H__
#define __Q_QMIX_H__

#include "emuconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

sint32 EMU_CALL _qmix_init(void);
uint32 EMU_CALL _qmix_get_state_size(void);
void   EMU_CALL _qmix_clear_state(void *state);

void   EMU_CALL _qmix_set_sample_rate(void *state, uint32 rate);
void   EMU_CALL _qmix_set_sample_rom(void *state, void *rom, uint32 size);
void   EMU_CALL _qmix_command(void *state, uint8 cmd, uint16 data);
void   EMU_CALL _qmix_render(void *state, sint16 *buf, uint32 samples);

#ifdef __cplusplus
}
#endif

#endif
