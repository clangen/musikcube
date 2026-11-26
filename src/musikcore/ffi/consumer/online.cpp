#include <musikcore/online.h>

#include "request.h"
#include "../common/error.h"
#include "../common/primitives.h"

#include <musikcore/support/Auddio.h>
#include <musikcore/support/LastFm.h>

#include <utility>

namespace {
    using namespace musik::core;
    using namespace musik::core::ffi;

    mc_string_view view(const std::string& text) {
        return { text.data(), text.size() };
    }

    /* Network workers own their reply storage until finishing. Cancellation
       can deliver earlier, so a failed completion never reads that storage. */
    template<class Reply, class Start, class Complete>
    mc_result submitOnline(mc_context& context, Start start, Complete complete, void* userData, mc_request_id& request) {
        const auto output = std::make_shared<Reply>();
        const auto cancelled = std::make_shared<std::stop_source>();

        /* The context joins this transfer before destroying the runtime. */
        return submit_request(&context, [output, cancelled, start = std::move(start)] {
            mc_result result = MC_ERROR_INTERNAL;
            start(cancelled->get_token(), [&](mc_result status, Reply reply) {
                if (status == MC_OK) {
                    *output = std::move(reply);
                }
                result = status;
            });
            return result;
        }, [output, complete](mc_result status, void* data) {
            complete(status, status == MC_OK ? output.get() : nullptr, data);
        }, userData, &request, {}, RequestEffect::Read, nullptr, [cancelled] { cancelled->request_stop(); });
    }

    struct LastFmLink {
        std::string token;
        std::string url;
    };
}

mc_result MC_CALL mc_lyrics_find_async(mc_context* context, mc_string_view artist, mc_string_view title,
    mc_online_text_fn complete, void* userData, mc_request_id* request)
{
    if (request) {
        *request = 0;
    }
    if (!context || !complete || !request || !valid_text(artist) || !valid_text(title, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&] {
        return submitOnline<std::string>(*context, [artist = std::string(artist.data ? artist.data : "", artist.length),
            title = std::string(title.data, title.length)](std::stop_token cancelled, auto finish) {
            auddio::FindLyrics(artist, title, cancelled, [finish](std::string text) {
                const auto status = text.empty() ? MC_ERROR_NOT_FOUND : MC_OK;
                finish(status, std::move(text));
            });
        }, [complete](mc_result status, const std::string* text, void* data) {
            complete(status, text ? view(*text) : mc_string_view {}, data);
        }, userData, *request);
    });
}

mc_result MC_CALL mc_lastfm_request_link_async(mc_context* context, mc_lastfm_link_fn complete,
    void* userData, mc_request_id* request)
{
    if (request) {
        *request = 0;
    }
    if (!context || !complete || !request) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&] {
        return submitOnline<LastFmLink>(*context, [](std::stop_token cancelled, auto finish) {
            lastfm::CreateAccountLinkToken(cancelled, [finish](std::string token) {
                if (token.empty()) {
                    finish(MC_ERROR_IO, {});
                    return;
                }

                try {
                    auto url = lastfm::CreateAccountLinkUrl(token);
                    finish(MC_OK, { std::move(token), std::move(url) });
                }
                catch (...) {
                    finish(MC_ERROR_INTERNAL, {});
                }
            });
        }, [complete](mc_result status, const LastFmLink* link, void* data) {
            complete(status, link ? view(link->token) : mc_string_view {},
                link ? view(link->url) : mc_string_view {}, data);
        }, userData, *request);
    });
}

mc_result MC_CALL mc_lastfm_complete_link_async(mc_context* context, mc_string_view token,
    mc_online_text_fn complete, void* userData, mc_request_id* request)
{
    if (request) {
        *request = 0;
    }
    if (!context || !complete || !request || !valid_text(token, false)) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&] {
        return submitOnline<lastfm::Session>(*context, [token = std::string(token.data, token.length)](std::stop_token cancelled, auto finish) {
            lastfm::CreateSession(token, cancelled, [finish](lastfm::Session session) {
                const auto status = session.valid ? MC_OK : MC_ERROR_IO;
                finish(status, std::move(session));
            });
        }, [complete](mc_result status, const lastfm::Session* session, void* data) {
            if (session) {
                status = exception_barrier(nullptr, [&] {
                    return lastfm::SaveSession(*session) ? MC_OK : MC_ERROR_IO;
                });
            }
            complete(status, status == MC_OK ? view(session->username) : mc_string_view {}, data);
        }, userData, *request);
    });
}

mc_result MC_CALL mc_lastfm_read_account(mc_online_text_fn read, void* userData) {
    if (!read) {
        return MC_ERROR_INVALID_ARGUMENT;
    }

    return exception_barrier(nullptr, [&] {
        const auto session = lastfm::LoadSession();
        if (!session.valid) {
            return MC_ERROR_NOT_FOUND;
        }
        read(MC_OK, view(session.username), userData);
        return MC_OK;
    });
}

mc_result MC_CALL mc_lastfm_unlink(void) {
    return exception_barrier(nullptr, [] {
        return lastfm::ClearSession() ? MC_OK : MC_ERROR_IO;
    });
}
