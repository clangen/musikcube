//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "IKeyHandler.h"
#include "Window.h"
#include "Text.h"

namespace cursespp {
    class ShortcutsWindow :
        public cursespp::Window,
        public cursespp::IKeyHandler
#if (__clang_major__ == 7 && __clang_minor__ == 3)
        , public std::enable_shared_from_this<ShortcutsWindow>
#endif
    {
        public:
            using ChangedCallback = std::function<void (std::string /* key */)>;

            ShortcutsWindow();
            virtual ~ShortcutsWindow();

            void SetAlignment(text::TextAlign alignment);

            void AddShortcut(
                const std::string& key,
                const std::string& description,
                int64_t attrs = -1);

            void SetChangedCallback(ChangedCallback callback);

            void RemoveAll();
            void SetActive(const std::string& key);

            virtual bool KeyPress(const std::string& key) override;
            virtual bool MouseEvent(const IMouseHandler::Event& mouseEvent) override;

        protected:
            virtual void OnRedraw() override;
            virtual void OnFocusChanged(bool focused) override;

        private:
            size_t CalculateLeftPadding();
            int getActiveIndex();

            struct Position {
                int offset{ 0 }, width{ 0 };
            };

            struct Entry {
                Entry(const std::string& key, const std::string& desc, int64_t attrs = -1) {
                    this->key = key;
                    this->description = desc;
                    this->attrs = attrs;
                }

                Position position;
                std::string key;
                std::string description;
                int64_t attrs;
            };

            using EntryList = std::vector<std::shared_ptr<Entry>>;

            ChangedCallback changedCallback;
            EntryList entries;
            std::string activeKey, originalKey;
            text::TextAlign alignment;
    };
}
