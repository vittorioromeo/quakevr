// vr_jobs_engine.cpp -- the game's thread pool (vr_jobs.hpp) on the engine's side: made at VR_Init, joined at
// VR_Shutdown; vr_jobs_threads, vr_jobs_parallel, vr_jobs_info and vr_jobs_test (its self-test: pools made and joined,
// every index covered once, the calling thread taking part, every worker busy, nested waits, results
// independent of the thread count, a pool destroyed with work queued, the game's pool: one line per check, then the
// totals).

#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_jobs.hpp"

#include "Zancle/Algorithm/Count.hpp"
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

// A task that is only posted (nobody waits for it): the pool's destructor must run it.
template <typename F>
void postOnly(Pool& p, F&& f)
{
    p.post(detail::JobPtr<detail::Job>{new detail::JobOf<void, ZA_DECAY(F)>(ZA_FORWARD(f))});
}

void sleepMs(int ms)
{
    za::ThisThread::sleepFor(za::milliseconds(ms));
}

// How many different threads ran the items (and whether `self` was one).
[[nodiscard]] za::SizeT distinctThreads(const za::Vector<za::ThreadId>& by)
{
    ankerl::unordered_dense::set<za::U64> ids;
    for(const za::ThreadId id : by)
    {
        ids.insert(id.value());
    }
    return ids.size();
}

} // namespace

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
    Cmd_AddCommand("vr_jobs_info", info_f);
    Cmd_AddCommand("vr_jobs_test", test_f);
}

void info_f()
{
    const Stats s = stats();
    Con_Printf("vr_jobs: %d workers (%d hardware threads), parallelFor %s\n", workers(), hardwareThreads(),
        parallel() ? "split between threads" : "on the caller alone (vr_jobs_parallel 0)");
    Con_Printf("  async tasks %zu (%zu run by their waiter); parallelFor %zu split, %zu on the caller alone\n", s.tasks,
        s.claimedByWaiter, s.loops, s.serialLoops);
    Con_Printf("  chunks: %zu by callers, %zu by helpers; %zu helpers called off (the caller done first)\n", s.chunksCaller,
        s.chunksHelpers, s.helpersCalledOff);
}

void test_f()
{
    int passed = 0, failed = 0;
    const auto check = [&](bool ok, const za::String& what) {
        Con_Printf("vr_jobs_test: %s %s\n", ok ? "ok  " : "FAIL", what.cStr());
        (ok ? passed : failed)++;
    };
    const za::ThreadId self = za::ThisThread::getId();
    const za::Clock clock;

    // Start and shutdown: every posted task run before the destructor returns, by more than one worker.
    for(const int n : {1, 2, 3, 8})
    {
        za::Atomic<int> ran{0};
        za::Vector<za::ThreadId> by(64);
        {
            Pool p{n};
            for(int i = 0; i < 64; i++)
            {
                postOnly(p, [&ran, &by, i] {
                    sleepMs(1);
                    by[static_cast<za::SizeT>(i)] = za::ThisThread::getId();
                    ran.fetchAddSeqCst(1);
                });
            }
        }
        const za::SizeT threads = distinctThreads(by);
        check(ran.loadSeqCst() == 64 && (n == 1 ? threads == 1 : threads >= 2) && za::count(by.begin(), by.end(), self) == 0,
            va("%d workers: 64 posted tasks all run by the join, on %d worker threads", n, static_cast<int>(threads)));
    }
    {
        bool ok = true;
        for(int i = 0; i < 50; i++)
        {
            za::Atomic<int> ran{0};
            {
                Pool p{1 + i % 4};
                for(int k = 0; k < i; k++)
                {
                    postOnly(p, [&ran] { ran.fetchAddSeqCst(1); });
                }
            }
            ok = ok && ran.loadSeqCst() == i;
        }
        check(ok, "50 pools made and destroyed at once, with 0..49 tasks queued: every task run");
    }

    Pool pool{4};

    // Every index covered exactly once, whatever the count and the chunk.
    {
        bool ok = true;
        for(const za::SizeT count : {za::SizeT{0}, za::SizeT{1}, za::SizeT{2}, za::SizeT{7}, za::SizeT{100},
                za::SizeT{1000}, za::SizeT{100003}})
        {
            for(const za::SizeT chunk : {za::SizeT{0}, za::SizeT{1}, za::SizeT{3}, za::SizeT{64}, za::SizeT{1000000}})
            {
                za::Vector<za::Atomic<unsigned char>> seen(count + 1); // (each 0)
                za::Atomic<bool> bad{false};
                pool.parallelFor(count, chunk, [&](za::SizeT b, za::SizeT e) {
                    if(b >= e || e > count)
                    {
                        bad.storeSeqCst(true);
                    }
                    for(za::SizeT i = b; i < e; i++)
                    {
                        seen[i].fetchAddSeqCst(1);
                    }
                });
                for(za::SizeT i = 0; i < count; i++)
                {
                    ok = ok && seen[i].loadSeqCst() == 1;
                }
                ok = ok && !bad.loadSeqCst() && seen[count].loadSeqCst() == 0;
            }
        }
        check(ok, "parallelFor: counts 0..100003, chunks auto..1000000: every index once, every range in bounds");
    }

    // The calling thread takes part, and so do the workers.
    {
        za::Vector<za::ThreadId> by(64);
        pool.parallelFor(64, 1, [&](za::SizeT b, za::SizeT e) {
            for(za::SizeT i = b; i < e; i++)
            {
                sleepMs(1);
                by[i] = za::ThisThread::getId();
            }
        });
        const za::SizeT mine = static_cast<za::SizeT>(za::count(by.begin(), by.end(), self));
        const za::SizeT threads = distinctThreads(by);
        check(mine > 0 && mine < 64 && threads >= 3,
            va("the calling thread ran %d of 64 chunks, %d threads in all", static_cast<int>(mine), static_cast<int>(threads)));
    }

    // Every worker busy (blocked): parallelFor still returns, the caller having run every chunk; its helpers, run
    // later, are called off.
    {
        za::Atomic<int> gate{0};
        za::Atomic<int> blocked{0};
        for(int i = 0; i < pool.workers(); i++)
        {
            postOnly(pool, [&] {
                blocked.fetchAddSeqCst(1);
                while(gate.loadSeqCst() == 0)
                {
                    gate.waitOnceSeqCst(0);
                }
            });
        }
        while(blocked.loadSeqCst() < pool.workers())
        {
            za::ThisThread::yield();
        }
        const Stats before = stats();
        za::Atomic<int> mine{0}, total{0};
        pool.parallelFor(1000, 10, [&](za::SizeT b, za::SizeT e) {
            total.fetchAddSeqCst(static_cast<int>(e - b));
            if(za::ThisThread::getId() == self)
            {
                mine.fetchAddSeqCst(static_cast<int>(e - b));
            }
        });
        const bool returned = total.loadSeqCst() == 1000 && mine.loadSeqCst() == 1000;
        gate.storeSeqCst(1);
        gate.notifyAll();
        Stats after = stats();
        for(int i = 0; i < 1000 && after.helpersCalledOff < before.helpersCalledOff + static_cast<za::SizeT>(pool.workers()); i++)
        {
            sleepMs(1); // (the workers free again: the helpers queued behind the blocking tasks run now)
            after = stats();
        }
        check(returned && after.helpersCalledOff >= before.helpersCalledOff + static_cast<za::SizeT>(pool.workers()),
            va("every worker blocked: the caller ran all 1000 items; %d helpers called off once freed",
                static_cast<int>(after.helpersCalledOff - before.helpersCalledOff)));
    }

    // Nested waits: parallelFor in parallelFor (three deep), async waited for inside chunks and tasks, every worker
    // waiting for a task queued behind it (the waiter runs it).
    {
        za::Atomic<long long> sum{0};
        pool.parallelFor(8, 1, [&](za::SizeT, za::SizeT) {
            pool.parallelFor(8, 1, [&](za::SizeT, za::SizeT) {
                Future<int> inner = pool.async([&] {
                    int s = 0;
                    pool.parallelFor(10, 1, [&](za::SizeT b, za::SizeT e) { sum.fetchAddSeqCst(static_cast<long long>(e - b)); });
                    return s + 1;
                });
                sum.fetchAddSeqCst(inner.get());
            });
        });
        check(sum.loadSeqCst() == 8 * 8 * 11, va("nested parallelFor x3 with async waits inside: %lld of 704", sum.loadSeqCst()));
    }
    {
        Pool two{2};
        const Stats before = stats();
        za::Atomic<int> started{0};
        za::Vector<Future<int>> outer;
        for(int i = 0; i < 2; i++)
        {
            outer.pushBack(two.async([&two, &started, i] {
                started.fetchAddSeqCst(1);
                while(started.loadSeqCst() < 2)
                {
                    za::ThisThread::yield(); // both workers taken
                }
                Future<int> inner = two.async([i] { return 10 + i; }); // queued: no worker free
                return inner.get();
            }));
        }
        const int a = outer[0].get(), b = outer[1].get();
        const Stats s = stats();
        check(a == 10 && b == 11 && s.claimedByWaiter >= before.claimedByWaiter + 2, "every worker waiting for a task queued behind it: each ran its own");
    }

    // The same results whatever the thread count (partial sums per item, reduced in order).
    {
        const auto run = [](Pool* p, bool par) {
            za::Vector<float> part(997);
            const auto body = [&](za::SizeT b, za::SizeT e) {
                for(za::SizeT i = b; i < e; i++)
                {
                    float s = 0.f;
                    for(int k = 1; k < 2000; k++)
                    {
                        s += za::sin(static_cast<float>(i * 7919 + static_cast<za::SizeT>(k))) / static_cast<float>(k);
                    }
                    part[i] = s;
                }
            };
            if(p)
            {
                p->parallelFor(part.size(), 0, body, par);
            }
            else
            {
                body(0, part.size());
            }
            float total = 0.f;
            for(const float v : part)
            {
                total += v;
            }
            return total;
        };
        const float serial = run(nullptr, false);
        bool same = true;
        for(const int n : {1, 2, 5, 16})
        {
            Pool p{n};
            same = same && run(&p, true) == serial && run(&p, false) == serial;
        }
        check(same, va("float reductions with 1, 2, 5, 16 workers, parallel or not: bit for bit the serial one (%.9g)", serial));
    }

    // A pool destroyed with work queued: all of it run, a Future made before still gives its value.
    {
        za::Atomic<int> ran{0};
        Future<int> kept;
        {
            Pool p{2};
            for(int i = 0; i < 100; i++)
            {
                postOnly(p, [&ran] {
                    sleepMs(1);
                    ran.fetchAddSeqCst(1);
                });
            }
            kept = p.async([] { return 42; });
        }
        check(ran.loadSeqCst() == 100 && kept.ready() && kept.get() == 42, "destroyed with 100 tasks queued: every one run, its Future ready");
    }

    // The game's pool.
    {
        Pool* g = jobs::pool();
        int v = 0;
        if(g)
        {
            v = jobs::async([] { return 7; }).get();
        }
        check(g && jobs::workers() > 0 && v == 7, va("the game's pool: %d workers (%d hardware threads)", jobs::workers(), jobs::hardwareThreads()));
    }

    Con_Printf("vr_jobs_test: %d passed, %d failed (%.0f ms)\n", passed, failed,
        static_cast<double>(clock.getElapsedTime().asMicroseconds()) / 1e3);
}

} // namespace qvr::jobs

extern "C" int VR_OnMainThread(void)
{
    // (before VR_Init, only the main thread runs)
    return qvr::jobs::mainThread == za::ThreadId{} || za::ThisThread::getId() == qvr::jobs::mainThread;
}
