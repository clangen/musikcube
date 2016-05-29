#include "stdafx.h"

#include <string>
#include <algorithm>
#include "Mpg123DecoderFactory.h"
#include "Mpg123Decoder.h"
#include "mpg123.h"

using namespace musik::core::audio;

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

  if (musik::sdk::endsWith(str, ".mp3") ||
      str.find("audio/mpeg3") != std::string::npos ||
      str.find("audio/x-mpeg-3") != std::string::npos ||
      str.find("audio/mp3") != std::string::npos)
  {
      return true;
  }

  return false;
}
