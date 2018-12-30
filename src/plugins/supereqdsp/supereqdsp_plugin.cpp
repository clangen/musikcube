//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2019 musikcube team
//
// All rights reserved.
//
// Note: this musikcube plugin (supereq) is not distributed under a BSD
// license like most other project components because it consumes GPL2
// code.
//
//////////////////////////////////////////////////////////////////////////////
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//
//////////////////////////////////////////////////////////////////////////////

#include "constants.h"
#include <core/sdk/constants.h>
#include <core/sdk/IPlugin.h>
#include <core/sdk/ISchema.h>
#include <core/sdk/IDSP.h>
#include "SuperEqDsp.h"

class SuperEqPlugin : public musik::core::sdk::IPlugin {
    public:
        virtual void Release() { delete this; }
        virtual const char* Name() { return "SuperEq IDSP"; }
        virtual const char* Version() { return "0.1.0"; }
        virtual const char* Author() { return "Naoki Shibata, Alexey Yakovenko, clangen"; }
        virtual const char* Guid() { return "6f0ed53b-0f13-4220-9b0a-ca496b6421cc"; }
        virtual bool Configurable() { return false; }
        virtual void Configure() { }
        virtual void Reload() { SuperEqDsp::NotifyChanged(); }
        virtual int SdkVersion() { return musik::core::sdk::SdkVersion; }
};

#ifdef WIN32
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    return true;
}
#endif

extern "C" DLLEXPORT musik::core::sdk::IPlugin* GetPlugin() {
    return new SuperEqPlugin();
}

extern "C" DLLEXPORT musik::core::sdk::IDSP* GetDSP() {
    return new SuperEqDsp();
}

extern "C" DLLEXPORT musik::core::sdk::ISchema* GetSchema();