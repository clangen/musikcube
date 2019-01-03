// Sega Master System SN76489 PSG sound chip emulator

// Sms_Snd_Emu $vers
#ifndef SMS_APU_H
#define SMS_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

struct sms_apu_state_t;

class Sms_Apu {
public:
// Basics

	// Sets buffer(s) to generate sound into, or 0 to mute. If only center is not 0,
	// output is mono.
	void set_output( Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL );

	// Emulates to time t, then writes data to Game Gear left/right assignment byte
	void write_ggstereo( blip_time_t t, int data );
	
	// Emulates to time t, then writes data
	void write_data( blip_time_t t, int data );
	
	// Emulates to time t, then subtracts t from the current time.
	// OK if previous write call had time slightly after t.
	void end_frame( blip_time_t t );

// More features

	// Resets sound chip and sets noise feedback bits and width
	void reset( unsigned noise_feedback = 0, int noise_width = 0 );
	
	// Same as set_output(), but for a particular channel
	// 0: Square 1, 1: Square 2, 2: Square 3, 3: Noise
	enum { osc_count = 4 }; // 0 <= chan < osc_count
	void set_output( int chan, Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL );
	
	// Sets overall volume, where 1.0 is normal
	void volume( double );
	
	// Sets treble equalization
	void treble_eq( blip_eq_t const& );
	
	// Saves full emulation state to state_out. Data format is portable and
	// includes some extra space to avoid expansion in case more state needs
	// to be stored in the future.
	void save_state( sms_apu_state_t* state_out );
	
	// Loads state. You should call reset() BEFORE this.
	blargg_err_t load_state( sms_apu_state_t const& in );

private:
	// noncopyable
	Sms_Apu( const Sms_Apu& );
	Sms_Apu& operator = ( const Sms_Apu& );

// Implementation
public:
	Sms_Apu();
	~Sms_Apu() { }
	BLARGG_DISABLE_NOTHROW
	
	// Use set_output() instead
	BLARGG_DEPRECATED( void output    (        Blip_Buffer* c                                 ) { set_output( c, c, c    ); } )
	BLARGG_DEPRECATED( void output    (        Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r ) { set_output( c, l, r    ); } )
	BLARGG_DEPRECATED( void osc_output( int i, Blip_Buffer* c                                 ) { set_output( i, c, c, c ); } )
	BLARGG_DEPRECATED( void osc_output( int i, Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r ) { set_output( i, c, l, r ); } )

private:
	struct Osc
	{
		Blip_Buffer* outputs [4]; // NULL, right, left, center
		Blip_Buffer* output;
		int          last_amp;
		
		int         volume;
		int         period;
		int         delay;
		unsigned    phase;
	};
	
	Osc     oscs [osc_count];
	int     ggstereo;
	int     latch;
	
	blip_time_t last_time;
	int         min_tone_period;
	unsigned    noise_feedback;
	unsigned    looped_feedback;
	Blip_Synth_Fast fast_synth;
	Blip_Synth_Norm norm_synth;
	
	int calc_output( int i ) const;
	void run_until( blip_time_t );
	const char* save_load( sms_apu_state_t*, bool save );
	friend class Sms_Apu_Tester;
};

struct sms_apu_state_t
{
	// If SMS_APU_CUSTOM_STATE is 1, values are stored as normal integers,
	// so your code can then save and load them however it likes. Otherwise,
	// they are 4-byte arrays in little-endian format, making entire
	// structure suitable for direct storage on disk.
	
#if SMS_APU_CUSTOM_STATE
	typedef int val_t;
#else
	typedef unsigned char val_t [4];
#endif
	
	enum { format0 = 0x50414D53 };
	
	val_t format;
	val_t version;
	val_t latch;
	val_t ggstereo;
	val_t periods [4];
	val_t volumes [4];
	val_t delays  [4];
	val_t phases  [4];
	
	val_t unused  [12]; // for future expansion
};

#endif
