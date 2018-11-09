//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2017 musikcube team
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

#include "ExternalArtReader.h"

#include <vector>
#include <string>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>

using namespace musik::core::sdk;

ExternalArtReader::ExternalArtReader() {
}

ExternalArtReader::~ExternalArtReader() {
}

void ExternalArtReader::Release() {
    delete this;
}

bool ExternalArtReader::CanRead(const char *extension){
    if (extension) {
        std::string ext(extension);
        boost::algorithm::to_lower(ext);

        return
            ext.compare("mp3") == 0 ||
            ext.compare("ogg") == 0 ||
            ext.compare("aac") == 0 ||
            ext.compare("m4a") == 0 ||
            ext.compare("flac") == 0 ||
            ext.compare("ape") == 0 ||
            ext.compare("mpc") == 0;
    }

    return false;
}

bool ExternalArtReader::Read(const char* uri, ITagStore *track) {
    std::string path(uri);
    std::string folder;

    std::string::size_type lastSlash = path.find_last_of("/");
    if (lastSlash != std::string::npos) {
        folder = path.substr(0, lastSlash + 1).c_str();
    }

    bool success = true;

    std::string album = "cover.jpg";
    if (Exists(folder + album)) {
        folder = folder + album;
    }
    
    std::ifstream in(folder);
    std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    
    track->SetThumbnail(contents.data(), contents.size());

    return true;
}

bool ExternalArtReader::Exists(const std::string& path) {
    if (FILE *file = fopen(path.c_str(), "r")) {
        fclose(file);
        return true;
    } else {
        return false;
    }   
}