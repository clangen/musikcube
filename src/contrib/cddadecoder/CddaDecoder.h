#pragma once

#include <core/sdk/IDecoder.h>
#include "CddaDataStream.h"

#include "ntddcdrm.h"
#include "devioctl.h"

using namespace musik::core::audio;
using namespace musik::core::io;

class CddaDecoder : public IDecoder
{
private:

public:
    CddaDecoder();
    ~CddaDecoder();

    bool Open(IDataStream* data);
    void Destroy();
    double SetPosition(double seconds);
    bool GetBuffer(IBuffer *buffer);

private:
    CddaDataStream* data;
    BYTE* buffer;
};
