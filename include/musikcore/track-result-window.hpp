#pragma once

#include <musikcore/detail/track-page-cache.hpp>

namespace musik { namespace core { namespace client {

    /*
     * Load metadata for the visible rows of a stable, ordered track result.
     * Search, album browse, and playlist views can share range loading,
     * prefetch, eviction, and stale-reply handling across local and remote
     * libraries. Callers select the metadata fields and decide when to redraw.
     * For a changing list such as the play queue, use a new window for each
     * ordered snapshot that the view displays.
     *
     * Neither copyable nor movable. Use on the dispatcher thread with a
     * longer-lived context.
     */
    class TrackResultWindow {
    public:
        /* Copyable and movable. Limits are measured in pages. */
        struct Options {
            size_t capacity = detail::TrackPageCache::DefaultCapacity;
            size_t prefetch = 1;
        };

        /* Called on dispatch with stored rows, if any, and the current load status. */
        /* Zero count means no rows changed. Status can include a failure to submit the next page. */
        using Changed = std::function<void(size_t offset, size_t count, mc_result status)>;

        enum class ViewportUpdate { Changed, Unchanged, InvalidRange, ExceedsCapacity };

        TrackResultWindow(Context& context, TrackResult result, std::vector<std::string> fields, Changed changed)
            : TrackResultWindow(context, std::move(result), std::move(fields), std::move(changed), Options{}) {}

        TrackResultWindow(Context& context, TrackResult result, std::vector<std::string> fields,
            Changed changed, Options options)
            : context(context), result(std::move(result)), fields(std::move(fields)),
              changed(std::move(changed)), options(options), pages(options.capacity) {
            if (!options.capacity) throw Error(MC_ERROR_INVALID_ARGUMENT);
        }

        ~TrackResultWindow() { this->Clear(); }
        TrackResultWindow(const TrackResultWindow&) = delete;
        TrackResultWindow& operator=(const TrackResultWindow&) = delete;

        const TrackResult& Result() const noexcept { return this->result; }
        bool Pending() const noexcept { return this->request != 0; }

        bool IntersectsViewport(size_t offset, size_t count) const noexcept {
            return this->viewportKnown && count && offset <= this->last &&
                (offset >= this->first || count > this->first - offset);
        }

        /* Borrows metadata until the page is evicted or the window is cleared or destroyed. */
        std::optional<TrackView> CachedRow(size_t row) const noexcept {
            const auto offset = this->PageOffset(row);
            const auto* page = this->pages.Find(offset);
            if (!page || row - offset >= page->Size()) return std::nullopt;
            return page->Track(row - offset);
        }

        /* The inclusive range must fit the cache. Rejected updates leave the viewport intact. */
        ViewportUpdate SetViewport(size_t first, size_t last, size_t size) {
            if (!size || first > last || first >= this->result.Size()) return ViewportUpdate::InvalidRange;
            last = std::min(last, this->result.Size() - 1);
            if (last / size - first / size >= this->options.capacity) return ViewportUpdate::ExceedsCapacity;
            if (this->viewportKnown && size == this->pageSize && first == this->first && last == this->last) {
                return ViewportUpdate::Unchanged;
            }

            if (size != this->pageSize) this->Clear();
            if (first != this->first) this->forward = first > this->first;
            this->pageSize = size;
            this->first = first;
            this->last = last;
            this->viewportKnown = true;
            this->failed.reset();
            return ViewportUpdate::Changed;
        }

        /* Clear cached metadata and cancel work while retaining the viewport for a reload. */
        void Clear() {
            ++*this->generation;
            if (this->request) this->context.Cancel(std::exchange(this->request, 0));
            this->pages.Clear();
            this->failed.reset();
        }

        /* Start or reprioritize loading. Synchronous submission failures return an error. */
        mc_result Load() {
            if (!this->viewportKnown) return MC_OK;
            const auto firstPage = this->PageOffset(this->first);
            const auto lastPage = this->PageOffset(this->last);
            auto next = firstPage;
            while (next < lastPage && this->pages.Find(next)) next += this->pageSize;
            const bool visibleMissing = !this->pages.Find(next);

            /* A missing visible page takes precedence over work outside the viewport. */
            if (this->request) {
                if (visibleMissing && (this->pending < firstPage || this->pending > lastPage)) {
                    this->context.Cancel(this->request);
                }
                return MC_OK;
            }

            if (!visibleMissing) {
                const auto ahead = this->PrefetchCount(firstPage, lastPage);
                auto candidate = this->forward ? lastPage : firstPage;
                for (size_t i = 0; i < ahead; ++i) {
                    candidate = this->forward ? candidate + this->pageSize : candidate - this->pageSize;
                    if (!this->pages.Find(candidate)) {
                        next = candidate;
                        break;
                    }
                }
                if (this->pages.Find(next)) return MC_OK;
            }
            if (this->failed == next) return MC_OK;

            try {
                TrackRange range{ next, this->pageSize, {} };
                for (const auto& field : this->fields) range.fields.emplace_back(field);
                const auto version = *this->generation;
                const std::weak_ptr<const size_t> lifetime = this->generation;
                this->request = ReadTracks(this->context, this->result, range,
                    [this, lifetime, version, next](mc_result status, TrackPage page, std::string_view) {
                        /* Clear or destruction can precede a completion already queued for dispatch. */
                        const auto current = lifetime.lock();
                        if (!current || *current != version) return;
                        this->Complete(next, status, std::move(page));
                    });
                this->pending = next;
                return MC_OK;
            } catch (const Error& error) {
                /* A full request queue can accept this page on a later Load. */
                if (error.Result() != MC_ERROR_BUSY) this->failed = next;
                return error.Result();
            }
        }

    private:
        size_t PageOffset(size_t row) const noexcept { return row / this->pageSize * this->pageSize; }

        size_t PrefetchCount(size_t firstPage, size_t lastPage) const noexcept {
            const auto visible = (lastPage - firstPage) / this->pageSize + 1;
            const auto available = this->forward
                ? (this->result.Size() - 1 - lastPage) / this->pageSize : firstPage / this->pageSize;
            return std::min({ this->options.prefetch, this->options.capacity - visible, available });
        }

        void Complete(size_t offset, mc_result status, TrackPage page) {
            this->request = 0;
            size_t count = 0;
            if (status == MC_OK) {
                count = page.Size();
                auto firstPage = this->PageOffset(this->first);
                auto lastPage = this->PageOffset(this->last);
                const auto prefetchSpan = this->PrefetchCount(firstPage, lastPage) * this->pageSize;
                if (this->forward) lastPage += prefetchSpan;
                else firstPage -= prefetchSpan;
                if (!this->pages.Store(offset, std::move(page), firstPage, lastPage)) count = 0;
            } else if (status != MC_ERROR_CANCELLED && status != MC_ERROR_BUSY) {
                this->failed = offset;
            }

            const auto nextStatus = status == MC_ERROR_BUSY ? status : this->Load();
            const auto notify = this->changed;
            if (notify) notify(offset, count, nextStatus == MC_OK ? status : nextStatus);
        }

        Context& context;
        TrackResult result;
        const std::vector<std::string> fields;
        const Changed changed;
        const Options options;
        detail::TrackPageCache pages;
        std::shared_ptr<size_t> generation = std::make_shared<size_t>(0);
        size_t pageSize = 1;
        size_t first = 0;
        size_t last = 0;
        size_t pending = 0;
        bool forward = true;
        bool viewportKnown = false;
        mc_request_id request = 0;
        std::optional<size_t> failed;
    };

} } }
