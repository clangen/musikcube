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

#include "pch.hpp"

#include <musikcore/debug.h>
#include <musikcore/io/LocalFileStream.h>
#include <musikcore/config.h>
#include <musikcore/support/Common.h>
#include <musikcore/config.h>
#include <cstdio>
#include <iostream>
#include <filesystem>

static const std::string TAG = "LocalFileStream";

using namespace musik::core::io;
using namespace musik::core::sdk;

LocalFileStream::LocalFileStream() noexcept
: file(nullptr)
, filesize(-1) {
}

LocalFileStream::~LocalFileStream() noexcept {
    try {
        this->Close();
    }
    catch (...) {
        musik::debug::error(TAG, "error closing file");
    }
}

bool LocalFileStream::Seekable() noexcept {
    return true;
}

bool LocalFileStream::Open(const char *filename, OpenFlags flags) {
    try {
        this->uri = filename;
        debug::info(TAG, "opening file: " + std::string(filename));

        std::filesystem::path file(std::filesystem::u8path(filename));
        const bool exists = std::filesystem::exists(file);

        if (flags & OpenFlags::Read && !exists) {
            debug::error(TAG, "open with OpenFlags::Read failed because file doesn't exist. " + this->uri);
            return false;
        }

        if (exists && !std::filesystem::is_regular_file(file)) {
            debug::error(TAG, "not a regular file" + this->uri);
            return false;
        }

        std::error_code ec;
        this->filesize = narrow_cast<long>(std::filesystem::file_size(file, ec));

        if (ec && flags & OpenFlags::Write) {
            this->filesize = 0;
        }

        /* convert the OpenFlags bitmask to an fopen compatible string */
        std::string openFlags = "";
        if (flags & OpenFlags::Read) {
            openFlags += "rb";
        }

        if (flags & OpenFlags::Write) {
            if (openFlags.size() == 2) {
                openFlags += "+";
            }
            else {
                this->filesize = 0;
                openFlags = "wb";
            }
        }

        this->extension = file.extension().u8string();
#ifdef WIN32
        std::wstring u16fn = u8to16(this->uri);
        std::wstring u16flags = u8to16(openFlags);
        this->file = _wfopen(u16fn.c_str(), u16flags.c_str());
#else
        this->file = fopen(filename, openFlags.c_str());
#endif

        if (this->file.load()) {
            this->flags = flags;
            return true;
        }
    }
    catch(...) {
    }

    debug::error(TAG, "open failed " + std::string(filename));
    return false;
}

void LocalFileStream::Interrupt() noexcept {

}

bool LocalFileStream::Close() noexcept {
    auto file = this->file.exchange(nullptr);
    if (file) {
        if (fclose(file) == 0) {
            return true;
        }
    }

    return false;
}

void LocalFileStream::Release() noexcept {
    delete this;
}

PositionType LocalFileStream::Read(void* buffer, PositionType readBytes) noexcept {
    if (!this->file.load()) {
        return 0;
    }

    return narrow_cast<PositionType>(fread(buffer, 1, readBytes, this->file));
}

PositionType LocalFileStream::Write(void* buffer, PositionType writeBytes) noexcept {
    if (!this->file.load()) {
        return 0;
    }

    const long position = ftell(this->file);
    const size_t written = fwrite(buffer, 1, writeBytes, this->file);
    if (written + position > this->filesize) {
        this->filesize = narrow_cast<int>(written) + position;
    }

    return narrow_cast<PositionType>(written);
}


bool LocalFileStream::SetPosition(PositionType position) noexcept {
    if (!this->file.load()) {
        return false;
    }

    return fseek(this->file, position, SEEK_SET) == 0;
}

PositionType LocalFileStream::Position() noexcept {
    if (!this->file.load()) {
        return -1;
    }

    return ftell(this->file);
}

bool LocalFileStream::Eof() noexcept {
    return !this->file.load() || feof(this->file) != 0;
}

long LocalFileStream::Length() noexcept {
    return this->filesize;
}

const char* LocalFileStream::Type() noexcept {
    return this->extension.c_str();
}

const char* LocalFileStream::Uri() noexcept {
    return this->uri.c_str();
}
