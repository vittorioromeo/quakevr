#pragma once

// vr_audio_internal.hpp -- what vr_audio.cpp (see vr_audio.hpp) shares with its part vr_audio_io.cpp (derived and
// layered sounds, the band-limit tables, the full-band resampler). Internal: included by those two alone. What the
// part uses of the file's anonymous namespaces is audio::detail: its types here, its variables declared here and
// defined where they stood.

#include "vr_stats.hpp"
#include "vr_geom.hpp"
#include "vr_random.hpp"
#include "vr_audio.hpp"
#include "vr_alloccount.hpp"
#include "vr_audiobench.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_units.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Algorithm/NthElement.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Log10.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"
#include <emmintrin.h> // (SSE2: x64's baseline)
#include <stdio.h>

namespace qvr::audio::detail
{

// vr_audio.cpp: the jobs sites (their parallelFor calls: vr_jobs_sites)
extern jobs::Site audioTaps;
extern jobs::Site audioBandlimit;

// vr_audio.cpp: VR_SndBandLimit's work since the start (the band-limited copies made as sounds load: the main thread).
struct BandLimitStats
{
    int sounds{0};
    double ms{0.0};
    double worstMs{0.0};
    long long bytes{0};
};
extern BandLimitStats bandLimitStats;

} // namespace qvr::audio::detail
