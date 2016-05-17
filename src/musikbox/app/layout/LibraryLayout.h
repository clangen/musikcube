#pragma once

#include <cursespp/LayoutBase.h>

#include <app/window/CategoryListView.h>
#include <app/window/TrackListView.h>

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

    private:
        void InitializeWindows();

        void OnCategoryViewSelectionChanged(
            ListWindow *view, size_t newIndex, size_t oldIndex);

        Transport* transport;
        LibraryPtr library;
        std::shared_ptr<CategoryListView> albumList;
        std::shared_ptr<TrackListView> trackList;
};