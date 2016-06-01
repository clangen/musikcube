#include <stdafx.h>
#include "SingleLineEntry.h"
#include <boost/algorithm/string.hpp>

using namespace cursespp;

SingleLineEntry::SingleLineEntry(const std::string& value) {
    this->value = value;
    this->attrs = -1;
}

void SingleLineEntry::SetWidth(size_t width) {
    this->width = width;
}

int64 SingleLineEntry::GetAttrs(size_t line) {
    return this->attrs;
}

void SingleLineEntry::SetAttrs(int64 attrs) {
    this->attrs = attrs;
}

size_t SingleLineEntry::GetLineCount() {
    return 1;
}

std::string SingleLineEntry::GetLine(size_t line) {
    return u8substr(this->value, 0, this->width > 0 ? this->width : 0);
}