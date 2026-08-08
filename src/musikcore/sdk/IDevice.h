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

/** @file IDevice.h @brief Defines audio device interfaces and helper functions for device selection. */
#pragma once

#include <string>
#include <string.h>

/** @namespace musik::core::sdk @brief Core SDK interfaces shared between the musikcube application and its plugins. */
namespace musik { namespace core { namespace sdk {

    /** @brief Describes a single audio output device available to an output plugin. */
    class IDevice {
        public:
            /** @brief Releases the device; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Returns the human-readable name of the device.
             *  @return The device name. */
            virtual const char* Name() const = 0;

            /** @brief Returns the unique identifier of the device.
             *  @return The device id. */
            virtual const char* Id() const = 0;
    };

    /** @brief A read-only list of audio devices exposed by an output plugin. */
    class IDeviceList {
        public:
            /** @brief Releases the list; callers must invoke this when done. */
            virtual void Release() = 0;

            /** @brief Returns the number of devices in the list.
             *  @return The device count. */
            virtual size_t Count() const = 0;

            /** @brief Returns the device at the given index.
             *  @param index The zero-based index into the list.
             *  @return The device, or null if the index is out of range. */
            virtual const IDevice* At(size_t index) const = 0;
    };

    /** @brief Finds and constructs a device instance by id from an output's device list.
     *  @tparam Device The concrete device type to construct.
     *  @tparam Output The output plugin type providing the device list.
     *  @param output The output plugin whose device list will be searched.
     *  @param deviceId The device id to look for.
     *  @return A newly constructed Device matching the id, or null if not found. */
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

    /** @brief Sets the default output device, persisting the selection through the given preferences.
     *  @tparam Prefs The preferences type used to persist the selection.
     *  @tparam Device The concrete device type used for validation.
     *  @tparam Output The output plugin type providing the device list.
     *  @param prefs The preferences object used to persist the device id.
     *  @param output The output plugin whose device list will be searched.
     *  @param key The preferences key under which the device id is stored.
     *  @param deviceId The device id to select; an empty value clears the selection.
     *  @return True if the device was selected, or the selection was cleared. */
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
