#include <stdafx.h>
#include "EntryWithHeader.h"
#include <boost/algorithm/string.hpp>
#include <app/util/Text.h>

using namespace musik::box;

EntryWithHeader::EntryWithHeader(
    const std::string& header, const std::string& value) 
{
    this->header = header;
    this->value = value;
    this->attrs = -1;
}

void EntryWithHeader::SetWidth(size_t width) {
    this->width = width;
}

int64 EntryWithHeader::GetAttrs(size_t line) {
    return (line == 0) ? this->headerAttrs : this->attrs;
}

void EntryWithHeader::SetAttrs(int64 headerAttrs, int64 attrs) {
    this->headerAttrs = headerAttrs;
    this->attrs = attrs;
}

size_t EntryWithHeader::GetLineCount() {
    return 2;
}

#define TRUNCATE(value, width) \
    u8substr(value, 0, width > 0 ? width : 0)

std::string EntryWithHeader::GetLine(size_t line) {
    if (line == 0) {
        std::string ellipsized = this->header;
        musik::box::text::Ellipsize(ellipsized, this->width);
        return ellipsized;
    }

    return TRUNCATE(this->value, this->width);
}
