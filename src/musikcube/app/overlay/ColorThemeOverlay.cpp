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

#include "ColorThemeOverlay.h"

#include <core/support/Common.h>
#include <core/support/PreferenceKeys.h>
#include <core/support/Preferences.h>

#include <app/util/PreferenceKeys.h>

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>

using namespace musik;
using namespace musik::core;
using namespace musik::cube;
using namespace cursespp;
using namespace boost::filesystem;

using Callback = std::function<void()>;

static void showNeedsRestart(Callback cb = Callback()) {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle(_TSTR("default_overlay_title"))
        .SetMessage(_TSTR("settings_needs_restart"))
        .AddButton("KEY_ENTER", "ENTER", _TSTR("button_ok"), [cb](std::string key) {
            if (cb) {
                cb();
            }
        });

    App::Overlays().Push(dialog);
}

ColorThemeOverlay::ColorThemeOverlay() {
}

void ColorThemeOverlay::Show(std::function<void()> callback) {
    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    auto prefs = core::Preferences::
        ForComponent(core::prefs::components::Settings);

    std::string currentTheme = prefs->GetString(cube::prefs::keys::ColorTheme);
    bool disableCustomColors = prefs->GetBool(cube::prefs::keys::DisableCustomColors);

    int selectedIndex = disableCustomColors ? 1 : 0;

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry(_TSTR("settings_8color_theme_name"));

    auto themes = Colors::ListThemes();

    for (size_t i = 0; i < themes.size(); i++) {
        adapter->AddEntry(themes.at(i));
        if (themes.at(i) == currentTheme) {
            selectedIndex = i + 1;
        }
    }

    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle(_TSTR("color_theme_list_overlay_title"))
        .SetSelectedIndex(selectedIndex)
        .SetWidth(36)
        .SetItemSelectedCallback(
            [callback, prefs, themes, currentTheme, disableCustomColors]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index == 0) {
                    prefs->SetString(cube::prefs::keys::ColorTheme, "");
                    prefs->SetBool(cube::prefs::keys::DisableCustomColors, true);

                    if (!disableCustomColors) {
                        showNeedsRestart();
                    }
                }
                else {
                    std::string selected = themes.at(index - 1);
                    if (selected != currentTheme) {
                        prefs->SetString(cube::prefs::keys::ColorTheme, selected.c_str());
                        prefs->SetBool(cube::prefs::keys::DisableCustomColors, false);
                        Colors::SetTheme(selected);
                        if (disableCustomColors) {
                            showNeedsRestart();
                        }
                    }
                }

                prefs->Save();

                if (callback) {
                    callback();
                }
            });

    cursespp::App::Overlays().Push(dialog);
}

void ColorThemeOverlay::Show256ColorsInfo(bool enabled, std::function<void()> callback) {
    auto prefs = core::Preferences::
        ForComponent(core::prefs::components::Settings);

    if (enabled) {
        prefs->SetBool(cube::prefs::keys::UsePaletteColors, enabled);
        prefs->Save();
        showNeedsRestart(callback);
    }
    else {
        std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

        (*dialog)
            .SetTitle(_TSTR("default_overlay_title"))
            .SetMessage(_TSTR("color_theme_256_overlay_message"))
            .AddButton(
                "y", "y", _TSTR("button_yes"), [prefs, callback](std::string key) {
                    prefs->SetBool(cube::prefs::keys::UsePaletteColors, false);
                    prefs->Save();
                    showNeedsRestart(callback);
                })
            .AddButton(
                "n", "n", _TSTR("button_no"), [callback](std::string key) {
                    if (callback) {
                        callback();
                    }
                });

            App::Overlays().Push(dialog);
    }
}
