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

#include <core/support/Common.h>
#include <core/support/Preferences.h>
#include <core/support/PreferenceKeys.h>
#include <core/plugin/PluginFactory.h>
#include <core/sdk/ISchema.h>

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/InputOverlay.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>

#include <algorithm>
#include <ostream>
#include <iomanip>
#include <limits>

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
using PrefsPtr = std::shared_ptr<Preferences>;
using SinglePtr = std::shared_ptr<SingleLineEntry>;
using SchemaPtr = std::shared_ptr<ISchema>;

#define DEFAULT(type) reinterpret_cast<const ISchema::type*>(entry)->defaultValue

static size_t DEFAULT_INPUT_WIDTH = 26;

static std::string stringValueForDouble(const double value, const int precision = 2) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

static std::function<std::string(int)> INT_FORMATTER =
[](int value) -> std::string {
    return std::to_string(value);
};

static std::function<std::string(double)> doubleFormatter(int precision) {
    return [precision](double value) -> std::string {
        return stringValueForDouble(value, precision);
    };
}

template <typename T>
bool bounded(T minimum, T maximum) {
    return
        minimum != std::numeric_limits<T>::min() &&
        maximum != std::numeric_limits<T>::max();
}

template <typename T>
std::string numberInputTitle(
    std::string keyName,
    T minimum,
    T maximum,
    std::function<std::string(T)> formatter)
{
    if (bounded(minimum, maximum)) {
        return keyName + " (" + formatter(minimum)
            + " - " + formatter(maximum) + ")";
    }
    return keyName;
}

template <typename T>
static std::string stringValueFor(
    PrefsPtr prefs,
    const T* entry,
    ISchema::Type type,
    const std::string& name)
{
    switch (type) {
        case ISchema::Type::Bool:
            return prefs->GetBool(name, DEFAULT(BoolEntry)) ? "true" : "false";
        case ISchema::Type::Int:
            return std::to_string(prefs->GetInt(name, DEFAULT(IntEntry)));
        case ISchema::Type::Double: {
            auto doubleEntry = reinterpret_cast<const ISchema::DoubleEntry*>(entry);
            auto defaultValue = doubleEntry->defaultValue;
            auto precision = doubleEntry->precision;
            return stringValueForDouble(prefs->GetDouble(name, defaultValue), precision);
        }
        case ISchema::Type::String:
            return prefs->GetString(name, DEFAULT(StringEntry));
        case ISchema::Type::Enum:
            return prefs->GetString(name, DEFAULT(EnumEntry));
    }
    throw std::runtime_error("invalid type passed to stringValueFor!");
}

template <typename T>
static std::string stringValueFor(PrefsPtr prefs, const T* entry) {
    return stringValueFor(prefs, entry, entry->entry.type, entry->entry.name);
}

static std::string stringValueFor(PrefsPtr prefs, const ISchema::Entry* entry) {
    return stringValueFor(prefs, entry, entry->type, entry->name);
}

class StringListAdapter : public ScrollAdapterBase {
    public:
        StringListAdapter(std::vector<std::string>& items) : items(items) { }
        std::string At(const size_t index) { return items[index]; }
        virtual ~StringListAdapter() { }
        virtual size_t GetEntryCount() override { return items.size(); }
        virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override {
            auto entry = std::make_shared<SingleLineEntry>(
                text::Ellipsize(items[index], window->GetWidth()));

            entry->SetAttrs(CURSESPP_DEFAULT_COLOR);
            if (index == window->GetScrollPosition().logicalIndex) {
                entry->SetAttrs(COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM));
            }

            return entry;
        }
    private:
        std::vector<std::string> items;
};

class SchemaAdapter: public ScrollAdapterBase {
    public:
        SchemaAdapter(PrefsPtr prefs, SchemaPtr schema): prefs(prefs), schema(schema) {
        }

        virtual ~SchemaAdapter() {
        }

        bool Changed() const {
            return this->changed;
        }

        virtual size_t GetEntryCount() override {
            return schema->Count();
        }

        virtual EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override {
            auto entry = schema->At(index);

            std::string name = entry->name;
            std::string value = stringValueFor(prefs, entry);
            int width = window->GetContentWidth();
            int avail = std::max(0, width - int(u8cols(name)) - 1 - 1);
            auto display = " " + name + " " + text::Align(value + " ", text::AlignRight, avail);

            SinglePtr result = SinglePtr(new SingleLineEntry(text::Ellipsize(display, width)));

            result->SetAttrs(CURSESPP_DEFAULT_COLOR);
            if (index == window->GetScrollPosition().logicalIndex) {
                result->SetAttrs(COLOR_PAIR(CURSESPP_HIGHLIGHTED_LIST_ITEM));
            }

            return result;
        }

        void ShowOverlay(size_t index) {
            auto entry = schema->At(index);
            switch (entry->type) {
                case ISchema::Type::Bool:
                    return ShowBoolOverlay(reinterpret_cast<const ISchema::BoolEntry*>(entry));
                case ISchema::Type::Int:
                    return ShowIntOverlay(reinterpret_cast<const ISchema::IntEntry*>(entry));
                case ISchema::Type::Double:
                    return ShowDoubleOverlay(reinterpret_cast<const ISchema::DoubleEntry*>(entry));
                case ISchema::Type::String:
                    return ShowStringOverlay(reinterpret_cast<const ISchema::StringEntry*>(entry));
                case ISchema::Type::Enum:
                    return ShowEnumOverlay(reinterpret_cast<const ISchema::EnumEntry*>(entry));
            }
        }

    private:
        template <typename T>
        struct NumberValidator : public InputOverlay::IValidator {
            using Formatter = std::function<std::string(T)>;

            NumberValidator(T minimum, T maximum, Formatter formatter)
                : minimum(minimum), maximum(maximum), formatter(formatter) {
            }

            virtual bool IsValid(const std::string& input) const override {
                try {
                    double result = std::stod(input);
                    if (bounded(minimum, maximum) && (result < minimum || result > maximum)) {
                        return false;
                    }
                }
                catch (std::invalid_argument) {
                    return false;
                }
                return true;
            }

            virtual const std::string ErrorMessage() const override {
                if (bounded(minimum, maximum)) {
                    std::string result = _TSTR("validator_dialog_number_parse_bounded_error");
                    ReplaceAll(result, "{{minimum}}", formatter(minimum));
                    ReplaceAll(result, "{{maximum}}", formatter(maximum));
                    return result;
                }
                return _TSTR("validator_dialog_number_parse_error");
            }

            Formatter formatter;
            T minimum, maximum;
        };

        void ShowListOverlay(
            const std::string& title,
            std::vector<std::string>& items,
            const std::string defaultValue,
            std::function<void(std::string)> cb)
        {
            auto stringAdapter = std::make_shared<StringListAdapter>(items);
            std::shared_ptr<ListOverlay> dialog(new ListOverlay());

            size_t width = u8cols(title) + 4; /* extra padding for border and spacing */
            size_t index = 0;

            for (size_t i = 0; i < items.size(); i++) {
                auto current = items[i];
                if (current == defaultValue) {
                    index = i;
                }
                width = std::max(width, u8cols(current));
            }

            dialog->SetAdapter(stringAdapter)
                .SetTitle(title)
                .SetWidth(width)
                .SetSelectedIndex(index)
                .SetAutoDismiss(true)
                .SetItemSelectedCallback(
                    [cb, stringAdapter](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                        if (cb) {
                            cb(stringAdapter->At(index));
                        }
                    });

            cursespp::App::Overlays().Push(dialog);
        }

        void ShowBoolOverlay(const ISchema::BoolEntry* entry) {
            auto name = entry->entry.name;
            std::vector<std::string> items = { "true", "false" };
            ShowListOverlay(name, items, stringValueFor(prefs, entry),
                [this, name](std::string value) {
                    this->prefs->SetBool(name, value == "true");
                    this->changed = true;
                });
        }

        void ShowIntOverlay(const ISchema::IntEntry* entry) {
            auto name = entry->entry.name;

            auto title = numberInputTitle(
                name, entry->minValue, entry->maxValue, INT_FORMATTER);

            auto validator = std::make_shared<NumberValidator<int>>(
                entry->minValue,  entry->maxValue, INT_FORMATTER);

            auto width = std::max(u8cols(title), DEFAULT_INPUT_WIDTH);

            std::shared_ptr<InputOverlay> dialog(new InputOverlay());

            dialog->SetTitle(title)
                .SetText(stringValueFor(prefs, entry))
                .SetValidator(validator)
                .SetWidth(width)
                .SetInputAcceptedCallback([this, name](const std::string& value) {
                    this->prefs->SetInt(name, std::stoi(value));
                    this->changed = true;
                });

            App::Overlays().Push(dialog);
        }

        void ShowDoubleOverlay(const ISchema::DoubleEntry* entry) {
            auto name = entry->entry.name;

            auto formatter = doubleFormatter(entry->precision);

            auto title = numberInputTitle(
                name, entry->minValue, entry->maxValue, formatter);

            auto validator = std::make_shared<NumberValidator<double>>(
                entry->minValue, entry->maxValue, formatter);

            auto width = std::max(u8cols(title) + 4, DEFAULT_INPUT_WIDTH);

            std::shared_ptr<InputOverlay> dialog(new InputOverlay());

            dialog->SetTitle(title)
                .SetText(stringValueFor(prefs, entry))
                .SetValidator(validator)
                .SetWidth(width)
                .SetInputAcceptedCallback([this, name](const std::string& value) {
                    this->prefs->SetDouble(name, std::stod(value));
                    this->changed = true;
                });

            App::Overlays().Push(dialog);
        }

        void ShowStringOverlay(const ISchema::StringEntry* entry) {
            auto name = entry->entry.name;
            std::shared_ptr<InputOverlay> dialog(new InputOverlay());
            dialog->SetTitle(name)
                .SetText(stringValueFor(prefs, entry))
                .SetInputAcceptedCallback([this, name](const std::string& value) {
                    this->prefs->SetString(name, value.c_str());
                    this->changed = true;
                });
            App::Overlays().Push(dialog);
        }

        void ShowEnumOverlay(const ISchema::EnumEntry* entry) {
            auto name = entry->entry.name;
            std::vector<std::string> items;

            for (size_t i = 0; i < entry->count; i++) {
                items.push_back(entry->values[i]);
            }

            ShowListOverlay(name, items, stringValueFor(prefs, entry),
                [this, name](std::string value) {
                    this->prefs->SetString(name, value.c_str());
                    this->changed = true;
                });
        }

        PrefsPtr prefs;
        SchemaPtr schema;
        bool changed{false};
};

static void showConfigureOverlay(IPlugin* plugin, SchemaPtr schema) {
    auto prefs = Preferences::ForPlugin(plugin->Name());
    std::shared_ptr<SchemaAdapter> schemaAdapter(new SchemaAdapter(prefs, schema));
    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    std::string title = _TSTR("settings_configure_plugin_title");
    ReplaceAll(title, "{{name}}", plugin->Name());

    dialog->SetAdapter(schemaAdapter)
        .SetTitle(title)
        .SetWidthPercent(80)
        .SetAutoDismiss(false)
        .SetItemSelectedCallback(
            [schemaAdapter](ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                schemaAdapter->ShowOverlay(index);
            })
        .SetDismissedCallback([plugin, schemaAdapter](ListOverlay* overlay) {
                if (schemaAdapter->Changed()) {
                    plugin->Reload();
                }
            });

    cursespp::App::Overlays().Push(dialog);
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
