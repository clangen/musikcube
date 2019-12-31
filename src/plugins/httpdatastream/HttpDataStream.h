//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include <core/sdk/IDataStream.h>

#include <string>
#include <curl/curl.h>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace musik::core::sdk;

class FileReadStream;

class HttpDataStream : public IDataStream {
    public:
        using OpenFlags = musik::core::sdk::OpenFlags;

        HttpDataStream();
        ~HttpDataStream();

        virtual void Release();
        virtual bool Open(const char *uri, OpenFlags flags = OpenFlags::Read);
        virtual bool Close();
        virtual bool Readable() { return true; }
        virtual bool Writable() { return false; }
        virtual PositionType Read(void* buffer, PositionType readBytes);
        virtual PositionType Write(void* buffer, PositionType writeBytes) { return 0; }
        virtual bool SetPosition(PositionType position);
        virtual PositionType Position();
        virtual bool Eof();
        virtual long Length();
        virtual bool Seekable();
        virtual const char* Type();
        virtual const char* Uri();
        virtual void Interrupt();
        virtual bool CanPrefetch();

    private:
        enum State {
            Idle,
            Cached,
            Loading,
            Error,
            Finished
        };

        void ThreadProc();

        static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
        static size_t CurlHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata);

        static int CurlTransferCallback(
            void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow);

        std::string uri, type;
        size_t length;
        std::string filename;
        FILE* writeFile;
        CURL* curlEasy;

        volatile long written;
        volatile bool interrupted;
        volatile State state;

        std::mutex stateMutex;
        std::condition_variable startedContition;
        std::shared_ptr<std::thread> downloadThread;
        std::shared_ptr<FileReadStream> reader;
};