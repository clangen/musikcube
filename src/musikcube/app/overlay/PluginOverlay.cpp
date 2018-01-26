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

#include "stdafx.h"

#include "PluginOverlay.h"

#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>
#include <core/plugin/PluginFactory.h>

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>

using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

static const std::string unchecked = "[ ]";
static const std::string checked = "[x]";

struct PluginInfo {
    std::string name;
    std::string fn;
    bool enabled;
};

using PluginInfoPtr = std::shared_ptr<PluginInfo>;
using PluginList = std::vector<PluginInfoPtr>;
using SinglePtr = std::shared_ptr<SingleLineEntry>;

class PluginAdapter : public ScrollAdapterBase {
    public:
        PluginAdapter() {
            this->prefs = Preferences::ForComponent(prefs::components::Plugins);
            this->Refresh();
        }

        virtual ~PluginAdapter() {
            this->prefs->Save();
        }

        void toggleEnabled(size_t index) {
            PluginInfoPtr plugin = this->plugins.at(index);
            plugin->enabled = !plugin->enabled;
            this->prefs->SetBool(plugin->fn, plugin->enabled);
        }

        virtual size_t GetEntryCount() {
            return plugins.size();
        }

        virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) {
            PluginInfoPtr plugin = plugins.at(index);

            std::string display =
                " " +
                (plugin->enabled ? checked : unchecked) + " " +
                plugin->name + " (" + plugin->fn + ")";

            SinglePtr result = SinglePtr(new SingleLineEntry(text::Ellipsize(display, this->GetWidth())));

            result->SetAttrs(CURSESPP_DEFAULT_COLOR);

            if (index == window->GetScrollPosition().logicalIndex) {
                result->SetAttrs(COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM));
            }

            return result;
        }

    private:
        static std::string SortKey(const std::string& input) {
            std::string name = input;
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            return name;
        }

        void Refresh() {
            plugins.clear();

            PluginList& plugins = this->plugins;
            auto prefs = this->prefs;

            using Deleter = PluginFactory::NullDeleter<IPlugin>;
            using Plugin = std::shared_ptr<IPlugin>;

            PluginFactory::Instance().QueryInterface<IPlugin, Deleter>(
                "GetPlugin",
                [&plugins, prefs](Plugin plugin, const std::string& fn) {
                    PluginInfoPtr info(new PluginInfo());
                    info->name = plugin->Name();
                    info->fn = boost::filesystem::path(fn).filename().string();
                    info->enabled = prefs->GetBool(info->fn, true);
                    plugins.push_back(info);
                });

            std::sort(plugins.begin(), plugins.end(), [](PluginInfoPtr p1, PluginInfoPtr p2) -> bool {
                return SortKey(p1->name) < SortKey(p2->name);
            });
        }

        std::shared_ptr<Preferences> prefs;
        PluginList plugins;
};

PluginOverlay::PluginOverlay() {

}

void PluginOverlay::Show() {
    std::shared_ptr<PluginAdapter> pluginAdapter(new PluginAdapter());
    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(pluginAdapter)
        .SetTitle(_TSTR("plugin_overlay_title"))
        .SetWidthPercent(80)
        .SetAutoDismiss(false)
        .SetItemSelectedCallback(
            [pluginAdapter](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                pluginAdapter->toggleEnabled(index);
                overlay->RefreshAdapter();
            });

    cursespp::App::Overlays().Push(dialog);
}
