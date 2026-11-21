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

#include <cursespp/SchemaOverlay.h>

#include <musikcore/i18n/Locale.h>
#include <musikcore/utfutil.h>

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/InputOverlay.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/NumberValidator.h>
#include <cursespp/Screen.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>

#include <iomanip>
#include <sstream>

using namespace cursespp;

namespace {
    using Field = SchemaOverlay::Field;
    using Value = SchemaOverlay::Value;
    using Input = std::function<void(std::string)>;

    int overlayWidth() {
        return static_cast<int>(0.8f * Screen::GetWidth());
    }

    std::string formatNumber(double value, int precision) {
        std::ostringstream text;
        text << std::fixed << std::setprecision(precision) << value;
        return text.str();
    }

    std::string displayValue(const Field& field) {
        return std::visit([&](const auto& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, bool>) {
                return value ? "true" : "false";
            }
            else if constexpr (std::is_same_v<T, int32_t>) {
                return std::to_string(value);
            }
            else if constexpr (std::is_same_v<T, double>) {
                return formatNumber(value, field.precision);
            }
            else {
                return value;
            }
        }, field.value);
    }

    void showChoices(const Field& field, const std::vector<std::string>& choices, Input submit) {
        auto adapter = std::make_shared<SimpleScrollAdapter>();
        adapter->SetSelectable(true);
        const auto current = displayValue(field);
        size_t selected = 0;
        for (size_t i = 0; i < choices.size(); ++i) {
            adapter->AddEntry(choices[i]);
            if (choices[i] == current) {
                selected = i;
            }
        }

        auto dialog = std::make_shared<ListOverlay>();
        dialog->SetAdapter(adapter)
            .SetTitle(field.key)
            .SetWidth(overlayWidth())
            .SetSelectedIndex(selected)
            .SetItemSelectedCallback([choices, submit = std::move(submit)](ListOverlay*, IScrollAdapterPtr, size_t index) {
                if (index < choices.size()) {
                    submit(choices[index]);
                }
            });
        App::Overlays().Push(dialog);
    }

    template<typename T>
    void showNumber(const Field& field, typename NumberValidator<T>::Formatter format, Input submit) {
        const auto minimum = static_cast<T>(field.minimum);
        const auto maximum = static_cast<T>(field.maximum);
        auto title = field.key;
        if (NumberValidator<T>::bounded(minimum, maximum)) {
            title += " (" + format(minimum) + " - " + format(maximum) + ")";
        }

        auto dialog = std::make_shared<InputOverlay>();
        dialog->SetTitle(title)
            .SetText(displayValue(field))
            .SetValidator(std::make_shared<NumberValidator<T>>(minimum, maximum, std::move(format)))
            .SetWidth(overlayWidth())
            .SetInputAcceptedCallback(std::move(submit));
        App::Overlays().Push(dialog);
    }

    void showText(const Field& field, Input submit) {
        auto dialog = std::make_shared<InputOverlay>();
        dialog->SetTitle(field.key)
            .SetText(displayValue(field))
            .SetWidth(overlayWidth())
            .SetAllowEmptyValue(field.allowEmpty)
            .SetInputAcceptedCallback(std::move(submit));
        App::Overlays().Push(dialog);
    }

    /* Nested editors keep the field snapshot alive until their callback returns. */
    class SchemaAdapter final :
        public ScrollAdapterBase,
        public std::enable_shared_from_this<SchemaAdapter>
    {
    public:
        SchemaAdapter(std::vector<Field> fields, SchemaOverlay::Write write)
            : fields(std::move(fields)), write(std::move(write)) {}

        bool Changed() const {
            return this->changed;
        }

        size_t GetEntryCount() override {
            return this->fields.size();
        }

        EntryPtr GetEntry(ScrollableWindow* window, size_t index) override {
            const auto& field = this->fields.at(index);
            const auto width = window->GetContentWidth();
            const auto available = std::max(0, width - static_cast<int>(u8cols(field.key)) - 2);
            const auto text = " " + field.key + " " + text::Align(displayValue(field) + " ", text::AlignRight, available);
            auto result = std::make_shared<SingleLineEntry>(text::Ellipsize(text, width));
            result->SetAttrs(index == window->GetScrollPosition().logicalIndex
                ? Color::ListItemHighlighted
                : Color::Default);
            return result;
        }

        void Edit(size_t index) {
            const auto& field = this->fields.at(index);
            auto submit = [self = this->shared_from_this(), index](Value value) {
                self->Submit(index, std::move(value));
            };

            if (std::holds_alternative<bool>(field.value)) {
                showChoices(field, { "false", "true" }, [submit](const std::string& text) { submit(text == "true"); });
            }
            else if (std::holds_alternative<int32_t>(field.value)) {
                showNumber<int32_t>(field, [](int32_t value) { return std::to_string(value); },
                    [submit](const std::string& text) { submit(static_cast<int32_t>(std::stod(text))); });
            }
            else if (std::holds_alternative<double>(field.value)) {
                showNumber<double>(field, [precision = field.precision](double value) {
                    return formatNumber(value, precision);
                }, [submit](const std::string& text) { submit(std::stod(text)); });
            }
            else if (!field.choices.empty()) {
                showChoices(field, field.choices, [submit](std::string text) { submit(std::move(text)); });
            }
            else {
                showText(field, [submit](std::string text) { submit(std::move(text)); });
            }
        }

    private:
        void Submit(size_t index, Value value) {
            auto& field = this->fields.at(index);
            if (this->write(field.key, value) == MC_OK) {
                field.value = std::move(value);
                this->changed = true;
                return;
            }

            auto dialog = std::make_shared<DialogOverlay>();
            dialog->SetTitle(_TSTR("settings_server_invalid_settings_title"))
                .SetMessage(_TSTR("settings_server_invalid_settings_message"))
                .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"));
            App::Overlays().Push(dialog);
        }

        std::vector<Field> fields;
        SchemaOverlay::Write write;
        bool changed = false;
    };
}

void SchemaOverlay::Show(const std::string& title, std::vector<Field> fields, Write write,
    std::function<void(bool)> completed)
{
    auto adapter = std::make_shared<SchemaAdapter>(std::move(fields), std::move(write));
    auto dialog = std::make_shared<ListOverlay>();
    dialog->SetAdapter(adapter)
        .SetTitle(title)
        .SetWidthPercent(80)
        .SetAutoDismiss(false)
        .SetItemSelectedCallback([adapter](ListOverlay*, IScrollAdapterPtr, size_t index) {
            adapter->Edit(index);
        })
        .SetDismissedCallback([adapter, completed = std::move(completed)](ListOverlay*) {
            if (completed) {
                completed(adapter->Changed());
            }
        });
    App::Overlays().Push(dialog);
}
