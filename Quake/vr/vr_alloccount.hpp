#pragma once

#include "Zancle/Base/IntTypes.hpp"

// vr_alloccount.cpp: the C++ allocations (operator new) the calling thread has made so far (the profiler's per-frame
// "allocations", read on the main thread).


namespace qvr::alloccount
{

[[nodiscard]] za::U64 thisThread();

} // namespace qvr::alloccount
