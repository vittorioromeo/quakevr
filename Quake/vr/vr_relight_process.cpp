// vr_relight_process.cpp -- the in-game relighting's light process (vr_relight.cpp): ericw-tools' light run with the
// arguments given, below normal priority, without a console window, its output (stdout and stderr) into a log file
// the relighting reads its progress from. On Windows it is put in a job object that ends it when the game's process
// ends (a crash too), so that it never outlives the game. Its own translation unit: <windows.h> stays out of the
// engine's headers.

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

namespace qvr::relight::process
{

namespace
{
#ifdef _WIN32
// The light process and the job object that ends it with the game (main thread).
HANDLE process = nullptr;
HANDLE job = nullptr;

using Wide = za::Vector<wchar_t>;

[[nodiscard]] Wide widen(const za::String& s)
{
    Wide w;
    if(!s.empty())
    {
        const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        w.resize(static_cast<za::SizeT>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    }
    w.pushBack(L'\0');
    return w;
}

// One argument quoted as CommandLineToArgvW (and the C runtime light is built with) reads it back.
void appendQuoted(Wide& line, const Wide& arg)
{
    line.pushBack(L'"');
    za::SizeT slashes = 0;
    for(const wchar_t c : arg)
    {
        if(c == L'\0')
        {
            break;
        }
        if(c == L'\\')
        {
            slashes++;
            continue;
        }
        line.resize(line.size() + (c == L'"' ? slashes * 2 + 1 : slashes), L'\\');
        slashes = 0;
        line.pushBack(c);
    }
    line.resize(line.size() + slashes * 2, L'\\');
    line.pushBack(L'"');
}
#else
pid_t pid = 0;
int status = 0;
#endif
int exitCode = 0;
} // namespace

bool start(const za::String& exe, const za::Vector<za::String>& args, const za::String& cwd, const za::String& log,
    za::String& error)
{
#ifdef _WIN32
    if(process)
    {
        error = "already running";
        return false;
    }
    Wide line;
    appendQuoted(line, widen(exe));
    for(const za::String& a : args)
    {
        line.pushBack(L' ');
        appendQuoted(line, widen(a));
    }
    line.pushBack(L'\0');

    SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const Wide logWide = widen(log);
    HANDLE out = CreateFileW(logWide.data(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &inherit,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if(out == INVALID_HANDLE_VALUE)
    {
        error = "can't write its log (error " + za::toString(static_cast<long long>(GetLastError())) + ")";
        return false;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = nullptr;
    si.hStdOutput = out;
    si.hStdError = out;
    PROCESS_INFORMATION pi{};
    const Wide exeWide = widen(exe);
    const Wide cwdWide = widen(cwd);
    const BOOL made = CreateProcessW(exeWide.data(), line.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW | CREATE_SUSPENDED | BELOW_NORMAL_PRIORITY_CLASS, nullptr, cwd.empty() ? nullptr : cwdWide.data(), &si, &pi);
    const DWORD why = GetLastError();
    CloseHandle(out); // (the process has its own)
    if(!made)
    {
        error = "can't start it (error " + za::toString(static_cast<long long>(why)) + ")";
        return false;
    }
    // Ended with the game, whatever ends the game (a job object closed with its last handle, at the process's end).
    if(!job)
    {
        job = CreateJobObjectW(nullptr, nullptr);
        if(job)
        {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
        }
    }
    if(job)
    {
        AssignProcessToJobObject(job, pi.hProcess); // (if it fails, VR_Shutdown still stops it on a normal quit)
    }
    ResumeThread(pi.hThread);
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
    const int fd = open(log.cStr(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd < 0)
    {
        error = "can't write its log";
        return false;
    }
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, fd, 1);
    posix_spawn_file_actions_adddup2(&actions, fd, 2);
    (void)cwd; // (the paths given are absolute)
    za::Vector<char*> argv{const_cast<char*>(exe.cStr())};
    for(const za::String& a : args)
    {
        argv.pushBack(const_cast<char*>(a.cStr()));
    }
    argv.pushBack(nullptr);
    const int rc = posix_spawn(&pid, exe.cStr(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fd);
    if(rc != 0)
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
    if(job)
    {
        CloseHandle(job);
        job = nullptr;
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

// The executable `name` on PATH ("light.exe"), or empty.
za::String onPath(const char* name)
{
#ifdef _WIN32
    wchar_t found[MAX_PATH * 2];
    const Wide w = widen(za::String{name});
    const DWORD n = SearchPathW(nullptr, w.data(), nullptr, MAX_PATH * 2, found, nullptr);
    if(n == 0 || n >= MAX_PATH * 2)
    {
        return {};
    }
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, found, static_cast<int>(n), nullptr, 0, nullptr, nullptr);
    za::String out(static_cast<za::SizeT>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, 0, found, static_cast<int>(n), out.data(), bytes, nullptr, nullptr);
    return out;
#else
    const char* path = getenv("PATH");
    if(!path)
    {
        return {};
    }
    za::String dir;
    for(const char* p = path;; p++)
    {
        if(*p == ':' || *p == '\0')
        {
            if(!dir.empty())
            {
                za::String full = dir;
                full += "/";
                full += name;
                if(access(full.cStr(), X_OK) == 0)
                {
                    return full;
                }
            }
            dir.clear();
            if(*p == '\0')
            {
                break;
            }
            continue;
        }
        dir.pushBack(*p);
    }
    return {};
#endif
}

} // namespace qvr::relight::process
