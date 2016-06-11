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
#include <core/support/Version.h>
#include <boost/format.hpp>

using namespace musik::core;

Version::Version()
: version(0) {
}

Version::Version(VERSION major, VERSION minor, VERSION revision, VERSION build) {
    version = ((major & 0xff) << 48) | ((minor & 0xff) << 32) | ((revision & 0xff) << 16) | (build & 0xff);
}

Version::Version(VERSION version)
: version(version){
}

void Version::setVersion(VERSION major, VERSION minor, VERSION revision, VERSION build){
    version = ((major & 0xff) << 48) | ((minor & 0xff) << 32) | ((revision & 0xff) << 16) | (build & 0xff);
}

void Version::setVersion(VERSION version){
    version = version;
}

Version::~Version(void){
}

std::string Version::getVersion() {
    return boost::str(boost::format("%1%.%2%.%3%.%4%")
        % ((version >> 48) & 0xff)
        % ((version >> 32) & 0xff)
        % ((version >> 16) & 0xff)
        % (version & 0xff));
}

int Version::getMajorVersion() {
    return (int)((version >> 48) & 0xff);
}
int Version::getMinorVersion() {
    return (int)((version >> 32) & 0xff);
}
int Version::getRevisionVersion() {
    return (int)((version >> 16) & 0xff);
}
int Version::getBuildVersion() {
    return (int)(version & 0xff);
}
