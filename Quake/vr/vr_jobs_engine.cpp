// vr_jobs_engine.cpp -- the game's thread pool (vr_jobs.hpp) on the engine's side: made at VR_Init, joined at
// VR_Shutdown; vr_jobs_threads, vr_jobs_parallel, vr_jobs_info, vr_jobs_sites (each parallelFor site's counters),
// vr_jobs_bench (a small parallelFor split against the caller alone) and vr_jobs_test (its self-test: pools made and
// joined, every index covered once, the calling thread taking part, every worker busy, nested waits, results
// independent of the thread count, a pool destroyed with work queued, the game's pool: one line per check, then the
// totals).

#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_jobs.hpp"

#include "Zancle/Algorithm/Count.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"

namespace qvr::jobs
{

namespace
{

// vr_jobs_threads: the pool made again with that many workers (0: the hardware's threads less one), whatever is
// queued run first.
void onThreads(cvar_t* var)
{
    if(pool())
    {
        restart(za::clamp(static_cast<int>(var->value), 0, 64));
        Con_Printf("vr_jobs: %d workers\n", workers());
    }
}

void onParallel(cvar_t* var)
{
    setParallel(var->value != 0.f);
}

} // namespace

// -jobs <n>: the pool's workers from the start (vr_jobs_threads is read after the pool is made).
namespace
{
za::ThreadId mainThread; // (start's: VR_Init runs on the main thread)
} // namespace

void start()
{
    mainThread = za::ThisThread::getId();
    const int i = COM_CheckParm("-jobs");
    init(i && i + 1 < com_argc ? za::clamp(Q_atoi(com_argv[i + 1]), 0, 64) : 0);
}

void registerCommands()
{
    Cvar_SetCallback(&vr_jobs_threads, onThreads);
    Cvar_SetCallback(&vr_jobs_parallel, onParallel);
    setParallel(vr_jobs_parallel.value != 0.f);
    registerDiagCommands(); // vr_jobs_info, vr_jobs_test, vr_jobs_sites, vr_jobs_bench (vr_jobs_diag.cpp)
}

} // namespace qvr::jobs

extern "C" int VR_OnMainThread(void)
{
    // (before VR_Init, only the main thread runs)
    return qvr::jobs::mainThread == za::ThreadId{} || za::ThisThread::getId() == qvr::jobs::mainThread;
}
