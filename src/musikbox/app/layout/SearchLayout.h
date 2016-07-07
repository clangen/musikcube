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

#include <cursespp/LayoutBase.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <app/window/TransportWindow.h>
#include <app/service/PlaybackService.h>

#include <core/library/ILibrary.h>

#include <sigslot/sigslot.h>

namespace musik {
    namespace box {
        class SearchLayout :
            public cursespp::LayoutBase,
#if (__clang_major__ == 7 && __clang_minor__ == 3)
            public std::enable_shared_from_this<SearchLayout>,
#endif
            public sigslot::has_slots<>
        {
            public:
                sigslot::signal3<SearchLayout*, std::string, DBID> SearchResultSelected;

                SearchLayout(
                    PlaybackService& playback,
                    musik::core::LibraryPtr library);

                virtual ~SearchLayout();

                virtual void Layout();
                virtual void OnVisibilityChanged(bool visible);
                virtual bool KeyPress(const std::string& key);

                void FocusInput();

            private:
                void InitializeWindows(PlaybackService& playback);
                void Requery();

                void OnInputChanged(
                    cursespp::TextInput* sender, 
                    std::string value);

                musik::core::LibraryPtr library;
                std::shared_ptr<cursespp::TextLabel> albumsLabel;
                std::shared_ptr<CategoryListView> albums;
                std::shared_ptr<cursespp::TextLabel> artistsLabel;
                std::shared_ptr<CategoryListView> artists;
                std::shared_ptr<cursespp::TextLabel> genresLabel;
                std::shared_ptr<CategoryListView> genres;
                std::shared_ptr<cursespp::TextInput> input;
        };
    }
}
