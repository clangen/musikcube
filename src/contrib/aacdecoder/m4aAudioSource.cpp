#include "stdafx.h"

#include <io.h>

#include "m4aaudiosource.h"

static uint32_t read_callback(void *user_data, void *buffer, uint32_t length)
{
    return fread(buffer, 1, length, (FILE*)user_data);
}

static uint32_t seek_callback(void *user_data, uint64_t position)
{
    return fseek((FILE*)user_data, position, SEEK_SET);
}

static uint32_t write_callback(void *user_data, void *buffer, uint32_t length)
{
    return fwrite(buffer, 1, length, (FILE*)user_data);
}

static uint32_t truncate_callback(void *user_data)
{
    _chsize(fileno((FILE*)user_data), ftell((FILE*)user_data));
    return 1;
}

static int GetAACTrack(mp4ff_t *infile)
{
    /* find AAC track */
    int i, rc;
    int numTracks = mp4ff_total_tracks(infile);

    for (i = 0; i < numTracks; i++)
    {
        unsigned char *buff = NULL;
        int buff_size = 0;
        mp4AudioSpecificConfig mp4ASC;

        mp4ff_get_decoder_config(infile, i, &buff, (unsigned int *)&buff_size);

        if (buff)
        {
            rc = AudioSpecificConfig(buff, buff_size, &mp4ASC);
            free(buff);

            if (rc < 0)
                continue;
            return i;
        }
    }

    /* can't decode this */
    return -1;
}

M4ADecoder::M4ADecoder(void)
{
	m_hDecoder		= NULL;
	m_mp4FileHandle = NULL;

}

M4ADecoder::~M4ADecoder(void)
{
}

bool M4ADecoder::Open(const utfchar* source)
{
    unsigned char				*	buffer;
    unsigned int					buffer_size;
    NeAACDecConfigurationPtr		config;

	m_hDecoder = faacDecOpen();
	if(!m_hDecoder)
	{
		return false;
	}

    config = faacDecGetCurrentConfiguration(m_hDecoder);

    config->outputFormat	= FAAD_FMT_FLOAT;
    config->downMatrix		= 0;

    int ret = faacDecSetConfiguration(m_hDecoder, config);

    m_mp4FileHandle			= _wfopen(source, _T("rb"));
    m_mp4cb.read			= read_callback;
    m_mp4cb.seek			= seek_callback;
    m_mp4cb.user_data		= m_mp4FileHandle;

    m_mp4file = mp4ff_open_read(&m_mp4cb);
    if(!m_mp4file)
    {
        return false;
    }

	if((m_mp4track = GetAACTrack(m_mp4file)) < 0)
	{
		return false;
	}

	buffer = NULL;
	buffer_size = 0;
	mp4ff_get_decoder_config(m_mp4file, m_mp4track, &buffer, &buffer_size);
	if(!buffer)
	{
		return false;
	}

    if(faacDecInit2(	m_hDecoder, 
						buffer, 
						buffer_size, 
						(unsigned long *)&m_SampleRate, 
						(unsigned char*)&m_NumChannels) < 0)
    {
        if(buffer) 
			free(buffer);
        return false;
    }

	free(buffer);

    m_numSamples = mp4ff_num_samples(m_mp4file, m_mp4track);
	m_sampleId = 0;

	return true;
}

bool M4ADecoder::Close()
{
	fclose(m_mp4FileHandle);
	mp4ff_close(m_mp4file);

	if(m_hDecoder)
	{
		faacDecClose(m_hDecoder);
		m_hDecoder = NULL;
	}

	return true;
}


void		M4ADecoder::Destroy(void)
{
	this->Close();
	delete this;
}

bool		M4ADecoder::GetLength(unsigned long * MS)
{
    long length;

    length = mp4ff_get_track_duration(m_mp4file, m_mp4track);

    *MS = (long)(length*1000.0 / (float)mp4ff_time_scale(m_mp4file, m_mp4track) + 0.5);

	return true;
}

bool		M4ADecoder::SetPosition(unsigned long * MS)
{
	int64_t		duration;
	float		fms = *MS;
	int32_t		skip_samples = 0;

	duration = (int64_t)(fms/1000.0 * m_SampleRate + 0.5);
	m_sampleId = mp4ff_find_sample_use_offsets(	m_mp4file, 
												m_mp4track, 
												duration, 
												&skip_samples);


	return true;
}

bool		M4ADecoder::SetState(unsigned long State)
{
	return true;
}

bool		M4ADecoder::GetFormat(unsigned long * SampleRate, unsigned long * Channels)
{
	*SampleRate		= m_SampleRate;
	*Channels		= m_NumChannels;

	return true;
}

bool		M4ADecoder::GetBuffer(float ** ppBuffer, unsigned long * NumSamples)
{
    void			*	sample_buffer	= NULL;
    unsigned char	*	buffer			= NULL;
    unsigned long		buffer_size		= 0;
    NeAACDecFrameInfo	frameInfo;

	int					rc;
	long				dur;

	/* get acces unit from MP4 file */

	dur = mp4ff_get_sample_duration(m_mp4file, 
									m_mp4track, 
									m_sampleId);

	if(dur <= 0)
	{
		return false;
	}

	rc = mp4ff_read_sample(	m_mp4file, 
							m_mp4track, 
							m_sampleId, 
							&buffer,  
							(unsigned int*)&buffer_size);

	m_sampleId++;

	if (rc == 0 || buffer == NULL)
	{
		return false;
	} 

	sample_buffer = faacDecDecode(	m_hDecoder, 
									&frameInfo, 
									buffer, 
									buffer_size);

	if (buffer) 
		free(buffer);

	if (frameInfo.error > 0)
	{
		return false;
	}

	if(m_sampleId > m_numSamples)
	{
		return false;
	}

	*ppBuffer	= (float*)sample_buffer;
	*NumSamples = frameInfo.samples;

	return true;
}
