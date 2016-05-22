#pragma once

#include <sigslot/sigslot.h>

#include <cursespp/ListWindow.h>
#include <cursespp/ScrollAdapterBase.h>

#include <app/query/CategoryListViewQuery.h>

#include <core/library/IQuery.h>
#include <core/library/ILibrary.h>

using musik::core::QueryPtr;
using musik::core::LibraryPtr;

class CategoryListView : public ListWindow, public sigslot::has_slots<> {
    public:
        CategoryListView(LibraryPtr library, const std::string& fieldName);
        virtual ~CategoryListView();

        void Requery();

        virtual void ProcessMessage(IMessage &message);

        DBID GetSelectedId();
        std::string GetFieldName();
        void SetFieldName(const std::string& fieldName);

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

        std::string fieldName;
        std::shared_ptr<CategoryListViewQuery> activeQuery;
        CategoryListViewQuery::ResultList metadata;
};