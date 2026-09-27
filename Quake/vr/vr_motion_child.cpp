// vr_motion_child.cpp -- a second copy of the game, run in the background for the review's Re-evaluate
// (vr_motion_review.cpp): this executable again, with the arguments given, below normal priority, without taking
// the focus (QVR_TEST_BACKGROUND) and without an error dialog (QVR_NO_ERROR_DIALOG). Its own translation unit:
// <windows.h> stays out of the engine's headers.

#include <string>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace qvr::motion::child
{

namespace
{
#ifdef _WIN32
HANDLE process = nullptr;

[[nodiscard]] std::wstring widen(const std::string& s)
{
    if(s.empty())
    {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// One argument quoted as CommandLineToArgvW reads it back.
void appendQuoted(std::wstring& line, const std::wstring& arg)
{
    line += L'"';
    size_t slashes = 0;
    for(const wchar_t c : arg)
    {
        if(c == L'\\')
        {
            slashes++;
            continue;
        }
        if(c == L'"')
        {
            line.append(slashes * 2 + 1, L'\\');
        }
        else
        {
            line.append(slashes, L'\\');
        }
        slashes = 0;
        line += c;
    }
    line.append(slashes * 2, L'\\');
    line += L'"';
}
#else
pid_t pid = 0;
int status = 0;
#endif
int exitCode = 0;
} // namespace

bool start(const std::vector<std::string>& args, std::string& error)
{
#ifdef _WIN32
    if(process)
    {
        error = "already running";
        return false;
    }
    wchar_t exe[MAX_PATH * 2];
    const DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH * 2);
    if(n == 0 || n >= MAX_PATH * 2)
    {
        error = "can't find the game's executable";
        return false;
    }
    std::wstring line;
    appendQuoted(line, exe);
    for(const std::string& a : args)
    {
        line += L' ';
        appendQuoted(line, widen(a));
    }

    // This process's environment, and the two test switches (only the copy's).
    std::wstring env;
    if(wchar_t* block = GetEnvironmentStringsW())
    {
        for(const wchar_t* p = block; *p; p += wcslen(p) + 1)
        {
            env += p;
            env += L'\0';
        }
        FreeEnvironmentStringsW(block);
    }
    env += L"QVR_TEST_BACKGROUND=1";
    env += L'\0';
    env += L"QVR_NO_ERROR_DIALOG=1";
    env += L'\0';
    env += L'\0';

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe, line.data(), nullptr, nullptr, FALSE, BELOW_NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT,
           env.data(), nullptr, &si, &pi))
    {
        error = "can't start it (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(pi.hThread);
    process = pi.hProcess;
    exitCode = 0;
    return true;
#else
    if(pid > 0)
    {
        error = "already running";
        return false;
    }
    char exe[4096];
    const ssize_t n = readlink("/proc/self/exe", exe, sizeof(exe) - 1);
    if(n <= 0)
    {
        error = "can't find the game's executable";
        return false;
    }
    exe[n] = '\0';
    std::vector<std::string> envs;
    for(char** e = environ; *e; e++)
    {
        envs.emplace_back(*e);
    }
    envs.emplace_back("QVR_TEST_BACKGROUND=1");
    envs.emplace_back("QVR_NO_ERROR_DIALOG=1");
    std::vector<char*> argv{exe};
    for(const std::string& a : args)
    {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);
    std::vector<char*> envp;
    for(std::string& e : envs)
    {
        envp.push_back(e.data());
    }
    envp.push_back(nullptr);
    if(posix_spawn(&pid, exe, nullptr, nullptr, argv.data(), envp.data()) != 0)
    {
        pid = 0;
        error = "can't start it";
        return false;
    }
    exitCode = 0;
    return true;
#endif
}

bool running()
{
#ifdef _WIN32
    if(!process)
    {
        return false;
    }
    if(WaitForSingleObject(process, 0) != WAIT_OBJECT_0)
    {
        return true;
    }
    DWORD code = 0;
    GetExitCodeProcess(process, &code);
    exitCode = static_cast<int>(code);
    CloseHandle(process);
    process = nullptr;
    return false;
#else
    if(pid <= 0)
    {
        return false;
    }
    if(waitpid(pid, &status, WNOHANG) == 0)
    {
        return true;
    }
    exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    pid = 0;
    return false;
#endif
}

int lastExitCode()
{
    return exitCode;
}

void stop()
{
#ifdef _WIN32
    if(process)
    {
        TerminateProcess(process, 1);
        WaitForSingleObject(process, 5000);
        CloseHandle(process);
        process = nullptr;
        exitCode = 1;
    }
#else
    if(pid > 0)
    {
        kill(pid, SIGTERM);
        waitpid(pid, &status, 0);
        pid = 0;
        exitCode = 1;
    }
#endif
}

} // namespace qvr::motion::child
