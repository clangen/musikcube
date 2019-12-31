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

#include "pch.hpp"

#include <core/debug.h>
#include <core/io/LocalFileStream.h>
#include <core/config.h>
#include <core/support/Common.h>
#include <core/config.h>
#include <cstdio>
#include <iostream>

static const std::string TAG = "LocalFileStream";

using namespace musik::core::io;
using namespace musik::core::sdk;

LocalFileStream::LocalFileStream()
: file(nullptr)
, filesize(-1) {
}

LocalFileStream::~LocalFileStream() {
    try {
        this->Close();
    }
    catch (...) {
        musik::debug::error(TAG, "error closing file");
    }
}

bool LocalFileStream::Seekable() {
    return true;
}

bool LocalFileStream::Open(const char *filename, OpenFlags flags) {
    try {
        this->uri = filename;
        debug::info(TAG, "opening file: " + std::string(filename));

        boost::filesystem::path file(filename);
        bool exists = boost::filesystem::exists(file);

        if (flags & OpenFlags::Read && !exists) {
            debug::error(TAG, "open with OpenFlags::Read failed because file doesn't exist. " + this->uri);
            return false;
        }

        if (exists && !boost::filesystem::is_regular(file)) {
            debug::error(TAG, "not a regular file" + this->uri);
            return false;
        }

        boost::system::error_code ec;
        this->filesize = (long) boost::filesystem::file_size(file, ec);

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

        this->extension = file.extension().string();
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

void LocalFileStream::Interrupt() {

}

bool LocalFileStream::Close() {
    auto file = this->file.exchange(nullptr);
    if (file) {
        if (fclose(file) == 0) {
            return true;
        }
    }

    return false;
}

void LocalFileStream::Release() {
    delete this;
}

PositionType LocalFileStream::Read(void* buffer, PositionType readBytes) {
    if (!this->file.load()) {
        return 0;
    }

    return (PositionType) fread(buffer, 1, readBytes, this->file);
}

PositionType LocalFileStream::Write(void* buffer, PositionType writeBytes) {
    if (!this->file.load()) {
        return 0;
    }

    long position = ftell(this->file);
    size_t written = fwrite(buffer, 1, writeBytes, this->file);
    if (written + position > this->filesize) {
        this->filesize = written + position;
    }

    return (PositionType) written;
}


bool LocalFileStream::SetPosition(PositionType position) {
    if (!this->file.load()) {
        return false;
    }

    return fseek(this->file, position, SEEK_SET) == 0;
}

PositionType LocalFileStream::Position() {
    if (!this->file.load()) {
        return -1;
    }

    return ftell(this->file);
}

bool LocalFileStream::Eof() {
    return !this->file.load() || feof(this->file) != 0;
}

long LocalFileStream::Length() {
    return this->filesize;
}

const char* LocalFileStream::Type() {
    return this->extension.c_str();
}

const char* LocalFileStream::Uri() {
    return this->uri.c_str();
}
