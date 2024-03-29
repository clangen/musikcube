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

#include <musikcore/sdk/IIndexerSource.h>
#include "CddaDataModel.h"

#include <functional>
#include <set>

class CddaIndexerSource :
    public musik::core::sdk::IIndexerSource,
    public CddaDataModel::EventListener
{
    public:
        CddaIndexerSource();
        ~CddaIndexerSource();

        /* IIndexerSource */
        void Release() override;
        void OnBeforeScan() override;
        void OnAfterScan() override;
        int SourceId() override;

        musik::core::sdk::ScanResult Scan(
            musik::core::sdk::IIndexerWriter* indexer,
            const char** indexerPaths,
            unsigned indexerPathsCount) override;

        void ScanTrack(
            musik::core::sdk::IIndexerWriter* indexer,
            musik::core::sdk::ITagStore* tagStore,
            const char* externalId) override;

        void Interrupt() override;
        bool HasStableIds() noexcept override { return true; }
        bool NeedsTrackScan() noexcept override { return true; }

        /* CddaDataModel::EventListener */
        void OnAudioDiscInsertedOrRemoved() override;

    private:
        void RefreshModel();

        CddaDataModel& model;
        std::set<std::string> discIds;
        std::vector<CddaDataModel::AudioDiscPtr> discs;
};