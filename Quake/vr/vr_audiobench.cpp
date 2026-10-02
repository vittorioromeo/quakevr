// vr_audiobench.cpp -- the spatial audio's benchmark recorder; see vr_audiobench.hpp.

#include "vr_audiobench.hpp"
#include "vr_engine.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"

namespace qvr::audio::bench
{

bool on{false};

static_assert(int{VR_SNDBENCH_PAINT} == int{Paint} && int{VR_SNDBENCH_QUAKE} == int{Quake} && int{VR_SNDBENCH_FILTERS} == int{Filters});

namespace
{

constexpr const char* names[] = {"listener", "brush", "sim_update", "paint", "spatial", "select", "lock", "voices",
    "reverb", "reverb_conv", "reverb_decode", "antialias", "quake", "filters", "shadow", "limit", "cpu_read", "cpu_direct", "cpu_binaural", "cpu_other", "allocs"};
static_assert(sizeof names / sizeof names[0] == Count);

// The bench's frames (vr_snd_bench: its own data, kept for its report till the next one).
struct Recording
{
    int wanted{0};
    za::Array<double, Count> current{};
    int currentSamples{0};
    int currentVoices{0};
    bool currentAny{false};
    za::Array<za::Vector<float>, Count> rows; // ms, frame by frame
    za::Vector<int> rowSamples;
    za::Vector<int> rowVoices;
};
Recording rec;

} // namespace

const char* stageName(int s)
{
    return s >= 0 && s < Count ? names[s] : "?";
}

double now()
{
    return on ? Sys_DoubleTime() : 0.0;
}

void add(int stage, double since)
{
    if(on && since != 0.0)
    {
        rec.current[stage] += Sys_DoubleTime() - since;
        rec.currentAny = true;
    }
}

void addSeconds(int stage, double seconds)
{
    if(on)
    {
        rec.current[stage] += seconds;
        rec.currentAny = true;
    }
}

void addCount(int stage, double n)
{
    if(on && n != 0.0)
    {
        rec.current[stage] += n * 0.001; // (shown as ms: the count)
        rec.currentAny = true;
    }
}

void painted(int samples, int voices)
{
    if(on)
    {
        rec.currentSamples += samples;
        rec.currentVoices = za::max(rec.currentVoices, voices);
    }
}

void start(int frames)
{
    rec.wanted = frames;
    for(za::Vector<float>& r : rec.rows)
    {
        r.clear();
        r.reserve(static_cast<za::SizeT>(frames));
    }
    rec.rowSamples.clear();
    rec.rowSamples.reserve(static_cast<za::SizeT>(frames));
    rec.rowVoices.clear();
    rec.rowVoices.reserve(static_cast<za::SizeT>(frames));
    rec.current = {};
    rec.currentSamples = 0;
    rec.currentVoices = 0;
    rec.currentAny = false;
    on = true;
}

void stop()
{
    on = false;
}

void frame()
{
    if(!on)
    {
        return;
    }
    if(rec.currentAny || rec.currentSamples > 0)
    {
        for(int s = 0; s < Count; s++)
        {
            rec.rows[s].pushBack(static_cast<float>(rec.current[s] * 1000.0));
        }
        rec.rowSamples.pushBack(rec.currentSamples);
        rec.rowVoices.pushBack(rec.currentVoices);
    }
    rec.current = {};
    rec.currentSamples = 0;
    rec.currentVoices = 0;
    rec.currentAny = false;
}

Stats stats(int stage, int rate)
{
    Stats st;
    const za::Vector<float>& r = rec.rows[stage];
    // (The mix's stages over the frames that painted; the listener's over all.)
    za::Vector<float> sorted;
    sorted.reserve(r.size());
    for(za::SizeT i = 0; i < r.size(); i++)
    {
        if(stage < Paint || stage == Allocs || rec.rowSamples[i] > 0)
        {
            sorted.pushBack(r[i]);
        }
    }
    if(sorted.empty())
    {
        return st;
    }
    za::quickSort(sorted.begin(), sorted.end(), [](float a, float b) { return a < b; });
    const int n = static_cast<int>(sorted.size());
    const auto at = [&](double q) { return static_cast<double>(sorted[za::min(n - 1, static_cast<int>(q * (n - 1) + 0.5))]); };
    st.n = n;
    st.median = at(0.5);
    st.p95 = at(0.95);
    st.p99 = at(0.99);
    st.max = sorted[n - 1];
    double sum = 0.0;
    for(float x : sorted)
    {
        sum += x;
    }
    st.mean = sum / n;
    const long long total = samples();
    st.perSecond = total > 0 && rate > 0 ? sum / (static_cast<double>(total) / rate) : 0.0;
    return st;
}

int frames()
{
    return static_cast<int>(rec.rowSamples.size());
}

long long samples()
{
    long long t = 0;
    for(int s : rec.rowSamples)
    {
        t += s;
    }
    return t;
}

double meanVoices()
{
    double t = 0.0;
    int n = 0;
    for(za::SizeT i = 0; i < rec.rowVoices.size(); i++)
    {
        if(rec.rowSamples[i] > 0) // (the frames that painted)
        {
            t += rec.rowVoices[i];
            n++;
        }
    }
    return n > 0 ? t / n : 0.0;
}

} // namespace qvr::audio::bench

extern "C" double VR_SndBenchNow(void)
{
    return qvr::audio::bench::now();
}

extern "C" void VR_SndBenchAdd(int stage, double since)
{
    qvr::audio::bench::add(stage, since);
}
