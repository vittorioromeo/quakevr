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

#include <array>
#include <vector>

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
};
[[nodiscard]] Features featuresFromCvars();

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
    void set(int v, const VoiceInput& in);
    [[nodiscard]] int activeCount() const;

    // `blocks` frames of every active voice, added to outL/outR (blocks x frameSize samples, paint buffer units).
    // `reverb`: the room's (null: no reverb, its tail ringing out).
    void render(int blocks, const Listener& l, const Features& f, const IPLReflectionEffectParams* reverb, float* outL,
        float* outR);

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
        std::vector<float> in0, mid, l, r;   // one frame
        std::vector<float> outL, outR, send; // the call's
    };

    void prepare(Voice& v, const Listener& l, const Features& f) const;
    void process(Voice& v, int blocks, const Features& f);
    void read(Voice& v, float* out, float step0, float step1);

    const steamaudio::Api* sa{nullptr};
    int sampleRate{0};
    int frame{0};
    bool sofaLoaded{false};
    IPLHRTF hrtf{nullptr};
    std::array<Voice, maxVoices> voices;

    // The reverb.
    IPLReflectionEffect reflection{nullptr};
    IPLAmbisonicsDecodeEffect decode{nullptr};
    IPLReflectionEffectType reverbType{IPL_REFLECTIONEFFECTTYPE_PARAMETRIC};
    int reverbOrder{-1};
    int reverbChannels{0};
    int irSize{0};
    int reverbSilence{1 << 30}; // samples since the reverb last had input
    std::vector<float> reverbIn;
    std::vector<std::vector<float>> reverbAmbi;
    std::vector<float> reverbL, reverbR;
    IPLCoordinateSpace3 orientation{};
};

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

} // namespace qvr::audio
