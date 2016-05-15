#pragma once

#include "LayoutBase.h"
#include "CategoryListView.h"
#include "TrackListView.h"

#include <core/library/ILibrary.h>

using musik::core::LibraryPtr;

class LibraryLayout : public LayoutBase {
    public:
        LibraryLayout(LibraryPtr library);
        virtual ~LibraryLayout();

        virtual void Layout();
        virtual void OnIdle();
        virtual void Show();

    private:
        void UpdateWindows();
        void InitializeWindows();

        LibraryPtr library;
        std::shared_ptr<CategoryListView> albumList;
        std::shared_ptr<TrackListView> trackList;
};