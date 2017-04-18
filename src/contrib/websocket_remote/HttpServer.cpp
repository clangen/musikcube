//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
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
#include "Constants.h"
#include "Util.h"

#include <core/sdk/IRetainedTrack.h>

#include <boost/filesystem.hpp>
#include <boost/algorithm/string.hpp>

#include <unordered_map>
#include <string>

#include <fcntl.h>
#include <stdio.h>

#ifdef WIN32
#include <io.h>
#endif

#include <vector>

using namespace musik::core::sdk;

std::unordered_map<std::string, std::string> CONTENT_TYPE_MAP = {
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
    { ".wma", "audio/x-ms-wma" }
};

struct Range {
    size_t from;
    size_t to;
    size_t total;
    FILE* file;

    std::string HeaderValue() {
        return "bytes " + std::to_string(from) + "-" + std::to_string(to) + "/" + std::to_string(total);
    }
};

static std::string contentType(const std::string& fn) {
    try {
        boost::filesystem::path p(fn);
        std::string ext = boost::trim_copy(p.extension().string());

        auto it = CONTENT_TYPE_MAP.find(ext);
        if (it != CONTENT_TYPE_MAP.end()) {
            return it->second;
        }
    }
    catch (...) {
    }

    return "application/octet-stream";
}

/* toHex, urlEncode, fromHex, urlDecode are stolen from here:
http://dlib.net/dlib/server/server_http.cpp.html */
static inline unsigned char toHex(unsigned char x) {
    return x + (x > 9 ? ('A' - 10) : '0');
}

std::string urlEncode(const std::string& s) {
    std::ostringstream os;

    for (std::string::const_iterator ci = s.begin(); ci != s.end(); ++ci) {
        if ((*ci >= 'a' && *ci <= 'z') ||
            (*ci >= 'A' && *ci <= 'Z') ||
            (*ci >= '0' && *ci <= '9'))
        { // allowed
            os << *ci;
        }
        else if (*ci == ' ') {
            os << '+';
        }
        else {
            os << '%' << toHex(*ci >> 4) << toHex(*ci % 16);
        }
    }

    return os.str();
}

inline unsigned char fromHex(unsigned char ch) {
    if (ch <= '9' && ch >= '0') {
        ch -= '0';
    }
    else if (ch <= 'f' && ch >= 'a') {
        ch -= 'a' - 10;
    }
    else if (ch <= 'F' && ch >= 'A') {
        ch -= 'A' - 10;
    }
    else {
        ch = 0;
    }

    return ch;
}

std::string urlDecode(const std::string& str) {
    using namespace std;
    string result;
    string::size_type i;

    for (i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            result += ' ';
        }
        else if (str[i] == '%' && str.size() > i + 2) {
            const unsigned char ch1 = fromHex(str[i + 1]);
            const unsigned char ch2 = fromHex(str[i + 2]);
            const unsigned char ch = (ch1 << 4) | ch2;
            result += ch;
            i += 2;
        }
        else {
            result += str[i];
        }
    }

    return result;
}

static ssize_t fileReadCallback(void *cls, uint64_t pos, char *buf, size_t max) {
    Range* range = static_cast<Range*>(cls);

    size_t offset = (size_t) pos + range->from;
    offset = std::min(range->to, offset);

    size_t avail = range->total - offset;
    size_t count = std::min(avail, max);

    if (fseek(range->file, offset, SEEK_SET) == 0) {
        count = fread(buf, 1, count, range->file);
        if (count > 0) {
            return count;
        }
    }

    return MHD_CONTENT_READER_END_OF_STREAM;
}

static void fileFreeCallback(void *cls) {
    Range* range = static_cast<Range*>(cls);
    fclose(range->file);
    delete range;
}

static Range* parseRange(FILE* file, const char* range) {
    Range* result = new Range();

    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    fseek(file, 0, SEEK_SET);

    result->file = file;
    result->total = size;
    result->from = 0;
    result->to = (size == 0) ? 0 : size - 1;

    if (range) {
        std::string str(range);

        if (str.substr(0, 6) == "bytes=") {
            str = str.substr(6);

            std::vector<std::string> parts;
            boost::split(parts, str, boost::is_any_of("-"));

            if (parts.size() == 2) {
                try {
                    size_t from = std::stoul(boost::algorithm::trim_copy(parts[0]));
                    size_t to = size;

                    if (parts.at(1).size()) {
                        to = std::stoul(boost::algorithm::trim_copy(parts[1]));
                    }

                    result->from = from;
                    result->to = (to == 0) ? 0 : to - 1;
                }
                catch (...) {
                    /* return false below */
                }
            }
        }
    }

    return result;
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
        httpServer = MHD_start_daemon(
#if MHD_VERSION >= 0x00095300
            MHD_USE_AUTO | MHD_USE_INTERNAL_POLLING_THREAD,
#else
            MHD_USE_SELECT_INTERNALLY,
#endif
            context.prefs->GetInt(prefs::http_server_port.c_str(), defaults::http_server_port),
            nullptr,
            nullptr,
            &HttpServer::HandleRequest,
            this,
            MHD_OPTION_UNESCAPE_CALLBACK,
            &HttpServer::HandleUnescape,
            this,
            MHD_OPTION_END);

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

int HttpServer::HandleRequest(
    void *cls,
    struct MHD_Connection *connection,
    const char *url,
    const char *method,
    const char *version,
    const char *upload_data,
    size_t *upload_data_size,
    void **con_cls)
{
    HttpServer* server = static_cast<HttpServer*>(cls);

    struct MHD_Response* response = nullptr;
    int ret = MHD_NO;
    int status = MHD_HTTP_OK;

    try {
        std::string urlStr(url);
        if (urlStr[0] == '/') {
            urlStr = urlStr.substr(1);
        }

        std::vector<std::string> parts;
        boost::split(parts, urlStr, boost::is_any_of("/"));
        if (parts.size() > 0) {
            if (parts.at(0) == fragment::audio && parts.size() == 3) {
                IRetainedTrack* track = nullptr;

                if (parts.at(1) == fragment::id) {
                    musik_uint64 id = std::stoull(urlDecode(parts.at(2)));
                    track = server->context.dataProvider->QueryTrackById(id);
                }
                else if (parts.at(1) == fragment::external_id) {
                    std::string externalId = urlDecode(parts.at(2));
                    track = server->context.dataProvider->QueryTrackByExternalId(externalId.c_str());
                }

                if (track) {
                    std::string filename = GetMetadataString(track, key::filename);
                    track->Release();
#ifdef WIN32
                    FILE* file = _wfopen(utf8to16(filename.c_str()).c_str(), L"rb");
#else
                    FILE* file = fopen(filename.c_str(), "rb");
#endif
                    if (file) {
                        const char* rangeVal = MHD_lookup_connection_value(
                            connection, MHD_HEADER_KIND, "Range");

                        Range* range = parseRange(file, rangeVal);

                        size_t length = (range->to - range->from);

                        response = MHD_create_response_from_callback(
                            length == 0 ? 0 : length + 1,
                            4096,
                            &fileReadCallback,
                            range,
                            &fileFreeCallback);

                        if (response) {
                            MHD_add_response_header(response, "Accept-Ranges", "bytes");
                            MHD_add_response_header(response, "Content-Type", contentType(filename).c_str());
                            MHD_add_response_header(response, "Server", "musikcube websocket_remote");

                            if ((rangeVal && strlen(rangeVal)) || range->from > 0) {
                                MHD_add_response_header(response, "Content-Range", range->HeaderValue().c_str());
                                status = MHD_HTTP_PARTIAL_CONTENT;
                            }
                        }
                        else {
                            fclose(file);
                        }
                    }

                }
            }
        }
    }
    catch (...) {
    }

    if (response) {
        ret = MHD_queue_response(connection, status, response);
        MHD_destroy_response(response);
    }

    return ret;
}
