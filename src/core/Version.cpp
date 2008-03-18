//////////////////////////////////////////////////////////////////////////////
//
// License Agreement:
//
// The following are Copyright © 2008, Daniel Önnerby
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
#include <core/Version.h>
#include <boost/format.hpp>

using namespace musik::core;

Version::Version(void):iVersion(0){
}

Version::Version(VERSION iMajor,VERSION iMinor,VERSION iRevision,VERSION iBuild){
    iVersion    = ((iMajor&0xff)<<48) | ((iMinor&0xff)<<32) | ((iRevision&0xff)<<16) | (iBuild&0xff);
}

Version::Version(VERSION iNewVersion):iVersion(iNewVersion){
}

void Version::setVersion(VERSION iMajor,VERSION iMinor,VERSION iRevision,VERSION iBuild){
    iVersion    = ((iMajor&0xff)<<48) | ((iMinor&0xff)<<32) | ((iRevision&0xff)<<16) | (iBuild&0xff);
}

void Version::setVersion(VERSION iNewVersion){
    iVersion    = iNewVersion;
}

Version::~Version(void){
}

utfstring Version::getVersion(){
    utfstring sVersion;
    sVersion = boost::str( boost::wformat(UTF("%1%.%2%.%3%.%4%")) %
        ((iVersion>>48) & 0xff) %
        ((iVersion>>32) & 0xff) %
        ((iVersion>>16) & 0xff) %
        (iVersion & 0xff) );
    return sVersion;
}

int Version::getMajorVersion(){
    return (int)((iVersion>>48) & 0xff);
}
int Version::getMinorVersion(){
    return (int)((iVersion>>32) & 0xff);
}
int Version::getRevisionVersion(){
    return (int)((iVersion>>16) & 0xff);
}
int Version::getBuildVersion(){
    return (int)(iVersion & 0xff);
}
