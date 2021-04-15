//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2021 musikcube team
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

#include "Utility.h"
#include <string.h>
#include <musikcore/sdk/String.h>
#include <musikcore/sdk/Filesystem.h>

#ifdef WIN32
#include <Windows.h>
#endif

using namespace musik::core::sdk;

bool isFileTypeSupported(const char* type) {
    const char* actualType = strlen(type) && type[0] == '.' ? &type[1] : type;
    return openmpt_is_extension_supported(actualType);
}

bool isFileSupported(const std::string& filename) {
    return isFileTypeSupported(fs::getFileExtension(filename).c_str());
}

bool fileToByteArray(const std::string& path, char** target, int& size) {
#ifdef WIN32
    std::wstring u16fn = str::u8to16(path.c_str());
    FILE* file = _wfopen(u16fn.c_str(), L"rb");
#else
    FILE* file = fopen(path.c_str(), "rb");
#endif

    *target = nullptr;
    size = 0;

    if (!file) {
        return false;
    }

    bool success = false;

    if (fseek(file, 0L, SEEK_END) == 0) {
        long fileSize = ftell(file);
        if (fileSize == -1) {
            goto close_and_return;
        }

        if (fseek(file, 0L, SEEK_SET) != 0) {
            goto close_and_return;
        }

        *target = (char*)malloc(sizeof(char) * fileSize);
        size = fread(*target, sizeof(char), fileSize, file);

        if (size == fileSize) {
            success = true;
        }
    }

close_and_return:
    fclose(file);

    if (!success) {
        free(*target);
    }

    return success;
}

std::string readMetadataValue(openmpt_module* module, const char* key, const char* defaultValue) {
    std::string result;
    if (module && key && strlen(key)) {
        const char* value = openmpt_module_get_metadata(module, key);
        if (value) {
            result = value;
            openmpt_free_string(value);
        }
    }
    if (!result.size() && defaultValue) {
        result = defaultValue;
    }
    return result;
}

ISchema* createSchema() {
    auto schema = new musik::core::sdk::TSchema<>();
    schema->AddString(KEY_DEFAULT_ALBUM_NAME, DEFAULT_ALBUM_NAME);
    schema->AddString(KEY_DEFAULT_ARTIST_NAME, DEFAULT_ARTIST_NAME);
    return schema;
}