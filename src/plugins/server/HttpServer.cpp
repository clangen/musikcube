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

#include "HttpServer.h"
#include "musikcore/sdk/Filesystem.h"
#include "Constants.h"
#include "Util.h"
#include "Transcoder.h"
#include <charconv>
#include <limits>
#include <string_view>

#include <musikcore/sdk/String.h>

#pragma warning(push, 0)
#include <websocketpp/base64/base64.hpp>
#pragma warning(pop, 0)

#include <unordered_map>
#include <string>
#include <cstdlib>
#include <filesystem>

#include <fcntl.h>
#include <stdio.h>

#ifdef WIN32
#include <io.h>
#endif

#include <vector>

#define ENABLE_DEBUG 1

static const char* ENVIRONMENT_DISABLE_HTTP_SERVER_AUTH = "MUSIKCUBE_DISABLE_HTTP_SERVER_AUTH";
static const char* TAG = "HttpServer";

using musik::core::sdk::fs::pathToUtf8;

namespace std {
    namespace fs = std::filesystem;
}

using namespace musik::core::sdk;

static const std::unordered_map<std::string, std::string> CONTENT_TYPE_MAP = {
    { ".mp3", "audio/mpeg" },
    { ".ogg", "audio/ogg" },
    { ".opus", "audio/ogg" },
    { ".oga", "audio/ogg" },
    { ".spx", "audio/ogg" },
    { ".flac", "audio/flac" },
    { ".aac", "audio/aac" },
    { ".mp4", "audio/mp4" },
    { ".m4a", "audio/mp4" },
    { ".wav", "audio/wav" },
    { ".mpc", "audio/x-musepack" },
    { ".mp+", "audio/x-musepack" },
    { ".mpp", "audio/x-musepack" },
    { ".ape", "audio/monkeys-audio" },
    { ".wma", "audio/x-ms-wma" },
    { ".jpg", "image/jpeg" }
};

struct ResponseRange {
    std::unique_ptr<server::ResponseBody> body;
    uint64_t start = 0;
    std::optional<uint64_t> length;
};

static std::string contentType(const std::string& extension) {
    const auto it = CONTENT_TYPE_MAP.find("." + str::ToLowerCopy(extension));
    return it == CONTENT_TYPE_MAP.end() ? "application/octet-stream" : it->second;
}

static std::string fileExtension(const std::string& fn) {
    try {
        std::fs::path p(std::fs::u8path(fn));
        std::string ext = str::ToLowerCopy(str::Trim(pathToUtf8(p.extension())));
        if (ext.size()) {
            return ext[0] == '.' ? ext.substr(1) : ext;
        }
    }
    catch (...) {
    }

    return {};
}

static ssize_t fileReadCallback(void* context, uint64_t position, char* dst, size_t capacity) {
    auto& range = *static_cast<ResponseRange*>(context);
    if (range.length) {
        if (position >= *range.length) {
            return MHD_CONTENT_READER_END_OF_STREAM;
        }
        capacity = static_cast<size_t>(std::min<uint64_t>(*range.length - position, capacity));
    }
    if (position > UINT64_MAX - range.start) {
        return MHD_CONTENT_READER_END_WITH_ERROR;
    }
    try {
        size_t count = 0;
        if (range.body->Read(range.start + position, dst, capacity, count) != MC_OK ||
            (range.length && !count && capacity))
        {
            return MHD_CONTENT_READER_END_WITH_ERROR;
        }
        return count ? static_cast<ssize_t>(count) : MHD_CONTENT_READER_END_OF_STREAM;
    }
    catch (const std::bad_alloc&) {
        /* microhttpd will drop the response and release its partial cache. */
        return MHD_CONTENT_READER_END_WITH_ERROR;
    }
}

static void fileFreeCallback(void* context) {
    delete static_cast<ResponseRange*>(context);
}

static bool byteOffset(std::string_view value, uint64_t& offset) {
    if (value.empty()) {
        return false;
    }
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), offset);
    return parsed.ec == std::errc() && parsed.ptr == value.data() + value.size();
}

/* the inclusive endpoints of one HTTP byte range. a live encoder can stream
bytes=0- immediately, but a bounded range or suffix needs a completed file. */
struct RequestedRange {
    std::optional<uint64_t> first;
    std::optional<uint64_t> last;

    bool IsPartialRequest() const { return !first || *first != 0 || last.has_value(); }

    bool Apply(uint64_t size, ResponseRange& result) const {
        if (!size || (first && *first >= size)) {
            return false;
        }
        if (!first) {
            result.length = std::min(*last, size);
            result.start = size - *result.length;
        }
        else {
            result.start = *first;
            result.length = std::min(last.value_or(size - 1), size - 1) - *first + 1;
        }
        return true;
    }
};

static std::optional<RequestedRange> parseRange(std::string_view header) {
    if (header.substr(0, 6) != "bytes=") {
        return {};
    }
    header.remove_prefix(6);
    const auto dash = header.find('-');
    if (dash == std::string_view::npos) {
        return {};
    }
    const auto left = header.substr(0, dash);
    const auto right = header.substr(dash + 1);
    RequestedRange range;
    uint64_t offset;
    if (!left.empty()) {
        if (!byteOffset(left, offset)) {
            return {};
        }
        range.first = offset;
    }
    if (!right.empty()) {
        if (!byteOffset(right, offset)) {
            return {};
        }
        range.last = offset;
    }
    if ((!range.first && !range.last.value_or(0)) || (range.first && range.last && *range.last < *range.first)) {
        return {};
    }
    return range;
}

static MHD_Response* makeFileResponse(
    std::unique_ptr<server::ResponseBody> body, const std::optional<RequestedRange>& requestedRange, int& status)
{
    const auto total = body->Length();
    const bool seekable = body->Seekable();
    auto range = std::make_unique<ResponseRange>();
    range->body = std::move(body);
    range->length = total;
    const bool partial = seekable && total && requestedRange && requestedRange->Apply(*total, *range);

    std::string contentRange;
    if (partial) {
        contentRange = "bytes " + std::to_string(range->start) + "-" +
            std::to_string(range->start + (*range->length - 1)) + "/" + std::to_string(*total);
    }

    /* an on-demand encoder learns the actual size only after flushing. a false
    Content-Length causes the HTTP client to discard the end of the track. */
    auto* response = MHD_create_response_from_callback(range->length.value_or(MHD_SIZE_UNKNOWN), 4096,
        fileReadCallback, range.get(), fileFreeCallback);
    if (!response) {
        return nullptr;
    }
    range.release();
    status = partial ? MHD_HTTP_PARTIAL_CONTENT : MHD_HTTP_OK;
    if (partial) {
        MHD_add_response_header(response, "Content-Range", contentRange.c_str());
    }
    if (seekable && total) {
        MHD_add_response_header(response, "Accept-Ranges", "bytes");
    }
    return response;
}

static size_t getUnsignedUrlParam(
    struct MHD_Connection *connection,
    const std::string& argument,
    size_t defaultValue)
{
    const char* stringValue =
        MHD_lookup_connection_value(
            connection,
            MHD_GET_ARGUMENT_KIND,
            argument.c_str());

    if (stringValue != 0) {
        try {
            return std::stoul(urlDecode(stringValue));
        }
        catch (...) {
            /* invalid bitrate */
        }
    }

    return defaultValue;
}

static std::string getStringUrlParam(
    struct MHD_Connection *connection,
    const std::string& argument,
    std::string defaultValue)
{
    const char* stringValue =
        MHD_lookup_connection_value(
            connection,
            MHD_GET_ARGUMENT_KIND,
            argument.c_str());

    return stringValue ? std::string(stringValue) : defaultValue;
}

static bool isAuthenticated(MHD_Connection *connection, Context& context) {
    const char* disableAuth = std::getenv(ENVIRONMENT_DISABLE_HTTP_SERVER_AUTH);
    if (disableAuth && std::string(disableAuth) == "1") {
        return true;
    }

    const char* authPtr = MHD_lookup_connection_value(
        connection, MHD_HEADER_KIND, "Authorization");

    if (authPtr && strlen(authPtr)) {
        std::string auth(authPtr);
        if (auth.find("Basic ") == 0) {
            std::string encoded = auth.substr(6);
            if (encoded.size()) {
                std::string decoded = websocketpp::base64_decode(encoded);
                std::vector<std::string> userPass = str::Split(decoded, ":");
                if (userPass.size() == 2) {
                    std::string password = context.settings.Text(serverprefs::Password, defaults::password);
                    return userPass[0] == "default" && userPass[1] == password;
                }
            }
        }
    }

    return false;
}

HttpServer::HttpServer(Context& context)
: context(context)
, running(false) {
    this->httpServer = nullptr;
}

HttpServer::~HttpServer() {
    this->Stop();
}

void HttpServer::Wait() {
    std::unique_lock<std::mutex> lock(this->exitMutex);
    while (this->running) {
        this->exitCondition.wait(lock);
    }
}

bool HttpServer::Start() {
    if (this->Stop()) {
        Transcoder::RemoveTempTranscodeFiles(this->context);

        MHD_FLAG ipVersion = MHD_NO_FLAG;
        if (context.settings.Boolean(serverprefs::UseIpv6.c_str(), defaults::use_ipv6)) {
            ipVersion = MHD_USE_IPv6;
        }

        int serverFlags =
#if MHD_VERSION >= 0x00095300
            MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD | MHD_USE_THREAD_PER_CONNECTION | ipVersion;
#else
            MHD_USE_SELECT_INTERNALLY | MHD_USE_THREAD_PER_CONNECTION | ipVersion;
#endif

        int serverPort =
            context.settings.Integer(serverprefs::AudioPort.c_str(), serverprefs::DefaultAudioPort);

        httpServer = MHD_start_daemon(
            serverFlags,
            serverPort,
            nullptr,                                    /* accept() policy callback */
            nullptr,                                    /* accept() policy callback data */
            &HttpServer::HandleRequest,                 /* request handler callback */
            this,                                       /* request handler callback data */
            MHD_OPTION_UNESCAPE_CALLBACK,               /* option to configure unescaping */
            &HttpServer::HandleUnescape,                /* callback to be called for unescaping data */
            this,                                       /* unescape data callback data */
            MHD_OPTION_LISTENING_ADDRESS_REUSE,         /* option to configure address reuse */
            1,                                          /* enable address reuse */
            MHD_OPTION_END);                            /* terminal option */

        this->running = (httpServer != nullptr);
        return running;
    }

    return false;
}

bool HttpServer::Stop() {
    if (httpServer) {
        MHD_stop_daemon(this->httpServer);
        this->httpServer = nullptr;
    }

    this->running = false;
    this->exitCondition.notify_all();

    return true;
}

size_t HttpServer::HandleUnescape(void * cls, struct MHD_Connection *c, char *s) {
    /* don't do anything. the default implementation will decode the
    entire path, which breaks if we have individually decoded segments. */
    return strlen(s);
}

MHD_Result HttpServer::HandleRequest(
    void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls)
{
    auto server = static_cast<HttpServer*>(cls);

#ifdef ENABLE_DEBUG
    server->context.Log(MC_LOG_INFO, TAG, str::Format("new request: %s", url).c_str());
#endif

    struct MHD_Response* response = nullptr;
    int ret = MHD_NO;
    int status = MHD_HTTP_NOT_FOUND;

    try {
        if (server->context.stopping.load()) {
            status = MHD_HTTP_SERVICE_UNAVAILABLE;
            response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        }
        else if (method && std::string(method) == "GET") {
            if (!isAuthenticated(connection, server->context)) {
                status = 401; /* unauthorized */
                static const char* error = "unauthorized";
                response = MHD_create_response_from_buffer(strlen(error), (void*)error, MHD_RESPMEM_PERSISTENT);
#ifdef ENABLE_DEBUG
                server->context.Log(MC_LOG_WARNING, TAG, "unauthorized request, returning a 401");
#endif
            }
            else {
                /* if we get here we're authenticated */
                std::string urlStr(url);

                if (urlStr[0] == '/') {
                    urlStr = urlStr.substr(1);
                }

                std::vector<std::string> parts = str::Split(urlStr, "/");
                if (parts.size() > 0) {
                    /* /audio/id/<id> OR /audio/external_id/<external_id> */
                    if (parts.at(0) == fragment::audio && parts.size() == 3) {
                        status = HandleAudioTrackRequest(server, response, connection, parts);
                    }
                    /* /thumbnail/<id> */
                    else if (parts.at(0) == fragment::thumbnail && parts.size() == 2) {
                        status = HandleThumbnailRequest(server, response, connection, parts);
                    }
                }
            }
        }
    }
    catch (...) {
    }

    if (response) {
#ifdef ENABLE_DEBUG
        server->context.Log(MC_LOG_INFO, TAG, str::Format("return http %d", status).c_str());
#endif

        ret = MHD_queue_response(connection, status, response);
        MHD_destroy_response(response);
    }

    return (MHD_Result) ret;
}

int HttpServer::HandleAudioTrackRequest(
    HttpServer* server,
    MHD_Response*& response,
    MHD_Connection *connection,
    std::vector<std::string>& pathParts)
{
    size_t bitrate = getUnsignedUrlParam(connection, "bitrate", 0);
    int maxActiveTranscoders = server->context.settings.Integer(
        serverprefs::ActiveTranscodeLimit.c_str(),
        serverprefs::DefaultActiveTranscodeLimit);

    if (bitrate != 0 && Transcoder::GetActiveCount() >= maxActiveTranscoders) {
        response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
        return MHD_HTTP_TOO_MANY_REQUESTS;
    }

    int status = MHD_HTTP_OK;

    bool byExternalId = (pathParts.at(1) == fragment::external_id);
    mc_track_result* rawTracks = nullptr;
    const auto& library = *server->context.library;
    if (byExternalId) {
        std::string externalId = urlDecode(pathParts.at(2));
        const mc_string_view id { externalId.data(), externalId.size() };
        library.tracks_by_external_ids(library.context, &id, 1, &rawTracks);
    }
    else if (pathParts.at(1) == fragment::id) {
        const int64_t id = std::stoll(urlDecode(pathParts.at(2)));
        library.tracks_by_ids(library.context, &id, 1, &rawTracks);
    }

    auto destroyTracks = [](mc_track_result* tracks) { mc_track_result_destroy(&tracks); };
    std::unique_ptr<mc_track_result, decltype(destroyTracks)> tracks(rawTracks, destroyTracks);
    enum TrackField : size_t { Duration, Filename, Title, ExternalId };
    const mc_string_view fields[] {
        { .data = MC_TRACK_FIELD_DURATION, .length = sizeof(MC_TRACK_FIELD_DURATION) - 1 },
        { .data = MC_TRACK_FIELD_FILENAME, .length = sizeof(MC_TRACK_FIELD_FILENAME) - 1 },
        { .data = MC_TRACK_FIELD_TITLE, .length = sizeof(MC_TRACK_FIELD_TITLE) - 1 },
        { .data = MC_TRACK_FIELD_EXTERNAL_ID, .length = sizeof(MC_TRACK_FIELD_EXTERNAL_ID) - 1 },
    };
    auto range = mc_track_range MC_TRACK_RANGE_DEFAULT;
    range.limit = 1;
    range.fields = fields;
    range.field_count = sizeof(fields) / sizeof(fields[0]);
    mc_track_page* rawPage = nullptr;
    if (tracks && mc_track_result_get_count(tracks.get()) &&
        library.read_tracks(library.context, tracks.get(), &range, &rawPage) == MC_OK)
    {
        auto destroyPage = [](mc_track_page* page) { mc_track_page_destroy(&page); };
        std::unique_ptr<mc_track_page, decltype(destroyPage)> page(rawPage, destroyPage);
        if (!page || !mc_track_page_get_count(page.get())) {
            response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            return MHD_HTTP_NOT_FOUND;
        }
        const auto value = [&](size_t field) {
            const auto text = mc_track_page_get_string(page.get(), 0, field);
            return std::string(text.data ? text.data : "", text.length);
        };
        const std::string duration = value(Duration);
        const std::string filename = value(Filename);
        const std::string title = value(Title);
        const std::string externalId = value(ExternalId);

        std::string format;

        if (bitrate != 0) {
            format = getStringUrlParam(connection, "format", "mp3");
        }

        const char* header = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "Range");
        const auto range = parseRange(header ? header : "");
        auto request = range && range->IsPartialRequest()
            ? Transcoder::Request::PartialTrack
            : Transcoder::Request::WholeTrack;
        /* musikdroid's shipped HTTP cache uses Android's default Dalvik agent
        and external track IDs. it retries EOF forever without a byte length
        when the server cache is disabled. send the finalized MP3 on its first GET. */
        const bool mp3 = str::ToLowerCopy(format) == "mp3";
        const bool externalMp3 = byExternalId && mp3;
        if (externalMp3) {
            const char* agent = MHD_lookup_connection_value(connection, MHD_HEADER_KIND, "User-Agent");
            if (agent && std::string_view(agent).starts_with("Dalvik/")) {
                request = Transcoder::Request::CompleteFile;
            }
        }
        auto body = bitrate
            ? Transcoder::Transcode(server->context, filename, bitrate, format, request)
            : ::server::OpenFileBody(*server->context.media, filename);
        if (!body) {
            response = MHD_create_response_from_buffer(0, nullptr, MHD_RESPMEM_PERSISTENT);
            return MHD_HTTP_NOT_FOUND;
        }
        const bool seekable = body->Seekable();
        response = makeFileResponse(std::move(body), range, status);
        if (response) {
            const std::string extension = format.empty() ? fileExtension(filename) : format;
            /* preserve the download filename fallback for extensionless tracks.
            their actual MIME type is unknown. */
            MHD_add_response_header(response, "X-musikcube-File-Extension", extension.empty() ? "mp3" : extension.c_str());
            if (seekable && std::fs::exists(std::fs::u8path(filename))) {
                MHD_add_response_header(response, "X-musikcube-Filename-Override", externalId.c_str());
            }
            /* legacy clients use this marker to refresh the gapless header
            after downloading live MP3 bytes. Content-Length remains unknown. */
            if (!seekable && mp3) {
                MHD_add_response_header(response, "X-musikcube-Estimated-Content-Length", "true");
            }
            if (externalMp3) {
                MHD_add_response_header(response, "Vary", "User-Agent");
            }
            if (!duration.empty()) {
                MHD_add_response_header(response, "X-Content-Duration", duration.c_str());
                MHD_add_response_header(response, "Content-Duration", duration.c_str());
            }
            if (byExternalId) {
                MHD_add_response_header(response, "Cache-Control", seekable ? "public, max-age=31536000" : "no-cache");
            }
            MHD_add_response_header(response, "Content-Type", contentType(extension).c_str());
            MHD_add_response_header(response, "Server", "musikcube server");
        }
    }
    else {
        status = MHD_HTTP_NOT_FOUND;
    }

    return status;
}

int HttpServer::HandleThumbnailRequest(
    HttpServer* server,
    MHD_Response*& response,
    MHD_Connection* connection,
    std::vector<std::string>& pathParts)
{
    int status = MHD_HTTP_NOT_FOUND;

    size_t consumed = 0;
    const auto id = std::stoll(pathParts.at(1), &consumed);
    if (consumed != pathParts.at(1).size() || id <= 0) {
        return status;
    }
    const auto& library = *server->context.library;
    const auto read = [](mc_result result, mc_byte_view bytes, const mc_error*, void* data) {
        auto& output = *static_cast<MHD_Response**>(data);
        if (result == MC_OK) {
            output = MHD_create_response_from_buffer(bytes.length, const_cast<uint8_t*>(bytes.data), MHD_RESPMEM_MUST_COPY);
        }
    };
    if (library.read_thumbnail(library.context, id, read, &response) == MC_OK && response) {
        status = MHD_HTTP_OK;
        MHD_add_response_header(response, "Cache-Control", "public, max-age=31536000");
        MHD_add_response_header(response, "Content-Type", "image/jpeg");
        MHD_add_response_header(response, "Server", "musikcube server");
    }

    return status;
}
