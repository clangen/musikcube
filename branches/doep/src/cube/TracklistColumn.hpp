#pragma once

#include <core/config.h>
#include <win32cpp/ListView.hpp>

//////////////////////////////////////////////////////////////////////////////

using namespace win32cpp;

namespace musik { namespace cube {

//////////////////////////////////////////////////////////////////////////////

class TracklistColumn : public win32cpp::ListView::Column{
    public:
        TracklistColumn(const char *metakey,const utfchar *name, int defaultWidth = 100, TextAlignment alignment = TextAlignLeft);
        ~TracklistColumn(void);

        std::string metaKey;
        int metaKeyType;
};

//////////////////////////////////////////////////////////////////////////////

} }     // musik::cube


