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

#include <core/debug.h>
#include <core/io/LocalFileStream.h>
#include <core/config.h>
#include <core/support/Common.h>
#include <core/config.h>

static const std::string TAG = "LocalFileStream";

using namespace musik::core::io;

LocalFileStream::LocalFileStream()
: file(NULL)
, filesize(-1) {
}

LocalFileStream::~LocalFileStream() {
    try {
        this->Close();
    }
    catch (...) {
        musik::debug::err(TAG, "error closing file");
    }
}

bool LocalFileStream::Open(const char *filename, unsigned int options) {
    try {
        std::string fn(filename);
        debug::info(TAG, "opening file: " + std::string(filename));

        boost::filesystem::path file(filename);

        if (!boost::filesystem::exists(file)) {
            debug::err(TAG, "open failed " + fn);
            return false;
        }

        if (!boost::filesystem::is_regular(file)) {
            debug::err(TAG, "not a regular file" + fn);
            return false;
        }

        this->filesize = (long)boost::filesystem::file_size(file);
        this->extension = file.extension().string();
#ifdef WIN32
        std::wstring u16fn = u8to16(fn);
        this->file = _wfopen(u16fn.c_str(), L"rb");
#else
        this->file = fopen(filename, "rb");
#endif

        if (this->file != NULL) {
            debug::info(TAG, "opened successfully");
            return true;
        }
    }
    catch(...) {
    }

    debug::err(TAG, "open failed " + std::string(filename));
    return false;
}

bool LocalFileStream::Close() {
    if (this->file) {
        if (fclose(this->file) == 0) {
            this->file = NULL;
            return true;
        }
    }

    return false;
}

void LocalFileStream::Destroy() {
    delete this;
}

PositionType LocalFileStream::Read(void* buffer, PositionType readBytes) {
    return (PositionType) fread(buffer, 1, readBytes, this->file);
}

bool LocalFileStream::SetPosition(PositionType position) {
    return fseek(this->file, position, SEEK_SET) == 0;
}

PositionType LocalFileStream::Position() {
    return ftell(this->file);
}

bool LocalFileStream::Eof() {
    return feof(this->file) != 0;
}

long LocalFileStream::Length() {
    return this->filesize;
}

const char* LocalFileStream::Type() {
    return this->extension.c_str();
}
