#include <iostream>
#include <fstream>
#include <csignal>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ev++.h>

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

#define MESSAGE_QUEUE_TIMEOUT_MS 500
#define LOCKFILE "/tmp/musikcubed.lock"

static const short EVENT_DISPATCH = 1;
static const short EVENT_QUIT = 2;

static void sigtermHandler(ev::sig &signal, int revents);

class EvMessageQueue: public MessageQueue {
    public:
        void Post(IMessagePtr message, int64_t delayMs) {
            MessageQueue::Post(message, delayMs);

            if (delayMs <= 0) {
                write(fd[1], &EVENT_DISPATCH, sizeof(EVENT_DISPATCH));
            }
            else {
                double delayTs = (double) delayMs / 1000.0;
                loop.once<
                    EvMessageQueue,
                    &EvMessageQueue::DelayedDispatch
                >(-1, ev::TIMER, (ev::tstamp) delayTs, this);
            }
        }

        void DelayedDispatch(int revents) {
            this->Dispatch();
        }

        void ReadCallback(ev::io& watcher, int revents) {
            short type;
            if (read(this->fd[0], &type, sizeof(type)) == 0) {
                std::cerr << "read() failed.\n";
                exit(EXIT_FAILURE);
            }
            switch (type) {
                case EVENT_DISPATCH: this->Dispatch(); break;
                case EVENT_QUIT: loop.break_loop(ev::ALL); break;
            }
        }

        void Run() {
            if (pipe(this->fd) != 0) {
                std::cerr << "couldn't create pipe\n";
                exit(EXIT_FAILURE);
            }

            io.set(loop);
            io.set(this->fd[0], ev::READ);
            io.set<EvMessageQueue, &EvMessageQueue::ReadCallback>(this);
            io.start();

            sio.set(loop);
            sio.set<&sigtermHandler>();
            sio.start(SIGTERM);

            write(fd[1], &EVENT_DISPATCH, sizeof(EVENT_DISPATCH));

            loop.run(0);
        }

        void Quit() {
            write(fd[1], &EVENT_QUIT, sizeof(EVENT_QUIT));
        }

    private:
        int fd[2];
        ev::dynamic_loop loop;
        ev::io io;
        ev::sig sio;
} messageQueue;

static void sigtermHandler(ev::sig &signal, int revents) {
    messageQueue.Quit();
}

static void exitIfRunning() {
    std::ifstream lock(LOCKFILE);
    if (lock.good()) {
        int pid;
        lock >> pid;
        if (kill((pid_t) pid, 0) == 0) {
            std::cerr << "musikcubed is already running!\n";
            exit(EXIT_SUCCESS);
        }
    }
    std::cerr << "musikcubed is starting...\n";
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

    messageQueue.Run();

    remove(LOCKFILE);
}
