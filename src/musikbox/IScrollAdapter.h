#pragma once

#include "stdafx.h"

class IScrollAdapter {
    public:
        virtual void SetDisplaySize(size_t width, size_t height) = 0;
        virtual size_t GetLineCount() = 0;
        virtual size_t GetEntryCount() = 0;
        virtual void DrawPage(WINDOW* window, size_t index) = 0;

        class IEntry { /* can we slim this down? */
            public:
                virtual size_t GetIndex() = 0;
                virtual void SetIndex(size_t index) = 0;
                virtual size_t GetLineCount() = 0;
                virtual std::string GetLine(size_t line) = 0;
                virtual std::string GetValue() = 0;
                virtual void SetWidth(size_t width) = 0;
                virtual void SetAttrs(int64 attrs) = 0;
                virtual int64 GetAttrs() = 0;
        };
};

