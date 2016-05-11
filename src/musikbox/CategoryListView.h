#pragma once

#include "ScrollableWindow.h"
#include "SimpleScrollAdapter.h"
#include <core/library/IQuery.h>

using musik::core::QueryPtr;

class CategoryListView : public ScrollableWindow {
    public:
        CategoryListView();
        ~CategoryListView(); /* non-virtual for now*/

    protected:
        virtual IScrollAdapter& GetScrollAdapter();

    private:
        void OnQueryCompleted(QueryPtr query);

        SimpleScrollAdapter adapter;
};