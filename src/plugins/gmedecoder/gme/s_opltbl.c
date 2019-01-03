#include "nestypes.h"
#include "s_logtbl.h"
#include "s_opltbl.h"

#if STATIC_TABLES

static void OplTableRelease(void *ctx)
{
}

static KMIF_OPLTABLE opl_static_tables = {
	&opl_static_tables;
	OplTableRelease,
#include "s_oplt.h"
};

KMIF_OPLTABLE *OplTableAddRef(void)
{
	opl_static_tables.release = OplTableRelease;
	return &opl_static_tables;
}

#else

#include <math.h>
#ifndef M_PI
#ifdef PI
#define M_PI PI
#else
#define M_PI            3.14159265358979323846
#endif
#endif

#define AM_DEPTH1 4.8  /* dB */
#define AM_DEPTH2 1.0  /* dB */
#define PM_DEPTH1 14.0 /* cent */
#define PM_DEPTH2  7.0 /* cent */
#define LOG_KEYOFF (15 << (LOG_BITS + 1))

#define DB0375_TO_LOG(x) ((Uint32)(0.375 * (1 << (LOG_BITS + x)) / 10))

#define AR_OFF (128 << ARTBL_SHIFT)
#define AR_MAX (127 << ARTBL_SHIFT)


static volatile Uint32 opl_tables_mutex = 0;
static Uint32 opl_tables_refcount = 0;
static KMIF_OPLTABLE *opl_tables = 0;

static void OplTableRelease(void *ctx)
{
	++opl_tables_mutex;
	while (opl_tables_mutex != 1)
	{
		XSLEEP(0);
	}
	opl_tables_refcount--;
	if (!opl_tables_refcount)
	{
		XFREE(ctx);
		opl_tables = 0;
	}
	--opl_tables_mutex;
}

static void OplTableCalc(KMIF_OPLTABLE *tbl)
{
	Uint32 u, u2, i;
	tbl->sin_table[0][0] = tbl->sin_table[0][1 << (SINTBL_BITS - 1)] = LOG_KEYOFF;
	for (i = 1 ;i < (1 << (SINTBL_BITS - 1)); i++)
	{
		double d;
		d = (1 << LOG_BITS) * -(log(sin(2.0 * M_PI * ((double)i) / (1 << SINTBL_BITS))) / log(2));
		if (d > (LOG_KEYOFF >> 1)) d = (LOG_KEYOFF >> 1);
		tbl->sin_table[0][i] = ((Uint32)d) << 1;
		tbl->sin_table[0][i + (1 << (SINTBL_BITS - 1))] = (((Uint32)d) << 1) + 1;
	}
	for (i = 0 ;i < (1 << SINTBL_BITS); i++)
	{
		tbl->sin_table[1][i] = (tbl->sin_table[0][i] & 1) ? tbl->sin_table[0][0] : tbl->sin_table[0][i];
		tbl->sin_table[2][i] = tbl->sin_table[0][i] & ~1;
		tbl->sin_table[3][i] =  (i & (1 << (SINTBL_BITS - 2))) ? LOG_KEYOFF : tbl->sin_table[2][i];
	}
	for (i = 0; i < (1 << TLLTBL_BITS); i++)
	{
		tbl->tll2log_table[i] = (i * DB0375_TO_LOG(0)) << 1;
	}
	for (i = 0; i < (1 << AMTBL_BITS); i++)
	{
		u  = (Uint32)((1 + sin(2 * M_PI * ((double)i) / (1 << AMTBL_BITS))) * ((1 << LOG_BITS) * AM_DEPTH1 / 20.0));
		u2 = (Uint32)((1 + sin(2 * M_PI * ((double)i) / (1 << AMTBL_BITS))) * ((1 << LOG_BITS) * AM_DEPTH2 / 20.0));
		tbl->am_table1[i] = u << 1;
		tbl->am_table2[i] = u2 << 1;
	}
	for (i = 0; i < (1 << PMTBL_BITS); i++)
	{
		u  = (Uint32)((1 << PM_SHIFT) * pow(2, sin(2 * M_PI * ((double)i) / (1 << PMTBL_BITS)) * PM_DEPTH1 / 1200.0));
		u2 = (Uint32)((1 << PM_SHIFT) * pow(2, sin(2 * M_PI * ((double)i) / (1 << PMTBL_BITS)) * PM_DEPTH2 / 1200.0));
		tbl->pm_table1[i] = u;
		tbl->pm_table2[i] = u2;
	}

	for (i = 0; i < (1 << ARTBL_BITS); i++)
	{
		u = (Uint32)(((double)AR_MAX) * (1 - log(1 + i) / log(1 << ARTBL_BITS)));
		tbl->ar_tablelog[i] = u;
#if 1
		u = (Uint32)(((double)AR_MAX) * (pow(1 - i / (double)(1 << ARTBL_BITS), 8)));
		tbl->ar_tablepow[i] = u;
#endif
	}
}

KMIF_OPLTABLE *OplTableAddRef(void)
{
	++opl_tables_mutex;
	while (opl_tables_mutex != 1)
	{
		XSLEEP(0);
	}
	if (!opl_tables_refcount)
	{
		opl_tables = (KMIF_OPLTABLE*) XMALLOC(sizeof(KMIF_OPLTABLE));
		if (opl_tables)
		{
			opl_tables->ctx = opl_tables;
			opl_tables->release = OplTableRelease;
			OplTableCalc(opl_tables);
		}
	}
	if (opl_tables) opl_tables_refcount++;
	--opl_tables_mutex;
	return opl_tables;
}

#endif
