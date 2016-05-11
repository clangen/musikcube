#pragma once

#include "stdafx.h"
#include "CategoryListView.h"

CategoryListView::CategoryListView() {

}

CategoryListView::~CategoryListView() {

}

IScrollAdapter& CategoryListView::GetScrollAdapter() {
    return adapter;
}

void CategoryListView::OnQueryCompleted(QueryPtr query) {

}
