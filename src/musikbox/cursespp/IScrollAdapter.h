#pragma once

#include <stdafx.h>

class IScrollAdapter {
    public:
        virtual ~IScrollAdapter() = 0 { }

        struct ScrollPosition {
            ScrollPosition() {
                firstVisibleEntryIndex = 0;
                visibleEntryCount = 0;
                lineCount = 0;
                logicalIndex = 0;
                totalEntries = 0;
            }

            size_t firstVisibleEntryIndex;
            size_t visibleEntryCount;
            size_t lineCount;
            size_t totalEntries;
            size_t logicalIndex;
        };

        class IEntry {
        public:
            virtual ~IEntry() = 0 { }
            virtual size_t GetLineCount() = 0;
            virtual std::string GetLine(size_t line) = 0;
            virtual std::string GetValue() = 0;
            virtual void SetWidth(size_t width) = 0;
            virtual void SetAttrs(int64 attrs) = 0;
            virtual int64 GetAttrs() = 0;
        };

        typedef std::shared_ptr<IEntry> EntryPtr;

        virtual void SetDisplaySize(size_t width, size_t height) = 0;
        virtual size_t GetEntryCount() = 0;
        virtual EntryPtr GetEntry(size_t index) = 0;
        virtual void DrawPage(WINDOW* window, size_t index, ScrollPosition *result = NULL) = 0;
};

