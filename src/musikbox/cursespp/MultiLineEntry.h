#pragma once

#include "IScrollAdapter.h"

namespace cursespp {
    class MultiLineEntry : public IScrollAdapter::IEntry {
        public:
            MultiLineEntry(const std::string& value, int64 attrs = -1);

            size_t GetIndex();
            void SetIndex(size_t index);
            size_t GetLineCount();
            std::string GetLine(size_t line);
            std::string GetValue();
            void SetWidth(size_t width);
            void SetAttrs(int64 attrs);
            int64 GetAttrs();

        private:
            size_t index;
            std::string value;
            std::vector<std::string> lines;
            size_t charCount;
            int64 attrs;
            size_t width;
    };
}