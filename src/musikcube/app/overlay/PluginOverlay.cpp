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
#include "PluginOverlay.h"

#include <musikcore/plugins.hpp>
#include <common/Utf8.h>

#include <cursespp/App.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/SchemaOverlay.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/SingleLineEntry.h>

#include <algorithm>
#include <cctype>

using namespace musik::core;
using namespace musik::cube;
using namespace cursespp;

namespace {
    void showNoSchemaDialog(const std::string& name) {
        auto dialog = std::make_shared<DialogOverlay>();
        auto message = _TSTR("settings_no_plugin_config_message");
        u8replace(message, "{{name}}", name);

        dialog->SetTitle(_TSTR("settings_no_plugin_config_title"))
            .SetMessage(message)
            .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));
        App::Overlays().Push(dialog);
    }

    std::string sortKey(std::string name) {
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return std::tolower(c);
        });
        return name;
    }

    class PluginListAdapter final : public ScrollAdapterBase {
    public:
        PluginListAdapter() : plugins(api::ListPlugins()) {
            std::sort(this->plugins.begin(), this->plugins.end(), [](const auto& left, const auto& right) {
                return sortKey(left.name) < sortKey(right.name);
            });
        }

        void Configure(size_t index) {
            const auto& plugin = this->plugins.at(index);
            auto fields = api::ReadPluginSettings(plugin.id);
            if (!fields) {
                showNoSchemaDialog(plugin.name);
                return;
            }

            auto title = _TSTR("settings_configure_plugin_title");
            u8replace(title, "{{name}}", plugin.name);
            SchemaOverlay::Show(title, std::move(*fields),
                [id = plugin.id](std::string_view key, const api::PreferenceValue& value) {
                    return api::WritePluginSetting(id, key, value);
                });
        }

        void ToggleEnabled(size_t index) {
            auto& plugin = this->plugins.at(index);
            if (api::SetPluginEnabled(plugin.filename, !plugin.enabled) == MC_OK) {
                plugin.enabled = !plugin.enabled;
            }
        }

        size_t GetEntryCount() override {
            return this->plugins.size();
        }

        EntryPtr GetEntry(ScrollableWindow* window, size_t index) override {
            const auto& plugin = this->plugins.at(index);
            const auto display = (plugin.enabled ? " [x] " : " [ ] ") +
                plugin.name + " (" + plugin.filename + ") [v" + plugin.version + "]";
            auto entry = std::make_shared<SingleLineEntry>(text::Ellipsize(display, this->GetWidth()));
            entry->SetAttrs(index == window->GetScrollPosition().logicalIndex
                ? Color::ListItemHighlighted
                : Color::Default);
            return entry;
        }

    private:
        std::vector<api::PluginInfo> plugins;
    };
}

void PluginOverlay::Show() {
    std::shared_ptr<PluginListAdapter> pluginAdapter(new PluginListAdapter());
    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(pluginAdapter)
        .SetTitle(_TSTR("plugin_overlay_title"))
        .SetWidthPercent(80)
        .SetAutoDismiss(false)
        .SetKeyInterceptorCallback(
            [pluginAdapter](ListOverlay* overlay, std::string key) -> bool {
                if (key == " ") {
                    size_t index = overlay->GetSelectedIndex();
                    if (index != ListWindow::NO_SELECTION) {
                        pluginAdapter->ToggleEnabled(index);
                        overlay->RefreshAdapter();
                        return true;
                    }
                }
                return false;
            })
        .SetItemSelectedCallback(
            [pluginAdapter](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                pluginAdapter->Configure(index);
            });

    cursespp::App::Overlays().Push(dialog);
}
