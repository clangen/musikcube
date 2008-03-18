#include "StdAfx.h"
#include "APEdecoder.h"


APEDecoder::APEDecoder(void)
{
}

APEDecoder::~APEDecoder(void)
{
}

bool APEDecoder::Open(const utfchar* source)
{
	int nRetVal;
    MACDecompressor = CreateIAPEDecompress(source, &nRetVal);
	if (nRetVal != ERROR_SUCCESS)
	{
		return false;
	}
	return true;
}

bool APEDecoder::Close()
{
	delete MACDecompressor;
	return true;
}

void		APEDecoder::Destroy(void)
{
	Close();
	delete this;
}

bool		APEDecoder::GetFormat(unsigned long * SampleRate, unsigned long * Channels)
{
	*SampleRate = (unsigned long)MACDecompressor->GetInfo(APE_INFO_SAMPLE_RATE);
	*Channels	= (unsigned long)MACDecompressor->GetInfo(APE_INFO_CHANNELS);

	return true;
}

bool		APEDecoder::GetLength(unsigned long * MS)
{
	*MS = (unsigned long)MACDecompressor->GetInfo(APE_INFO_LENGTH_MS);
	return true;
}

bool		APEDecoder::SetPosition(unsigned long * MS)
{
	int seekBlocks, totalBlocks, result;

	seekBlocks = ((unsigned long)*MS/1000) * MACDecompressor->GetInfo(APE_INFO_SAMPLE_RATE);
	totalBlocks = MACDecompressor->GetInfo(APE_DECOMPRESS_TOTAL_BLOCKS) - 1024;
	if (seekBlocks > totalBlocks) {
		seekBlocks = totalBlocks;
	}
	else if (seekBlocks <= 0) {
		seekBlocks = 0;
	}
	result = MACDecompressor->Seek(seekBlocks);
	if( result != ERROR_SUCCESS ) {
		return false;
	}

	return true;
}

bool		APEDecoder::SetState(unsigned long State)
{
	return true;
}

bool		APEDecoder::GetBuffer(float ** ppBuffer, unsigned long * NumSamples)
{
	int result;
	int nChannels		= MACDecompressor->GetInfo(APE_INFO_CHANNELS);
	int nBlockAlign		= MACDecompressor->GetInfo(APE_INFO_BLOCK_ALIGN);
	int nBitsPerSample	= MACDecompressor->GetInfo(APE_INFO_BITS_PER_SAMPLE);
	int nBytesPerSample	= MACDecompressor->GetInfo(APE_INFO_BYTES_PER_SAMPLE);

	static char * pRawData = NULL;
	
	if(!pRawData)
	{
		pRawData = new char [1024 * nBlockAlign];
	}
	
	int nBlocksRetrieved;
	result = MACDecompressor->GetData (pRawData, 1024, &nBlocksRetrieved);
	if((nBlocksRetrieved == 0) || (result != ERROR_SUCCESS))
		return false;

	if(nBytesPerSample == 2)  //16bit
	{
		short * pData = (short *)pRawData;
		float * pBuffer = m_Buffer;
		
		for(int x=0; x<nBlocksRetrieved*nChannels; x++)
		{
			*pBuffer = (*pData) / 32767.0f;	
			pData ++;
			pBuffer++;
		}
	}
	else if (nBytesPerSample == 3) //24bit
	{
		char * pData = (char *)pRawData;
		float *pBuffer = m_Buffer;
		int sourceIndex = 0;
		int padding = nBlockAlign - nChannels * (nBitsPerSample >> 3);
		int32 workingValue;

		for ( unsigned int i = 0; i < 1024; i++ ) {
			for ( unsigned int j = 0; j < nChannels; j++ ) {
				workingValue = 0;
				workingValue |= ((int32)pRawData[sourceIndex++]) << 8 & 0xFF00;
				workingValue |= ((int32)pRawData[sourceIndex++]) << 16 & 0xFF0000;
				workingValue |= ((int32)pRawData[sourceIndex++]) << 24 & 0xFF000000;

				*pBuffer = ((float)workingValue) * QUANTFACTOR;

				pBuffer ++;
				pData += 3;
			}
			sourceIndex += padding;
		}
	}
	else if(nBytesPerSample == 4)  //32bit?
	{
		int * pData = (int *)pRawData;
		float * pBuffer = m_Buffer;
		
		float divider = (float)((1<<nBitsPerSample)-1);
		
		for(int x=0; x<nBlocksRetrieved*nChannels;x++)
		{
			*pBuffer = (*pData) / divider;	
			pData ++;
			pBuffer++;
		}
	}

	*ppBuffer = m_Buffer;
	*NumSamples = nBlocksRetrieved * nChannels;
	return true;
}
