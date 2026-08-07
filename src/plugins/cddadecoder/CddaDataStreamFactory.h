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
/// @file CddaDataStreamFactory.h
/// @brief Factory that creates CddaDataStream instances for cdda:// URIs.
/// @details Recognizes CDDA track URIs and produces the IDataStream used by
/// the CddaDecoder to read raw audio sectors. Windows-only.
///

#include <musikcore/sdk/IDataStreamFactory.h>
#include <musikcore/sdk/ITagReader.h>
#include <string>

using namespace musik::core::sdk;

/** @brief Creates CDDA data streams.
 *  @details Registers the "cdda://" URI scheme so the decoder SDK can resolve
 *  audio-CD tracks to raw sector streams. */
class CddaDataStreamFactory : public IDataStreamFactory {
    public:
        /** @brief Open flags alias. */
        using OpenFlags = musik::core::sdk::OpenFlags;

        CddaDataStreamFactory();
        ~CddaDataStreamFactory();

        /** @brief Returns whether this factory can open the given URI.
         *  @param uri The URI to check.
         *  @return True if the URI is a cdda:// track. */
        bool CanRead(const char *uri) override;
        /** @brief Opens a stream for the given URI.
         *  @param uri The cdda:// URI to open.
         *  @param flags Open flags.
         *  @return A new CddaDataStream, or null on failure. */
        IDataStream* Open(const char *uri, OpenFlags flags) override;
        /** @brief Destroys the factory instance. */
        void Release() override;
};