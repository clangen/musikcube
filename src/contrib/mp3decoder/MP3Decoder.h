#pragma once

#include <core/sdk/IDecoder.h>

#include "FrameSplitter.h"
#include "Layer3Decoder.h"

using namespace musik::core::audio;

class MP3Decoder : public IDecoder
{
public:

	MP3Decoder();
	~MP3Decoder();

    bool Open(musik::core::io::IDataStream *dataStream);
	double SetPosition(double seconds, double totalLength);
	bool GetBuffer(IBuffer *buffer);
    void Destroy();

protected:
    CFrameSplitter m_Splitter;
    IMPEGDecoder *m_pDecoder;
    Frame m_Frame;

    unsigned long m_LastLayer;
    unsigned long m_SampleRate;
    unsigned long m_NumChannels;
    unsigned long m_ID3v2Length;
    unsigned long m_StreamLengthMS;
    unsigned long m_NumFrames;
    unsigned long m_StreamDataLength;
    unsigned long m_VbrScale;
    unsigned char m_TOC[100];
    bool m_bXingValid;

    unsigned long GetID3HeaderLength(unsigned char * buffer);
    bool GetXingHeader(unsigned char * XingBuffer);
    bool GetStreamData();

private:
    musik::core::io::IDataStream *dataStream;

	// Stubs
	void LogConsoleMessage(LPTSTR szModuleName, LPTSTR szMessage) {}; // TODO: replace with sigslot
};
