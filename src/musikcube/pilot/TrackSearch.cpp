#include <musikcore/musikcore.hpp>

#include <cursespp/curses_config.h>
#include <algorithm>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
    using namespace musik::core::api;
    using Clock = std::chrono::steady_clock;
    constexpr auto debounce = std::chrono::milliseconds(300);
    constexpr size_t viewport = 8;
    constexpr auto cachePagesKey = "MUSIKCUBE_SEARCH_CACHE_PAGES";
    constexpr auto prefetchPagesKey = "MUSIKCUBE_SEARCH_PREFETCH_PAGES";

    size_t readSize(const char *key, size_t fallback) {
        const auto *raw = std::getenv(key);
        if (!raw) return fallback;
        const std::string_view text(raw);
        size_t value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
            throw std::runtime_error(std::string(key) + " must be a nonnegative integer");
        }
        return value;
    }

    /* Copyable and movable. Limits are measured in viewports. */
    struct CacheOptions {
        size_t capacity = TrackResultWindow::Options{}.capacity;
        size_t prefetch = 1;
    };

    CacheOptions cacheOptions() {
        CacheOptions value;
        value.capacity = readSize(cachePagesKey, value.capacity);
        value.prefetch = readSize(prefetchPagesKey, std::min(value.prefetch, value.capacity ? value.capacity - 1 : 0));
        if (!value.capacity || value.prefetch >= value.capacity) {
            throw std::runtime_error("Cache pages must be positive and prefetch pages must be smaller than cache pages");
        }
        return value;
    }

    /* The dispatcher borrows this noncopyable, nonmovable wakeup pipe. */
    class Wakeup {
    public:
        Wakeup() {
            if (pipe(fds) != 0) throw std::runtime_error("Could not open wakeup pipe");
            for (const auto fd : fds) {
                if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
                    close(fds[0]);
                    close(fds[1]);
                    throw std::runtime_error("Could not configure wakeup pipe");
                }
            }
        }
        ~Wakeup() { close(fds[0]); close(fds[1]); }
        Wakeup(const Wakeup &) = delete;
        Wakeup &operator=(const Wakeup &) = delete;

        static void MC_CALL Signal(void *data) {
            const char byte = 0;
            const auto fd = static_cast<Wakeup *>(data)->fds[1];
            /* A full pipe already has a pending wakeup. */
            while (write(fd, &byte, 1) < 0 && errno == EINTR) {}
        }

        int Descriptor() const { return fds[0]; }
        void Drain() const {
            char bytes[128];
            ssize_t count;
            do {
                count = read(fds[0], bytes, sizeof(bytes));
            } while (count > 0 || (count < 0 && errno == EINTR));
        }

    private:
        int fds[2];
    };

    /* Owns curses until scope exit. Neither copyable nor movable. */
    class Terminal {
    public:
        Terminal() {
            std::setlocale(LC_ALL, "");
            initscr();
            cbreak();
            noecho();
            keypad(stdscr, TRUE);
            nodelay(stdscr, TRUE);
            curs_set(0);
        }
        ~Terminal() { endwin(); }
        Terminal(const Terminal &) = delete;
        Terminal &operator=(const Terminal &) = delete;
    };

    mc_context_options options(Wakeup &wakeup) {
        mc_context_options value = MC_CONTEXT_OPTIONS_DEFAULT;
        value.services = MC_SERVICE_NONE;
        value.wakeup = Wakeup::Signal;
        value.wakeup_user_data = &wakeup;
        return value;
    }

    /* Owns result handles and cached pages on the dispatcher thread. Neither copyable nor movable. */
    class SearchView {
    public:
        SearchView(std::string_view directory, CacheOptions cache, bool diagnostics)
            : cache(cache), diagnostics(diagnostics), context(options(wakeup)), library(directory) {}
        ~SearchView() {
            window.reset();
            context.Shutdown();
        }
        SearchView(const SearchView &) = delete;
        SearchView &operator=(const SearchView &) = delete;

        void Run() {
            pollfd descriptors[] = { { STDIN_FILENO, POLLIN, 0 }, { wakeup.Descriptor(), POLLIN, 0 } };
            Render();
            while (running) {
                const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    nextSearch - Clock::now()).count();
                const auto timeout = nextSearch == Clock::time_point::max()
                    ? -1 : static_cast<int>(std::max<int64_t>(0, remaining));
                const int ready = poll(descriptors, 2, timeout);
                if (ready < 0 && errno == EINTR) continue;
                if (ready < 0) throw std::runtime_error("Could not wait for terminal events");
                if (descriptors[0].revents & (POLLHUP | POLLERR)) break;

                for (int key; (key = getch()) != ERR && running;) Handle(key);
                if (!running) break;
                if (Clock::now() >= nextSearch) Search();
                if (descriptors[1].revents & POLLIN) {
                    wakeup.Drain();
                    if (context.Dispatch() != MC_OK) throw std::runtime_error("Could not dispatch search");
                }
                Render();
            }
        }

    private:
        void Handle(int key) {
            if (key == 4) {
                running = false;
            } else if (!opened && key == 't') {
                opened = true;
                Search();
            } else if (opened && key == KEY_DOWN && editing) {
                editing = false;
            } else if (opened && (key == KEY_UP || key == '\t')) {
                editing = true;
            } else if (opened && editing) {
                if ((key == KEY_BACKSPACE || key == 127) && !filter.empty()) {
                    filter.pop_back();
                } else if (key >= ' ' && key <= '~') {
                    filter.push_back(static_cast<char>(key));
                } else {
                    return;
                }
                nextSearch = Clock::now() + debounce;
            } else if (opened && TrackCount()) {
                if (key == '\n' || key == KEY_ENTER) {
                    InspectSelection();
                    return;
                }
                if (key == 'j' || key == 'k') {
                    const auto lastRow = std::min(viewport, TrackCount() - offset) - 1;
                    selected = key == 'j' ? std::min(lastRow, selected + 1) : (selected ? selected - 1 : 0);
                    ClearSelection();
                    return;
                }
                const auto previous = offset;
                const auto last = ((TrackCount() - 1) / viewport) * viewport;
                if (key == KEY_END) offset = last;
                else if (key == KEY_HOME) offset = 0;
                else if (key == KEY_NPAGE || key == KEY_DOWN) offset = std::min(last, offset + viewport);
                else if (key == KEY_PPAGE) offset = offset < viewport ? 0 : offset - viewport;
                else return;
                if (offset != previous) {
                    selected = 0;
                    ClearSelection();
                    forward = offset > previous;
                    lastMove = window->CachedRow(offset) ? "cache hit" : "cache miss";
                }
            }
        }

        void Search() {
            nextSearch = Clock::time_point::max();
            ++generation;
            if (searchRequest) context.Cancel(searchRequest);
            window.reset();
            offset = 0;
            selected = 0;
            ClearSelection();
            forward = true;
            lastMove = nullptr;
            error.clear();

            const auto current = generation;
            searchRequest = OpenTrackSearch(context, library, TrackSearch{ filter },
                [this, current](mc_result status, TrackResult tracks, std::string_view message) {
                    if (current != generation) return;
                    searchRequest = 0;
                    if (status != MC_OK) {
                        error = message.empty() ? "Search failed" : std::string(message);
                        return;
                    }
                    window = std::make_unique<TrackResultWindow>(context, std::move(tracks),
                        std::vector<std::string>{ MC_TRACK_FIELD_TITLE },
                        [this](size_t page, size_t count, mc_result status) {
                            if (!count && page == offset && status != MC_OK &&
                                status != MC_ERROR_CANCELLED && status != MC_ERROR_BUSY) {
                                error = "Could not load tracks";
                            }
                        }, TrackResultWindow::Options{ cache.capacity, cache.prefetch });
                });
        }

        size_t Ahead(size_t distance) const {
            return forward ? offset + distance * viewport : offset - distance * viewport;
        }

        void ClearSelection() {
            ++selectionGeneration;
            if (selectionRequest) context.Cancel(selectionRequest);
            selectionRequest = 0;
            selectionStatus.clear();
        }

        void InspectSelection() {
            ClearSelection();
            const auto row = offset + selected;
            const auto identity = window->Result().Track(row);
            const auto id = identity.Id();
            const auto current = selectionGeneration;

            /* Re-resolve the selected identity when an action needs current metadata. */
            selectionRequest = ReadTrack(context, identity,
                [this, current, id](mc_result status, TrackPage page, std::string_view) {
                    if (current != selectionGeneration) return;
                    selectionRequest = 0;
                    if (status == MC_ERROR_NOT_FOUND) selectionStatus = "Selected track is unavailable";
                    else if (status != MC_OK || page.Track(0).Id() != id) selectionStatus = "Could not resolve selected track";
                    else selectionStatus = "Selected track: " + std::string(page.Track(0).Title());
                });
        }

        size_t PrefetchCount() const {
            if (offset >= TrackCount()) return 0;
            const auto available = forward ? (TrackCount() - 1 - offset) / viewport : offset / viewport;
            return std::min(cache.prefetch, available);
        }

        size_t TrackCount() const { return window ? window->Result().Size() : 0; }

        void RequestWindow() {
            if (!window || offset >= TrackCount() || !error.empty()) return;
            using Update = TrackResultWindow::ViewportUpdate;
            const auto update = window->SetViewport(offset, offset + std::min(viewport, TrackCount() - offset) - 1, viewport);
            const auto status = update == Update::Changed || update == Update::Unchanged
                ? window->Load() : MC_ERROR_INVALID_ARGUMENT;
            if (status != MC_OK && status != MC_ERROR_BUSY) error = "Could not load tracks";
        }

        void Render() {
            erase();
            mvaddstr(0, 0, "Track Search Pilot");
            if (opened) {
                mvaddstr(1, 0, "search: ");
                addnstr(filter.c_str(), std::max(0, COLS - 9));
                mvaddstr(3, 0, "tracks (sorted by album)");
                if (!error.empty()) {
                    mvaddnstr(4, 0, error.c_str(), COLS - 1);
                } else if (window && window->CachedRow(offset)) {
                    const auto count = std::min(viewport, TrackCount() - offset);
                    for (size_t row = 0; row < count && row + 5 < static_cast<size_t>(LINES); ++row) {
                        const auto metadata = window->CachedRow(offset + row);
                        const auto title = metadata ? metadata->Title() : std::string_view{};
                        mvprintw(static_cast<int>(row + 5), 0, "%c%c %.*s",
                            !editing && row == selected ? '>' : ' ',
                            window->Result().StartsAlbum(offset + row) ? '*' : ' ',
                            static_cast<int>(title.empty() ? 14 : title.size()),
                            title.empty() ? "[unavailable]" : title.data());
                    }
                } else if (searchRequest || offset < TrackCount()) {
                    mvaddstr(5, 0, "loading tracks");
                } else {
                    mvaddstr(5, 0, "no tracks");
                }
            }
            if (diagnostics && opened) {
                if (PrefetchCount() && window->CachedRow(Ahead(1))) mvaddstr(13, 0, "adjacent page ready");
                if (lastMove) mvaddstr(14, 0, lastMove);
            }
            if (!selectionStatus.empty()) mvaddnstr(15, 0, selectionStatus.c_str(), COLS - 1);
            mvaddstr(std::max(0, LINES - 1), 0, "t search  j/k select  enter inspect  page up/down scroll  ^D quit");
            refresh();
            RequestWindow();
        }

        Wakeup wakeup;
        const CacheOptions cache;
        const bool diagnostics;
        bool running = true;
        bool opened = false;
        bool editing = true;
        bool forward = true;
        const char *lastMove = nullptr;
        size_t offset = 0;
        size_t selected = 0;
        size_t generation = 0;
        size_t selectionGeneration = 0;
        mc_request_id searchRequest = 0;
        mc_request_id selectionRequest = 0;
        Clock::time_point nextSearch = Clock::time_point::max();
        std::string filter;
        std::string error;
        std::string selectionStatus;
        Context context;
        Library library;
        std::unique_ptr<TrackResultWindow> window;
    };
}

int main() {
    try {
        const auto *directory = std::getenv("MUSIKCUBE_SEARCH_DATA");
        if (!directory) throw std::runtime_error("Set MUSIKCUBE_SEARCH_DATA to the fixture database directory");
        const auto cache = cacheOptions();
        Terminal terminal;
        SearchView view(directory, cache, std::getenv("MUSIKCUBE_SEARCH_CACHE_DIAGNOSTICS") != nullptr);
        view.Run();
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
