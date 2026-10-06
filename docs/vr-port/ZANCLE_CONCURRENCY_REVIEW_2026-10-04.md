# Zancle Concurrency review — 2026-10-04

Zancle's concurrency module is competitive for CPU loops and batched dispatch on this machine. Its caller participation and late-helper gates are useful for frame work. It has several confirmed failure-path defects, an unbounded stale-helper backlog under saturation, and workload-dependent performance tradeoffs. The evidence does not support calling it issue-free, universally fastest, or fully tuned.

This review changed no production code; its five defects were fixed afterwards (Status, below).

## Status (2026-10-07: all five fixed on Zancle's branch `zancle-concurrency-fixes`, vendored)

All five defects are fixed on Zancle's branch `zancle-concurrency-fixes` (off `rebrand_to_zancle`'s `7bd385db`), one
commit each, for the author to merge into `rebrand_to_zancle`. Quake VR vendors its head, `2f8a1ca5`
(`Quake/vr/external/zancle/README.md`; `ZANCLE_REPORT.md` B9-B13). Each fix has a regression test in Zancle's suite
that failed (or hung) before it, except finding 1's abort, checked with this review's fault-injected queue instead.
Paths below are relative to `Quake/vr/external/zancle/`. (Checked 2026-10-06 against `bbe2ed97`: all five open.)

| Finding | Status | Fix |
|---|---|---|
| 1. enqueue failure loses tasks | **fixed** (`0e23061a5`) | Every enqueue in `src/Zancle/Concurrency/ThreadPool.cpp` (`post`, `postBulk`, `postCopies`, the stop tasks and their reinsertion) aborts with `[[ZANCLE THREADPOOL FAILURE]]` and a stack trace when the queue cannot allocate, in every build. The fault-injected queue: `post` aborts instead of losing the task, `parallelFor` aborts instead of leaving its slots waiting forever. |
| 2. partial construction deadlock | **fixed** (`edf931db7`) | A scope guard in the constructor posts the started workers' stop tasks and joins them before the exception leaves (if that queuing fails too, finding 1's abort). Test: `test/TestUtilities/AlignedAllocationUtil` injects `std::bad_alloc` into each constructor allocation in turn (it hung before). |
| 3. throwing callable leaks entry | **fixed** (`95b4fe0e3`) | `include/Zancle/Concurrency/Thread.hpp`'s `allocateEntry` frees the block if the callable's constructor throws; the constructor documents what it can throw. Test: one leaked aligned block before, none after. |
| 4. `getId` after join/detach | **fixed** (`1f86cc4c2`) | `join` and `detach` clear the id (`Thread.cpp`); tests for both. |
| 5. unbounded stale-helper backlog | **fixed** (`2f8a1ca5b`) | `ParallelForSlots::reserveHelpers` keeps the helpers not yet finished within `outstandingHelpersPerWorker` (64) per worker of the call's pool, for both wake strategies; past it, calls run on their callers. This review's saturation stress: 107.38 MiB growth and a 54 ms drain before, 0.05 MiB and 0.18 ms after. Test: 5,000 calls with both workers busy leave at most 128 helpers queued (10,000 before). |

Verification (2026-10-07): Zancle's base and system suites (MSYS2 clang64 Debug with ASan/UBSan, where
`AlignedAllocationUtil`'s tests compile out, and UCRT64 Debug and Release, where they run): all concurrency tests pass;
the only failure, `FileInputStream`'s temporary-file test, is outside `Concurrency`. Quake VR: Release and Debug
(`QVR_ZANCLE_DEBUG`) builds, warden, ad_grendel and e1m1 loads (AO bakes included) with hull, liquid, decal and hit-model hashes the same with
`vr_jobs_parallel 0`, a relight batch started and cancelled, `bench.sh --validate` on the three load scenarios.

Local paths: the `C:/OHWorkspace/SFML` and `C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/...` links point
to the author's machine (an ignored build tree, still present on 2026-10-06). They are not in Git.

## Scope and versions

- Input: `C:/OHWorkspace/SFML`, clean working tree, HEAD `304ea6c3bfe209bb27f18c848b605c713c7bf8bd`.
- Inspected all ten headers/source files under `Zancle/Concurrency`: ThreadPool, ParallelFor/ParallelForSlots, Thread, Atomic/AtomicWait, AtomicMutex, and LockGuard. Also inspected FixedFunction, InPlacePImpl, allocation support, the moodycamel queue/semaphore, and Quake VR's `vr_jobs` integration.
- All ten concurrency files are byte-identical to Quake VR's vendored copy. File hashes and comparison results are saved in [reviewed-files.json](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/reviewed-files.json).
- This is an audit of the concurrency module and its relevant dependencies, not an audit of every graphics/audio/container facility in the larger Zancle toolkit.
- Comparison implementations: BS::thread_pool 5.1.0 at `bd4533f1f70c2b975cbd5769a60d8eaaea1d2233`; Taskflow at `bbd7251d577b33a4aeff434ce5f5569b94d4cc48`; locally installed oneTBB 2023.1.0. BS and Taskflow were downloaded from their current upstream branches and pinned for the tests. oneTBB was the installed version, not a newly downloaded latest release.

## Confirmed defects and resource behavior

### 1. P1: enqueue allocation failure silently loses tasks in Release (fixed 2026-10-07, `0e23061a5`)

Locations: [ThreadPool.cpp:136](C:/OHWorkspace/SFML/src/Zancle/Concurrency/ThreadPool.cpp:136), bulk enqueue at line 176, and `enqueueCopies` at line 63. Stop-task reinsertion at line 200 ignores the result even in Debug.

The queue reports allocation failure through a boolean. Public posting functions return `void` and only check that boolean with `ZA_ASSERT`, which disappears under `NDEBUG`. A rejected task therefore looks successfully posted to the caller. This also affects the allocation-free-looking parallel-for interface: its callback/frame need no heap block, but its queued helpers can require queue allocations.

`parallelFor` increments `m_outstanding` before posting. A rejected helper leaves that count permanently elevated: the call can complete on its caller, but destruction of its slots waits forever. Failed stop-task submission or reinsertion can similarly leave a worker asleep while the destructor joins it.

Reproducer: a scratch copy of ThreadPool.cpp substitutes a moodycamel traits allocator that returns null on request; the posting/shutdown code is otherwise unchanged. No real memory exhaustion is induced.

Observed:

```text
post returned normally; executed=0 rejected allocations=1
parallelFor returned; processed=10, rejected allocations=1; destroying slots now
```

The second process failed to finish within 2.5 seconds and was stopped. Inspection explains the permanent outstanding count.

Recommendation: check every enqueue result in all builds. For the current void, noexcept-oriented design, an unconditional fail-fast path is the smallest coherent fix. Recoverable posting needs an explicit result and correct helper-accounting rollback/fallback; throwing from a noexcept parallel-for is not recovery. Include stop submission and reinsertion in the same policy.

### 2. P2: partial pool construction deadlocks on a C++ allocation exception (fixed 2026-10-07, `edf931db7`)

Location: [ThreadPool.cpp:100](C:/OHWorkspace/SFML/src/Zancle/Concurrency/ThreadPool.cpp:100).

Worker thread entries allocate individually. If entry allocation throws after a worker has started, ThreadPool's constructor unwinds. Its destructor is not called for a failed construction. The partially constructed implementation destroys its worker vector; each `Thread` automatically joins, but no stop task has been posted, so a worker remains blocked in `wait_dequeue`.

Reproducer: the second aligned allocation throws `std::bad_alloc`, after the first worker starts. The process prints the injection point and never reaches the catch outside pool construction; it was stopped after 2.5 seconds.

Recommendation: add constructor cleanup that stops and joins already-created workers before propagating an exception, or consistently terminate at thread-entry allocation failure if the API is intended to be fail-fast. Cleanup must itself work when ordinary queue allocation fails; simply calling the existing allocation-dependent shutdown routine is insufficient for a robust recovery guarantee.

This is an exception-enabled library defect. Quake VR's normal build disables exceptions, so its allocation-failure behavior differs. The silent moodycamel enqueue failure in finding 1 still matters there.

### 3. P2: a throwing callable construction leaks a thread-entry allocation (fixed 2026-10-07, `95b4fe0e3`)

Location: [Thread.hpp:270](C:/OHWorkspace/SFML/include/Zancle/Concurrency/Thread.hpp:270).

`allocateEntry` allocates a combined entry/callable block, then placement-constructs the callable without a cleanup guard. If a copy or move constructor throws, control never reaches the OS thread constructor or its spawn-failure cleanup, and the allocation is leaked.

The reproducer's callable copy throws; instrumentation records exactly one aligned allocation left outstanding.

Recommendation: keep a scope guard on the allocated block until callable construction completes. Document the actual exception policy: the existing constructor can throw from allocation or callable construction despite wording suggesting it throws no C++ exceptions. This also concerns exception-enabled users rather than Quake VR's usual build.

### 4. P2: getId violates its documented post-join/post-detach contract (fixed 2026-10-07, `1f86cc4c2`)

Location: [Thread.cpp:340](C:/OHWorkspace/SFML/src/Zancle/Concurrency/Thread.cpp:340).

The header says `getId()` returns an empty identifier when the object is not joinable. `join()` and `detach()` clear joinability but retain `m_id`, and `getId()` returns it unconditionally.

Observed:

```text
getId before=1 after_join=1 joinable=0
```

Recommendation: clear the ID on successful join/detach, or have `getId()` return an empty ID for a non-joinable object. Add tests for both operations. Keeping a historical ID instead would require deliberately changing the documented contract.

### 5. P2: completed parallel-for calls can accumulate an unbounded helper backlog (fixed 2026-10-07, `2f8a1ca5b`)

Locations: [ParallelFor.hpp:395](C:/OHWorkspace/SFML/include/Zancle/Concurrency/ParallelFor.hpp:395), tree helper posting at line 241, and slot reuse at line 216.

Gates bound concurrent active calls, not queued helper tasks. A caller finishes its work, closes and releases its gate, and can immediately post helpers for another call while every worker remains occupied. Late helpers are memory-safe under the normal generation protocol, but they remain real queue entries until consumed.

With seven workers deliberately parked, 100,000 calls over 32 trivial items with grain 1 produced:

```text
calls=100000 helpers/call=7 items=3200000
private_memory_growth_MiB=107.39 loop_time_ms=43.51
pool drain+join after release_ms=78.17
retained_memory_after_pool_MiB=0.33
```

This is a backlog, not a permanent memory leak. Nevertheless, sustained saturation can consume memory quickly and delay subsequent work or shutdown. The 64-gate limit does not protect against it. This is an intentionally extreme stress case, not an observed 107 MiB increase during ordinary VR gameplay.

Recommendation: bound outstanding helper submissions and run additional calls on their callers when that budget is exhausted. Consider coalescing helpers or using an approximate worker-availability signal as a performance hint. Preserve the existing rule that the frame caller does not execute unrelated jobs. Tree wake reduces initial queued helpers from all available helpers to at most two, but by itself does not bound growth.

## Performance measurements

### Method and practical limits

Hardware: Intel Core i9-13900K, 8 P cores with SMT plus 16 E cores, 32 logical processors. Most comparisons restricted only the benchmark process to the 16 P-core logical processors (`0xffff`), avoiding differences in CPU class. Individual threads were not pinned to specific cores. A separate 31-worker run used all processors.

Compiler: MSYS2 UCRT Clang 22.1.8, C++23, `-O3 -DNDEBUG`, common executable and arithmetic kernel. These are Windows/MinGW results, not measurements of the game's clang-cl executable, headset frame timings, or another operating system.

Pools exist before timing. Outputs are checked against a serial reference. Each scenario has 30 warmups, followed by 80–600 samples. Cold scenarios sleep for 2 ms before each sample. Times include executing the work and waiting for its completion. Very short values approach the clock's resolution and should not be treated as precision rankings.

For loops, Zancle uses W workers plus its caller, and BS/Taskflow use W+1 workers; oneTBB's arena permits W+1 participants including its caller. That compares equal potential computational participation, rather than giving Zancle an extra CPU participant. For asynchronous queue tests, every implementation has W background participants and uses the same batch-completion protocol.

BS uses its native block API with four blocks per participant; its default block count was checked separately. Taskflow uses a reusable graph and a dynamic partitioner with the same nominal grain as Zancle; a guided partitioner was also checked. oneTBB uses its native automatic partitioner. In particular, oneTBB can choose coarser partitions even when given a small grain, and BS does not promise Zancle's exact callback chunk boundaries. The grain-1 row compares ways to perform the same arithmetic workload, not identical scheduling semantics.

Runs were sequential, but system background activity, CPU frequency, OS scheduling and runtime order were not controlled experimentally. All-core results varied substantially: a medium flat-wake loop measured 176.5 us in the full comparison and about 59.5 us in later isolated repeats. Consequently, these numbers establish competitiveness and tuning directions, not a definitive universal ranking. Latency percentiles are useful observations from small samples, not statistical confidence bounds.

### Representative loop results

Seven workers, P cores only; ranges are medians from two final comparison runs, in microseconds. Lower is better.

| Workload | Serial | Zancle | BS | Taskflow dynamic | oneTBB auto |
|---|---:|---:|---:|---:|---:|
| 4,096 items, 128 arithmetic rounds/item, default Zancle grain | 577 | 99–102 | 114–123 | 125–126 | 90–93 |
| 65,536 items, 32 rounds/item | 1,360–1,366 | 222–225 | 235–236 | 277–283 | 221–228 |
| 512 uneven items, scattered much heavier items | 361–363 | 67–71 | 91–93 | 83–84 | 57–58 |
| First workload with Zancle grain 1 | 576–580 | 130–136 | 114–125 | 142–146 | 86–90 |
| 32 trivial items, cold | about 0.3 | 19–20 | 79–82 | 84–97 | about 12 |

Zancle is competitive with these established runtimes. Its large uniform loop is effectively level with oneTBB in these two runs; oneTBB wins the medium and uneven cases. Zancle outperforms the tested BS and dynamic Taskflow configurations in most of these loop cases. That does not establish dominance over every partitioner or implementation available online.

Grain 1 adds substantial atomic-claim traffic for uniform work. It is justified for the game's small, uneven solve items, but should not be the default choice for large uniform ranges. The tiny workload is much faster serially than through any runtime; caller participation can hide worker latency, but posting helpers still costs something.

### Dispatch and completion

Seven background workers, P cores only; medians from the same final runs, in microseconds. Each queued task executes eight rounds of the arithmetic kernel, writes a separate output, and participates in a common completion protocol.

| Workload | Zancle | BS | Taskflow | oneTBB |
|---|---:|---:|---:|---:|
| One cold posted task | 18–22 | 25–26 | 63–68 | 34–36 |
| 1,024 individually posted tasks | 356–527 | 6,251–6,892 | 360–364 | 178–201 |
| 1,024 tasks using native bulk submission | 92–138 | 306–318 | not measured | not measured |

Zancle's individual posting is variable: the batch's p95 was about 1.51–1.62 ms, versus about 0.41 ms for Taskflow and 0.21–0.23 ms for oneTBB in these runs. Bulk submission substantially improves it. With 31 workers, individually submitting the same batch took about 1.7–1.8 ms in several runs, compared with roughly 0.14–0.18 ms for bulk.

These are whole-batch dispatch/completion measurements, not isolated queue operations. The shared completion counter is itself a contention point. No future/promise allocation is charged selectively to a competitor.

### Wake policy experiment

Windows currently hardcodes flat wake because its semaphore can release several waiters in one system call. A single syscall does not remove the costs of queueing tasks, waking/scheduling many threads, and gate contention.

The existing internal tree implementation was exercised without changing library sources. Two back-to-back pairs of flat/tree runs at 31 workers gave these median ranges, in microseconds:

| Workload | Current flat | Experimental tree |
|---|---:|---:|
| Trivial warm loop | 1.5–1.7 | about 0.3 |
| Trivial cold loop | 44–45 | 6.8–7.1 |
| Medium uniform loop | 59.5–59.7 | 70.3–72.6 |
| Large uniform loop | 143–154 | 150–157 |
| Uneven loop | 54–55 | 56–58 |

The repeated evidence supports tree wake for small/cold calls and flat wake for some larger calls. It does not support blindly replacing the Windows default. Expose a policy or choose using workload/helper count, then validate against actual `vr_jobs_sites` and grasp-solve frame tails on the game's toolchain.

### AtomicMutex

P-core process affinity, 100,000 protected increments per thread, median of 16 runs. Worker creation is outside timing; completion/join is included.

| Threads | za::AtomicMutex (us) | std::mutex (us) |
|---:|---:|---:|
| 1 | 787 | 962 |
| 2 | 2,299 | 4,592 |
| 4 | 8,683 | 8,505 |
| 8 | 59,015 | 35,549 |

The compact three-state mutex performs well under light contention. At eight threads on this tiny critical section, it is about 1.66 times slower than this MinGW standard mutex. Adaptive spinning/backoff and cache-line placement deserve profiling for highly contended uses. This does not establish a fairness failure, nor predict MSVC's mutex performance.

## Design audit and optimization priorities

The core gate protocol is carefully designed: queue publication makes the frame available; an entering helper increments the gate before touching that frame; the caller closes the gate and drains entered helpers before returning; late helpers avoid the old frame. Release/acquire operations and their release sequences publish chunk writes to the caller. The outstanding count keeps slots alive for queued helpers, and its final notification uses an address key after the decrement. I found no ordinary-path missing-wakeup or gate lifetime defect in the examined protocol and tests. This is not a formal proof for every supported target.

The 32-bit wait backend's 64-bit version-word indirection avoids waiting on an unchanged half of a value. Hash collisions for those version words wake all waiters, avoiding a notification being consumed only by a waiter for an unrelated address. The SeqCst waiter registration/fence should not be weakened merely because an x86 stress test passes. AtomicMutex's acquire/release transitions and LockGuard's lifetime behavior are appropriate.

ThreadPool workers already use consumer tokens and promptly destroy task captures. Task closures are inline, but queue blocks and producer bookkeeping can allocate. `Task` is 144 bytes on the tested ABI: copying/moving small closures still transfers the full 128-byte storage buffer. A smaller-capacity task type is an experiment to benchmark, not a justified unconditional ABI change.

Recommended order:

1. Fix enqueue failure handling, including shutdown and helper accounting. Add injected-failure regression tests.
2. Bound stale helper submissions under sustained saturation.
3. Fix constructor unwind cleanup, thread-entry exception cleanup, and getId's contract.
4. Prefer bulk submission for independent small jobs. Investigate an optional explicit producer handle for posting-heavy clients, with a documented lifetime; avoid a hidden token cache that can outlive its pool.
5. Tune wake strategy and minimum work thresholds per call site. Preserve Quake VR's existing decision to run known small sites serially. A universal count threshold is insufficient when item costs differ.
6. Use larger grains for uniform work and benchmark worker counts against frame tails. On hybrid CPUs, hardware logical-thread count is a capacity hint, not an optimum. Windows `usableHardwareConcurrency()` still reported 32 with the benchmark process restricted to 16 processors; this fallback is documented, but limits the usefulness of the hint.
7. Profile heavily contended mutexes separately and revisit the fixed 50 us spin/1 ms yield window for waiting parallel-for callers under oversubscription. Those budgets can waste CPU when helper work is long or a worker is descheduled.

Other limits to record:

- The queue has per-producer ordering, not one global FIFO order or NUMA-aware scheduling. Task execution/completion order is even less constrained with multiple consumers. Zancle should document this explicitly if users might assume global ordering.
- 32-bit gate generations wrap after 2^32 reuses. A very old helper must not survive until its generation repeats; sharing slots between pools makes that theoretically possible without accumulating 2^32 tasks in one queue. This was not exercised, and is not a realistic normal VR frame scenario, but the lifetime safety guarantee needs an epoch/retirement bound for arbitrarily long executions.
- A chunk counter can wrap at the extreme `SizeT` boundary. Add arithmetic-boundary tests or an explicit input bound if the range API is intended to support such virtual ranges.
- Emscripten's native atomic waits cannot block the browser main thread; use a worker/proxied main as documented. Current wording should describe that unsupported operation accurately rather than promise a spin fallback from a native atomic-wait instruction.
- Linux/Android/Apple/BSD/WASM runtime behavior, NUMA machines, and ThreadSanitizer were not tested here. Windows AddressSanitizer does not prove freedom from data races.

## Verification and reproducibility

| Configuration | Result |
|---|---|
| Clang UCRT Release | 56 test cases, 919 assertions, all passed |
| Clang UCRT Debug | 56 test cases, 919 assertions, all passed |
| Clang CLANG64 AddressSanitizer, RelWithDebInfo | 56 test cases, 919 assertions, all passed; no ASan diagnostics |

The upstream tests include concurrent and nested calls, nesting beyond 64 gates, two pools sharing slots, stale-helper reuse stress, destruction in either order, publication of chunk writes, both wake algorithms, recursive posting during shutdown, atomic wait/notify, and mutex stress. The passing suite does not cover the injected failure cases above.

Review harness and raw results are retained in [the scratch directory](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/CMakeLists.txt), which is under the ignored build tree:

- [benchmark.cpp](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/benchmark.cpp)
- [final P-core comparison](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/benchmark-final-pcores.csv), [repeat at seven workers](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/benchmark-repeat7-pcores.csv), [31-worker full comparison](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/benchmark-final-allcores31.csv)
- [alternative peer partitioners](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/benchmark-peers-pcores.csv)
- [flat A](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/wake-flat-a.csv), [tree A](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/wake-tree-a.csv), [flat B](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/wake-flat-b.csv), [tree B](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/wake-tree-b.csv)
- [allocation_fault.cpp](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/allocation_fault.cpp), [instrumented pool source](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/fault_ThreadPool.cpp), [constructor_fault.cpp](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/constructor_fault.cpp), [thread reproducer](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/repro.cpp)
- [saturation result](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/saturation-results.txt), [mutex results](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/mutex-results.csv)
- [Release tests](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/upstream-tests-release.txt), [Debug tests](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/upstream-tests-debug.txt), [ASan tests](C:/OHWorkspace/quakevr-iw/build-cmake/zancle-review/upstream-tests-asan.txt)

From the Quake VR checkout, with the existing dependencies and downloaded peer sources retained:

```powershell
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH
cmake -S build-cmake/zancle-review -B build-cmake/zancle-review/bin -G Ninja `
  -DCMAKE_CXX_COMPILER=C:/msys64/ucrt64/bin/clang++.exe -DCMAKE_BUILD_TYPE=Release
cmake --build build-cmake/zancle-review/bin -j 6
& build-cmake/zancle-review/bin/upstream_tests.exe
& build-cmake/zancle-review/bin/benchmark.exe all pcores 7
& build-cmake/zancle-review/bin/benchmark.exe zancle allcores 31
& build-cmake/zancle-review/bin/benchmark.exe tree allcores 31
& build-cmake/zancle-review/bin/allocation_fault.exe
& build-cmake/zancle-review/bin/repro.exe
& build-cmake/zancle-review/bin/saturation.exe
& build-cmake/zancle-review/bin/mutex_bench.exe
```

The `allocation_fault parallel` and `constructor_fault` modes intentionally block to reproduce the defects; run them with a timeout, as done for this review. The mutex fixture's P-core mask is specific to this host. Re-check `reviewed-files.json` if upstream changes before reproducing.

## Primary online references

- [moodycamel ConcurrentQueue documentation](https://github.com/cameron314/concurrentqueue): allocation/result behavior, producer tokens, bulk operations, ordering, and NUMA limitations.
- [BS::thread_pool documentation](https://github.com/bshoshany/thread-pool/blob/bd4533f1f70c2b975cbd5769a60d8eaaea1d2233/README.md) and [the pinned header](https://github.com/bshoshany/thread-pool/blob/bd4533f1f70c2b975cbd5769a60d8eaaea1d2233/include/BS_thread_pool.hpp): native loop/bulk APIs and scheduling implementation.
- [Taskflow's pinned source](https://github.com/taskflow/taskflow/tree/bbd7251d577b33a4aeff434ce5f5569b94d4cc48/taskflow): executor and dynamic/guided partitioning implementations. These were compiled directly for the comparison.
- [oneTBB automatic chunking](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/Automatic_Chunking.html) and [controlling chunking](https://uxlfoundation.github.io/oneTBB/main/tbb_userguide/Controlling_Chunking_os.html): why a nominal grain need not imply exact chunk boundaries under automatic partitioning.
- [Emscripten atomic API](https://github.com/emscripten-core/emscripten/blob/main/system/include/emscripten/atomic.h): browser main-thread restriction on native atomic waits.

These references explain design and API behavior. Performance rankings in this review come from the local harness, not upstream marketing or benchmarks from different machines.

