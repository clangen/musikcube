// Super Nintendo SFM music file emulator

// Game_Music_Emu $vers
#ifndef SPC_SFM_H
#define SPC_SFM_H

#include "Music_Emu.h"
#include "higan/smp/smp.hpp"
#include "Spc_Filter.h"

#include "Bml_Parser.h"

#if GME_SPC_FAST_RESAMPLER
    #include "Upsampler.h"
    typedef Upsampler Spc_Emu_Resampler;
#else
    #include "Fir_Resampler.h"
    typedef Fir_Resampler<24> Spc_Emu_Resampler;
#endif

class Sfm_Emu : public Music_Emu {
public:
    // Minimum allowed file size
    enum { sfm_min_file_size = 8 + 65536 + 128 };

    // The Super Nintendo hardware samples at 32kHz. Other sample rates are
    // handled by resampling the 32kHz output; emulation accuracy is not affected.
    enum { native_sample_rate = 32000 };
    
    // This will serialize the current state of the emulator into a new SFM file
    blargg_err_t serialize( std::vector<uint8_t> & out );

    // Disables annoying pseudo-surround effect some music uses
    void disable_surround( bool disable = true )    { smp.dsp.disable_surround( disable ); }

    // Enables gaussian, cubic or sinc interpolation
    void interpolation_level( int level = 0 )   { smp.dsp.spc_dsp.interpolation_level( level ); }

    SuperFamicom::SMP const* get_smp() const;
    SuperFamicom::SMP * get_smp();

    blargg_err_t hash_( Hash_Function& ) const;

    static gme_type_t static_type()                 { return gme_sfm_type; }
    
// Implementation
public:
    Sfm_Emu();
    ~Sfm_Emu();

protected:
    virtual blargg_err_t load_mem_( byte const [], int );
    virtual blargg_err_t track_info_( track_info_t*, int track ) const;
    virtual blargg_err_t set_track_info_( const track_info_t*, int track );
    virtual blargg_err_t set_sample_rate_( int );
    virtual blargg_err_t start_track_( int );
    virtual blargg_err_t play_( int, sample_t [] );
    virtual blargg_err_t skip_( int );
    virtual void mute_voices_( int );
    virtual void set_tempo_( double );
    virtual blargg_err_t save_( gme_writer_t, void* ) const;

private:
    Spc_Emu_Resampler resampler;
    Spc_Filter filter;
    SuperFamicom::SMP smp;

    Bml_Parser metadata;
    void create_updated_metadata(Bml_Parser &out) const;

    blargg_err_t play_and_filter( int count, sample_t out [] );
};

inline SuperFamicom::SMP const* Sfm_Emu::get_smp() const { return &smp; }
inline SuperFamicom::SMP * Sfm_Emu::get_smp() { return &smp; }

#endif // SPC_SFM_H
