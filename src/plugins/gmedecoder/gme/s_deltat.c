#include "kmsnddev.h"
#include "divfix.h"
#include "s_logtbl.h"
#include "s_deltat.h"
#include <string.h>

#define CPS_SHIFT 16
#define PHASE_SHIFT 16 /* 16(fix) */

typedef struct {
	KMIF_SOUND_DEVICE kmif;
	KMIF_LOGTABLE *logtbl;
	struct YMDELTATPCMSOUND_COMMON_TAG {
		Int32 mastervolume;
		Int32 step;
		Int32 output;
		Uint32 cnt;
		Uint32 cps;
		Uint32 phase;
		Uint32 deltan;
		Uint32 scale;
		Uint32 mem;
		Uint32 play;
		Uint32 start;
		Uint32 stop;
		Int32 level32;
		Uint8 key;
		Uint8 level;
		Uint8 granuality;
		Uint8 pad4_3;
		Uint8 regs[0x10];
	} common;
	Uint8 *romrambuf;
	Uint32 romrammask;
	Uint8 *rambuf;
	Uint32 rammask;
	Uint8 *rombuf;
	Uint32 rommask;
	Uint8 ymdeltatpcm_type;
	Uint8 memshift;
} YMDELTATPCMSOUND;

static Uint8 const table_step[8] =
{
	  1,   3,   5,   7,   9,  11,  13,  15,
};
static Uint8 const table_scale[16] =
{
	 57,  57,  57,  57,  77, 102, 128, 153,
	 57,  57,  57,  57,  77, 102, 128, 153,
};

__inline static void writeram(YMDELTATPCMSOUND *sndp, Uint32 v)
{
	sndp->rambuf[(sndp->common.mem >> 1) & sndp->rammask] = v;
	sndp->common.mem += 1 << 1;
}

__inline static Uint32 readram(YMDELTATPCMSOUND *sndp)
{
	Uint32 v;
	v = sndp->romrambuf[(sndp->common.play >> 1) & sndp->romrammask];
	if (sndp->common.play & 1)
		v &= 0x0F;
	else
		v >>= 4;
	sndp->common.play += 1;
	if (sndp->common.play >= sndp->common.stop)
	{
		if (sndp->common.regs[0] & 0x10)
		{
			sndp->common.play = sndp->common.start;
			sndp->common.step = 0;
			sndp->common.scale = 127;
		}
		else
		{
			sndp->common.key = 0;
		}
	}
	return v;
}

__inline static void DelrtatStep(YMDELTATPCMSOUND *sndp, Uint32 data)
{
	if (data & 8)
		sndp->common.step -= (table_step[data & 7] * sndp->common.scale) >> 3;
	else
		sndp->common.step += (table_step[data & 7] * sndp->common.scale) >> 3;
	if (sndp->common.step > ((1 << 15) - 1)) sndp->common.step = ((1 << 15) - 1);
	if (sndp->common.step < -(1 << 15)) sndp->common.step = -(1 << 15);
	sndp->common.scale = (sndp->common.scale * table_scale[data]) >> 6;
	if (sndp->common.scale > 24576) sndp->common.scale = 24576;
	if (sndp->common.scale < 127) sndp->common.scale = 127;
}

#if (((-1) >> 1) == -1)
#define SSR(x, y) (((Int32)x) >> (y))
#else
#define SSR(x, y) (((x) >= 0) ? ((x) >> (y)) : (-((-(x) - 1) >> (y)) - 1))
#endif

static int sndsynth(YMDELTATPCMSOUND *sndp )
{
	if (!sndp->common.key)
		return 0;
	{
		Uint32 step;
		sndp->common.cnt += sndp->common.cps;
		step = sndp->common.cnt >> CPS_SHIFT;
		sndp->common.cnt &= (1 << CPS_SHIFT) - 1;
		sndp->common.phase += step * sndp->common.deltan;
		step = sndp->common.phase >> PHASE_SHIFT;
		sndp->common.phase &= (1 << PHASE_SHIFT) - 1;
		if (step)
		{
			do
			{
				DelrtatStep(sndp, readram(sndp));
			} while (--step);
			sndp->common.output = sndp->common.step * sndp->common.level32;
			sndp->common.output = SSR(sndp->common.output, 8 + 2);
		}
	}
	return sndp->common.output;
}



static void sndwrite(YMDELTATPCMSOUND *sndp, Uint32 a, Uint32 v)
{
	sndp->common.regs[a] = v;
	switch (a)
	{
		/* START,REC,MEMDATA,REPEAT,SPOFF,--,--,RESET */
		case 0x00:  /* Control Register 1 */
			if ((v & 0x80) && !sndp->common.key)
			{
				sndp->common.key = 1;
				sndp->common.play = sndp->common.start;
				sndp->common.step = 0;
				sndp->common.scale = 127;
			}
			if (v & 1) sndp->common.key = 0;
			break;
		/* L,R,-,-,SAMPLE,DA/AD,RAMTYPE,ROM */
		case 0x01:  /* Control Register 2 */
			sndp->romrambuf  = (sndp->common.regs[1] & 1) ? sndp->rombuf  : sndp->rambuf;
			sndp->romrammask = (sndp->common.regs[1] & 1) ? sndp->rommask : sndp->rammask;
			break;
		case 0x02:  /* Start Address L */
		case 0x03:  /* Start Address H */
			sndp->common.granuality = (v & 2) ? 1 : 4;
			sndp->common.start = ((sndp->common.regs[3] << 8) + sndp->common.regs[2]) << (sndp->memshift + 1);
			sndp->common.mem = sndp->common.start;
			break;
		case 0x04:  /* Stop Address L */
		case 0x05:  /* Stop Address H */
			sndp->common.stop = ((sndp->common.regs[5] << 8) + sndp->common.regs[4]) << (sndp->memshift + 1);
			break;
		case 0x06:  /* Prescale L */
		case 0x07:  /* Prescale H */
			break;
		case 0x08:  /* Data */
			if ((sndp->common.regs[0] & 0x60) == 0x60) writeram(sndp, v);
			break;
		case 0x09:  /* Delta-N L */
		case 0x0A:  /* Delta-N H */
			sndp->common.deltan = (sndp->common.regs[0xA] << 8) + sndp->common.regs[0x9];
			if (sndp->common.deltan < 0x100) sndp->common.deltan = 0x100;
			break;
		case 0x0B:  /* Level Control */
			sndp->common.level = v;
			sndp->common.level32 = ((Int32)(sndp->common.level * LogToLin(sndp->logtbl, sndp->common.mastervolume, LOG_LIN_BITS - 15))) >> 7;
			sndp->common.output = sndp->common.step * sndp->common.level32;
			sndp->common.output = SSR(sndp->common.output, 8 + 2);
			break;
	}
}

static Uint32 sndread(YMDELTATPCMSOUND *sndp, Uint32 a)
{
	return 0;
}

static void sndreset(YMDELTATPCMSOUND *sndp, Uint32 clock, Uint32 freq)
{
	XMEMSET(&sndp->common, 0, sizeof(sndp->common));
	sndp->common.cps = DivFix(clock, 72 * freq, CPS_SHIFT);
	sndp->romrambuf  = (sndp->common.regs[1] & 1) ? sndp->rombuf  : sndp->rambuf;
	sndp->romrammask = (sndp->common.regs[1] & 1) ? sndp->rommask : sndp->rammask;
	sndp->common.granuality = 4;
}

static void sndvolume(YMDELTATPCMSOUND *sndp, Int32 volume)
{
	volume = (volume << (LOG_BITS - 8)) << 1;
	sndp->common.mastervolume = volume;
	sndp->common.level32 = ((Int32)(sndp->common.level * LogToLin(sndp->logtbl, sndp->common.mastervolume, LOG_LIN_BITS - 15))) >> 7;
	sndp->common.output = sndp->common.step * sndp->common.level32;
	sndp->common.output = SSR(sndp->common.output, 8 + 2);
}

static void sndrelease(YMDELTATPCMSOUND *sndp)
{
	if (sndp->logtbl) sndp->logtbl->release(sndp->logtbl->ctx);
	XFREE(sndp);
}

static void setinst(YMDELTATPCMSOUND *sndp, Uint32 n, void *p, Uint32 l)
{
	if (n) return;
	if (p)
	{
		sndp->rombuf  = (Uint8*) p;
		sndp->rommask = l - 1;
		sndp->romrambuf  = (sndp->common.regs[1] & 1) ? sndp->rombuf  : sndp->rambuf;
		sndp->romrammask = (sndp->common.regs[1] & 1) ? sndp->rommask : sndp->rammask;
	}
	else
	{
		sndp->rombuf  = 0;
		sndp->rommask = 0;
	}

}

KMIF_SOUND_DEVICE *YMDELTATPCMSoundAlloc(Uint32 ymdeltatpcm_type)
{
	Uint32 ram_size;
	YMDELTATPCMSOUND *sndp;
	switch (ymdeltatpcm_type)
	{
		case YMDELTATPCM_TYPE_Y8950:
			ram_size = 32 * 1024;
			break;
		case YMDELTATPCM_TYPE_YM2608:
			ram_size = 256 * 1024;
			break;
		default:
			ram_size = 0;
			break;
	}
	sndp = (YMDELTATPCMSOUND*) XMALLOC(sizeof(YMDELTATPCMSOUND) + ram_size);
	if (!sndp) return 0;
	sndp->ymdeltatpcm_type = ymdeltatpcm_type;
	switch (ymdeltatpcm_type)
	{
		case YMDELTATPCM_TYPE_Y8950:
			sndp->memshift = 2;
			break;
		case YMDELTATPCM_TYPE_YM2608:
			/* OPNA */
			sndp->memshift = 6;
			break;
		case YMDELTATPCM_TYPE_YM2610:
			sndp->memshift = 9;
			break;
	}
	sndp->kmif.ctx = sndp;
	sndp->kmif.release = (void (*)( void* )) sndrelease;
	sndp->kmif.synth = (int (*)( void* )) sndsynth;
	sndp->kmif.volume = (void (*)( void*, int )) sndvolume;
	sndp->kmif.reset = (void (*)( void*, Uint32, Uint32 )) sndreset;
	sndp->kmif.write = (void (*)( void*, Uint32, Uint32 )) sndwrite;
	sndp->kmif.read = (Uint32 (*)( void*, Uint32 )) sndread;
	sndp->kmif.setinst = (void (*)( void*, Uint32, void*, Uint32 )) setinst;
	/* RAM */
	sndp->rambuf = ram_size ? (Uint8 *)(sndp + 1) : 0;
	sndp->rammask = ram_size ? (ram_size - 1) : 0;
	/* ROM */
	sndp->rombuf = 0;
	sndp->rommask = 0;
	sndp->logtbl = LogTableAddRef();
	if (!sndp->logtbl)
	{
		sndrelease(sndp);
		return 0;
	}
	return &sndp->kmif;
}
