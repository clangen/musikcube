//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
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

///

/// @file HttpDataStream.h
/// @brief IDataStream that streams remote audio over HTTP using libcurl.
/// @details Downloads the requested URL to a local cache file in a background
/// thread while playback reads from the partially downloaded file, so streaming
/// can begin before the transfer completes. Handles retries, progress reporting
/// and an LRU disk cache that bounds the number of cached downloads.

#include <musikcore/sdk/IDataStream.h>

#include <string>
#include <curl/curl.h>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>

using namespace musik::core::sdk;

class FileReadStream;

/** @brief Streams audio data from a remote HTTP URL.
 *  @details The stream opens a remote URI, immediately opens a local cache file
 *  for writing, and starts a download thread that fills the cache while the
 *  caller reads from it. Seeking operates on the local file once the download
 *  has progressed far enough; otherwise reads block until data is available.
 */
class HttpDataStream : public IDataStream {
    public:
        /** @brief Host prefix used to flag remote-track URIs. */
        static const std::string kRemoteTrackHost;

        /** @brief Open flags alias. */
        using OpenFlags = musik::core::sdk::OpenFlags;

        /** @brief Constructs a stream in the NotStarted state. */
        HttpDataStream();
        /** @brief Destroys the stream, aborting any active download. */
        virtual ~HttpDataStream();

        /** @brief Destroys the stream instance. */
        void Release() override;
        /** @brief Opens a remote URI and starts the background download.
         *  @param rawUri The http/https URI to stream.
         *  @param flags Open flags.
         *  @return True if the stream opened. */
        bool Open(const char *rawUri, OpenFlags flags = OpenFlags::Read) override;
        /** @brief Closes the stream and stops the download thread.
         *  @return True on success. */
        bool Close() override;
        /** @brief Returns whether the stream can be read.
         *  @return Always returns true. */
        bool Readable() override  { return true; }
        /** @brief Returns whether the stream can be written.
         *  @return Always returns false. */
        bool Writable() override  { return false; }
        /** @brief Reads bytes from the downloaded data.
         *  @param buffer Destination buffer.
         *  @param readBytes Number of bytes to read.
         *  @return Number of bytes read. */
        PositionType Read(void* buffer, PositionType readBytes) override;
        /** @brief Writing is not supported.
         *  @return Always returns 0. */
        PositionType Write(void* buffer, PositionType writeBytes) override  { return 0; }
        /** @brief Seeks to a byte offset in the downloaded data.
         *  @param position Target byte offset.
         *  @return True if the seek was allowed. */
        bool SetPosition(PositionType position) override;
        /** @brief Returns the current byte offset.
         *  @return Current position in bytes. */
        PositionType Position() override;
        /** @brief Returns whether end of data has been reached.
         *  @return True at end of the download. */
        bool Eof() override;
        /** @brief Returns the total download length in bytes.
         *  @return Length in bytes, or -1 if unknown. */
        long Length() override;
        /** @brief Returns whether the stream supports seeking.
         *  @return True once the download length is known. */
        bool Seekable() override;
        /** @brief Returns the stream type.
         *  @return The stream type string. */
        const char* Type() override;
        /** @brief Returns the stream URI.
         *  @return The original URI this stream was opened with. */
        const char* Uri() override;
        /** @brief Interrupts the background download.
         *  @details Causes reads to return and the download thread to exit. */
        void Interrupt() override;
        /** @brief Returns whether the stream can be prefetched.
         *  @return True if the download can be prefetched. */
        bool CanPrefetch() override;

    private:
        /** @brief Lifecycle states of the underlying download. */
        enum class State {
            /** @brief The download has not started. */
            NotStarted,
            /** @brief The remote server rejected the request. */
            Conflict,
            /** @brief The download is served from the disk cache. */
            Cached,
            /** @brief The download is in progress. */
            Downloading,
            /** @brief A failed transfer is being retried. */
            Retrying,
            /** @brief The download was aborted. */
            Aborted,
            /** @brief The download finished successfully. */
            Downloaded,
            /** @brief The download failed with an error. */
            Error,
        };

        /** @brief Body of the background download thread. */
        void ThreadProc();
        /** @brief Closes and clears the local cache file handles. */
        void ResetFileHandles();

        /** @brief libcurl write callback storing received bytes.
         *  @return Number of bytes consumed. */
        static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);
        /** @brief libcurl header callback inspecting response headers.
         *  @return Number of bytes consumed. */
        static size_t CurlReadHeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata);
        /** @brief libcurl progress callback reporting transfer progress.
         *  @return Non-zero to abort the transfer. */
        static int CurlTransferCallback(void *ptr, curl_off_t downTotal, curl_off_t downNow, curl_off_t upTotal, curl_off_t upNow);

        #if LIBCURL_VERSION_NUM < 0x072000
        /** @brief Legacy double-typed progress callback for older libcurl. */
        static int LegacyCurlTransferCallback(void *ptr, double downTotal, double downNow, double upTotal, double upNow);
        #endif

        /** @brief Original URI, resolved HTTP URI and stream type. */
        std::string originalUri, httpUri, type;
        /** @brief Known content length in bytes. */
        size_t length;
        /** @brief Path of the local cache file. */
        std::string filename;
        /** @brief Handle of the cache file being written. */
        FILE* writeFile;
        /** @brief libcurl easy handle for the transfer. */
        CURL* curlEasy;
        /** @brief Request headers applied to the transfer. */
        curl_slist *curlHeaders{ nullptr };

        /** @brief Bytes written and total bytes written. */
        std::atomic<long> written, totalWritten;
        /** @brief True once the transfer was interrupted. */
        std::atomic<bool> interrupted;
        /** @brief Current download state. */
        std::atomic<State> state;

        /** @brief Guards the state machine. */
        std::mutex stateMutex;
        /** @brief Signals when the download thread starts. */
        std::condition_variable startedContition;
        /** @brief The background download thread. */
        std::shared_ptr<std::thread> downloadThread;
        /** @brief Local reader used once the cache file is complete. */
        std::shared_ptr<FileReadStream> reader;
        /** @brief Pre-cache, chunk and cache-size limits. */
        int precacheSizeBytes, chunkSizeBytes, maxCacheFiles;
        /** @brief Unique id for this stream instance. */
        int64_t instanceId;
};