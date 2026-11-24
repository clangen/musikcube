#include "DirectSoundDevices.h"

#include <windows.h>
#include <dsound.h>
#include <objbase.h>

#include <string>

namespace {
    std::string Utf8(const wchar_t* value) {
        if (!value) {
            return {};
        }
        const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1) {
            return {};
        }
        std::string result(static_cast<size_t>(size), '\0');
        if (!WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr)) {
            return {};
        }
        result.pop_back();
        return result;
    }

    struct Enumeration {
        mc_output_device_fn read;
        void* user;
        const GUID* wanted;
        bool found = false;
        bool failed = false;
    };

    BOOL CALLBACK Visit(LPGUID id, LPCWSTR description, LPCWSTR, LPVOID context) noexcept {
        auto& enumeration = *static_cast<Enumeration*>(context);
        if (!id || (enumeration.wanted && *id != *enumeration.wanted)) {
            return TRUE;
        }
        try {
            wchar_t guid[39] {};
            if (!StringFromGUID2(*id, guid, static_cast<int>(std::size(guid)))) {
                enumeration.failed = true;
                return FALSE;
            }
            const auto deviceId = Utf8(guid);
            const auto name = Utf8(description);
            if (deviceId.empty() || name.empty()) {
                enumeration.failed = true;
                return FALSE;
            }
            const mc_output_device device { { deviceId.data(), deviceId.size() }, { name.data(), name.size() } };
            enumeration.read(enumeration.user, &device);
            enumeration.found = true;
            return enumeration.wanted ? FALSE : TRUE;
        }
        catch (...) {
            enumeration.failed = true;
            return FALSE;
        }
    }
}

mc_result directsound::VisitDevices(mc_output_device_fn read, void* user) {
    Enumeration enumeration { read, user, nullptr };
    const HRESULT result = DirectSoundEnumerateW(Visit, &enumeration);
    return FAILED(result) || enumeration.failed ? MC_ERROR_IO : MC_OK;
}

mc_result directsound::ReadDefaultDevice(mc_output_device_fn read, void* user) {
    GUID id {};
    if (FAILED(GetDeviceID(&DSDEVID_DefaultPlayback, &id))) {
        return MC_ERROR_NOT_FOUND;
    }
    Enumeration enumeration { read, user, &id };
    const HRESULT result = DirectSoundEnumerateW(Visit, &enumeration);
    if (FAILED(result) || enumeration.failed) {
        return MC_ERROR_IO;
    }
    return enumeration.found ? MC_OK : MC_ERROR_NOT_FOUND;
}
