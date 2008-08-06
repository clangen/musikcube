#include "pch.hpp"

#include <core/config.h>
#include <core/audio/AudioPacketizer.h>

using namespace musik::core::audio;

AudioPacketizer::AudioPacketizer(void) :
    m_pMainBuffer(NULL),        
    m_PacketSize(0),
    m_ReadBlock(0),
    m_WriteBlock(0),
    m_Finished(false),
    m_OverflowSize(0),
    m_pOverflowBuffer(0)
{
}

AudioPacketizer::~AudioPacketizer(void)
{
    SetPacketSize(0);
}

bool AudioPacketizer::SetPacketSize(unsigned long Size)
{
    if(Size != m_PacketSize)
    {
        if(m_pMainBuffer)
        {
            delete [] m_pMainBuffer;
            m_pMainBuffer = NULL;
        }

        if (m_pOverflowBuffer)
        {
            delete[] m_pOverflowBuffer;
            m_pOverflowBuffer = 0;
        }

        if(Size)
        {
            m_pMainBuffer    = new float[Size * 16];
            m_PacketSize    = Size;

            for(int x=0; x<16; x++)
            {
                m_pBufferArray[x] = &m_pMainBuffer[x*Size];
            }

            m_pOverflowBuffer = new float[Size];
        }
    }

    m_ReadBlock        = 0;
    m_WriteBlock    = 0;
    m_OverflowSize    = 0;
    m_Finished        = false;

    return(true);
}

bool AudioPacketizer::WriteData(float * Data, unsigned long Samples)
{
    unsigned long SamplesLeft = Samples;

    if(Samples >= m_PacketSize*16)
        return(false);

    if(m_OverflowSize)
    {
        if((m_OverflowSize+Samples) >= m_PacketSize) // if overflow and new samples can create a packet make one
        {
            CopyFloat(m_pBufferArray[m_WriteBlock & 0x0f],                    m_pOverflowBuffer,    m_OverflowSize);
            CopyFloat(&m_pBufferArray[m_WriteBlock & 0x0f][m_OverflowSize],    Data,                m_PacketSize - m_OverflowSize);

            SamplesLeft -= (m_PacketSize - m_OverflowSize);
            Data = &Data[m_PacketSize - m_OverflowSize];
            m_WriteBlock++;
            m_OverflowSize = 0;
        }
    }

    while(SamplesLeft >= m_PacketSize)    // if the samples we have left can make packets
    {
        CopyFloat(m_pBufferArray[m_WriteBlock & 0x0f], Data, m_PacketSize);

        SamplesLeft -= (m_PacketSize);
        Data = &Data[m_PacketSize];
        m_WriteBlock++;
    }
    
    if(SamplesLeft)
    {
        CopyFloat(&m_pOverflowBuffer[m_OverflowSize], Data, SamplesLeft);
        m_OverflowSize += SamplesLeft;
    }

    return(true);
}

bool AudioPacketizer::Finished(void)
{
    float * pData = m_pBufferArray[m_WriteBlock & 0x0f];
    for(int x=0; x<m_PacketSize; x++)
    {
        pData[x] = 0.0f;
    }

    if(m_OverflowSize)
    {
        CopyFloat(pData, m_pOverflowBuffer, m_OverflowSize);
        m_OverflowSize = 0;
    }

    m_WriteBlock++;

    m_Finished = true;

    return true;
}

bool    AudioPacketizer::IsBufferAvailable(void)
{
    if(m_ReadBlock >= m_WriteBlock)
    {
        return(false);
    }

    return true;
}

bool    AudioPacketizer::GetBuffer(float * ToHere)
{
    CopyFloat(ToHere, m_pBufferArray[m_ReadBlock & 0x0f], m_PacketSize);

    return true;
}

bool AudioPacketizer::Advance(unsigned long Packets)
{
    m_ReadBlock += Packets;
    return true;
}

bool AudioPacketizer::Rewind(unsigned long Packets)
{
    m_ReadBlock -= Packets;

    if(m_ReadBlock<0)
        m_ReadBlock = 0;

    return true;
}
bool AudioPacketizer::Reset(void)
{
    m_ReadBlock        = 0;
    m_WriteBlock    = 0;
    m_OverflowSize    = 0;
    return true;
}
