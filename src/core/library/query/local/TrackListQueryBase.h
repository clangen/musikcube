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

#pragma once

#include <core/library/query/local/LocalQueryBase.h>
#include <core/db/Connection.h>
#include <core/library/track/Track.h>
#include <core/library/track/TrackList.h>

namespace musik { namespace core { namespace db { namespace local {

    class TrackListQueryBase : public musik::core::db::LocalQueryBase {
        public:
            typedef std::shared_ptr<musik::core::TrackList> Result;
            typedef std::shared_ptr<std::set<size_t> > Headers;

            TrackListQueryBase() {
                this->limit = -1;
                this->offset = 0;
            }

            virtual ~TrackListQueryBase() { };
            virtual std::string Name() = 0;
            virtual Result GetResult() = 0;
            virtual Headers GetHeaders() = 0;
            virtual size_t GetQueryHash() = 0;

            virtual void SetLimitAndOffset(int limit, int offset = 0) {
                this->limit = limit;
                this->offset = offset;
            }

            virtual musik::core::sdk::ITrackList* GetSdkResult() {
                return new WrappedTrackList(GetResult());
            }

        protected:
            std::string GetLimitAndOffset() {
                if (this->limit > 0 && this->offset >= 0) {
                    return u8fmt("LIMIT %d OFFSET %d", this->limit, this->offset);
                }
                return "";
            }

        private:
            int limit, offset;

            class WrappedTrackList : public musik::core::sdk::ITrackList {
                public:
                    WrappedTrackList(Result wrapped) {
                        this->wrapped = wrapped;
                    }

                    virtual void Release() {
                        delete this;
                    }

                    virtual size_t Count() const {
                        return this->wrapped->Count();
                    }

                    virtual int64_t GetId(size_t index) const {
                        return this->wrapped->GetId(index);
                    }

                    virtual int IndexOf(int64_t id) const {
                        return this->wrapped->IndexOf(id);
                    }

                    virtual musik::core::sdk::ITrack* GetTrack(size_t index) const {
                        return this->wrapped->GetTrack(index);
                    }

                private:
                    Result wrapped;
            };
    };

} } } }
