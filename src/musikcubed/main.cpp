#ifdef EV_ERROR
    #undef EV_ERROR
#endif

#include <iostream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#include <sys/file.h>
#include <cstdlib>
#include <csignal>
#include <ctime>
#include <stdexcept>
#include <utility>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <ev++.h>

#include <musikcore/musikcore.hpp>
#include "common/version.h"

namespace api = musik::core::api;

static const char* DEFAULT_LOCKFILE = "/tmp/musikcubed.lock";
static const char* LOCKFILE_OVERRIDE = "MUSIKCUBED_LOCKFILE_OVERRIDE";
static const pid_t NOT_RUNNING = (pid_t) -1;
static bool foreground = false;

static void printHelp();
static void handleCommandLine(int argc, char** argv);
static pid_t getDaemonPid();
static void stopDaemon();

/* Owns the event loop and its watchers. Neither copyable nor movable. */
class DaemonLoop {
    public:
        DaemonLoop() {
            this->wakeup.set<DaemonLoop, &DaemonLoop::Dispatch>(this);
            this->interrupt.set<DaemonLoop, &DaemonLoop::Quit>(this);
            this->terminate.set<DaemonLoop, &DaemonLoop::Quit>(this);
            this->rescan.set<DaemonLoop, &DaemonLoop::Rescan>(this);
            this->wakeup.start();
            this->interrupt.start(SIGINT);
            this->terminate.start(SIGTERM);
            this->rescan.start(SIGUSR1);
        }

        static void MC_CALL Wake(void* data) {
            static_cast<DaemonLoop*>(data)->wakeup.send();
        }

        int Run(api::Context& context) {
            this->context = &context;
            this->wakeup.send();
            this->loop.run();
            this->context = nullptr;
            return this->status == MC_OK ? EXIT_SUCCESS : EXIT_FAILURE;
        }

    private:
        void Dispatch(ev::async&, int) {
            this->status = this->context->Dispatch();
            if (this->status != MC_OK) {
                this->loop.break_loop(ev::ALL);
            }
        }

        void Quit(ev::sig&, int) { this->loop.break_loop(ev::ALL); }

        void Rescan(ev::sig&, int) {
            const auto status = api::Indexer(*this->context).Schedule(MC_INDEXER_SCAN_ALL);
            if (status == MC_OK) {
                api::WriteLog(MC_LOG_INFO, "daemon", "received SIGUSR1, rescanning the library");
            }
            else {
                api::WriteLog(MC_LOG_ERROR, "daemon", "could not schedule a library rescan");
            }
        }

        ev::default_loop loop;
        ev::async wakeup { this->loop };
        ev::sig interrupt { this->loop };
        ev::sig terminate { this->loop };
        ev::sig rescan { this->loop };
        api::Context* context {};
        mc_result status { MC_OK };
};

static void MC_CALL logToConsole(void*, mc_log_level, mc_string_view tag, mc_string_view text) {
    std::cerr << api::AsStringView(tag) << ": " << api::AsStringView(text) << '\n';
}

static void printHelp() {
    std::cout << "\n  musikcubed:\n";
    std::cout << "    --start: start the daemon\n";
    std::cout << "    --foreground: start the in the foreground\n";
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
        else if (command == "--foreground") {
            std::cout << "\n  musikcubed starting in the foreground...\n\n";
            ::foreground = true;
            return;
        }
        else if (command == "--stop") {
            stopDaemon();
        }
        else if (command == "--version") {
            std::cout << "\n  musikcubed version: " << MUSIKCUBE_VERSION << " " << MUSIKCUBE_VERSION_COMMIT_HASH << "\n\n";
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

static std::string getLockfileFn() {
    std::string result = DEFAULT_LOCKFILE;
    const char* userLock = std::getenv(LOCKFILE_OVERRIDE);
    if (userLock && *userLock) {
        result = userLock;
    }
    return result;
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
            if (getDaemonPid() == pid) {
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
    const auto path = getLockfileFn();
    const auto guard = path + ".guard";
    const int fd = open(guard.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (fd == -1) {
        return NOT_RUNNING;
    }

    const bool owned = flock(fd, LOCK_SH | LOCK_NB) == -1 && errno == EWOULDBLOCK;
    close(fd);
    if (!owned) {
        return NOT_RUNNING;
    }

    std::ifstream lock(path);
    if (lock.good()) {
        pid_t pid {};
        if ((lock >> pid) && pid > 0 && kill(pid, 0) == 0) {
            return pid;
        }
    }
    return NOT_RUNNING;
}

/* Owns the daemon guard. Neither copyable nor movable.
   The guard inode stays in place so every starter locks the same file.
   The PID file is published separately once the runtime can accept work. */
class DaemonLock {
    public:
        explicit DaemonLock(std::string path) : path(std::move(path)) {
            const auto guard = this->path + ".guard";
            this->fd = open(guard.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
            if (this->fd == -1) {
                throw std::runtime_error("could not open daemon ownership lock");
            }

            if (flock(this->fd, LOCK_EX | LOCK_NB) == -1) {
                const auto error = errno;
                close(this->fd);
                this->fd = -1;
                if (error != EWOULDBLOCK) {
                    throw std::runtime_error("could not acquire daemon ownership lock");
                }
            }
        else if (unlink(this->path.c_str()) == -1 && errno != ENOENT) {
            close(this->fd);
            throw std::runtime_error("could not clear stale daemon PID file");
        }
        }

        ~DaemonLock() {
            if (this->fd != -1) {
                if (this->published) {
                    unlink(this->path.c_str());
                }
                close(this->fd);
            }
        }

        DaemonLock(const DaemonLock&) = delete;
        DaemonLock& operator=(const DaemonLock&) = delete;

        bool Acquired() const { return this->fd != -1; }

        void Publish() {
            const auto staging = this->path + ".pid";
            const int output = open(staging.c_str(),
                O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
            if (output == -1) {
                throw std::runtime_error("could not open daemon PID file");
            }

            const auto pid = std::to_string(getpid());
            const auto count = write(output, pid.data(), pid.size());
            close(output);
            if (count != static_cast<ssize_t>(pid.size()) ||
                rename(staging.c_str(), this->path.c_str()) == -1)
            {
                unlink(staging.c_str());
                throw std::runtime_error("could not publish daemon PID file");
            }
            this->published = true;
        }

    private:
        const std::string path;
        int fd { -1 };
        bool published {};
};

/* The parent waits for a readiness byte. Any startup failure closes the pipe. */
static int initDaemon() {
    int startup[2];
    if (pipe(startup) == -1) {
        throw std::runtime_error("could not create daemon startup pipe");
    }
    const pid_t pid = fork();
    if (pid == -1) {
        close(startup[0]);
        close(startup[1]);
        throw std::runtime_error("could not fork daemon");
    }

    if (pid > 0) {
        close(startup[1]);
        char ready;
        ssize_t count;
        do {
            count = read(startup[0], &ready, sizeof(ready));
        } while (count == -1 && errno == EINTR);
        close(startup[0]);
        if (count != sizeof(ready)) {
            std::cerr << "musikcubed failed to start\n";
            exit(EXIT_FAILURE);
        }
        exit(EXIT_SUCCESS);
    }

    close(startup[0]);
    umask(0);
    const int device = open("/dev/null", O_RDWR);
    if (setsid() == -1 || chdir("/") == -1 || device == -1 ||
        dup2(device, STDIN_FILENO) == -1 || dup2(device, STDOUT_FILENO) == -1 ||
        dup2(device, STDERR_FILENO) == -1)
    {
        close(startup[1]);
        exit(EXIT_FAILURE);
    }
    if (device > STDERR_FILENO) {
        close(device);
    }
    return startup[1];
}

int main(int argc, char** argv) {
    std::cout << "\n  using lockfile at: " << getLockfileFn();
    handleCommandLine(argc, argv);

    int result = EXIT_FAILURE;
    int startup = -1;
    try {
        DaemonLock lock(std::filesystem::absolute(getLockfileFn()).string());
        if (!lock.Acquired()) {
            std::cerr << "\n  musikcubed is already starting or running\n";
            return foreground || getDaemonPid() != NOT_RUNNING ? EXIT_SUCCESS : EXIT_FAILURE;
        }

        /* Keep ownership across fork while the parent waits for readiness. */
        if (!foreground) {
            startup = initDaemon();
        }
        std::srand(static_cast<unsigned>(std::time(nullptr)));

        DaemonLoop loop;
        mc_context_options options = MC_CONTEXT_OPTIONS_DEFAULT;
        options.flags = MC_CONTEXT_ENABLE_LOGGING | MC_CONTEXT_START_LOCAL;
        options.wakeup = DaemonLoop::Wake;
        options.wakeup_user_data = &loop;
        options.log = foreground ? logToConsole : nullptr;
        api::Context context(options);

        const api::Settings prefs { MC_SETTINGS_APPLICATION };
        if (prefs.GetBool(MC_SETTING_KEY_SYNC_ON_STARTUP, true)) {
            api::Indexer(context).Schedule(MC_INDEXER_SCAN_ALL);
        }

        lock.Publish();
        if (foreground) {
            std::cout << "musikcubed ready" << std::endl;
        }
        else {
            const char ready = 1;
            const auto count = write(startup, &ready, sizeof(ready));
            close(startup);
            startup = -1;
            if (count != sizeof(ready)) {
                throw std::runtime_error("could not notify daemon starter");
            }
        }

        result = loop.Run(context);
        if (context.Close() != MC_OK) {
            result = EXIT_FAILURE;
        }
    }
    catch (const std::exception& error) {
        std::cerr << "musikcubed: " << error.what() << '\n';
    }

    if (startup != -1) {
        close(startup);
    }
    return result;
}
