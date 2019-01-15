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

#include <string>
#include <vector>
#include <sigslot/sigslot.h>
#include <functional>

#include <core/library/IIndexer.h>
#include <core/library/IQuery.h>
#include <core/runtime/IMessageQueue.h>

namespace musik { namespace core {

    class ILibrary {
        public:
            using QueryPtr = std::shared_ptr<musik::core::db::IQuery>;
            using Callback = std::function<void(QueryPtr)>;

            sigslot::signal1<musik::core::db::IQuery*> QueryCompleted;

            enum QueryFlag {
                QuerySynchronous = 1
            };

            virtual ~ILibrary() { }

            virtual int Enqueue(
                QueryPtr query,
                unsigned int options = 0,
                Callback = Callback()) = 0;

            virtual IIndexer *Indexer() = 0;
            virtual int Id() = 0;
            virtual const std::string& Name() = 0;
            virtual void SetMessageQueue(musik::core::runtime::IMessageQueue& queue) = 0;
            virtual musik::core::runtime::IMessageQueue& GetMessageQueue() = 0;
            virtual void Close() = 0;
    };

    typedef std::shared_ptr<ILibrary> ILibraryPtr;

} }
