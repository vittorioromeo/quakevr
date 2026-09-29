#pragma once

// vr_alloccount.cpp: the C++ allocations (operator new) the calling thread has made so far (the profiler's per-frame
// "allocations", read on the main thread).

#include <cstdint>

namespace qvr::alloccount
{

[[nodiscard]] std::uint64_t thisThread();

} // namespace qvr::alloccount
