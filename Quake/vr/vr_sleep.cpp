// vr_sleep.cpp -- the frame cap's sleep on a high-resolution waitable timer (Windows 10 1803 and later; main_sdl.c's
// Sys_WaitUntil calls VR_HiResSleepUntil). It keeps its precision (tenths of a millisecond) whatever the system
// timer's resolution: SDL_Delay (Sleep) wakes at the system timer's ticks, and Windows 11 ignores a process's
// timeBeginPeriod (which SDL makes, 1 ms) while its window is hidden or covered (a headset player's desktop window, a
// test run in the background): then each 1 ms sleep took 15.6 ms and a 90 fps cap ran at 64. One sleep up to the
// timer's expected lateness before the end (learnt as Sys_WaitUntil learns SDL_Delay's: Welford's mean and variance),
// then Sys_WaitUntil's short spin. Its own translation unit: <windows.h> stays out of the engine's headers.

#include <algorithm>
#include <cmath>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
#endif

extern "C" double Sys_DoubleTime(void);

namespace
{

#ifdef _WIN32
// The timer (made on the first sleep; none before Windows 10 1803) and its lateness (the main thread's).
struct HiResTimer
{
    HANDLE timer = nullptr;
    bool tried = false;
    double estimate = 2e-4; // the lateness allowed for: the mean + 1.5 standard deviations
    double mean = 2e-4;
    double m2 = 0.0;
    double count = 1.0;
};
HiResTimer hr;

// Sleeps about `seconds` on the timer; false without one.
bool hiResSleep(double seconds)
{
    if(!hr.tried)
    {
        hr.tried = true;
        hr.timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    }
    if(!hr.timer)
    {
        return false;
    }
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(seconds * 1e7); // relative, in 100 ns units
    if(due.QuadPart >= 0)
    {
        return true;
    }
    if(!SetWaitableTimerEx(hr.timer, &due, 0, nullptr, nullptr, nullptr, 0))
    {
        return false;
    }
    WaitForSingleObject(hr.timer, INFINITE);
    return true;
}
#endif

} // namespace

// Sys_WaitUntil: sleeps on the high-resolution timer until its expected lateness before `endtime` (*now: the time, on
// entry and after); 0 without that timer (then Sys_WaitUntil's 1 ms sleeps of SDL_Delay).
extern "C" int VR_HiResSleepUntil(double endtime, double* now)
{
#ifdef _WIN32
    if(!hiResSleep(0.0)) // the timer exists
    {
        return 0;
    }
    while(*now + hr.estimate < endtime)
    {
        const double before = *now;
        const double asked = endtime - *now - hr.estimate;
        if(!hiResSleep(asked))
        {
            break;
        }
        *now = Sys_DoubleTime();

        if(hr.count < 1e6)
        {
            ++hr.count;
            const double observed = std::max(0.0, *now - before - asked);
            const double delta = observed - hr.mean;
            hr.mean += delta / hr.count;
            hr.m2 += delta * (observed - hr.mean);
            const double stddev = std::sqrt(hr.m2 / (hr.count - 1.0));
            hr.estimate = std::clamp(hr.mean + 1.5 * stddev, 5e-5, 2e-3);
        }
    }
    return 1;
#else
    (void)endtime;
    (void)now;
    return 0;
#endif
}
