//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2020 musikcube team
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

#include <musikcore/sdk/IDataStream.h>

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
        static const std::string kRemoteTrackHost;

        using OpenFlags = musik::core::sdk::OpenFlags;

        HttpDataStream();
        virtual ~HttpDataStream();

        void Release() override;
        bool Open(const char *rawUri, OpenFlags flags = OpenFlags::Read) override;
        bool Close() override;
        bool Readable() override  { return true; }
        bool Writable() override  { return false; }
        PositionType Read(void* buffer, PositionType readBytes) override;
        PositionType Write(void* buffer, PositionType writeBytes) override  { return 0; }
        bool SetPosition(PositionType position) override;
        PositionType Position() override;
        bool Eof() override;
        long Length() override;
        bool Seekable() override;
        const char* Type() override;
        const char* Uri() override;
        void Interrupt() override;
        bool CanPrefetch() override;

    private:
        enum class State {
            NotStarted,
            Conflict,
            Cached,
            Downloading,
            Retrying,
            Downloaded,
            Error,
        };

        void ThreadProc();
        void ResetFileHandles();

        static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
        static size_t CurlReadHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata);
        static int CurlTransferCallback(void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow);

        std::string originalUri, httpUri, type;
        size_t length;
        std::string filename;
        FILE* writeFile;
        CURL* curlEasy;
        curl_slist *curlHeaders{ nullptr };

        volatile long written, totalWritten;
        volatile bool interrupted;
        volatile State state;

        std::mutex stateMutex;
        std::condition_variable startedContition;
        std::shared_ptr<std::thread> downloadThread;
        std::shared_ptr<FileReadStream> reader;
        int precacheSizeBytes, chunkSizeBytes, maxCacheFiles;
        int64_t instanceId;
};