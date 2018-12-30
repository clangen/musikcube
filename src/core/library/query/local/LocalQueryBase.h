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

#include <core/config.h>
#include <core/library/IQuery.h>
#include <core/db/Connection.h>

#include <sigslot/sigslot.h>

#include <mutex>
#include <atomic>

namespace musik { namespace core { namespace db {

    class LocalQueryBase : public IQuery, public sigslot::has_slots<> {
        public:
            LocalQueryBase()
            : status(0)
            , options(0)
            , queryId(nextId())
            , cancel(false) {
            }

            virtual ~LocalQueryBase() {
            }

            bool Run(musik::core::db::Connection &db) {
                this->SetStatus(Running);
                try {
                    if (this->IsCanceled()) {
                        this->SetStatus(Canceled);
                        return true;
                    }
                    else if (OnRun(db)) {
                        this->SetStatus(Finished);
                        return true;
                    }
                }
                catch (...) {
                }

                this->SetStatus(Failed);
                return false;
            }

            virtual int GetStatus() {
                std::unique_lock<std::mutex> lock(this->stateMutex);
                return this->status;
            }

            virtual int GetId() {
                return this->queryId;
            }

            virtual int GetOptions() {
                std::unique_lock<std::mutex> lock(this->stateMutex);
                return this->options;
            }

            virtual void Cancel() {
                this->cancel = true;
            }

            virtual bool IsCanceled() {
                return cancel;
            }

            virtual std::string Name() = 0;

        protected:
            void SetStatus(int status) {
                std::unique_lock<std::mutex> lock(this->stateMutex);
                this->status = status;
            }

            void SetOptions(int options) {
                std::unique_lock<std::mutex> lock(this->stateMutex);
                this->options = options;
            }

            virtual bool OnRun(musik::core::db::Connection& db) = 0;

        private:
            static int nextId() {
                static std::atomic<int> next(0);
                return ++next;
            }

            unsigned int status;
            unsigned int queryId;
            unsigned int options;
            volatile bool cancel;
            std::mutex stateMutex;
    };

} } }
