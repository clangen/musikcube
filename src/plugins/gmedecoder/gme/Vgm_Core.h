// Sega VGM music file emulator core

// Game_Music_Emu $vers
#ifndef VGM_CORE_H
#define VGM_CORE_H

#include "Gme_Loader.h"
#include "Ymz280b_Emu.h"
#include "Ymf262_Emu.h"
#include "Ym2612_Emu.h"
#include "Ym2610b_Emu.h"
#include "Ym2608_Emu.h"
#include "Ym3812_Emu.h"
#include "Ym2413_Emu.h"
#include "Ym2151_Emu.h"
#include "C140_Emu.h"
#include "SegaPcm_Emu.h"
#include "Rf5C68_Emu.h"
#include "Rf5C164_Emu.h"
#include "Pwm_Emu.h"
#include "Okim6258_Emu.h"
#include "Okim6295_Emu.h"
#include "K051649_Emu.h"
#include "K053260_Emu.h"
#include "K054539_Emu.h"
#include "Qsound_Apu.h"
#include "Ym2203_Emu.h"
#include "Ay_Apu.h"
#include "Gb_Apu.h"
#include "Hes_Apu.h"
#include "Sms_Apu.h"
#include "Multi_Buffer.h"
#include "Chip_Resampler.h"

	template<class Emu>
	class Chip_Emu : public Emu {
		int last_time;
		short* out;
		enum { disabled_time = -1 };
	public:
		Chip_Emu()                      { last_time = disabled_time; out = NULL; }
		void enable( bool b = true )    { last_time = b ? 0 : disabled_time; }
		bool enabled() const            { return last_time != disabled_time; }
		void begin_frame( short* buf )  { out = buf; last_time = 0; }
		
		int run_until( int time )
		{
			int count = time - last_time;
			if ( count > 0 )
			{
				if ( last_time < 0 )
					return false;
				last_time = time;
				short* p = out;
				out += count * Emu::out_chan_count;
				Emu::run( count, p );
			}
			return true;
		}
	};

class Vgm_Core : public Gme_Loader {
public:

	// VGM file header
	struct header_t
	{
		enum { size_min = 0x40 };
		enum { size_151 = 0x80 };
		enum { size_max = 0xC0 };
		
		char tag            [4]; // 0x00
		byte data_size      [4]; // 0x04
		byte version        [4]; // 0x08
		byte psg_rate       [4]; // 0x0C
		byte ym2413_rate    [4]; // 0x10
		byte gd3_offset     [4]; // 0x14
		byte track_duration [4]; // 0x18
		byte loop_offset    [4]; // 0x1C
		byte loop_duration  [4]; // 0x20
		byte frame_rate     [4]; // 0x24 v1.01 V
		byte noise_feedback [2]; // 0x28 v1.10 V
		byte noise_width;        // 0x2A
		byte sn76489_flags;      // 0x2B v1.51 <
		byte ym2612_rate    [4]; // 0x2C v1.10 V
		byte ym2151_rate    [4]; // 0x30
		byte data_offset    [4]; // 0x34 v1.50 V
		byte segapcm_rate   [4]; // 0x38 v1.51 V
		byte segapcm_reg    [4]; // 0x3C
		byte rf5c68_rate    [4]; // 0x40
		byte ym2203_rate    [4]; // 0x44
		byte ym2608_rate    [4]; // 0x48
		byte ym2610_rate    [4]; // 0x4C
		byte ym3812_rate    [4]; // 0x50
		byte ym3526_rate    [4]; // 0x54
		byte y8950_rate     [4]; // 0x58
		byte ymf262_rate    [4]; // 0x5C
		byte ymf278b_rate   [4]; // 0x60
		byte ymf271_rate    [4]; // 0x64
		byte ymz280b_rate   [4]; // 0x68
		byte rf5c164_rate   [4]; // 0x6C
		byte pwm_rate       [4]; // 0x70
		byte ay8910_rate    [4]; // 0x74
		byte ay8910_type;        // 0x78
		byte ay8910_flags;       // 0x79
		byte ym2203_ay8910_flags;// 0x7A
		byte ym2608_ay8910_flags;// 0x7B
		byte volume_modifier;    // 0x7C v1.60 V
		byte reserved;           // 0x7D
		byte loop_base;          // 0x7E
		byte loop_modifier;      // 0x7F v1.51 <
		byte gbdmg_rate     [4]; // 0x80 v1.61 V
		byte nesapu_rate    [4]; // 0x84
		byte multipcm_rate  [4]; // 0x88
		byte upd7759_rate   [4]; // 0x8C
		byte okim6258_rate  [4]; // 0x90
		byte okim6258_flags;     // 0x94
		byte k054539_flags;      // 0x95
		byte c140_type;          // 0x96
		byte reserved_flags;     // 0x97
		byte okim6295_rate  [4]; // 0x98
		byte k051649_rate   [4]; // 0x9C
		byte k054539_rate   [4]; // 0xA0
		byte huc6280_rate   [4]; // 0xA4
		byte c140_rate      [4]; // 0xA8
		byte k053260_rate   [4]; // 0xAC
		byte pokey_rate     [4]; // 0xB0
		byte qsound_rate    [4]; // 0xB4
		byte reserved2      [4]; // 0xB8
		byte extra_offset   [4]; // 0xBC
		
		// True if header has valid file signature
		bool valid_tag() const;
		int size() const;
		void cleanup();
	};
	
	// Header for currently loaded file
	header_t const& header() const      { return _header; }
	
	// Raw file data, for parsing GD3 tags
	byte const* file_begin() const      { return Gme_Loader::file_begin(); }
	byte const* file_end  () const      { return Gme_Loader::file_end(); }
	
	// If file uses FM, initializes FM sound emulator using *sample_rate. If
	// *sample_rate is zero, sets *sample_rate to the proper accurate rate and
	// uses that. The output of the FM sound emulator is resampled to the
	// final sampling rate.
	blargg_err_t init_chips( double* fm_rate, bool reinit = false );
	
	// True if any FM chips are used by file. Always false until init_fm()
	// is called.
	bool uses_fm() const                { return ym2612[0].enabled() || ym2413[0].enabled() || ym2151[0].enabled() || c140.enabled() ||
		segapcm.enabled() || rf5c68.enabled() || rf5c164.enabled() || pwm.enabled() || okim6258[0].enabled() || okim6295[0].enabled() ||
		k051649.enabled() || k053260.enabled() || k054539.enabled() || ym2203[0].enabled() || ym3812[0].enabled() || ymf262[0].enabled() ||
        ymz280b.enabled() || ym2610[0].enabled() || ym2608[0].enabled() || qsound[0].enabled() ||
        (header().ay8910_rate[0] | header().ay8910_rate[1] | header().ay8910_rate[2] | header().ay8910_rate[3]) ||
        (header().huc6280_rate[0] | header().huc6280_rate[1] | header().huc6280_rate[2] | header().huc6280_rate[3]) ||
		(header().gbdmg_rate[0] | header().gbdmg_rate[1] | header().gbdmg_rate[2] | header().gbdmg_rate[3]); }
	
	// Adjusts music tempo, where 1.0 is normal. Can be changed while playing.
	// Loading a file resets tempo to 1.0.
	void set_tempo( double );

	void set_sample_rate( int r ) { sample_rate = r; }
	
	// Starts track
	void start_track();
	
	// Runs PSG-only VGM for msec and returns number of clocks it ran for
	blip_time_t run_psg( int msec );
	
	// Plays FM for at most count samples into *out, and returns number of
	// samples actually generated (always even). Also runs PSG for blip_time.
	int play_frame( blip_time_t blip_time, int count, blip_sample_t out [] );
	
	// True if all of file data has been played
	bool track_ended() const            { return pos >= file_end(); }
	
    // 0 for PSG and YM2612 DAC, 1 for AY, 2 for HuC6280, 3 for GB DMG
    Stereo_Buffer stereo_buf[4];

    // PCM sound is always generated here
    Blip_Buffer * blip_buf[2];
	
	// PSG sound chips, for assigning to Blip_Buffer, and setting volume and EQ
	Sms_Apu psg[2];
	Ay_Apu ay[2];
    Hes_Apu huc6280[2];
	Gb_Apu gbdmg[2];
	
	// PCM synth, for setting volume and EQ
	Blip_Synth_Fast pcm;
	
	// FM sound chips
	Chip_Resampler_Emu<Ymf262_Emu> ymf262[2];
	Chip_Resampler_Emu<Ym3812_Emu> ym3812[2];
	Chip_Resampler_Emu<Ym2612_Emu> ym2612[2];
	Chip_Resampler_Emu<Ym2610b_Emu> ym2610[2];
	Chip_Resampler_Emu<Ym2608_Emu> ym2608[2];
	Chip_Resampler_Emu<Ym2413_Emu> ym2413[2];
	Chip_Resampler_Emu<Ym2151_Emu> ym2151[2];
	Chip_Resampler_Emu<Ym2203_Emu> ym2203[2];

	// PCM sound chips
	Chip_Resampler_Emu<C140_Emu> c140;
	Chip_Resampler_Emu<SegaPcm_Emu> segapcm;
	Chip_Resampler_Emu<Rf5C68_Emu> rf5c68;
	Chip_Resampler_Emu<Rf5C164_Emu> rf5c164;
	Chip_Resampler_Emu<Pwm_Emu> pwm;
	Chip_Resampler_Emu<Okim6258_Emu> okim6258[2]; int okim6258_hz[2];
	Chip_Resampler_Emu<Okim6295_Emu> okim6295[2]; int okim6295_hz;
	Chip_Resampler_Emu<K051649_Emu> k051649;
	Chip_Resampler_Emu<K053260_Emu> k053260;
	Chip_Resampler_Emu<K054539_Emu> k054539;
	Chip_Resampler_Emu<Ymz280b_Emu> ymz280b; int ymz280b_hz;
    Chip_Resampler_Emu<Qsound_Apu> qsound[2];

	// DAC control
	typedef struct daccontrol_data
	{
		bool Enable;
		byte Bank;
	} DACCTRL_DATA;

	byte DacCtrlUsed;
	byte DacCtrlUsg[0xFF];
	DACCTRL_DATA DacCtrl[0xFF];
	byte DacCtrlMap[0xFF];
	int DacCtrlTime[0xFF];
	void ** dac_control;

	void dac_control_grow(byte chip_id);

	int dac_control_recursion;

	int run_dac_control( int time );

public:
	void chip_reg_write(unsigned Sample, byte ChipType, byte ChipID, byte Port, byte Offset, byte Data);

// Implementation
public:
	Vgm_Core();
	~Vgm_Core();

protected:
	virtual blargg_err_t load_mem_( byte const [], int );
	
private:
	//          blip_time_t // PSG clocks
	typedef int vgm_time_t; // 44100 per second, REGARDLESS of sample rate
	typedef int fm_time_t;  // FM sample count
	
	int sample_rate;
	int vgm_rate;   // rate of log, 44100 normally, adjusted by tempo
	double fm_rate; // FM samples per second

	header_t _header;
	
	// VGM to FM time
	int fm_time_factor;     
	int fm_time_offset;
	fm_time_t to_fm_time( vgm_time_t ) const;

	// VGM to PSG time
	int blip_time_factor;
	blip_time_t to_psg_time( vgm_time_t ) const;

	int blip_ay_time_factor;
	int ay_time_offset;
	blip_time_t to_ay_time( vgm_time_t ) const;

    int blip_huc6280_time_factor;
    int huc6280_time_offset;
    blip_time_t to_huc6280_time( vgm_time_t ) const;

	int blip_gbdmg_time_factor;
	int gbdmg_time_offset;
	blip_time_t to_gbdmg_time( vgm_time_t ) const;
	
	// Current time and position in log
	vgm_time_t vgm_time;
	byte const* pos;
	byte const* loop_begin;
	bool has_looped;
	
	// PCM
	enum { PCM_BANK_COUNT = 0x40 };
	typedef struct _vgm_pcm_bank_data
	{
		unsigned DataSize;
		byte* Data;
		unsigned DataStart;
	} VGM_PCM_DATA;
	typedef struct _vgm_pcm_bank
	{
		unsigned BankCount;
		VGM_PCM_DATA* Bank;
		unsigned DataSize;
		byte* Data;
		unsigned DataPos;
		unsigned BnkPos;
	} VGM_PCM_BANK;

	typedef struct pcmbank_table
	{
		byte ComprType;
		byte CmpSubType;
		byte BitDec;
		byte BitCmp;
		unsigned EntryCount;
		void* Entries;
	} PCMBANK_TBL;

	VGM_PCM_BANK PCMBank[PCM_BANK_COUNT];
	PCMBANK_TBL PCMTbl;

	void ReadPCMTable(unsigned DataSize, const byte* Data);
	void AddPCMData(byte Type, unsigned DataSize, const byte* Data);
	bool DecompressDataBlk(VGM_PCM_DATA* Bank, unsigned DataSize, const byte* Data);
	const byte* GetPointerFromPCMBank(byte Type, unsigned DataPos);

	byte const* pcm_pos;    // current position in PCM data
	int dac_amp[2];
	int dac_disabled[2];       // -1 if disabled
	void write_pcm( vgm_time_t, int chip, int amp );
	
	blip_time_t run( vgm_time_t );
	int run_ym2151( int chip, int time );
	int run_ym2203( int chip, int time );
	int run_ym2413( int chip, int time );
	int run_ym2612( int chip, int time );
	int run_ym3812( int chip, int time );
	int run_ymf262( int chip, int time );
	int run_ym2610( int chip, int time );
	int run_ym2608( int chip, int time );
	int run_ymz280b( int time );
	int run_c140( int time );
	int run_segapcm( int time );
	int run_rf5c68( int time );
	int run_rf5c164( int time );
	int run_pwm( int time );
	int run_okim6258( int chip, int time );
	int run_okim6295( int chip, int time );
	int run_k051649( int time );
	int run_k053260( int time );
	int run_k054539( int time );
    int run_qsound( int chip, int time );
	void update_fm_rates( int* ym2151_rate, int* ym2413_rate, int* ym2612_rate ) const;
};

#endif
