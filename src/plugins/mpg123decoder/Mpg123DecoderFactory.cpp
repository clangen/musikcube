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

#include "stdafx.h"

#include <string>
#include <algorithm>
#include "Mpg123DecoderFactory.h"
#include "Mpg123Decoder.h"
#include "mpg123.h"

using namespace musik::core::sdk;

inline bool endsWith(const std::string& s, const std::string& suffix) {
    return
        s.size() >= suffix.size() &&
        s.rfind(suffix) == (s.size() - suffix.size());
}

Mpg123DecoderFactory::Mpg123DecoderFactory() {
    mpg123_init();
}

Mpg123DecoderFactory::~Mpg123DecoderFactory() {
    mpg123_exit();
}

void Mpg123DecoderFactory::Destroy() {
    delete this;
}

IDecoder* Mpg123DecoderFactory::CreateDecoder() {
    return new Mpg123Decoder();
}

bool Mpg123DecoderFactory::CanHandle(const char* type) const {
  std::string str(type);
  std::transform(str.begin(), str.end(), str.begin(), tolower);

  if (endsWith(str, ".mp3") ||
      str.find("audio/mpeg3") != std::string::npos ||
      str.find("audio/x-mpeg-3") != std::string::npos ||
      str.find("audio/mp3") != std::string::npos)
  {
      return true;
  }

  return false;
}
