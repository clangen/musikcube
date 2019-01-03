#include "nestypes.h"
#include "s_logtbl.h"

#if STATIC_TABLES

static void LogTableRelease(void *ctx)
{
}

static KMIF_LOGTABLE log_static_tables = {
	&log_static_tables;
	LogTableRelease,
#include "s_logt.h"
};


KMIF_LOGTABLE *LogTableAddRef(void)
{
	log_static_tables.release = LogTableRelease;
	return &log_static_tables;
}

#else

#include <math.h>

static volatile Uint32 log_tables_mutex = 0;
static Uint32 log_tables_refcount = 0;
static KMIF_LOGTABLE *log_tables = 0;

static void LogTableRelease(void *ctx)
{
	++log_tables_mutex;
	while (log_tables_mutex != 1)
	{
		XSLEEP(0);
	}
	log_tables_refcount--;
	if (!log_tables_refcount)
	{
		XFREE(ctx);
		log_tables = 0;
	}
	--log_tables_mutex;
}

static void LogTableCalc(KMIF_LOGTABLE *kmif_lt)
{
	Uint32 i;
	double a;
	for (i = 0; i < (1 << LOG_BITS); i++)
	{
		a = (1 << LOG_LIN_BITS) / pow(2, i / (double)(1 << LOG_BITS));
		kmif_lt->logtbl[i] = (Uint32)a;
	}
	kmif_lt->lineartbl[0] = LOG_LIN_BITS << LOG_BITS;
	for (i = 1; i < (1 << LIN_BITS) + 1; i++)
	{
		Uint32 ua;
		a = i << (LOG_LIN_BITS - LIN_BITS);
		ua = (Uint32)((LOG_LIN_BITS - (log(a) / log(2))) * (1 << LOG_BITS));
		kmif_lt->lineartbl[i] = ua << 1;
	}
}

KMIF_LOGTABLE *LogTableAddRef(void)
{
	++log_tables_mutex;
	while (log_tables_mutex != 1)
	{
		XSLEEP(0);
	}
	if (!log_tables_refcount)
	{
		log_tables = (KMIF_LOGTABLE*) XMALLOC(sizeof(KMIF_LOGTABLE));
		if (log_tables)
		{
			log_tables->ctx = log_tables;
			log_tables->release = LogTableRelease;
			LogTableCalc(log_tables);
		}
	}
	if (log_tables) log_tables_refcount++;
	--log_tables_mutex;
	return log_tables;
}

#endif
