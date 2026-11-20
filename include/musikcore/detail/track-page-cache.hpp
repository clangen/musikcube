#pragma once

#include <musikcore/library.hpp>

#include <algorithm>
#include <map>

namespace musik { namespace core { namespace api { namespace detail {

    /* Owns projected pages. Neither copyable nor movable. */
    class TrackPageCache {
    public:
        static constexpr size_t DefaultCapacity = 3;

        explicit TrackPageCache(size_t capacity = DefaultCapacity) : capacity(capacity) {}
        TrackPageCache(const TrackPageCache&) = delete;
        TrackPageCache& operator=(const TrackPageCache&) = delete;

        const api::TrackPage* Find(size_t offset) const {
            const auto found = this->pages.find(offset);
            return found == this->pages.end() ? nullptr : &found->second;
        }

        void Clear() { this->pages.clear(); }

        bool Store(size_t offset, api::TrackPage page, size_t first, size_t last) {
            if (!this->capacity) return false;

            /* Keep pages in the requested window ahead of speculative or stale pages. */
            if (this->pages.size() == this->capacity && !this->Find(offset)) {
                const auto distance = [first, last](size_t position) {
                    if (position < first) return first - position;
                    return position > last ? position - last : 0;
                };
                const auto victim = std::max_element(this->pages.begin(), this->pages.end(),
                    [&](const auto& a, const auto& b) { return distance(a.first) < distance(b.first); });
                if (distance(offset) > distance(victim->first)) return false;
                this->pages.erase(victim);
            }

            this->pages.insert_or_assign(offset, std::move(page));
            return true;
        }

    private:
        const size_t capacity;
        std::map<size_t, api::TrackPage> pages;
    };

} } } }
