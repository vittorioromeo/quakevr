// vr_motion_child.cpp -- a second copy of the game, run in the background for the review's Re-evaluate
// (vr_motion_review.cpp): this executable again, with the arguments given, below normal priority, without taking
// the focus (QVR_TEST_BACKGROUND) and without an error dialog (QVR_NO_ERROR_DIALOG). Its own translation unit:
// <windows.h> stays out of the engine's headers.

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"

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

// Wide text for the system's calls (UTF-16, no terminator unless added; Zancle has no wide string).
using Wide = za::Vector<wchar_t>;

void append(Wide& w, const wchar_t* s)
{
    for(; *s; s++)
    {
        w.pushBack(*s);
    }
}

void append(Wide& w, za::SizeT count, wchar_t c)
{
    w.resize(w.size() + count, c);
}

[[nodiscard]] Wide widen(const za::String& s)
{
    Wide w;
    if(s.empty())
    {
        return w;
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    w.resize(static_cast<za::SizeT>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// One argument quoted as CommandLineToArgvW reads it back.
void appendQuoted(Wide& line, const Wide& arg)
{
    line.pushBack(L'"');
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
            append(line, slashes * 2 + 1, L'\\');
        }
        else
        {
            append(line, slashes, L'\\');
        }
        slashes = 0;
        line.pushBack(c);
    }
    append(line, slashes * 2, L'\\');
    line.pushBack(L'"');
}
#else
pid_t pid = 0;
int status = 0;
#endif
int exitCode = 0;
} // namespace

bool start(const za::Vector<za::String>& args, za::String& error)
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
    Wide exeWide;
    append(exeWide, exe);
    Wide line;
    appendQuoted(line, exeWide);
    for(const za::String& a : args)
    {
        line.pushBack(L' ');
        appendQuoted(line, widen(a));
    }
    line.pushBack(L'\0');

    // This process's environment, and the two test switches (only the copy's).
    Wide env;
    if(wchar_t* block = GetEnvironmentStringsW())
    {
        for(const wchar_t* p = block; *p; p += wcslen(p) + 1)
        {
            append(env, p);
            env.pushBack(L'\0');
        }
        FreeEnvironmentStringsW(block);
    }
    append(env, L"QVR_TEST_BACKGROUND=1");
    env.pushBack(L'\0');
    append(env, L"QVR_NO_ERROR_DIALOG=1");
    env.pushBack(L'\0');
    env.pushBack(L'\0');

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe, line.data(), nullptr, nullptr, FALSE, BELOW_NORMAL_PRIORITY_CLASS | CREATE_UNICODE_ENVIRONMENT,
           env.data(), nullptr, &si, &pi))
    {
        error = "can't start it (error " + za::toString(GetLastError()) + ")";
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
    za::Vector<za::String> envs;
    for(char** e = environ; *e; e++)
    {
        envs.emplaceBack(*e);
    }
    envs.emplaceBack("QVR_TEST_BACKGROUND=1");
    envs.emplaceBack("QVR_NO_ERROR_DIALOG=1");
    za::Vector<char*> argv{exe};
    for(const za::String& a : args)
    {
        argv.pushBack(const_cast<char*>(a.cStr()));
    }
    argv.pushBack(nullptr);
    za::Vector<char*> envp;
    for(za::String& e : envs)
    {
        envp.pushBack(e.data());
    }
    envp.pushBack(nullptr);
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
