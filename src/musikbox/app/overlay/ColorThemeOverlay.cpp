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

#include "stdafx.h"

#include "ColorThemeOverlay.h"

#include <core/support/Common.h>
#include <core/support/PreferenceKeys.h>
#include <core/support/Preferences.h>

#include <musikbox/app/util/PreferenceKeys.h>

#include <cursespp/App.h>
#include <cursespp/Colors.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/DialogOverlay.h>

#include <boost/filesystem.hpp>

using namespace musik;
using namespace musik::core;
using namespace musik::box;
using namespace cursespp;
using namespace boost::filesystem;

using Callback = std::function<void()>;

static void showNeedsRestart(Callback cb = Callback()) {
    std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

    (*dialog)
        .SetTitle("musikbox")
        .SetMessage("you will need to restart musikbox for this change to take effect.")
        .AddButton("KEY_ENTER", "ENTER", "ok", [cb](std::string key) {
            if (cb) {
                cb();
            }
        });

    App::Overlays().Push(dialog);
}

static std::string ThemesDirectory() {
    return musik::core::GetApplicationDirectory() + "/themes/";
}

ColorThemeOverlay::ColorThemeOverlay() {
}

void ColorThemeOverlay::Show(std::function<void()> callback) {
    using Adapter = cursespp::SimpleScrollAdapter;
    using ListOverlay = cursespp::ListOverlay;

    auto prefs = core::Preferences::
        ForComponent(core::prefs::components::Settings);

    std::string currentTheme = prefs->GetString(box::prefs::keys::ColorTheme);
    bool disableCustomColors = prefs->GetBool(box::prefs::keys::DisableCustomColors);

    int selectedIndex = disableCustomColors ? 1 : 0;

    std::shared_ptr<Adapter> adapter(new Adapter());
    adapter->AddEntry("default");
    adapter->AddEntry("8 colors (compatibilty mode)");

    std::shared_ptr<std::vector<std::string>> themes(new std::vector<std::string>());

    path colorPath(ThemesDirectory());
    if (exists(colorPath)) {
        int i = 2;
        directory_iterator end;
        for (directory_iterator file(colorPath); file != end; file++) {
            const path& p = file->path();

            if (p.has_extension() && p.extension().string() == ".json") {
                std::string fn = p.filename().string();
                fn = fn.substr(0, fn.rfind("."));
                themes->push_back(fn);
                adapter->AddEntry(fn);

                if (currentTheme == fn) {
                    selectedIndex = i;
                }

                ++i;
            }
        }
    }

    adapter->SetSelectable(true);

    std::shared_ptr<ListOverlay> dialog(new ListOverlay());

    dialog->SetAdapter(adapter)
        .SetTitle("color themes")
        .SetSelectedIndex(selectedIndex)
        .SetWidth(36)
        .SetItemSelectedCallback(
            [callback, prefs, themes, currentTheme, disableCustomColors]
            (ListOverlay* overlay, IScrollAdapterPtr adapter, size_t index) {
                if (index == 0) {
                    prefs->SetString(box::prefs::keys::ColorTheme, "");
                    prefs->SetBool(box::prefs::keys::DisableCustomColors, false);
                    Colors::SetTheme("");

                    if (disableCustomColors) {
                        showNeedsRestart();
                    }
                }
                else if (index == 1) {
                    prefs->SetString(box::prefs::keys::ColorTheme, "");
                    prefs->SetBool(box::prefs::keys::DisableCustomColors, true);

                    if (!disableCustomColors) {
                        showNeedsRestart();
                    }
                }
                else {
                    std::string selected = themes->at(index - 2).c_str();
                    if (selected != currentTheme) {
                        prefs->SetString(box::prefs::keys::ColorTheme, selected.c_str());
                        prefs->SetBool(box::prefs::keys::DisableCustomColors, false);
                        Colors::SetTheme(ThemesDirectory() + selected + ".json");

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
        prefs->SetBool(box::prefs::keys::UsePaletteColors, enabled);
        prefs->Save();
        showNeedsRestart(callback);
    }
    else {
        std::shared_ptr<DialogOverlay> dialog(new DialogOverlay());

        (*dialog)
            .SetTitle("musikbox")
            .SetMessage(
                "disabling 256 color degradation will enable RGB color mode, which will replace colors in the stock "
                "palette. disabling this option results in higher fidelity themes, but it may cause display "
                "issues in other applications until the terminal is reset.\n\n"
                "are you sure you want to disable 256 color degradation?")
            .AddButton(
                "y", "y", "yes", [prefs, callback](std::string key) {
                    prefs->SetBool(box::prefs::keys::UsePaletteColors, false);
                    prefs->Save();
                    showNeedsRestart(callback);
                })
            .AddButton(
                "n", "n", "no", [callback](std::string key) {
                    if (callback) {
                        callback();
                    }
                });

            App::Overlays().Push(dialog);
    }
}
