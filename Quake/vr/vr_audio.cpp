// vr_audio.cpp -- spatial audio over Quake's mixer; see vr_audio.hpp and docs/vr-port/ROUND21.md, "Spatial audio
// (Steam Audio)".
//
// - The listener (S_Update, VR_SndListener): in VR, the head (hands::State: between the eyes, its angles), so the
//   panning, the ambient levels and the underwater filter (Ironwail's snd_waterfx, by the leaf the listener is in) go
//   by the head, not the body. Its velocity (Doppler) from its moves.
// - The hands' sounds (SND_Spatialize, VR_SndSpatialize): the player's weapon channels (QC VRGetGunChannel:
//   CHAN_WEAPON the main hand, CHAN_WEAPON2 the off hand) play from that hand's muzzle, not from the middle of the
//   head (vr_snd_hands), with Quake's panning too when the voices are off. Other entities' sounds follow their entity
//   while it moves (vr_snd_follow: the offset they started at kept; frozen once it stops being sent, jumps or changes
//   model), which gives them a velocity for the Doppler.
// - Voices (S_PaintChannels, VR_SndPaint): each paint call, the channels are ranked by Quake's own loudness (master
//   volume by distance falloff) and the loudest vr_snd_voices take a voice (a channel keeps its voice: half again
//   louder to lose it). A voice reads the channel's samples itself (Doppler: at a changing rate, interpolated), and
//   keeps the channel's pos and end as Quake's would be, so that a channel can go back to Quake at any time. The
//   paint calls come in whole frames (VR_SndMixEnd rounds the mix-ahead down to a frame; a remainder is carried).
//   Voices render on the game's thread pool (one task a voice; summed in their order after), then the reverb.
// - Per voice: Steam Audio's direct effect (occlusion and transmission as a 3-band EQ, the air's absorption), the
//   binaural effect (HRTF; vr_snd_hrtf 0: Quake's panning), the near field (a source within a metre of the head: the
//   nearer ear louder and the farther one shadowed, by how much it is to the side), a send to the reverb.
// - The reverb: Steam Audio's reflections simulated from the listener (vr_audiosim.cpp), its effect on the sum of the
//   voices' sends, decoded binaurally with the head's orientation, vr_snd_reverb loud.
// - Quake's 11 kHz lowpass (sndspeed 11025): by default (vr_snd_fullband 1) the voices go round it (held out of the
//   paint buffer and added after it: VR_SndBypass), each reading its sound's band-limited copy (VR_SndBandLimit, made
//   as it loads; between samples windowed-sinc: fullBandAt), not Quake's held samples, whose images would be hiss; their
//   reverb anti-aliased (renderVoices). With vr_snd_fullband 0 the voices' sum (and the reverb) is low-passed before it
//   joins Quake's paint buffer (AntiAlias, vr_snd_antialias): that lowpass keeps every fourth sample, which folded the
//   HRTF's highs into the lows.
// - The simulations (vr_audiosim.hpp): the map's scene built on the pool at each new map; doors and lifts moved in
//   it; the direct paths 30 times a second and the reverb every vr_snd_reverb_interval, each a pool task, their
//   results taken under a mutex.

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

namespace qvr::audio
{

// ----------------------------------------------------------------------------
// Settings

Features featuresFromCvars()
{
    Features f;
    f.hrtf = vr_snd_hrtf.value != 0.f;
    f.bilinear = vr_snd_hrtf_interp.value != 0.f;
    f.hrtfGain = za::clamp(vr_snd_hrtf_gain.value, 0.f, 4.f);
    f.occlusion = za::clamp(vr_snd_occlusion.value, 0.f, 2.f);
    f.air = vr_snd_air.value != 0.f;
    f.reverb = za::clamp(vr_snd_reverb.value, 0.f, 2.f);
    f.doppler = za::clamp(vr_snd_doppler.value, 0.f, 4.f);
    f.nearfield = za::clamp(vr_snd_nearfield.value, 0.f, 2.f);
    f.unitsPerMetre = units::metresToUnits();
    f.rate = VR_SndRate();
    f.fullBand = VR_SndFullBand() >= 1;
    f.reverbBeside = vr_snd_reverb_beside.value != 0.f;
    return f;
}

namespace
{

struct Quality
{
    IPLReflectionEffectType type;
    int order;
    float duration;
    int rays;
    int bounces;
};

// vr_snd_reverb_quality: 0 parametric (a feedback delay network driven by the simulated decay times: cheapest, no
// echoes), 1 convolution with 2 s of the simulated response, 2 convolution with 3 s from twice the rays and bounces.
// (Steam Audio's hybrid, convolution then parametric, measured no cheaper here than convolution for the one reverb,
// and its tail came out 13-20 dB quieter than either's: vr_snd_test reverb.)
Quality qualityPreset(int q)
{
    switch(za::clamp(q, 0, 2))
    {
        case 0: return {IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, 1, 1.0f, 1024, 8};
        case 2: return {IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, 1, 3.0f, 4096, 32};
        default: return {IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, 1, 2.0f, 2048, 16};
    }
}

constexpr float speedOfSound = 343.f; // m/s

} // namespace

// ----------------------------------------------------------------------------
// Mixer

Mixer::~Mixer()
{
    destroy();
}

bool Mixer::create(int rate, int frameSize, const char* sofa)
{
    destroy();
    sa = steamaudio::api();
    if(!sa)
    {
        return false;
    }
    sampleRate = rate;
    frame = frameSize;
    IPLAudioSettings as{rate, frameSize};
    IPLHRTFSettings hs{};
    hs.type = IPL_HRTFTYPE_DEFAULT;
    hs.volume = 1.f;
    hs.normType = IPL_HRTFNORMTYPE_NONE;
    sofaLoaded = false;
    if(sofa && *sofa)
    {
        IPLHRTFSettings custom = hs;
        custom.type = IPL_HRTFTYPE_SOFA;
        custom.sofaFileName = sofa;
        if(sa->iplHRTFCreate(steamaudio::context(), &as, &custom, &hrtf) == IPL_STATUS_SUCCESS && hrtf)
        {
            sofaLoaded = true;
        }
        else
        {
            hrtf = nullptr;
        }
    }
    if(!hrtf && sa->iplHRTFCreate(steamaudio::context(), &as, &hs, &hrtf) != IPL_STATUS_SUCCESS)
    {
        hrtf = nullptr;
        return false;
    }
    // The lanes' HRTFs: copies of it (as many as the pool's threads, the caller's too, up to maxLanes).
    laneHrtfs[0] = hrtf;
    lanes = 1;
    const int wanted = za::clamp(jobs::workers() + 1, 1, maxLanes);
    while(lanes < wanted)
    {
        IPLHRTFSettings copy = hs;
        if(sofaLoaded)
        {
            copy.type = IPL_HRTFTYPE_SOFA;
            copy.sofaFileName = sofa;
        }
        IPLHRTF h = nullptr;
        if(sa->iplHRTFCreate(steamaudio::context(), &as, &copy, &h) != IPL_STATUS_SUCCESS || !h)
        {
            break;
        }
        laneHrtfs[lanes++] = h;
    }
    {
        IPLHRTFSettings copy = hs;
        if(sofaLoaded)
        {
            copy.type = IPL_HRTFTYPE_SOFA;
            copy.sofaFileName = sofa;
        }
        if(sa->iplHRTFCreate(steamaudio::context(), &as, &copy, &reverbHrtf) != IPL_STATUS_SUCCESS || !reverbHrtf)
        {
            reverbHrtf = nullptr;
            destroy();
            return false;
        }
    }
    for(Voice& v : voices)
    {
        IPLBinauralEffectSettings bs{hrtf};
        IPLDirectEffectSettings ds{1};
        if(sa->iplBinauralEffectCreate(steamaudio::context(), &as, &bs, &v.binaural) != IPL_STATUS_SUCCESS ||
           sa->iplDirectEffectCreate(steamaudio::context(), &as, &ds, &v.direct) != IPL_STATUS_SUCCESS)
        {
            destroy();
            return false;
        }
        v.in0.clear();
        v.in0.resize(frame, 0.f);
        v.mid.clear();
        v.mid.resize(frame, 0.f);
        v.l.clear();
        v.l.resize(frame, 0.f);
        v.r.clear();
        v.r.resize(frame, 0.f);
        v.outL.clear();
        v.outL.resize(maxSamples, 0.f);
        v.outR.clear();
        v.outR.resize(maxSamples, 0.f);
        v.send.clear();
        v.send.resize(maxSamples, 0.f);
        v.active = false;
    }
    reverbOrder = -1;
    return true;
}

void Mixer::destroy()
{
    if(!sa)
    {
        return;
    }
    for(Voice& v : voices)
    {
        if(v.binaural)
        {
            sa->iplBinauralEffectRelease(&v.binaural);
        }
        if(v.direct)
        {
            sa->iplDirectEffectRelease(&v.direct);
        }
        v = Voice{};
    }
    if(reflection)
    {
        sa->iplReflectionEffectRelease(&reflection);
    }
    if(decode)
    {
        sa->iplAmbisonicsDecodeEffectRelease(&decode);
    }
    reflection = nullptr;
    decode = nullptr;
    reverbOrder = -1;
    for(int i = 1; i < lanes; i++)
    {
        sa->iplHRTFRelease(&laneHrtfs[i]);
    }
    laneHrtfs = {};
    lanes = 0;
    if(reverbHrtf)
    {
        sa->iplHRTFRelease(&reverbHrtf);
    }
    reverbHrtf = nullptr;
    if(hrtf)
    {
        sa->iplHRTFRelease(&hrtf);
    }
    hrtf = nullptr;
    sa = nullptr;
}

void Mixer::setReverb(IPLReflectionEffectType type, int order, float duration)
{
    const int size = za::max(frame, static_cast<int>(za::ceil(duration * static_cast<float>(sampleRate))));
    if(!valid() || (reflection && type == reverbType && order == reverbOrder && size == irSize))
    {
        return;
    }
    if(reflection)
    {
        sa->iplReflectionEffectRelease(&reflection);
    }
    if(decode)
    {
        sa->iplAmbisonicsDecodeEffectRelease(&decode);
    }
    reflection = nullptr;
    decode = nullptr;
    reverbType = type;
    reverbOrder = order;
    reverbChannels = (order + 1) * (order + 1);
    irSize = size;
    IPLAudioSettings as{sampleRate, frame};
    IPLReflectionEffectSettings rs{type, irSize, reverbChannels};
    if(sa->iplReflectionEffectCreate(steamaudio::context(), &as, &rs, &reflection) != IPL_STATUS_SUCCESS)
    {
        reflection = nullptr;
        return;
    }
    IPLAmbisonicsDecodeEffectSettings ds{};
    ds.speakerLayout.type = IPL_SPEAKERLAYOUTTYPE_STEREO;
    ds.hrtf = reverbHrtf;
    ds.maxOrder = order;
    if(sa->iplAmbisonicsDecodeEffectCreate(steamaudio::context(), &as, &ds, &decode) != IPL_STATUS_SUCCESS)
    {
        sa->iplReflectionEffectRelease(&reflection);
        reflection = nullptr;
        decode = nullptr;
        return;
    }
    reverbIn.clear();
    reverbIn.resize(frame, 0.f);
    reverbAmbi.clear();
    reverbAmbi.resize(reverbChannels, za::Vector<float>(maxSamples, 0.f));
    reverbL.clear();
    reverbL.resize(frame, 0.f);
    reverbR.clear();
    reverbR.resize(frame, 0.f);
    reverbOutL.clear();
    reverbOutL.resize(maxSamples, 0.f);
    reverbOutR.clear();
    reverbOutR.resize(maxSamples, 0.f);
    reverbSilence = 1 << 30;
}

void Mixer::start(int index, const sfxcache_t* sc, double pos)
{
    Voice& v = voices[index];
    v.active = true;
    v.ended = false;
    v.sc = sc;
    v.pos = za::max(0.0, pos);
    v.first = true;
    v.lp[0] = v.lp[1] = 0.f;
    for(int b = 0; b < 3; b++)
    {
        v.eq[b] = v.eqTarget[b] = v.air[b] = 1.f;
    }
    v.doppler = v.dopplerTarget = 1.f;
    sa->iplBinauralEffectReset(v.binaural);
    sa->iplDirectEffectReset(v.direct);
}

void Mixer::stop(int index)
{
    voices[index].active = false;
    voices[index].sc = nullptr;
}

void Mixer::set(int index, const VoiceInput& in)
{
    voices[index].in = in;
}

void Mixer::prepare(Voice& v, const Listener& l, const Features& f) const
{
    const glm::vec3 d = v.in.pos - l.pos;
    const float dist = glm::length(d);
    const glm::vec3 dir = dist > 0.01f ? d / dist : l.fwd;
    const float x = glm::dot(dir, l.right);
    const float y = glm::dot(dir, l.up);
    const float z = glm::dot(dir, l.fwd);
    v.dir = IPLVector3{x, y, -z};
    v.lateral = x;
    const float metres = dist / f.unitsPerMetre;
    v.blend = za::clamp(metres / 0.1f, 0.f, 1.f); // (inside the head: towards the middle)
    v.gainTarget = za::max(0.f, v.in.gain);

    v.dopplerTarget = 1.f;
    if(f.doppler > 0.f && !v.in.attached)
    {
        const float c = speedOfSound * f.unitsPerMetre;
        const float vs = za::clamp(glm::dot(v.in.vel, dir) * f.doppler, -0.5f * c, 0.5f * c);
        const float vl = za::clamp(glm::dot(l.vel, dir) * f.doppler, -0.5f * c, 0.5f * c);
        v.dopplerTarget = za::clamp((c + vl) / (c + vs), 0.5f, 2.f);
    }
    v.dopplerTarget *= f.rate * v.in.pitch; // slow motion: read slower (lower), the rate eased as the Doppler's; a pitch

    v.closeness = f.nearfield > 0.f ? za::clamp((1.f - metres) / 0.9f, 0.f, 1.f) * f.nearfield : 0.f;

    for(int b = 0; b < 3; b++)
    {
        v.eqTarget[b] = 1.f;
        v.air[b] = 1.f;
    }
    if(v.in.hasDirect && f.occlusion > 0.f)
    {
        // What reaches the ear: the open part, and the hidden part through the wall; the strength an exponent (0.5:
        // half as many dB), so that a weaker setting keeps it duller, not just louder.
        const float occ = v.in.direct.occlusion;
        for(int b = 0; b < 3; b++)
        {
            const float e = occ + (1.f - occ) * v.in.direct.transmission[b];
            v.eqTarget[b] = za::pow(za::max(e, 1e-4f), f.occlusion);
        }
    }
    if(v.in.hasDirect && f.air)
    {
        for(int b = 0; b < 3; b++)
        {
            v.air[b] = v.in.direct.air[b];
        }
    }
    if(v.first)
    {
        v.first = false;
        v.gain = v.gainTarget;
        v.doppler = v.dopplerTarget;
        za::copy(v.eqTarget, v.eqTarget + 3, v.eq);
    }
}

void Mixer::read(Voice& v, float* out, float step0, float step1, bool fullBand)
{
    const int n = frame;
    const sfxcache_t* sc = v.sc;
    if(v.ended || !sc || sc->length <= 0)
    {
        za::fill(out, out + n, 0.f);
        v.ended = true;
        return;
    }
    const int length = sc->length;
    const int loop = sc->loopstart;
    const bool wide = sc->width == 2;
    // The band-limited copy (vr_snd_fullband: the voices not low-passed by Quake), when it was resampled.
    const short* full = fullBand ? fullBandData(sc) : nullptr;
    const auto sample = [&](int i) -> float {
        if(full)
        {
            return static_cast<float>(full[i] * S_FULLBAND_SCALE);
        }
        return wide ? static_cast<float>(reinterpret_cast<const short*>(sc->data)[i])
                    : static_cast<float>(static_cast<signed char>(sc->data[i])) * 256.f;
    };
    const double integral = za::floor(v.pos);
    if(step0 == 1.f && step1 == 1.f && v.pos == integral)
    {
        int p = static_cast<int>(v.pos);
        for(int i = 0; i < n; i++)
        {
            if(p >= length)
            {
                if(loop < 0 || loop >= length)
                {
                    za::fill(out + i, out + n, 0.f);
                    v.ended = true;
                    v.pos = length;
                    return;
                }
                p = loop;
            }
            out[i] = sample(p++);
        }
        v.pos = p;
        return;
    }
    double pos = v.pos;
    for(int i = 0; i < n; i++)
    {
        if(pos >= length)
        {
            if(loop < 0 || loop >= length)
            {
                za::fill(out + i, out + n, 0.f);
                v.ended = true;
                v.pos = length;
                return;
            }
            pos = loop + za::fmod(pos - length, static_cast<double>(length - loop));
        }
        if(full) // (band-limited: read so, not linearly, or its highs' images come back)
        {
            out[i] = fullBandAt(full, length, loop, pos) * S_FULLBAND_SCALE;
        }
        else
        {
            const int i0 = static_cast<int>(pos);
            const float frac = static_cast<float>(pos - i0);
            const int i1 = i0 + 1 < length ? i0 + 1 : (loop >= 0 ? loop : i0);
            const float s0 = sample(i0);
            out[i] = s0 + (sample(i1) - s0) * frac;
        }
        pos += step0 + (step1 - step0) * (static_cast<float>(i) / static_cast<float>(n));
    }
    v.pos = pos;
}

namespace
{

// vr_snd_bench: the time since the voice's last lap into its part `part` (when this render is timed).
void lap(bool timed, double& since, double* cpu, int part)
{
    if(timed)
    {
        const double now = Sys_DoubleTime();
        cpu[part] += now - since;
        since = now;
    }
}

} // namespace

void Mixer::processSource(Voice& v, int blocks, const Features& f)
{
    const int n = frame;
    const bool timed = timing; // (vr_snd_bench)
    v.lapStart = timed ? Sys_DoubleTime() : 0.0;
    const za::U64 allocs = timed ? alloccount::thisThread() : 0;
    for(int b = 0; b < blocks; b++)
    {
        // Doppler: the rate eased towards its target (half the way each frame), ramped within the frame.
        const float step0 = v.doppler;
        const float step1 = v.doppler + (v.dopplerTarget - v.doppler) * 0.5f;
        read(v, v.in0.data(), step0, step1, f.fullBand);
        v.doppler = step1;
        lap(timed, v.lapStart, v.cpu, 0);

        // The volume ramped to its new value over the first frame.
        const float g0 = v.gain;
        const float g1 = v.gainTarget;
        if(b == 0 && g0 != g1)
        {
            for(int i = 0; i < n; i++)
            {
                v.in0[i] *= g0 + (g1 - g0) * (static_cast<float>(i + 1) / static_cast<float>(n));
            }
        }
        else
        {
            for(int i = 0; i < n; i++)
            {
                v.in0[i] *= g1;
            }
        }
        v.gain = g1;

        float* src = v.in0.data();
        bool filtered = false;
        for(int k = 0; k < 3; k++)
        {
            v.eq[k] += (v.eqTarget[k] - v.eq[k]) * 0.35f;
            filtered = filtered || v.eq[k] < 0.999f || v.air[k] < 0.999f;
        }
        if(filtered)
        {
            IPLDirectEffectParams p{};
            p.flags = static_cast<IPLDirectEffectFlags>(IPL_DIRECTEFFECTFLAGS_APPLYOCCLUSION |
                                                        IPL_DIRECTEFFECTFLAGS_APPLYTRANSMISSION |
                                                        IPL_DIRECTEFFECTFLAGS_APPLYAIRABSORPTION);
            p.transmissionType = IPL_TRANSMISSIONTYPE_FREQDEPENDENT;
            p.distanceAttenuation = 1.f;
            p.directivity = 1.f;
            p.occlusion = 0.f; // all of it through the "transmission": the EQ worked out in prepare
            for(int k = 0; k < 3; k++)
            {
                p.transmission[k] = v.eq[k];
                p.airAbsorption[k] = v.air[k];
            }
            float* inPtr[1] = {v.in0.data()};
            float* outPtr[1] = {v.mid.data()};
            IPLAudioBuffer in{1, n, inPtr};
            IPLAudioBuffer out{1, n, outPtr};
            sa->iplDirectEffectApply(v.direct, &p, &in, &out);
            src = v.mid.data();
        }
        za::copy(src, src + n, v.send.data() + b * n);
        lap(timed, v.lapStart, v.cpu, 1);
    }
    // Not-numbers (it never should make them) kept out of the reverb: they would stay in its convolution and crackle on
    // and on. The voice is dropped from this call (render).
    float check[4]{};
    const float* send = v.send.data();
    const int count = blocks * n;
    int i = 0;
    for(; i + 4 <= count; i += 4)
    {
        for(int k = 0; k < 4; k++)
        {
            check[k] += send[i + k] * 0.f;
        }
    }
    for(; i < count; i++)
    {
        check[0] += send[i] * 0.f;
    }
    v.bad = (check[0] + check[1]) + (check[2] + check[3]) != 0.f;
    if(v.bad)
    {
        za::fill(v.send.begin(), v.send.begin() + count, 0.f);
    }
    if(timed)
    {
        v.cpu[4] += static_cast<double>(alloccount::thisThread() - allocs);
    }
}

void Mixer::processEars(Voice& v, int blocks, const Features& f, IPLHRTF laneHrtf)
{
    const int n = frame;
    const bool timed = timing; // (vr_snd_bench)
    v.lapStart = timed ? Sys_DoubleTime() : 0.0;
    const za::U64 allocs = timed ? alloccount::thisThread() : 0;
    for(int b = 0; b < blocks; b++)
    {
        float* src = v.send.data() + b * n; // (the direct sound: the reverb's input too)
        if(f.hrtf)
        {
            IPLBinauralEffectParams p{};
            p.direction = v.dir;
            p.interpolation = f.bilinear ? IPL_HRTFINTERPOLATION_BILINEAR : IPL_HRTFINTERPOLATION_NEAREST;
            p.spatialBlend = v.blend;
            p.hrtf = laneHrtf;
            p.peakDelays = nullptr;
            float* inPtr[1] = {src};
            float* outPtr[2] = {v.l.data(), v.r.data()};
            IPLAudioBuffer in{1, n, inPtr};
            IPLAudioBuffer out{2, n, outPtr};
            sa->iplBinauralEffectApply(v.binaural, &p, &in, &out);
            if(f.hrtfGain != 1.f)
            {
                for(int i = 0; i < n; i++)
                {
                    v.l[i] *= f.hrtfGain;
                    v.r[i] *= f.hrtfGain;
                }
            }
        }
        else
        {
            // Quake's panning (SND_Spatialize).
            const float gl = 1.f - v.lateral;
            const float gr = 1.f + v.lateral;
            for(int i = 0; i < n; i++)
            {
                v.l[i] = src[i] * gl;
                v.r[i] = src[i] * gr;
            }
        }
        lap(timed, v.lapStart, v.cpu, 2);

        // The near field: the nearer ear louder, the farther one quieter and duller (the head's shadow), by how near
        // (within a metre) and how much to the side.
        const float amount = za::min(1.f, v.closeness * za::abs(v.lateral));
        const int contra = v.lateral >= 0.f ? 0 : 1; // the far ear: the left for a sound on the right
        float* nearEar = contra == 0 ? v.r.data() : v.l.data();
        float* farEar = contra == 0 ? v.l.data() : v.r.data();
        if(amount > 0.001f)
        {
            const float db = 12.f * amount;
            const float gi = za::pow(10.f, db / 40.f);
            const float gc = za::pow(10.f, -db / 40.f);
            const float fc = za::max(1500.f, 20000.f * (1.f - 0.85f * amount));
            const float a = 1.f - za::exp(-2.f * 3.14159265f * fc / static_cast<float>(sampleRate));
            float lp = v.lp[contra];
            for(int i = 0; i < n; i++)
            {
                nearEar[i] *= gi;
                lp += a * (farEar[i] - lp);
                farEar[i] = lp * gc;
            }
            v.lp[contra] = lp;
        }
        else
        {
            v.lp[contra] = farEar[n - 1];
        }
        v.lp[1 - contra] = nearEar[n - 1];

        za::copy(v.l.data(), v.l.data() + n, v.outL.data() + b * n);
        za::copy(v.r.data(), v.r.data() + n, v.outR.data() + b * n);
        lap(timed, v.lapStart, v.cpu, 3);
    }
    // Not-numbers (it never should make them): the voice is dropped from this call and its effects reset (render), or
    // they would stay in the HRTF's convolution and crackle on and on.
    float check[4]{};
    const int count = blocks * n;
    const float* l = v.outL.data();
    const float* r = v.outR.data();
    int i = 0;
    for(; i + 4 <= count; i += 4)
    {
        for(int k = 0; k < 4; k++)
        {
            check[k] += l[i + k] * 0.f + r[i + k] * 0.f;
        }
    }
    for(; i < count; i++)
    {
        check[0] += l[i] * 0.f + r[i] * 0.f;
    }
    v.bad = v.bad || (check[0] + check[1]) + (check[2] + check[3]) != 0.f;
    if(timed)
    {
        v.cpu[4] += static_cast<double>(alloccount::thisThread() - allocs);
    }
}

namespace
{
// (Their parallelFor calls: vr_jobs_sites.)
jobs::Site audioSources{"audio sources"};
jobs::Site audioLanes{"audio lanes"};
jobs::Site audioTaps{"audio band-limit taps"};
jobs::Site audioBandlimit{"audio band-limit"};
} // namespace

Mixer::Times Mixer::takeTimes()
{
    Times out = times;
    for(Voice& v : voices)
    {
        for(int k = 0; k < 5; k++)
        {
            out.cpu[k] += v.cpu[k];
            v.cpu[k] = 0.0;
        }
    }
    times = Times{};
    return out;
}

// The reverb in two tasks one block behind the other: its convolution (reverbConvolve: the voices' sends summed, the
// reflection effect into reverbAmbi's block) and its decode (reverbDecode: the ambisonics decoded to the ears, added at
// its wet mix to toL/toR, then `roomFilter` on that block of roomL/roomR). The decode waits for each block's convolution
// (reverbConvolved); the convolution never waits, and parallelFor hands out its tasks in order (the convolution's, task 0,
// is taken first), so the wait always ends. Run on one thread, one after the other, it is the same.
void Mixer::reverbConvolve(int blocks, const int* list, int active, const IPLReflectionEffectParams& params)
{
    reverbStarted = timing ? Sys_DoubleTime() : 0.0;
    IPLReflectionEffectParams p = params; // (Steam Audio's takes it mutable: one copy for the call, as it was)
    for(int b = 0; b < blocks; b++)
    {
        bool silent = true;
        za::fill(reverbIn, 0.f);
        for(int k = 0; k < active; k++)
        {
            const float* s = voices[list[k]].send.data() + b * frame;
            for(int i = 0; i < frame; i++)
            {
                reverbIn[i] += s[i];
            }
            silent = false;
        }
        // Nothing in for longer than the response: the tail has rung out (the effect keeps saying it hasn't).
        reverbSilence = silent ? za::min(reverbSilence + frame, 1 << 30) : 0;
        reverbRan[b] = reverbSilence <= irSize + frame;
        if(reverbRan[b])
        {
            float* ambiPtr[16]{};
            for(int c = 0; c < reverbChannels && c < 16; c++)
            {
                ambiPtr[c] = reverbAmbi[c].data() + b * frame;
            }
            float* inPtr[1] = {reverbIn.data()};
            IPLAudioBuffer in{1, frame, inPtr};
            IPLAudioBuffer ambi{reverbChannels, frame, ambiPtr};
            const double convStart = timing ? Sys_DoubleTime() : 0.0;
            sa->iplReflectionEffectApply(reflection, &p, &in, &ambi, nullptr);
            times.reverbConv += timing ? Sys_DoubleTime() - convStart : 0.0;
        }
        reverbConvolved.storeRelease(b + 1);
    }
}

void Mixer::reverbDecode(int blocks, const Features& f, float* toL, float* toR, float* roomL, float* roomR,
    AntiAlias* roomFilter)
{
    for(int b = 0; b < blocks; b++)
    {
        while(reverbConvolved.loadAcquire() <= b)
        {
            _mm_pause();
        }
        if(reverbRan[b])
        {
            float* ambiPtr[16]{};
            for(int c = 0; c < reverbChannels && c < 16; c++)
            {
                ambiPtr[c] = reverbAmbi[c].data() + b * frame;
            }
            IPLAudioBuffer ambi{reverbChannels, frame, ambiPtr};
            const double decodeStart = timing ? Sys_DoubleTime() : 0.0;
            IPLAmbisonicsDecodeEffectParams dp{};
            dp.order = reverbOrder;
            dp.hrtf = reverbHrtf;
            dp.orientation = orientation;
            dp.binaural = f.hrtf ? IPL_TRUE : IPL_FALSE;
            float* stereoPtr[2] = {reverbL.data(), reverbR.data()};
            IPLAudioBuffer stereo{2, frame, stereoPtr};
            sa->iplAmbisonicsDecodeEffectApply(decode, &dp, &ambi, &stereo);
            times.reverbDecode += timing ? Sys_DoubleTime() - decodeStart : 0.0;
            const float mix = f.reverb;
            for(int i = 0; i < frame; i++)
            {
                toL[b * frame + i] += reverbL[i] * mix;
                toR[b * frame + i] += reverbR[i] * mix;
            }
        }
        if(roomFilter && roomL)
        {
            roomFilter->apply(roomL + b * frame, roomR + b * frame, frame);
        }
    }
    times.reverb += timing ? Sys_DoubleTime() - reverbStarted : 0.0;
}

void Mixer::render(int blocks, const Listener& l, const Features& f, const IPLReflectionEffectParams* reverb,
    float* outL, float* outR, float* roomL, float* roomR, AntiAlias* roomFilter)
{
    if(!valid() || blocks <= 0)
    {
        if(roomFilter && roomL && blocks > 0)
        {
            roomFilter->apply(roomL, roomR, blocks * frame);
        }
        return;
    }
    blocks = za::min(blocks, maxSamples / frame);
    const int count = blocks * frame;
    timing = bench::on;
    const double voicesStart = timing ? Sys_DoubleTime() : 0.0;
    int list[maxVoices];
    int active = 0;
    for(int i = 0; i < maxVoices; i++)
    {
        if(voices[i].active)
        {
            prepare(voices[i], l, f);
            list[active++] = i;
        }
    }
    const bool reverbOn = f.reverb > 0.f && reflection && decode && reverb;
    if(!reverbOn)
    {
        reverbSilence = 1 << 30;
    }
    // The reverb into the room's buffers, or its own (added to outL/outR after the voices' sum, as it was).
    float* revToL = roomL ? roomL : reverbOutL.data();
    float* revToR = roomR ? roomR : reverbOutR.data();
    IPLReflectionEffectParams params{};
    if(reverbOn)
    {
        params = *reverb;
        params.type = reverbType;
        params.numChannels = reverbChannels;
        params.irSize = irSize;
        orientation = coordinates(l.pos, l.fwd, l.right, l.up, f.unitsPerMetre);
        if(!roomL)
        {
            za::fill(reverbOutL.begin(), reverbOutL.begin() + count, 0.f);
            za::fill(reverbOutR.begin(), reverbOutR.begin() + count, 0.f);
        }
    }
    bool filtered = false; // (roomFilter applied)
    // First the sources (read, gain, direct effect: the reverb's input), each voice a task of its own on any of the
    // pool's threads (no HRTF in them). Then the ears: lane j renders the voices j, j + lanes, ... with its own HRTF
    // (never two threads on one); beside them the reverb (task 0, the longest: started first) and the room's filter
    // after it on its thread.
    if(active == 1)
    {
        processSource(voices[list[0]], blocks, f);
    }
    else if(active > 1)
    {
        jobs::parallelFor(audioSources, static_cast<za::SizeT>(active), 1, [&](za::SizeT begin, za::SizeT end) {
            for(za::SizeT k = begin; k < end; k++)
            {
                processSource(voices[list[k]], blocks, f);
            }
        });
    }
    const int used = za::min(active, lanes);
    // A pass: the reverb (its convolution, then its decode a block behind) and/or the lanes (the voices' HRTF).
    const auto pass = [&](bool withReverb, bool withLanes) {
        reverbConvolved.storeRelaxed(0);
        // (Beside the reverb, at most lanesBesideReverb lanes: more of them slow the reverb's convolution down, the
        // longest part, more than they save; vr_snd_bench: its p95 0.70 -> 0.56 ms with 4 of the 8.)
        const int laneCount = withLanes ? (withReverb ? za::min(used, lanesBesideReverb) : used) : 0;
        const int units = laneCount + (withReverb ? 2 : 0);
        if(units <= 1 || active == 0)
        {
            if(withReverb)
            {
                reverbConvolve(blocks, list, active, params);
                reverbDecode(blocks, f, revToL, revToR, roomL, roomR, roomFilter);
                filtered = true;
            }
            for(int k = 0; k < active && laneCount > 0; k++)
            {
                processEars(voices[list[k]], blocks, f, laneHrtfs[0]);
            }
            return;
        }
        // Each task one part, whichever is left: the calling thread takes the reverb's convolution first (the longest
        // single part: on the thread that waits for them all anyway, not on a slower core of the pool's), a helper its
        // decode once the convolution is taken (it waits for each block), the others a lane each. The decode is never
        // taken before the convolution, which never waits: its wait always ends.
        const za::ThreadId caller = za::ThisThread::getId();
        za::Atomic<int> convTaken{withReverb ? 0 : 1};
        za::Atomic<int> decodeTaken{withReverb ? 0 : 1};
        za::Atomic<int> nextLane{0};
        const auto takeConv = [&]() {
            if(convTaken.loadAcquire() != 0 || convTaken.exchangeSeqCst(1) != 0)
            {
                return false;
            }
            reverbConvolve(blocks, list, active, params);
            return true;
        };
        const auto takeDecode = [&]() {
            if(convTaken.loadAcquire() == 0 || decodeTaken.loadAcquire() != 0 || decodeTaken.exchangeSeqCst(1) != 0)
            {
                return false;
            }
            reverbDecode(blocks, f, revToL, revToR, roomL, roomR, roomFilter);
            return true;
        };
        const auto takeLane = [&]() {
            const int lane = nextLane.fetchAddRelaxed(1);
            if(lane >= laneCount)
            {
                return false;
            }
            for(int k = lane; k < active; k += laneCount)
            {
                processEars(voices[list[k]], blocks, f, laneHrtfs[lane]);
            }
            return true;
        };
        jobs::parallelFor(audioLanes, static_cast<za::SizeT>(units), 1, [&](za::SizeT begin, za::SizeT end) {
            for(za::SizeT j = begin; j < end; j++)
            {
                bool took = false;
                if(za::ThisThread::getId() == caller)
                {
                    took = takeConv() || takeLane() || takeDecode();
                }
                else
                {
                    took = takeDecode() || takeLane() || takeConv() || takeDecode();
                }
                ZA_ASSERT(took);
                (void)took;
            }
        });
        filtered = filtered || withReverb;
    };
    if(reverbOn && !f.reverbBeside)
    {
        pass(false, true); // (the lanes, then the reverb alone: vr_snd_reverb_beside 0)
        pass(true, false);
    }
    else
    {
        pass(reverbOn, true);
    }
    for(int k = 0; k < active; k++)
    {
        Voice& v = voices[list[k]];
        // A voice that made not-numbers (it never should) is dropped from this call and its effects reset: one would
        // stay in the reverb's and the HRTF's convolutions and crackle on and on.
        if(v.bad)
        {
            sa->iplBinauralEffectReset(v.binaural);
            sa->iplDirectEffectReset(v.direct);
            za::fill(v.send.begin(), v.send.begin() + count, 0.f);
            continue;
        }
        for(int i = 0; i < count; i++)
        {
            outL[i] += v.outL[i];
            outR[i] += v.outR[i];
        }
    }
    if(reverbOn && !roomL)
    {
        for(int i = 0; i < count; i++)
        {
            outL[i] += reverbOutL[i];
            outR[i] += reverbOutR[i];
        }
    }
    if(timing)
    {
        times.voices += Sys_DoubleTime() - voicesStart - times.reverb;
    }
    if(roomFilter && roomL && !filtered)
    {
        roomFilter->apply(roomL, roomR, count);
    }
}

// ----------------------------------------------------------------------------
// The voices' anti-aliasing (before Quake's 11 kHz lowpass)

bool quakeLowpassOn()
{
    return shm && sndspeed.value == 11025 && shm->speed == 44100 && VR_SndFullBand() < 2;
}

bool antiAliasWanted()
{
    return vr_snd_antialias.value != 0.f && VR_SndFullBand() == 0 && quakeLowpassOn();
}

void renderVoices(Mixer& m, int blocks, const Listener& l, const Features& f, const IPLReflectionEffectParams* reverb,
    za::Vector<float>& mixL, za::Vector<float>& mixR, za::Vector<float>& revL, za::Vector<float>& revR, AntiAlias& aa)
{
    const int rendered = blocks * m.frameSize();
    za::fill(mixL.begin(), mixL.begin() + rendered, 0.f);
    za::fill(mixR.begin(), mixR.begin() + rendered, 0.f);
    const bool fullBand = VR_SndFullBand() >= 1;
    const bool lowpass = shm && sndspeed.value == 11025 && shm->speed == 44100; // (Quake's, or skipped by mode 2)
    if(fullBand && lowpass)
    {
        za::fill(revL.begin(), revL.begin() + rendered, 0.f);
        za::fill(revR.begin(), revR.begin() + rendered, 0.f);
        m.render(blocks, l, f, reverb, mixL.data(), mixR.data(), revL.data(), revR.data(), &aa); // (aa: after the reverb, on its thread)
        for(int i = 0; i < rendered; i++)
        {
            mixL[i] += revL[i];
            mixR[i] += revR[i];
        }
        return;
    }
    m.render(blocks, l, f, reverb, mixL.data(), mixR.data());
    if(antiAliasWanted())
    {
        aa.apply(mixL.data(), mixR.data(), rendered);
    }
    else
    {
        aa.reset();
    }
}

const short* fullBandData(const sfxcache_t* sc)
{
    return sc->fullband != 0 ? reinterpret_cast<const short*>(sc->data + sc->fullband) : nullptr;
}

void AntiAlias::reset()
{
    za::fill(histL, 0.f);
    za::fill(histR, 0.f);
    quietL = quietR = taps;
}

void AntiAlias::apply(float* l, float* r, int n)
{
    if(kernel.empty())
    {
        // A Blackman-windowed sinc at 5650 Hz of 44100 (its sum 1).
        kernel.resize(taps, 0.f);
        constexpr double fc = 5650.0 / 44100.0;
        constexpr double pi = 3.14159265358979;
        double sum = 0.0;
        for(int k = 0; k < taps; k++)
        {
            const double t = k - delay;
            const double sinc = t == 0.0 ? 2.0 * fc : za::sin(2.0 * pi * fc * t) / (pi * t);
            const double w = 0.42 - 0.5 * za::cos(2.0 * pi * k / (taps - 1)) + 0.08 * za::cos(4.0 * pi * k / (taps - 1));
            kernel[k] = static_cast<float>(sinc * w);
            sum += sinc * w;
        }
        for(float& c : kernel)
        {
            c = static_cast<float>(c / sum);
        }
        histL.resize(taps - 1, 0.f);
        histR.resize(taps - 1, 0.f);
    }
    applyOne(l, histL, quietL, n);
    applyOne(r, histR, quietR, n);
}

void AntiAlias::applyOne(float* x, za::Vector<float>& history, int& quiet, int n)
{
    const int h = taps - 1;
    // Silence in, after silence for the filter's length (the reverb rung out, no voices): silence out, as it is.
    int trailing = 0;
    while(trailing < n && x[n - 1 - trailing] == 0.f)
    {
        trailing++;
    }
    const int quietBefore = quiet;
    quiet = trailing == n ? za::min(quiet + n, 1 << 30) : trailing;
    if(trailing == n && quietBefore >= h)
    {
        return; // (the history: zeros, as it would have been)
    }
    if(static_cast<int>(work.size()) < h + n)
    {
        work.resize(h + n, 0.f);
    }
    za::copy(history.begin(), history.end(), work.begin());
    za::copy(x, x + n, work.begin() + h);
    const float* c = kernel.data();
    int i = 0;
    // Four outputs at a time, each lane the scalar loop's own sums in its order (the same results, bit for bit).
    for(; i + 4 <= n; i += 4)
    {
        const float* in = work.data() + i;
        __m128 s[8];
        for(int j = 0; j < 8; j++)
        {
            s[j] = _mm_setzero_ps();
        }
        int k = 0;
        for(; k + 8 <= taps; k += 8)
        {
            for(int j = 0; j < 8; j++)
            {
                s[j] = _mm_add_ps(s[j], _mm_mul_ps(_mm_set1_ps(c[k + j]), _mm_loadu_ps(in + k + j)));
            }
        }
        __m128 y = _mm_add_ps(_mm_add_ps(_mm_add_ps(s[0], s[1]), _mm_add_ps(s[2], s[3])),
            _mm_add_ps(_mm_add_ps(s[4], s[5]), _mm_add_ps(s[6], s[7])));
        for(; k < taps; k++)
        {
            y = _mm_add_ps(y, _mm_mul_ps(_mm_set1_ps(c[k]), _mm_loadu_ps(in + k)));
        }
        _mm_storeu_ps(x + i, y);
    }
    for(; i < n; i++)
    {
        // (symmetric: the kernel the same reversed; eight sums, for the compiler to keep in lanes)
        const float* in = work.data() + i;
        float s[8]{};
        int k = 0;
        for(; k + 8 <= taps; k += 8)
        {
            for(int j = 0; j < 8; j++)
            {
                s[j] += c[k + j] * in[k + j];
            }
        }
        float y = ((s[0] + s[1]) + (s[2] + s[3])) + ((s[4] + s[5]) + (s[6] + s[7]));
        for(; k < taps; k++)
        {
            y += c[k] * in[k];
        }
        x[i] = y;
    }
    za::copy(work.begin() + n, work.begin() + n + h, history.begin());
}

// ----------------------------------------------------------------------------
// The game's mixer

namespace
{

constexpr int dynamicChannels = NUM_AMBIENTS + MAX_DYNAMIC_CHANNELS;

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

// VR_SndBandLimit's work since the start (the band-limited copies made as sounds load: the main thread).
struct BandLimitStats
{
    int sounds{0};
    double ms{0.0};
    double worstMs{0.0};
    long long bytes{0};
};
BandLimitStats bandLimitStats;

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
    AntiAlias antiAlias; // the voices' (vr_snd_antialias)
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

Live* live{nullptr};

// The whole mix's volume while the runtime's menu pauses the game (vr_xr_unfocused_volume; setDuck), on its way to the
// target a tenth of a second at a time (no click), in VR_SndLimit (with or without the spatial audio).
struct Duck
{
    float target{1.f};
    float gain{1.f};
};
Duck duck;

bool wanted()
{
    if(!live || !shm || shm->channels != 2 || cls.state == ca_dedicated || vr_snd_spatial.value <= 0.f)
    {
        return false;
    }
    if(vr_snd_spatial.value < 2.f && !VR_IsActive())
    {
        return false;
    }
    const Features f = featuresFromCvars();
    return f.hrtf || f.occlusion > 0.f || f.reverb > 0.f || f.doppler > 0.f || f.nearfield > 0.f;
}

bool running()
{
    return live && live->ok && wanted();
}

void releaseVoice(int v)
{
    const int c = live->channelOf[v];
    if(c >= 0)
    {
        live->voiceOf[c] = -1;
    }
    live->channelOf[v] = -1;
    live->mixer.stop(v);
}

void releaseAll()
{
    for(int v = 0; v < Mixer::maxVoices; v++)
    {
        releaseVoice(v);
    }
    live->carryLen = 0;
    live->voicesUsed = 0;
    live->antiAlias.reset();
}

za::String sofaPath()
{
    const char* name = vr_snd_hrtf_sofa.string;
    if(!name || !*name)
    {
        return {};
    }
    return za::String{com_gamedir} + "/" + name;
}

int frameSetting()
{
    const int f = static_cast<int>(vr_snd_frame.value);
    return f >= 1024 ? 1024 : f >= 512 ? 512 : 256;
}

void createSim()
{
    live->quality = static_cast<int>(vr_snd_reverb_quality.value);
    const Quality q = qualityPreset(live->quality);
    live->sim.create(live->rate, live->frame, q.type, q.order, q.duration, 8192);
    live->mixer.setReverb(q.type, q.order, q.duration);
    live->worldGen = -1; // (the scene made again)
    live->haveReverb = false;
    live->haveReverbSecond = false;
}

// Made (again) as the settings and the mix rate want it.
void ensure()
{
    const int frame = frameSetting();
    const za::String sofa = sofaPath();
    if(live->tried && live->rate == shm->speed && live->frame == frame && live->sofa == sofa)
    {
        if(live->ok && live->quality != static_cast<int>(vr_snd_reverb_quality.value))
        {
            createSim();
        }
        return;
    }
    if(live->ok)
    {
        releaseAll();
    }
    live->tried = true;
    live->ok = false;
    live->rate = shm->speed;
    live->frame = frame;
    live->sofa = sofa;
    live->sim.destroy();
    const double start = Sys_DoubleTime();
    if(!live->mixer.create(live->rate, live->frame, sofa.cStr()))
    {
        Con_DPrintf("Spatial audio: off (%s)\n", steamaudio::status());
        return;
    }
    if(!sofa.empty() && !live->mixer.customHrtf())
    {
        Con_Printf("Spatial audio: couldn't load the HRTF %s; Steam Audio's own instead\n", sofa.cStr());
    }
    za::fill(live->voiceOf, -1);
    za::fill(live->channelOf, -1);
    live->mixL.clear();
    live->mixL.resize(Mixer::maxSamples, 0.f);
    live->revL.resize(Mixer::maxSamples, 0.f);
    live->revR.resize(Mixer::maxSamples, 0.f);
    live->mixR.clear();
    live->mixR.resize(Mixer::maxSamples, 0.f);
    createSim();
    live->ok = true;
    Con_DPrintf("Spatial audio: %d Hz, %d-sample frames, %.1f ms to start\n", live->rate, live->frame,
        (Sys_DoubleTime() - start) * 1000.0);
}

// The hand of one of the player's hand channels: the weapon channels (QC VRGetGunChannel: CHAN_WEAPON the main hand,
// CHAN_WEAPON2 the off hand; from the muzzle) and the hands' own (VRGetHandChannel: CHAN_HAND, CHAN_HAND2, protocol.h
// SND_CHAN_HAND; any free one, from the hand: a parry, a reload, a holster). -1 for the player's other channels.
int handOf(const channel_t* ch)
{
    if(vr_snd_hands.value == 0.f || ch->entnum != cl.viewentity || cl.viewentity <= 0 || !VR_IsActive())
    {
        return -1;
    }
    const int c = ch->entchannel;
    if(c != 1 && c != 5 && c != SND_CHAN_HAND && c != SND_CHAN_HAND2)
    {
        return -1;
    }
    if(!hands::current().valid)
    {
        return -1;
    }
    return c == 5 || c == SND_CHAN_HAND2 ? HAND_OFF : HAND_MAIN;
}

// Where a hand channel's sound plays from: a weapon channel's from the muzzle (or the hand), a hand's own from the hand.
glm::vec3 handSoundAt(const channel_t* ch, int hand)
{
    const hands::State& hs = hands::current();
    const bool weapon = ch->entchannel == 1 || ch->entchannel == 5;
    return weapon && hs.muzzleValid[hand] ? hs.muzzle[hand] : hs.pos[hand];
}

// vr_snd_falloff: Quake's distance falloff scaled (1 Quake's own; 0.5 a sound carries twice as far).
float falloffScale()
{
    return za::clamp(vr_snd_falloff.value, 0.f, 4.f);
}

} // namespace

// A sound's place a little into a wall or the floor (a prop knocking: its contact; a hold the hand takes; a shot's
// impact) taken out of it, towards the head: Steam Audio's occlusion (and the world's line of sight standing in for it)
// found such a place behind the surface and hid it, 21 dB quieter than Quake in sight of the head (vr_snd_test
// distance). Up to 32 units (a metre) along the line to the head, in 2-unit steps; one deeper in stays where it is.
glm::vec3 outOfSolid(const glm::vec3& at, const glm::vec3& head)
{
    if(!cl.worldmodel)
    {
        return at;
    }
    const auto solid = [](const glm::vec3& p) {
        vec3_t q{p.x, p.y, p.z};
        const mleaf_t* leaf = Mod_PointInLeaf(q, cl.worldmodel);
        return leaf && leaf->contents == CONTENTS_SOLID;
    };
    if(!solid(at))
    {
        return at;
    }
    const glm::vec3 d = head - at;
    const float len = glm::length(d);
    if(len < 1.f)
    {
        return at;
    }
    const glm::vec3 dir = d / len;
    for(float step = 2.f; step <= za::min(32.f, len); step += 2.f)
    {
        const glm::vec3 p = at + dir * step;
        if(!solid(p))
        {
            return p + dir * 1.f; // (and a unit clear of the surface)
        }
    }
    return at;
}

namespace
{

bool eligible(const channel_t* ch)
{
    if(!ch->sfx || ch->entchannel < 0)
    {
        return false;
    }
    if(ch->entnum == cl.viewentity && cl.viewentity > 0)
    {
        return handOf(ch) >= 0; // (the player's other sounds stay in the middle of the head)
    }
    return true;
}

void selectVoices(int time)
{
    Live& L = *live;
    int n = 0;
    for(int i = NUM_AMBIENTS; i < total_channels; i++)
    {
        const channel_t* ch = &snd_channels[i];
        if(!eligible(ch))
        {
            continue;
        }
        const glm::vec3 d{ch->origin[0] - L.listener.pos.x, ch->origin[1] - L.listener.pos.y, ch->origin[2] - L.listener.pos.z};
        const float falloff = 1.f - glm::length(d) * ch->dist_mult;
        float priority = static_cast<float>(ch->master_vol) * falloff;
        if(priority <= 0.f)
        {
            continue;
        }
        if(L.voiceOf[i] >= 0)
        {
            priority *= 1.5f;
        }
        L.candidates[n++] = Candidate{i, priority};
    }
    const int k = za::min(n, za::clamp(static_cast<int>(vr_snd_voices.value), 1, Mixer::maxVoices));
    if(n > k)
    {
        // (The k kept, whatever the order of equal priorities: which of a tie at the k-th is kept is the partition's.)
        za::nthElement(L.candidates.begin(), L.candidates.begin() + k, L.candidates.begin() + n,
            [](const Candidate& a, const Candidate& b) { return a.priority > b.priority; });
    }
    for(int j = 0; j < k; j++)
    {
        L.keep[L.candidates[j].channel] = true;
    }
    for(int v = 0; v < Mixer::maxVoices; v++)
    {
        const int c = L.channelOf[v];
        if(c >= 0 && (!L.keep[c] || !snd_channels[c].sfx))
        {
            releaseVoice(v);
        }
    }
    int freeVoice = 0;
    for(int j = 0; j < k; j++)
    {
        const int c = L.candidates[j].channel;
        L.keep[c] = false;
        channel_t* ch = &snd_channels[c];
        sfxcache_t* sc = S_LoadSound(ch->sfx);
        if(!sc)
        {
            if(L.voiceOf[c] >= 0)
            {
                releaseVoice(L.voiceOf[c]);
            }
            continue;
        }
        int v = L.voiceOf[c];
        if(v >= 0 && !L.fresh[c])
        {
            L.mixer.source(v, sc); // (the same sound: its samples looked up again, in case the cache moved them)
            continue;
        }
        if(v < 0)
        {
            while(freeVoice < Mixer::maxVoices && L.channelOf[freeVoice] >= 0)
            {
                freeVoice++;
            }
            if(freeVoice >= Mixer::maxVoices)
            {
                continue;
            }
            v = freeVoice;
        }
        // A new sound starts where Quake would start it; one Quake was painting goes on from where it is now.
        double pos = ch->pos;
        if(!L.fresh[c])
        {
            // (Slow motion, a pitch: Quake painted it at that rate, its end in output samples; snd_mix.c.)
            const float rate = VR_SndRate() * S_CHANPITCH(ch);
            pos = rate == 1.f ? static_cast<double>(sc->length - (ch->end - time))
                              : static_cast<double>(sc->length) - static_cast<double>(ch->end - time) * rate;
            if(pos >= sc->length && sc->loopstart >= 0 && sc->loopstart < sc->length)
            {
                pos = sc->loopstart + za::fmod(pos - sc->length, static_cast<double>(sc->length - sc->loopstart));
            }
        }
        L.mixer.start(v, sc, pos);
        L.serial[v]++;
        L.voiceOf[c] = v;
        L.channelOf[v] = c;
    }
    for(int i = 0; i < total_channels; i++)
    {
        L.fresh[i] = false;
    }
}

// A voiced channel's input: its place, volume and velocity, and the live voice `v`'s simulation result (the world's
// own line of sight until its first). VR_SndPaint's voices; the game-time render's too.
VoiceInput channelInput(int c, int v, const Features& f, float volume)
{
    Live& L = *live;
    const channel_t* ch = &snd_channels[c];
    VoiceInput in;
    in.pos = glm::vec3{ch->origin[0], ch->origin[1], ch->origin[2]};
    const float falloff =
        za::max(0.f, 1.f - glm::length(in.pos - L.listener.pos) * ch->dist_mult * falloffScale());
    in.gain = static_cast<float>(ch->master_vol) * falloff * volume;
    in.attached = handOf(ch) >= 0;
    in.pitch = S_CHANPITCH(ch);
    if(c < dynamicChannels && L.follow[c].active && vr_snd_follow.value != 0.f)
    {
        in.vel = L.follow[c].vel;
    }
    const double lockStart = bench::now();
    in.hasDirect = L.sim.direct(v, L.serial[v], in.direct);
    bench::add(bench::Lock, lockStart);
    if(!in.hasDirect && f.occlusion > 0.f && L.sim.hasScene() && cl.worldmodel)
    {
        // Until the simulation's first result (a frame or two): the world's own line of sight (hull 0), so that a
        // sound starting behind a wall doesn't start at full volume.
        if(L.guessSerial[v] != L.serial[v])
        {
            L.guessSerial[v] = L.serial[v];
            trace_t trace;
            ZA_MEMSET(&trace, 0, sizeof trace);
            trace.fraction = 1.f;
            const glm::vec3 out = outOfSolid(in.pos, L.listener.pos);
            vec3_t from{L.listener.pos.x, L.listener.pos.y, L.listener.pos.z};
            vec3_t to{out.x, out.y, out.z};
            SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0.f, 1.f, from, to, &trace);
            L.guess[v] = DirectResult{};
            if(trace.fraction < 1.f || trace.allsolid)
            {
                const IPLMaterial& wall = material(SurfaceMaterial::Stone);
                L.guess[v].occlusion = 0.f;
                za::copy(wall.transmission, wall.transmission + 3, L.guess[v].transmission);
            }
        }
        in.hasDirect = true;
        in.direct = L.guess[v];
    }
    return in;
}

void report()
{
    Live& L = *live;
    if(vr_debug_snd.value <= 0.f || realtime - L.lastReport < 1.0)
    {
        return;
    }
    L.lastReport = realtime;
    const Features f = featuresFromCvars();
    Con_Printf("snd: %d voices, mix %.3f ms, direct %.2f ms, reverb %.2f ms (RT60 %.2f %.2f %.2f s)\n", L.voicesUsed,
        L.mixMs, L.sim.directMs(), L.sim.reflectionsMs(), L.haveReverb ? L.reverb.reverbTimes[0] : 0.f,
        L.haveReverb ? L.reverb.reverbTimes[1] : 0.f, L.haveReverb ? L.reverb.reverbTimes[2] : 0.f);
    if(vr_debug_snd.value < 2.f)
    {
        return;
    }
    for(int v = 0; v < Mixer::maxVoices; v++)
    {
        const int c = L.channelOf[v];
        if(c < 0)
        {
            continue;
        }
        const channel_t* ch = &snd_channels[c];
        const glm::vec3 d{ch->origin[0] - L.listener.pos.x, ch->origin[1] - L.listener.pos.y, ch->origin[2] - L.listener.pos.z};
        const float* eq = L.mixer.equaliser(v);
        const IPLVector3 dir = L.mixer.direction(v);
        Con_Printf("  %2d ch %3d ent %4d/%d %-24s %5.1f m  az %+6.1f el %+5.1f  eq %.2f %.2f %.2f  doppler %.3f\n", v, c,
            ch->entnum, ch->entchannel, ch->sfx ? ch->sfx->name : "-", glm::length(d) / f.unitsPerMetre,
            za::atan2(dir.x, -dir.z) * 180.f / 3.14159265f, za::atan2(dir.y, glm::length(glm::vec2{dir.x, dir.z})) * 180.f / 3.14159265f,
            eq[0], eq[1], eq[2], L.mixer.dopplerFactor(v));
    }
}

// Final mix to a WAV file (16-bit stereo).
bool writeWav16(const char* path, const za::Vector<float>& l, const za::Vector<float>& r, int rate)
{
    FILE* f = fopen(path, "wb");
    if(!f)
    {
        return false;
    }
    const auto u32 = [&](unsigned v) { fwrite(&v, 4, 1, f); };
    const auto u16 = [&](unsigned short v) { fwrite(&v, 2, 1, f); };
    const unsigned n = static_cast<unsigned>(za::min(l.size(), r.size()));
    fwrite("RIFF", 1, 4, f);
    u32(36 + n * 4);
    fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(static_cast<unsigned>(rate));
    u32(static_cast<unsigned>(rate) * 4);
    u16(4);
    u16(16);
    fwrite("data", 1, 4, f);
    u32(n * 4);
    for(unsigned i = 0; i < n; i++)
    {
        const short s[2] = {static_cast<short>(za::clamp(l[i], -32768.f, 32767.f)),
            static_cast<short>(za::clamp(r[i], -32768.f, 32767.f))};
        fwrite(s, 2, 2, f);
    }
    fclose(f);
    return true;
}

// The game-time WAV's header: 24-bit stereo at `rate`, `frames` long.
void writeWav24Header(FILE* f, int rate, long long frames)
{
    const auto u32 = [&](unsigned v) { fwrite(&v, 4, 1, f); };
    const auto u16 = [&](unsigned short v) { fwrite(&v, 2, 1, f); };
    const unsigned data = static_cast<unsigned>(frames * 6);
    fwrite("RIFF", 1, 4, f);
    u32(36 + data);
    fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);
    u16(2);
    u32(static_cast<unsigned>(rate));
    u32(static_cast<unsigned>(rate) * 6);
    u16(6);
    u16(24);
    fwrite("data", 1, 4, f);
    u32(data);
}

// `game`: vr_snd_capture_game's (the game-time mix, at normal speed), else the final mix's.
void finishCapture(Capture& c, bool game)
{
    c.running = false;
    const int rate = shm ? shm->speed : 44100;
    za::String path = za::String{com_gamedir} + (game ? "/sound_tests/capture_game_" : "/sound_tests/capture_") + c.name + ".wav";
    COM_CreatePath(path.data());
    writeWav16(path.cStr(), c.left, c.right, rate);
    const Levels lv = measure(c.left.data(), c.right.data(), static_cast<int>(c.left.size()), rate);
    Con_Printf("vr_snd_capture%s %s: %d samples, rms %.1f dB, left %.1f dB, right %.1f dB, below 500 Hz %.1f dB, above 4 kHz "
               "%.1f dB (%s)\n",
        game ? "_game" : "", c.name.cStr(), static_cast<int>(c.left.size()), lv.rms, lv.left, lv.right, lv.low, lv.high, path.cStr());
    if(game)
    {
        Con_Printf("  the game-time limiter: deepest %.1f dB\n", c.minGain > 0.f ? 20.f * za::log10(c.minGain) : -200.f);
        c.left.clear();
        c.right.clear();
        return;
    }
    const auto db = [](float x) { return x > 0.f ? 20.f * za::log10(x) : -200.f; };
    Con_Printf("  effects: peak %.1f dB of full scale, %d samples over it; mix: peak %.1f dB, %d samples clipped; limiter %s, "
               "deepest %.1f dB\n",
        db(c.busPeak / BusLimiter::fullScale), c.busOver, db(c.outPeak / BusLimiter::fullScale), c.outClipped,
        vr_snd_limiter.value != 0.f ? "on" : "off", db(c.minGain));
    c.left.clear();
    c.right.clear();
}

// ----------------------------------------------------------------------------
// Commands

void info_f()
{
    Con_Printf("Spatial audio (vr_snd_spatial %g): %s\n", vr_snd_spatial.value,
        running() ? "on" : wanted() ? "wanted, not running" : "off");
    Con_Printf("  Steam Audio: %s\n", steamaudio::status());
    if(!live)
    {
        return;
    }
    Live& L = *live;
    if(L.ok)
    {
        const Features f = featuresFromCvars();
        Con_Printf("  %d Hz, %d-sample frames; HRTF %s%s, voices %d of %d, mix %.3f ms a call\n", L.rate, L.frame,
            f.hrtf ? "on" : "off", L.mixer.customHrtf() ? " (SOFA)" : "", L.voicesUsed, static_cast<int>(vr_snd_voices.value),
            L.mixMs);
        Con_Printf("  scene: %d triangles (%.1f ms to build), %d brush models (of %d seen); direct %.2f ms, reverb %.2f ms\n",
            L.sim.sceneTriangles(), L.sim.sceneBuildMs(), L.sim.instanceCount(), L.brushSeen, L.sim.directMs(), L.sim.reflectionsMs());
        if(L.haveReverb && L.reverb.type == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION)
        {
            Con_Printf("  reverb: convolution, %d-sample response (%d channels)\n", L.reverb.irSize, L.reverb.numChannels);
        }
        else if(L.haveReverb)
        {
            Con_Printf("  reverb: RT60 %.2f %.2f %.2f s (low, mid, high)\n", L.reverb.reverbTimes[0], L.reverb.reverbTimes[1],
                L.reverb.reverbTimes[2]);
        }
    }
    Con_Printf("  mix: limiter %s (ceiling %.1f dB, %.0f ms release), deepest %.1f dB since the last info; %lld samples "
               "over full scale, %lld clipped\n",
        vr_snd_limiter.value != 0.f ? "on" : "off (Quake's hard clip)", za::clamp(vr_snd_limiter_ceiling.value, -12.f, 0.f),
        za::clamp(vr_snd_limiter_release.value, 0.02f, 1.f) * 1000.f,
        L.bus.minGain > 0.f ? 20.f * za::log10(L.bus.minGain) : -200.f, L.bus.over, L.bus.clipped);
    L.bus.minGain = 1.f;
    Con_Printf("  game-time render (vr_timescale_wav, vr_snd_capture_game): %s, %.3f ms a call; WAV: %s\n",
        L.shadow.running ? (L.shadow.mixer.valid() ? "on, with its own voices" : "on, Quake's mix only") : "off",
        L.shadow.ms, L.wav.file ? L.wav.path.cStr() : "none");
    const mleaf_t* leaf = cl.worldmodel ? Mod_PointInLeaf(&L.listener.pos.x, cl.worldmodel) : nullptr;
    Con_Printf("  listener: %s at %.0f %.0f %.0f (leaf contents %d), speed %.0f units/s\n", L.head ? "the head" : "the view",
        L.listener.pos.x, L.listener.pos.y, L.listener.pos.z, leaf ? leaf->contents : 0, glm::length(L.listener.vel));
}

void startCapture(bool game)
{
    const char* cmd = game ? "vr_snd_capture_game" : "vr_snd_capture";
    if(!live || !shm)
    {
        Con_Printf("%s: no sound (-nosound?)\n", cmd);
        return;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf(game ? "%s <seconds> [name]: the mix in the game's time (as at normal speed in slow motion; no music) to "
                          "<game>/sound_tests/capture_game_<name>.wav, and its levels\n"
                        : "%s <seconds> [name]: the final mix to <game>/sound_tests/capture_<name>.wav, and its levels\n",
            cmd);
        return;
    }
    Capture& c = game ? live->gameCapture : live->capture;
    c.wanted = static_cast<int>(za::clamp(Q_atof(Cmd_Argv(1)), 0.05f, 60.f) * static_cast<float>(shm->speed));
    c.name = Cmd_Argc() > 2 ? Cmd_Argv(2) : "capture";
    c.left.clear();
    c.right.clear();
    c.left.reserve(c.wanted);
    c.right.reserve(c.wanted);
    c.busSeen = 0;
    c.busPeak = 0.f;
    c.busOver = 0;
    c.outPeak = 0.f;
    c.outClipped = 0;
    c.minGain = 1.f;
    c.running = true;
    if(game)
    {
        live->gameBus.look = 0; // (fresh: nothing of an old mix in its delay)
    }
}

void capture_f()
{
    startCapture(false);
}

void captureGame_f()
{
    startCapture(true);
}

// Looping sounds in a ring round the listener (static sounds, till the map changes): a load for vr_profile.
void benchSpawn_f()
{
    if(!live || !shm || cls.state != ca_connected)
    {
        Con_Printf("vr_snd_bench_spawn: needs a map and sound\n");
        return;
    }
    const int count = Cmd_Argc() > 1 ? za::clamp(Q_atoi(Cmd_Argv(1)), 1, 128) : 32;
    const float radius = Cmd_Argc() > 2 ? Q_atof(Cmd_Argv(2)) : 200.f;
    const char* names[] = {"ambience/fire1.wav", "ambience/hum1.wav", "ambience/drip1.wav", "ambience/comp1.wav"};
    for(int i = 0; i < count; i++)
    {
        sfx_t* sfx = S_PrecacheSound(names[i % 4]);
        const float a = static_cast<float>(i) * 2.399963f; // (the golden angle: spread round)
        const float r = radius * (0.5f + 0.5f * static_cast<float>(i % 7) / 6.f);
        vec3_t at{live->listener.pos.x + za::cos(a) * r, live->listener.pos.y + za::sin(a) * r,
            live->listener.pos.z + static_cast<float>(i % 3 - 1) * 24.f};
        S_StaticSound(sfx, at, 255.f, 1.f);
    }
    Con_Printf("vr_snd_bench_spawn: %d looping sounds within %.0f units\n", count, radius);
}

void sceneObj_f()
{
    if(!live || !live->sim.hasScene())
    {
        Con_Printf("vr_snd_scene_obj: no scene (spatial audio off, or no map)\n");
        return;
    }
    za::String base = za::String{com_gamedir} + "/sound_tests/scene";
    COM_CreatePath(base.data());
    live->sim.saveObj(base.cStr());
    Con_Printf("vr_snd_scene_obj: %s.obj (%d triangles)\n", base.cStr(), live->sim.sceneTriangles());
}

// A sound at a place (the world's, channel auto): vr_snd_play <sample> <x> <y> <z> [volume] [attenuation].
void play_f()
{
    if(Cmd_Argc() < 5 || cls.state != ca_connected)
    {
        Con_Printf("vr_snd_play <sample> <x> <y> <z> [volume] [attenuation]: a sound from that place (in a map)\n");
        return;
    }
    sfx_t* sfx = S_PrecacheSound(Cmd_Argv(1));
    vec3_t at{Q_atof(Cmd_Argv(2)), Q_atof(Cmd_Argv(3)), Q_atof(Cmd_Argv(4))};
    const float volume = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 1.f;
    const float attenuation = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 1.f;
    S_StartSound(0, 0, sfx, at, volume, attenuation);
}

// A loaded sound as the mixers read it (vr_snd_dump <sample>): Quake's held samples and the band-limited copy
// (vr_snd_fullband), each a WAV at the mix's rate in sound_tests/ (dump_<sample>_held.wav, _full.wav), at half volume.
void dump_f()
{
    if(Cmd_Argc() < 2 || !shm)
    {
        Con_Printf("vr_snd_dump <sample>: the sound's held samples and band-limited copy to sound_tests/\n");
        return;
    }
    sfx_t* sfx = S_PrecacheSound(Cmd_Argv(1));
    const sfxcache_t* sc = sfx ? S_LoadSound(sfx) : nullptr;
    if(!sc)
    {
        Con_Printf("vr_snd_dump: %s isn't loaded\n", Cmd_Argv(1));
        return;
    }
    const short* full = fullBandData(sc);
    za::Vector<float> held(static_cast<za::SizeT>(sc->length)), copy(static_cast<za::SizeT>(sc->length));
    for(int i = 0; i < sc->length; i++)
    {
        // (both at the copy's scale, 1/S_FULLBAND_SCALE: its overshoot of a clipped sound not clipped in the file)
        held[i] = (sc->width == 2 ? static_cast<float>(reinterpret_cast<const short*>(sc->data)[i])
                                  : static_cast<float>(static_cast<signed char>(sc->data[i])) * 256.f) /
                  S_FULLBAND_SCALE;
        copy[i] = full ? static_cast<float>(full[i]) : held[i];
    }
    za::String name = Cmd_Argv(1);
    for(char& c : name)
    {
        c = c == '/' || c == '\\' || c == '.' ? '_' : c;
    }
    const za::String base = za::String{com_gamedir} + "/sound_tests/dump_" + name;
    COM_CreatePath((base + "_held.wav").data());
    writeWav16((base + "_held.wav").cStr(), held, held, shm->speed);
    writeWav16((base + "_full.wav").cStr(), copy, copy, shm->speed);
    Con_Printf("vr_snd_dump %s: %d samples at %d Hz, %d-bit, %s (%s_held.wav, _full.wav)\n", Cmd_Argv(1), sc->length,
        shm->speed, sc->width * 8, full ? "band-limited copy" : "no copy (loaded at the mix's rate)", base.cStr());
}

// A sound from a direction (vr_snd_play_dir <sample> <azimuth> [elevation] [metres] [volume] [attenuation]): degrees
// clockwise from where the listener faces (90 the right), up from level, at that distance (2 m) from the listener. For
// the left/right balance against the angle (ROUND21.md, "HRTF balance").
void playDir_f()
{
    if(Cmd_Argc() < 3 || cls.state != ca_connected || !live)
    {
        Con_Printf("vr_snd_play_dir <sample> <azimuth> [elevation] [metres] [volume] [attenuation]: a sound from that "
                   "direction (degrees, 90 the right) and distance (2 m)\n");
        return;
    }
    sfx_t* sfx = S_PrecacheSound(Cmd_Argv(1));
    const float a = Q_atof(Cmd_Argv(2)) * 3.14159265f / 180.f;
    const float e = (Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 0.f) * 3.14159265f / 180.f;
    const float distance = (Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : 2.f) * units::metresToUnits();
    const float volume = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 1.f;
    const float attenuation = Cmd_Argc() > 6 ? Q_atof(Cmd_Argv(6)) : 1.f;
    const Listener& l = live->listener;
    const glm::vec3 p =
        l.pos + distance * (za::cos(e) * (za::cos(a) * l.fwd + za::sin(a) * l.right) + za::sin(e) * l.up);
    vec3_t at{p.x, p.y, p.z};
    S_StartSound(-1, 0, sfx, at, volume, attenuation);
}

// Copies of a sound at once (vr_snd_burst <sample> [count] [distance] [spread] [volume]): ahead of the listener,
// spread round a circle, all started this frame as explosive boxes blowing up together are (Quake's S_StartSound
// offsets identical sounds started together a little). For the effects' bus's limiter (vr_snd_limiter).
void burst_f()
{
    if(cls.state != ca_connected || !live)
    {
        Con_Printf("vr_snd_burst <sample> [count] [distance] [spread] [volume]: copies of a sound at once, ahead of you\n");
        return;
    }
    const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "weapons/r_exp3.wav";
    const int count = Cmd_Argc() > 2 ? za::clamp(Q_atoi(Cmd_Argv(2)), 1, 32) : 5;
    const float distance = Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 96.f;
    const float spread = Cmd_Argc() > 4 ? Q_atof(Cmd_Argv(4)) : 32.f;
    const float volume = Cmd_Argc() > 5 ? Q_atof(Cmd_Argv(5)) : 1.f;
    sfx_t* sfx = S_PrecacheSound(name);
    const Listener& l = live->listener;
    for(int i = 0; i < count; i++)
    {
        const float a = static_cast<float>(i) * 2.399963f;
        const glm::vec3 p = l.pos + l.fwd * distance + l.right * (za::cos(a) * spread) + l.up * (za::sin(a) * spread * 0.5f);
        vec3_t at{p.x, p.y, p.z};
        S_StartSound(-1, 0, sfx, at, volume, 1.f);
    }
}

// vr_snd_bench <seconds> [label] [sounds a second] [orbit units/s]: records each sound frame's time in each stage of
// the mix for that long (real time), playing a combat-like scene meanwhile (random sounds of id's monsters, weapons and
// explosions round the listener, and the listener going round a circle), then prints each stage's median, 95th and 99th
// percentiles, worst and load, the simulations' runs and the sounds' memory, and appends them to
// sound_tests/bench.csv. Load the scene's other parts first (vr_snd_bench_spawn, vr_snd_voices, vr_timescale, ...).
} // namespace

// The calling thread's allocations in its scope into the bench's count (vr_snd_bench).
struct BenchAllocs
{
    za::U64 at{bench::on ? alloccount::thisThread() : 0};
    BenchAllocs() = default;
    BenchAllocs(const BenchAllocs&) = delete;
    BenchAllocs& operator=(const BenchAllocs&) = delete;
    ~BenchAllocs()
    {
        if(bench::on)
        {
            bench::addCount(bench::Allocs, static_cast<double>(alloccount::thisThread() - at));
        }
    }
};

namespace
{

void benchFinish()
{
    Live& L = *live;
    BenchRun& b = L.bench;
    b.running = false;
    bench::frame();
    bench::stop();
    const int rate = shm ? shm->speed : 44100;
    const long long samples = bench::samples();
    Con_Printf("vr_snd_bench %s: %d frames, %.1f s of sound, %.1f voices (the frames that painted, mean), %d workers\n", b.label.cStr(),
        bench::frames(), static_cast<double>(samples) / rate, bench::meanVoices(), jobs::workers());
    Con_Printf("  %-12s %7s %7s %7s %7s  %8s\n", "stage (ms)", "median", "p95", "p99", "max", "ms/s");
    za::String csvPath = za::String{com_gamedir} + "/sound_tests/bench.csv";
    COM_CreatePath(csvPath.data());
    FILE* csv = fopen(csvPath.cStr(), "ab");
    for(int st = 0; st < bench::Count; st++)
    {
        const bench::Stats x = bench::stats(st, rate);
        Con_Printf("  %-12s %7.3f %7.3f %7.3f %7.3f  %8.3f\n", bench::stageName(st), x.median, x.p95, x.p99, x.max,
            x.perSecond);
        if(csv)
        {
            fprintf(csv, "%s,%s,%d,%.4f,%.4f,%.4f,%.4f,%.4f\n", b.label.cStr(), bench::stageName(st), x.n, x.median, x.p95,
                x.p99, x.max, x.perSecond);
        }
    }
    const auto runLine = [&](const char* name, za::Vector<float>& ms) {
        if(ms.empty())
        {
            Con_Printf("  sim %s: no runs\n", name);
            return;
        }
        za::quickSort(ms.begin(), ms.end(), [](float a, float b) { return a < b; });
        const int n = static_cast<int>(ms.size());
        const auto at = [&](double q) { return ms[za::min(n - 1, static_cast<int>(q * (n - 1) + 0.5))]; };
        Con_Printf("  sim %s (a worker): %d runs, median %.3f ms, p95 %.3f, max %.3f\n", name, n, at(0.5), at(0.95),
            ms[n - 1]);
        if(csv)
        {
            fprintf(csv, "%s,sim_%s,%d,%.4f,%.4f,%.4f,%.4f,0\n", b.label.cStr(), name, n, at(0.5), at(0.95), at(0.99),
                ms[n - 1]);
        }
    };
    runLine("direct", b.directMs);
    runLine("reflections", b.reflectionsMs);
    int loaded = 0;
    int held = 0;
    int full = 0;
    S_SfxMemory(&loaded, &held, &full);
    Con_Printf("  sounds: %d in the cache, %d KiB as Quake mixes them, %d KiB band-limited copies; band-limiting: %d "
               "sounds in the bench (%.2f ms), %d since the start (%.1f ms, worst %.2f ms, %lld KiB)\n",
        loaded, held / 1024, full / 1024, bandLimitStats.sounds - b.bandLimited, bandLimitStats.ms - b.bandLimitMs,
        bandLimitStats.sounds, bandLimitStats.ms, bandLimitStats.worstMs, bandLimitStats.bytes / 1024);
    if(csv)
    {
        fprintf(csv, "%s,memory_kib,%d,%d,%d,0,0,0\n", b.label.cStr(), loaded, held / 1024, full / 1024);
        fprintf(csv, "%s,bandlimit_since_start,%d,%.4f,%.4f,0,0,0\n", b.label.cStr(), bandLimitStats.sounds,
            bandLimitStats.ms, bandLimitStats.worstMs);
        fclose(csv);
    }
}

// The bench's sounds (id's: never shipped, played from the game's own data).
constexpr const char* benchSounds[] = {"weapons/r_exp3.wav", "weapons/rocket1i.wav", "weapons/guncock.wav",
    "weapons/grenade.wav", "weapons/spike2.wav", "weapons/sgun1.wav", "weapons/lhit.wav", "soldier/sight1.wav",
    "soldier/pain1.wav", "soldier/death1.wav", "dog/dattack1.wav", "ogre/ogwake.wav", "ogre/ogsawatk.wav",
    "ogre/ogdrag.wav", "knight/sword1.wav", "knight/khurt.wav", "zombie/z_idle.wav", "enforcer/enfire.wav",
    "wizard/wattack.wav", "demon/dhit2.wav", "shambler/sattck1.wav", "player/pain1.wav"};
constexpr int benchSoundCount = static_cast<int>(sizeof benchSounds / sizeof benchSounds[0]);

void benchFrame()
{
    Live& L = *live;
    BenchRun& b = L.bench;
    if(!b.running)
    {
        return;
    }
    bench::frame();
    const Simulation::Runs r = L.sim.runs();
    if(r.direct != b.runs.direct)
    {
        b.directMs.pushBack(static_cast<float>(r.directMs));
    }
    if(r.reflections != b.runs.reflections)
    {
        b.reflectionsMs.pushBack(static_cast<float>(r.reflectionsMs));
    }
    b.runs = r;
    if(realtime >= b.end)
    {
        benchFinish();
        return;
    }
    // The combat: a sound every 1 / shotsPerSecond, at random, 100-900 units away.
    const auto random = [&]() {
        b.rng = b.rng * 1664525u + 1013904223u;
        return static_cast<float>(b.rng >> 8) / static_cast<float>(1u << 24);
    };
    while(b.shotsPerSecond > 0.f && realtime >= b.nextShot && cls.state == ca_connected)
    {
        b.nextShot += 1.0 / b.shotsPerSecond;
        sfx_t* sfx = S_PrecacheSound(benchSounds[za::min(benchSoundCount - 1, static_cast<int>(random() * benchSoundCount))]);
        const float a = random() * 6.2831853f;
        const float d = 100.f + random() * 800.f;
        vec3_t at{L.lastPos.x + za::cos(a) * d, L.lastPos.y + za::sin(a) * d, L.lastPos.z + (random() - 0.5f) * 128.f};
        S_StartSound(0, 0, sfx, at, 1.f, 1.f);
    }
}

void bench_f()
{
    if(!live || !shm)
    {
        Con_Printf("vr_snd_bench: needs sound\n");
        return;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_snd_bench <seconds> [label] [sounds a second] [orbit units/s]: each sound frame's time by stage "
                   "(median, p95, p99), with a combat scene\n");
        return;
    }
    BenchRun& b = live->bench;
    const double seconds = za::clamp(static_cast<double>(Q_atof(Cmd_Argv(1))), 0.5, 600.0);
    b.running = true;
    b.end = realtime + seconds;
    b.label = Cmd_Argc() > 2 ? Cmd_Argv(2) : "bench";
    b.shotsPerSecond = Cmd_Argc() > 3 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(3))), 0.f, 200.f) : 0.f;
    b.orbitSpeed = Cmd_Argc() > 4 ? za::clamp(static_cast<float>(Q_atof(Cmd_Argv(4))), 0.f, 4000.f) : 0.f;
    b.orbitStart = realtime;
    b.nextShot = realtime;
    b.rng = 12345u;
    b.runs = live->sim.runs();
    b.directMs.clear();
    b.reflectionsMs.clear();
    b.bandLimited = bandLimitStats.sounds;
    b.bandLimitMs = bandLimitStats.ms;
    bench::start(static_cast<int>(seconds * 1000.0));
    Con_Printf("vr_snd_bench %s: %.1f s\n", b.label.cStr(), seconds);
}

} // namespace

void init()
{
    live = new Live{};
    za::fill(live->voiceOf, -1);
    za::fill(live->channelOf, -1);
    Cmd_AddCommand("vr_snd_info", info_f);
    Cmd_AddCommand("vr_snd_test", test_f);
    Cmd_AddCommand("vr_snd_capture", capture_f);
    Cmd_AddCommand("vr_snd_capture_game", captureGame_f);
    Cmd_AddCommand("vr_snd_bench_spawn", benchSpawn_f);
    Cmd_AddCommand("vr_snd_scene_obj", sceneObj_f);
    Cmd_AddCommand("vr_snd_play", play_f);
    Cmd_AddCommand("vr_snd_burst", burst_f);
    Cmd_AddCommand("vr_snd_play_dir", playDir_f);
    Cmd_AddCommand("vr_snd_dump", dump_f);
    Cmd_AddCommand("vr_snd_bench", bench_f);
    makeInterpTable();
}

void shutdown()
{
    if(live)
    {
        stopGameWav();
        live->shadow.mixer.destroy();
        live->sim.destroy();
        live->mixer.destroy();
        delete live;
        live = nullptr;
    }
    steamaudio::shutdown();
}

void setDuck(float volume)
{
    duck.target = za::clamp(volume, 0.f, 1.f);
}

float duckGain()
{
    return duck.gain;
}

} // namespace qvr::audio

using namespace qvr;
using namespace qvr::audio;

// ----------------------------------------------------------------------------
// Hooks (vr_api.h)

extern "C" void VR_SndListener(float* origin, float* forward, float* right, float* up)
{
    if(!live)
    {
        return;
    }
    QVR_PROFILE("spatial audio");
    steamaudio::flushLog();
    Live& L = *live;
    benchFrame(); // (vr_snd_bench: the last frame's row; its scene)
    const bench::Timed timedListener{bench::Listener};
    const BenchAllocs allocs;

    // The head, in VR.
    const hands::State& hs = hands::current();
    L.head = VR_IsActive() && hs.valid && cls.state == ca_connected;
    if(L.head)
    {
        glm::vec3 f, r, u;
        hands::angleVectors(hs.headAngles, f, r, u);
        for(int i = 0; i < 3; i++)
        {
            origin[i] = hs.head[i];
            forward[i] = f[i];
            right[i] = r[i];
            up[i] = u[i];
        }
    }
    if(L.bench.running && L.bench.orbitSpeed > 0.f)
    {
        // (vr_snd_bench: round a circle)
        const double a = (realtime - L.bench.orbitStart) * L.bench.orbitSpeed / L.bench.orbitRadius;
        origin[0] += static_cast<float>(za::cos(a) - 1.0) * L.bench.orbitRadius;
        origin[1] += static_cast<float>(za::sin(a)) * L.bench.orbitRadius;
    }
    const glm::vec3 pos{origin[0], origin[1], origin[2]};
    const double dt = realtime - L.lastTime;
    if(dt > 0.0)
    {
        glm::vec3 v = (pos - L.lastPos) / static_cast<float>(dt);
        if(glm::length(v) > 5000.f || dt > 0.25) // (a teleport, a new map, a pause)
        {
            v = glm::vec3{0.f};
        }
        L.listener.vel += (v - L.listener.vel) * 0.3f;
        L.lastPos = pos;
        L.lastTime = realtime;
    }
    L.listener.pos = pos;
    L.listener.fwd = glm::vec3{forward[0], forward[1], forward[2]};
    L.listener.right = glm::vec3{right[0], right[1], right[2]};
    L.listener.up = glm::vec3{up[0], up[1], up[2]};

    if(!wanted())
    {
        if(L.ok)
        {
            releaseAll();
        }
        return;
    }
    ensure();
    if(!L.ok)
    {
        return;
    }

    // The simulations: the scene (a new map, the movers), the sources, the listener.
    const Features f = featuresFromCvars();
    if(L.sim.valid())
    {
        const int gen = worldGeneration();
        const bool need = cl.worldmodel && (f.occlusion > 0.f || f.reverb > 0.f || f.air);
        if(gen != L.worldGen || f.unitsPerMetre != L.worldUpm || need != L.worldNeeded)
        {
            L.worldGen = gen;
            L.worldUpm = f.unitsPerMetre;
            L.worldNeeded = need;
            L.haveReverb = false;
            L.haveReverbSecond = false;
            L.sim.dropScene(); // (the last map's out at once: no sound hidden by its walls meanwhile)
            if(need)
            {
                Mesh mesh;
                appendBrushModel(cl.worldmodel, mesh, f.unitsPerMetre);
                L.sim.buildScene(ZA_MOVE(mesh));
            }
        }
        if(f.occlusion > 0.f || f.reverb > 0.f)
        {
            const bench::Timed timed{bench::Brush};
            L.brushSeen = trackBrushEntities(L.sim, f.unitsPerMetre);
        }
        int n = 0;
        for(int v = 0; v < Mixer::maxVoices; v++)
        {
            Simulation::Source& s = L.sources[v];
            const int c = L.channelOf[v];
            s.active = c >= 0;
            s.serial = L.serial[v];
            if(c >= 0)
            {
                // A sound from inside a brush model (a door's: its box's middle) is taken out of it, towards the
                // listener, or its own faces would hide it.
                glm::vec3 at{snd_channels[c].origin[0], snd_channels[c].origin[1], snd_channels[c].origin[2]};
                const int e = snd_channels[c].entnum;
                if(e > 0 && e < cl.num_entities && cl_entities[e].model && cl_entities[e].model->type == mod_brush)
                {
                    const entity_t* ent = &cl_entities[e];
                    const glm::vec3 mins = glm::vec3{ent->origin[0], ent->origin[1], ent->origin[2]} +
                                           glm::vec3{ent->model->mins[0], ent->model->mins[1], ent->model->mins[2]};
                    const glm::vec3 maxs = glm::vec3{ent->origin[0], ent->origin[1], ent->origin[2]} +
                                           glm::vec3{ent->model->maxs[0], ent->model->maxs[1], ent->model->maxs[2]};
                    const glm::vec3 d = L.listener.pos - at;
                    float leave = 0.f; // how far along d the box is left
                    bool inside = true;
                    for(int k = 0; k < 3; k++)
                    {
                        if(at[k] < mins[k] || at[k] > maxs[k])
                        {
                            inside = false;
                        }
                    }
                    if(inside)
                    {
                        leave = 1e9f;
                        for(int k = 0; k < 3; k++)
                        {
                            if(d[k] > 1e-3f)
                            {
                                leave = za::min(leave, (maxs[k] - at[k]) / d[k]);
                            }
                            else if(d[k] < -1e-3f)
                            {
                                leave = za::min(leave, (mins[k] - at[k]) / d[k]);
                            }
                        }
                        if(leave < 1.f)
                        {
                            at += d * leave + glm::normalize(d) * 8.f;
                        }
                    }
                }
                s.pos = toSteam(outOfSolid(at, L.listener.pos), f.unitsPerMetre);
                n = v + 1;
            }
        }
        SimSettings ss;
        ss.occlusion = f.occlusion > 0.f;
        ss.occlusionSamples = static_cast<int>(vr_snd_occlusion_samples.value);
        ss.occlusionRadius = za::clamp(vr_snd_occlusion_radius.value, 0.05f, 4.f);
        ss.air = f.air;
        ss.reverb = f.reverb > 0.f;
        const Quality q = qualityPreset(L.quality);
        ss.rays = q.rays;
        ss.bounces = q.bounces;
        ss.duration = q.duration;
        ss.order = q.order;
        ss.reverbInterval = za::clamp(static_cast<double>(vr_snd_reverb_interval.value), 0.05, 5.0);
        ss.second = ss.reverb && (L.shadow.running || vr_timescale_wav.value != 0.f);
        const bench::Timed timed{bench::SimUpdate};
        L.sim.update(coordinates(L.listener.pos, L.listener.fwd, L.listener.right, L.listener.up, f.unitsPerMetre),
            L.sources.data(), n, ss, realtime);
        IPLReflectionEffectParams p{};
        if(L.sim.reflections(p))
        {
            L.reverb = p;
            L.haveReverb = true;
        }
        IPLReflectionEffectParams p2{};
        if(L.sim.reflectionsSecond(p2))
        {
            L.reverbSecond = p2;
            L.haveReverbSecond = true;
        }
        else if(!ss.second)
        {
            L.haveReverbSecond = false;
        }
    }
    report();
}

extern "C" int VR_SndHandOf(const channel_t* ch, float* hand)
{
    const int h = handOf(ch);
    if(h >= 0)
    {
        const glm::vec3 p = handSoundAt(ch, h);
        hand[0] = p.x;
        hand[1] = p.y;
        hand[2] = p.z;
    }
    return h < 0 ? -1 : h == HAND_MAIN ? 0 : 1;
}

extern "C" int VR_SndSpatialize(channel_t* ch)
{
    if(!live)
    {
        return 0;
    }
    const int index = static_cast<int>(ch - snd_channels);
    const int hand = handOf(ch);
    if(hand >= 0)
    {
        // The hand's muzzle (or the hand), panned as Quake would a sound there.
        const glm::vec3 p = handSoundAt(ch, hand);
        ch->origin[0] = p.x;
        ch->origin[1] = p.y;
        ch->origin[2] = p.z;
        vec3_t d;
        VectorSubtract(ch->origin, listener_origin, d);
        const float dist = VectorNormalize(d) * ch->dist_mult * falloffScale();
        const float dot = DotProduct(listener_right, d);
        const float scale = za::max(0.f, 1.f - dist);
        ch->rightvol = za::max(0, static_cast<int>(static_cast<float>(ch->master_vol) * scale * (1.f + dot)));
        ch->leftvol = za::max(0, static_cast<int>(static_cast<float>(ch->master_vol) * scale * (1.f - dot)));
        return 1;
    }
    if(index >= 0 && index < dynamicChannels && ch->sfx)
    {
        Follow& f = live->follow[index];
        if(f.active && vr_snd_follow.value != 0.f)
        {
            const entity_t* e = f.ent < cl.num_entities && cl_entities ? &cl_entities[f.ent] : nullptr;
            if(!e || e->model != f.model || e->msgtime != cl.mtime[0])
            {
                f.active = false; // (gone, hidden or another thing now: the sound stays where it was)
            }
            else
            {
                const glm::vec3 at = glm::vec3{e->origin[0], e->origin[1], e->origin[2]} + f.offset;
                const double dt = realtime - f.time;
                if(glm::length(at - f.last) > 256.f)
                {
                    f.active = false; // (a teleport, or the slot taken by another entity)
                }
                else
                {
                    if(dt > 1e-4)
                    {
                        const glm::vec3 v = (at - f.last) / static_cast<float>(dt);
                        f.vel += (v - f.vel) * 0.5f;
                        f.time = realtime;
                        f.last = at;
                    }
                    ch->origin[0] = at.x;
                    ch->origin[1] = at.y;
                    ch->origin[2] = at.z;
                }
            }
            if(!f.active)
            {
                f.vel = glm::vec3{0.f};
            }
        }
    }
    const float scale = falloffScale();
    if(scale == 1.f || ch->entnum == cl.viewentity)
    {
        return 0; // (Quake's own panning and falloff; the player's own sounds at full volume)
    }
    // SND_Spatialize's panning with the falloff scaled.
    vec3_t d;
    VectorSubtract(ch->origin, listener_origin, d);
    const float dist = VectorNormalize(d) * ch->dist_mult * scale;
    const float dot = shm && shm->channels == 1 ? 0.f : DotProduct(listener_right, d);
    const float gain = 1.f - dist;
    ch->rightvol = za::max(0, static_cast<int>(static_cast<float>(ch->master_vol) * gain * (1.f + dot)));
    ch->leftvol = za::max(0, static_cast<int>(static_cast<float>(ch->master_vol) * gain * (1.f - dot)));
    return 1;
}

extern "C" void VR_SndStarted(channel_t* ch)
{
    // The physics sounds louder than the server can send (its volume byte stops at 1): vr_physsound over 1 multiplies
    // them here (vr_physsound.cpp sends them at most at 1).
    if(ch->sfx && vr_physsound.value > 1.f && !q_strncasecmp(ch->sfx->name, "vr/phys/", 8))
    {
        const float boost = za::min(vr_physsound.value, 2.f);
        ch->master_vol = static_cast<int>(static_cast<float>(ch->master_vol) * boost);
        ch->leftvol = static_cast<int>(static_cast<float>(ch->leftvol) * boost); // (spatialized already)
        ch->rightvol = static_cast<int>(static_cast<float>(ch->rightvol) * boost);
    }
    if(!live)
    {
        return;
    }
    const int index = static_cast<int>(ch - snd_channels);
    if(index < 0 || index >= MAX_CHANNELS)
    {
        return;
    }
    live->fresh[index] = true;
    live->shadow.started[index] = true; // (the game-time render: a new sound, from here)
    live->shadow.startPos[index] = ch->pos;
    if(index >= dynamicChannels)
    {
        return;
    }
    Follow& f = live->follow[index];
    f = Follow{};
    if(ch->entnum <= 0 || ch->entnum == cl.viewentity || ch->entnum >= cl.num_entities || !cl_entities)
    {
        return;
    }
    const entity_t* e = &cl_entities[ch->entnum];
    if(!e->model)
    {
        return;
    }
    const glm::vec3 at{ch->origin[0], ch->origin[1], ch->origin[2]};
    const glm::vec3 offset = at - glm::vec3{e->origin[0], e->origin[1], e->origin[2]};
    if(glm::length(offset) > 256.f)
    {
        return; // (not the entity's own place)
    }
    f.active = true;
    f.ent = ch->entnum;
    f.model = e->model;
    f.offset = offset;
    f.last = at;
    f.time = realtime;
}

// The channels were all cleared (a disconnect, a map change, a game switch): the voices let go of them now, not at the
// next mix. Until then a voice kept a cleared channel (its place 0 0 0) that the listener's update (VR_SndListener, which
// runs first) placed in the world: after a game switch, on a world model already reset (ROUND21.md, "The Loading...
// crash, root cause").
extern "C" void VR_SndStopAll(void)
{
    if(live && live->ok)
    {
        releaseAll();
    }
}

extern "C" int VR_SndKeepStatics(void)
{
    return running() ? 1 : 0;
}

extern "C" int VR_SndMixEnd(int painted, int endtime)
{
    if(!running() || endtime <= painted)
    {
        return endtime;
    }
    const int frame = live->frame;
    return painted + (endtime - painted) / frame * frame;
}

extern "C" int VR_SndOwns(const channel_t* ch)
{
    return live && live->voiceOf[ch - snd_channels] >= 0 ? 1 : 0;
}

extern "C" void VR_SndPaint(portable_samplepair_t* buffer, int start, int end)
{
    if(!live)
    {
        return;
    }
    Live& L = *live;
    L.heldLen = 0;
    if(!running())
    {
        if(L.ok)
        {
            releaseAll();
        }
        return;
    }
    QVR_PROFILE("spatial audio mix");
    const double t0 = Sys_DoubleTime();
    const double benchStart = bench::now();
    const BenchAllocs allocs;
    const int count = end - start;
    // vr_snd_fullband: the voices are added after Quake's 11 kHz lowpass (VR_SndBypass), not to its input.
    if(VR_SndFullBand() >= 1)
    {
        if(static_cast<int>(L.held.size()) < count)
        {
            L.held.resize(count);
        }
        ZA_MEMSET(L.held.data(), 0, sizeof(portable_samplepair_t) * static_cast<za::SizeT>(count));
        L.heldLen = count;
        buffer = L.held.data();
    }
    int written = 0;
    if(L.carryLen > 0 && L.carryStart == start)
    {
        written = za::min(count, L.carryLen);
        for(int i = 0; i < written; i++)
        {
            buffer[i].left += static_cast<int>(L.carryL[i]);
            buffer[i].right += static_cast<int>(L.carryR[i]);
        }
        za::copy(L.carryL.begin() + written, L.carryL.begin() + L.carryLen, L.carryL.begin());
        za::copy(L.carryR.begin() + written, L.carryR.begin() + L.carryLen, L.carryR.begin());
        L.carryLen -= written;
        L.carryStart += written;
    }
    else
    {
        L.carryLen = 0;
    }
    if(written >= count)
    {
        return;
    }
    const int time = start + written;
    const double selectStart = bench::now();
    selectVoices(time);

    const Features f = featuresFromCvars();
    const float volume = sfxvolume.value;
    int used = 0;
    for(int v = 0; v < Mixer::maxVoices; v++)
    {
        const int c = L.channelOf[v];
        if(c < 0)
        {
            continue;
        }
        used++;
        L.mixer.set(v, channelInput(c, v, f, volume));
    }
    L.voicesUsed = used;
    bench::add(bench::Select, selectStart);

    const int remain = count - written;
    const int frame = L.frame;
    const int blocks = (remain + frame - 1) / frame;
    const int rendered = blocks * frame;
    const double renderStart = bench::now();
    renderVoices(L.mixer, blocks, L.listener, f, L.haveReverb ? &L.reverb : nullptr, L.mixL, L.mixR, L.revL, L.revR,
        L.antiAlias);
    if(bench::on)
    {
        const Mixer::Times mt = L.mixer.takeTimes();
        bench::addSeconds(bench::Voices, mt.voices);
        bench::addSeconds(bench::Reverb, mt.reverb);
        bench::addSeconds(bench::ReverbConv, mt.reverbConv);
        bench::addSeconds(bench::ReverbDecode, mt.reverbDecode);
        bench::addSeconds(bench::AntiAlias, Sys_DoubleTime() - renderStart - mt.voices - mt.reverb);
        for(int k = 0; k < 4; k++)
        {
            bench::addSeconds(bench::CpuRead + k, mt.cpu[k]);
        }
        bench::addCount(bench::Allocs, mt.cpu[4]);
        bench::painted(rendered, used);
    }

    // The channels as Quake would have left them at the end of what was rendered.
    const int renderedEnd = time + rendered;
    for(int v = 0; v < Mixer::maxVoices; v++)
    {
        const int c = L.channelOf[v];
        if(c < 0)
        {
            continue;
        }
        channel_t* ch = &snd_channels[c];
        if(L.mixer.ended(v))
        {
            ch->sfx = nullptr;
            releaseVoice(v);
            continue;
        }
        const sfxcache_t* sc = static_cast<const sfxcache_t*>(Cache_Check(&ch->sfx->cache));
        const double pos = L.mixer.position(v);
        ch->pos = static_cast<int>(pos);
        const int length = sc ? sc->length : ch->pos;
        ch->end = renderedEnd + static_cast<int>((length - pos) / za::max(0.01f, L.mixer.dopplerFactor(v)));
    }

    for(int i = 0; i < remain; i++)
    {
        buffer[written + i].left += static_cast<int>(L.mixL[i]);
        buffer[written + i].right += static_cast<int>(L.mixR[i]);
    }
    L.carryLen = rendered - remain;
    L.carryStart = end;
    for(int i = 0; i < L.carryLen; i++)
    {
        L.carryL[i] = L.mixL[remain + i];
        L.carryR[i] = L.mixR[remain + i];
    }
    const double ms = (Sys_DoubleTime() - t0) * 1000.0;
    L.mixMs += (ms - L.mixMs) * 0.05;
    bench::add(bench::Spatial, benchStart);
}

namespace
{

// Sounds made from Quake's own as they load (S_LoadSound, when no file has the name): one of id's recordings played
// slower, so lower and a little longer. Nothing of id's is shipped or written; a file of the name wins. (The mantle's
// deeper grunts, vr_climb_mantle_grunt_sound: vr_climb.cpp.)
struct DerivedSound
{
    const char* name;
    const char* source;
    float rate;
};

constexpr DerivedSound derivedSounds[] = {
    {"vr/derived/plyrjmp8_low.wav", "player/plyrjmp8.wav", 0.8f}, // the jump's grunt, ~4 semitones down
    {"vr/derived/land2_low.wav", "player/land2.wav", 0.88f},      // a hard landing's, ~2 semitones down
};

} // namespace

extern "C" const char* VR_SndDerived(const char* name, float* rate)
{
    for(const DerivedSound& d : derivedSounds)
    {
        if(!strcmp(d.name, name))
        {
            *rate = d.rate;
            return d.source;
        }
    }
    return nullptr;
}

// vr_snd_fullband: 0 Quake's 11 kHz lowpass on every sound (as it was), 1 not on the voices (the default), 2 on none.
extern "C" int VR_SndFullBand(void)
{
    return za::clamp(static_cast<int>(vr_snd_fullband.value), 0, 2);
}

namespace qvr::audio
{
namespace
{

// VR_SndBandLimit's tables of taps, one for each fracstep (each depends on it alone, and every sound of a rate has the
// same one): made at the first sound of the rate, kept. A table: 256 phases of 2 x half taps.
struct BandLimitTables
{
    za::Vector<int> fracsteps;
    za::Vector<int> halves;
    za::Vector<za::SizeT> offsets; // into rows
    za::Vector<float> rows;
    auto members() { return mem::list(fracsteps, halves, offsets, rows); }
};
mem::Cache<BandLimitTables> bandLimitTables{"spatial audio band-limiting", mem::Never};

constexpr int bandLimitPhases = 256;

// The table for `fracstep` (made now if it isn't there): its rows, and `half` its taps each side.
const float* bandLimitTable(int fracstep, int& half)
{
    BandLimitTables& t = bandLimitTables;
    for(za::SizeT k = 0; k < t.fracsteps.size(); k++)
    {
        if(t.fracsteps[k] == fracstep)
        {
            half = t.halves[k];
            return t.rows.data() + t.offsets[k];
        }
    }
    constexpr double pi = 3.14159265358979;
    constexpr int phases = bandLimitPhases;
    const double ratio = fracstep / 256.0;                     // the sound's samples an output sample
    const double cutoff = 0.475 / za::max(1.0, ratio);         // cycles a sample of the sound
    half = static_cast<int>(za::ceil(15.2 / cutoff));          // samples of the sound each side
    const int taps = 2 * half;
    const za::SizeT offset = t.rows.size();
    t.rows.resize(offset + static_cast<za::SizeT>(phases) * taps, 0.f);
    const int h = half;
    float* table = t.rows.data() + offset;
    // (Each phase on its own: the pool's threads share them.)
    jobs::parallelFor(audioTaps, static_cast<za::SizeT>(phases), 16, [&](za::SizeT begin, za::SizeT end) {
        for(za::SizeT pp = begin; pp < end; pp++)
        {
            const int p = static_cast<int>(pp);
            float* row = table + static_cast<za::SizeT>(p) * taps;
            double sum = 0.0;
            for(int j = 0; j < taps; j++)
            {
                const double tt = p / static_cast<double>(phases) + (h - 1 - j); // the output's time minus the sample's
                const double sinc = tt == 0.0 ? 2.0 * cutoff : za::sin(2.0 * pi * cutoff * tt) / (pi * tt);
                const double x = tt / h;
                const double w = za::abs(x) >= 1.0 ? 0.0 : 0.42 + 0.5 * za::cos(pi * x) + 0.08 * za::cos(2.0 * pi * x);
                row[j] = static_cast<float>(sinc * w);
                sum += sinc * w;
            }
            for(int j = 0; j < taps; j++)
            {
                row[j] = static_cast<float>(row[j] / sum);
            }
        }
    });
    t.fracsteps.pushBack(fracstep);
    t.halves.pushBack(half);
    t.offsets.pushBack(offset);
    return table;
}

// VR_SndBandLimit's samples as floats (scratch: the main thread, as sounds load).
struct BandLimitScratch
{
    za::Vector<float> src;
    auto members() { return mem::list(src); }
};
mem::Scratch<BandLimitScratch> bandLimitScratch{"spatial audio band-limiting"};

} // namespace
} // namespace qvr::audio

// A sound resampled to the mix's rate band-limited, beside Quake's copy (ResampleSfx: each sample held, so an 11 kHz
// sound's spectrum repeats above 5.5 kHz: images that Quake's 11 kHz lowpass takes out and a mix without it plays as
// hiss and aliasing). A windowed sinc (Blackman, ~30 zero crossings a side): -6 dB at 0.475 of the lower of the two
// rates (5.24 kHz for an 11025 Hz sound, 10.5 for 22050), flat to ~0.43 of it, under -70 dB from ~0.52; DC gain 1.
// Output i is at the sound's sample i x fracstep / 256, ResampleSfx's timing (so a channel passes between Quake's mix
// and a voice where it was), so its fraction is one of 256 phases, each with its own row of taps. A looping sound's
// samples past its end are its loop's start again, and before its loop (from the loop on) the loop's end. Kept at
// 1/S_FULLBAND_SCALE: a sound clipped at full scale (id's explosions) overshoots it band-limited, and clipped again it
// was hiss (r_exp3: its share above 5.8 kHz -54 dB, -89 kept whole). The taps' table is made once a rate
// (bandLimitTable); the samples are shared out among the pool's threads (each output on its own).
extern "C" void VR_SndBandLimit(const unsigned char* data, int width, int samples, int loopstart, int fracstep,
    short* out, int outcount)
{
    if(samples <= 0 || outcount <= 0 || fracstep <= 0)
    {
        return;
    }
    const double started = Sys_DoubleTime();
    int half = 0;
    const float* table = bandLimitTable(fracstep, half);
    const int taps = 2 * half;

    za::Vector<float>& src = bandLimitScratch.src;
    src.clear();
    src.resize(static_cast<za::SizeT>(samples), 0.f);
    for(int n = 0; n < samples; n++)
    {
        src[n] = width == 2 ? static_cast<float>(static_cast<short>(data[2 * n] | (data[2 * n + 1] << 8)))
                            : static_cast<float>((static_cast<int>(data[n]) - 128) * 256);
    }
    const int loopLen = loopstart >= 0 && loopstart < samples ? samples - loopstart : 0;
    const auto at = [&](long long n, bool inLoop) -> float {
        if(loopLen > 0 && (n >= samples || (inLoop && n < loopstart)))
        {
            long long k = (n - loopstart) % loopLen;
            n = loopstart + (k < 0 ? k + loopLen : k);
        }
        return n < 0 || n >= samples ? 0.f : src[static_cast<za::SizeT>(n)];
    };

    jobs::parallelFor(audioBandlimit, static_cast<za::SizeT>(outcount), 4096, [&](za::SizeT begin, za::SizeT end) {
        for(int i = static_cast<int>(begin); i < static_cast<int>(end); i++)
        {
            const long long pos = static_cast<long long>(i) * fracstep; // in 256ths of the sound's samples
            const long long base = pos >> 8;
            const float* row = table + static_cast<za::SizeT>(pos & 255) * taps;
            const long long first = base - half + 1;
            float y = 0.f;
            const bool inLoop = loopLen > 0 && base >= loopstart;
            if(first >= (inLoop ? loopstart : 0) && first + taps <= samples)
            {
                const float* x = src.data() + first;
                for(int j = 0; j < taps; j++)
                {
                    y += row[j] * x[j];
                }
            }
            else
            {
                for(int j = 0; j < taps; j++)
                {
                    y += row[j] * at(first + j, inLoop);
                }
            }
            out[i] = static_cast<short>(za::clamp(static_cast<int>(za::floor(y / S_FULLBAND_SCALE + 0.5f)), -32768, 32767));
        }
    });
    const double ms = (Sys_DoubleTime() - started) * 1000.0;
    bandLimitStats.sounds++;
    bandLimitStats.ms += ms;
    bandLimitStats.worstMs = za::max(bandLimitStats.worstMs, ms);
    bandLimitStats.bytes += static_cast<long long>(outcount) * 2;
}

namespace qvr::audio
{
namespace
{

// The band-limited copies read between their samples (slow motion, Doppler; vr_snd_fullband): a 32-tap windowed sinc
// (Blackman, -6 dB at 0.47 of the rate) at 512 fractions of a sample. Linear interpolation, read at a quarter speed,
// left images of the sound's highs at -21 to -40 dB (8-14 kHz), which Quake's 11 kHz lowpass used to take out.
constexpr int interpTaps = 32;
constexpr int interpHalf = interpTaps / 2;
constexpr int interpPhases = 512;
za::Vector<float> interpTable; // (made at init)

} // namespace

void makeInterpTable()
{
    constexpr double pi = 3.14159265358979;
    constexpr double cutoff = 0.47;
    interpTable.resize(static_cast<za::SizeT>(interpPhases + 1) * interpTaps, 0.f);
    for(int p = 0; p <= interpPhases; p++)
    {
        float* row = interpTable.data() + static_cast<za::SizeT>(p) * interpTaps;
        double sum = 0.0;
        for(int j = 0; j < interpTaps; j++)
        {
            const double t = p / static_cast<double>(interpPhases) + (interpHalf - 1 - j);
            const double sinc = t == 0.0 ? 2.0 * cutoff : za::sin(2.0 * pi * cutoff * t) / (pi * t);
            const double x = t / interpHalf;
            const double w = za::abs(x) >= 1.0 ? 0.0 : 0.42 + 0.5 * za::cos(pi * x) + 0.08 * za::cos(2.0 * pi * x);
            row[j] = static_cast<float>(sinc * w);
            sum += sinc * w;
        }
        for(int j = 0; j < interpTaps; j++)
        {
            row[j] = static_cast<float>(row[j] / sum);
        }
    }
}

float fullBandAt(const short* x, int length, int loop, double pos)
{
    const int i0 = static_cast<int>(pos);
    const double frac = pos - i0;
    if(interpTable.empty())
    {
        const int i1 = i0 + 1 < length ? i0 + 1 : (loop >= 0 ? loop : i0);
        return static_cast<float>(x[i0] + (x[i1] - x[i0]) * frac);
    }
    const float* row = interpTable.data() + static_cast<za::SizeT>(frac * interpPhases + 0.5) * interpTaps;
    const int first = i0 - interpHalf + 1;
    float y = 0.f;
    if(first >= 0 && first + interpTaps <= length)
    {
        // Eight taps at a time (SSE2): two sums of four lanes, added at the end (the same taps as one by one, summed in
        // another order: within a few units in the last place).
        static_assert(interpTaps % 8 == 0);
        const short* s = x + first;
        __m128 acc0 = _mm_setzero_ps();
        __m128 acc1 = _mm_setzero_ps();
        for(int j = 0; j < interpTaps; j += 8)
        {
            const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(s + j));
            const __m128 lo = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpacklo_epi16(v, v), 16));
            const __m128 hi = _mm_cvtepi32_ps(_mm_srai_epi32(_mm_unpackhi_epi16(v, v), 16));
            acc0 = _mm_add_ps(acc0, _mm_mul_ps(_mm_loadu_ps(row + j), lo));
            acc1 = _mm_add_ps(acc1, _mm_mul_ps(_mm_loadu_ps(row + j + 4), hi));
        }
        __m128 sum = _mm_add_ps(acc0, acc1);
        sum = _mm_add_ps(sum, _mm_movehl_ps(sum, sum));
        sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, 1));
        return _mm_cvtss_f32(sum);
    }
    // Past the end: the loop's start again (or silence); before the start: a loop from 0's end (or silence).
    const int loopLen = loop >= 0 && loop < length ? length - loop : 0;
    for(int j = 0; j < interpTaps; j++)
    {
        int n = first + j;
        if(n >= length)
        {
            n = loopLen > 0 ? loop + (n - length) % loopLen : -1;
        }
        else if(n < 0)
        {
            n = loop == 0 && loopLen > 0 ? n + length : -1;
        }
        y += n >= 0 && n < length ? row[j] * static_cast<float>(x[n]) : 0.f;
    }
    return y;
}

} // namespace qvr::audio

extern "C" float VR_SndFullBandAt(const short* data, int length, int loopstart, double pos)
{
    return qvr::audio::fullBandAt(data, length, loopstart, pos);
}

namespace qvr::audio
{
namespace
{

// The limiter's settings from the rate and the release (cvar); its state reset when they change.
// `stretch`: the release times this (slow motion's output: its release in the game's time).
void configureBus(BusLimiter& b, int rate, float stretch)
{
    const float release = za::clamp(vr_snd_limiter_release.value, 0.02f, 1.f) * stretch;
    const int look = za::clamp(static_cast<int>(static_cast<float>(rate) * 0.003f), 1, BusLimiter::maxLook); // 3 ms
    const float coef = 1.f - za::exp(-1.f / (static_cast<float>(rate) * release));
    if(b.look == look && b.rate == rate && b.releaseCoef == coef)
    {
        return;
    }
    const bool fresh = b.look != look || b.rate != rate;
    b.rate = rate;
    b.releaseCoef = coef;
    if(!fresh)
    {
        return;
    }
    b.look = look;
    b.n = 0;
    za::fill(b.delayL.begin(), b.delayL.end(), 0.f);
    za::fill(b.delayR.begin(), b.delayR.end(), 0.f);
    za::fill(b.box.begin(), b.box.end(), 1.f);
    b.boxSum = static_cast<double>(look);
    b.dqFront = 0;
    b.dqCount = 0;
    b.gain = 1.f;
}

// One sample through the limiter: `l` and `r` in, the one from its look-ahead back out at the gain it needs.
void limitStep(BusLimiter& b, float& l, float& r, float peak, float threshold)
{
    const int look = b.look;
    // The gain this sample needs, into the sliding minimum over the look-ahead.
    const float need = peak > threshold ? threshold / peak : 1.f;
    while(b.dqCount > 0 && b.dqGain[(b.dqFront + b.dqCount - 1) % BusLimiter::maxLook] >= need)
    {
        b.dqCount--;
    }
    const int back = (b.dqFront + b.dqCount) % BusLimiter::maxLook;
    b.dqGain[back] = need;
    b.dqAt[back] = b.n;
    b.dqCount++;
    while(b.dqAt[b.dqFront] <= b.n - look)
    {
        b.dqFront = (b.dqFront + 1) % BusLimiter::maxLook;
        b.dqCount--;
    }
    const float held = b.dqGain[b.dqFront];
    // Averaged over the same length: smooth, and never above what the delayed sample needs.
    const int slot = static_cast<int>(b.n % look);
    b.boxSum += static_cast<double>(held) - static_cast<double>(b.box[slot]);
    b.box[slot] = held;
    const float smooth = za::min(1.f, static_cast<float>(b.boxSum / static_cast<double>(look)));
    b.gain = smooth < b.gain ? smooth : b.gain + (smooth - b.gain) * b.releaseCoef;
    b.minGain = za::min(b.minGain, b.gain);
    // The delay: this sample in, the one look - 1 samples back out.
    b.delayL[slot] = l;
    b.delayR[slot] = r;
    const int out = static_cast<int>((b.n + 1) % look);
    l = b.delayL[out] * b.gain;
    r = b.delayR[out] * b.gain;
    b.n++;
}

} // namespace
} // namespace qvr::audio

extern "C" void VR_SndBus(portable_samplepair_t* buffer, int count)
{
    constexpr int ceiling = 32767 * 256;
    constexpr int bottom = -32768 * 256;
    const bool limit = live && shm && vr_snd_limiter.value != 0.f;
    if(live && live->capture.running)
    {
        Capture& c = live->capture;
        const int measured = za::max(0, za::min(count, c.wanted - c.busSeen));
        const portable_samplepair_t* held = live->heldLen >= measured ? live->held.data() : nullptr; // (vr_snd_fullband)
        for(int i = 0; i < measured; i++)
        {
            const int peak = held ? za::max(za::abs(buffer[i].left + held[i].left), za::abs(buffer[i].right + held[i].right))
                                  : za::max(za::abs(buffer[i].left), za::abs(buffer[i].right));
            c.busPeak = za::max(c.busPeak, static_cast<float>(peak));
            c.busOver += peak > ceiling ? 1 : 0;
        }
        c.busSeen += measured;
    }
    // With the limiter, the effects keep what is over full scale here (the 6 dB under the output's full scale that
    // Quake keeps for the music): the limiter on the whole mix (VR_SndLimit) brings it down. (The wide clamp: only so
    // that the filters on the way can't overflow.)
    const int hi = limit ? ceiling * 8 : ceiling;
    const int lo = limit ? bottom * 8 : bottom;
    for(int i = 0; i < count; i++)
    {
        buffer[i].left = za::clamp(buffer[i].left, lo, hi) / 2;
        buffer[i].right = za::clamp(buffer[i].right, lo, hi) / 2;
    }
}

extern "C" void VR_SndBypass(portable_samplepair_t* buffer, int count)
{
    if(!live || live->heldLen <= 0)
    {
        return;
    }
    // VR_SndBus's clip and halving, on the voices held out of Quake's lowpass (vr_snd_fullband), then added.
    constexpr int ceiling = 32767 * 256;
    constexpr int bottom = -32768 * 256;
    const bool limit = shm && vr_snd_limiter.value != 0.f;
    const int hi = limit ? ceiling * 8 : ceiling;
    const int lo = limit ? bottom * 8 : bottom;
    const portable_samplepair_t* held = live->held.data();
    const int n = za::min(count, live->heldLen);
    for(int i = 0; i < n; i++)
    {
        buffer[i].left += za::clamp(held[i].left, lo, hi) / 2;
        buffer[i].right += za::clamp(held[i].right, lo, hi) / 2;
    }
    live->heldLen = 0;
}

extern "C" void VR_SndLimit(portable_samplepair_t* buffer, int count)
{
    // The runtime's menu pausing the game (setDuck): the whole mix faded, the music too.
    if(shm && (duck.gain != duck.target || duck.gain != 1.f))
    {
        const float step = 10.f / static_cast<float>(za::max(shm->speed, 1));
        for(int i = 0; i < count; i++)
        {
            duck.gain = duck.gain < duck.target ? za::min(duck.target, duck.gain + step)
                                                : za::max(duck.target, duck.gain - step);
            buffer[i].left = static_cast<int>(static_cast<float>(buffer[i].left) * duck.gain);
            buffer[i].right = static_cast<int>(static_cast<float>(buffer[i].right) * duck.gain);
        }
    }
    if(!live || !shm)
    {
        return;
    }
    const bench::Timed timed{bench::Limit};
    const BenchAllocs allocs;
    BusLimiter& b = live->bus;
    Capture& c = live->capture;
    const int measured = c.running ? za::max(0, za::min(count, c.wanted - static_cast<int>(c.left.size()))) : 0;
    const bool limit = vr_snd_limiter.value != 0.f;
    if(limit)
    {
        configureBus(b, shm->speed, 1.f);
    }
    else
    {
        b.look = 0; // (fresh when it's back on)
        b.gain = 1.f;
    }
    constexpr int ceiling = 32767 * 256;
    constexpr int bottom = -32768 * 256;
    const float threshold = BusLimiter::fullScale * za::pow(10.f, za::clamp(vr_snd_limiter_ceiling.value, -12.f, 0.f) / 20.f);
    for(int i = 0; i < count; i++)
    {
        float l = static_cast<float>(buffer[i].left);
        float r = static_cast<float>(buffer[i].right);
        const float peak = za::max(za::abs(l), za::abs(r));
        if(peak > BusLimiter::fullScale)
        {
            b.over++;
        }
        if(i < measured)
        {
            c.outPeak = za::max(c.outPeak, peak);
        }
        if(limit)
        {
            limitStep(b, l, r, peak, threshold);
            if(i < measured)
            {
                c.minGain = za::min(c.minGain, b.gain);
            }
        }
        const int li = static_cast<int>(l);
        const int ri = static_cast<int>(r);
        if(li > ceiling || li < bottom || ri > ceiling || ri < bottom)
        {
            b.clipped++;
            if(i < measured)
            {
                c.outClipped++;
            }
        }
        buffer[i].left = za::clamp(li, bottom, ceiling);
        buffer[i].right = za::clamp(ri, bottom, ceiling);
    }
}

extern "C" void VR_SndCapture(const portable_samplepair_t* buffer, int count)
{
    if(!live || !live->capture.running)
    {
        return;
    }
    Capture& c = live->capture;
    for(int i = 0; i < count && static_cast<int>(c.left.size()) < c.wanted; i++)
    {
        c.left.pushBack(static_cast<float>(buffer[i].left) / 256.f);
        c.right.pushBack(static_cast<float>(buffer[i].right) / 256.f);
    }
    if(static_cast<int>(c.left.size()) >= c.wanted)
    {
        finishCapture(c, false);
    }
}

// ----------------------------------------------------------------------------
// The game-time mix (the recording's WAV, vr_snd_capture_game)

extern "C" void VR_SndGameMix(const portable_samplepair_t* buffer, int count)
{
    if(!live || !shm)
    {
        return;
    }
    Capture& c = live->gameCapture;
    GameWav& w = live->wav;
    if(!c.running && !w.file)
    {
        return;
    }
    BusLimiter& b = live->gameBus;
    const bool limit = vr_snd_limiter.value != 0.f;
    if(limit)
    {
        configureBus(b, shm->speed, 1.f);
    }
    const float threshold = BusLimiter::fullScale * za::pow(10.f, za::clamp(vr_snd_limiter_ceiling.value, -12.f, 0.f) / 20.f);
    constexpr float ceiling = 8388607.f; // (24 bits: paintbuffer's units are 16-bit samples times 256)
    w.bytes.clear();
    for(int i = 0; i < count; i++)
    {
        float l = static_cast<float>(buffer[i].left);
        float r = static_cast<float>(buffer[i].right);
        if(limit)
        {
            limitStep(b, l, r, za::max(za::abs(l), za::abs(r)), threshold);
        }
        l = za::clamp(l, -ceiling - 1.f, ceiling);
        r = za::clamp(r, -ceiling - 1.f, ceiling);
        if(c.running && static_cast<int>(c.left.size()) < c.wanted)
        {
            c.left.pushBack(l / 256.f);
            c.right.pushBack(r / 256.f);
            c.minGain = za::min(c.minGain, b.gain);
        }
        if(w.file)
        {
            const int s[2] = {static_cast<int>(l), static_cast<int>(r)};
            for(const int v : s)
            {
                w.bytes.pushBack(static_cast<unsigned char>(v & 0xff));
                w.bytes.pushBack(static_cast<unsigned char>((v >> 8) & 0xff));
                w.bytes.pushBack(static_cast<unsigned char>((v >> 16) & 0xff));
            }
        }
    }
    if(c.running && static_cast<int>(c.left.size()) >= c.wanted)
    {
        finishCapture(c, true);
    }
    if(w.file && !w.bytes.empty())
    {
        constexpr long long most = 0x7fffffffLL / 6 - 64; // (a WAV's sizes are 32 bits)
        const bool wrote = fwrite(w.bytes.data(), 1, w.bytes.size(), w.file) == w.bytes.size();
        w.frames += count;
        if(!wrote || w.frames >= most)
        {
            Con_Printf("vr_timescale_wav: %s ended (%s)\n", w.path.cStr(), wrote ? "2 GB" : "can't write");
            stopGameWav();
        }
    }
}

namespace qvr::audio
{

void startGameWav(const char* path)
{
    stopGameWav();
    if(!live || !shm)
    {
        return;
    }
    GameWav& w = live->wav;
    w.file = fopen(path, "wb");
    if(!w.file)
    {
        Con_Printf("vr_timescale_wav: can't write %s\n", path);
        return;
    }
    w.path = path;
    w.frames = 0;
    writeWav24Header(w.file, shm->speed, 0);
    live->gameBus.look = 0; // (fresh)
    Con_Printf("vr_timescale_wav: the game-time mix to %s\n", path);
}

void stopGameWav()
{
    if(!live || !live->wav.file)
    {
        return;
    }
    GameWav& w = live->wav;
    const int rate = shm ? shm->speed : 44100;
    fseek(w.file, 0, SEEK_SET);
    writeWav24Header(w.file, rate, w.frames);
    fclose(w.file);
    w.file = nullptr;
    Con_Printf("vr_timescale_wav: %s, %.1f s\n", w.path.cStr(), static_cast<double>(w.frames) / rate);
}

} // namespace qvr::audio

// ----------------------------------------------------------------------------
// The game-time render (Shadow; S_PaintChannels, VR_SndShadow)

namespace qvr::audio
{
namespace
{

void shadowStopVoice(Shadow& sh, int v)
{
    const int c = sh.channelOf[v];
    if(c >= 0)
    {
        sh.channels[c].voice = -1;
    }
    sh.channelOf[v] = -1;
    sh.mixer.stop(v);
}

// Its mixer made as the live one is (rate, frames, HRTF, reverb); false while there is none.
bool shadowMixer(Shadow& sh)
{
    Live& L = *live;
    if(!running())
    {
        return false;
    }
    if(sh.tried && sh.rate == L.rate && sh.frame == L.frame && sh.sofa == L.sofa && sh.quality == L.quality)
    {
        return sh.mixer.valid();
    }
    for(int v = 0; v < Mixer::maxVoices; v++)
    {
        if(sh.channelOf[v] >= 0)
        {
            shadowStopVoice(sh, v);
        }
    }
    sh.tried = true;
    sh.rate = L.rate;
    sh.frame = L.frame;
    sh.sofa = L.sofa;
    sh.quality = L.quality;
    sh.mixer.destroy();
    if(!sh.mixer.create(L.rate, L.frame, L.sofa.cStr()))
    {
        return false;
    }
    const Quality q = qualityPreset(L.quality);
    sh.mixer.setReverb(q.type, q.order, q.duration);
    sh.carryLen = 0;
    return true;
}

// From the live channels as they are now (the render starting).
void shadowReset(Shadow& sh)
{
    for(int v = 0; v < Mixer::maxVoices; v++)
    {
        if(sh.channelOf[v] >= 0)
        {
            shadowStopVoice(sh, v);
        }
    }
    for(int c = 0; c < MAX_CHANNELS; c++)
    {
        ShadowChannel& s = sh.channels[c];
        s = ShadowChannel{};
        if(c < total_channels && snd_channels[c].sfx)
        {
            s.sfx = snd_channels[c].sfx;
            s.pos = snd_channels[c].pos;
            s.done = false;
        }
        sh.started[c] = false;
    }
    sh.owed = 0.0;
    sh.carryLen = 0;
    sh.antiAlias.reset();
}

// `count` samples of the game-time mix (at most a paint call's) into sh.buffer.
void shadowRender(Shadow& sh, int count)
{
    Live& L = *live;
    portable_samplepair_t* out = sh.buffer.data();
    ZA_MEMSET(out, 0, sizeof(portable_samplepair_t) * static_cast<za::SizeT>(count));
    // vr_snd_fullband: the voices kept out of its lowpass (as the live mix's, VR_SndBypass), Quake's mix in `out`.
    const int fullBand = VR_SndFullBand();
    portable_samplepair_t* voicesOut = out;
    if(fullBand >= 1)
    {
        voicesOut = sh.held.data();
        ZA_MEMSET(voicesOut, 0, sizeof(portable_samplepair_t) * static_cast<za::SizeT>(count));
    }

    // New sounds, sounds stopped (S_StopSound, S_StopAllSounds: no end left).
    for(int c = 0; c < MAX_CHANNELS; c++)
    {
        ShadowChannel& s = sh.channels[c];
        const channel_t* ch = &snd_channels[c];
        const bool fresh = sh.started[c] || (ch->sfx && ch->sfx != s.sfx);
        const bool stopped = !ch->sfx && ch->end == 0;
        if(fresh || (stopped && !s.done))
        {
            if(s.voice >= 0)
            {
                shadowStopVoice(sh, s.voice);
            }
            s.sfx = fresh ? ch->sfx : nullptr;
            s.pos = sh.started[c] ? sh.startPos[c] : ch->pos;
            s.done = !s.sfx;
        }
        sh.started[c] = false;
    }

    // The voices: the channels the live mix voices (the same sound), and those still sounding here whose live
    // channel ended.
    if(shadowMixer(sh))
    {
        Features f = featuresFromCvars();
        f.rate = 1.f; // (the game's time: the normal speed)
        const float volume = sfxvolume.value;
        for(int c = 0; c < MAX_CHANNELS; c++)
        {
            ShadowChannel& s = sh.channels[c];
            const channel_t* ch = &snd_channels[c];
            const int liveVoice = c < total_channels ? L.voiceOf[c] : -1;
            const bool voiced = !s.done && liveVoice >= 0 && ch->sfx == s.sfx;
            const bool orphan = !s.done && s.voice >= 0 && !ch->sfx;
            if(s.voice >= 0 && !voiced && !orphan)
            {
                s.pos = static_cast<int>(sh.mixer.position(s.voice)); // (Quake's mix goes on from there)
                shadowStopVoice(sh, s.voice);
            }
            if(voiced && s.voice < 0)
            {
                const sfxcache_t* sc = S_LoadSound(s.sfx);
                int v = 0;
                while(v < Mixer::maxVoices && sh.channelOf[v] >= 0)
                {
                    v++;
                }
                if(sc && v < Mixer::maxVoices)
                {
                    sh.mixer.start(v, sc, s.pos);
                    sh.channelOf[v] = c;
                    s.voice = v;
                }
            }
            if(voiced && s.voice >= 0)
            {
                sh.mixer.set(s.voice, channelInput(c, liveVoice, f, volume));
            }
        }

        const int frame = sh.frame;
        int written = za::min(count, sh.carryLen);
        for(int i = 0; i < written; i++)
        {
            voicesOut[i].left += static_cast<int>(sh.carryL[i]);
            voicesOut[i].right += static_cast<int>(sh.carryR[i]);
        }
        za::copy(sh.carryL.begin() + written, sh.carryL.begin() + sh.carryLen, sh.carryL.begin());
        za::copy(sh.carryR.begin() + written, sh.carryR.begin() + sh.carryLen, sh.carryR.begin());
        sh.carryLen -= written;
        const int remain = count - written;
        if(remain > 0)
        {
            const int blocks = (remain + frame - 1) / frame;
            const int rendered = blocks * frame;
            renderVoices(sh.mixer, blocks, L.listener, f, L.haveReverbSecond ? &L.reverbSecond : nullptr, sh.mixL, sh.mixR,
                sh.revL, sh.revR, sh.antiAlias);
            for(int i = 0; i < remain; i++)
            {
                voicesOut[written + i].left += static_cast<int>(sh.mixL[i]);
                voicesOut[written + i].right += static_cast<int>(sh.mixR[i]);
            }
            sh.carryLen = rendered - remain;
            for(int i = 0; i < sh.carryLen; i++)
            {
                sh.carryL[i] = sh.mixL[remain + i];
                sh.carryR[i] = sh.mixR[remain + i];
            }
        }
        for(int v = 0; v < Mixer::maxVoices; v++)
        {
            const int c = sh.channelOf[v];
            if(c < 0)
            {
                continue;
            }
            ShadowChannel& s = sh.channels[c];
            s.pos = static_cast<int>(sh.mixer.position(v));
            if(sh.mixer.ended(v))
            {
                s.done = true;
                shadowStopVoice(sh, v);
            }
        }
    }
    else
    {
        for(int v = 0; v < Mixer::maxVoices; v++)
        {
            if(sh.channelOf[v] >= 0)
            {
                ShadowChannel& s = sh.channels[sh.channelOf[v]];
                s.pos = static_cast<int>(sh.mixer.position(v));
                shadowStopVoice(sh, v);
            }
        }
        sh.carryLen = 0;
    }

    // Quake's own mix of the rest (SND_PaintChannelFrom8/16's volumes, read at the normal speed).
    for(int c = 0; c < MAX_CHANNELS; c++)
    {
        ShadowChannel& s = sh.channels[c];
        const channel_t* ch = &snd_channels[c];
        if(s.done || s.voice >= 0 || (!ch->leftvol && !ch->rightvol))
        {
            continue;
        }
        const sfxcache_t* sc = S_LoadSound(s.sfx);
        if(!sc || sc->length <= 0)
        {
            s.done = true;
            continue;
        }
        const bool wide = sc->width == 2;
        const short* full = fullBand == 2 ? fullBandData(sc) : nullptr; // (as SND_PaintChannelFull)
        const float lv = static_cast<float>(wide ? ch->leftvol : za::min(ch->leftvol, 255)) * sfxvolume.value;
        const float rv = static_cast<float>(wide ? ch->rightvol : za::min(ch->rightvol, 255)) * sfxvolume.value;
        const int loop = sc->loopstart >= 0 && sc->loopstart < sc->length ? sc->loopstart : -1;
        for(int i = 0; i < count; i++)
        {
            if(s.pos >= sc->length)
            {
                if(loop < 0)
                {
                    s.done = true;
                    break;
                }
                s.pos = loop;
            }
            const float data = full ? static_cast<float>(full[s.pos] * S_FULLBAND_SCALE)
                               : wide ? static_cast<float>(reinterpret_cast<const short*>(sc->data)[s.pos])
                                      : static_cast<float>(static_cast<signed char>(sc->data[s.pos])) * 256.f;
            out[i].left += static_cast<int>(data * lv);
            out[i].right += static_cast<int>(data * rv);
            s.pos++;
        }
    }

    // The effects' bus (VR_SndBus's, without its counts), Quake's lowpass and the underwater filter (their own).
    constexpr int ceiling = 32767 * 256;
    constexpr int bottom = -32768 * 256;
    const bool limit = vr_snd_limiter.value != 0.f;
    const int hi = limit ? ceiling * 8 : ceiling;
    const int lo = limit ? bottom * 8 : bottom;
    for(int i = 0; i < count; i++)
    {
        out[i].left = za::clamp(out[i].left, lo, hi) / 2;
        out[i].right = za::clamp(out[i].right, lo, hi) / 2;
    }
    if(voicesOut != out)
    {
        for(int i = 0; i < count; i++)
        {
            voicesOut[i].left = za::clamp(voicesOut[i].left, lo, hi) / 2;
            voicesOut[i].right = za::clamp(voicesOut[i].right, lo, hi) / 2;
        }
    }
    S_ShadowFilters(out, voicesOut != out ? voicesOut : nullptr, count);
}

} // namespace
} // namespace qvr::audio

extern "C" void VR_SndShadow(int count)
{
    if(!live || !shm)
    {
        return;
    }
    Shadow& sh = live->shadow;
    if(!live->gameCapture.running && !live->wav.file)
    {
        sh.running = false;
        return;
    }
    if(!sh.running)
    {
        sh.running = true;
        if(sh.buffer.empty())
        {
            sh.buffer.resize(2048);
            sh.held.resize(2048);
            sh.mixL.resize(Mixer::maxSamples, 0.f);
            sh.mixR.resize(Mixer::maxSamples, 0.f);
            sh.revL.resize(Mixer::maxSamples, 0.f);
            sh.revR.resize(Mixer::maxSamples, 0.f);
            sh.carryL.resize(Mixer::maxSamples, 0.f);
            sh.carryR.resize(Mixer::maxSamples, 0.f);
            za::fill(sh.channelOf, -1);
        }
        shadowReset(sh);
    }
    sh.owed += static_cast<double>(count) * VR_TimeScale();
    int due = static_cast<int>(za::floor(sh.owed));
    sh.owed -= due;
    QVR_PROFILE("game-time sound render");
    const double t0 = Sys_DoubleTime();
    const bench::Timed timed{bench::Shadow};
    const BenchAllocs allocs;
    while(due > 0)
    {
        const int n = za::min(due, 2048);
        shadowRender(sh, n);
        VR_SndGameMix(sh.buffer.data(), n);
        due -= n;
    }
    sh.ms += ((Sys_DoubleTime() - t0) * 1000.0 - sh.ms) * 0.05;
}
