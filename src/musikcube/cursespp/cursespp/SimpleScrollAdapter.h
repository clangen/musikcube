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

#include <cursespp/curses_config.h>
#include <cursespp/Colors.h>
#include <cursespp/ScrollAdapterBase.h>
#include <sigslot/sigslot.h>
#include <deque>
#include <map>

namespace cursespp {
    class SimpleScrollAdapter : public ScrollAdapterBase {
        public:
            sigslot::signal1<SimpleScrollAdapter*> Changed;

            SimpleScrollAdapter();
            virtual ~SimpleScrollAdapter();

            virtual void AddEntry(EntryPtr entry);
            virtual void SetMaxEntries(const size_t size = 500);
            virtual void Clear();

            virtual size_t GetEntryCount();
            virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index);

            void SetSelectable(bool selectable);
            void AddEntry(const std::string& entry);
            std::string StringAt(size_t index);

        private:
            typedef std::deque<EntryPtr> EntryList; /* TODO: this is O(n) lookup */
            typedef EntryList::iterator Iterator;
            std::map<size_t, Color> indexToColor;
            EntryList entries;
            size_t maxEntries;
            bool selectable;
    };
}
