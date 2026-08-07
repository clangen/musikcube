//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2019 musikcube team
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

/** @file SchemaOverlay.h @brief Static helpers that show settings overlays from an ISchema. */
#pragma once

#include <musikcore/support/Preferences.h>
#include <musikcore/sdk/ISchema.h>

namespace cursespp {
    /** @brief Factory that presents typed settings editors as modal overlays.
     *
     *  @details SchemaOverlay is a static utility. Given a musikcore
     *  ISchema entry (Bool, Int, Double, String or Enum) and a Preferences
     *  store, it constructs and shows the appropriate curses overlay
     *  (Checkbox, input or list) so the user can edit the value. The result is
     *  delivered through the callback as a string. Show() is the generic
     *  entry point that dispatches based on the schema entry type.
     */
    class SchemaOverlay {
        public:
            using Prefs = musik::core::Preferences;
            using ISchema = musik::core::sdk::ISchema;
            using SchemaPtr = std::shared_ptr<ISchema>;
            using PrefsPtr = std::shared_ptr<Prefs>;

            /** @brief Displays an editor appropriate for the given schema entry.
             *  @param title the overlay title.
             *  @param prefs the preferences store bound to the entry.
             *  @param schema the schema containing the entry.
             *  @param callback invoked with the resulting value as a string.
             */
            static void Show(
                const std::string& title,
                PrefsPtr prefs,
                SchemaPtr schema,
                std::function<void(bool)> callback);

            /** @brief Shows a list overlay for selecting one of several string items.
             *  @param title the overlay title.
             *  @param items the selectable items.
             *  @param defaultValue the item preselected.
             *  @param cb invoked with the chosen item.
             */
            static void ShowListOverlay(
                const std::string& title,
                std::vector<std::string>& items,
                const std::string defaultValue,
                std::function<void(std::string)> cb);

            /** @brief Shows a boolean editor (checkbox) for a BoolEntry.
             *  @param entry the schema boolean entry.
             *  @param prefs the preferences store.
             *  @param callback invoked with the resulting value as a string.
             */
            static void ShowBoolOverlay(
                const ISchema::BoolEntry* entry,
                PrefsPtr prefs,
                std::function<void(std::string)> callback);

            /** @brief Shows a numeric editor for an IntEntry.
             *  @param entry the schema integer entry.
             *  @param prefs the preferences store.
             *  @param callback invoked with the resulting value as a string.
             */
            static void ShowIntOverlay(
                const ISchema::IntEntry* entry,
                PrefsPtr prefs,
                std::function<void(std::string)> callback);

            /** @brief Shows a numeric editor for a DoubleEntry.
             *  @param entry the schema double entry.
             *  @param prefs the preferences store.
             *  @param callback invoked with the resulting value as a string.
             */
            static void ShowDoubleOverlay(
                const ISchema::DoubleEntry* entry,
                PrefsPtr prefs,
                std::function<void(std::string)> callback);

            /** @brief Shows a text editor for a StringEntry.
             *  @param entry the schema string entry.
             *  @param prefs the preferences store.
             *  @param callback invoked with the resulting value as a string.
             */
            static void ShowStringOverlay(
                const ISchema::StringEntry* entry,
                PrefsPtr prefs,
                std::function<void(std::string)> callback);

            /** @brief Shows a list overlay for an EnumEntry.
             *  @param entry the schema enum entry.
             *  @param prefs the preferences store.
             *  @param callback invoked with the resulting value as a string.
             */
            static void ShowEnumOverlay(
                const ISchema::EnumEntry* entry,
                PrefsPtr prefs,
                std::function<void(std::string)> callback);

        private:
            SchemaOverlay();
    };
}
