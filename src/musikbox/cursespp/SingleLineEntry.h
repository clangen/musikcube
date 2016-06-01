#pragma once

#include "IScrollAdapter.h"

namespace cursespp {
    class SingleLineEntry : public IScrollAdapter::IEntry {
        public:
            SingleLineEntry(const std::string& value);
            virtual ~SingleLineEntry() { }

            virtual void SetWidth(size_t width);
            virtual int64 GetAttrs(size_t line);
            virtual size_t GetLineCount();
            virtual std::string GetLine(size_t line);

            void SetAttrs(int64 attrs);
            
        private:
            size_t width;
            std::string value;
            int64 attrs;
    };
}