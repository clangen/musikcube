#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <core/audio/PlaybackService.h>
#include <core/audio/MasterTransport.h>
#include <core/debug.h>
#include <core/library/LibraryFactory.h>
#include <core/plugin/Plugins.h>
#include <core/runtime/MessageQueue.h>
#include <core/runtime/Message.h>
#include <core/support/PreferenceKeys.h>
#include <core/support/Common.h>

#include <boost/locale.hpp>
#include <boost/filesystem/detail/utf8_codecvt_facet.hpp>

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::runtime;

#ifdef __linux__
#define LOCKFILE "/var/lock/musikcubed.lock"
#else
#define LOCKFILE "/tmp/musikcubed.lock"
#endif

static MessageQueue messageQueue;
static volatile bool quit = false;

static void sigtermHandler(int signal) {
    quit = true;

    /* pump a dummy message in the queue so it wakes up
    immediately and goes back to the top of the loop, where
    the quit flag will be processed */
    messageQueue.Broadcast(Message::Create(nullptr, 0));
}

static bool exitIfRunning() {
    std::ifstream lock(LOCKFILE);
    if (lock.good()) {
        int pid;
        lock >> pid;
        if (kill((pid_t) pid, 0) == 0) {
            std::cerr << "musikcubed is already running!\n";
            exit(EXIT_SUCCESS);
            return true;
        }
    }
    std::cerr << "musikcubed is starting...\n";
    return false;
}

static void startDaemon() {
    pid_t pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    pid_t sid = setsid();
    if (sid < 0) {
        exit(EXIT_SUCCESS);
    }

    if (chdir("/") < 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    freopen("/tmp/musikcube.log", "w", stderr);

    std::ofstream lock(LOCKFILE);
    if (lock.good()) {
        lock << std::to_string((int) getpid());
    }
}

int main() {
    exitIfRunning();
    startDaemon();

    srand((unsigned int) time(0));

    std::signal(SIGTERM, sigtermHandler);

    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);

    debug::init();

    MasterTransport transport;
    auto library = LibraryFactory::Libraries().at(0);
    auto prefs = Preferences::ForComponent(prefs::components::Settings);

    library->SetMessageQueue(messageQueue);
    PlaybackService playback(messageQueue, library, transport);

    plugin::InstallDependencies(&messageQueue, &playback, library);

    if (prefs->GetBool(prefs::keys::SyncOnStartup, true)) {
        library->Indexer()->Schedule(IIndexer::SyncType::All);
    }

    while (!quit) {
        messageQueue.WaitAndDispatch();
    }

    remove(LOCKFILE);
}
