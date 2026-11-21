#pragma once

#include <musikcore/locale.hpp>

#define _TSTR(KEY) (musik::core::api::Translate(KEY))
#define _TCP(KEY) (musik::core::api::Translate(KEY).c_str())
#define _DIMEN(KEY, DEFAULT) (musik::core::api::LocaleInteger(KEY, DEFAULT))
