/*
    DeaDBeeF - The Ultimate Music Player
    Copyright (C) 2009-2013 Alexey Yakovenko <waker@users.sourceforge.net>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
#ifndef __EQU_H
#define __EQU_H

/// @file Equ.h
/// @brief C interface to the SuperEQ FFT-based equalizer engine.
/// @details Vendored from the DeaDBeeF player. Provides the SuperEqState
/// structure and the routines to initialize the equalizer, rebuild its filter
/// table from a parameter list, and process both integer and float PCM.

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Floating-point sample type used by the equalizer. */
typedef float REAL;

/** @brief Processing state of the SuperEQ equalizer.
 *  @details Holds all internal FFT buffers, the current/previous filter
 *  tables, and the configured FFT window size. A single state is used for all
 *  channels. */
typedef struct {
    /** @brief Current, previous and double-buffered filter impulse responses. */
    REAL *lires,*lires1,*lires2;
    /** @brief Impulse response workspace. */
    REAL *irest;
    /** @brief Scratch buffer for filtered samples. */
    REAL *fsamples;
    /** @brief Dither noise buffer. */
    REAL *ditherbuf;
    /** @brief Dither buffer cursor. */
    int ditherptr;
    /** @brief Pending and current impulse response indices. */
    volatile int chg_ires,cur_ires;
    /** @brief FFT window length, its log2, table size and buffered samples. */
    int winlen,winlenbit,tabsize,nbufsamples;
    /** @brief Input buffer for the FFT. */
    REAL *finbuf;
    /** @brief Output buffer of the FFT. */
    REAL *outbuf;
    /** @brief Whether dithering is enabled. */
    int dither;
    /** @brief Number of interleaved channels. */
    int channels;
    /** @brief Whether the equalizer is enabled. */
    int enable;
    /** @brief FFT size in bits. */
    int fft_bits;
} SuperEqState;

/** @brief Allocates a new parameter list.
 *  @return An opaque parameter list handle. */
void *paramlist_alloc (void);
/** @brief Frees a parameter list.
 *  @param param The parameter list handle. */
void paramlist_free (void *);
/** @brief Rebuilds the equalizer filter table from a parameter list.
 *  @param state The equalizer state.
 *  @param lbc Band control gains.
 *  @param param The parameter list handle.
 *  @param fs Sample rate in Hz. */
void equ_makeTable(SuperEqState *state, float *lbc,void *param,float fs);
/** @brief Processes an integer PCM buffer through the equalizer.
 *  @param state The equalizer state.
 *  @param buf Interleaved sample buffer.
 *  @param nsamples Number of samples to process.
 *  @param nch Number of channels.
 *  @param bps Bytes per sample.
 *  @return Non-zero on success. */
int equ_modifySamples(SuperEqState *state, char *buf,int nsamples,int nch,int bps);
/** @brief Processes a float PCM buffer through the equalizer.
 *  @param state The equalizer state.
 *  @param buf Interleaved float sample buffer.
 *  @param nsamples Number of samples to process.
 *  @param nch Number of channels.
 *  @return Non-zero on success. */
int equ_modifySamples_float (SuperEqState *state, char *buf,int nsamples,int nch);
/** @brief Clears the equalizer's internal buffers.
 *  @param state The equalizer state. */
void equ_clearbuf(SuperEqState *state);
/** @brief Initializes the equalizer state.
 *  @param state The equalizer state.
 *  @param wb FFT window size in bits.
 *  @param channels Number of channels. */
void equ_init(SuperEqState *state, int wb, int channels);
/** @brief Frees all equalizer buffers.
 *  @param state The equalizer state. */
void equ_quit(SuperEqState *state);

#ifdef __cplusplus
}
#endif

#endif
