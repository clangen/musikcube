#pragma once

#include <sigslot/sigslot.h>

#include "ScrollableWindow.h"
#include "ScrollAdapterBase.h"

#include <core/library/IQuery.h>
#include <core/library/ILibrary.h>

using musik::core::QueryPtr;
using musik::core::LibraryPtr;

class CategoryListView : public ScrollableWindow, public sigslot::has_slots<> {
    public:
        CategoryListView(LibraryPtr library);
        ~CategoryListView(); /* non-virtual for now*/

        void OnIdle();

    protected:
        virtual IScrollAdapter& GetScrollAdapter();

        class Adapter : public ScrollAdapterBase {
            public:
                Adapter(CategoryListView &parent);

                virtual size_t GetEntryCount();
                virtual EntryPtr GetEntry(size_t index);

            private:
                CategoryListView &parent;
                IScrollAdapter::ScrollPosition spos;
        };

    private:
        LibraryPtr library;
        QueryPtr activeQuery;
        Adapter *adapter;

        boost::shared_ptr<std::vector<std::string>> metadata;
};