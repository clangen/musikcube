#include "legacy-library-selection.h"
#include "playback-binding.h"
#include "request.h"

#include <musikcore/audio/PlaybackService.h>
#include <musikcore/library/MasterLibrary.h>
#include <musikcore/library/LibraryFactory.h>
#include <musikcore/library/RemoteLibrary.h>
#include <musikcore/plugin/Plugins.h>
#include <musikcore/support/Preferences.h>
#include <musikcore/support/PreferenceKeys.h>

namespace musik { namespace core { namespace ffi {
    ILibraryPtr CreateLegacyLibrary() {
        return std::make_shared<library::MasterLibrary>();
    }

    void AttachLegacyLibrary(api::Context& musikcore, const ILibraryPtr& library, audio::PlaybackService& playback) {
        auto* adapter = static_cast<library::MasterLibrary*>(library.get());

        /* Detach the engine and context references before releasing the adapter. */
        std::shared_ptr<library::MasterLibrary> bound(adapter, [library](auto*) {
            auto& selected = static_cast<musik::core::library::MasterLibrary&>(*library);
            selected.GetMessageQueue().UnregisterForBroadcasts(&selected);
            selected.AttachContext(nullptr, nullptr);
        });
        auto& factory = LibraryFactory::Instance();
        attach_library_selector(*musikcore.Get(),
            [bound](mc_library_kind kind) {
                const auto type = kind == MC_LIBRARY_LOCAL ? ILibrary::Type::Local : ILibrary::Type::Remote;
                bound->Select(type);
                Preferences::ForComponent(prefs::components::Settings)
                    ->SetInt(prefs::keys::LibraryType, static_cast<int>(type));
                return MC_OK;
            },
            [&factory] {
                const auto remote = std::static_pointer_cast<library::RemoteLibrary>(factory.DefaultRemoteLibrary());
                remote->Connect();
            },
            *factory.DefaultLocalLibrary()->Indexer());
        adapter->AttachContext(musikcore.Get(), &playback);
        adapter->GetMessageQueue().RegisterForBroadcasts(std::static_pointer_cast<library::MasterLibrary>(library));
    }

    void StartLegacyApp(api::Context& musikcore, runtime::IMessageQueue& queue) {
        LibraryFactory::Initialize(queue);
        const auto library = CreateLegacyLibrary();
        auto playback = std::make_shared<audio::PlaybackService>(queue, library);
        audio::AttachPlayback(musikcore, *playback);
        own_context_playback(*musikcore.Get(), playback);
        AttachLegacyLibrary(musikcore, library, *playback);
        plugin::Start(&queue, playback.get(), library);
    }

    void StopLegacyIndexing() {
        LibraryFactory::Instance().DefaultLocalLibrary()->Indexer()->Shutdown();
    }

    void ShutdownLegacyLibraries() {
        LibraryFactory::Instance().Shutdown();
    }
} } }
