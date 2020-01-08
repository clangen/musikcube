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

#include <stdafx.h>

#include "PluginOverlay.h"

#include <core/support/Common.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>
#include <core/plugin/PluginFactory.h>
#include <core/sdk/ISchema.h>

#include <cursespp/App.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/SchemaOverlay.h>

using namespace musik::core;
using namespace musik::core::sdk;
using namespace musik::cube;
using namespace cursespp;

static const std::string unchecked = "[ ]";
static const std::string checked = "[x]";

struct PluginInfo {
    IPlugin* plugin;
    std::string fn;
    bool enabled;
};

using PluginInfoPtr = std::shared_ptr<PluginInfo>;
using PluginList = std::vector<PluginInfoPtr>;
using SinglePtr = std::shared_ptr<SingleLineEntry>;
using SchemaPtr = std::shared_ptr<ISchema>;

static void showConfigureOverlay(IPlugin* plugin, SchemaPtr schema) {
    std::string title = _TSTR("settings_configure_plugin_title");
    ReplaceAll(title, "{{name}}", plugin->Name());
    auto prefs = Preferences::ForPlugin(plugin->Name());
    SchemaOverlay::Show(title, prefs, schema, [](bool) {});
}

static void showNoSchemaDialog(const std::string& name) {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    std::string message = _TSTR("settings_no_plugin_config_message");
    ReplaceAll(message, "{{name}}", name);

    (*dialog)
        .SetTitle(_TSTR("settings_no_plugin_config_title"))
        .SetMessage(message)
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));

    App::Overlays().Push(dialog);
}

class PluginListAdapter : public ScrollAdapterBase {
    public:
        PluginListAdapter() {
            this->prefs = Preferences::ForComponent(prefs::components::Plugins);
            this->Refresh();
        }

        virtual ~PluginListAdapter() {
            this->prefs->Save();
        }

        void configure(size_t index) {
            SchemaPtr schema;

            using Deleter = PluginFactory::ReleaseDeleter<ISchema>;
            std::string guid = plugins[index]->plugin->Guid();
            PluginFactory::Instance().QueryInterface<ISchema, Deleter>(
                "GetSchema",
                [guid, &schema](IPlugin* raw, SchemaPtr plugin, const std::string& fn) {
                    if (raw->Guid() == guid) {
                        schema = plugin;
                    }
                });

            if (schema) {
                showConfigureOverlay(plugins[index]->plugin, schema);
            }
            else {
                showNoSchemaDialog(plugins[index]->plugin->Name());
            }
        }

        void toggleEnabled(size_t index) {
            PluginInfoPtr plugin = this->plugins.at(index);
            plugin->enabled = !plugin->enabled;
            this->prefs->SetBool(plugin->fn, plugin->enabled);
        }

        virtual size_t GetEntryCount() override {
            return plugins.size();
        }

        virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override {
            PluginInfoPtr info = plugins.at(index);

            std::string display =
                " " +
                (info->enabled ? checked : unchecked) + " " +
                info->plugin->Name() + " (" + info->fn + ")";

            SinglePtr result = SinglePtr(new SingleLineEntry(text::Ellipsize(display, this->GetWidth())));

            result->SetAttrs(Color::Default);
            if (index == window->GetScrollPosition().logicalIndex) {
                result->SetAttrs(Color::ListItemHighlighted);
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
                [&plugins, prefs](IPlugin* raw, Plugin plugin, const std::string& fn) {
                    PluginInfoPtr info(new PluginInfo());
                    info->plugin = raw;
                    info->fn = boost::filesystem::path(fn).filename().string();
                    info->enabled = prefs->GetBool(info->fn, true);
                    plugins.push_back(info);
                });

            std::sort(plugins.begin(), plugins.end(), [](PluginInfoPtr p1, PluginInfoPtr p2) -> bool {
                return SortKey(p1->plugin->Name()) < SortKey(p2->plugin->Name());
            });
        }

        std::shared_ptr<Preferences> prefs;
        PluginList plugins;
};

PluginOverlay::PluginOverlay() {

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
                        pluginAdapter->toggleEnabled(index);
                        overlay->RefreshAdapter();
                        return true;
                    }
                }
                return false;
            })
        .SetItemSelectedCallback(
            [pluginAdapter](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                pluginAdapter->configure(index);
            });

    cursespp::App::Overlays().Push(dialog);
}
