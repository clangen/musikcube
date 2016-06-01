#pragma once

#include "IScrollAdapter.h"

namespace cursespp {
    class MultiLineEntry : public IScrollAdapter::IEntry {
        public:
            MultiLineEntry(const std::string& value, int64 attrs = -1);
            virtual ~MultiLineEntry() { }

            virtual size_t GetLineCount();
            virtual std::string GetLine(size_t line);
            virtual void SetWidth(size_t width);
            virtual int64 GetAttrs(size_t line);

            void SetAttrs(int64 attrs);

        private:
            std::string value;
            std::vector<std::string> lines;
            size_t charCount;
            int64 attrs;
            size_t width;
    };
}