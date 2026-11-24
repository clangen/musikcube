#include <musikcore/plugin.h>
#include <musikcore/plugin/decoder.h>
#include <musikcore/plugin/indexer.h>
#include <musikcore/plugin/stream.h>
#include <windows.h>
#include <dbt.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct mc_metadata_record {
    std::map<std::string, std::string> values;
};

struct mc_audio_scratch_buffer {
    mc_audio_format format = MC_AUDIO_FORMAT_DEFAULT;
    std::vector<float> samples;
    size_t frames = 0;
};

namespace {
    const mc_stream_factory* streamFactory = nullptr;
    const mc_decoder_factory* decoderFactory = nullptr;
    const mc_indexer_source_factory* indexerFactory = nullptr;
    std::atomic<unsigned> rescans { 0 };
    std::vector<std::map<std::string, std::string>> tracks;

    void Require(bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "CDDA plugin: %s\n", message);
            std::abort();
        }
    }

    mc_result MC_CALL Register(void*, const mc_plugin_capability* capability) {
        const std::string_view id(capability->id.data, capability->id.length);
        if (id == MC_PLUGIN_CAPABILITY_STREAM) {
            streamFactory = static_cast<const mc_stream_factory*>(capability->api);
        }
        else if (id == MC_PLUGIN_CAPABILITY_DECODER) {
            decoderFactory = static_cast<const mc_decoder_factory*>(capability->api);
        }
        else if (id == MC_PLUGIN_CAPABILITY_INDEXER_SOURCE) {
            indexerFactory = static_cast<const mc_indexer_source_factory*>(capability->api);
        }
        return MC_OK;
    }

    void MC_CALL Log(void*, mc_log_level, mc_string_view, mc_string_view) {
    }
    mc_result MC_CALL Error(void*, mc_result, mc_string_view, mc_error**) {
        return MC_ERROR_INTERNAL;
    }
    void MC_CALL DestroyError(void*, mc_error**) {
    }
    mc_result MC_CALL RequestRescan(void*) {
        ++rescans;
        return MC_OK;
    }

    BOOL CALLBACK FindMonitor(HWND window, LPARAM context) {
        DWORD process = 0;
        GetWindowThreadProcessId(window, &process);
        if (process != GetCurrentProcessId()) {
            return TRUE;
        }
        wchar_t name[64] {};
        GetClassNameW(window, name, static_cast<int>(std::size(name)));
        if (std::wstring_view(name).starts_with(L"musikcube-cdda-monitor-")) {
            *reinterpret_cast<HWND*>(context) = window;
            return FALSE;
        }
        return TRUE;
    }

    mc_result MC_CALL CreateRecord(void*, mc_metadata_record** record) {
        *record = new mc_metadata_record;
        return MC_OK;
    }

    mc_result MC_CALL SetValue(void* context, mc_string_view key, mc_string_view value) {
        auto& record = *static_cast<mc_metadata_record*>(context);
        record.values[std::string(key.data, key.length)] = std::string(value.data, value.length);
        return MC_OK;
    }

    mc_result MC_CALL GetWriter(void*, mc_metadata_record* record, mc_metadata_writer* writer) {
        *writer = MC_METADATA_WRITER_DEFAULT;
        writer->context = record;
        writer->set_value = SetValue;
        return MC_OK;
    }

    void MC_CALL DestroyRecord(void*, mc_metadata_record** record) {
        delete *record;
        *record = nullptr;
    }

    mc_result MC_CALL Save(void*, const mc_metadata_record* record, mc_string_view id) {
        Require(id.length != 0, "indexed track has a stable external ID");
        tracks.push_back(record->values);
        return MC_OK;
    }

    mc_result MC_CALL LastModified(void*, mc_string_view, int64_t*) {
        return MC_ERROR_NOT_FOUND;
    }

    mc_result MC_CALL SetFormat(void*, mc_audio_scratch_buffer* buffer, const mc_audio_format* format) {
        buffer->format = *format;
        return MC_OK;
    }

    mc_result MC_CALL Reserve(void*, mc_audio_scratch_buffer* buffer, size_t frames) {
        buffer->samples.resize(frames * buffer->format.channels);
        return MC_OK;
    }

    mc_result MC_CALL BeginWrite(void*, mc_audio_scratch_buffer* buffer, mc_audio_buffer_view* view) {
        view->data = buffer->samples.data();
        return MC_OK;
    }

    mc_result MC_CALL EndWrite(void*, mc_audio_scratch_buffer* buffer, size_t frames) {
        buffer->frames = frames;
        return MC_OK;
    }
}

int main(int argc, char** argv) {
    Require(argc >= 2, "plugin path supplied");
    const HMODULE library = LoadLibraryA(argv[1]);
    Require(library != nullptr, "load combined plugin DLL");
    const auto entry = reinterpret_cast<mc_plugin_get_api_fn>(GetProcAddress(library, "mc_plugin_get_api"));
    Require(entry != nullptr, "C plugin entry exists");

    mc_host_api host = MC_HOST_API_DEFAULT;
    host.log = Log;
    host.create_error = Error;
    host.destroy_error = DestroyError;
    host.register_plugin_capability = Register;
    mc_plugin_api plugin = MC_PLUGIN_API_DEFAULT;
    Require(entry(MC_PLUGIN_ABI_VERSION, &host, &plugin, nullptr) == MC_OK, "negotiate C plugin");
    void* pluginState = nullptr;
    Require(
        plugin.initialize(&host, &pluginState, nullptr) == MC_OK && streamFactory && decoderFactory && indexerFactory,
        "register stream, decoder, and indexer together");

    mc_indexer_source_host_api sourceHost = MC_INDEXER_SOURCE_HOST_API_DEFAULT;
    sourceHost.request_rescan = RequestRescan;
    mc_plugin_services services = MC_PLUGIN_SERVICES_DEFAULT;
    services.host = &host;
    services.indexer_source = &sourceHost;
    void* source = nullptr;
    Require(indexerFactory->create(indexerFactory->context, &services, &source, nullptr) == MC_OK && source,
        "create indexer and its device monitor");
    Require(rescans.load() != 0, "request initial scan for an already-inserted disc");

    HWND monitor = nullptr;
    EnumWindows(FindMonitor, reinterpret_cast<LPARAM>(&monitor));
    Require(monitor != nullptr, "device monitor owns a top-level window");
    DEV_BROADCAST_VOLUME volume {};
    volume.dbcv_size = sizeof(volume);
    volume.dbcv_devicetype = DBT_DEVTYP_VOLUME;
    volume.dbcv_flags = DBTF_MEDIA;
    const auto previousRescans = rescans.load();
    SendMessageW(monitor, WM_DEVICECHANGE, DBT_DEVICEARRIVAL, reinterpret_cast<LPARAM>(&volume));
    Require(rescans.load() > previousRescans, "device arrival requests another scan");
    const auto afterArrival = rescans.load();
    SendMessageW(monitor, WM_DEVICECHANGE, DBT_DEVICEREMOVECOMPLETE, reinterpret_cast<LPARAM>(&volume));
    Require(rescans.load() > afterArrival, "device removal requests another scan");

    mc_indexer_source_info info = MC_INDEXER_SOURCE_INFO_DEFAULT;
    Require(indexerFactory->instance_api->get_info(source, &info) == MC_OK && info.stable_ids,
        "describe stable indexer source");
    if (argc > 2) {
        /* Exercise stream and decoder through the same factory tables that
        musikcore uses, with a real disc and host-owned scratch storage. */
        const std::string path(argv[2]);
        mc_media_hint hint = MC_MEDIA_HINT_DEFAULT;
        hint.uri = { path.data(), path.size() };
        uint32_t supported = 0;
        Require(streamFactory->can_handle(streamFactory->context, &hint, &supported) == MC_OK && supported,
            "stream claims the CD track URI");

        mc_stream stream = MC_STREAM_DEFAULT;
        stream.api = streamFactory->instance_api;
        Require(streamFactory->create(streamFactory->context, &services, &stream.instance, nullptr) == MC_OK &&
                stream.api->open(stream.instance, hint.uri, MC_STREAM_OPEN_READ, nullptr) == MC_OK,
            "open CD stream");
        mc_stream_info streamInfo = MC_STREAM_INFO_DEFAULT;
        Require(stream.api->get_info(stream.instance, &streamInfo) == MC_OK, "read stream format hint");
        hint.extension = streamInfo.extension;
        Require(decoderFactory->can_handle(decoderFactory->context, &hint, &supported) == MC_OK && supported,
            "decoder claims CD PCM");

        mc_audio_scratch_buffer_api scratch = MC_AUDIO_SCRATCH_BUFFER_API_DEFAULT;
        scratch.set_format = SetFormat;
        scratch.reserve = Reserve;
        scratch.begin_write = BeginWrite;
        scratch.end_write = EndWrite;
        services.audio_scratch = &scratch;
        void* decoder = nullptr;
        Require(decoderFactory->create(decoderFactory->context, &services, &decoder, nullptr) == MC_OK &&
                decoderFactory->instance_api->open(decoder, &stream, nullptr) == MC_OK,
            "open CD decoder");
        mc_audio_scratch_buffer pcm;
        mc_decode_state decoded = MC_DECODE_END;
        Require(decoderFactory->instance_api->decode(decoder, &pcm, &decoded, nullptr) == MC_OK && pcm.frames == 1024 &&
                pcm.format.sample_rate == 44100 && pcm.format.channels == 2,
            "decode CD audio through the C plugin API");
        decoderFactory->instance_api->destroy(decoder);
        Require(stream.api->close(stream.instance) == MC_OK, "close CD stream");
        stream.api->destroy(stream.instance);

        mc_indexer_writer writer = MC_INDEXER_WRITER_DEFAULT;
        writer.create_record = CreateRecord;
        writer.get_writer = GetWriter;
        writer.destroy_record = DestroyRecord;
        writer.save = Save;
        writer.last_modified = LastModified;
        mc_indexer_scan_decision decision = MC_INDEXER_SCAN_ROLLBACK;
        const auto* api = indexerFactory->instance_api;
        Require(api->begin_scan(source, nullptr) == MC_OK, "begin disc scan");
        Require(
            api->scan(source, nullptr, 0, &writer, &decision, nullptr) == MC_OK && decision == MC_INDEXER_SCAN_COMMIT,
            "scan disc tracks");
        Require(api->end_scan(source, decision, nullptr) == MC_OK, "end scan");
        Require(!tracks.empty(), "disc produced indexed tracks");
        for (const auto& track : tracks) {
            Require(track.count("filename") && track.count("title") && track.count("duration") &&
                track.count("genre") && !track.at("genre").empty(),
                "indexed track has a path, title, duration, and genre");
        }
        std::printf("Indexed %zu tracks; album: %s\n", tracks.size(), tracks.front()["album"].c_str());
    }

    indexerFactory->instance_api->destroy(source);
    plugin.shutdown(pluginState);
    Require(FreeLibrary(library) != 0, "unload combined plugin after monitor joins");
}
