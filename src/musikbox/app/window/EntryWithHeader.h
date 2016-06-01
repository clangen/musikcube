#pragma once

#include <cursespp/IScrollAdapter.h>

namespace musik {
    namespace box {
        class EntryWithHeader : public cursespp::IScrollAdapter::IEntry {
            public:
                EntryWithHeader(const std::string& header, const std::string& value);
                virtual ~EntryWithHeader() { }

                virtual void SetWidth(size_t width);
                virtual int64 GetAttrs(size_t line);
                virtual size_t GetLineCount();
                virtual std::string GetLine(size_t line);

                void SetAttrs(int64 headerAttrs, int64 attrs);

            private:
                size_t width;
                std::string header, value;
                int64 headerAttrs, attrs;
        };
    }
}