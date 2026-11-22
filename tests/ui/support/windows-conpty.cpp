#include <windows.h>

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
    struct CloseHandleDeleter {
        void operator()(void* handle) const { CloseHandle(handle); }
    };
    using Handle = std::unique_ptr<void, CloseHandleDeleter>;

    bool readBytes(HANDLE pipe, char* data, DWORD size) {
        while (size) {
            DWORD count;
            if (!ReadFile(pipe, data, size, &count, nullptr) || !count) {
                return false;
            }
            data += count;
            size -= count;
        }
        return true;
    }

    bool writeBytes(HANDLE pipe, const char* data, DWORD size) {
        while (size) {
            DWORD count;
            if (!WriteFile(pipe, data, size, &count, nullptr) || !count) {
                return false;
            }
            data += count;
            size -= count;
        }
        return true;
    }

    void receive(HPCON console, HANDLE appInput) {
        auto input = GetStdHandle(STD_INPUT_HANDLE);
        char kind;
        while (readBytes(input, &kind, 1)) {
            unsigned char args[4];
            if (!readBytes(input, reinterpret_cast<char*>(args), sizeof(args))) {
                break;
            }
            const auto first = args[0] | (args[1] << 8);
            const auto second = args[2] | (args[3] << 8);
            if (kind == 'r' && first > 0 && first <= SHRT_MAX && second > 0 && second <= SHRT_MAX) {
                if (FAILED(ResizePseudoConsole(console, { (short) first, (short) second }))) {
                    break;
                }
            }
            else if (kind == 'k') {
                std::vector<char> data(first);
                if (!readBytes(input, data.data(), first) || !writeBytes(appInput, data.data(), first)) {
                    break;
                }
            }
            else {
                break;
            }
        }
    }

    int launch(const wchar_t* executable, COORD size) {
        HANDLE read, write;
        if (!CreatePipe(&read, &write, nullptr, 0)) {
            return 1;
        }
        Handle consoleInput(read), appInput(write);
        if (!CreatePipe(&read, &write, nullptr, 0)) {
            return 1;
        }
        Handle appOutput(read), consoleOutput(write);
        HPCON console;
        if (FAILED(CreatePseudoConsole(size, consoleInput.get(), consoleOutput.get(), 0, &console))) {
            return 1;
        }
        consoleInput.reset();
        consoleOutput.reset();

        /* drain output on a separate thread while the console is active and closing. */
        const auto destination = GetStdHandle(STD_OUTPUT_HANDLE);
        std::thread output([&] {
            char buffer[4096];
            DWORD count;
            bool connected = true;
            while (ReadFile(appOutput.get(), buffer, sizeof(buffer), &count, nullptr) && count) {
                if (connected) {
                    connected = writeBytes(destination, buffer, count);
                }
            }
        });

        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        std::vector<char> storage(bytes);
        STARTUPINFOEXW startup {};
        startup.StartupInfo.cb = sizeof(startup);
        startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());

        /* a job owns the child even when the driver has to kill this launcher. */
        Handle job(CreateJobObjectW(nullptr, nullptr));
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits {};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        PROCESS_INFORMATION info {};
        bool started = false;
        if (InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &bytes)) {
            std::wstring command = std::wstring(L"\"") + executable + L"\"";
            const auto input = GetStdHandle(STD_INPUT_HANDLE);
            const auto errors = GetStdHandle(STD_ERROR_HANDLE);
            /* let the console fill the child's standard handles rather than copying our redirected pipes. */
            SetStdHandle(STD_INPUT_HANDLE, nullptr);
            SetStdHandle(STD_OUTPUT_HANDLE, nullptr);
            SetStdHandle(STD_ERROR_HANDLE, nullptr);
            started = job &&
                SetInformationJobObject(job.get(), JobObjectExtendedLimitInformation, &limits, sizeof(limits)) &&
                UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                    console, sizeof(console), nullptr, nullptr) &&
                CreateProcessW(executable, &command[0], nullptr, nullptr, false,
                    EXTENDED_STARTUPINFO_PRESENT | CREATE_SUSPENDED, nullptr, nullptr, &startup.StartupInfo, &info);
            SetStdHandle(STD_INPUT_HANDLE, input);
            SetStdHandle(STD_OUTPUT_HANDLE, destination);
            SetStdHandle(STD_ERROR_HANDLE, errors);
            DeleteProcThreadAttributeList(startup.lpAttributeList);
        }

        DWORD status = 1;
        if (started) {
            Handle process(info.hProcess), processThread(info.hThread);
            if (AssignProcessToJobObject(job.get(), process.get()) && ResumeThread(processThread.get()) != (DWORD) -1) {
                /* sampling must target the app process rather than the console launcher. */
                FILE* pid;
                if (fopen_s(&pid, ".tui-pid", "w") == 0) {
                    std::fprintf(pid, "%lu\n", info.dwProcessId);
                    std::fclose(pid);
                }
                std::thread input(receive, console, appInput.get());
                HANDLE waits[] = { process.get(), input.native_handle() };
                if (WaitForMultipleObjects(2, waits, false, INFINITE) == WAIT_OBJECT_0) {
                    GetExitCodeProcess(process.get(), &status);
                }
                else {
                    TerminateJobObject(job.get(), 1);
                }

                /* the child has stopped, so interrupt whichever pipe operation remains. */
                while (WaitForSingleObject(input.native_handle(), 10) == WAIT_TIMEOUT) {
                    CancelSynchronousIo(input.native_handle());
                }
                input.join();
            }
            else {
                TerminateProcess(process.get(), 1);
            }
        }
        appInput.reset();
        ClosePseudoConsole(console);
        output.join();
        return (int) status;
    }
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 4) {
        return 1;
    }
    const auto columns = std::wcstol(argv[2], nullptr, 10);
    const auto rows = std::wcstol(argv[3], nullptr, 10);
    if (columns < 1 || columns > SHRT_MAX || rows < 1 || rows > SHRT_MAX) {
        return 1;
    }
    const int result = launch(argv[1], { (short) columns, (short) rows });
    if (result) {
        std::fprintf(stderr, "Windows console launcher exited with status %d\n", result);
    }
    return result;
}
