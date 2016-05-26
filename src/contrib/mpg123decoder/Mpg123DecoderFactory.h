#pragma once

#include <core/sdk/IDecoderFactory.h>

class Mpg123DecoderFactory : public musik::core::audio::IDecoderFactory {
  public:
      Mpg123DecoderFactory();
      virtual ~Mpg123DecoderFactory();

      musik::core::audio::IDecoder* CreateDecoder();
      void Destroy();
      bool CanHandle(const char* type) const;
};
