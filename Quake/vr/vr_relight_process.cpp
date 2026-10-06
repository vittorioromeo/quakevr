// vr_relight_process.cpp -- the in-game relighting's light processes (vr_relight.cpp): ericw-tools' light run with the
// arguments given, below normal priority, without a console window, its output (stdout and stderr) into a log file
// the relighting reads its progress from. Up to maxSlots at once (a batch's maps side by side), each in its slot. On
// Windows each is put in a job object that ends it when the game's process ends (a crash too), so that none outlives
// the game. Its own translation unit: <windows.h> stays out of the engine's headers.

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

constexpr int maxSlots = 8; // (vr_relight.cpp's own copy: vr_relight_parallel's most)

namespace
{
#ifdef _WIN32
// The light processes, a slot each, and the job object that ends them with the game (main thread).
HANDLE process[maxSlots]{};
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
pid_t pid[maxSlots]{};
int status = 0;
#endif
int exitCode[maxSlots]{};

[[nodiscard]] bool validSlot(int slot)
{
    return slot >= 0 && slot < maxSlots;
}
} // namespace

bool start(int slot, const za::String& exe, const za::Vector<za::String>& args, const za::String& cwd,
    const za::String& log, za::String& error)
{
    if(!validSlot(slot))
    {
        error = "no such slot";
        return false;
    }
#ifdef _WIN32
    if(process[slot])
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
    process[slot] = pi.hProcess;
    exitCode[slot] = 0;
    return true;
#else
    if(pid[slot] > 0)
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
    const int rc = posix_spawn(&pid[slot], exe.cStr(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(fd);
    if(rc != 0)
    {
        pid[slot] = 0;
        error = "can't start it";
        return false;
    }
    exitCode[slot] = 0;
    return true;
#endif
}

bool running(int slot)
{
    if(!validSlot(slot))
    {
        return false;
    }
#ifdef _WIN32
    if(!process[slot])
    {
        return false;
    }
    if(WaitForSingleObject(process[slot], 0) != WAIT_OBJECT_0)
    {
        return true;
    }
    DWORD code = 0;
    GetExitCodeProcess(process[slot], &code);
    exitCode[slot] = static_cast<int>(code);
    CloseHandle(process[slot]);
    process[slot] = nullptr;
    return false;
#else
    if(pid[slot] <= 0)
    {
        return false;
    }
    if(waitpid(pid[slot], &status, WNOHANG) == 0)
    {
        return true;
    }
    exitCode[slot] = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    pid[slot] = 0;
    return false;
#endif
}

int lastExitCode(int slot)
{
    return validSlot(slot) ? exitCode[slot] : -1;
}

int processId(int slot)
{
#ifdef _WIN32
    return validSlot(slot) && process[slot] ? static_cast<int>(GetProcessId(process[slot])) : 0;
#else
    return validSlot(slot) ? static_cast<int>(pid[slot]) : 0;
#endif
}

// Every slot's process ended (all told to end first, then each waited for: a batch's stop is as quick as one), and,
// with `closeJob`, the job object closed (the game is quitting).
void stopAll(bool closeJob)
{
#ifdef _WIN32
    for(HANDLE p : process)
    {
        if(p)
        {
            TerminateProcess(p, 1);
        }
    }
    for(int i = 0; i < maxSlots; i++)
    {
        if(process[i])
        {
            WaitForSingleObject(process[i], 5000);
            CloseHandle(process[i]);
            process[i] = nullptr;
            exitCode[i] = 1;
        }
    }
    if(closeJob && job)
    {
        CloseHandle(job);
        job = nullptr;
    }
#else
    (void)closeJob;
    for(const pid_t p : pid)
    {
        if(p > 0)
        {
            kill(p, SIGTERM);
        }
    }
    for(int i = 0; i < maxSlots; i++)
    {
        if(pid[i] > 0)
        {
            waitpid(pid[i], &status, 0);
            pid[i] = 0;
            exitCode[i] = 1;
        }
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
