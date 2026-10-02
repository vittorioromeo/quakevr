// vr_audio.hpp -- spatial audio over Quake's mixer (snd_dma.c, snd_mix.c): Steam Audio's HRTF from the head's pose,
// occlusion and transmission through the map, the room's reverb; Doppler, the near field, the hands' sounds from the
// hands. See vr_audio.cpp and docs/vr-port/ROUND21.md, "Spatial audio (Steam Audio)".
//
// Quake still picks the channels, their sounds and their volumes (distance falloff and all); the loudest of them
// (vr_snd_voices) are each rendered by a voice here instead of its panning, in Steam Audio's fixed frames, and mixed
// into Quake's paint buffer; the others, the ambient channels and the player's own sounds are painted as ever.
// Everything is off with vr_snd_spatial 0, outside VR at 1, or without phonon.dll: Quake's mixer as it was.
#pragma once

#include "vr_audiosim.hpp"
#include "vr_engine.hpp"
#include "vr_steamaudio.hpp"

#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"


namespace qvr::audio
{

struct Listener
{
    glm::vec3 pos{0.f};
    glm::vec3 fwd{1.f, 0.f, 0.f};
    glm::vec3 right{0.f, -1.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    glm::vec3 vel{0.f}; // units / second
};

// The features, as the settings have them (featuresFromCvars).
struct Features
{
    bool hrtf{true};
    bool bilinear{true};     // the HRTF's interpolation between its measured directions
    float hrtfGain{1.f};
    float occlusion{0.f};    // strength (0 off)
    bool air{false};
    float reverb{0.f};       // wet mix (0 off)
    float doppler{0.f};      // scale (0 off)
    float nearfield{0.f};    // strength (0 off)
    float unitsPerMetre{26.25f};
    float rate{1.f};         // slow motion's playback rate (VR_SndRate: slower and lower; 1 normal)
    bool fullBand{false};    // the voices read their sounds' band-limited copies (vr_snd_fullband; sfxcache_t::fullband)
    bool reverbBeside{true}; // the reverb rendered beside the voices' HRTF, not after it (vr_snd_reverb_beside: the same sound)
};
[[nodiscard]] Features featuresFromCvars();

// A place a little into the map's solid (a contact, an impact) moved out of it towards `head` (vr_audio.cpp).
[[nodiscard]] glm::vec3 outOfSolid(const glm::vec3& at, const glm::vec3& head);

// What a voice plays from where, set each time it renders.
struct VoiceInput
{
    glm::vec3 pos{0.f};
    glm::vec3 vel{0.f};  // units / second (Doppler)
    float gain{0.f};     // Quake's: master volume x distance falloff x sfxvolume (paint buffer units per sample unit)
    bool attached{false}; // moves with the listener (a hand's sound): no Doppler
    bool hasDirect{false};
    DirectResult direct;
};

// Voices rendering mono sounds (Quake's sfxcache_t) to stereo, and the reverb, in Steam Audio's frames.
class AntiAlias;

class Mixer
{
public:
    static constexpr int maxVoices = Simulation::maxSources;
    static constexpr int maxSamples = 2048 + 1024; // a paint call (2048) and the frame rounding up

    Mixer() = default;
    Mixer(const Mixer&) = delete;
    Mixer& operator=(const Mixer&) = delete;
    ~Mixer();

    // `sofa`: an HRTF file (empty: Steam Audio's own).
    bool create(int rate, int frameSize, const char* sofa);
    void destroy();
    [[nodiscard]] bool valid() const
    {
        return hrtf != nullptr;
    }
    [[nodiscard]] int rate() const
    {
        return sampleRate;
    }
    [[nodiscard]] int frameSize() const
    {
        return frame;
    }
    [[nodiscard]] int laneCount() const
    {
        return lanes;
    }
    [[nodiscard]] bool customHrtf() const
    {
        return sofaLoaded;
    }

    // The reverb's algorithm, as the simulation's (made again when it changes).
    void setReverb(IPLReflectionEffectType type, int order, float duration);

    void start(int v, const sfxcache_t* sc, double pos);
    void source(int v, const sfxcache_t* sc)
    {
        voices[v].sc = sc;
    }
    void stop(int v);
    [[nodiscard]] bool active(int v) const
    {
        return voices[v].active;
    }
    [[nodiscard]] bool ended(int v) const
    {
        return voices[v].ended;
    }
    [[nodiscard]] double position(int v) const
    {
        return voices[v].pos;
    }
    [[nodiscard]] float dopplerFactor(int v) const
    {
        return voices[v].doppler;
    }
    [[nodiscard]] const float* equaliser(int v) const
    {
        return voices[v].eq;
    }
    // The direction its HRTF was given last (Steam Audio's axes: x right, y up, -z ahead).
    [[nodiscard]] IPLVector3 direction(int v) const
    {
        return voices[v].dir;
    }
    void set(int v, const VoiceInput& in);
    [[nodiscard]] int activeCount() const;

    // `blocks` frames of every active voice, added to outL/outR (blocks x frameSize samples, paint buffer units).
    // `reverb`: the room's (null: no reverb, its tail ringing out), added to roomL/R when given, else to outL/R.
    // `blocks` frames of every voice added to outL/outR, and the reverb (when on) to roomL/roomR if given (else to
    // outL/outR), `roomFilter` then applied to roomL/roomR (renderVoices' anti-aliasing). The voices go in two passes on
    // the pool: their sounds read and filtered (the reverb's input), each on any thread; then the reverb's convolution
    // (on the calling thread) and its decode (a block behind, on another) while the lanes do the voices' HRTF
    // (vr_snd_bench: the reverb's convolution is the longest single part of the mix).
    void render(int blocks, const Listener& l, const Features& f, const IPLReflectionEffectParams* reverb, float* outL,
        float* outR, float* roomL = nullptr, float* roomR = nullptr, AntiAlias* roomFilter = nullptr);

    // vr_snd_bench (vr_audiobench.hpp): the last render's wall time in the voices and in the reverb (seconds), and the
    // voices' own time in each part (read, direct effect, binaural, the rest) summed over threads since the last take.
    struct Times
    {
        double voices{0.0};
        double reverb{0.0};
        double reverbConv{0.0};
        double reverbDecode{0.0};
        double cpu[5]{}; // (and [4]: the voices' tasks' allocations, a count)
    };
    [[nodiscard]] Times takeTimes();

private:
    struct Voice
    {
        bool active{false};
        bool ended{false};
        const sfxcache_t* sc{nullptr};
        double pos{0.0};
        VoiceInput in;
        bool first{true};
        float gain{0.f};
        float gainTarget{0.f};
        float doppler{1.f};
        float dopplerTarget{1.f};
        float eq[3]{1.f, 1.f, 1.f};
        float eqTarget[3]{1.f, 1.f, 1.f};
        float air[3]{1.f, 1.f, 1.f};
        IPLVector3 dir{0.f, 0.f, -1.f};
        float blend{1.f};
        float lateral{0.f}; // right +1, left -1
        float closeness{0.f}; // the near field's amount (0 far)
        float lp[2]{0.f, 0.f};
        IPLBinauralEffect binaural{nullptr};
        IPLDirectEffect direct{nullptr};
        za::Vector<float> in0, mid, l, r;   // one frame
        za::Vector<float> outL, outR, send; // the call's
        double cpu[5]{};                    // vr_snd_bench: read, direct, binaural, the rest (seconds); allocations
        double lapStart{0.0};               // (vr_snd_bench: the part's start)
        bool bad{false};                    // made not-numbers this call (dropped, its effects reset)
    };

    void prepare(Voice& v, const Listener& l, const Features& f) const;
    void processSource(Voice& v, int blocks, const Features& f);              // read, gain, direct effect: v.send
    void processEars(Voice& v, int blocks, const Features& f, IPLHRTF laneHrtf); // HRTF, near field: v.outL, v.outR
    void reverbConvolve(int blocks, const int* list, int active, const IPLReflectionEffectParams& params);
    void reverbDecode(int blocks, const Features& f, float* toL, float* toR, float* roomL, float* roomR, AntiAlias* roomFilter);
    void read(Voice& v, float* out, float step0, float step1, bool fullBand);

    const steamaudio::Api* sa{nullptr};
    int sampleRate{0};
    int frame{0};
    bool sofaLoaded{false};
    IPLHRTF hrtf{nullptr};
    IPLHRTF reverbHrtf{nullptr}; // the reverb's decode's own copy (it runs beside the lanes)
    // The voices are rendered in lanes (a pool task each, its voices one after another), each lane with an HRTF of
    // its own (the first: `hrtf`): Steam Audio's bilinear interpolation works in the HRTF's own buffers, so two voices
    // interpolating one HRTF at once on two threads made not-numbers (the author's crackling, ROUND21.md).
    static constexpr int maxLanes = 8;
    static constexpr int lanesBesideReverb = 4; // (render: the lanes while the reverb's convolution runs)
    za::Array<IPLHRTF, maxLanes> laneHrtfs{};
    int lanes{0};
    za::Array<Voice, maxVoices> voices;

    // The reverb.
    IPLReflectionEffect reflection{nullptr};
    IPLAmbisonicsDecodeEffect decode{nullptr};
    IPLReflectionEffectType reverbType{IPL_REFLECTIONEFFECTTYPE_PARAMETRIC};
    Times times;        // (vr_snd_bench)
    bool timing{false}; // (vr_snd_bench: this render timed, read by its tasks)
    int reverbOrder{-1};
    int reverbChannels{0};
    int irSize{0};
    int reverbSilence{1 << 30}; // samples since the reverb last had input
    za::Vector<float> reverbIn;
    za::Vector<za::Vector<float>> reverbAmbi; // each channel: the call's blocks (the decode a block behind the convolution)
    za::Array<bool, maxSamples / 256 + 1> reverbRan{}; // each block: convolved (not rung out)
    za::Atomic<int> reverbConvolved{0};                // blocks convolved this call (reverbConvolve to reverbDecode)
    double reverbStarted{0.0};                         // (vr_snd_bench)
    za::Vector<float> reverbL, reverbR;
    za::Vector<float> reverbOutL, reverbOutR; // the call's reverb, when it goes to outL/outR (after the voices' sum)
    IPLCoordinateSpace3 orientation{};
};

// The voices' anti-aliasing filter (vr_snd_antialias). Quake's 11 kHz sound (sndspeed 11025, mixed at 44100) low-passes
// the whole mix by keeping every fourth sample and filtering that (snd_mix.c, S_ApplyFilter), so whatever is above
// 5.5 kHz folds down below it. Quake's own sounds (11 kHz, each sample held four times) come back as they were; the
// voices' HRTF output doesn't: its highs, much louder in the nearer ear, fold into the lows and mids of that ear (a
// sound at the side came out up to 12 dB more one-sided below 2 kHz than its HRTF, and the balance changed with where
// the fourth samples fell). A linear-phase low-pass (flat to 5.2 kHz, -6 dB at 5.65, under -70 dB from 6.1 kHz;
// 2.9 ms late) on the voices' mix before it joins Quake's takes out what would fold.
class AntiAlias
{
public:
    static constexpr int taps = 257;
    static constexpr int delay = (taps - 1) / 2; // samples

    void reset();
    // `n` samples of each side in place, as a stream (each call goes on from the last).
    void apply(float* l, float* r, int n);

private:
    void applyOne(float* x, za::Vector<float>& history, int& quiet, int n);

    za::Vector<float> kernel;          // (made at the first call)
    za::Vector<float> histL, histR;    // the last taps - 1 inputs
    za::Vector<float> work;            // history and block
    int quietL{taps}, quietR{taps};    // zeros in a row at the end of the input so far (taps - 1 or more: a silent
                                       // block comes out silent, unfiltered)
};

// Whether Quake low-passes the mix to 11 kHz (sndspeed 11025 at 44100: snd_mix.c), and the voices are to be filtered
// for it (vr_snd_antialias; not with vr_snd_fullband, which keeps the voices out of that lowpass).
[[nodiscard]] bool antiAliasWanted();

// Whether Quake's 11 kHz lowpass is on (sndspeed 11025 at 44100; vr_snd_fullband 2 skips it, its sounds band-limited).
[[nodiscard]] bool quakeLowpassOn();

// The voices' render (Mixer::render) into mixL/mixR (blocks x frame samples, cleared first), filtered for Quake's
// 11 kHz lowpass: with vr_snd_fullband 0, all of it anti-aliased (vr_snd_antialias), to go through that lowpass; else
// the direct sound as it is and the reverb anti-aliased (revL/revR its room; as before: its own highs above 5.5 kHz,
// even from Quake's 11 kHz sounds, are Steam Audio's parametric reverb's, up to -32 dB of it), to go round it.
void renderVoices(Mixer& m, int blocks, const Listener& l, const Features& f, const IPLReflectionEffectParams* reverb,
    za::Vector<float>& mixL, za::Vector<float>& mixR, za::Vector<float>& revL, za::Vector<float>& revR, AntiAlias& aa);

// A sound's band-limited copy (sfxcache_t::fullband: snd_mem.c, VR_SndBandLimit), or null when it is at the mix's rate.
[[nodiscard]] const short* fullBandData(const sfxcache_t* sc);

// A band-limited copy (`length` samples, its loop from `loop`, -1 none) at a fractional position, windowed-sinc
// interpolated (slow motion and Doppler read it so; vr_audio.cpp, VR_SndFullBandAt). At the copy's scale.
[[nodiscard]] float fullBandAt(const short* x, int length, int loop, double pos);
void makeInterpTable(); // (init)

// Levels of a stereo signal, in dB (full scale 32768): all of it, each side, below 500 Hz and above 4 kHz.
struct Levels
{
    float rms{-200.f};
    float left{-200.f};
    float right{-200.f};
    float low{-200.f};
    float high{-200.f};
};
[[nodiscard]] Levels measure(const float* l, const float* r, int n, int rate);

// Offline renders and measurements (vr_audiotest.cpp): vr_snd_test.
void test_f();

// VR_Init (the commands), VR_Shutdown (Steam Audio's objects, the library).
void init();
void shutdown();

// The game-time render's WAV (vr_timescale_wav; the highlights log's sync marks): a new file from the next sample
// rendered (the last one finished), and the end of it. Nothing without sound.
void startGameWav(const char* path);
void stopGameWav();

} // namespace qvr::audio
