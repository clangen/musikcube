#include "pch.hpp"
#include "TracklistColumn.hpp"

using namespace musik::cube;

TracklistColumn::TracklistColumn(const char *metakey,const utfchar *name, int defaultWidth, TextAlignment alignment) : 
    win32cpp::ListView::Column(name,defaultWidth,alignment),
    metaKey(metakey)
{
}

TracklistColumn::~TracklistColumn(void){
}
