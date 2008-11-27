#pragma once

namespace musik { namespace core { namespace audio {

class AudioPacketizer
{
protected:

    float *            m_pMainBuffer;
    float *            m_pBufferArray[16];


    unsigned long    m_PacketSize;

    long            m_ReadBlock;
    long            m_WriteBlock;

    float *            m_pOverflowBuffer;
    unsigned long    m_OverflowSize;

    bool            m_Finished;

public:
    AudioPacketizer(void);
    ~AudioPacketizer(void);

    bool IsFinished(void)
    {
        return(m_Finished);
    }

    bool SetPacketSize(unsigned long Size);

    bool WriteData(float * Data, unsigned long Samples);
    bool Finished(void);

    bool IsBufferAvailable(void);
    bool GetBuffer(float * ToHere);

    bool Advance(unsigned long Packets = 1);
    bool Rewind(unsigned long Packets);
    bool Reset(void);
};

}}} // NS