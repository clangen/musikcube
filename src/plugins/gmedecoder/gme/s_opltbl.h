#ifndef S_OPLTBL_H__
#define S_OPLTBL_H__

#ifdef __cplusplus
extern "C" {
#endif

#define SINTBL_BITS 11
#define AMTBL_BITS 8
#define PMTBL_BITS 8
#define PM_SHIFT 9
#define ARTBL_BITS 7
#define ARTBL_SHIFT 20
#define TLLTBL_BITS 7

typedef struct
{
	void *ctx;
	void (*release)(void *ctx);
	Uint32 sin_table[4][1 << SINTBL_BITS];
	Uint32 tll2log_table[1 << TLLTBL_BITS];
	Uint32 ar_tablelog[1 << ARTBL_BITS];
	Uint32 am_table1[1 << AMTBL_BITS];
	Uint32 pm_table1[1 << PMTBL_BITS];
#if 1
	Uint32 ar_tablepow[1 << ARTBL_BITS];
#endif
	Uint32 am_table2[1 << AMTBL_BITS];
	Uint32 pm_table2[1 << PMTBL_BITS];
} KMIF_OPLTABLE;

KMIF_OPLTABLE *OplTableAddRef(void);

#ifdef __cplusplus
}
#endif

#endif /* S_OPLTBL_H__ */
