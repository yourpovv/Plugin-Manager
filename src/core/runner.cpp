#include "runner.h"
#include "text.h"
#include "win32.h"

runner::RunResult runner::runCaptured(
    const std::wstring& commandLine,
    const std::string& workingFolder,
    const std::function<void(const std::string& line)>& onLine
) {
    RunResult result;
    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readPipe = nullptr;
    HANDLE writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
        result.userMessage = "Couldn't start the Equicord build because a log pipe failed to open.";
        return result;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    HANDLE nullInput = CreateFileW(
        L"NUL",
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security,
        OPEN_EXISTING,
        0,
        nullptr
    );

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    startup.hStdOutput = writePipe;
    startup.hStdError = writePipe;
    startup.hStdInput = nullInput == INVALID_HANDLE_VALUE ? nullptr : nullInput;

    PROCESS_INFORMATION info{};
    std::wstring command = commandLine;
    std::wstring work = wideFromUtf8(workingFolder);

    BOOL created = CreateProcessW(
        nullptr,
        command.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        work.empty() ? nullptr : work.c_str(),
        &startup,
        &info
    );
    CloseHandle(writePipe);
    if (nullInput != INVALID_HANDLE_VALUE)
        CloseHandle(nullInput);

    if (!created) {
        CloseHandle(readPipe);
        result.userMessage =
            "Couldn't start that command. Install Node.js LTS and pnpm, then try again.";
        result.output = "CreateProcess " + std::to_string(GetLastError());
        return result;
    }

    result.started = true;
    std::string pending;
    char buffer[4096];
    DWORD read = 0;
    DWORD startTick = GetTickCount();
    bool timedOut = false;
    while (true) {
        DWORD avail = 0;
        BOOL peekOk = PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr);
        if (peekOk && avail > 0) {
            if (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) {
                pending.append(buffer, read);
                size_t pos;
                while ((pos = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, pos);
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    result.output += line;
                    result.output += "\n";
                    if (onLine)
                        onLine(line);
                    pending.erase(0, pos + 1);
                }
                startTick = GetTickCount();
                continue;
            }
        }
        DWORD wait = WaitForSingleObject(info.hProcess, 50);
        if (wait == WAIT_OBJECT_0) {
            while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
                if (!ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) || read == 0) break;
                pending.append(buffer, read);
                size_t pos;
                while ((pos = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, pos);
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    result.output += line;
                    result.output += "\n";
                    if (onLine)
                        onLine(line);
                    pending.erase(0, pos + 1);
                }
            }
            break;
        }

        if (GetTickCount() - startTick > 300000) {
            timedOut = true;
            break;
        }
        Sleep(10);
    }
    if (!pending.empty()) {
        result.output += pending;
        if (onLine)
            onLine(pending);
    }

    if (timedOut) {
        TerminateProcess(info.hProcess, 1);
        WaitForSingleObject(info.hProcess, 2000);
        result.exitCode = 1;
        result.output += "\n[TIMEOUT] No output for 5 minutes, so the process was killed. Close Discord fully (tray icon, Quit) and try again.";
        result.userMessage = "That step stopped responding and was cancelled. Quit Discord from the tray, then try again.";
    } else {
        GetExitCodeProcess(info.hProcess, &result.exitCode);
    }
    CloseHandle(info.hThread);
    CloseHandle(info.hProcess);
    CloseHandle(readPipe);
    return result;
}
