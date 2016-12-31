//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include <sigslot/sigslot.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread/mutex.hpp>
#include <core/library/IQuery.h>

namespace musik { namespace core { namespace query {

    typedef enum {
        Prioritize = 1
    } Options;

    class QueryBase : public IQuery, public sigslot::has_slots<> {
        public:
            QueryBase();
            virtual ~QueryBase();

            virtual bool Run(db::Connection &db);
            virtual int GetStatus();
            virtual int GetId();
            virtual int GetOptions();
            virtual void Cancel() { this->cancel = true; }
            virtual bool IsCanceled() { return cancel; }

        protected:
            void SetStatus(int status);
            void SetOptions(int options);

            virtual bool OnRun(db::Connection &db) = 0;
            virtual std::string Name() = 0;

        private:
            unsigned int status;
            unsigned int queryId;
            unsigned int options;
            volatile bool cancel;
            boost::mutex stateMutex;
    };

} } }
