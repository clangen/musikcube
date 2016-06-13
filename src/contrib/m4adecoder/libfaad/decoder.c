/*
** FAAD2 - Freeware Advanced Audio (AAC) Decoder including SBR decoding
** Copyright (C) 2003-2005 M. Bakker, Nero AG, http://www.nero.com
**  
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software 
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**
** Any non-GPL usage of this software or parts of this software is strictly
** forbidden.
**
** The "appropriate copyright message" mentioned in section 2c of the GPLv2
** must read: "Code from FAAD2 is copyright (c) Nero AG, www.nero.com"
**
** Commercial non-GPL licensing of this software is possible.
** For more info contact Nero AG through Mpeg4AAClicense@nero.com.
**
** $Id: decoder.c,v 1.117 2009/02/05 00:51:03 menno Exp $
**/

#include "common.h"
#include "structs.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mp4.h"
#include "syntax.h"
#include "error.h"
#include "output.h"
#include "filtbank.h"
#include "drc.h"
#ifdef SBR_DEC
#include "sbr_dec.h"
#include "sbr_syntax.h"
#endif
#ifdef SSR_DEC
#include "ssr.h"
#endif

#ifdef ANALYSIS
uint16_t dbg_count;
#endif

/* static function declarations */
static void* aac_frame_decode(NeAACDecStruct *hDecoder,
                              NeAACDecFrameInfo *hInfo,
                              unsigned char *buffer,
                              unsigned long buffer_size,
                              void **sample_buffer2,
                              unsigned long sample_buffer_size);
static void create_channel_config(NeAACDecStruct *hDecoder,
                                  NeAACDecFrameInfo *hInfo);


char* NEAACDECAPI NeAACDecGetErrorMessage(unsigned char errcode)
{
    if (errcode >= NUM_ERROR_MESSAGES)
        return NULL;
    return err_msg[errcode];
}

unsigned long NEAACDECAPI NeAACDecGetCapabilities(void)
{
    uint32_t cap = 0;

    /* can't do without it */
    cap += LC_DEC_CAP;

#ifdef MAIN_DEC
    cap += MAIN_DEC_CAP;
#endif
#ifdef LTP_DEC
    cap += LTP_DEC_CAP;
#endif
#ifdef LD_DEC
    cap += LD_DEC_CAP;
#endif
#ifdef ERROR_RESILIENCE
    cap += ERROR_RESILIENCE_CAP;
#endif
#ifdef FIXED_POINT
    cap += FIXED_POINT_CAP;
#endif

    return cap;
}

const unsigned char mes[] = { 0x67,0x20,0x61,0x20,0x20,0x20,0x6f,0x20,0x72,0x20,0x65,0x20,0x6e,0x20,0x20,0x20,0x74,0x20,0x68,0x20,0x67,0x20,0x69,0x20,0x72,0x20,0x79,0x20,0x70,0x20,0x6f,0x20,0x63 };
NeAACDecHandle NEAACDECAPI NeAACDecOpen(void)
{
    uint8_t i;
    NeAACDecStruct *hDecoder = NULL;

    if ((hDecoder = (NeAACDecStruct*)faad_malloc(sizeof(NeAACDecStruct))) == NULL)
        return NULL;

    memset(hDecoder, 0, sizeof(NeAACDecStruct));

    hDecoder->cmes = mes;
    hDecoder->config.outputFormat  = FAAD_FMT_16BIT;
    hDecoder->config.defObjectType = MAIN;
    hDecoder->config.defSampleRate = 44100; /* Default: 44.1kHz */
    hDecoder->config.downMatrix = 0;
    hDecoder->adts_header_present = 0;
    hDecoder->adif_header_present = 0;
	hDecoder->latm_header_present = 0;
#ifdef ERROR_RESILIENCE
    hDecoder->aacSectionDataResilienceFlag = 0;
    hDecoder->aacScalefactorDataResilienceFlag = 0;
    hDecoder->aacSpectralDataResilienceFlag = 0;
#endif
    hDecoder->frameLength = 1024;

    hDecoder->frame = 0;
    hDecoder->sample_buffer = NULL;

    hDecoder->__r1 = 1;
    hDecoder->__r2 = 1;

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        hDecoder->window_shape_prev[i] = 0;
        hDecoder->time_out[i] = NULL;
        hDecoder->fb_intermed[i] = NULL;
#ifdef SSR_DEC
        hDecoder->ssr_overlap[i] = NULL;
        hDecoder->prev_fmd[i] = NULL;
#endif
#ifdef MAIN_DEC
        hDecoder->pred_stat[i] = NULL;
#endif
#ifdef LTP_DEC
        hDecoder->ltp_lag[i] = 0;
        hDecoder->lt_pred_stat[i] = NULL;
#endif
    }

#ifdef SBR_DEC
    for (i = 0; i < MAX_SYNTAX_ELEMENTS; i++)
    {
        hDecoder->sbr[i] = NULL;
    }
#endif

    hDecoder->drc = drc_init(REAL_CONST(1.0), REAL_CONST(1.0));

    return hDecoder;
}

NeAACDecConfigurationPtr NEAACDECAPI NeAACDecGetCurrentConfiguration(NeAACDecHandle hpDecoder)
{
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if (hDecoder)
    {
        NeAACDecConfigurationPtr config = &(hDecoder->config);

        return config;
    }

    return NULL;
}

unsigned char NEAACDECAPI NeAACDecSetConfiguration(NeAACDecHandle hpDecoder,
                                                   NeAACDecConfigurationPtr config)
{
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if (hDecoder && config)
    {
        /* check if we can decode this object type */
        if (can_decode_ot(config->defObjectType) < 0)
            return 0;
        hDecoder->config.defObjectType = config->defObjectType;

        /* samplerate: anything but 0 should be possible */
        if (config->defSampleRate == 0)
            return 0;
        hDecoder->config.defSampleRate = config->defSampleRate;

        /* check output format */
#ifdef FIXED_POINT
        if ((config->outputFormat < 1) || (config->outputFormat > 4))
            return 0;
#else
        if ((config->outputFormat < 1) || (config->outputFormat > 5))
            return 0;
#endif
        hDecoder->config.outputFormat = config->outputFormat;

        if (config->downMatrix > 1)
            return 0;
        hDecoder->config.downMatrix = config->downMatrix;

        /* OK */
        return 1;
    }

    return 0;
}


static int latmCheck(latm_header *latm, bitfile *ld)
{
    uint32_t good=0, bad=0, bits, m;

    while (ld->bytes_left)
    {
        bits = faad_latm_frame(latm, ld);
        if(bits==-1U)
            bad++;
        else
        {
            good++;
            while(bits>0)
            {
                m = min(bits, 8);
                faad_getbits(ld, m);
                bits -= m;
            }
        }
    }

    return (good>0);
}


long NEAACDECAPI NeAACDecInit(NeAACDecHandle hpDecoder,
                              unsigned char *buffer,
                              unsigned long buffer_size,
                              unsigned long *samplerate,
                              unsigned char *channels)
{
    uint32_t bits = 0;
    bitfile ld;
    adif_header adif;
    adts_header adts;
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;


    if ((hDecoder == NULL) || (samplerate == NULL) || (channels == NULL))
        return -1;

    hDecoder->sf_index = get_sr_index(hDecoder->config.defSampleRate);
    hDecoder->object_type = hDecoder->config.defObjectType;
    *samplerate = get_sample_rate(hDecoder->sf_index);
    *channels = 1;

    if (buffer != NULL)
    {
#if 0
        int is_latm;
        latm_header *l = &hDecoder->latm_config;
#endif

        faad_initbits(&ld, buffer, buffer_size);
 
#if 0
        memset(l, 0, sizeof(latm_header));
        is_latm = latmCheck(l, &ld);
        l->inited = 0;
        l->frameLength = 0;
        faad_rewindbits(&ld);
        if(is_latm && l->ASCbits>0)
        {
            int32_t x;
            hDecoder->latm_header_present = 1;
            x = NeAACDecInit2(hDecoder, l->ASC, (l->ASCbits+7)/8, samplerate, channels);
            if(x!=0)
                hDecoder->latm_header_present = 0;
            return x;
        } else
#endif
        /* Check if an ADIF header is present */
        if ((buffer[0] == 'A') && (buffer[1] == 'D') &&
			(buffer[2] == 'I') && (buffer[3] == 'F'))
		{
            hDecoder->adif_header_present = 1;

            get_adif_header(&adif, &ld);
            faad_byte_align(&ld);

            hDecoder->sf_index = adif.pce[0].sf_index;
            hDecoder->object_type = adif.pce[0].object_type + 1;

            *samplerate = get_sample_rate(hDecoder->sf_index);
            *channels = adif.pce[0].channels;

            memcpy(&(hDecoder->pce), &(adif.pce[0]), sizeof(program_config));
            hDecoder->pce_set = 1;

            bits = bit2byte(faad_get_processed_bits(&ld));

        /* Check if an ADTS header is present */
        } else if (faad_showbits(&ld, 12) == 0xfff) {
            hDecoder->adts_header_present = 1;

            adts.old_format = hDecoder->config.useOldADTSFormat;
            adts_frame(&adts, &ld);

            hDecoder->sf_index = adts.sf_index;
            hDecoder->object_type = adts.profile + 1;

            *samplerate = get_sample_rate(hDecoder->sf_index);
            *channels = (adts.channel_configuration > 6) ?
                2 : adts.channel_configuration;
        }

        if (ld.error)
        {
            faad_endbits(&ld);
            return -1;
        }
        faad_endbits(&ld);
    }

#if (defined(PS_DEC) || defined(DRM_PS))
    /* check if we have a mono file */
    if (*channels == 1)
    {
        /* upMatrix to 2 channels for implicit signalling of PS */
        *channels = 2;
    }
#endif

    hDecoder->channelConfiguration = *channels;

#ifdef SBR_DEC
    /* implicit signalling */
    if (*samplerate <= 24000 && (hDecoder->config.dontUpSampleImplicitSBR == 0))
    {
        *samplerate *= 2;
        hDecoder->forceUpSampling = 1;
    } else if (*samplerate > 24000 && (hDecoder->config.dontUpSampleImplicitSBR == 0)) {
        hDecoder->downSampledSBR = 1;
    }
#endif

    /* must be done before frameLength is divided by 2 for LD */
#ifdef SSR_DEC
    if (hDecoder->object_type == SSR)
        hDecoder->fb = ssr_filter_bank_init(hDecoder->frameLength/SSR_BANDS);
    else
#endif
        hDecoder->fb = filter_bank_init(hDecoder->frameLength);

#ifdef LD_DEC
    if (hDecoder->object_type == LD)
        hDecoder->frameLength >>= 1;
#endif

    if (can_decode_ot(hDecoder->object_type) < 0)
        return -1;

    return bits;
}

/* Init the library using a DecoderSpecificInfo */
char NEAACDECAPI NeAACDecInit2(NeAACDecHandle hpDecoder,
                               unsigned char *pBuffer,
                               unsigned long SizeOfDecoderSpecificInfo,
                               unsigned long *samplerate,
                               unsigned char *channels)
{
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    int8_t rc;
    mp4AudioSpecificConfig mp4ASC;

    if((hDecoder == NULL)
        || (pBuffer == NULL)
        || (SizeOfDecoderSpecificInfo < 2)
        || (samplerate == NULL)
        || (channels == NULL))
    {
        return -1;
    }

    hDecoder->adif_header_present = 0;
    hDecoder->adts_header_present = 0;

    /* decode the audio specific config */
    rc = AudioSpecificConfig2(pBuffer, SizeOfDecoderSpecificInfo, &mp4ASC,
        &(hDecoder->pce), hDecoder->latm_header_present);

    /* copy the relevant info to the decoder handle */
    *samplerate = mp4ASC.samplingFrequency;
    if (mp4ASC.channelsConfiguration)
    {
        *channels = mp4ASC.channelsConfiguration;
    } else {
        *channels = hDecoder->pce.channels;
        hDecoder->pce_set = 1;
    }
#if (defined(PS_DEC) || defined(DRM_PS))
    /* check if we have a mono file */
    if (*channels == 1)
    {
        /* upMatrix to 2 channels for implicit signalling of PS */
        *channels = 2;
    }
#endif
    hDecoder->sf_index = mp4ASC.samplingFrequencyIndex;
    hDecoder->object_type = mp4ASC.objectTypeIndex;
#ifdef ERROR_RESILIENCE
    hDecoder->aacSectionDataResilienceFlag = mp4ASC.aacSectionDataResilienceFlag;
    hDecoder->aacScalefactorDataResilienceFlag = mp4ASC.aacScalefactorDataResilienceFlag;
    hDecoder->aacSpectralDataResilienceFlag = mp4ASC.aacSpectralDataResilienceFlag;
#endif
#ifdef SBR_DEC
    hDecoder->sbr_present_flag = mp4ASC.sbr_present_flag;
    hDecoder->downSampledSBR = mp4ASC.downSampledSBR;
    if (hDecoder->config.dontUpSampleImplicitSBR == 0)
        hDecoder->forceUpSampling = mp4ASC.forceUpSampling;
    else
        hDecoder->forceUpSampling = 0;

    /* AAC core decoder samplerate is 2 times as low */
    if (((hDecoder->sbr_present_flag == 1)&&(!hDecoder->downSampledSBR)) || hDecoder->forceUpSampling == 1)
    {
        hDecoder->sf_index = get_sr_index(mp4ASC.samplingFrequency / 2);
    }
#endif

    if (rc != 0)
    {
        return rc;
    }
    hDecoder->channelConfiguration = mp4ASC.channelsConfiguration;
    if (mp4ASC.frameLengthFlag)
#ifdef ALLOW_SMALL_FRAMELENGTH
        hDecoder->frameLength = 960;
#else
        return -1;
#endif

    /* must be done before frameLength is divided by 2 for LD */
#ifdef SSR_DEC
    if (hDecoder->object_type == SSR)
        hDecoder->fb = ssr_filter_bank_init(hDecoder->frameLength/SSR_BANDS);
    else
#endif
        hDecoder->fb = filter_bank_init(hDecoder->frameLength);

#ifdef LD_DEC
    if (hDecoder->object_type == LD)
        hDecoder->frameLength >>= 1;
#endif

    return 0;
}

#ifdef DRM
char NEAACDECAPI NeAACDecInitDRM(NeAACDecHandle *hpDecoder,
                                 unsigned long samplerate,
                                 unsigned char channels)
{
    NeAACDecStruct** hDecoder = (NeAACDecStruct**)hpDecoder;
    if (hDecoder == NULL)
        return 1; /* error */

    NeAACDecClose(*hDecoder);

    *hDecoder = NeAACDecOpen();

    /* Special object type defined for DRM */
    (*hDecoder)->config.defObjectType = DRM_ER_LC;

    (*hDecoder)->config.defSampleRate = samplerate;
#ifdef ERROR_RESILIENCE // This shoudl always be defined for DRM
    (*hDecoder)->aacSectionDataResilienceFlag = 1; /* VCB11 */
    (*hDecoder)->aacScalefactorDataResilienceFlag = 0; /* no RVLC */
    (*hDecoder)->aacSpectralDataResilienceFlag = 1; /* HCR */
#endif
    (*hDecoder)->frameLength = 960;
    (*hDecoder)->sf_index = get_sr_index((*hDecoder)->config.defSampleRate);
    (*hDecoder)->object_type = (*hDecoder)->config.defObjectType;

    if ((channels == DRMCH_STEREO) || (channels == DRMCH_SBR_STEREO))
        (*hDecoder)->channelConfiguration = 2;
    else
        (*hDecoder)->channelConfiguration = 1;

#ifdef SBR_DEC
    if ((channels == DRMCH_MONO) || (channels == DRMCH_STEREO))
        (*hDecoder)->sbr_present_flag = 0;
    else
        (*hDecoder)->sbr_present_flag = 1;
#endif

    (*hDecoder)->fb = filter_bank_init((*hDecoder)->frameLength);

    return 0;
}
#endif

void NEAACDECAPI NeAACDecClose(NeAACDecHandle hpDecoder)
{
    uint8_t i;
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;

    if (hDecoder == NULL)
        return;

#ifdef PROFILE
    printf("AAC decoder total:  %I64d cycles\n", hDecoder->cycles);
    printf("requant:            %I64d cycles\n", hDecoder->requant_cycles);
    printf("spectral_data:      %I64d cycles\n", hDecoder->spectral_cycles);
    printf("scalefactors:       %I64d cycles\n", hDecoder->scalefac_cycles);
    printf("output:             %I64d cycles\n", hDecoder->output_cycles);
#endif

    for (i = 0; i < MAX_CHANNELS; i++)
    {
        if (hDecoder->time_out[i]) faad_free(hDecoder->time_out[i]);
        if (hDecoder->fb_intermed[i]) faad_free(hDecoder->fb_intermed[i]);
#ifdef SSR_DEC
        if (hDecoder->ssr_overlap[i]) faad_free(hDecoder->ssr_overlap[i]);
        if (hDecoder->prev_fmd[i]) faad_free(hDecoder->prev_fmd[i]);
#endif
#ifdef MAIN_DEC
        if (hDecoder->pred_stat[i]) faad_free(hDecoder->pred_stat[i]);
#endif
#ifdef LTP_DEC
        if (hDecoder->lt_pred_stat[i]) faad_free(hDecoder->lt_pred_stat[i]);
#endif
    }

#ifdef SSR_DEC
    if (hDecoder->object_type == SSR)
        ssr_filter_bank_end(hDecoder->fb);
    else
#endif
        filter_bank_end(hDecoder->fb);

    drc_end(hDecoder->drc);

    if (hDecoder->sample_buffer) faad_free(hDecoder->sample_buffer);

#ifdef SBR_DEC
    for (i = 0; i < MAX_SYNTAX_ELEMENTS; i++)
    {
        if (hDecoder->sbr[i])
            sbrDecodeEnd(hDecoder->sbr[i]);
    }
#endif

    if (hDecoder) faad_free(hDecoder);
}

void NEAACDECAPI NeAACDecPostSeekReset(NeAACDecHandle hpDecoder, long frame)
{
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if (hDecoder)
    {
        hDecoder->postSeekResetFlag = 1;

        if (frame != -1)
            hDecoder->frame = frame;
    }
}

static void create_channel_config(NeAACDecStruct *hDecoder, NeAACDecFrameInfo *hInfo)
{
    hInfo->num_front_channels = 0;
    hInfo->num_side_channels = 0;
    hInfo->num_back_channels = 0;
    hInfo->num_lfe_channels = 0;
    memset(hInfo->channel_position, 0, MAX_CHANNELS*sizeof(uint8_t));

    if (hDecoder->downMatrix)
    {
        hInfo->num_front_channels = 2;
        hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
        hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
        return;
    }

    /* check if there is a PCE */
    if (hDecoder->pce_set)
    {
        uint8_t i, chpos = 0;
        uint8_t chdir, back_center = 0;

        hInfo->num_front_channels = hDecoder->pce.num_front_channels;
        hInfo->num_side_channels = hDecoder->pce.num_side_channels;
        hInfo->num_back_channels = hDecoder->pce.num_back_channels;
        hInfo->num_lfe_channels = hDecoder->pce.num_lfe_channels;

        chdir = hInfo->num_front_channels;
        if (chdir & 1)
        {
#if (defined(PS_DEC) || defined(DRM_PS))
            /* When PS is enabled output is always stereo */
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_RIGHT;
#else
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_CENTER;
            chdir--;
#endif
        }
        for (i = 0; i < chdir; i += 2)
        {
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = FRONT_CHANNEL_RIGHT;
        }

        for (i = 0; i < hInfo->num_side_channels; i += 2)
        {
            hInfo->channel_position[chpos++] = SIDE_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = SIDE_CHANNEL_RIGHT;
        }

        chdir = hInfo->num_back_channels;
        if (chdir & 1)
        {
            back_center = 1;
            chdir--;
        }
        for (i = 0; i < chdir; i += 2)
        {
            hInfo->channel_position[chpos++] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[chpos++] = BACK_CHANNEL_RIGHT;
        }
        if (back_center)
        {
            hInfo->channel_position[chpos++] = BACK_CHANNEL_CENTER;
        }

        for (i = 0; i < hInfo->num_lfe_channels; i++)
        {
            hInfo->channel_position[chpos++] = LFE_CHANNEL;
        }

    } else {
        switch (hDecoder->channelConfiguration)
        {
        case 1:
#if (defined(PS_DEC) || defined(DRM_PS))
            /* When PS is enabled output is always stereo */
            hInfo->num_front_channels = 2;
            hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
#else
            hInfo->num_front_channels = 1;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
#endif
            break;
        case 2:
            hInfo->num_front_channels = 2;
            hInfo->channel_position[0] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[1] = FRONT_CHANNEL_RIGHT;
            break;
        case 3:
            hInfo->num_front_channels = 3;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            break;
        case 4:
            hInfo->num_front_channels = 3;
            hInfo->num_back_channels = 1;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            hInfo->channel_position[3] = BACK_CHANNEL_CENTER;
            break;
        case 5:
            hInfo->num_front_channels = 3;
            hInfo->num_back_channels = 2;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            hInfo->channel_position[3] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[4] = BACK_CHANNEL_RIGHT;
            break;
        case 6:
            hInfo->num_front_channels = 3;
            hInfo->num_back_channels = 2;
            hInfo->num_lfe_channels = 1;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            hInfo->channel_position[3] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[4] = BACK_CHANNEL_RIGHT;
            hInfo->channel_position[5] = LFE_CHANNEL;
            break;
        case 7:
            hInfo->num_front_channels = 3;
            hInfo->num_side_channels = 2;
            hInfo->num_back_channels = 2;
            hInfo->num_lfe_channels = 1;
            hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
            hInfo->channel_position[1] = FRONT_CHANNEL_LEFT;
            hInfo->channel_position[2] = FRONT_CHANNEL_RIGHT;
            hInfo->channel_position[3] = SIDE_CHANNEL_LEFT;
            hInfo->channel_position[4] = SIDE_CHANNEL_RIGHT;
            hInfo->channel_position[5] = BACK_CHANNEL_LEFT;
            hInfo->channel_position[6] = BACK_CHANNEL_RIGHT;
            hInfo->channel_position[7] = LFE_CHANNEL;
            break;
        default: /* channelConfiguration == 0 || channelConfiguration > 7 */
            {
                uint8_t i;
                uint8_t ch = hDecoder->fr_channels - hDecoder->has_lfe;
                if (ch & 1) /* there's either a center front or a center back channel */
                {
                    uint8_t ch1 = (ch-1)/2;
                    if (hDecoder->first_syn_ele == ID_SCE)
                    {
                        hInfo->num_front_channels = ch1 + 1;
                        hInfo->num_back_channels = ch1;
                        hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                        for (i = 1; i <= ch1; i+=2)
                        {
                            hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                            hInfo->channel_position[i+1] = FRONT_CHANNEL_RIGHT;
                        }
                        for (i = ch1+1; i < ch; i+=2)
                        {
                            hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                            hInfo->channel_position[i+1] = BACK_CHANNEL_RIGHT;
                        }
                    } else {
                        hInfo->num_front_channels = ch1;
                        hInfo->num_back_channels = ch1 + 1;
                        for (i = 0; i < ch1; i+=2)
                        {
                            hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                            hInfo->channel_position[i+1] = FRONT_CHANNEL_RIGHT;
                        }
                        for (i = ch1; i < ch-1; i+=2)
                        {
                            hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                            hInfo->channel_position[i+1] = BACK_CHANNEL_RIGHT;
                        }
                        hInfo->channel_position[ch-1] = BACK_CHANNEL_CENTER;
                    }
                } else {
                    uint8_t ch1 = (ch)/2;
                    hInfo->num_front_channels = ch1;
                    hInfo->num_back_channels = ch1;
                    if (ch1 & 1)
                    {
                        hInfo->channel_position[0] = FRONT_CHANNEL_CENTER;
                        for (i = 1; i <= ch1; i+=2)
                        {
                            hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                            hInfo->channel_position[i+1] = FRONT_CHANNEL_RIGHT;
                        }
                        for (i = ch1+1; i < ch-1; i+=2)
                        {
                            hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                            hInfo->channel_position[i+1] = BACK_CHANNEL_RIGHT;
                        }
                        hInfo->channel_position[ch-1] = BACK_CHANNEL_CENTER;
                    } else {
                        for (i = 0; i < ch1; i+=2)
                        {
                            hInfo->channel_position[i] = FRONT_CHANNEL_LEFT;
                            hInfo->channel_position[i+1] = FRONT_CHANNEL_RIGHT;
                        }
                        for (i = ch1; i < ch; i+=2)
                        {
                            hInfo->channel_position[i] = BACK_CHANNEL_LEFT;
                            hInfo->channel_position[i+1] = BACK_CHANNEL_RIGHT;
                        }
                    }
                }
                hInfo->num_lfe_channels = hDecoder->has_lfe;
                for (i = ch; i < hDecoder->fr_channels; i++)
                {
                    hInfo->channel_position[i] = LFE_CHANNEL;
                }
            }
            break;
        }
    }
}

void* NEAACDECAPI NeAACDecDecode(NeAACDecHandle hpDecoder,
                                 NeAACDecFrameInfo *hInfo,
                                 unsigned char *buffer,
                                 unsigned long buffer_size)
{
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    return aac_frame_decode(hDecoder, hInfo, buffer, buffer_size, NULL, 0);
}

void* NEAACDECAPI NeAACDecDecode2(NeAACDecHandle hpDecoder,
                                  NeAACDecFrameInfo *hInfo,
                                  unsigned char *buffer,
                                  unsigned long buffer_size,
                                  void **sample_buffer,
                                  unsigned long sample_buffer_size)
{
    NeAACDecStruct* hDecoder = (NeAACDecStruct*)hpDecoder;
    if ((sample_buffer == NULL) || (sample_buffer_size == 0))
    {
        hInfo->error = 27;
        return NULL;
    }

    return aac_frame_decode(hDecoder, hInfo, buffer, buffer_size,
        sample_buffer, sample_buffer_size);
}

#ifdef DRM

#define ERROR_STATE_INIT 6

static void conceal_output(NeAACDecStruct *hDecoder, uint16_t frame_len,
                           uint8_t out_ch, void *sample_buffer)
{
    return;
}
#endif

static void* aac_frame_decode(NeAACDecStruct *hDecoder,
                              NeAACDecFrameInfo *hInfo,
                              unsigned char *buffer,
                              unsigned long buffer_size,
                              void **sample_buffer2,
                              unsigned long sample_buffer_size)
{
    uint16_t i;
    uint8_t channels = 0;
    uint8_t output_channels = 0;
    bitfile ld = {0};
    uint32_t bitsconsumed;
    uint16_t frame_len;
    void *sample_buffer;
    uint32_t startbit=0, endbit=0, payload_bits=0;

#ifdef PROFILE
    int64_t count = faad_get_ts();
#endif

    /* safety checks */
    if ((hDecoder == NULL) || (hInfo == NULL) || (buffer == NULL))
    {
        return NULL;
    }

#if 0
    printf("%d\n", buffer_size*8);
#endif

    frame_len = hDecoder->frameLength;


    memset(hInfo, 0, sizeof(NeAACDecFrameInfo));
    memset(hDecoder->internal_channel, 0, MAX_CHANNELS*sizeof(hDecoder->internal_channel[0]));

#ifdef USE_TIME_LIMIT
    if ((TIME_LIMIT * get_sample_rate(hDecoder->sf_index)) > hDecoder->TL_count)
    {
        hDecoder->TL_count += 1024;
    } else {
        hInfo->error = (NUM_ERROR_MESSAGES-1);
        goto error;
    }
#endif


    /* check for some common metadata tag types in the bitstream
     * No need to return an error
     */
    /* ID3 */
    if (buffer_size >= 128)
    {
        if (memcmp(buffer, "TAG", 3) == 0)
        {
            /* found it */
            hInfo->bytesconsumed = 128; /* 128 bytes fixed size */
            /* no error, but no output either */
            return NULL;
        }
    }


    /* initialize the bitstream */
    faad_initbits(&ld, buffer, buffer_size);

#if 0
    {
        int i;
        for (i = 0; i < ((buffer_size+3)>>2); i++)
        {
            uint8_t *buf;
            uint32_t temp = 0;
            buf = faad_getbitbuffer(&ld, 32);
            //temp = getdword((void*)buf);
            temp = *((uint32_t*)buf);
            printf("0x%.8X\n", temp);
            free(buf);
        }
        faad_endbits(&ld);
        faad_initbits(&ld, buffer, buffer_size);
    }
#endif

#if 0
    if(hDecoder->latm_header_present)
    {
        payload_bits = faad_latm_frame(&hDecoder->latm_config, &ld);
        startbit = faad_get_processed_bits(&ld);
        if(payload_bits == -1U)
        {
            hInfo->error = 1;
            goto error;
        }
    }
#endif

#ifdef DRM
    if (hDecoder->object_type == DRM_ER_LC)
    {
        /* We do not support stereo right now */
        if (0) //(hDecoder->channelConfiguration == 2)
        {
            hInfo->error = 28; // Throw CRC error
            goto error;
        }

        faad_getbits(&ld, 8
            DEBUGVAR(1,1,"NeAACDecDecode(): skip CRC"));
    }
#endif

    if (hDecoder->adts_header_present)
    {
        adts_header adts;

        adts.old_format = hDecoder->config.useOldADTSFormat;
        if ((hInfo->error = adts_frame(&adts, &ld)) > 0)
            goto error;

        /* MPEG2 does byte_alignment() here,
         * but ADTS header is always multiple of 8 bits in MPEG2
         * so not needed to actually do it.
         */
    }

#ifdef ANALYSIS
    dbg_count = 0;
#endif

    /* decode the complete bitstream */
#ifdef DRM
    if (/*(hDecoder->object_type == 6) ||*/ (hDecoder->object_type == DRM_ER_LC))
    {
        DRM_aac_scalable_main_element(hDecoder, hInfo, &ld, &hDecoder->pce, hDecoder->drc);
    } else {
#endif
        raw_data_block(hDecoder, hInfo, &ld, &hDecoder->pce, hDecoder->drc);
#ifdef DRM
    }
#endif

#if 0
    if(hDecoder->latm_header_present)
    {
        endbit = faad_get_processed_bits(&ld);
        if(endbit-startbit > payload_bits)
            fprintf(stderr, "\r\nERROR, too many payload bits read: %u > %d. Please. report with a link to a sample\n",
                endbit-startbit, payload_bits);
        if(hDecoder->latm_config.otherDataLenBits > 0)
            faad_getbits(&ld, hDecoder->latm_config.otherDataLenBits);
        faad_byte_align(&ld);
    }
#endif

    channels = hDecoder->fr_channels;

    if (hInfo->error > 0)
        goto error;

    /* safety check */
    if (channels == 0 || channels > MAX_CHANNELS)
    {
        /* invalid number of channels */
        hInfo->error = 12;
        goto error;
    }

    /* no more bit reading after this */
    bitsconsumed = faad_get_processed_bits(&ld);
    hInfo->bytesconsumed = bit2byte(bitsconsumed);
    if (ld.error)
    {
        hInfo->error = 14;
        goto error;
    }
    faad_endbits(&ld);


    if (!hDecoder->adts_header_present && !hDecoder->adif_header_present
#if 0
        && !hDecoder->latm_header_present
#endif
        )
    {
        if (hDecoder->channelConfiguration == 0)
            hDecoder->channelConfiguration = channels;

        if (channels == 8) /* 7.1 */
            hDecoder->channelConfiguration = 7;
        if (channels == 7) /* not a standard channelConfiguration */
            hDecoder->channelConfiguration = 0;
    }

    if ((channels == 5 || channels == 6) && hDecoder->config.downMatrix)
    {
        hDecoder->downMatrix = 1;
        output_channels = 2;
    } else {
        output_channels = channels;
    }

#if (defined(PS_DEC) || defined(DRM_PS))
    hDecoder->upMatrix = 0;
    /* check if we have a mono file */
    if (output_channels == 1)
    {
        /* upMatrix to 2 channels for implicit signalling of PS */
        hDecoder->upMatrix = 1;
        output_channels = 2;
    }
#endif

    /* Make a channel configuration based on either a PCE or a channelConfiguration */
    create_channel_config(hDecoder, hInfo);

    /* number of samples in this frame */
    hInfo->samples = frame_len*output_channels;
    /* number of channels in this frame */
    hInfo->channels = output_channels;
    /* samplerate */
    hInfo->samplerate = get_sample_rate(hDecoder->sf_index);
    /* object type */
    hInfo->object_type = hDecoder->object_type;
    /* sbr */
    hInfo->sbr = NO_SBR;
    /* header type */
    hInfo->header_type = RAW;
    if (hDecoder->adif_header_present)
        hInfo->header_type = ADIF;
    if (hDecoder->adts_header_present)
        hInfo->header_type = ADTS;
#if 0
    if (hDecoder->latm_header_present)
        hInfo->header_type = LATM;
#endif
#if (defined(PS_DEC) || defined(DRM_PS))
    hInfo->ps = hDecoder->ps_used_global;
#endif

    /* check if frame has channel elements */
    if (channels == 0)
    {
        hDecoder->frame++;
        return NULL;
    }

    /* allocate the buffer for the final samples */
    if ((hDecoder->sample_buffer == NULL) ||
        (hDecoder->alloced_channels != output_channels))
    {
        static const uint8_t str[] = { sizeof(int16_t), sizeof(int32_t), sizeof(int32_t),
            sizeof(float32_t), sizeof(double), sizeof(int16_t), sizeof(int16_t),
            sizeof(int16_t), sizeof(int16_t), 0, 0, 0
        };
        uint8_t stride = str[hDecoder->config.outputFormat-1];
#ifdef SBR_DEC
        if (((hDecoder->sbr_present_flag == 1)&&(!hDecoder->downSampledSBR)) || (hDecoder->forceUpSampling == 1))
        {
            stride = 2 * stride;
        }
#endif
        /* check if we want to use internal sample_buffer */
        if (sample_buffer_size == 0)
        {
            if (hDecoder->sample_buffer)
                faad_free(hDecoder->sample_buffer);
            hDecoder->sample_buffer = NULL;
            hDecoder->sample_buffer = faad_malloc(frame_len*output_channels*stride);
        } else if (sample_buffer_size < frame_len*output_channels*stride) {
            /* provided sample buffer is not big enough */
            hInfo->error = 27;
            return NULL;
        }
        hDecoder->alloced_channels = output_channels;
    }

    if (sample_buffer_size == 0)
    {
        sample_buffer = hDecoder->sample_buffer;
    } else {
        sample_buffer = *sample_buffer2;
    }

#ifdef SBR_DEC
    if ((hDecoder->sbr_present_flag == 1) || (hDecoder->forceUpSampling == 1))
    {
        uint8_t ele;

        /* this data is different when SBR is used or when the data is upsampled */
        if (!hDecoder->downSampledSBR)
        {
            frame_len *= 2;
            hInfo->samples *= 2;
            hInfo->samplerate *= 2;
        }

        /* check if every element was provided with SBR data */
        for (ele = 0; ele < hDecoder->fr_ch_ele; ele++)
        {
            if (hDecoder->sbr[ele] == NULL)
            {
                hInfo->error = 25;
                goto error;
            }
        }

        /* sbr */
        if (hDecoder->sbr_present_flag == 1)
        {
            hInfo->object_type = HE_AAC;
            hInfo->sbr = SBR_UPSAMPLED;
        } else {
            hInfo->sbr = NO_SBR_UPSAMPLED;
        }
        if (hDecoder->downSampledSBR)
        {
            hInfo->sbr = SBR_DOWNSAMPLED;
        }
    }
#endif


    sample_buffer = output_to_PCM(hDecoder, hDecoder->time_out, sample_buffer,
        output_channels, frame_len, hDecoder->config.outputFormat);


#ifdef DRM
    //conceal_output(hDecoder, frame_len, output_channels, sample_buffer);
#endif


    hDecoder->postSeekResetFlag = 0;

    hDecoder->frame++;
#ifdef LD_DEC
    if (hDecoder->object_type != LD)
    {
#endif
        if (hDecoder->frame <= 1)
            hInfo->samples = 0;
#ifdef LD_DEC
    } else {
        /* LD encoders will give lower delay */
        if (hDecoder->frame <= 0)
            hInfo->samples = 0;
    }
#endif

    /* cleanup */
#ifdef ANALYSIS
    fflush(stdout);
#endif

#ifdef PROFILE
    count = faad_get_ts() - count;
    hDecoder->cycles += count;
#endif

    return sample_buffer;

error:


#ifdef DRM
    hDecoder->error_state = ERROR_STATE_INIT;
#endif

    /* reset filterbank state */
    for (i = 0; i < MAX_CHANNELS; i++)
    {
        if (hDecoder->fb_intermed[i] != NULL)
        {
            memset(hDecoder->fb_intermed[i], 0, hDecoder->frameLength*sizeof(real_t));
        }
    }
#ifdef SBR_DEC
    for (i = 0; i < MAX_SYNTAX_ELEMENTS; i++)
    {
        if (hDecoder->sbr[i] != NULL)
        {
            sbrReset(hDecoder->sbr[i]);
        }
    }
#endif


    faad_endbits(&ld);

    /* cleanup */
#ifdef ANALYSIS
    fflush(stdout);
#endif

    return NULL;
}
