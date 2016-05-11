#include "stdafx.h"
#include "SingleLineEntry.h"
#include <boost/algorithm/string.hpp>

SingleLineEntry::SingleLineEntry(const std::string& value) {
    this->value = value;
}

size_t SingleLineEntry::GetIndex() {
    return this->index;
}

void SingleLineEntry::SetWidth(size_t width) {
    this->width = width;
}

void SingleLineEntry::SetIndex(size_t index) {
    this->index = index;
}

int64 SingleLineEntry::GetAttrs() {
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
std::string SingleLineEntry::GetValue() {
    return this->value;
}