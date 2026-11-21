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

#include <stdafx.h>
#include <musikcore/log.hpp>

#include "BrowseOverlays.h"
#include <app/util/MagicConstants.h>
#include <musikcore/indexer.hpp>
#include <musikcore/library.hpp>
#include <app/util/Locale.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/App.h>
#include <set>

using namespace cursespp;
using namespace musik::cube;
using namespace musik::core;

static const std::set<std::string> BLACKLIST = { "bitrate", "channels", "lyrics", "path_id", "directory" };
static std::string lastSelectedCategory, lastSelectedDirectory;

static void showNoPathsError() {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle(_TSTR("browse_no_paths_overlay_error_title"))
        .SetMessage(_TSTR("browse_no_paths_overlay_error_message"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

    App::Overlays().Push(dialog);
}

void BrowseOverlays::ShowCategoryChooser(api::Context& musikcore, AsyncCallbackScope::Token token,
    std::function<void(std::string, std::string)> callback)
{
    try {
        api::ListCategoryFields(musikcore,
            [token, callback = std::move(callback)](mc_result status, std::vector<std::string> fields, std::string_view) {
                if (status != MC_OK || !token.IsCurrent()) {
                    return;
                }

                std::vector<std::pair<std::string, std::string>> choices;
                for (auto& field : fields) {
                    if (!BLACKLIST.count(field)) {
                        choices.emplace_back(std::move(field), "");
                    }
                }
                choices.emplace_back(_TSTR("browse_title_directory"), MagicConstants::DirectoryCategoryType);
                std::sort(choices.begin(), choices.end());

                auto adapter = std::make_shared<SimpleScrollAdapter>();
                adapter->SetSelectable(true);
                size_t selected = 0;
                for (const auto& choice : choices) {
                    if (choice.first == lastSelectedCategory) {
                        selected = adapter->GetEntryCount();
                    }
                    adapter->AddEntry(choice.first);
                }

                auto dialog = std::make_shared<ListOverlay>();
                dialog->SetAdapter(adapter)
                    .SetTitle(_TSTR("browse_categories_title"))
                    .SetWidth(_DIMEN("browse_categories_overlay_width", 35))
                    .SetSelectedIndex(selected)
                    .SetItemSelectedCallback(
                        [token, callback, choices = std::move(choices)](ListOverlay*, IScrollAdapterPtr, size_t index) {
                            if (token.IsCurrent() && index < choices.size()) {
                                const auto& choice = choices[index];
                                lastSelectedCategory = choice.first;
                                callback(choice.first, choice.second);
                            }
                        });
                App::Overlays().Push(dialog);
            });
    }
    catch (const api::Error& error) {
        musik::core::api::WriteLog(MC_LOG_ERROR, "BrowseOverlays", error.what());
    }
}

void BrowseOverlays::ShowDirectoryChooser(
    api::Context& musikcore, AsyncCallbackScope::Token token,
    std::function<void(std::string)> callback)
{
    using StringList = std::vector<std::string>;
    using Adapter = cursespp::SimpleScrollAdapter;

    const auto paths = std::make_shared<StringList>(musikcore.GetLibraryStatus().kind == MC_LIBRARY_LOCAL
        ? api::Indexer(musikcore).Paths()
        : StringList {});

    if (paths->size() == 0) {
        showNoPathsError();
    }
    else if (paths->size() == 1) {
        if (callback) {
            callback(paths->at(0));
        }
    }
    else {
        std::shared_ptr<Adapter> adapter(new Adapter());
        adapter->SetSelectable(true);

        size_t selectedIndex = 0;
        for (size_t i = 0; i < paths->size(); i++) {
            auto dir = paths->at(i);
            adapter->AddEntry(dir);
            if (dir == lastSelectedDirectory) {
                selectedIndex = i;
            }
        }

        std::shared_ptr<ListOverlay> dialog(new ListOverlay());

        dialog->SetAdapter(adapter)
            .SetTitle(_TSTR("browse_pick_path_overlay_title"))
            .SetWidthPercent(80)
            .SetSelectedIndex(selectedIndex)
            .SetItemSelectedCallback(
                [paths, callback, token]
                (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                    if (token.IsCurrent() && callback && index < paths->size()) {
                        lastSelectedDirectory = paths->at(index);
                        callback(lastSelectedDirectory);
                    }
                });

        cursespp::App::Overlays().Push(dialog);
    }
}

void BrowseOverlays::ShowIndexer(api::Context& musikcore) {
    using Adapter = cursespp::SimpleScrollAdapter;
    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("indexer_overlay_reindex"));
    adapter->AddEntry(_TSTR("indexer_overlay_rebuild"));
    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("indexer_overlay_title"))
        .SetWidth(_DIMEN("indexer_overlay_width", 28))
        .SetSelectedIndex(0)
        .SetItemSelectedCallback(
        [&musikcore](ListOverlay*, IScrollAdapterPtr, size_t index) {
            if (musikcore.GetLibraryStatus().kind != MC_LIBRARY_LOCAL) {
                return;
            }

            switch (index) {
                case 0: api::Indexer(musikcore).Schedule(MC_INDEXER_SCAN_LOCAL); break;
                case 1: api::Indexer(musikcore).Schedule(MC_INDEXER_REBUILD); break;
            }
        });

    cursespp::App::Overlays().Push(dialog);
}