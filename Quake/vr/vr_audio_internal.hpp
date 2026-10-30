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


// The mixer's state (vr_audio.cpp; its commands: vr_audio_cmds.cpp).

inline constexpr int dynamicChannels = NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS;

// A channel following its entity (vr_snd_follow).
struct Follow
{
    bool active{false};
    int ent{0};
    const qmodel_t* model{nullptr};
    glm::vec3 offset{0.f};
    glm::vec3 last{0.f};
    glm::vec3 vel{0.f};
    double time{0.0};
};

struct Candidate
{
    int channel;
    float priority;
};

// A capture of the final mix (vr_snd_capture).
struct Capture
{
    bool running{false};
    int wanted{0};
    za::String name;
    za::Vector<float> left, right;
    // The effects' sum (VR_SndBus): its loudest sample (dB of its full scale, where Quake clips it) and the samples
    // over it. The whole mix (VR_SndLimit): its loudest sample before the limiter (dB of the output's full scale), the
    // samples cut at full scale, and the limiter's deepest gain.
    int busSeen{0};
    float busPeak{0.f};
    int busOver{0};
    float outPeak{0.f};
    int outClipped{0};
    float minGain{1.f};
};

// The mix's limiter (S_PaintChannels: VR_SndBus, VR_SndLimit): Quake clips the sum of its effects hard at full scale
// (then halves it, for the music's headroom), so loud sounds piling up (explosive boxes blowing up together) crackle
// with the clipping. vr_snd_limiter 1: the effects keep what is over, and a look-ahead peak limiter on the whole mix
// brings it under vr_snd_limiter_ceiling. A sample under it passes unchanged; over it, the gain comes down smoothly
// over the look-ahead (min-hold, then a box average of the same length: the gain is never above what the sample needs
// when it comes out of the delay) and back up over the release.
struct BusLimiter
{
    static constexpr int maxLook = 512;
    static constexpr float fullScale = 32767.f * 256.f; // (paintbuffer's units)
    int look{0};  // samples of look-ahead (the delay, plus one)
    int rate{0};
    float releaseCoef{0.f};
    long long n{0};
    za::Array<float, maxLook> delayL{}, delayR{};
    za::Array<float, maxLook> box{};  // the held gains, for the average
    double boxSum{0.0};
    za::Array<float, maxLook> dqGain{};  // the sliding minimum of the needed gains: a monotonic queue (a ring)
    za::Array<long long, maxLook> dqAt{};
    int dqFront{0};
    int dqCount{0};
    float gain{1.f};
    float minGain{1.f};      // the deepest since vr_snd_info last said
    long long clipped{0};    // samples the bus cut (each side), since the start
    long long over{0};       // samples over full scale coming in
};

// The game-time render's WAV for a recording (vr_timescale_wav; VR_SndShadow, VR_SndGameMix): written as it is made,
// 24-bit stereo, the sizes put in when it ends.
struct GameWav
{
    FILE* file{nullptr};
    za::String path;
    long long frames{0};
    za::Vector<unsigned char> bytes; // (a chunk's, to write)
};

// The game-time render (VR_SndShadow; vr_timescale_wav's WAV, vr_snd_capture_game): the effects mixed a second time,
// at their normal speed on a clock of the game's time, beside the live mix (which slow motion plays slowed and lower,
// and stays so). Its own cursors in the channels' sounds, its own voices (a second mixer: the same HRTF, reverb and
// occlusion, rendering in the game's time), its own filters; the live channels and voices are only read. A sound
// whose live channel ended goes on here to its own end (the live mix's ran a little ahead).
struct ShadowChannel
{
    sfx_t* sfx{nullptr};
    int pos{0};      // in the sound's cache (samples)
    int voice{-1};   // the shadow mixer's voice playing it
    bool done{true};
};

struct Shadow
{
    bool running{false};
    Mixer mixer;
    bool tried{false};
    int rate{0};
    int frame{0};
    za::String sofa;
    int quality{-1};
    double owed{0.0}; // game-time samples due (the output's samples times the time scale), the fraction kept
    za::Array<ShadowChannel, MAX_CHANNELS> channels{};
    za::Array<int, Mixer::maxVoices> channelOf{};
    za::Array<bool, MAX_CHANNELS> started{};  // a new sound on the channel (VR_SndStarted) since the last render
    za::Array<int, MAX_CHANNELS> startPos{};  // its position then (Quake's start offset)
    za::Vector<float> mixL, mixR;
    za::Vector<float> carryL, carryR;
    int carryLen{0};
    AntiAlias antiAlias; // its voices' (before its own 11 kHz lowpass)
    za::Vector<portable_samplepair_t> buffer;
    za::Vector<portable_samplepair_t> held; // its voices, kept out of its 11 kHz lowpass (vr_snd_fullband)
    za::Vector<float> revL, revR;           // their reverb (renderVoices)
    double ms{0.0}; // a paint call's share of it, averaged
};

// vr_snd_bench: the scene it plays while it records (vr_audiobench.hpp has the recorder): combat's sounds started round
// the listener at random (a fixed seed), and the listener taken round a circle (each voice moving against it: Doppler).
struct BenchRun
{
    bool running{false};
    double end{0.0};
    double nextShot{0.0};
    float shotsPerSecond{0.f};
    float orbitSpeed{0.f}; // units a second
    float orbitRadius{96.f};
    double orbitStart{0.0};
    unsigned rng{1};
    za::String label;
    Simulation::Runs runs;
    za::Vector<float> directMs, reflectionsMs; // each simulation run's in the bench
    int bandLimited{0};                         // VR_SndBandLimit's count and time at its start
    double bandLimitMs{0.0};
};

struct Live
{
    Mixer mixer;
    Simulation sim;
    bool tried{false};
    bool ok{false};
    int rate{0};
    int frame{0};
    za::String sofa;
    int quality{-1};
    float simUpm{0.f};

    za::Array<int, MAX_CHANNELS> voiceOf{};
    za::Array<int, Mixer::maxVoices> channelOf{};
    za::Array<unsigned, Mixer::maxVoices> serial{};
    za::Array<unsigned, Mixer::maxVoices> guessSerial{}; // (the voice's sound its first guess is for)
    za::Array<DirectResult, Mixer::maxVoices> guess{};  // occlusion before the simulation's first result
    za::Array<bool, MAX_CHANNELS> fresh{};
    za::Array<bool, MAX_CHANNELS> keep{};
    za::Array<Follow, dynamicChannels> follow{};
    za::Array<Candidate, MAX_CHANNELS> candidates{};
    za::Array<Simulation::Source, Simulation::maxSources> sources{};
    int voicesUsed{0};
    int brushSeen{0};

    Listener listener;
    bool head{false}; // the listener is the headset's
    glm::vec3 lastPos{0.f};
    double lastTime{0.0};

    int worldGen{-1};
    float worldUpm{0.f};
    bool worldNeeded{false};
    IPLReflectionEffectParams reverb{};
    bool haveReverb{false};
    IPLReflectionEffectParams reverbSecond{}; // the game-time render's (Shadow)
    bool haveReverbSecond{false};

    za::Vector<float> mixL, mixR;
    AntiAlias antiAlias; // the voices' (antiAliasWanted)
    za::Vector<portable_samplepair_t> held; // the voices, kept out of Quake's 11 kHz lowpass (vr_snd_fullband: VR_SndBypass)
    za::Vector<float> revL, revR;           // their reverb (renderVoices)
    int heldLen{0};
    za::Array<float, 1024> carryL{}, carryR{};
    int carryStart{0};
    int carryLen{0};

    double mixMs{0.0};   // the voices' render, averaged
    double lastReport{0.0};
    Capture capture;
    BusLimiter bus;
    Capture gameCapture;  // vr_snd_capture_game
    BusLimiter gameBus;   // the game-time mix's own limiter (its capture and WAV)
    GameWav wav;
    Shadow shadow;        // the game-time render (the WAV's and vr_snd_capture_game's mix)
    BenchRun bench;       // vr_snd_bench
};

extern Live* live;
bool wanted();
bool running();
bool writeWav16(const char* path, const za::Vector<float>& l, const za::Vector<float>& r, int rate);

// vr_audio_cmds.cpp.
void info_f();
void startCapture(bool game);
void capture_f();
void captureGame_f();
void benchSpawn_f();
void sceneObj_f();
void play_f();
void dump_f();
void playDir_f();
void burst_f();
void benchFinish();
void benchFrame();
void bench_f();

} // namespace qvr::audio::detail
