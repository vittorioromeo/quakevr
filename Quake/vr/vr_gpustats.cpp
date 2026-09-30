// vr_gpustats.cpp -- see vr_gpustats.hpp. A thread samples once a second: NVML (loaded at run time from
// nvml.dll, installed with NVIDIA's driver; absent: those columns stay empty) and the "GPU Engine"
// performance counters (PDH), summed by process and engine type (3D, compute, copy, video encode).
// The log's row averages what was sampled since the previous row. Windows only.

#include "vr_gpustats.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"
#include "vr_zancle.hpp"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <wchar.h>
#pragma comment(lib, "pdh.lib")
#endif

extern "C" void Con_DPrintf(const char* fmt, ...); // console.h (the sampling thread's start and stop, with developer 1)

namespace qvr::gpustats
{
namespace
{

#ifdef _WIN32

// ---- NVML, by hand (no SDK needed) ----------------------------------------------------------------

using NvmlDevice = struct NvmlDeviceOpaque*;
struct NvmlUtilization
{
    unsigned int gpu;
    unsigned int memory;
};

struct Nvml
{
    HMODULE dll{nullptr};
    NvmlDevice device{nullptr};
    int (*getClockInfo)(NvmlDevice, int, unsigned int*){nullptr};
    int (*getTemperature)(NvmlDevice, int, unsigned int*){nullptr};
    int (*getPowerUsage)(NvmlDevice, unsigned int*){nullptr};
    int (*getUtilizationRates)(NvmlDevice, NvmlUtilization*){nullptr};
    int (*getEncoderUtilization)(NvmlDevice, unsigned int*, unsigned int*){nullptr};
    int (*getThrottleReasons)(NvmlDevice, unsigned long long*){nullptr};

    bool open()
    {
        dll = LoadLibraryA("nvml.dll");
        if(!dll)
        {
            return false;
        }
        const auto get = [&](const char* name) { return reinterpret_cast<void*>(GetProcAddress(dll, name)); };
        const auto init = reinterpret_cast<int (*)()>(get("nvmlInit_v2"));
        const auto byIndex = reinterpret_cast<int (*)(unsigned int, NvmlDevice*)>(get("nvmlDeviceGetHandleByIndex_v2"));
        if(!init || !byIndex || init() != 0 || byIndex(0, &device) != 0)
        {
            FreeLibrary(dll);
            dll = nullptr;
            return false;
        }
        getClockInfo = reinterpret_cast<decltype(getClockInfo)>(get("nvmlDeviceGetClockInfo"));
        getTemperature = reinterpret_cast<decltype(getTemperature)>(get("nvmlDeviceGetTemperature"));
        getPowerUsage = reinterpret_cast<decltype(getPowerUsage)>(get("nvmlDeviceGetPowerUsage"));
        getUtilizationRates = reinterpret_cast<decltype(getUtilizationRates)>(get("nvmlDeviceGetUtilizationRates"));
        getEncoderUtilization = reinterpret_cast<decltype(getEncoderUtilization)>(get("nvmlDeviceGetEncoderUtilization"));
        getThrottleReasons = reinterpret_cast<decltype(getThrottleReasons)>(get("nvmlDeviceGetCurrentClocksThrottleReasons"));
        return true;
    }

    void close()
    {
        if(dll)
        {
            if(const auto shutdown = reinterpret_cast<int (*)()>(GetProcAddress(dll, "nvmlShutdown")))
            {
                shutdown();
            }
            FreeLibrary(dll);
            dll = nullptr;
        }
    }
};

// NVML's clock-slowdown reasons worth naming (nvmlClocksThrottleReason*; idle and application
// clocks left out: they are not slowdowns).
constexpr struct
{
    unsigned long long bit;
    const char* name;
} reasonNames[] = {
    {0x4ull, "power_cap"},
    {0x8ull, "hw_slowdown"},
    {0x20ull, "sw_thermal"},
    {0x40ull, "hw_thermal"},
    {0x80ull, "power_brake"},
};

// ---- The samples ----------------------------------------------------------------------------------

struct Engines
{
    double graphics{0.0}; // 3D and compute
    double copy{0.0};
    double encode{0.0};
};

struct Accum
{
    int samples{0};
    double gpuClock{0.0}, memClock{0.0}, temperature{0.0}, powerW{0.0}, gpuUtil{0.0}, encUtil{0.0};
    int nvmlSamples{0};
    unsigned long long reasons{0};
    int slowedSamples{0}; // samples with a power or thermal slowdown
    int engineSamples{0};
    ankerl::unordered_dense::map<DWORD, Engines> byProcess; // summed over the samples
};

za::AtomicMutex mutex;
Accum accum;
za::Thread worker;
za::Atomic<bool> running{false};
HANDLE wake = nullptr; // an event: the worker's sleep between samples, cut short by stop()

ankerl::unordered_dense::map<DWORD, za::String> processNames; // worker thread only

[[nodiscard]] const za::String& processName(DWORD pid)
{
    if(const auto it = processNames.find(pid); it != processNames.end())
    {
        return it->second;
    }
    za::String name = "pid" + za::toString(pid);
    if(HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid))
    {
        char path[MAX_PATH];
        DWORD size = MAX_PATH;
        if(QueryFullProcessImageNameA(h, 0, path, &size))
        {
            const char* base = za::max(strrchr(path, '\\'), strrchr(path, '/'));
            name = base ? base + 1 : path;
        }
        CloseHandle(h);
    }
    for(char& ch : name)
    {
        if(ch == ',' || ch == ' ' || ch == ';' || ch == ':')
        {
            ch = '_'; // the log is comma separated; the list below uses spaces and colons
        }
    }
    return processNames.emplace(pid, ZA_MOVE(name)).first->second;
}

// "pid_1234_luid_0x00000000_0x0000D1B0_phys_0_eng_0_engtype_3D": the process and the engine's type.
[[nodiscard]] bool parseInstance(const wchar_t* name, DWORD& pid, const wchar_t*& type)
{
    if(wcsncmp(name, L"pid_", 4) != 0)
    {
        return false;
    }
    pid = static_cast<DWORD>(wcstoul(name + 4, nullptr, 10));
    const wchar_t* t = wcsstr(name, L"engtype_");
    if(!t)
    {
        return false;
    }
    type = t + 8;
    return true;
}

void sampleEngines(PDH_HQUERY query, PDH_HCOUNTER counter, ankerl::unordered_dense::map<DWORD, Engines>& out)
{
    if(PdhCollectQueryData(query) != ERROR_SUCCESS)
    {
        return;
    }
    DWORD bytes = 0, count = 0;
    if(PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, nullptr) != PDH_MORE_DATA)
    {
        return;
    }
    za::Vector<unsigned char> buffer(bytes);
    auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
    if(PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &bytes, &count, items) != ERROR_SUCCESS)
    {
        return;
    }
    for(DWORD i = 0; i < count; i++)
    {
        DWORD pid;
        const wchar_t* type;
        if(items[i].FmtValue.CStatus != ERROR_SUCCESS || !parseInstance(items[i].szName, pid, type))
        {
            continue;
        }
        const double v = items[i].FmtValue.doubleValue;
        Engines& e = out[pid];
        if(!wcsncmp(type, L"3D", 2) || !wcsncmp(type, L"Compute", 7) || !wcsncmp(type, L"Graphics", 8))
        {
            e.graphics += v;
        }
        else if(!wcsncmp(type, L"Copy", 4))
        {
            e.copy += v;
        }
        else if(!wcsncmp(type, L"VideoEncode", 11))
        {
            e.encode += v;
        }
    }
}

void run()
{
    Nvml nvml;
    const bool haveNvml = nvml.open();

    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    bool havePdh = PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS
        && PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &counter) == ERROR_SUCCESS;
    if(havePdh)
    {
        PdhCollectQueryData(query); // the first of two: utilisation is a rate between collections
    }

    while(running.loadSeqCst())
    {
        // A sample a second; stop() wakes it at once.
        WaitForSingleObject(wake, 1000);
        if(!running.loadSeqCst())
        {
            break;
        }

        ankerl::unordered_dense::map<DWORD, Engines> engines;
        if(havePdh)
        {
            sampleEngines(query, counter, engines);
        }

        unsigned int gpuClock = 0, memClock = 0, temperature = 0, powerMw = 0, enc = 0, period = 0;
        NvmlUtilization util{};
        unsigned long long reasons = 0;
        bool nvmlOk = false;
        if(haveNvml)
        {
            nvmlOk = nvml.getClockInfo && nvml.getClockInfo(nvml.device, 0, &gpuClock) == 0;
            if(nvml.getClockInfo)
            {
                nvml.getClockInfo(nvml.device, 2, &memClock);
            }
            if(nvml.getTemperature)
            {
                nvml.getTemperature(nvml.device, 0, &temperature);
            }
            if(nvml.getPowerUsage)
            {
                nvml.getPowerUsage(nvml.device, &powerMw);
            }
            if(nvml.getUtilizationRates)
            {
                nvml.getUtilizationRates(nvml.device, &util);
            }
            if(nvml.getEncoderUtilization)
            {
                nvml.getEncoderUtilization(nvml.device, &enc, &period);
            }
            if(nvml.getThrottleReasons)
            {
                nvml.getThrottleReasons(nvml.device, &reasons);
            }
        }

        za::LockGuard lock{mutex};
        accum.samples++;
        if(nvmlOk)
        {
            accum.nvmlSamples++;
            accum.gpuClock += gpuClock;
            accum.memClock += memClock;
            accum.temperature += temperature;
            accum.powerW += powerMw / 1000.0;
            accum.gpuUtil += util.gpu;
            accum.encUtil += enc;
            accum.reasons |= reasons;
            for(const auto& r : reasonNames)
            {
                if(reasons & r.bit)
                {
                    accum.slowedSamples++;
                    break;
                }
            }
        }
        if(havePdh && !engines.empty())
        {
            accum.engineSamples++;
            for(const auto& [pid, e] : engines)
            {
                Engines& sum = accum.byProcess[pid];
                sum.graphics += e.graphics;
                sum.copy += e.copy;
                sum.encode += e.encode;
            }
            // Names while the processes live (a name looked up later may be gone).
            for(const auto& [pid, e] : engines)
            {
                if(e.graphics + e.copy + e.encode > 0.5)
                {
                    (void)processName(pid);
                }
            }
        }
    }

    if(query)
    {
        PdhCloseQuery(query);
    }
    nvml.close();
}

#endif // _WIN32

void add(za::Vector<Column>& c, const char* name, const char* fmt, double v, bool valid)
{
    char buf[64] = "";
    if(valid)
    {
        snprintf(buf, sizeof(buf), fmt, v);
    }
    c.pushBack(Column{name, buf});
}

} // namespace

void start()
{
#ifdef _WIN32
    if(!running.exchangeSeqCst(true))
    {
        if(!wake)
        {
            wake = CreateEventW(nullptr, FALSE, FALSE, nullptr); // (auto-reset)
        }
        worker = za::Thread([] { run(); });
        Con_DPrintf("gpustats: sampling thread started\n");
    }
#endif
}

void stop()
{
#ifdef _WIN32
    const bool was = running.exchangeSeqCst(false);
    if(wake)
    {
        SetEvent(wake);
    }
    if(was && worker.joinable())
    {
        worker.join();
        Con_DPrintf("gpustats: sampling thread stopped\n");
    }
#endif
}

void columns(za::Vector<Column>& c)
{
#ifdef _WIN32
    Accum a;
    {
        za::LockGuard lock{mutex};
        a = ZA_MOVE(accum);
        accum = Accum{};
    }
    const double n = a.nvmlSamples;
    const bool nv = a.nvmlSamples > 0;
    add(c, "gpu_clock_mhz", "%.0f", nv ? a.gpuClock / n : 0.0, nv);
    add(c, "gpu_mem_clock_mhz", "%.0f", nv ? a.memClock / n : 0.0, nv);
    add(c, "gpu_temp_c", "%.1f", nv ? a.temperature / n : 0.0, nv);
    add(c, "gpu_power_w", "%.0f", nv ? a.powerW / n : 0.0, nv);
    add(c, "gpu_util_pct", "%.0f", nv ? a.gpuUtil / n : 0.0, nv);
    add(c, "gpu_encoder_pct", "%.0f", nv ? a.encUtil / n : 0.0, nv);
    add(c, "gpu_slowed_pct", "%.0f", nv ? 100.0 * a.slowedSamples / n : 0.0, nv);
    za::String reasons;
    for(const auto& r : reasonNames)
    {
        if(a.reasons & r.bit)
        {
            reasons += (reasons.empty() ? "" : " ") + za::String{r.name};
        }
    }
    c.pushBack(Column{"gpu_slowdown_reasons", reasons});

    // Engine use by process, averaged over the samples: ours, SteamVR's compositor, Virtual Desktop's
    // streamer, and the busiest programs by name.
    const double m = za::max(a.engineSamples, 1);
    const DWORD self = GetCurrentProcessId();
    double selfGfx = 0.0, compositorGfx = 0.0, streamerGfx = 0.0, streamerEnc = 0.0, totalGfx = 0.0, totalEnc = 0.0;
    ankerl::unordered_dense::map<za::String, Engines> byName; // several processes of one program summed
    {
        za::LockGuard lock{mutex}; // processNames is filled by the worker
        for(const auto& [pid, e] : a.byProcess)
        {
            const auto it = processNames.find(pid);
            const za::String name = pid == self ? "quake" : it != processNames.end() ? it->second : "pid" + za::toString(pid);
            Engines& sum = byName[name];
            sum.graphics += e.graphics / m;
            sum.copy += e.copy / m;
            sum.encode += e.encode / m;
        }
    }
    const auto byNameSorted = qza::sortedByKey(byName); // (summed in the names' order, as a std::map had them)
    for(const auto* entry : byNameSorted)
    {
        const auto& [name, e] = *entry;
        totalGfx += e.graphics;
        totalEnc += e.encode;
        if(name == "quake")
        {
            selfGfx += e.graphics;
        }
        else if(_stricmp(name.cStr(), "vrcompositor.exe") == 0)
        {
            compositorGfx += e.graphics;
        }
        else if(_strnicmp(name.cStr(), "VirtualDesktop", 14) == 0)
        {
            streamerGfx += e.graphics;
            streamerEnc += e.encode;
        }
    }
    const bool eng = a.engineSamples > 0;
    add(c, "gpu3d_quake_pct", "%.1f", selfGfx, eng);
    add(c, "gpu3d_vrcompositor_pct", "%.1f", compositorGfx, eng);
    add(c, "gpu3d_virtualdesktop_pct", "%.1f", streamerGfx, eng);
    add(c, "gpuenc_virtualdesktop_pct", "%.1f", streamerEnc, eng);
    add(c, "gpu3d_all_pct", "%.1f", totalGfx, eng);
    add(c, "gpuenc_all_pct", "%.1f", totalEnc, eng);

    za::Vector<const decltype(byName)::value_type*> top(byNameSorted.begin(), byNameSorted.end());
    za::quickSort(top.begin(), top.end(), [](const auto* x, const auto* y) {
        return x->second.graphics + x->second.encode + x->second.copy > y->second.graphics + y->second.encode + y->second.copy;
    });
    za::String list;
    for(za::SizeT i = 0; i < top.size() && i < 6; i++)
    {
        const Engines& e = top[i]->second;
        if(e.graphics + e.encode + e.copy < 0.5)
        {
            break;
        }
        char buf[160];
        snprintf(buf, sizeof(buf), "%s%s:3d%.0f/enc%.0f/copy%.0f", list.empty() ? "" : " ", top[i]->first.cStr(),
            e.graphics, e.encode, e.copy);
        list += buf;
    }
    c.pushBack(Column{"gpu_programs", list});
#else
    (void)c;
#endif
}

} // namespace qvr::gpustats
