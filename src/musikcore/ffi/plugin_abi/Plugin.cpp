#include "Plugin.h"
#include <common/Contract.h>
#include "../common/error.h"
#include <musikcore/plugin/output.h>
#include <musikcore/plugin/decoder.h>
#include <musikcore/plugin/tag.h>
#include <musikcore/plugin/indexer.h>
#include <cmath>
#include <musikcore/plugin/dsp.h>
#include <musikcore/plugin/analyzer.h>
#include <musikcore/plugin/encoder.h>

#include <climits>
#include <cstring>
#include <exception>
#include <unordered_set>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace musik { namespace core { namespace ffi {
    namespace {
        /* accept empty text or valid UTF-8 without embedded NUL bytes, up to
        maximum bytes. nonempty views require data. bound metadata copies and
        reject invalid Unicode before passing text to logs or the UI. */
        bool validText(mc_string_view text, size_t maximum = MC_PLUGIN_METADATA_MAX_BYTES) {
            if (text.length > maximum || (text.length && !text.data)) {
                return false;
            }
            size_t i = 0;
            while (i < text.length) {
                const auto first = static_cast<unsigned char>(text.data[i++]);
                if (!first) {
                    return false;
                }
                if (first < 0x80) {
                    continue;
                }
                /* determine the UTF-8 sequence length from its leading byte. */
                unsigned remaining = 0;
                if (first >= 0xc2 && first <= 0xdf) {
                    remaining = 1;
                }
                else if (first >= 0xe0 && first <= 0xef) {
                    remaining = 2;
                }
                else if (first >= 0xf0 && first <= 0xf4) {
                    remaining = 3;
                }
                if (!remaining || remaining > text.length - i) {
                    return false;
                }
                const unsigned minimum = remaining == 1 ? 0x80 : remaining == 2 ? 0x800 : 0x10000;
                unsigned value = first & (0x7f >> (remaining + 1));
                while (remaining--) {
                    const auto byte = static_cast<unsigned char>(text.data[i++]);
                    if ((byte & 0xc0) != 0x80) {
                        return false;
                    }
                    value = (value << 6) | (byte & 0x3f);
                }
                /* reject overlong encodings, values beyond Unicode, and UTF-16
                surrogates: each sequence must encode a Unicode scalar value. */
                if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) {
                    return false;
                }
            }
            return true;
        }

        /* accept 1..MC_PLUGIN_ID_MAX_BYTES ASCII characters from [A-Za-z0-9_.-].
        stable identifiers exclude whitespace and non-ASCII text so matching
        needs no locale rules or Unicode normalization. */
        bool validId(mc_string_view id) {
            if (!id.length || !validText(id, MC_PLUGIN_ID_MAX_BYTES)) {
                return false;
            }
            for (size_t i = 0; i < id.length; ++i) {
                const char c = id.data[i];
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                        c == '_' || c == '-'))
                {
                    return false;
                }
            }
            return true;
        }

        bool compatible(uint32_t version) {
            return version != 0 && MC_ABI_VERSION_MAJOR(version) == MC_ABI_VERSION_MAJOR(MC_PLUGIN_ABI_VERSION) &&
                MC_ABI_VERSION_MINOR(version) <= MC_ABI_VERSION_MINOR(MC_PLUGIN_ABI_VERSION);
        }

        /* validate a factory once, while it can still be rejected as a whole.
        all three families publish immutable, plugin-owned method tables. */
        template<typename Factory, typename Check>
        bool validFactory(const mc_plugin_capability& capability, Check check) {
            if (capability.api_size < sizeof(Factory) || reinterpret_cast<uintptr_t>(capability.api) % alignof(Factory)) {
                return false;
            }
            const auto& factory = *static_cast<const Factory*>(capability.api);
            const auto* methods = factory.instance_api;
            using Methods = std::remove_pointer_t<decltype(methods)>;
            return factory.create && methods && reinterpret_cast<uintptr_t>(methods) % alignof(Methods) == 0 &&
                methods->struct_size >= sizeof(Methods) && methods->destroy && check(factory, *methods);
        }

        bool validCapability(const std::string& id, const mc_plugin_capability& capability) {
            if (id == MC_PLUGIN_CAPABILITY_CONFIGURATION) {
                if (capability.api_size < sizeof(mc_configuration_api) ||
                    reinterpret_cast<uintptr_t>(capability.api) % alignof(mc_configuration_api))
                {
                    return false;
                }
                const auto& table = *static_cast<const mc_configuration_api*>(capability.api);
                if (table.integer_count && (!table.integers ||
                    reinterpret_cast<uintptr_t>(table.integers) % alignof(mc_integer_setting)))
                {
                    return false;
                }
                if ((table.boolean_count && (!table.booleans ||
                        reinterpret_cast<uintptr_t>(table.booleans) % alignof(mc_boolean_setting))) ||
                    (table.number_count && (!table.numbers ||
                        reinterpret_cast<uintptr_t>(table.numbers) % alignof(mc_number_setting))) ||
                    (table.text_count && (!table.texts ||
                        reinterpret_cast<uintptr_t>(table.texts) % alignof(mc_text_setting))))
                {
                    return false;
                }

                std::unordered_set<std::string_view> keys;
                for (size_t i = 0; i < table.integer_count; ++i) {
                    const auto& setting = table.integers[i];
                    if (!validId(setting.key) || setting.minimum > setting.default_value ||
                        setting.maximum < setting.default_value ||
                        !keys.emplace(setting.key.data, setting.key.length).second)
                    {
                        return false;
                    }
                }
                for (size_t i = 0; i < table.boolean_count; ++i) {
                    const auto& setting = table.booleans[i];
                    if (!validId(setting.key) || !keys.emplace(setting.key.data, setting.key.length).second) {
                        return false;
                    }
                }
                for (size_t i = 0; i < table.number_count; ++i) {
                    const auto& setting = table.numbers[i];
                    if (!validId(setting.key) || !std::isfinite(setting.default_value) ||
                        !std::isfinite(setting.minimum) || !std::isfinite(setting.maximum) ||
                        setting.default_value < setting.minimum || setting.default_value > setting.maximum ||
                        setting.precision < 0 || !keys.emplace(setting.key.data, setting.key.length).second)
                    {
                        return false;
                    }
                }
                for (size_t i = 0; i < table.text_count; ++i) {
                    const auto& setting = table.texts[i];
                    if (!validId(setting.key) || !validText(setting.default_value) ||
                        !keys.emplace(setting.key.data, setting.key.length).second)
                    {
                        return false;
                    }
                }
                return true;
            }
            if (id == MC_PLUGIN_CAPABILITY_TAG_READER) {
                return validFactory<mc_tag_reader_factory>(capability, [](const auto& factory, const auto& methods) {
                    return factory.can_read && methods.read;
                });
            }
            if (id == MC_PLUGIN_CAPABILITY_INDEXER_SOURCE) {
                return validFactory<mc_indexer_source_factory>(capability, [](const auto&, const auto& methods) {
                    return methods.get_info && methods.begin_scan && methods.scan && methods.validate_track &&
                        methods.end_scan && methods.interrupt;
                });
            }
            if (id == MC_PLUGIN_CAPABILITY_ENCODER) {
                if (capability.api_size < sizeof(mc_encoder_factory) ||
                    reinterpret_cast<uintptr_t>(capability.api) % alignof(mc_encoder_factory))
                {
                    return false;
                }
                const auto& factory = *static_cast<const mc_encoder_factory*>(capability.api);
                return factory.can_handle && factory.create;
            }
            if (id == MC_PLUGIN_CAPABILITY_STREAM) {
                return validFactory<mc_stream_factory>(capability, [](const auto& factory, const auto& methods) {
                    return factory.can_handle && methods.open && methods.close && methods.interrupt && methods.get_info &&
                        methods.read && methods.write && methods.seek && methods.tell;
                });
            }
            if (id == MC_PLUGIN_CAPABILITY_DECODER) {
                return validFactory<mc_decoder_factory>(capability, [](const auto& factory, const auto& methods) {
                    return factory.can_handle && methods.open && methods.interrupt && methods.decode && methods.seek &&
                        methods.get_duration && methods.set_preferred_sample_rate;
                });
            }
            if (id == MC_PLUGIN_CAPABILITY_DSP) {
                return validFactory<mc_dsp_factory>(capability, [](const auto& factory, const auto& methods) {
                    return factory.reload && methods.process;
                });
            }
            if (id == MC_PLUGIN_CAPABILITY_PLAYBACK_REMOTE) {
                return validFactory<mc_playback_remote_factory>(capability, [](const auto&, const auto& methods) {
                    return methods.changed && methods.queue_changed && methods.start && methods.request_stop &&
                        methods.join;
                });
            }
            if (id == MC_PLUGIN_CAPABILITY_ANALYZER) {
                return validFactory<mc_analyzer_factory>(capability, [](const auto&, const auto& methods) {
                    return methods.begin && methods.process && methods.finish;
                });
            }
            if (id == MC_PLUGIN_CAPABILITY_OUTPUT) {
                return validFactory<mc_output_factory>(capability, [](const auto& factory, const auto& methods) {
                    return factory.visit_devices && factory.read_default_device && methods.get_default_format &&
                        methods.prepare && methods.queue_audio_buffer && methods.pause && methods.resume &&
                        methods.set_volume && methods.get_volume && methods.stop && methods.drain && methods.get_timing;
                });
            }
            return true;
        }

        /* catch only the actual foreign entry/initialize call. host work stays
        outside this boundary, so its exceptions are not blamed on the plugin. */
        template<typename Work>
        mc_result invokePlugin(PluginCallState& calls, const char* operation, mc_error** error, Work&& work) {
            PluginCallState::Scope scope(calls);
            try {
                return work();
            }
            catch (const std::exception& exception) {
                mc_error_destroy(error);
                try {
                    const std::string message = std::string(operation) + ": " + exception.what();
                    return fail(MC_ERROR_PLUGIN, message.c_str(), error);
                }
                catch (...) {
                    return fail(MC_ERROR_PLUGIN, operation, error);
                }
            }
            catch (...) {
                mc_error_destroy(error);
                return fail(MC_ERROR_PLUGIN, operation, error);
            }
        }

        /* the optional diagnostic is core-owned and may outlive native unload. */
        mc_result transferError(mc_result status, mc_error* detail, mc_error** error) {
            if (detail && mc_error_get_code(detail) != status) {
                mc_error_destroy(&detail);
                status = MC_ERROR_PLUGIN;
            }
            if (error && detail) {
                *error = detail;
            }
            else {
                mc_error_destroy(&detail);
                fail(status, "Provider initialization or negotiation failed", error);
            }
            return status;
        }
    }


    /* native handles never escape into the public C tables. */
    class NativeLibrary {
    public:
        explicit NativeLibrary(const std::string& filename) {
#ifdef _WIN32
            if (filename.size() >= static_cast<size_t>(INT_MAX)) {
                return;
            }
            const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.c_str(), -1, nullptr, 0);
            if (!length) {
                return;
            }
            std::vector<wchar_t> wide(static_cast<size_t>(length));
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.c_str(), -1, wide.data(), length);
            this->handle = LoadLibraryExW(
                wide.data(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
            this->handle = dlopen(filename.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif
        }
        ~NativeLibrary() { this->Close(); }

        void Close() {
            const auto handle = std::exchange(this->handle, nullptr);
            if (handle) {
#ifdef _WIN32
                FreeLibrary(handle);
#else
                dlclose(handle);
#endif
            }
        }

        MUSIK_DELETE_COPY_AND_MOVE(NativeLibrary);

        bool IsOpen() const { return this->handle != nullptr; }
        bool IsSame(const NativeLibrary& other) const { return this->handle == other.handle; }
        mc_plugin_get_api_fn Entry() const {
#ifdef _WIN32
            return reinterpret_cast<mc_plugin_get_api_fn>(GetProcAddress(this->handle, "mc_plugin_get_api"));
#else
            return reinterpret_cast<mc_plugin_get_api_fn>(dlsym(this->handle, "mc_plugin_get_api"));
#endif
        }

    private:
#ifdef _WIN32
        HMODULE handle = nullptr;
#else
        void* handle = nullptr;
#endif
    };


    std::unique_ptr<Plugin> Plugin::Open(const std::string& filename, PluginRegistry::Logger logger) {
        if (filename.empty() || filename.find('\0') != std::string::npos) {
            return {};
        }
        auto plugin = std::unique_ptr<Plugin>(new Plugin(filename, std::move(logger)));
        if (!plugin->native->IsOpen()) {
            return {};
        }
        return plugin;
    }

    Plugin::Plugin(const std::string& filename, PluginRegistry::Logger logger)
    : native(std::make_unique<NativeLibrary>(filename))
    , logger(std::move(logger))
    , host { sizeof(mc_host_api), MC_PLUGIN_ABI_VERSION, this, Log, CreateError, DestroyError, Register }
    , metadata { filename, {}, {}, {}, {}, false } {
        this->api.struct_size = static_cast<uint32_t>(sizeof(this->api));
    }

    Plugin::~Plugin() {
        const auto status = this->Stop();
        musik::contract::Assert(status == MC_OK, "Plugin destruction must run on its owning thread");
    }

    mc_result Plugin::ValidateCallingThread() const {
        if (!this->calls.IsOwnerThread()) {
            return MC_ERROR_WRONG_THREAD;
        }
        musik::contract::Assert(!this->calls.IsActive(), "Plugin lifecycle reentered an active call");
        return MC_OK;
    }

    mc_result Plugin::Stop() {
        if (!this->calls.IsOwnerThread()) {
            return MC_ERROR_WRONG_THREAD;
        }
        {
            std::lock_guard<std::mutex> lock(this->mutex);
            if (this->phase == Phase::Stopped) {
                return MC_OK;
            }
            musik::contract::Assert(!this->calls.IsActive(), "Plugin Stop reentered an active call");
            /* users and instances have already stopped. publish completion
            only after shutdown and native handle closure actually finish. */
            this->registrations.clear();
        }
        /* native unload can also execute DLL destructors, so keep the reentry
        guard through handle closure as well as the explicit shutdown hook. */
        PluginCallState::Scope scope(this->calls);
        if (this->needsShutdown) {
            this->needsShutdown = false;
            try {
                this->api.shutdown(this->pluginContext);
            }
            catch (...) {
                /* failed cleanup cannot prove workers stopped. never unload
                their code or continue as if this were a recoverable error. */
                musik::contract::Assert(false, "Plugin shutdown threw; cleanup is not safe");
            }
        }
        this->native->Close();
        std::lock_guard<std::mutex> lock(this->mutex);
        this->phase = Phase::Stopped;
        return MC_OK;
    }

    void MC_CALL Plugin::Log(void* context, mc_log_level level, mc_string_view tag, mc_string_view text) noexcept {
        auto& plugin = *static_cast<Plugin*>(context);
        if (plugin.logger && level <= MC_LOG_ERROR && validText(tag) && validText(text, MC_PLUGIN_DIAGNOSTIC_MAX_BYTES))
        {
            PluginCallState::Scope scope(plugin.calls);
            try {
                plugin.logger(level, tag, text);
            }
            catch (...) {
                /* diagnostics cannot unwind through provider code. */
            }
        }
    }

    mc_result MC_CALL Plugin::CreateError(void*, mc_result code, mc_string_view message, mc_error** error) noexcept {
        if (!error) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        *error = nullptr;
        if (code == MC_OK || !validText(message, MC_PLUGIN_DIAGNOSTIC_MAX_BYTES)) {
            return MC_ERROR_INVALID_ARGUMENT;
        }
        try {
            const std::string text(message.data ? message.data : "", message.length);
            fail(code, text.c_str(), error);
            return *error ? MC_OK : MC_ERROR_INTERNAL;
        }
        catch (...) {
            return MC_ERROR_INTERNAL;
        }
    }

    void MC_CALL Plugin::DestroyError(void*, mc_error** error) noexcept {
        mc_error_destroy(error);
    }

    mc_result MC_CALL Plugin::Register(void* context, const mc_plugin_capability* capability) noexcept {
        auto& plugin = *static_cast<Plugin*>(context);
        if (!plugin.calls.IsOwnerThread()) {
            return MC_ERROR_WRONG_THREAD;
        }
        return exception_barrier(nullptr, [&]() -> mc_result {
            std::lock_guard<std::mutex> lock(plugin.mutex);
            if (plugin.phase != Phase::Initializing) {
                return MC_ERROR_IN_FLIGHT;
            }
            if (!capability || capability->struct_size < sizeof(mc_plugin_capability) || !validId(capability->id) ||
                !capability->api || capability->api_size < sizeof(uint32_t))
            {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            uint32_t declaredSize = 0;
            std::memcpy(&declaredSize, capability->api, sizeof(declaredSize));
            if (declaredSize != capability->api_size) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            const std::string id(capability->id.data, capability->id.length);
            if (!validCapability(id, *capability)) {
                return MC_ERROR_INVALID_ARGUMENT;
            }
            for (const auto& registration : plugin.registrations) {
                if (registration.id == id) {
                    return MC_ERROR_INVALID_ARGUMENT;
                }
            }
            plugin.registrations.push_back({ id, capability->api, capability->api_size, plugin.api.abi_version });
            return MC_OK;
        });
    }

    bool Plugin::IsSameImage(const Plugin& other) const {
        return this->native->IsSame(*other.native);
    }

    bool Plugin::HasId(std::string_view id) const {
        std::lock_guard<std::mutex> lock(this->mutex);
        return !this->metadata.id.empty() && std::string_view(this->metadata.id) == id;
    }

    mc_result Plugin::Identify(mc_error** error) {
        return exception_barrier(error, [&]() -> mc_result {
            const auto owner = this->ValidateCallingThread();
            if (owner != MC_OK) {
                return owner;
            }
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                if (this->phase != Phase::Opened) {
                    return fail(MC_ERROR_IN_FLIGHT, "Plugin identification requires a fresh image", error);
                }
                /* a failed entry is terminal: retrying it could reuse partial
                plugin state without a defined cleanup/reset contract. */
                this->phase = Phase::Failed;
            }
            const auto entry = this->native->Entry();
            if (!entry) {
                return MC_ERROR_UNSUPPORTED;
            }
            mc_error* detail = nullptr;
            const auto status = invokePlugin(this->calls, "Plugin entry threw an exception", &detail, [&] {
                return entry(MC_PLUGIN_ABI_VERSION, &this->host, &this->api, &detail);
            });
            if (status != MC_OK) {
                return transferError(status, detail, error);
            }
            if (detail) {
                mc_error_destroy(&detail);
                return fail(MC_ERROR_PLUGIN, "Plugin returned an error with success", error);
            }
            if (this->api.struct_size != sizeof(mc_plugin_api) || !compatible(this->api.abi_version) ||
                !validId(this->api.id) || !this->api.name.length || !validText(this->api.name) ||
                !validText(this->api.version) || !validText(this->api.author) || !this->api.initialize ||
                !this->api.shutdown)
            {
                return fail(MC_ERROR_PLUGIN, "Invalid or incompatible plugin table", error);
            }

            /* own metadata before initialization can start freeing plugin state.
            readers never need to follow plugin-owned views during teardown. */
            const auto copy = [](mc_string_view text) {
                return text.length ? std::string(text.data, text.length) : std::string();
            };
            auto info = this->Describe(false);
            info.id = copy(this->api.id);
            info.name = copy(this->api.name);
            info.version = copy(this->api.version);
            info.author = copy(this->api.author);
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                this->metadata = std::move(info);
                this->phase = Phase::Identified;
            }
            return MC_OK;
        });
    }

    mc_result Plugin::Initialize(mc_error** error) {
        return exception_barrier(error, [&]() -> mc_result {
            const auto owner = this->ValidateCallingThread();
            if (owner != MC_OK) {
                return owner;
            }
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                if (this->phase != Phase::Identified) {
                    return fail(MC_ERROR_IN_FLIGHT, "Plugin initialization requires successful identification", error);
                }
                this->phase = Phase::Initializing;
                this->needsShutdown = true;
            }
            mc_error* detail = nullptr;
            auto status = invokePlugin(this->calls, "Plugin initialization threw an exception", &detail, [&] {
                return this->api.initialize(&this->host, &this->pluginContext, &detail);
            });
            if (status == MC_OK && detail) {
                mc_error_destroy(&detail);
                status = MC_ERROR_PLUGIN;
            }
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                this->phase = status == MC_OK ? Phase::Running : Phase::Failed;
            }
            return status == MC_OK ? MC_OK : transferError(status, detail, error);
        });
    }

    PluginRegistry::PluginInfo Plugin::Describe(bool enabled) const {
        std::lock_guard<std::mutex> lock(this->mutex);
        auto result = this->metadata;
        result.enabled = enabled;
        return result;
    }

    std::optional<Plugin::CapabilityTable> Plugin::FindCapability(std::string_view capabilityId) const {
        std::lock_guard<std::mutex> lock(this->mutex);
        if (this->phase == Phase::Running) {
            for (const auto& registration : this->registrations) {
                if (std::string_view(registration.id) == capabilityId) {
                    return CapabilityTable { registration.api, registration.size, registration.version };
                }
            }
        }
        return {};
    }

} } }
