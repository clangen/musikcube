//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2004-2023 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#pragma once

/**
 * @file TrackListView.h
 * @brief List window that displays a list of tracks.
 * @details Renders a TrackList with optional header and separator rows,
 *          supports per-row decorators and custom row renderers, keeps the
 *          playing track highlighted and emits a signal when it is requeried.
 */

#include <cursespp/curses_config.h>
#include <cursespp/Colors.h>
#include <cursespp/ScrollAdapterBase.h>
#include <cursespp/IKeyHandler.h>
#include <cursespp/ListWindow.h>
#include <cursespp/SingleLineEntry.h>

#include <musikcore/library/query/TrackListQueryBase.h>
#include <musikcore/audio/PlaybackService.h>

#include <musikcore/runtime/IMessage.h>
#include <musikcore/library/ILibrary.h>

#include <app/util/TrackRowRenderers.h>

namespace musik {
    namespace cube {
        /**
         * @brief A scrollable list of tracks with group headers.
         * @details Displays the tracks of a TrackList, inserting separator
         *          rows at album boundaries. Supports activating and
         *          context-menu operations, and redraws itself when the
         *          playing track changes.
         */
        class TrackListView:
            public cursespp::ListWindow,
            public sigslot::has_slots<>
        {
            public:
                typedef musik::core::TrackPtr TrackPtr;
                typedef musik::core::library::query::TrackListQueryBase TrackListQueryBase;

                /* events */
                /**
                 * @brief Emitted when the view is requeried with a new query.
                 */
                sigslot::signal1<musik::core::library::query::TrackListQueryBase*> Requeried;

                /* types */
                using RowDecorator = std::function<cursespp::Color(TrackPtr, size_t)>;
                using Headers = TrackListQueryBase::Headers;
                using Durations = TrackListQueryBase::Durations;

                /* ctor, dtor */
                /**
                 * @brief Creates the list bound to the given playback service
                 *        and library.
                 * @param playback the playback service providing track changes
                 * @param library the library providing track metadata
                 * @param decorator optional per-row color decorator
                 */
                TrackListView(
                    musik::core::audio::PlaybackService& playback,
                    musik::core::ILibraryPtr library,
                    RowDecorator decorator = RowDecorator());

                /* IWindow */
                /**
                 * @brief Processes runtime messages.
                 * @param message the message to process
                 */
                void ProcessMessage(musik::core::runtime::IMessage &message) override;
                /**
                 * @brief Handles keyboard input.
                 * @param key the key sequence that was pressed
                 * @return true if the event was consumed
                 */
                bool KeyPress(const std::string& key) override;

                /* regular methods */
                /**
                 * @brief Returns the currently displayed track list.
                 * @return the shared track list
                 */
                std::shared_ptr<musik::core::TrackList> GetTrackList() noexcept;
                /**
                 * @brief Replaces the displayed track list.
                 * @param trackList the new track list
                 */
                void SetTrackList(std::shared_ptr<musik::core::TrackList> trackList);
                /**
                 * @brief Returns the currently selected track.
                 * @return the selected track, or nullptr
                 */
                musik::core::TrackPtr GetSelectedTrack();
                /**
                 * @brief Returns the index of the selected track.
                 * @return the track index, or a negative sentinel
                 */
                size_t GetSelectedTrackIndex();
                /**
                 * @brief Converts a track index to an adapter index.
                 * @param index the track index
                 * @return the adapter row index
                 */
                size_t TrackIndexToAdapterIndex(size_t index);
                /**
                 * @brief Returns the number of displayed tracks.
                 * @return the track count
                 */
                size_t TrackCount() noexcept;
                /**
                 * @brief Returns the number of rows including separators.
                 * @return the entry count
                 */
                size_t EntryCount();
                /**
                 * @brief Marks the displayed data as stale for redraw.
                 */
                void InvalidateData();

                /**
                 * @brief Sets how the track number is derived.
                 * @param type the track number type
                 */
                void SetTrackNumType(TrackRowRenderers::TrackNumType type);
                /**
                 * @brief Sets the function used to render each row.
                 * @param renderer the row renderer
                 */
                void SetRowRenderer(TrackRowRenderers::Renderer renderer);

                /**
                 * @brief Clears the current data and selection.
                 */
                void Reset();
                /**
                 * @brief Re-queries the view with the given query.
                 * @param query the query that will produce the track list
                 */
                void Requery(std::shared_ptr<TrackListQueryBase> query);

            protected:
                /* IScrollableWindow */
                /**
                 * @brief Returns the scroll adapter feeding the list.
                 * @return the adapter
                 */
                cursespp::IScrollAdapter& GetScrollAdapter() noexcept override;
                /**
                 * @brief Handles activation of an entry.
                 * @param index the activated entry index
                 * @return true if the activation was consumed
                 */
                bool OnEntryActivated(size_t index) override;
                /**
                 * @brief Handles the context menu on an entry.
                 * @param index the entry index
                 * @return true if a menu was shown
                 */
                bool OnEntryContextMenu(size_t index) override;
                /**
                 * @brief Called when the window dimensions change.
                 */
                void OnDimensionsChanged() override;

                /**
                 * @brief Handles the track list cache window being filled.
                 * @param track the track list that changed
                 * @param from the first updated index
                 * @param to the last updated index
                 */
                void OnTrackListWindowCached(
                    const musik::core::TrackList* track, size_t from, size_t to);

                /**
                 * @brief Replaces the track list and its event handlers.
                 * @param trackList the new track list
                 */
                void SetTrackListAndUpateEventHandlers(
                    std::shared_ptr<musik::core::TrackList> trackList);

                /**
                 * @brief Handles completion of the track list query.
                 * @param query the completed query
                 */
                void OnQueryCompleted(musik::core::db::IQuery* query);
                /**
                 * @brief Shows the context menu for the selection.
                 */
                void ShowContextMenu();

                /* this view has headers and track entry types */
                /**
                 * @brief The type of a rendered row.
                 */
                enum class RowType : char { Track = 't', Separator = 's' };

                /* our special type of list entry */
                /**
                 * @brief A list entry that carries its row type and index.
                 */
                class TrackListEntry : public cursespp::SingleLineEntry {
                    public:
                        /**
                         * @brief Creates an entry with the given text.
                         * @param str the entry text
                         * @param index the underlying track index
                         * @param type the row type
                         */
                        TrackListEntry(const std::string& str, int index, RowType type)
                            : cursespp::SingleLineEntry(str), index(index), type(type) { }

                        /**
                         * @brief Returns the row type.
                         * @return the row type
                         */
                        RowType GetType() noexcept { return type; }
                        /**
                         * @brief Returns the underlying track index.
                         * @return the track index
                         */
                        int GetIndex() noexcept { return index; }

                    private:
                        RowType type;   /**< the row type */
                        int index;      /**< the underlying track index */
                };

                /* our adapter */
                /**
                 * @brief Adapter that renders track and separator rows.
                 */
                class Adapter : public cursespp::ScrollAdapterBase {
                    public:
                        /**
                         * @brief Creates the adapter for the given parent.
                         * @param parent the owning TrackListView
                         */
                        Adapter(TrackListView &parent);
                        /**
                         * @brief Returns the number of rows.
                         * @return the row count
                         */
                        size_t GetEntryCount() noexcept override;
                        /**
                         * @brief Returns the entry for the given index.
                         * @param window the owning scrollable window
                         * @param index the row index
                         * @return the entry to render
                         */
                        EntryPtr GetEntry(cursespp::ScrollableWindow* window, size_t index) override;

                    private:
                        TrackListView &parent;                      /**< the owning list view */
                        IScrollAdapter::ScrollPosition spos;        /**< the persisted scroll position */
                };

            private:
                /* class to help with header offset calculation. this thing is really gross and
                should probably be refactored at some point. */
                /**
                 * @brief Maps between track indices and adapter row indices.
                 * @details Accounts for the separator rows inserted at album
                 *          boundaries so that selections can be translated
                 *          between the track list and the adapter.
                 */
                class HeaderCalculator {
                    public:
                        static const size_t NO_INDEX = (size_t) -1;

                        /**
                         * @brief Initializes the calculator with raw offsets.
                         * @param rawOffsets the header offsets
                         * @param durations the durations for each header
                         */
                        void Set(Headers rawOffsets, Durations durations);
                        /**
                         * @brief Clears the cached offsets.
                         */
                        void Reset() noexcept;
                        /**
                         * @brief Returns true if a header sits at the index.
                         * @param index the adapter index
                         * @return true if the row is a header
                         */
                        bool HeaderAt(size_t index);
                        /**
                         * @brief Converts an adapter index to a track index.
                         * @param index the adapter index
                         * @return the track index
                         */
                        size_t AdapterToTrackListIndex(size_t index);
                        /**
                         * @brief Converts a track index to an adapter index.
                         * @param index the track index
                         * @return the adapter index
                         */
                        size_t TrackListToAdapterIndex(size_t index);
                        /**
                         * @brief Returns the duration at an adapter index.
                         * @param index the adapter index
                         * @return the total duration of the group
                         */
                        size_t DurationFromAdapterIndex(size_t index);
                        /**
                         * @brief Returns the next header after the index.
                         * @param selectedIndex the current index
                         * @return the next header index, or NO_INDEX
                         */
                        size_t NextHeaderIndex(size_t selectedIndex) noexcept;
                        /**
                         * @brief Returns the previous header before the index.
                         * @param selectedIndex the current index
                         * @return the previous header index, or NO_INDEX
                         */
                        size_t PrevHeaderIndex(size_t selectedIndex) noexcept;
                        /**
                         * @brief Returns the number of headers.
                         * @return the header count
                         */
                        size_t Count() noexcept;

                    private:
                        size_t ApplyHeaderOffset(size_t index, Headers offsets, int delta);

                        Headers absoluteOffsets;    /**< offsets in adapter space */
                        Headers rawOffsets;         /**< offsets in track space */
                        Durations durations;        /**< the duration of each group */
                };

                /**
                 * @brief Highlights the row when the playing track changes.
                 * @param index the index of the changed track
                 * @param track the changed track
                 */
                void OnTrackChanged(size_t index, musik::core::TrackPtr track);

                /**
                 * @brief Adjusts the track list cache window size.
                 */
                void AdjustTrackListCacheWindowSize();

                /**
                 * @brief Scrolls the list so the playing row is visible.
                 */
                void ScrollToPlaying();
                /**
                 * @brief Selects the first track in the list.
                 */
                void SelectFirstTrack();

                std::shared_ptr<TrackListQueryBase> query;  /**< the active query */
                std::shared_ptr<musik::core::TrackList> tracks; /**< the displayed track list */
                HeaderCalculator headers;                   /**< header offset calculator */
                std::unique_ptr<Adapter> adapter;           /**< the scroll adapter */
                musik::core::audio::PlaybackService& playback; /**< the playback service */
                musik::core::TrackPtr playing;              /**< the currently playing track */
                musik::core::ILibraryPtr library;           /**< the library providing metadata */
                size_t lastQueryHash;                       /**< hash of the last query */
                RowDecorator decorator;                     /**< per-row color decorator */
                TrackRowRenderers::Renderer renderer;       /**< the row renderer */
                std::chrono::milliseconds lastChanged;      /**< when the selection last changed */
                TrackRowRenderers::TrackNumType trackNumType; /**< how track numbers are derived */
        };
    }
}
