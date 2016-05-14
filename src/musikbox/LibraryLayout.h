#pragma once

#include "LayoutBase.h"
#include "CategoryListView.h"
#include "TrackListView.h"

#include <core/library/ILibrary.h>

using musik::core::LibraryPtr;

class LibraryLayout : public LayoutBase {
    public:
        LibraryLayout(LibraryPtr library);
        ~LibraryLayout(); /* not virtual */

        virtual IWindow* FocusNext();
        virtual IWindow* FocusPrev();
        virtual IWindow* GetFocus();
        virtual void Layout();
        virtual void OnIdle();

    private:
        boost::shared_ptr<CategoryListView> albumList;
        boost::shared_ptr<TrackListView> trackList;
};