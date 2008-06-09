#include "pch.hpp"
#include "TracklistColumn.hpp"

#include <core/MetaKey.h>

using namespace musik::cube;

TracklistColumn::TracklistColumn(const char *metakey,const utfchar *name, int defaultWidth, TextAlignment alignment) 
: win32cpp::ListView::Column(name,defaultWidth,alignment)
, metaKey(metakey)
, metaKeyType(musik::core::MetaKey::TypeOf(metakey))
{
}

TracklistColumn::~TracklistColumn(void){
}
