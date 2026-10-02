// vr_audiobench.hpp -- the spatial audio's benchmark recorder (vr_snd_bench, vr_audio.cpp): each sound frame's time in
// each stage of the mix, kept frame by frame while a bench runs, then their median, 95th and 99th percentiles, worst and
// load (ms of each second of sound). Off (a bench not running) a stage's timing is one bool read.
//
// A frame is one S_Update (VR_SndListener starts the next one). Main-thread stages are wall time; the Cpu* stages are
// the voices' own time summed over the pool's threads (the live mixer's). The mix's stages (Paint on) are taken over the
// frames that painted (the mix goes in whole voice frames, vr_snd_frame samples).
#pragma once

namespace qvr::audio::bench
{

enum Stage : int
{
    Listener,  // VR_SndListener: the head, the movers into the scene, the simulations' inputs and results
    Brush,     // trackBrushEntities (the movers; a brush model's first sight builds its sub-scene)
    SimUpdate, // Simulation::update (tasks started, finished ones taken, the scene committed) and the results' copies
    Paint,     // S_PaintChannels, all of it (snd_mix.c)
    Spatial,   // VR_SndPaint: the voices' share
    Select,    // the voices chosen, each one's input (VR_SndPaint)
    Lock,      // the simulation's results taken under its mutex (in Select)
    Voices,    // Mixer::render: the voices (prepared, rendered on the pool, summed)
    Reverb,    // Mixer::render: the reverb (reflection effect, ambisonics decode), wall time on the main thread
    ReverbConv,   // the reflection effect alone (the convolution with the simulated response), on whichever thread
    ReverbDecode, // the ambisonics decode alone (binaural), on whichever thread
    AntiAlias, // renderVoices: the reverb's (or the voices') lowpass before Quake's
    Quake,     // S_PaintChannels: Quake's own channels (snd_mix.c)
    Filters,   // S_PaintChannels: the bus, Quake's lowpass, the bypass, underwater, levels (snd_mix.c)
    Shadow,    // VR_SndShadow: the game-time render
    Limit,     // VR_SndLimit and the capture
    CpuRead,   // the voices reading their sounds (resampling, interpolation): summed over threads
    CpuDirect, // the direct effect (occlusion EQ, air)
    CpuBinaural, // the HRTF (or Quake's panning)
    CpuOther,  // gain, near field, copies
    Allocs,    // C++ allocations (operator new; a count, not ms) in the sound's hooks on the main thread and in the voices' tasks
    Count
};

[[nodiscard]] const char* stageName(int s);

extern bool on; // a bench is recording

[[nodiscard]] double now(); // (0 when off)
void add(int stage, double since); // now - since, into this frame's stage (nothing when off or since is 0)
void addSeconds(int stage, double seconds);
void addCount(int stage, double n); // (Allocs: n, where the others have ms)
void painted(int samples, int voices); // this frame's painted samples (VR_SndPaint) and voices

void start(int frames);
void stop();
void frame(); // the last frame's row kept (VR_SndListener, before the new frame's work)

struct Stats
{
    int n{0};          // frames
    double median{0.0}; // ms
    double p95{0.0};
    double p99{0.0};
    double max{0.0};
    double mean{0.0};
    double perSecond{0.0}; // ms per second of sound painted (the load)
};
[[nodiscard]] Stats stats(int stage, int rate);
[[nodiscard]] int frames();
[[nodiscard]] long long samples();
[[nodiscard]] double meanVoices();

} // namespace qvr::audio::bench

namespace qvr::audio::bench
{

// A scope's time into a stage (when a bench is recording).
struct Timed
{
    int stage;
    double since;
    explicit Timed(int s) : stage{s}, since{now()}
    {
    }
    Timed(const Timed&) = delete;
    Timed& operator=(const Timed&) = delete;
    ~Timed()
    {
        add(stage, since);
    }
};

} // namespace qvr::audio::bench
