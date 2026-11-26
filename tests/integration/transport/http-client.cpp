#include <common/HttpClient.h>
#include <common/HttpUrl.h>

#include <sstream>
#include <stop_token>
#include <string>
#include <thread>
#include <iostream>

int main(int argc, char** argv) {
    if (musik::core::sdk::EncodeUrlValue("a b+\xc3\xa9") != "a%20b%2B%C3%A9") {
        return 1;
    }
    if (argc != 4) {
        return 1;
    }
    using Client = musik::core::sdk::HttpClient<std::stringstream>;
    const std::string mode = argv[2];
    auto client = Client::Create(std::stringstream());
    bool called = false;
    bool success = false;
    bool canceled = false;
    bool headerSeen = false;
    int status = 0;
    std::stop_source stop;
    std::thread cancelThread;
    client->Url(std::string(argv[3]) + "://127.0.0.1:" + argv[1] + "/" + mode)
        .Mode(mode == "background" ? Client::Thread::Background : Client::Thread::Current)
        .Headers([&](const std::string& key, const std::string& value) {
            if ((key == "X-Reply" || key == "x-reply") && value == "present") {
                headerSeen = true;
            }
        })
        .Canceled([&](Client*) {
            canceled = true;
        });
    if (mode == "cancel") {
        stop.request_stop();
        client->Cancellation(stop.get_token());
    }
    else if (mode == "cancel-active") {
        client->Cancellation(stop.get_token());
        cancelThread = std::thread([&] {
            std::string line;
            std::getline(std::cin, line);
            stop.request_stop();
        });
    }
    else if (mode == "timeout") {
        client->Timeout(100);
    }
    else if (mode == "verified" || mode == "untrusted-ip") {
        client->VerifyTls(true);
        if (mode == "untrusted-ip") {
            client->AllowUntrustedAuthority(true);
        }
    }
    else if (mode == "post" || mode == "background") {
        client->Method(Client::HttpMethod::Post).Header("X-Test", "yes").PostBody("hello");
    }
    client->Run([&](Client*, int code, bool ok) {
        called = true;
        success = ok;
        status = code;
    });
    if (mode == "background") {
        client->Wait();
    }
    if (cancelThread.joinable()) {
        cancelThread.join();
    }
    const bool expectBody = mode == "post" || mode == "background" || mode == "get" || mode == "redirect";
    const bool valid = expectBody ? success && status == 200 && client->Stream().str() == "world" && headerSeen
        : mode == "error"         ? !success && status == 404
        : (mode == "verified" || mode == "untrusted-ip")
        ? !success && client->Error() == musik::core::sdk::HttpError::Certificate
        : !success && (mode == "timeout" || canceled);
    return called && valid ? 0 : 1;
}
