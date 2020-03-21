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

#pragma once

#include <core/config.h>
#include <core/i18n/Locale.h>
#include <core/library/ILibrary.h>
#include <core/library/LocalLibrary.h>
#include <core/library/track/Track.h>
#include <core/library/track/TrackList.h>
#include <core/audio/PlaybackService.h>
#include <core/audio/ITransport.h>
#include <core/support/Preferences.h>
#include <cursespp/curses_config.h>
#include <core/utfutil.h>

#include <cursespp/App.h>
#include <cursespp/AppLayout.h>
#include <cursespp/Checkbox.h>
#include <cursespp/Colors.h>
#include <cursespp/DialogOverlay.h>
#include <cursespp/InputOverlay.h>
#include <cursespp/IMouseHandler.h>
#include <cursespp/LayoutBase.h>
#include <cursespp/ListOverlay.h>
#include <cursespp/ListWindow.h>
#include <cursespp/MultiLineEntry.h>
#include <cursespp/OverlayStack.h>
#include <cursespp/SchemaOverlay.h>
#include <cursespp/Screen.h>
#include <cursespp/ScrollableWindow.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/Scrollbar.h>
#include <cursespp/ShortcutsWindow.h>
#include <cursespp/SimpleScrollAdapter.h>
#include <cursespp/SingleLineEntry.h>
#include <cursespp/Text.h>
#include <cursespp/TextInput.h>
#include <cursespp/TextLabel.h>
#include <cursespp/ToastOverlay.h>
#include <cursespp/Window.h>

#include <set>
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <thread>

#include <cmath>
#include <string>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string.hpp>