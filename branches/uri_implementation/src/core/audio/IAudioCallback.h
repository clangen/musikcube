#pragma once

namespace musik { namespace core { namespace audio {

class IAudioCallback
{
protected: virtual ~IAudioCallback() {};

public: virtual bool GetBuffer(float * pAudioBuffer, unsigned long NumSamples)   = 0;
};

}}} // NS