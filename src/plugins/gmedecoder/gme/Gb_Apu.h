// Nintendo Game Boy sound hardware emulator with save state support

// Gb_Snd_Emu $vers
#ifndef GB_APU_H
#define GB_APU_H

#include "Gb_Oscs.h"

struct gb_apu_state_t;

class Gb_Apu {
public:
// Basics

	// Sets buffer(s) to generate sound into, or NULL to mute. If only center is not NULL,
	// output is mono.
	void set_output( Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL );
	
	// Emulates to time t, then writes data to addr
	void write_register( blip_time_t t, int addr, int data );
	
	// Emulates to time t, then subtracts t from the current time.
	// OK if previous write call had time slightly after t.
	void end_frame( blip_time_t t );
	
// More features
	
	// Clock rate sound hardware runs at
	enum { clock_rate = 4194304 * GB_APU_OVERCLOCK };
	
	// Registers are at io_addr to io_addr+io_size-1
	enum { io_addr = 0xFF10 };
	enum { io_size = 0x30 };
	
	// Emulates to time t, then reads from addr
	int read_register( blip_time_t t, int addr );
	
	// Resets hardware to state after power, BEFORE boot ROM runs. Mode selects
	// sound hardware. If agb_wave is true, enables AGB's extra wave features.
	enum mode_t {
		mode_dmg,   // Game Boy monochrome
		mode_cgb,   // Game Boy Color
		mode_agb    // Game Boy Advance
	};
	void reset( mode_t mode = mode_cgb, bool agb_wave = false );
	
	// Same as set_output(), but for a particular channel
	// 0: Square 1, 1: Square 2, 2: Wave, 3: Noise
	enum { osc_count = 4 }; // 0 <= chan < osc_count
	void set_output( int chan, Blip_Buffer* center,
			Blip_Buffer* left = NULL, Blip_Buffer* right = NULL );
	
	// Sets overall volume, where 1.0 is normal
	void volume( double );
	
	// Sets treble equalization
	void treble_eq( blip_eq_t const& );
	
	// Treble and bass values for various hardware.
	enum {
		speaker_treble =  -47, // speaker on system
		speaker_bass   = 2000,
		dmg_treble     =    0, // headphones on each system
		dmg_bass       =   30,
		cgb_treble     =    0,
		cgb_bass       =  300, // CGB has much less bass
		agb_treble     =    0,
		agb_bass       =   30
	};
	
	// If true, reduces clicking by disabling DAC biasing. Note that this reduces
	// emulation accuracy, since the clicks are authentic.
	void reduce_clicks( bool reduce = true );
	
	// Sets frame sequencer rate, where 1.0 is normal. Meant for adjusting the
	// tempo in a music player.
	void set_tempo( double );
	
	// Saves full emulation state to state_out. Data format is portable and
	// includes some extra space to avoid expansion in case more state needs
	// to be stored in the future.
	void save_state( gb_apu_state_t* state_out );
	
	// Loads state. You should call reset() BEFORE this.
	blargg_err_t load_state( gb_apu_state_t const& in );

private:
	// noncopyable
	Gb_Apu( const Gb_Apu& );
	Gb_Apu& operator = ( const Gb_Apu& );

// Implementation
public:
	Gb_Apu();
	
	// Use set_output() in place of these
	BLARGG_DEPRECATED( void output    (        Blip_Buffer* c                                 ); )
	BLARGG_DEPRECATED( void output    (        Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r ); )
	BLARGG_DEPRECATED( void osc_output( int i, Blip_Buffer* c                                 ) { set_output( i, c, c, c ); } )
	BLARGG_DEPRECATED( void osc_output( int i, Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r ) { set_output( i, c, l, r ); } )
	
	BLARGG_DEPRECATED_TEXT( enum { start_addr = 0xFF10 }; )
	BLARGG_DEPRECATED_TEXT( enum { end_addr   = 0xFF3F }; )
	BLARGG_DEPRECATED_TEXT( enum { register_count = end_addr - start_addr + 1 }; )

private:    
	Gb_Osc*     oscs [osc_count];
	blip_time_t last_time;          // time sound emulator has been run to
	blip_time_t frame_period;       // clocks between each frame sequencer step
	double      volume_;
	bool        reduce_clicks_;
	
	Gb_Sweep_Square square1;
	Gb_Square       square2;
	Gb_Wave         wave;
	Gb_Noise        noise;
	blip_time_t     frame_time;     // time of next frame sequencer action
	int             frame_phase;    // phase of next frame sequencer step
	enum { regs_size = io_size + 0x10 };
	BOOST::uint8_t  regs [regs_size];// last values written to registers
	
	// large objects after everything else
	Blip_Synth_Norm norm_synth;
	Blip_Synth_Fast fast_synth;
	
	void reset_lengths();
	void reset_regs();
	int calc_output( int osc ) const;
	void apply_stereo();
	void apply_volume();
	void synth_volume( int );
	void run_until_( blip_time_t );
	void run_until( blip_time_t );
	void silence_osc( Gb_Osc& );
	void write_osc( int reg, int old_data, int data );
	const char* save_load( gb_apu_state_t*, bool save );
	void save_load2( gb_apu_state_t*, bool save );
	friend class Gb_Apu2;
};

// Format of save state. Should be stable across versions of the library,
// with earlier versions properly opening later save states. Includes some
// room for expansion so the state size shouldn't increase.
struct gb_apu_state_t
{
#if GB_APU_CUSTOM_STATE
	// Values stored as plain int so your code can read/write them easily.
	// Structure can NOT be written to disk, since format is not portable.
	typedef int val_t;
#else
	// Values written in portable little-endian format, allowing structure
	// to be written directly to disk.
	typedef unsigned char val_t [4];
#endif
	
	enum { format0 = 0x50414247 }; // 'GBAP'
	
	val_t format;   // format of all following data
	val_t version;  // later versions just add fields to end
	
	unsigned char regs [0x40];
	val_t frame_time;
	val_t frame_phase;
	
	val_t sweep_freq;
	val_t sweep_delay;
	val_t sweep_enabled;
	val_t sweep_neg;
	val_t noise_divider;
	val_t wave_buf;
	
	val_t delay      [4];
	val_t length_ctr [4];
	val_t phase      [4];
	val_t enabled    [4];
	
	val_t env_delay   [3];
	val_t env_volume  [3];
	val_t env_enabled [3];
	
	val_t unused  [13]; // for future expansion
};

inline void Gb_Apu::set_output( Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	for ( int i = osc_count; --i >= 0; )
		set_output( i, c, l, r );
}

BLARGG_DEPRECATED_TEXT( inline void Gb_Apu::output( Blip_Buffer* c                                 ) { set_output(    c, c, c ); } )
BLARGG_DEPRECATED_TEXT( inline void Gb_Apu::output( Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r ) { set_output(    c, l, r ); } )

#endif
