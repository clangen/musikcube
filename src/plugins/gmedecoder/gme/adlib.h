/*
 *  Copyright (C) 2002-2009  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* $Id: adlib.h,v 1.4 2009/04/26 10:33:53 harekiet Exp $ */

#ifndef DOSBOX_ADLIB_H
#define DOSBOX_ADLIB_H

/*#include "dosbox.h"
#include "mixer.h"
#include "inout.h"
#include "mixer.h"
#include "setup.h"
#include "pic.h"
#include "hardware.h"*/


namespace Adlib {

struct Timer {
	double start;
	double delay;
	bool enabled, overflow, masked;
	Bit8u counter;
	Timer() {
		masked = false;
		overflow = false;
		enabled = false;
		counter = 0;
		delay = 0;
	}
	//Call update before making any further changes
	void Update( double time ) {
		if ( !enabled || !delay ) 
			return;
		double deltaStart = time - start;
		//Only set the overflow flag when not masked
		if ( deltaStart >= 0 && !masked ) {
			overflow = 1;
		}
	}
	//On a reset make sure the start is in sync with the next cycle
	void Reset(const double& time ) {
		overflow = false;
		if ( !delay || !enabled )
			return;
		double delta = (time - start);
		double rem = fmod( delta, delay );
		double next = delay - rem;
		start = time + next;		
	}
	void Stop( ) {
		enabled = false;
	}
	void Start( const double& time, Bits scale ) {
		//Don't enable again
		if ( enabled ) {
			return;
		}
		enabled = true;
		delay = 0.001 * (256 - counter ) * scale;
		start = time + delay;
	}

};

struct Chip {
	//Last selected register
	Timer timer[2];
	//Check for it being a write to the timer
	bool Write( Bit32u addr, Bit8u val );
	//Read the current timer state, will use current double
	Bit8u Read( );
};

//The type of handler this is
typedef enum {
	MODE_OPL2,
	MODE_DUALOPL2,
	MODE_OPL3
} Mode;

/*class Handler {
public:
	//Write an address to a chip, returns the address the chip sets
	virtual Bit32u WriteAddr( Bit32u port, Bit8u val ) = 0;
	//Write to a specific register in the chip
	virtual void WriteReg( Bit32u addr, Bit8u val ) = 0;
	//Generate a certain amount of samples
	virtual void Generate( MixerChannel* chan, Bitu samples ) = 0;
	//Initialize at a specific sample rate and mode
	virtual void Init( Bitu rate ) = 0;
	virtual ~Handler() {
	}
};*/

//The cache for 2 chips or an opl3
typedef Bit8u RegisterCache[512];

//Internal class used for dro capturing
class Capture;

/*class Module: public Module_base {
	IO_ReadHandleObject ReadHandler[3];
	IO_WriteHandleObject WriteHandler[3];
	MixerObject mixerObject;

	//Mode we're running in
	Mode mode;
	//Last selected address in the chip for the different modes
	union {
		Bit32u normal;
		Bit8u dual[2];
	} reg;
	void CacheWrite( Bit32u reg, Bit8u val );
	void DualWrite( Bit8u index, Bit8u reg, Bit8u val );
public:
	static OPL_Mode oplmode;
	MixerChannel* mixerChan;
	Bit32u lastUsed;				//Ticks when adlib was last used to turn of mixing after a few second

	Handler* handler;				//Handler that will generate the sound
	RegisterCache cache;
	Capture* capture;
	Chip	chip[2];

	//Handle port writes
	void PortWrite( Bitu port, Bitu val, Bitu iolen );
	Bitu PortRead( Bitu port, Bitu iolen );
	void Init( Mode m );

	Module( Section* configuration); 
	~Module();
};*/


}		//Adlib namespace

#endif
