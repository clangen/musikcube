#ifndef MUSIKCORE_ONLINE_HPP
#define MUSIKCORE_ONLINE_HPP

#include <musikcore/detail/async.hpp>
#include <musikcore/online.h>

#include <functional>

namespace musik { namespace core { namespace api {
    using OnlineTextCompletion = std::function<void(mc_result status, std::string_view text)>;
    using LastFmLinkCompletion = std::function<void(mc_result status, std::string_view token, std::string_view url)>;

    namespace detail {
        inline void MC_CALL onlineTextCompleted(mc_result status, mc_string_view text, void* data) {
            const std::unique_ptr<OnlineTextCompletion> done(static_cast<OnlineTextCompletion*>(data));
            (*done)(status, AsStringView(text));
        }

        inline void MC_CALL lastFmLinkCompleted(mc_result status, mc_string_view token, mc_string_view url, void* data) {
            const std::unique_ptr<LastFmLinkCompletion> done(static_cast<LastFmLinkCompletion*>(data));
            (*done)(status, AsStringView(token), AsStringView(url));
        }
    }

    /* Accepted work is dispatched through the context. Completion views are
       valid during the callback only. Rejected submissions throw Error. */
    inline mc_request_id FindLyrics(Context& context, std::string_view artist, std::string_view title,
        OnlineTextCompletion completion)
    {
        return detail::submitRequest(std::move(completion), [&](void* data, mc_request_id* request) {
            return mc_lyrics_find_async(context.Get(), { artist.data(), artist.size() }, { title.data(), title.size() },
                detail::onlineTextCompleted, data, request);
        });
    }

    inline mc_request_id RequestLastFmLink(Context& context, LastFmLinkCompletion completion) {
        return detail::submitRequest(std::move(completion), [&](void* data, mc_request_id* request) {
            return mc_lastfm_request_link_async(context.Get(), detail::lastFmLinkCompleted, data, request);
        });
    }

    inline mc_request_id CompleteLastFmLink(Context& context, std::string_view token, OnlineTextCompletion completion) {
        return detail::submitRequest(std::move(completion), [&](void* data, mc_request_id* request) {
            return mc_lastfm_complete_link_async(context.Get(), { token.data(), token.size() },
                detail::onlineTextCompleted, data, request);
        });
    }

    inline std::optional<std::string> ReadLastFmAccount() {
        std::optional<std::string> account;
        mc_lastfm_read_account([](mc_result, mc_string_view text, void* data) {
            static_cast<std::optional<std::string>*>(data)->emplace(AsStringView(text));
        }, &account);
        return account;
    }

    inline mc_result UnlinkLastFmAccount() {
        return mc_lastfm_unlink();
    }

} } }

#endif
