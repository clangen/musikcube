#pragma once

#include "ILayout.h"
#include "CategoryListView.h"

class LibraryLayout : public ILayout {
    public:
        LibraryLayout();
        ~LibraryLayout(); /* not virtual */

        virtual IWindow* FocusNext();
        virtual IWindow* FocusPrev();
        virtual IWindow* GetFocus();
        virtual void Layout();
        virtual void OnIdle();

    private:
        CategoryListView albumList;
};