/* macOS script(1) requires terminal stdin. This host also accepts Node pipes. */
#include <util.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

static int write_all(int descriptor, const char* bytes, ssize_t count) {
    while (count > 0) {
        const ssize_t written = write(descriptor, bytes, (size_t)count);
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) return 0;
        bytes += written;
        count -= written;
    }
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    int master = -1;
    const pid_t child = forkpty(&master, NULL, NULL, NULL);
    if (child < 0) { perror("forkpty"); return 1; }
    if (child == 0) {
        execvp(argv[1], argv + 1);
        perror("execvp");
        _exit(127);
    }
    signal(SIGPIPE, SIG_IGN);
    struct pollfd descriptors[] = {{STDIN_FILENO, POLLIN, 0}, {master, POLLIN, 0}};
    char bytes[4096];
    int running = 1;
    while (running) {
        const int ready = poll(descriptors, 2, -1);
        if (ready < 0 && errno == EINTR) continue;
        if (ready < 0) break;
        for (int i = 0; i < 2; ++i) {
            if (!(descriptors[i].revents & (POLLIN | POLLHUP | POLLERR))) continue;
            const ssize_t count = read(descriptors[i].fd, bytes, sizeof(bytes));
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) {
                if (i == 0) descriptors[i].fd = -1;
                else running = 0;
            }
            else if (!write_all(i == 0 ? master : STDOUT_FILENO, bytes, count)) running = 0;
        }
    }
    close(master);
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
