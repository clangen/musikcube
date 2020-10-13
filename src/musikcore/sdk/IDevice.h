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

#include <string>
#include <string.h>

namespace musik { namespace core { namespace sdk {

    class IDevice {
        public:
            virtual void Release() = 0;
            virtual const char* Name() const = 0;
            virtual const char* Id() const = 0;
    };

    class IDeviceList {
        public:
            virtual void Release() = 0;
            virtual size_t Count() const = 0;
            virtual const IDevice* At(size_t index) const = 0;
    };

    template <typename Device, typename Output>
    IDevice* findDeviceById(Output* output, const std::string& deviceId) {
        IDevice* result = nullptr;
        auto deviceList = output->GetDeviceList();
        if (deviceList) {
            for (size_t i = 0; i < deviceList->Count(); i++) {
                auto device = deviceList->At(i);
                if (device->Id() == deviceId) {
                    return new Device(device->Id(), device->Name());
                }
            }
            deviceList->Release();
        }
        return result;
    }

    template <typename Prefs, typename Device, typename Output>
    bool setDefaultDevice(Prefs* prefs, Output* output, const char* key, const char* deviceId) {
        if (!prefs || !deviceId || !strlen(deviceId)) {
            prefs->SetString(key, "");
            return true;
        }

        auto device = findDeviceById<Device, Output>(output, deviceId);
        if (device) {
            device->Release();
            prefs->SetString(key, deviceId);
            return true;
        }

        return false;
    }

} } }
