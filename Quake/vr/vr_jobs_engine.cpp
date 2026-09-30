// vr_jobs_engine.cpp -- the game's thread pool (vr_jobs.hpp) on the engine's side: made at VR_Init, joined at
// VR_Shutdown; vr_jobs_threads, vr_jobs_parallel, vr_jobs_info and vr_jobs_test (its self-test: pools made and joined,
// every index covered once, the calling thread taking part, every worker busy, exceptions, nested waits, results
// independent of the thread count, a pool destroyed with work queued, the game's pool: one line per check, then the
// totals).

#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_jobs.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace qvr::jobs
{
namespace
{

// A task that is only posted (nobody waits for it): the pool's destructor must run it.
template <typename F>
void postOnly(Pool& p, F&& f)
{
    p.post(std::make_shared<detail::JobOf<void, std::decay_t<F>>>(std::forward<F>(f)));
}

void sleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
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
        restart(std::clamp(static_cast<int>(var->value), 0, 64));
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
std::thread::id mainThread; // (start's: VR_Init runs on the main thread)
} // namespace

void start()
{
    mainThread = std::this_thread::get_id();
    const int i = COM_CheckParm("-jobs");
    init(i && i + 1 < com_argc ? std::clamp(Q_atoi(com_argv[i + 1]), 0, 64) : 0);
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
    const auto check = [&](bool ok, const std::string& what) {
        Con_Printf("vr_jobs_test: %s %s\n", ok ? "ok  " : "FAIL", what.c_str());
        (ok ? passed : failed)++;
    };
    const std::thread::id self = std::this_thread::get_id();
    const auto t0 = std::chrono::steady_clock::now();

    // Start and shutdown: every posted task run before the destructor returns, by more than one worker.
    for(const int n : {1, 2, 3, 8})
    {
        std::atomic<int> ran{0};
        std::vector<std::thread::id> by(64);
        {
            Pool p{n};
            for(int i = 0; i < 64; i++)
            {
                postOnly(p, [&ran, &by, i] {
                    sleepMs(1);
                    by[static_cast<std::size_t>(i)] = std::this_thread::get_id();
                    ran++;
                });
            }
        }
        const std::set<std::thread::id> threads(by.begin(), by.end());
        check(ran == 64 && (n == 1 ? threads.size() == 1 : threads.size() >= 2) && !threads.count(self),
            va("%d workers: 64 posted tasks all run by the join, on %d worker threads", n, static_cast<int>(threads.size())));
    }
    {
        bool ok = true;
        for(int i = 0; i < 50; i++)
        {
            std::atomic<int> ran{0};
            {
                Pool p{1 + i % 4};
                for(int k = 0; k < i; k++)
                {
                    postOnly(p, [&ran] { ran++; });
                }
            }
            ok = ok && ran == i;
        }
        check(ok, "50 pools made and destroyed at once, with 0..49 tasks queued: every task run");
    }

    Pool pool{4};

    // Every index covered exactly once, whatever the count and the chunk.
    {
        bool ok = true;
        for(const std::size_t count : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{7}, std::size_t{100},
                std::size_t{1000}, std::size_t{100003}})
        {
            for(const std::size_t chunk : {std::size_t{0}, std::size_t{1}, std::size_t{3}, std::size_t{64}, std::size_t{1000000}})
            {
                std::unique_ptr<std::atomic<unsigned char>[]> seen{new std::atomic<unsigned char>[count + 1]};
                for(std::size_t i = 0; i <= count; i++)
                {
                    seen[i] = 0;
                }
                std::atomic<bool> bad{false};
                pool.parallelFor(count, chunk, [&](std::size_t b, std::size_t e) {
                    if(b >= e || e > count)
                    {
                        bad = true;
                    }
                    for(std::size_t i = b; i < e; i++)
                    {
                        seen[i]++;
                    }
                });
                for(std::size_t i = 0; i < count; i++)
                {
                    ok = ok && seen[i] == 1;
                }
                ok = ok && !bad && seen[count] == 0;
            }
        }
        check(ok, "parallelFor: counts 0..100003, chunks auto..1000000: every index once, every range in bounds");
    }

    // The calling thread takes part, and so do the workers.
    {
        std::vector<std::thread::id> by(64);
        pool.parallelFor(64, 1, [&](std::size_t b, std::size_t e) {
            for(std::size_t i = b; i < e; i++)
            {
                sleepMs(1);
                by[i] = std::this_thread::get_id();
            }
        });
        const std::size_t mine = static_cast<std::size_t>(std::count(by.begin(), by.end(), self));
        const std::set<std::thread::id> threads(by.begin(), by.end());
        check(mine > 0 && mine < 64 && threads.size() >= 3,
            va("the calling thread ran %d of 64 chunks, %d threads in all", static_cast<int>(mine), static_cast<int>(threads.size())));
    }

    // Every worker busy (blocked): parallelFor still returns, the caller having run every chunk; its helpers, run
    // later, are called off.
    {
        std::atomic<int> gate{0};
        std::atomic<int> blocked{0};
        for(int i = 0; i < pool.workers(); i++)
        {
            postOnly(pool, [&] {
                blocked++;
                while(gate.load() == 0)
                {
                    gate.wait(0);
                }
            });
        }
        while(blocked.load() < pool.workers())
        {
            std::this_thread::yield();
        }
        const Stats before = stats();
        std::atomic<int> mine{0}, total{0};
        pool.parallelFor(1000, 10, [&](std::size_t b, std::size_t e) {
            total += static_cast<int>(e - b);
            if(std::this_thread::get_id() == self)
            {
                mine += static_cast<int>(e - b);
            }
        });
        const bool returned = total == 1000 && mine == 1000;
        gate = 1;
        gate.notify_all();
        Stats after = stats();
        for(int i = 0; i < 1000 && after.helpersCalledOff < before.helpersCalledOff + static_cast<std::size_t>(pool.workers()); i++)
        {
            sleepMs(1); // (the workers free again: the helpers queued behind the blocking tasks run now)
            after = stats();
        }
        check(returned && after.helpersCalledOff >= before.helpersCalledOff + static_cast<std::size_t>(pool.workers()),
            va("every worker blocked: the caller ran all 1000 items; %d helpers called off once freed",
                static_cast<int>(after.helpersCalledOff - before.helpersCalledOff)));
    }

    // Exceptions: the lowest chunk's, after every chunk ran; the same without threads; async's through get().
    for(const bool par : {true, false})
    {
        std::atomic<int> ran{0};
        std::string what;
        try
        {
            pool.parallelFor(
                64, 1,
                [&](std::size_t b, std::size_t) {
                    ran++;
                    if(b == 5 || b == 17 || b == 40)
                    {
                        sleepMs(b == 5 ? 3 : 0); // (the lowest thrown last)
                        throw std::runtime_error("chunk " + std::to_string(b)); // (va isn't thread-safe)
                    }
                },
                par);
        }
        catch(const std::exception& e)
        {
            what = e.what();
        }
        check(what == "chunk 5" && ran == 64,
            va("parallelFor%s: chunks 5, 17, 40 threw: \"%s\" came back, %d of 64 chunks ran", par ? "" : " (serial)",
                what.c_str(), ran.load()));
    }
    {
        Future<int> f = pool.async([]() -> int { throw std::logic_error("async threw"); });
        std::string what;
        try
        {
            (void)f.get();
        }
        catch(const std::exception& e)
        {
            what = e.what();
        }
        {
            Future<int> dropped = pool.async([]() -> int { throw std::logic_error("dropped"); }); // (never got: no terminate)
        }
        std::atomic<long long> sum{0};
        pool.parallelFor(1000, 0, [&](std::size_t b, std::size_t e) {
            for(std::size_t i = b; i < e; i++)
            {
                sum += static_cast<long long>(i);
            }
        });
        check(what == "async threw" && sum == 499500, "async: get() rethrew; a Future dropped with an exception is quiet; the pool works after");
    }

    // Nested waits: parallelFor in parallelFor (three deep), async waited for inside chunks and tasks, every worker
    // waiting for a task queued behind it (the waiter runs it).
    {
        std::atomic<long long> sum{0};
        pool.parallelFor(8, 1, [&](std::size_t, std::size_t) {
            pool.parallelFor(8, 1, [&](std::size_t, std::size_t) {
                Future<int> inner = pool.async([&] {
                    int s = 0;
                    pool.parallelFor(10, 1, [&](std::size_t b, std::size_t e) { sum += static_cast<long long>(e - b); });
                    return s + 1;
                });
                sum += inner.get();
            });
        });
        check(sum == 8 * 8 * 11, va("nested parallelFor x3 with async waits inside: %lld of 704", sum.load()));
    }
    {
        Pool two{2};
        const Stats before = stats();
        std::atomic<int> started{0};
        std::vector<Future<int>> outer;
        for(int i = 0; i < 2; i++)
        {
            outer.push_back(two.async([&two, &started, i] {
                started++;
                while(started.load() < 2)
                {
                    std::this_thread::yield(); // both workers taken
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
            std::vector<float> part(997);
            const auto body = [&](std::size_t b, std::size_t e) {
                for(std::size_t i = b; i < e; i++)
                {
                    float s = 0.f;
                    for(int k = 1; k < 2000; k++)
                    {
                        s += std::sin(static_cast<float>(i * 7919 + static_cast<std::size_t>(k))) / static_cast<float>(k);
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
        std::atomic<int> ran{0};
        Future<int> kept;
        {
            Pool p{2};
            for(int i = 0; i < 100; i++)
            {
                postOnly(p, [&ran] {
                    sleepMs(1);
                    ran++;
                });
            }
            kept = p.async([] { return 42; });
        }
        check(ran == 100 && kept.ready() && kept.get() == 42, "destroyed with 100 tasks queued: every one run, its Future ready");
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
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count() * 1e3);
}

} // namespace qvr::jobs

extern "C" int VR_OnMainThread(void)
{
    return std::this_thread::get_id() == qvr::jobs::mainThread;
}
