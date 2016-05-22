#pragma once

#include <cursespp/LayoutBase.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>
#include <app/window/TransportWindow.h>

#include <core/playback/Transport.h>
#include <core/library/ILibrary.h>

#include <sigslot/sigslot.h>

using musik::core::LibraryPtr;
using musik::core::audio::Transport;

class LibraryLayout : public LayoutBase, public sigslot::has_slots<> {
    public:
        LibraryLayout(Transport& transport, LibraryPtr library);
        virtual ~LibraryLayout();

        virtual void Layout();
        virtual void Show();
        virtual bool LibraryLayout::KeyPress(int64 ch);

    private:
        void InitializeWindows();

        void RequeryTrackList(ListWindow *view);

        void OnCategoryViewSelectionChanged(
            ListWindow *view, size_t newIndex, size_t oldIndex);

        void OnCategoryViewInvalidated(
            ListWindow *view, size_t selectedIndex);

        Transport& transport;
        LibraryPtr library;
        std::shared_ptr<CategoryListView> categoryList;
        std::shared_ptr<TrackListView> trackList;
        std::shared_ptr<TransportWindow> transportView;
};