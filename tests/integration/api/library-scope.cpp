#include <musikcore/musikcore.h>
#include <sqlite3.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <thread>

namespace {
    void require(bool value, const char* message) {
        if (!value) {
            std::fprintf(stderr, "library scope: %s\n", message);
            std::abort();
        }
    }
    struct Reply {
        bool done = false;
        mc_result status = MC_OK;
        size_t rows = 0;
    };
    void MC_CALL completed(mc_result status, mc_track_page* page, const mc_error*, void* data) {
        auto& reply = *static_cast<Reply*>(data);
        reply.status = status;
        reply.rows = mc_track_page_get_count(page);
        reply.done = true;
        mc_track_page_destroy(&page);
    }
    void wait(mc_context* context, Reply& reply) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!reply.done && std::chrono::steady_clock::now() < deadline) {
            mc_context_dispatch(context);
            /* bounded dispatch pumping for this worker/dispatcher contract test. */
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        require(reply.done, "completion deadline");
    }
    void MC_CALL changed(void* data, const void* event) {
        ++*static_cast<int*>(data);
        auto& change = *static_cast<const mc_library_scope_change*>(event);
        mc_library_scope_snapshot* snapshot = nullptr;
        require(mc_library_get_scope(change.library, &snapshot) == MC_OK, "reentrant scope read");
        mc_library_scope_destroy(&snapshot);
    }
}

int main(int argc, char** argv) {
    require(argc == 2, "expected fixture directory");
    const auto directory =
        std::filesystem::path(argv[1]) / std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    std::filesystem::create_directories(directory);
    sqlite3* database = nullptr;
    require(sqlite3_open((directory / "musik.db").string().c_str(), &database) == SQLITE_OK, "open fixture");
    require(sqlite3_exec(database,
                "CREATE TABLE tracks(id INTEGER PRIMARY KEY,title TEXT,visible INTEGER,source_id INTEGER);"
                "INSERT INTO tracks VALUES(1,'Files',1,0),(2,'Provider',1,7);",
                nullptr, nullptr, nullptr) == SQLITE_OK,
        "seed fixture");
    sqlite3_close(database);
    const auto locator = directory.string();
    auto options = mc_library_options MC_LIBRARY_OPTIONS_DEFAULT;
    options.locator = { locator.data(), locator.size() };
    mc_library* library = nullptr;
    mc_library* independent = nullptr;
    require(mc_library_create(&options, &library) == MC_OK && mc_library_create(&options, &independent) == MC_OK,
        "create instances");
    mc_context* context = nullptr;
    auto contextOptions = mc_context_options MC_CONTEXT_OPTIONS_DEFAULT;
    contextOptions.services = MC_SERVICE_NONE;
    require(mc_context_create(&contextOptions, &context, nullptr) == MC_OK, "create context");
    require(mc_context_set_library(context, library) == MC_OK, "select instance");
    mc_library* alias = nullptr;
    require(mc_context_get_library(context, &alias) == MC_OK, "get alias");
    int first = 0, second = 0;
    mc_subscription* one = nullptr;
    mc_subscription* two = nullptr;
    auto legacyScope = mc_library_scope MC_LIBRARY_SCOPE_DEFAULT;
    legacyScope.struct_size = sizeof(legacyScope.struct_size);
    require(mc_library_set_scope(library, &legacyScope) == MC_OK, "older scope defaults missing filter to null");
    require(mc_library_subscribe_scope(library, changed, &first, &one) == MC_OK &&
            mc_library_subscribe_scope(alias, changed, &second, &two) == MC_OK,
        "subscribe observers");
    int32_t ids[] { 7, 0, 7 };
    auto scope = mc_library_scope MC_LIBRARY_SCOPE_DEFAULT;
    mc_library_source_filter filter = MC_LIBRARY_SOURCE_FILTER_DEFAULT;
    scope.sources = &filter;
    filter.ids = ids;
    filter.count = 3;
    require(mc_library_set_scope(library, &scope) == MC_OK && first == 1 && second == 1, "effective change");
    require(mc_library_set_scope(alias, &scope) == MC_OK && first == 1, "equivalent set");
    mc_library_scope_snapshot* captured = nullptr;
    require(mc_library_get_scope(alias, &captured) == MC_OK, "capture alias");
    ids[0] = 99;
    const auto* view = mc_library_scope_read(captured);
    require(
        view->sources->count == 2 && view->sources->ids[0] == 0 && view->sources->ids[1] == 7, "copied normalized IDs");
    mc_library_scope_snapshot* separate = nullptr;
    require(
        mc_library_get_scope(independent, &separate) == MC_OK && mc_library_scope_read(separate)->sources == nullptr,
        "independent instance");
    mc_library_scope_destroy(&separate);
    scope.struct_size = sizeof(scope) + 8;
    require(
        mc_library_set_scope(library, &scope) == MC_ERROR_UNSUPPORTED && first == 1, "unknown future filters rejected");
    scope.struct_size = sizeof(scope);
    filter.struct_size = offsetof(mc_library_source_filter, count);
    require(mc_library_set_scope(library, &scope) == MC_ERROR_INVALID_ARGUMENT, "present missing field rejected");
    filter.struct_size = sizeof(filter) + 8;
    require(mc_library_set_scope(library, &scope) == MC_ERROR_UNSUPPORTED && first == 1,
        "unknown future filter layout rejected without event");
    filter.struct_size = sizeof(filter);
    filter.count = MC_LIBRARY_SCOPE_MAX_SOURCES + 1;
    require(mc_library_set_scope(library, &scope) == MC_ERROR_INVALID_ARGUMENT, "selection bound before reading");
    filter.count = 1;
    filter.ids = nullptr;
    require(mc_library_set_scope(library, &scope) == MC_ERROR_INVALID_ARGUMENT, "null array rejected");
    int32_t invalid = -1;
    filter.ids = &invalid;
    require(mc_library_set_scope(library, &scope) == MC_ERROR_INVALID_ARGUMENT, "sentinel rejected");
    auto query = mc_track_query MC_TRACK_QUERY_DEFAULT;
    Reply old;
    mc_request_id request = 0;
    require(mc_library_query_tracks_async(context, library, &query, completed, &old, &request) == MC_OK,
        "submit original intent");
    filter.ids = ids;
    filter.count = 0;
    require(mc_library_set_scope(alias, &scope) == MC_OK, "switch to empty");
    wait(context, old);
    require(old.status == MC_ERROR_CANCELLED && old.rows == 0, "invalidate queued completion");
    Reply empty;
    require(mc_library_query_tracks_async(context, nullptr, &query, completed, &empty, &request) == MC_OK,
        "context-selected scope");
    wait(context, empty);
    require(empty.status == MC_OK && empty.rows == 0, "empty selection");
    int32_t files = 0;
    filter.ids = &files;
    filter.count = 1;
    require(mc_library_set_scope(library, &scope) == MC_OK, "select Files");
    Reply fileRows;
    require(
        mc_library_query_tracks_async(context, alias, &query, completed, &fileRows, &request) == MC_OK, "query Files");
    wait(context, fileRows);
    require(fileRows.status == MC_OK && fileRows.rows == 1, "source zero filters before page");
    /* once admitted, a callback may finish while another thread replaces scope.
    joining that update also proves delivery holds no scope lock. */
    struct Admitted {
        mc_library* library;
        Reply reply;
    } admitted { library, {} };
    const auto finishAdmitted = [](mc_result status, mc_track_page* page, const mc_error*, void* data) {
        auto& admitted = *static_cast<Admitted*>(data);
        std::thread update([&] {
            auto scope = mc_library_scope MC_LIBRARY_SCOPE_DEFAULT;
            auto sources = mc_library_source_filter MC_LIBRARY_SOURCE_FILTER_DEFAULT;
            scope.sources = &sources;
            require(mc_library_set_scope(admitted.library, &scope) == MC_OK, "update during admitted callback");
        });
        update.join();
        completed(status, page, nullptr, &admitted.reply);
    };
    require(mc_library_query_tracks_async(context, library, &query, finishAdmitted, &admitted, &request) == MC_OK,
        "submit admitted completion");
    wait(context, admitted.reply);
    require(admitted.reply.status == MC_OK && admitted.reply.rows == 1, "admission wins before scope update");
    require(mc_library_scope_read(captured)->sources->count == 2, "snapshot survives updates");
    mc_subscription_destroy(&one);
    mc_subscription_destroy(&two);
    mc_library_scope_destroy(&captured);
    mc_context_shutdown(context);
    mc_context_destroy(&context);
    mc_library_destroy(&alias);
    mc_library_destroy(&library);
    mc_library_destroy(&independent);
    std::filesystem::remove_all(directory);
}
