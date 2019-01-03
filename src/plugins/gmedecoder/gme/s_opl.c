/*
  s_opl.c -- YM2413/Y8950/YM3526/YM3812 emulator by Mamiya, 2001.

  References: 
    fmopl.c        -- 1999,2000 written by Tatsuyuki Satoh (MAME development).
    fmopl.c(fixed) -- 2000 modified by mamiya (NEZplug development).
    fmgen.cpp      -- 1999-2001 written by cisc.
    emu2413.c      -- a YM2413 emulator : written by Mitsutaka Okazaki 2001
    fmpac.ill      -- 2000 created by sama. 
    fmpac.ill      -- 2000 created by NARUTO. 
    fmpac.ill      -- 2001 created by Okazaki. 
    YM2413 application manual
*/

#include "kmsnddev.h"
#include "divfix.h"
#include "s_logtbl.h"
#include "s_opltbl.h"
#include "s_opl.h"
#include "s_deltat.h"
#include <string.h>

#define PG_SHIFT 10 /* fix */
#define CPS_SHIFTE 20
#define CPS_SHIFTP 14
#define LFO_SHIFT 16

#define OPLL_INST_WORK  0x40
#define OPLL_INST_WORK2 (OPLL_INST_WORK + 8 * 0x13)

#define AR_BITS   6 /* fix */
#define AR_SHIFT 14 /* fix */
#define EG_SHIFT 15 /* fix */

#define AR_PHASEMAX (((1 << AR_BITS) - 1) << AR_SHIFT)
#define EG_PHASEMAX (127 << EG_SHIFT)
#define EG_KEYOFF   (128 << EG_SHIFT)
#define LOG_KEYOFF   (31 << (LOG_BITS + 1))

#if 0
  0-48dB
  AR_BITS=6
  AR_SHIFT=14
  x = FC000h(20.7618(sec) * 3579545 / 72)cycles 63 * 4000h
  x / 3 * 4 1730.15(ms)
  x / 3 * 256 27.03(ms)

  EG_BITS=7
  EG_SHIFT=15
  x = 3F8000h(83.7064(sec) * 3579545 / 72)cycles 127 * 8000h
  x / 4 = 20926.6(ms)
#endif


#define TESTING_OPTIMIZE_AME 1
#define USE_FBBUF 1

typedef struct
{
	Uint32 phase;
	Uint32 spd;
	Uint32 rng;
} OPL_PG;

enum {
	EG_MODE_ATTACK,
	EG_MODE_DECAY,
	EG_MODE_SUSTINE,
	EG_MODE_RELEASE,
	EG_MODE_NUM,
	EG_MODE_SUSHOLD,
	EG_MODE_OFF = EG_MODE_NUM
};

typedef struct
{
	Uint32 phasear;
	Uint32 phase;
	Uint32 spd [EG_MODE_NUM];
	Uint32 dr_phasemax;
	Uint8 mode;
	Uint8 pad4_1;
	Uint8 pad4_2;
	Uint8 pad4_3;
} OPL_EG;

enum
{
	FLAG_AME        = (1 << 0),
	FLAG_PME        = (1 << 1),
	FLAG_EGT        = (1 << 2),
	FLAG_KSR        = (1 << 3)
};

typedef struct
{
	OPL_PG pg;
	OPL_EG eg;
	Int32 input;
#if USE_FBBUF
	Int32 fbbuf;
#endif
#if TESTING_OPTIMIZE_AME
	Uint32* amin;
#endif
	Uint32 tl_ofs;
	Uint32* sintable;

	Uint8 modcar;   /* 1:m 0:c */
	Uint8 fb;
	Uint8 lvl;
	Uint8 nst;

	Uint8 tll;
	Uint8 key;
	Uint8 rkey;
	Uint8 prevkey;

	Uint8 enable;
	Uint8 __enablehold__;
	Uint8 flag;
	Uint8 ksr;

	Uint8 mul;
	Uint8 ksl;
	Uint8 ar;
	Uint8 dr;
	Uint8 sl;
	Uint8 rr;
	Uint8 tl;
	Uint8 wf;
} OPL_OP;

typedef struct
{
	OPL_OP op [2];
	Uint8 con;
	Uint8 freql;
	Uint8 freqh;
	Uint8 blk;
	Uint8 kcode;
	Uint8 sus;
	Uint8 ksb;
	Uint8 pad4_3;
} OPL_CH;

enum
{
	LFO_UNIT_AM,
	LFO_UNIT_PM,
	LFO_UNIT_NUM
};

typedef struct
{
	Uint32 output;
	Uint32 cnt;
	Uint32 sps; /* step per sample */
	Uint32 adr;
	Uint32 adrmask;
	Uint32* table;
} OPL_LFO;

typedef struct
{
	KMIF_SOUND_DEVICE kmif;
	KMIF_SOUND_DEVICE* deltatpcm;
	KMIF_LOGTABLE* logtbl;
	KMIF_OPLTABLE* opltbl;
	OPL_CH ch [9];
	OPL_LFO lfo [LFO_UNIT_NUM];
	struct OPLSOUND_COMMON_TAG
	{
		Uint32 cpsp;
		Uint32 cnt;
		Uint32* ar_table;
		Uint32* tll2logtbl;
#if TESTING_OPTIMIZE_AME
		Uint32 amzero;
#endif
		Int32 mastervolume;
		Uint32 sintablemask;
		Uint32 ratetbla [4];
		Uint32 ratetbl [4];
		Uint8 adr;
		Uint8 wfe;
		Uint8 rc;
		Uint8 rmode;
		Uint8 enable;
	} common;
	Uint8 regs [0x100];
	Uint8 opl_type;
} OPLSOUND;

static Uint8 romtone [3] [16 * 19] =
{
	{
#include "i_fmpac.h"
	},
	{
#include "i_fmunit.h"
	},
	{
#include "i_vrc7.h"
	},
};

static void SetOpOff(OPL_OP* opp )
{
	opp->eg.mode = EG_MODE_OFF;
	opp->eg.phase = EG_KEYOFF;
	opp->enable = 0;
}

inline static void EgStep( OPLSOUND* sndp, OPL_OP* opp )
{
	switch ( opp->eg.mode )
	{
	default:
		NEVER_REACH
	
	case EG_MODE_ATTACK:
		opp->eg.phase = sndp->common.ar_table [opp->eg.phasear >> (AR_SHIFT + AR_BITS - ARTBL_BITS)] >> (ARTBL_SHIFT -  EG_SHIFT);
		opp->eg.phasear += opp->eg.spd [EG_MODE_ATTACK];
		if ( opp->eg.phasear >= AR_PHASEMAX )
		{
			opp->eg.mode = EG_MODE_DECAY;
			opp->eg.phase = 0;
		}
		break;
	
	case EG_MODE_DECAY:
		opp->eg.phase += opp->eg.spd [EG_MODE_DECAY];
		if ( opp->eg.phase >= opp->eg.dr_phasemax )
		{
			opp->eg.phase = opp->eg.dr_phasemax;
			opp->eg.mode = (opp->flag & FLAG_EGT) ? EG_MODE_SUSHOLD : EG_MODE_SUSTINE;
		}
		break;
	
	case EG_MODE_SUSTINE:
	case EG_MODE_RELEASE:
		opp->eg.phase += opp->eg.spd [opp->eg.mode];
		if ( opp->eg.phase >= EG_PHASEMAX )
			SetOpOff(opp);
		break;
	
	case EG_MODE_SUSHOLD:
	case EG_MODE_OFF:
		break;
	}
}

static void OpStep( OPLSOUND* sndp, OPL_OP* opp )
{
	int step;
	EgStep(sndp, opp);
	step = opp->pg.spd;
	if ( opp->flag & FLAG_PME )
		step = (step * sndp->lfo [LFO_UNIT_PM].output) >> PM_SHIFT;
	opp->pg.phase += step;
}

__inline static void OpStepNG( OPLSOUND* sndp, OPL_OP* opp )
{
	Uint32 step;
	EgStep(sndp, opp);
	opp->pg.phase += opp->pg.spd;
	step = opp->pg.phase >> opp->nst/*(PG_SHIFT + 5)*/;
	opp->pg.phase &=  (1 << opp->nst/*(PG_SHIFT + 5)*/) - 1;
	while ( step-- )
	{
		opp->pg.rng ^= ((opp->pg.rng & 1) << 16) + ((opp->pg.rng & 1) << 13);
		opp->pg.rng >>= 1;
	}
}

#if -1 >> 1 == -1
/* RIGHT SHIFT IS SIGNED */
#define SSR(x, y) ((Int32)(x) >> (y))
#else
/* RIGHT SHIFT IS UNSIGNED */
#define SSR(x, y) (((x) >= 0) ? ((x) >> (y)) : (-((-(x) - 1) >> (y)) - 1))
#endif


inline static void OpSynthMod( OPLSOUND* sndp, OPL_OP* opp )
{
	if ( opp->enable )
	{
		Uint32 tll;
		Int32 output;
		OpStep(sndp, opp);
		tll = opp->tll + (opp->eg.phase >> EG_SHIFT);
		tll = (tll >= (1 << TLLTBL_BITS)) ? LOG_KEYOFF : sndp->common.tll2logtbl [tll];
		tll += opp->tl_ofs;
#if TESTING_OPTIMIZE_AME
		tll += *opp->amin;
#else
		if ( opp->flag & FLAG_AME )
			tll += sndp->lfo [LFO_UNIT_AM].output;
#endif
		tll += opp->sintable [sndp->common.sintablemask & (opp->input + (opp->pg.phase >> PG_SHIFT))];
		output = LogToLin(sndp->logtbl, tll, -8 + ((LOG_LIN_BITS + 1) - (SINTBL_BITS + 2)));
		if ( opp->fb )
		{
#if USE_FBBUF
			Int32 fbtmp;
			fbtmp = opp->fbbuf + output;
			opp->fbbuf = output;
			opp->input = SSR(fbtmp, (9 - opp->fb));
#else
			opp->input = SSR(output, (8 - opp->fb));
#endif
		}
		opp [1].input = output;
	}
}

inline static Int32 OpSynthCarFb( OPLSOUND* sndp, OPL_OP* opp )
{
	if ( opp->enable )
	{
		Uint32 tll;
		OpStep(sndp, opp);
		tll = opp->tll + (opp->eg.phase >> EG_SHIFT);
		tll = (tll >= (1 << TLLTBL_BITS)) ? LOG_KEYOFF : sndp->common.tll2logtbl [tll];
		tll += opp->tl_ofs;
#if TESTING_OPTIMIZE_AME
		tll +=* opp->amin;
#else
		if ( opp->flag & FLAG_AME) tll += sndp->lfo [LFO_UNIT_AM].output;
#endif
		tll += opp->sintable [sndp->common.sintablemask & (opp->input + (opp->pg.phase >> PG_SHIFT))];
		if ( opp->fb )
		{
#if USE_FBBUF
			Int32 output, fbtmp;
			output = LogToLin(sndp->logtbl, tll, -8 + ((LOG_LIN_BITS + 1) - (SINTBL_BITS + 2)));
			fbtmp = opp->fbbuf + output;
			opp->fbbuf = output;
			opp->input = SSR(fbtmp, (9 - opp->fb));
#else
			Int32 output;
			output = LogToLin(sndp->logtbl, tll, -8 + ((LOG_LIN_BITS + 1) - (SINTBL_BITS + 2)));
			opp->input = SSR(output, (8 - opp->fb));
#endif
		}
		return LogToLin(sndp->logtbl, tll + sndp->common.mastervolume, -8 + LOG_LIN_BITS - 19 - opp->lvl);
	}
	return 0;
}

inline static Int32 OpSynthCar( OPLSOUND* sndp, OPL_OP* opp )
{
	if ( opp->enable )
	{
		Uint32 tll;
		OpStep(sndp, opp);
		tll = opp->tll + (opp->eg.phase >> EG_SHIFT);
		tll = (tll >= (1 << TLLTBL_BITS)) ? LOG_KEYOFF : sndp->common.tll2logtbl [tll];
		tll += opp->tl_ofs;
#if TESTING_OPTIMIZE_AME
		tll += *opp->amin;
#else
		if ( opp->flag & FLAG_AME )
			tll += sndp->lfo [LFO_UNIT_AM].output;
#endif
		tll += opp->sintable [sndp->common.sintablemask & (opp->input + (opp->pg.phase >> PG_SHIFT))];
		return LogToLin(sndp->logtbl, tll + sndp->common.mastervolume, -8 + LOG_LIN_BITS - 19 - opp->lvl);
	}
	return 0;
}

inline static Int32 OpSynthTom( OPLSOUND* sndp, OPL_OP* opp )
{
	if ( opp->enable )
	{
		Uint32 tll;
		OpStep(sndp, opp);
		tll = opp->tll + (opp->eg.phase >> EG_SHIFT);
		tll = (tll >= 128 - 16) ? LOG_KEYOFF : sndp->common.tll2logtbl [tll];
		tll += opp->tl_ofs;
		tll += opp->sintable [sndp->common.sintablemask & (opp->pg.phase >> PG_SHIFT)];
		return LogToLin(sndp->logtbl, tll + sndp->common.mastervolume, -8 + LOG_LIN_BITS - 19 - opp->lvl);
	}
	return 0;
}


static Int32 OpSynthRym( OPLSOUND* sndp, OPL_OP* opp )
{
	if ( opp->enable )
	{
		Uint32 tll;
		OpStepNG(sndp, opp);
		tll = opp->tll + (opp->eg.phase >> EG_SHIFT) + 0x10/* +6dB */;
		tll = (tll >= (1 << TLLTBL_BITS)) ? LOG_KEYOFF : sndp->common.tll2logtbl [tll];
		tll += opp->tl_ofs;
		tll += (opp->pg.rng & 1);
		return LogToLin(sndp->logtbl, tll + sndp->common.mastervolume, -8 + LOG_LIN_BITS - 19 - opp->lvl);
	}
	return 0;
}

inline static void LfoStep(OPL_LFO* lfop )
{
	lfop->cnt += lfop->sps;
	lfop->adr += lfop->cnt >> LFO_SHIFT;
	lfop->cnt &= (1 << LFO_SHIFT) - 1;
	lfop->output = lfop->table [lfop->adr & lfop->adrmask];
}

static int sndsynth( OPLSOUND* sndp )
{
	Int32 accum = 0;
	if ( sndp->common.enable )
	{
		Uint32 i, rch;
		for ( i = 0; i < LFO_UNIT_NUM; i++ )
			LfoStep(&sndp->lfo [i]);
		
		rch = sndp->common.rmode ? 7 : 9;
		for ( i = 0; i < rch; i++ )
		{
			if ( sndp->ch [i].op [0].modcar )
				OpSynthMod(sndp, &sndp->ch [i].op [0]);
			else
				accum += OpSynthCarFb(sndp, &sndp->ch [i].op [0]);
			accum += OpSynthCar(sndp, &sndp->ch [i].op [1]);
		}
		if ( sndp->common.rmode )
		{
			accum += OpSynthRym(sndp, &sndp->ch [7].op [0]);
			accum += OpSynthRym(sndp, &sndp->ch [7].op [1]);
			accum += OpSynthTom(sndp, &sndp->ch [8].op [0]);
			accum += OpSynthRym(sndp, &sndp->ch [8].op [1]);
		}
	}
	if ( sndp->deltatpcm )
	{
		accum += sndp->deltatpcm->synth(sndp->deltatpcm->ctx);
	}
#if 0
	/* NISE DAC */
	if ( accum >= 0 )
		accum =  (Int32)(((Uint32) accum) & (((1 << 8) - 1) << (23 - 8)));
	else
		accum = -(Int32)(((Uint32)-accum) & (((1 << 8) - 1) << (23 - 8)));
#endif
	return accum;
}

static void sndvolume( OPLSOUND* sndp, Int32 volume )
{
	if ( sndp->deltatpcm) sndp->deltatpcm->volume(sndp->deltatpcm->ctx, volume);
	volume = (volume << (LOG_BITS - 8)) << 1;
	sndp->common.mastervolume = volume;
}

static Uint8 const op_table [0x20]=
{
	   0,   2,   4,   1,   3,   5,0xFF,0xFF,
	   6,   8,  10,   7,   9,  11,0xFF,0xFF,
	  12,  14,  16,  13,  15,  17,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
};

static Uint8 const mul_table [0x10]=
{
	 1+0, 2+0, 4+0, 2+4, 8+0, 8+2, 8+4,16-2,
	16+0,16+2,16+4,16+4,16+8,16+8,32-2,32-2,
};

#define DB2TLL(x) (x * 2 / 375 )
static Uint8 const ksl_table [8] [16]=
{
	{
		DB2TLL(    0), DB2TLL(    0), DB2TLL(    0), DB2TLL(    0),
		DB2TLL(    0), DB2TLL(    0), DB2TLL(    0), DB2TLL(    0),
		DB2TLL(    0), DB2TLL(    0), DB2TLL(    0), DB2TLL(    0),
		DB2TLL(    0), DB2TLL(    0), DB2TLL(    0), DB2TLL(    0),
	},{
		DB2TLL(    0), DB2TLL(    0), DB2TLL(    0), DB2TLL(    0),
		DB2TLL(    0), DB2TLL(    0), DB2TLL(    0), DB2TLL(    0),
		DB2TLL(    0), DB2TLL(  750), DB2TLL( 1125), DB2TLL( 1500),
		DB2TLL( 1875), DB2TLL( 2250), DB2TLL( 2625), DB2TLL( 3000),
	},{
		DB2TLL(    0), DB2TLL(    0), DB2TLL(    0), DB2TLL(    0),
		DB2TLL(    0), DB2TLL( 1125), DB2TLL( 1875), DB2TLL( 2625),
		DB2TLL( 3000), DB2TLL( 3750), DB2TLL( 4125), DB2TLL( 4500),
		DB2TLL( 4875), DB2TLL( 5250), DB2TLL( 5625), DB2TLL( 6000),
	},{
		DB2TLL(    0), DB2TLL(    0), DB2TLL(    0), DB2TLL( 1875),
		DB2TLL( 3000), DB2TLL( 4125), DB2TLL( 4875), DB2TLL( 5625),
		DB2TLL( 6000), DB2TLL( 6750), DB2TLL( 7125), DB2TLL( 7500),
		DB2TLL( 7875), DB2TLL( 8250), DB2TLL( 8625), DB2TLL( 9000),
	},{
		DB2TLL(    0), DB2TLL(    0), DB2TLL( 3000), DB2TLL( 4875),
		DB2TLL( 6000), DB2TLL( 7125), DB2TLL( 7875), DB2TLL( 8625),
		DB2TLL( 9000), DB2TLL( 9750), DB2TLL(10125), DB2TLL(10500),
		DB2TLL(10875), DB2TLL(11250), DB2TLL(11625), DB2TLL(12000),
	},{
		DB2TLL(    0), DB2TLL( 3000), DB2TLL( 6000), DB2TLL( 7875),
		DB2TLL( 9000), DB2TLL(10125), DB2TLL(10875), DB2TLL(11625),
		DB2TLL(12000), DB2TLL(12750), DB2TLL(13125), DB2TLL(13500),
		DB2TLL(13875), DB2TLL(14250), DB2TLL(14625), DB2TLL(15000),
	},{
		DB2TLL(    0), DB2TLL( 6000), DB2TLL( 9000), DB2TLL(10875),
		DB2TLL(12000), DB2TLL(13125), DB2TLL(13875), DB2TLL(14625),
		DB2TLL(15000), DB2TLL(15750), DB2TLL(16125), DB2TLL(16500),
		DB2TLL(16875), DB2TLL(17250), DB2TLL(17625), DB2TLL(18000),
	},{
		DB2TLL(    0), DB2TLL( 9000), DB2TLL(12000), DB2TLL(13875),
		DB2TLL(15000), DB2TLL(16125), DB2TLL(16875), DB2TLL(17625),
		DB2TLL(18000), DB2TLL(18750), DB2TLL(19125), DB2TLL(19500),
		DB2TLL(19875), DB2TLL(20250), DB2TLL(20625), DB2TLL(21000),
	}
};
#undef DB2TLL

static Uint32 rateconvAR( OPLSOUND* sndp, Uint32 rrr, Uint32 ksr )
{
	if ( !rrr )
		return 0;
	rrr = rrr + (ksr >> 2);
	if ( rrr >= 15)
		return AR_PHASEMAX;
	return sndp->common.ratetbla [ksr & 3] >> (CPS_SHIFTE + 1 - rrr);
}

static Uint32 rateconv( OPLSOUND* sndp, Uint32 rrr, Uint32 ksr )
{
	if ( !rrr )
		return 0;
	rrr = rrr + (ksr >> 2);
	if ( rrr > 15 )
		rrr = 15;
	return sndp->common.ratetbl [ksr & 3] >> (CPS_SHIFTE + 1 - rrr);
}

static void OpUpdateWF( OPLSOUND* sndp, OPL_OP* opp )
{
	opp->sintable = sndp->opltbl->sin_table [opp->wf & sndp->common.wfe];
}

static void OpUpdatePG( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp )
{
	opp->pg.spd = (((chp->freqh << 8) + chp->freql) * opp->mul * sndp->common.cpsp) >> (CPS_SHIFTP - chp->blk);
}

static void OpUpdateEG( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp )
{
	Uint32 sr, rr;
	opp->ksr = chp->kcode >> ((opp->flag & FLAG_KSR) ? 0 : 2);
	opp->eg.dr_phasemax = opp->sl << (1 + 2 + EG_SHIFT);    /* 3dB->eg */
	opp->eg.spd [EG_MODE_ATTACK] = rateconvAR(sndp, opp->ar, opp->ksr);
	opp->eg.spd [EG_MODE_DECAY] = rateconv(sndp, opp->dr, opp->ksr);
	if ( opp->flag & FLAG_EGT )
	{
		if ( opp->eg.mode == EG_MODE_SUSTINE )
			opp->eg.mode = EG_MODE_SUSHOLD;
		sr = 0;
		rr = opp->rr;
	}
	else
	{
		if ( opp->eg.mode == EG_MODE_SUSHOLD )
			opp->eg.mode = EG_MODE_SUSTINE;
		sr = opp->rr;
		rr = 7;
	}
	if ( chp->sus )
	{
		rr = 5;
	}
	opp->eg.spd [EG_MODE_SUSTINE] = rateconv(sndp, sr, opp->ksr);
	opp->eg.spd [EG_MODE_RELEASE] = rateconv(sndp, rr, opp->ksr);
}

static void OpUpdateTLL( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp )
{
	opp->tll = (opp->tl + (chp->ksb >> opp->ksl)) << 1;
}



static void oplsetopmul( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp, Uint32 v )
{
	opp->flag &= ~(FLAG_AME | FLAG_PME | FLAG_EGT | FLAG_KSR);
#if TESTING_OPTIMIZE_AME
	if ( v & 0x80 )
		opp->amin = &sndp->lfo [LFO_UNIT_AM].output; else opp->amin = &sndp->common.amzero;
#else
	if ( v & 0x80 )
		opp->flag |= FLAG_AME;
#endif
	if ( v & 0x40 ) opp->flag |= FLAG_PME;
	if ( v & 0x20 ) opp->flag |= FLAG_EGT;
	if ( v & 0x10 ) opp->flag |= FLAG_KSR;
	opp->mul = mul_table [v & 0x0F];
	OpUpdateEG(sndp, chp, opp);
	OpUpdatePG(sndp, chp, opp);
}

static void oplsetopkstl( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp, Uint32 v )
{
	opp->ksl = (v >> 6) ? (3 - (v >> 6)) : 15;  /* 0 / 1.5 / 3 / 6 db/OCT */
	opp->tl = v & 0x3F; /* 0.75 db */
	OpUpdateTLL(sndp, chp, opp);
}

static void oplsetopardr( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp, Uint32 v )
{
	opp->ar = v >> 4;
	opp->dr = v & 0xF;
	OpUpdateEG(sndp, chp, opp);
}

static void oplsetopslrr( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp, Uint32 v )
{
	opp->sl = v >> 4;
	opp->rr = v & 0xF;
	OpUpdateEG(sndp, chp, opp);
}

static void oplsetopwf( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp, Uint32 v )
{
	opp->wf = v & 0x3;
	OpUpdateWF(sndp, opp);
}

static void oplsetopkey( OPLSOUND* sndp, OPL_OP* opp )
{
	Uint8 nextkey = ((sndp->common.rmode) && opp->rkey) || opp->key;
	if ( opp->prevkey ^ nextkey )
	{
		opp->prevkey = nextkey;
		if ( nextkey )
		{
			sndp->common.enable = 1;
			opp->eg.mode = EG_MODE_ATTACK;
			opp->eg.phase = EG_KEYOFF;
			opp->enable = 1;
			opp->eg.phasear = 0;
		}
		else if ( !opp->modcar && opp->eg.mode != EG_MODE_OFF )
			opp->eg.mode = EG_MODE_RELEASE;
	}
}

static void oplsetchfreql( OPLSOUND* sndp, OPL_CH* chp, Uint32 v )
{
	chp->freql = v & 0xFF;
	chp->ksb = ksl_table [chp->blk] [(chp->freqh << 2) + (chp->freql >> 6)];
	OpUpdatePG(sndp, chp, &chp->op [0]);
	OpUpdatePG(sndp, chp, &chp->op [1]);
	OpUpdateTLL(sndp, chp, &chp->op [0]);
	OpUpdateTLL(sndp, chp, &chp->op [1]);
}

static void oplsetchfreqh( OPLSOUND* sndp, OPL_CH* chp, Uint32 v )
{
	Uint32 key = v & 0x20;
	chp->kcode = (v >> 1) & 15;
	chp->freqh = v & 3;
	chp->blk = (v >> 2) & 7;
	chp->op [0].key = chp->op [1].key = key;
	oplsetopkey(sndp, &chp->op [0]);
	oplsetopkey(sndp, &chp->op [1]);
	chp->sus = 0;
	chp->ksb = ksl_table [chp->blk] [(chp->freqh << 2) + (chp->freql >> 6)];
	OpUpdateEG(sndp, chp, &chp->op [0]);
	OpUpdateEG(sndp, chp, &chp->op [1]);
	OpUpdatePG(sndp, chp, &chp->op [0]);
	OpUpdatePG(sndp, chp, &chp->op [1]);
	OpUpdateTLL(sndp, chp, &chp->op [0]);
	OpUpdateTLL(sndp, chp, &chp->op [1]);
}

static void oplsetchfbcon( OPLSOUND* sndp, OPL_CH* chp, Uint32 v )
{
	chp->op [0].fb = (v >> 1) & 7;
#if USE_FBBUF
	chp->op [0].fbbuf = 0;
#endif
	chp->con = v & 1;
	chp->op [0].modcar = (chp->con) ? 0 : 1;
	OpUpdateEG(sndp, chp, &chp->op [0]);
	chp->op [1].input = 0;
}

static void opllsetopvolume( OPLSOUND* sndp, OPL_CH* chp, OPL_OP* opp, Uint32 v )
{
	opp->tl = v;
	OpUpdateTLL(sndp, chp, opp);
}

static void opllsetchfreql( OPLSOUND* sndp, OPL_CH* chp, Uint32 v )
{
	chp->freql = v & 0xFF;
	chp->ksb = ksl_table [chp->blk] [(chp->freqh << 3) + (chp->freql >> 5)];
	OpUpdatePG(sndp, chp, &chp->op [0]);
	OpUpdatePG(sndp, chp, &chp->op [1]);
	OpUpdateTLL(sndp, chp, &chp->op [0]);
	OpUpdateTLL(sndp, chp, &chp->op [1]);
}

static void opllsetchfreqh( OPLSOUND* sndp, OPL_CH* chp, Uint32 v )
{
	Uint32 key = v & 0x10;
	chp->kcode = v & 15;
	chp->freqh = v & 1;
	chp->blk = (v >> 1) & 7;
	chp->op [0].key = chp->op [1].key = key;
	oplsetopkey(sndp, &chp->op [0]);
	oplsetopkey(sndp, &chp->op [1]);
	chp->sus = v & 0x20;
	chp->ksb = ksl_table [chp->blk] [(chp->freqh << 3) + (chp->freql >> 5)];
	OpUpdateEG(sndp, chp, &chp->op [0]);
	OpUpdateEG(sndp, chp, &chp->op [1]);
	OpUpdatePG(sndp, chp, &chp->op [0]);
	OpUpdatePG(sndp, chp, &chp->op [1]);
	OpUpdateTLL(sndp, chp, &chp->op [0]);
	OpUpdateTLL(sndp, chp, &chp->op [1]);
}

static void SetOpTone( OPLSOUND* sndp, OPL_OP* opp, Uint8* tonep )
{
	opp->flag &= ~(FLAG_AME | FLAG_PME | FLAG_EGT | FLAG_KSR);
#if TESTING_OPTIMIZE_AME
	if ( tonep [0] & 0x80 ) opp->amin = &sndp->lfo [LFO_UNIT_AM].output; else opp->amin = &sndp->common.amzero;
#else
	if ( tonep [0] & 0x80 ) opp->flag |= FLAG_AME;
#endif
	if ( tonep [0] & 0x40 ) opp->flag |= FLAG_PME;
	if ( tonep [0] & 0x20 ) opp->flag |= FLAG_EGT;
	if ( tonep [0] & 0x10 ) opp->flag |= FLAG_KSR;
	opp->mul = mul_table [tonep [0] & 0x0F] << 1;
	opp->ksl = (tonep [2] >> 6) ? (3 - (tonep [2] >> 6)) : 15;
	opp->ar = tonep [4] >> 4;
	opp->dr = tonep [4] & 0xF;
	opp->sl = tonep [6] >> 4;
	opp->rr = tonep [6] & 0xF;
}
static void SetChTone( OPLSOUND* sndp, OPL_CH* chp, Uint8* tonep, Uint8* tlofsp )
{
	Uint32 op;
	for ( op = 0; op < 2; op++ )
		SetOpTone(sndp, &chp->op [op], &tonep [op]);
	chp->op [0].tl_ofs = (tlofsp [0] ^ 0x80) << (LOG_BITS - 4 + 1);
	chp->op [1].tl_ofs = (tlofsp [1] ^ 0x80) << (LOG_BITS - 4 + 1);
	chp->op [0].tl = tonep [2] & 0x3F;
	chp->op [0].fb = tonep [3] & 0x7;
	chp->op [0].wf = (tonep [3] >> 3) & 1;
	chp->op [1].wf = (tonep [3] >> 4) & 1;
#if USE_FBBUF
	chp->op [0].fbbuf = 0;
#endif
	chp->op [1].input = 0;
	OpUpdateWF(sndp, &chp->op [0]);
	OpUpdateWF(sndp, &chp->op [1]);
	OpUpdateEG(sndp, chp, &chp->op [0]);
	OpUpdateEG(sndp, chp, &chp->op [1]);
	OpUpdatePG(sndp, chp, &chp->op [0]);
	OpUpdatePG(sndp, chp, &chp->op [1]);
	OpUpdateTLL(sndp, chp, &chp->op [0]);
	OpUpdateTLL(sndp, chp, &chp->op [1]);
}

static void opllsetchtone( OPLSOUND* sndp, OPL_CH* chp, Uint32 tone )
{
	SetChTone(sndp, chp, &sndp->regs [OPLL_INST_WORK + (tone << 3)], &sndp->regs [OPLL_INST_WORK2 + (tone << 1) + 0]);
}

static void recovercon( OPLSOUND* sndp, OPL_CH* chp )
{
	chp->op [0].modcar = (chp->con) ? 0 : 1;
	chp->op [0].lvl = chp->con ? 1 : 0;
	chp->op [1].lvl = 1;
	OpUpdateEG(sndp, chp, &chp->op [0]);
	chp->op [1].input = 0;
}

static void initrc_common( OPLSOUND* sndp, Uint32 rmode )
{
	if ( rmode )
	{
		/* BD */
		sndp->ch [6].op [0].modcar = 1;
		sndp->ch [6].op [0].lvl = 0;
		OpUpdateEG(sndp, &sndp->ch [6], &sndp->ch [6].op [0]);
		sndp->ch [6].op [1].input = 0;
		sndp->ch [6].op [1].lvl = 2;
		/* CYM */
		sndp->ch [7].op [0].modcar = 0;
		sndp->ch [7].op [0].lvl = 1;
		OpUpdateEG(sndp, &sndp->ch [7], &sndp->ch [7].op [0]);
		/* SD */
		sndp->ch [7].op [1].input = 0;
		sndp->ch [7].op [1].lvl = 2;
		/* TOM */
		sndp->ch [8].op [0].modcar = 0;
		sndp->ch [8].op [0].lvl = 2;
		OpUpdateEG(sndp, &sndp->ch [8], &sndp->ch [8].op [0]);
		/* HH */
		sndp->ch [8].op [1].input = 0;
		sndp->ch [8].op [1].lvl = 1;
	}
	else
	{
		recovercon(sndp, &sndp->ch [6]);
		if ( !sndp->ch [6].op [0].key ) SetOpOff(&sndp->ch [6].op [0]);
		if ( !sndp->ch [6].op [1].key ) SetOpOff(&sndp->ch [6].op [1]);
		recovercon(sndp, &sndp->ch [7]);
		if ( !sndp->ch [7].op [0].key ) SetOpOff(&sndp->ch [7].op [0]);
		if ( !sndp->ch [7].op [1].key ) SetOpOff(&sndp->ch [7].op [1]);
		recovercon(sndp, &sndp->ch [8]);
		if ( !sndp->ch [8].op [0].key ) SetOpOff(&sndp->ch [8].op [0]);
		if ( !sndp->ch [8].op [1].key ) SetOpOff(&sndp->ch [8].op [1]);
	}
}

static void oplsetrc( OPLSOUND* sndp, Uint32 rc )
{
	sndp->lfo [LFO_UNIT_AM].table = (rc & 0x80) ? sndp->opltbl->am_table1 : sndp->opltbl->am_table2;
	sndp->lfo [LFO_UNIT_PM].table = (rc & 0x40) ? sndp->opltbl->pm_table1 : sndp->opltbl->pm_table2;
	if ( (sndp->common.rmode ^ rc) & 0x20 )
	{
		if ( rc & 0x20 )
		{
#if 0
			static Uint8 volini [2] = { 0, 0 };
			static Uint8 bdtone [8] = { 0x04, 0x20, 0x28, 0x00, 0xDF, 0xF8, 0xFF, 0xF8 };
			SetChTone(sndp, &sndp->ch [6], bdtone, volini);
			SetChTone(sndp, &sndp->ch [7], &romtone [0] [0x11 << 4], volini);
			SetChTone(sndp, &sndp->ch [8], &romtone [0] [0x12 << 4], volini);
#endif
			sndp->ch [7].op [0].nst = PG_SHIFT + 4;
			sndp->ch [7].op [1].nst = PG_SHIFT + 6;
			sndp->ch [8].op [1].nst = PG_SHIFT + 5;
		}
		initrc_common(sndp, rc & 0x20);
	}
	sndp->common.rmode = rc & 0x20;
	sndp->common.rc = rc & 0x1F;
	/* BD */
	sndp->ch [6].op [0].rkey = sndp->ch [6].op [1].rkey = rc & 0x10;
	oplsetopkey(sndp, &sndp->ch [6].op [0]);
	oplsetopkey(sndp, &sndp->ch [6].op [1]);
	/* CYM */
	sndp->ch [7].op [0].rkey = rc & 0x01;
	oplsetopkey(sndp, &sndp->ch [7].op [0]);
	/* SD */
	sndp->ch [7].op [1].rkey = rc & 0x08;
	oplsetopkey(sndp, &sndp->ch [7].op [1]);
	/* TOM */
	sndp->ch [8].op [0].rkey = rc & 0x04;
	oplsetopkey(sndp, &sndp->ch [8].op [0]);
	/* HH */
	sndp->ch [8].op [1].rkey = rc & 0x02;
	oplsetopkey(sndp, &sndp->ch [8].op [1]);
}

static void opllsetrc( OPLSOUND* sndp, Uint32 rc )
{
	if ( (sndp->common.rmode ^ rc) & 0x20 )
	{
		if ( rc & 0x20 )
		{
			opllsetchtone(sndp, &sndp->ch [6], 0x10);
			opllsetchtone(sndp, &sndp->ch [7], 0x11);
			opllsetchtone(sndp, &sndp->ch [8], 0x12);
			opllsetopvolume(sndp, &sndp->ch [7], &sndp->ch [7].op [0], (sndp->regs [0x37] & 0xF0) >> 2);
			opllsetopvolume(sndp, &sndp->ch [8], &sndp->ch [8].op [0], (sndp->regs [0x38] & 0xF0) >> 2);
			sndp->ch [7].op [0].nst = PG_SHIFT + 5;
			sndp->ch [7].op [1].nst = PG_SHIFT + 5;
			sndp->ch [8].op [1].nst = PG_SHIFT + 5;
		}
		else
		{
			opllsetchtone(sndp, &sndp->ch [6], sndp->regs [0x36]>>4);
			opllsetchtone(sndp, &sndp->ch [7], sndp->regs [0x37]>>4);
			opllsetchtone(sndp, &sndp->ch [8], sndp->regs [0x38]>>4);
		}
		initrc_common(sndp, rc & 0x20);
	}
	sndp->common.rmode = rc & 0x20;
	sndp->common.rc = rc & 0x1F;
	/* BD */
	sndp->ch [6].op [0].rkey = sndp->ch [6].op [1].rkey = rc & 0x10;
	oplsetopkey(sndp, &sndp->ch [6].op [0]);
	oplsetopkey(sndp, &sndp->ch [6].op [1]);
	/* CYM */
	sndp->ch [7].op [0].rkey = rc & 0x01;
	oplsetopkey(sndp, &sndp->ch [7].op [0]);
	/* SD */
	sndp->ch [7].op [1].rkey = rc & 0x08;
	oplsetopkey(sndp, &sndp->ch [7].op [1]);
	/* TOM */
	sndp->ch [8].op [0].rkey = rc & 0x04;
	oplsetopkey(sndp, &sndp->ch [8].op [0]);
	/* HH */
	sndp->ch [8].op [1].rkey = rc & 0x02;
	oplsetopkey(sndp, &sndp->ch [8].op [1]);
}

#define OPLSETOP(func) { \
	Uint32 op = op_table [a & 0x1F]; \
	if ( op != 0xFF) func(sndp, &sndp->ch [op >> 1], &sndp->ch [op >> 1].op [op & 1], v); \
}

__inline static void oplwritereg( OPLSOUND* sndp, Uint32 a, Uint32 v )
{
	switch ( a >> 5 )
	{
	default:
		NEVER_REACH
	
	case 0:
		switch ( a & 0x1F )
		{
		case 0x01:
			if ( sndp->opl_type == OPL_TYPE_OPL2 )
			{
				Uint32 i;
				sndp->common.wfe = (v & 0x20) ? 3 : 0;
				for ( i = 0; i < 9; i++ )
				{
					OpUpdateWF(sndp, &sndp->ch [i].op [0]);
					OpUpdateWF(sndp, &sndp->ch [i].op [1]);
				}
			}
			break;
		
		case 0x08:
			/* CSM mode */
		case 0x07:  case 0x09:  case 0x0A:  case 0x0B:  case 0x0C:
		case 0x0D:  case 0x0E:  case 0x0F:  case 0x10:  case 0x11:  case 0x12:
			if ( sndp->deltatpcm )
				sndp->deltatpcm->write(sndp->deltatpcm->ctx, a - 0x07, v);
			break;
		}
		break;
	
	case 1: OPLSETOP(oplsetopmul); break;
	case 2: OPLSETOP(oplsetopkstl); break;
	case 3: OPLSETOP(oplsetopardr); break;
	case 4: OPLSETOP(oplsetopslrr); break;
	case 7: OPLSETOP(oplsetopwf); break;
	case 5:
		if ( (a & 0x1F) == (0xBD & 0x1F) )
			oplsetrc(sndp, v);
		else if ( (a & 0x1F) < 9 )
			oplsetchfreql(sndp, &sndp->ch [a & 0xF], v);
		else if ( (a & 0xF) < 9 )
			oplsetchfreqh(sndp, &sndp->ch [a & 0xF], v);
		break;
	
	case 6:
		if ( (a & 0x1F) < 9) oplsetchfbcon(sndp, &sndp->ch [a & 0xF], v);
		break;
	}
}

static void oplwrite( OPLSOUND* sndp, Uint32 a, Uint32 v )
{
	if ( a & 1 )
	{
		sndp->regs [sndp->common.adr] = v;
		oplwritereg(sndp, sndp->common.adr, v);
	}
	else
		sndp->common.adr = v;
}

static Uint32 oplread( OPLSOUND* sndp, Uint32 a )
{
	if ( a & 1 )
		return sndp->regs [sndp->common.adr];
	else
		return 0x80;
}

__inline static void opllwritereg( OPLSOUND* sndp, Uint32 a, Uint32 v )
{
	switch ( a >> 3 )
	{
	default:
		NEVER_REACH
	case 0:
		sndp->regs [OPLL_INST_WORK + (a & 7)] = v;
		break;
	
	case 1:
		if ( a == 0xE) opllsetrc(sndp, v & 0x3F);
		break;
	
	case 2:
	case 3:
		a &= 0xF;
		if ( a < 9) opllsetchfreql(sndp, &sndp->ch [a], v);
		break;
	
	case 4:
	case 5:
		a &= 0xF;
		if ( a < 9) opllsetchfreqh(sndp, &sndp->ch [a], v);
		break;
	
	case 6:
	case 7:
		a &= 0xF;
		if ( a < 9 )
		{
			if ( (sndp->common.rmode) && (a >= 6) )
			{
				if ( a != 6) opllsetopvolume(sndp, &sndp->ch [a], &sndp->ch [a].op [0], (v & 0xF0) >> 2);
			}
			else
			{
				opllsetchtone(sndp, &sndp->ch [a], (v & 0xF0) >> 4);
			}
			opllsetopvolume(sndp, &sndp->ch [a], &sndp->ch [a].op [1], (v & 0xF) << 2);
		}
		break;
	}
}

static void opllwrite( OPLSOUND* sndp, Uint32 a, Uint32 v )
{
	if ( a & 1 )
	{
		if ( sndp->common.adr < 0x40 )
		{
			sndp->regs [sndp->common.adr] = v;
			opllwritereg(sndp, sndp->common.adr, v);
		}
	}
	else
		sndp->common.adr = v;
}

static Uint32 opllread( OPLSOUND* sndp, Uint32 a )
{
	return 0xFF;
}

static void opreset( OPLSOUND* sndp, OPL_OP* opp )
{
	/* XMEMSET(opp, 0, sizeof(OPL_OP)); */
	SetOpOff(opp);
	opp->tl_ofs = 0x80 << (LOG_BITS - 4 + 1);
#if TESTING_OPTIMIZE_AME
	opp->amin = &sndp->common.amzero;
#endif
	opp->pg.rng = 0xFFFF;
}

static void chreset( OPLSOUND* sndp, OPL_CH* chp, Uint32 clock, Uint32 freq )
{
	Uint32 op;
	XMEMSET(chp, 0, sizeof(OPL_CH));
	for ( op = 0; op < 2; op++ )
	{
		opreset(sndp, &chp->op [op]);
	}
	recovercon(sndp, chp);
}

static void sndreset( OPLSOUND* sndp, Uint32 clock, Uint32 freq )
{
	Uint32 i, cpse;
	XMEMSET(&sndp->common, 0, sizeof(sndp->common));
	XMEMSET(&sndp->lfo [LFO_UNIT_AM], 0, sizeof(OPL_LFO));
	sndp->lfo [LFO_UNIT_AM].sps = DivFix(37 * (1 << AMTBL_BITS), freq * 10, LFO_SHIFT);
	sndp->lfo [LFO_UNIT_AM].adrmask = (1 << AMTBL_BITS) - 1;
	sndp->lfo [LFO_UNIT_AM].table = sndp->opltbl->am_table1;
	XMEMSET(&sndp->lfo [LFO_UNIT_PM], 0, sizeof(OPL_LFO));
	sndp->lfo [LFO_UNIT_PM].sps = DivFix(64 * (1 << PMTBL_BITS), freq * 10, LFO_SHIFT);
	sndp->lfo [LFO_UNIT_PM].adrmask = (1 << PMTBL_BITS) - 1;
	sndp->lfo [LFO_UNIT_PM].table = sndp->opltbl->pm_table1;
	sndp->common.cpsp = DivFix(clock, 72 * freq, CPS_SHIFTP);
	cpse = DivFix(clock, 72 * freq, CPS_SHIFTE);
	for ( i = 0; i < 4; i++ )
	{
		sndp->common.ratetbl [i] = (i + 4) * cpse;
		sndp->common.ratetbla [i] = 3 * sndp->common.ratetbl [i];
	}
	sndp->common.tll2logtbl = sndp->opltbl->tll2log_table;
	sndp->common.sintablemask = (1 << SINTBL_BITS) - 1;
	
	for ( i = 0; i < 9; i++ )
		chreset(sndp, &sndp->ch [i], clock, freq);
	
	if ( sndp->deltatpcm )
		sndp->deltatpcm->reset(sndp->deltatpcm->ctx, clock, freq);
	
	if ( sndp->opl_type & OPL_TYPE_OPL )
	{
		XMEMSET(&sndp->regs, 0, 0x100);
		sndp->common.ar_table = sndp->opltbl->ar_tablepow;
		sndp->common.sintablemask -= (1 << (SINTBL_BITS - 11)) - 1;
		for ( i = 0x0; i < 0x100; i++ )
		{
			oplwrite(sndp, 0, i);
			oplwrite(sndp, 1, 0x00);
		}
		for ( i = 0xA0; i < 0xA9; i++ )
		{
			oplwrite(sndp, 0, 0xA0 + i);
			oplwrite(sndp, 1, 0x40);
			oplwrite(sndp, 0, 0xB0 + i);
			oplwrite(sndp, 1, 0x0E);
		}
	}
	else
	{
		static Uint8 const fmbios_initdata [9] = "\x30\x10\x20\x20\xfb\xb2\xf3\xf3";
		XMEMSET(&sndp->regs, 0, 0x40);
		sndp->common.ar_table = sndp->opltbl->ar_tablelog;
		sndp->common.wfe = 1;
		sndp->common.sintablemask -= (1 << (SINTBL_BITS - 8)) - 1;
		for ( i = 0; i < sizeof(fmbios_initdata)-1; i++ )
		{
			opllwrite(sndp, 0, i);
			opllwrite(sndp, 1, fmbios_initdata [i]);
		}
		opllwrite(sndp, 0, 0x0E);
		opllwrite(sndp, 1, 0x00);
		opllwrite(sndp, 0, 0x0F);
		opllwrite(sndp, 1, 0x00);
		for ( i = 0; i < 9; i++ )
		{
			opllwrite(sndp, 0, 0x10 + i);
			opllwrite(sndp, 1, 0x20);
			opllwrite(sndp, 0, 0x20 + i);
			opllwrite(sndp, 1, 0x07);
			opllwrite(sndp, 0, 0x30 + i);
			opllwrite(sndp, 1, 0xB3);
		}
	}
}

static void oplsetinst( OPLSOUND* sndp, Uint32 n, void* p, Uint32 l )
{
	if ( sndp->deltatpcm) sndp->deltatpcm->setinst(sndp->deltatpcm->ctx, n, p, l);
}

__inline static Uint32 GetDwordLE(Uint8* p )
{
	return p [0] | (p [1] << 8) | (p [2] << 16) | (p [3] << 24);
}
#define GetDwordLEM(p) (Uint32)((((Uint8* )p) [0] | (((Uint8* )p) [1] << 8) | (((Uint8* )p) [2] << 16) | (((Uint8* )p) [3] << 24)) )

static void opllsetinst( OPLSOUND* sndp, Uint32 n, Uint8* p, Uint32 l )
{
	Int32 i, j, sb = 9;
	if ( n )
		return;
	if ( (GetDwordLE(p) & 0xF0FFFFFF) == GetDwordLEM("ILL0") )
	{
		if ( 0 < p [4] && p [4] <= SINTBL_BITS) sb = p [4];
		for ( j = 1; j < 16 + 3; j++ )
			for ( i = 0; i < 8; i++ )
				sndp->regs [OPLL_INST_WORK + (j << 3) + i] = p [(j << 4) + i];
		for ( j = 0; j < 16 + 3; j++ )
		{
			sndp->regs [OPLL_INST_WORK2 + (j << 1) + 0] = p [(j << 4) + 8];
			sndp->regs [OPLL_INST_WORK2 + (j << 1) + 1] = p [(j << 4) + 9];
		}
	}
	else
	{
		for ( j = 1; j < 16; j++ )
			for ( i = 0; i < 8; i++ )
				sndp->regs [OPLL_INST_WORK + (j << 3) + i] = p [((j - 1) << 3) + i];
	}
	sndp->common.sintablemask =  (1 <<  SINTBL_BITS)       - 1;
	sndp->common.sintablemask -= (1 << (SINTBL_BITS - sb)) - 1;
}

static void sndrelease( OPLSOUND* sndp )
{
	if ( sndp->logtbl) sndp->logtbl->release(sndp->logtbl->ctx);
	if ( sndp->opltbl) sndp->opltbl->release(sndp->opltbl->ctx);
	if ( sndp->deltatpcm) sndp->deltatpcm->release(sndp->deltatpcm->ctx);
	XFREE(sndp);
}

KMIF_SOUND_DEVICE* OPLSoundAlloc(Uint32 opl_type )
{
	OPLSOUND* sndp;
	sndp = (OPLSOUND*) XMALLOC(sizeof(OPLSOUND));
	if ( !sndp) return 0;
	sndp->opl_type = opl_type;
	sndp->kmif.ctx = sndp;
	sndp->kmif.release = (void (*)( void* )) sndrelease;
	sndp->kmif.volume = (void (*)( void*, int )) sndvolume;
	sndp->kmif.reset = (void (*)( void*, Uint32, Uint32 )) sndreset;
	sndp->kmif.synth = (int (*)( void* )) sndsynth;
	if ( sndp->opl_type == OPL_TYPE_MSXAUDIO )
	{
		sndp->deltatpcm = YMDELTATPCMSoundAlloc(YMDELTATPCM_TYPE_Y8950);
	}
	else
		sndp->deltatpcm = 0;
	if ( sndp->opl_type & OPL_TYPE_OPL )
	{
		sndp->kmif.write = (void (*)( void*, Uint32, Uint32 )) oplwrite;
		sndp->kmif.read = (Uint32 (*)( void*, Uint32 )) oplread;
		sndp->kmif.setinst = (void (*)( void*, Uint32, void*, Uint32 )) oplsetinst;
	}
	else
	{
		sndp->kmif.write = (void (*)( void*, Uint32, Uint32 )) opllwrite;
		sndp->kmif.read = (Uint32 (*)( void*, Uint32 )) opllread;
		sndp->kmif.setinst = (void (*)( void*, Uint32, void*, Uint32 )) opllsetinst;
		switch ( sndp->opl_type )
		{
		case OPL_TYPE_OPLL:
		case OPL_TYPE_MSXMUSIC:
			opllsetinst(sndp, 0, romtone [0], 16 * 19);
			break;
		
		case OPL_TYPE_SMSFMUNIT:
			opllsetinst(sndp, 0, romtone [1], 16 * 19);
			break;
		
		case OPL_TYPE_VRC7:
			opllsetinst(sndp, 0, romtone [2], 16 * 19);
			break;
		}
	}
	sndp->logtbl = LogTableAddRef();
	sndp->opltbl = OplTableAddRef();
	if ( !sndp->logtbl || !sndp->opltbl )
	{
		sndrelease(sndp);
		return 0;
	}

	return &sndp->kmif;
}
