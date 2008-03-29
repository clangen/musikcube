#include "StdAfx.h"
#include "oggdecoder.h"

OGGDecoder::OGGDecoder()
{
	m_currentsection = 0;
}

OGGDecoder::~OGGDecoder()
{
}

bool OGGDecoder::Open(const utfchar* source)
{
	m_file = _wfopen(source, TEXT("rbS"));

	if(m_file == NULL)
		return false;
	
	if(ov_open(m_file, &m_vf, NULL, 0) < 0) 
	{
		return false;
	}

	vorbis_info *	vi = ov_info(&m_vf, -1);
	m_Buffer = (float*)malloc(4096 * 4 * vi->channels);

    this->sourcePath = source;

	return(true);
}

bool OGGDecoder::Close()
{
	free(m_Buffer);
	ov_clear(&m_vf);
	return(true);
}

void		OGGDecoder::Destroy(void)
{
	Close();
	delete this;
}

bool		OGGDecoder::GetFormat(unsigned long * SampleRate, unsigned long * Channels)
{
	vorbis_info *	vi = ov_info(&m_vf, -1);

	*SampleRate = vi->rate;
	*Channels	= vi->channels;

	return(true);
}

bool		OGGDecoder::GetLength(unsigned long * MS)
{

	double time = ov_time_total(&m_vf, 0);

	*MS = (unsigned long)(time * 1000.0);

	return(true);
}

bool		OGGDecoder::SetPosition(unsigned long * MS)
{
	double pos = *MS / 1000.0;

	if(ov_seekable(&m_vf))
	{
		if(!ov_time_seek(&m_vf, pos))
		{
			double time = ov_time_tell(&m_vf);
			*MS = (unsigned long)(time * 1000.0);

			return(true);
		}
	}

	return(false);
}

bool		OGGDecoder::SetState(unsigned long State)
{
	return(true);
}

bool		OGGDecoder::GetBuffer(float ** ppBuffer, unsigned long * NumSamples)
{
	float ** pcm;
	vorbis_info *	vi = ov_info(&m_vf, -1);

	*NumSamples =0;

	unsigned long bytes_read = ov_read_float(&m_vf, &pcm, 4096, (int *)&m_currentsection);
    if(bytes_read == 0) 
	{
		return(false);
    }

/*		TUNIAC EXPECTS
SPEAKER_FRONT_LEFT 0x1 
SPEAKER_FRONT_RIGHT 0x2 
SPEAKER_FRONT_CENTER 0x4 
SPEAKER_LOW_FREQUENCY 0x8 
SPEAKER_BACK_LEFT 0x10 
SPEAKER_BACK_RIGHT 0x20 
*/

/*		OGG DOES

One channel: the stream is monophonic
Two channels: the stream is stereo. channel order: left, right.
Three channels: the stream is a 1d-surround encoding. channel order: left, center, right
Four channels: the stream is quadraphonic surround. channel order: front left, front right, rear left, rear right
Five channels: the stream is five-channel surround. channel order: front left, front center, front right, rear left, rear right
Six channels: (used in Dolby Digital/AC3) the stream is 5,1 surround. channel order: front left, front center, front right, rear left, rear right, LFE (the sixth channel is entirely bass). 
*/

//		so we need to REORDER

	if(vi->channels == 3)
	{
		float * pDataBuffer = m_Buffer;
		for(unsigned long x=0; x<bytes_read; x++)
		{
			*pDataBuffer = pcm[0][x];
			pDataBuffer++;

			*pDataBuffer = pcm[2][x];
			pDataBuffer++;

			*pDataBuffer = pcm[1][x];
			pDataBuffer++;
		}
	}
	else if(vi->channels == 5)
	{
		float * pDataBuffer = m_Buffer;
		for(unsigned long x=0; x<bytes_read; x++)
		{
			*pDataBuffer = pcm[0][x];
			pDataBuffer++;

			*pDataBuffer = pcm[2][x];
			pDataBuffer++;

			*pDataBuffer = pcm[1][x];
			pDataBuffer++;

			*pDataBuffer = pcm[3][x];
			pDataBuffer++;

			*pDataBuffer = pcm[4][x];
			pDataBuffer++;
		}
	}
	else if(vi->channels == 6)
	{
		float * pDataBuffer = m_Buffer;
		for(unsigned long x=0; x<bytes_read; x++)
		{
			*pDataBuffer = pcm[0][x];
			pDataBuffer++;

			*pDataBuffer = pcm[2][x];
			pDataBuffer++;

			*pDataBuffer = pcm[1][x];
			pDataBuffer++;

			*pDataBuffer = pcm[5][x];
			pDataBuffer++;

			*pDataBuffer = pcm[3][x];
			pDataBuffer++;

			*pDataBuffer = pcm[4][x];
			pDataBuffer++;
		}
	}
	else
	{
		float * pDataBuffer = m_Buffer;
		for(unsigned long x=0; x<bytes_read; x++)
		{
			for(/*unsigned long*/int channel=0; channel < vi->channels; channel++)
			{
				*pDataBuffer = pcm[channel][x];
				pDataBuffer++;
			}
		}
	}

	*NumSamples = bytes_read * vi->channels;
	*ppBuffer = m_Buffer;

	return(true);
}
