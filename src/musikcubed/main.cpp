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

#include "../musikcube/app/version.h"

using namespace musik;
using namespace musik::core;
using namespace musik::core::audio;
using namespace musik::core::runtime;

static const char* LOCKFILE = "/tmp/musikcubed.lock";
static const short EVENT_DISPATCH = 1;
static const short EVENT_QUIT = 2;
static const pid_t NOT_RUNNING = (pid_t) -1;
static int pipeFd[2] = { 0 };

static void printHelp();
static void handleCommandLine(int argc, char** argv);
static void exitIfRunning();
static pid_t getDaemonPid();
static void startDaemon();
static void stopDaemon();
static void initUtf8();

class EvMessageQueue: public MessageQueue {
    public:
        void Post(IMessagePtr message, int64_t delayMs) {
            MessageQueue::Post(message, delayMs);

            if (delayMs <= 0) {
                write(pipeFd[1], &EVENT_DISPATCH, sizeof(EVENT_DISPATCH));
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

        static void SignalCallback(ev::sig& signal, int revents) {
            write(pipeFd[1], &EVENT_QUIT, sizeof(EVENT_QUIT));
        }

        void ReadCallback(ev::io& watcher, int revents) {
            short type;
            if (read(pipeFd[0], &type, sizeof(type)) == 0) {
                std::cerr << "read() failed.\n";
                exit(EXIT_FAILURE);
            }
            switch (type) {
                case EVENT_DISPATCH: this->Dispatch(); break;
                case EVENT_QUIT: loop.break_loop(ev::ALL); break;
            }
        }

        void Run() {
            io.set(loop);
            io.set(pipeFd[0], ev::READ);
            io.set<EvMessageQueue, &EvMessageQueue::ReadCallback>(this);
            io.start();

            sio.set(loop);
            sio.set<&EvMessageQueue::SignalCallback>();
            sio.start(SIGTERM);

            write(pipeFd[1], &EVENT_DISPATCH, sizeof(EVENT_DISPATCH));

            loop.run(0);
        }

        void Quit() {
        }

    private:
        ev::dynamic_loop loop;
        ev::io io;
        ev::sig sio;
};

static void printHelp() {
    std::cout << "\n  musikcubed:\n";
    std::cout << "    --start: start the daemon\n";
    std::cout << "    --stop: shut down the daemon\n";
    std::cout << "    --running: check if the daemon is running\n";
    std::cout << "    --version: print the version\n";
    std::cout << "    --help: show this message\n\n";
}

static void handleCommandLine(int argc, char** argv) {
    if (argc >= 2) {
        const std::string command = std::string(argv[1]);
        if (command == "--start") {
            return;
        }
        else if (command == "--stop") {
            stopDaemon();
        }
        else if (command == "--version") {
            std::cout << "\n  musikcubed version: " << VERSION << "\n\n";
        }
        else if (command == "--running") {
            pid_t pid = getDaemonPid();
            if (pid == NOT_RUNNING) {
                std::cout << "\n  musikcubed is NOT running\n\n";
            }
            else {
                std::cout << "\n  musikcubed is running with pid " << pid << "\n\n";
            }
        }
        else {
            printHelp();
        }
        exit(EXIT_SUCCESS);
    }
}

static void stopDaemon() {
    pid_t pid = getDaemonPid();
    if (pid == NOT_RUNNING) {
        std::cout << "\n  musikcubed is not running\n\n";
    }
    else {
        std::cout << "\n  stopping musikcubed...";
        kill(pid, SIGTERM);
        int count = 0;
        bool dead = false;
        while (!dead && count++ < 7) { /* try for 7 seconds */
            if (kill(pid, 0) == 0) {
                std::cout << ".";
                std::cout.flush();
                usleep(500000);
            }
            else {
                dead = true;
            }
        }
        std::cout << (dead ? " success" : " failed") << "\n\n";
        if (!dead) {
            exit(EXIT_FAILURE);
        }
    }
}

static pid_t getDaemonPid() {
    std::ifstream lock(LOCKFILE);
    if (lock.good()) {
        int pid;
        lock >> pid;
        if (kill((pid_t) pid, 0) == 0) {
            return pid;
        }
    }
    return NOT_RUNNING;
}

static void exitIfRunning() {
    if (getDaemonPid() != NOT_RUNNING) {
        std::cerr << "\n musikcubed is already running!\n\n";
        exit(EXIT_SUCCESS);
    }
    std::cerr << "\n  musikcubed is starting...\n\n";
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

    if (pipe(pipeFd) != 0) {
        std::cerr << "\n  ERROR! couldn't create pipe\n\n";
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

static void initUtf8() {
    std::locale locale = std::locale();
    std::locale utf8Locale(locale, new boost::filesystem::detail::utf8_codecvt_facet);
    boost::filesystem::path::imbue(utf8Locale);
}

int main(int argc, char** argv) {
    handleCommandLine(argc, argv);
    exitIfRunning();
    startDaemon();
    initUtf8();

    srand((unsigned int) time(0));

    debug::init();

    EvMessageQueue messageQueue;
    auto library = LibraryFactory::Default();
    library->SetMessageQueue(messageQueue);

    {
        PlaybackService playback(messageQueue, library);

        plugin::InstallDependencies(&messageQueue, &playback, library);

        auto prefs = Preferences::ForComponent(prefs::components::Settings);
        if (prefs->GetBool(prefs::keys::SyncOnStartup, true)) {
            library->Indexer()->Schedule(IIndexer::SyncType::All);
        }

        messageQueue.Run();

        library->Indexer()->Stop();
    }

    remove(LOCKFILE);
}
