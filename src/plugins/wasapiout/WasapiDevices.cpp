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

#include "WasapiDevices.h"
#include "WasapiSettings.h"
#include <musikcore/plugin/services.h>

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <string>

using Microsoft::WRL::ComPtr;

namespace {
    class Apartment {
    public:
        Apartment() : result(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
        ~Apartment() {
            if (SUCCEEDED(this->result)) {
                CoUninitialize();
            }
        }
        bool Available() const { return SUCCEEDED(this->result) || this->result == RPC_E_CHANGED_MODE; }
    private:
        HRESULT result;
    };

    std::string utf8(const wchar_t* text) {
        if (!text) {
            return {};
        }
        const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1) {
            return {};
        }
        std::string result(static_cast<size_t>(size), '\0');
        if (!WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr)) {
            return {};
        }
        result.pop_back();
        return result;
    }

    std::wstring wide(mc_string_view text) {
        if (!text.length || text.length > INT_MAX) {
            return {};
        }
        const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data,
            static_cast<int>(text.length), nullptr, 0);
        if (!size) {
            return {};
        }
        std::wstring result(static_cast<size_t>(size), L'\0');
        if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data,
            static_cast<int>(text.length), result.data(), size)) {
            return {};
        }
        return result;
    }

    HRESULT enumerator(ComPtr<IMMDeviceEnumerator>& result) {
        return CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            IID_PPV_ARGS(result.GetAddressOf()));
    }

    HRESULT findDevice(IMMDeviceEnumerator* enumerator, mc_string_view id, ComPtr<IMMDevice>& device) {
        if (!id.length) {
            return enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, device.GetAddressOf());
        }
        const auto deviceId = wide(id);
        return deviceId.empty() ? E_INVALIDARG : enumerator->GetDevice(deviceId.c_str(), device.GetAddressOf());
    }

    mc_result report(IMMDevice* device, mc_output_device_fn read, void* user) {
        LPWSTR rawId = nullptr;
        if (FAILED(device->GetId(&rawId))) {
            return MC_ERROR_IO;
        }
        const auto id = utf8(rawId);
        CoTaskMemFree(rawId);
        ComPtr<IPropertyStore> properties;
        if (id.empty() || FAILED(device->OpenPropertyStore(STGM_READ, properties.GetAddressOf()))) {
            return MC_ERROR_IO;
        }
        PROPVARIANT value;
        PropVariantInit(&value);
        const HRESULT result = properties->GetValue(PKEY_Device_FriendlyName, &value);
        const auto name = SUCCEEDED(result) && value.vt == VT_LPWSTR ? utf8(value.pwszVal) : std::string();
        PropVariantClear(&value);
        if (name.empty()) {
            return MC_ERROR_IO;
        }
        const mc_output_device item {
            .id = { id.data(), id.size() },
            .name = { name.data(), name.size() },
        };
        read(user, &item);
        return MC_OK;
    }
}

mc_result wasapi::VisitDevices(mc_output_device_fn read, void* user) {
    Apartment apartment;
    ComPtr<IMMDeviceEnumerator> devices;
    ComPtr<IMMDeviceCollection> collection;
    if (!apartment.Available() || FAILED(enumerator(devices)) ||
        FAILED(devices->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, collection.GetAddressOf())))
    {
        return MC_ERROR_IO;
    }
    UINT count = 0;
    if (FAILED(collection->GetCount(&count))) {
        return MC_ERROR_IO;
    }
    for (UINT i = 0; i < count; ++i) {
        ComPtr<IMMDevice> device;
        if (SUCCEEDED(collection->Item(i, device.GetAddressOf()))) {
            report(device.Get(), read, user);
        }
    }
    return MC_OK;
}

mc_result wasapi::ReadDefaultDevice(mc_output_device_fn read, void* user) {
    Apartment apartment;
    ComPtr<IMMDeviceEnumerator> devices;
    ComPtr<IMMDevice> device;
    if (!apartment.Available() || FAILED(enumerator(devices))) {
        return MC_ERROR_IO;
    }
    if (FAILED(devices->GetDefaultAudioEndpoint(eRender, eMultimedia, device.GetAddressOf()))) {
        return MC_ERROR_NOT_FOUND;
    }
    return report(device.Get(), read, user);
}

/* called in the output worker's COM apartment. a missing pinned endpoint
falls back to the default while the host retains the pin for the next open. */
mc_result wasapi::OpenClient(mc_string_view id, IAudioClient** client) {
    ComPtr<IMMDeviceEnumerator> devices;
    ComPtr<IMMDevice> device;
    if (FAILED(enumerator(devices))) {
        return MC_ERROR_IO;
    }
    HRESULT result = findDevice(devices.Get(), id, device);
    if (FAILED(result) && id.length) {
        result = devices->GetDefaultAudioEndpoint(eRender, eMultimedia, device.GetAddressOf());
    }
    if (FAILED(result)) {
        return MC_ERROR_NOT_FOUND;
    }
    result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client));
    if (FAILED(result) && id.length) {
        device.Reset();
        result = devices->GetDefaultAudioEndpoint(eRender, eMultimedia, device.GetAddressOf());
        if (SUCCEEDED(result)) {
            result = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client));
        }
    }
    return SUCCEEDED(result) && *client ? MC_OK : MC_ERROR_IO;
}

mc_result wasapi::GetDefaultFormat(const mc_plugin_services* services, mc_string_view id, mc_audio_format& format) {
    bool resampling = DecoderResampling.default_value;
    if (services && services->preferences) {
        const auto* preferences = services->preferences;
        const auto result = preferences->read_boolean(preferences->context, DecoderResampling.key,
            DecoderResampling.default_value, &resampling);
        if (result != MC_OK) {
            return result;
        }
    }
    if (!resampling) {
        return MC_ERROR_UNSUPPORTED;
    }
    Apartment apartment;
    ComPtr<IMMDeviceEnumerator> devices;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> client;
    if (!apartment.Available() || FAILED(enumerator(devices))) {
        return MC_ERROR_IO;
    }
    if (FAILED(findDevice(devices.Get(), id, device))) {
        return MC_ERROR_NOT_FOUND;
    }
    if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
        reinterpret_cast<void**>(client.GetAddressOf()))) || !client)
    {
        return MC_ERROR_IO;
    }
    WAVEFORMATEX* mix = nullptr;
    if (FAILED(client->GetMixFormat(&mix)) || !mix) {
        return MC_ERROR_IO;
    }
    format.sample_rate = mix->nSamplesPerSec;
    format.channels = mix->nChannels;
    CoTaskMemFree(mix);
    return MC_OK;
}
