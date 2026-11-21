#pragma once

#include <app/util/AsyncCallbackScope.h>
#include <musikcore/context.hpp>

#include <optional>

namespace musik { namespace cube {
    /* Run one replaceable read on the dispatcher thread. Use for views where
       only the latest selection matters, such as opening an album in Browse
       or refreshing a track search after the filter changes. Store the catalog
       in Query when a retry must use the same origin as the first submission.
       Cancellation cannot retract a reply already queued on the dispatcher,
       so the scope also discards completions for superseded queries.
       Use AsyncCallbackScope directly when guarding callbacks in a workflow
       with its own sequencing, such as reading a tag before opening a dialog.

       Query::Open(context, completion) submits the read. For example, a view
       can own a LatestRequest<AlbumQuery, TrackResult>:

           LatestRequest<AlbumQuery, TrackResult> request(
               context,
               [&] { ScheduleRetry(); },
               [&](const AlbumQuery&, mc_result status, TrackResult tracks, std::string_view) {
                   if (status == MC_OK) {
                       ShowTracks(std::move(tracks));
                   }
               });

       Call Replace(query) on selection changes, TrySubmit() when the scheduled
       retry fires, and Clear() when the view is reset. Open must deliver
       accepted requests asynchronously. Busy and queue-full results retain
       the query for retry. Clear and destruction cancel the active request
       and invalidate its completion. The view owns retry timers and rendering.
       Noncopyable and nonmovable. */
    template<typename Query, typename Result, typename Context = musik::core::api::Context>
    class LatestRequest {
        public:
            using Completion = std::function<void(mc_result, Result, std::string_view)>;
            using Retry = std::function<void()>;
            using Deliver = std::function<void(const Query&, mc_result, Result, std::string_view)>;

            LatestRequest(Context& context, Retry retry, Deliver deliver)
                : context(context), retry(std::move(retry)), deliver(std::move(deliver)) {}

            ~LatestRequest() { this->Clear(); }

            const Query* Pending() const noexcept {
                return this->query ? &*this->query : nullptr;
            }

            void Replace(Query query) {
                this->Clear();
                this->query.emplace(std::move(query));
                this->TrySubmit();
            }

            void Clear() {
                /* Invalidate before canceling an already queued completion. */
                this->scope.Invalidate();
                if (this->request) {
                    this->context.Cancel(std::exchange(this->request, 0));
                }
                this->query.reset();
            }

            void TrySubmit() {
                if (!this->query || this->request) {
                    return;
                }

                try {
                    this->request = this->query->Open(this->context,
                        this->scope.Begin([this](mc_result status, Result result, std::string_view error) {
                            this->Finish(status, std::move(result), error);
                        }));
                }
                catch (const musik::core::api::Error& error) {
                    this->Finish(error.Result(), Result {}, error.what());
                }
            }

        private:
            void Finish(mc_result status, Result result, std::string_view error) {
                this->scope.Invalidate();
                this->request = 0;
                if (status == MC_ERROR_BUSY || status == MC_ERROR_QUEUE_FULL) {
                    this->retry();
                    return;
                }

                auto query = std::exchange(this->query, std::nullopt);
                this->deliver(*query, status, std::move(result), error);
            }

            Context& context;
            Retry retry;
            Deliver deliver;
            AsyncCallbackScope scope;
            std::optional<Query> query;
            mc_request_id request {};
    };
} }
