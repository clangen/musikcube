#pragma once

#include "IScrollAdapter.h"

namespace cursespp {
    class SingleLineEntry : public IScrollAdapter::IEntry {
        public:
            SingleLineEntry(const std::string& value);

            size_t GetIndex();
            void SetIndex(size_t index);
            void SetWidth(size_t width);
            void SetAttrs(int64 attrs);
            int64 GetAttrs();

            size_t GetLineCount();
            std::string GetLine(size_t line);
            std::string GetValue();

        private:
            size_t index, width;
            std::string value;
            int64 attrs;
    };
}