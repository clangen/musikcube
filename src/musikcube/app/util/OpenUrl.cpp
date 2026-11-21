#include <app/util/OpenUrl.h>

#ifdef WIN32
#include <musikcore/utfutil.h>
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

void musik::cube::OpenUrl(const std::string& url) {
    if ((!url.starts_with("https://") && !url.starts_with("http://")) ||
        url.find('\0') != std::string::npos)
    {
        return;
    }

#ifdef WIN32
    ShellExecuteW(nullptr, L"open", u8to16(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
#ifdef __APPLE__
    char command[] = "open";
#else
    char command[] = "xdg-open";
#endif
    /* Pass the URL verbatim as one argument to the system opener. */
    char* arguments[] { command, const_cast<char*>(url.c_str()), nullptr };
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        return;
    }

    pid_t child;
    const bool spawned = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null", O_WRONLY, 0) == 0 &&
        posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO) == 0 &&
        posix_spawnp(&child, command, &actions, nullptr, arguments, environ) == 0;
    posix_spawn_file_actions_destroy(&actions);

    if (spawned) {
        while (waitpid(child, nullptr, 0) == -1 && errno == EINTR) {
        }
    }
#endif
}
