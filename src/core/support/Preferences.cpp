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

#include "pch.hpp"

#include <core/support/Preferences.h>
#include <core/support/Common.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread/mutex.hpp>

#include <unordered_map>

using nlohmann::json;
using namespace musik::core;

static std::unordered_map<std::string, std::weak_ptr<Preferences> > cache;
static boost::mutex cacheMutex;

#define FILENAME(x) musik::core::GetDataDirectory() + "/" + x + ".json"

static FILE* openFile(const std::string& fn, const std::string& mode) {
#ifdef WIN32
    std::wstring u16fn = u8to16(fn);
    std::wstring u16mode = u8to16(mode);
    return _wfopen(u16fn.c_str(), u16mode.c_str());
#else
    return fopen(fn.c_str(), mode.c_str());
#endif
}

static std::string fileToString(const std::string& fn) {
    FILE* f = openFile(fn, "rb");
    std::string result;

    if (!f) {
        return result;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    if (len > 0) {
        char* bytes = new char[len];
        fread(static_cast<void*>(bytes), len, 1, f);
        result.assign(bytes, len);
        delete[] bytes;
    }

    fclose(f);

    return result;
}

static bool stringToFile(const std::string& fn, const std::string& str) {
    FILE* f = openFile(fn, "wb");

    if (!f) {
        return false;
    }

    size_t written = fwrite(str.c_str(), str.size(), 1, f);
    fclose(f);
    return (written == str.size());
}

#define CACHE_KEY(name, mode) \
    boost::str(boost::format("%s-%s") % name % mode)

std::shared_ptr<Preferences> Preferences::ForComponent(
    const std::string& c, Preferences::Mode mode)
{
    boost::mutex::scoped_lock lock(cacheMutex);

    std::string key = CACHE_KEY(c, mode);

    auto it = cache.find(key);
    if (it != cache.end()) {
        auto weak = it->second;
        try {
            auto shared = weak.lock();
            if (shared) {
                return shared;
            }
        }
        catch (...) {
            /* unable to lock, let's create a new one... */
        }
    }

    std::shared_ptr<Preferences> prefs(new Preferences(c, mode));
    cache[key] = prefs;
    return prefs;
}

Preferences::Preferences(const std::string& component, Mode mode) {
    this->mode = mode;
    this->component = component;
    this->Load();
}

Preferences::~Preferences() {
    if (this->mode == ModeAutoSave) {
        this->Save();
    }
}

#define RETURN_VALUE(defaultValue) \
    auto it = json.find(key); \
    if (it == json.end()) { \
        json[key] = defaultValue; \
        return defaultValue; \
    } \
    return it.value();

bool Preferences::GetBool(const std::string& key, bool defaultValue) {
    const char* p = key.c_str();
    boost::mutex::scoped_lock lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

int Preferences::GetInt(const std::string& key, int defaultValue) {
    boost::mutex::scoped_lock lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

double Preferences::GetDouble(const std::string& key, double defaultValue) {
    boost::mutex::scoped_lock lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

std::string Preferences::GetString(const std::string& key, const std::string& defaultValue) {
    boost::mutex::scoped_lock lock(this->mutex);
    RETURN_VALUE(defaultValue);
}

void Preferences::SetBool(const std::string& key, bool value) {
    boost::mutex::scoped_lock lock(this->mutex);
    json[key] = value;
}

void Preferences::SetInt(const std::string& key, int value) {
    boost::mutex::scoped_lock lock(this->mutex);
    json[key] = value;
}

void Preferences::SetDouble(const std::string& key, double value) {
    boost::mutex::scoped_lock lock(this->mutex);
    json[key] = value;
}

void Preferences::SetString(const std::string& key, const char* value) {
    boost::mutex::scoped_lock lock(this->mutex);
    json[key] = value;
}

void Preferences::GetKeys(std::vector<std::string>& target) {
    auto it = json.begin();
    for (; it != json.end(); it++) {
        target.push_back(it.key());
    }
}

void Preferences::Load() {
    std::string str = fileToString(FILENAME(this->component));
    if (str.size()) {
        try {
            this->json = json::parse(str);
        }
        catch (...) {
            std::cerr << "error loading " << this->component << ".json";
            this->json = json::parse("{ }");
        }
    }
}

void Preferences::Save() {
    if (this->mode == ModeReadOnly) {
        throw std::runtime_error("cannot save a ModeReadOnly Preference!");
    }

    stringToFile(FILENAME(this->component), this->json.dump(2));
}