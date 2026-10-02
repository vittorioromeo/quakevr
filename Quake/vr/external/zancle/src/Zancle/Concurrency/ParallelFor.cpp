// LICENSE AND COPYRIGHT (C) INFORMATION
// https://github.com/vittorioromeo/Zancle/blob/master/license.md


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "Zancle/Concurrency/ParallelFor.hpp"

#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/Thread.hpp"

#include "Zancle/Chrono/Clock.hpp"

#include "Zancle/Base/Assert.hpp"
#include "Zancle/Base/CpuRelax.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"


namespace
{
////////////////////////////////////////////////////////////
// How long a caller waiting for the helpers inside spins, then yields, before sleeping
constexpr za::I64 spinNanoseconds  = 50'000;
constexpr za::I64 yieldNanoseconds = 1'000'000;

} // namespace


namespace za
{
////////////////////////////////////////////////////////////
// `finishHelper` notifies through the address of the `Atomic` itself
static_assert(sizeof(Atomic<U64>) == sizeof(U64));


////////////////////////////////////////////////////////////
ParallelForSlots::ParallelForSlots() noexcept = default;


////////////////////////////////////////////////////////////
ParallelForSlots::~ParallelForSlots()
{
    ZA_ASSERT(m_usedMask.loadRelaxed() == 0u &&
              "ParallelForSlots destroyed while a parallelFor using it is in progress");

    // Helper tasks still queued in a pool will look at the gates: wait until all of them have run
    if (m_outstanding.fetchOrAcqRel(destroyBit) == 0u)
        return;

    m_outstanding.waitUntilAcquire([](const U64 outstanding) { return outstanding == destroyBit; });
}


////////////////////////////////////////////////////////////
void ParallelForSlots::waitForHelpersInside(const SizeT slot) noexcept
{
    Atomic<U64>& word = m_gates[slot].word;

    const auto helpersInside = [&word] { return (word.loadAcquire() & insideMask) != 0u; };

    // Every chunk is claimed: the helpers inside are finishing their last one. Stay awake for a while,
    // as waking up from a sleep can take far longer than a chunk (e.g. milliseconds on a busy machine).
    // First spin (the common case: chunks of microseconds), then yield the core to other threads.
    const I64 start = Clock::nowNanoseconds();

    for (I64 elapsed = 0; elapsed < spinNanoseconds; elapsed = Clock::nowNanoseconds() - start)
        for (int i = 0; i < 64; ++i)
        {
            if (!helpersInside())
                return;

            ZA_CPU_RELAX();
        }

    while (Clock::nowNanoseconds() - start < yieldNanoseconds)
    {
        if (!helpersInside())
            return;

        ThisThread::yield();
    }

    // Long chunks: sleep until the last helper leaves (it only notifies when this bit is set)
    if ((word.fetchOrAcqRel(waitingBit) & insideMask) == 0u)
        return;

    word.waitUntilAcquire([](const U64 value) { return (value & insideMask) == 0u; });
}

} // namespace za
