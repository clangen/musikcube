#include "StdAfx.h"
#include <math.h>
#include <iostream>
#include "mp3decoder.h"

static bool splitFrame(musik::core::io::IDataStream *dataStream, Frame &fr) {
    if (dataStream->Eof()) {
        return false;
    }

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

Mp3Decoder::Mp3Decoder()
: decoder(NULL) {
}

Mp3Decoder::~Mp3Decoder() {
}

unsigned long Mp3Decoder::GetID3HeaderLength(unsigned char * buffer) {
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

bool Mp3Decoder::GetXingHeader(unsigned char * xingBuffer) {
    #define FRAMES_FLAG         0x0001
    #define BYTES_FLAG          0x0002
    #define TOC_FLAG            0x0004
    #define VBR_SCALE_FLAG      0x0008
    #define GET_INT32BE(b)      (i = (b[0] << 24) | (b[1] << 16) | b[2] << 8 | b[3], b += 4, i) /* windows only? */

	unsigned long i;

	this->xingValid = false;

    if (strncmp((char *) xingBuffer, "Xing", 4)) {
        if (strncmp((char *)xingBuffer, "Info", 4)) {
            return false;
        }
    }

    xingBuffer += 4;

	unsigned long headFlags = GET_INT32BE(xingBuffer);

    this->numFrames = 0;
    if (headFlags & FRAMES_FLAG) {
        this->numFrames = GET_INT32BE(xingBuffer);
    }

    if (this->numFrames < 1) {
        return false;
    }

    this->streamDataLength = 0;
    if (headFlags & BYTES_FLAG) {
        this->streamDataLength = GET_INT32BE(xingBuffer);
    }

	if (headFlags & TOC_FLAG) {
        for (i = 0; i < 100; i++) {
            this->toc[i] = xingBuffer[i];
        }

        xingBuffer += 100;
	}
		
	this->vbrScale = -1;
	if (headFlags & VBR_SCALE_FLAG) {
        this->vbrScale = GET_INT32BE(xingBuffer);
	}

	this->xingValid = true;

	return true;
}

bool Mp3Decoder::GetStreamData() {
	unsigned char tbuf[11];
	unsigned long bytesread;
	Frame fr;

    this->dataStream->Read(tbuf, 10);
    this->id3v2Length = GetID3HeaderLength(tbuf);
    this->dataStream->SetPosition(this->id3v2Length);

	if (splitFrame(this->dataStream, fr)) {
		unsigned char * pHeader = fr.m_Data;
		if(!GetXingHeader(pHeader)) {
            this->dataStream->SetPosition(this->id3v2Length);

            // just guesstimate the number of frames!
            /* DANGEROUS -- ASSUMES FINITE LENGTH*/
			this->streamDataLength = this->dataStream->Filesize() - this->id3v2Length;

			// TODO: check for ID3 TAG at the end of the file and subtract
			// also remove the size of this current header
            this->streamDataLength -= fr.m_Header.GetTotalFrameSize();
            this->numFrames = this->streamDataLength / fr.m_Header.GetTotalFrameSize();
        }
        else {
            if (!this->xingValid) {
                std::cout << "Mp3Decoder.cpp: Mp3 has Xing header but it is invalid.";
            }
        }

        double bs[3] = { 384.0, 1152.0, 1152.0 };
        double timePerFrame = (double)bs[fr.m_Header.GetLayer()] / (((double)fr.m_Header.GetSampleFrequency() / 1000.0));

        this->streamLengthMs = timePerFrame * this->numFrames;

        if (fr.m_Header.GetMpegVersion() != MPEG1) {
            this->streamLengthMs /= 2;
        }

        this->sampleRate = fr.m_Header.GetSampleFrequency();
        this->numChannels = fr.m_Header.GetChannels();

        return true;
    }

    return false;
}

bool Mp3Decoder::Open(musik::core::io::IDataStream *dataStream) {
    this->dataStream = dataStream;
    this->lastLayer = -1;
    return GetStreamData();
}

void Mp3Decoder::Destroy(void) {
    delete this;
}

#define MP3_BUFFER_FLOAT_ALLOWANCE 2304 /* why? */

bool Mp3Decoder::GetBuffer(IBuffer *buffer) {
    buffer->SetChannels(this->numChannels);
    buffer->SetSamples(MP3_BUFFER_FLOAT_ALLOWANCE / buffer->Channels());
    buffer->SetSampleRate(this->sampleRate);

    if (splitFrame(this->dataStream, this->frame)) {
        /* bail if the mpeg layer is incorrect*/
        if ((this->frame.m_Header.GetLayer() != this->lastLayer) || (this->decoder == NULL)) {
            switch (this->frame.m_Header.GetLayer()) {
                case LAYER3: {
                    delete this->decoder;
                    this->decoder = NULL;
                    this->decoder = new CLayer3Decoder();
                    this->lastLayer = LAYER3;
                }
                break;

                default: {
                    return false;
                }
            }
        }

        /* we have an mp3... */
        unsigned long bufferCount = 0;
        if (!this->decoder->ProcessFrame(&this->frame, buffer->BufferPointer(), &bufferCount)) {
            this->frame.m_Header.Reset(); /* we're done if ProcessFrame returns false */
            return false;
        }

        buffer->SetSamples(bufferCount / buffer->Channels());
        return true;
    }

    return false;
}

double Mp3Decoder::SetPosition(double seconds, double totalLength) {
    float milliseconds = (float) seconds * 1000.0f;
	float percent = 100.00f * ((float) milliseconds / (float) this->streamLengthMs);
	unsigned long offset;

	if (this->xingValid) {
		/* interpolate in TOC to get file seek point in bytes */ 
		int a = min(percent, 99);
		float fa, fb, fx;

        fa = this->toc[a];
		
        if (a < 99) {
            fb = this->toc[a + 1];
        }
        else {
            fb = 256;
        }
		
		fx = fa + (fb - fa) * (percent - a);
		offset = (1.0f / 256.0f) * fx * this->streamDataLength;
	}
	else {
		offset = (float) this->streamDataLength * (float)(percent/ 100.0f) ;
	}

    this->dataStream->SetPosition(offset + this->id3v2Length);
	bool result = splitFrame(this->dataStream, this->frame);

	delete this->decoder;
    this->decoder = NULL;

    return result ? seconds : -1;
}
