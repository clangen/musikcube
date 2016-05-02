#include "StdAfx.h"
#include <math.h>
#include <iostream>
#include "mp3decoder.h"

static bool splitFrame(musik::core::io::IDataStream *dataStream, Frame &fr) {
    unsigned char headerbuf[4] = { 0, 0, 0, 0 };
    unsigned char crcbuf[2];
    unsigned char sideInfoBuffer[32];
    unsigned long bytesRead;
    unsigned long errorCount = 0;
    unsigned long currentOffset = dataStream->Position();

    if (dataStream->Read(headerbuf, 4) != 4) {
        return false;
    }

    while (!fr.m_Header.Load(headerbuf)) {
        headerbuf[0] = headerbuf[1];
        headerbuf[1] = headerbuf[2];
        headerbuf[2] = headerbuf[3];

        if (dataStream->Read(&headerbuf[3], 1) != 1) {
            return false;
        }

        if (errorCount++ >= fr.m_Header.GetDataSize()) {
            fr.m_Header.Reset();
        }

        currentOffset++;
    }

    if (fr.m_Header.IsCRC()) {
        if (dataStream->Read(crcbuf, 2) != 2) {
            return false;
        }

        fr.m_CRC.Load(crcbuf);
    }

    if (fr.m_Header.GetLayer() == LAYER3) {
        unsigned count = fr.m_Header.GetSideInfoSize();

        if (dataStream->Read(sideInfoBuffer, count) != count) {
            return false;
        }

        if (!fr.m_SI.Load(&fr.m_Header, sideInfoBuffer)) {
            return false;
        }
    }

    unsigned frameBytes = fr.m_Header.GetDataSize();
    if (dataStream->Read(fr.m_Data, frameBytes) != frameBytes) {
        return false;
    }

    return true;
}

MP3Decoder::MP3Decoder()
: m_pDecoder(NULL) {
}

MP3Decoder::~MP3Decoder() {
}

unsigned long MP3Decoder::GetID3HeaderLength(unsigned char * buffer) {
	unsigned char VerMajor;
	unsigned char VerMinor;
	unsigned char Flags;
	unsigned long Length;

	if( (toupper(buffer[0]) == 'I') &&
		(toupper(buffer[1]) == 'D') &&
		(toupper(buffer[2]) == '3'))
	{
		VerMajor = buffer[3];
		VerMinor = buffer[4];
		Flags = buffer[5];

		Length = (buffer[6] << 21) | (buffer[7] << 14) | (buffer[8] << 7) | buffer[9];
		Length += 10; // the header

		return(Length);
	}

    return 0;
}

bool MP3Decoder::GetXingHeader(unsigned char * xingBuffer) {
    #define FRAMES_FLAG         0x0001
    #define BYTES_FLAG          0x0002
    #define TOC_FLAG            0x0004
    #define VBR_SCALE_FLAG      0x0008
    #define GET_INT32BE(b)      (i = (b[0] << 24) | (b[1] << 16) | b[2] << 8 | b[3], b += 4, i) /* windows only? */

	unsigned long i;

	m_bXingValid = false;

    if (strncmp((char *) xingBuffer, "Xing", 4)) {
        if (strncmp((char *)xingBuffer, "Info", 4)) {
            return false;
        }
    }

    xingBuffer += 4;

	unsigned long headFlags = GET_INT32BE(xingBuffer);

	m_NumFrames = 0;
    if (headFlags & FRAMES_FLAG) {
        m_NumFrames = GET_INT32BE(xingBuffer);
    }

    if (m_NumFrames < 1) {
        return false;
    }

	m_StreamDataLength = 0;
    if (headFlags & BYTES_FLAG) {
        m_StreamDataLength = GET_INT32BE(xingBuffer);
    }

	if (headFlags & TOC_FLAG) {
        for (i = 0; i < 100; i++) {
            m_TOC[i] = xingBuffer[i];
        }

        xingBuffer += 100;
	}
		
	m_VbrScale = -1;
	if (headFlags & VBR_SCALE_FLAG) {
		m_VbrScale = GET_INT32BE(xingBuffer);
	}

	m_bXingValid = true;

	return true;
}

bool MP3Decoder::GetStreamData() {
	unsigned char tbuf[11];
	unsigned long bytesread;
	Frame fr;

    this->dataStream->Read(tbuf, 10);
    m_ID3v2Length = GetID3HeaderLength(tbuf);
    this->dataStream->SetPosition(m_ID3v2Length);

	if(splitFrame(this->dataStream, fr)) {
		unsigned char * pHeader = fr.m_Data;
		if(!GetXingHeader(pHeader))
		{
			// analyse file here
            this->dataStream->SetPosition(m_ID3v2Length);

            // just guesstimate the number of frames!
            /* DANGEROUS -- ASSUMES FINITE LENGTH*/
			m_StreamDataLength = this->dataStream->Filesize() - m_ID3v2Length;

			// TODO: check for ID3 TAG at the end of the file and subtract
			// also remove the size of this current header
			m_StreamDataLength -= fr.m_Header.GetTotalFrameSize();
			m_NumFrames = m_StreamDataLength / fr.m_Header.GetTotalFrameSize(); 
        }
        else {
            if (m_bXingValid == false) {
                std::cout << "Mp3Decoder.cpp: Mp3 has Xing header but it is invalid.";
            }
        }

        double bs[3] = { 384.0, 1152.0, 1152.0 };
        double TimePerFrame = (double)bs[fr.m_Header.GetLayer()] / (((double)fr.m_Header.GetSampleFrequency() / 1000.0));

        m_StreamLengthMS = TimePerFrame * m_NumFrames;

        if (fr.m_Header.GetMpegVersion() != MPEG1) {
            m_StreamLengthMS /= 2;
        }

        m_SampleRate = fr.m_Header.GetSampleFrequency();
        m_NumChannels = fr.m_Header.GetChannels();

        return true;
    }

    LogConsoleMessage(TEXT("MP3 Decoder"), TEXT("Error calculating mp3 stream information."));

    return false;
}

bool MP3Decoder::Open(musik::core::io::IDataStream *dataStream) {
    this->dataStream = dataStream;
    this->m_LastLayer = -1;
    return GetStreamData();
}

void MP3Decoder::Destroy(void) {
    delete this;
}

#define MP3_BUFFER_FLOAT_ALLOWANCE 2304 /* why? */

bool MP3Decoder::GetBuffer(IBuffer *buffer) {
    buffer->SetChannels(this->m_NumChannels);
    buffer->SetSamples(MP3_BUFFER_FLOAT_ALLOWANCE / buffer->Channels());
    buffer->SetSampleRate(this->m_SampleRate);

    if (splitFrame(this->dataStream, m_Frame)) {
        /* bail if the mpeg layer is incorrect*/
        if ((m_Frame.m_Header.GetLayer() != m_LastLayer) || (m_pDecoder == NULL)) {
            switch (m_Frame.m_Header.GetLayer()) {
                case LAYER3: {
                    if (m_pDecoder) {
                        delete m_pDecoder;
                        m_pDecoder = NULL;
                    }
                    m_pDecoder = new CLayer3Decoder();
                    m_LastLayer = LAYER3;
                }
                break;

                default: {
                    LogConsoleMessage(
                        L"MP3 Decoder", 
                        L"Unsupported Layer (Only Layer 3 supported).");
                    return false;
                }
            }
        }

        unsigned long bufferCount = 0;
        if (!m_pDecoder->ProcessFrame(&m_Frame, buffer->BufferPointer(), &bufferCount)) {
            m_Frame.m_Header.Reset();
            return false;
        }

        buffer->SetSamples(bufferCount / buffer->Channels());
    }

    return true;
}

double MP3Decoder::SetPosition(double seconds, double totalLength) {
    float milliseconds = (float) seconds * 1000.0f;
	float percent = 100.00f * ((float) milliseconds / (float) m_StreamLengthMS);
	unsigned long offset;

	if (m_bXingValid)
	{
		/* interpolate in TOC to get file seek point in bytes */ 
		int a = min(percent, 99);
		float fa, fb, fx;

        fa = m_TOC[a];
		
        if (a < 99) {
            fb = m_TOC[a + 1];
        }
        else {
            fb = 256;
        }
		
		fx = fa + (fb - fa) * (percent - a);
		offset = (1.0f / 256.0f) * fx * m_StreamDataLength;
	}
	else {
		offset = (float) m_StreamDataLength * (float)(percent/ 100.0f) ;
	}

    this->dataStream->SetPosition(offset + m_ID3v2Length);
	bool result = splitFrame(this->dataStream, m_Frame);

    if (m_pDecoder) {
		delete m_pDecoder;
		m_pDecoder = NULL;
	}

    return result ? seconds : -1;
}
