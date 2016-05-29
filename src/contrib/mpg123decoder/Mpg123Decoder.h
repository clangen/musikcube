#pragma once

#include <core/sdk/IDecoder.h>
#include <core/sdk/IDataStream.h>

#ifdef _MSC_VER /* ehh */
typedef long ssize_t;
#endif

#include <mpg123.h>

class Mpg123Decoder : public musik::core::audio::IDecoder {
    public:
        Mpg123Decoder();
        virtual ~Mpg123Decoder();

        virtual bool Open(musik::core::io::IDataStream *dataStream);
        virtual double SetPosition(double seconds);
        virtual bool GetBuffer(musik::core::audio::IBuffer *buffer);
        virtual void Destroy();

    private:
        bool Feed();

    private:
        musik::core::io::IDataStream *fileStream;
        mpg123_handle *decoder;

        unsigned long cachedLength;
        long sampleRate;
        int channels;
        int sampleSizeBytes;
        int lastMpg123Status;
};
