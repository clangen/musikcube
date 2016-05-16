#pragma once

#include <sigslot/sigslot.h>

#include "ListWindow.h"
#include "ScrollAdapterBase.h"
#include "CategoryListQuery.h"

#include <core/library/IQuery.h>
#include <core/library/ILibrary.h>

using musik::core::QueryPtr;
using musik::core::LibraryPtr;

class CategoryListView : public ListWindow, public sigslot::has_slots<> {
    public:
        CategoryListView(LibraryPtr library, IWindow *parent = NULL);
        ~CategoryListView(); /* non-virtual for now*/

        void Requery();
        virtual void ProcessMessage(IWindowMessage &message);

    protected:
        virtual IScrollAdapter& GetScrollAdapter();
        void OnQueryCompleted(QueryPtr query);

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
        Adapter *adapter;

        std::shared_ptr<CategoryListQuery> activeQuery;
        std::shared_ptr<std::vector<std::string>> metadata;
};