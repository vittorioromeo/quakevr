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
// - The simulations (vr_audiosim.hpp): the map's scene built on the pool at each new map; doors and lifts moved in
//   it; the direct paths 30 times a second and the reverb every vr_snd_reverb_interval, each a pool task, their
//   results taken under a mutex.

#include "vr_audio.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_main.hpp"
#include "vr_profile.hpp"
#include "vr_units.hpp"

#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <algorithm>
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
    ds.hrtf = hrtf;
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
    reverbAmbi.resize(reverbChannels, za::Vector<float>(frame, 0.f));
    reverbL.clear();
    reverbL.resize(frame, 0.f);
    reverbR.clear();
    reverbR.resize(frame, 0.f);
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

int Mixer::activeCount() const
{
    int n = 0;
    for(const Voice& v : voices)
    {
        n += v.active ? 1 : 0;
    }
    return n;
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

void Mixer::read(Voice& v, float* out, float step0, float step1)
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
    const auto sample = [&](int i) -> float {
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
        const int i0 = static_cast<int>(pos);
        const float frac = static_cast<float>(pos - i0);
        const int i1 = i0 + 1 < length ? i0 + 1 : (loop >= 0 ? loop : i0);
        const float s0 = sample(i0);
        out[i] = s0 + (sample(i1) - s0) * frac;
        pos += step0 + (step1 - step0) * (static_cast<float>(i) / static_cast<float>(n));
    }
    v.pos = pos;
}

void Mixer::process(Voice& v, int blocks, const Features& f, IPLHRTF laneHrtf)
{
    const int n = frame;
    for(int b = 0; b < blocks; b++)
    {
        // Doppler: the rate eased towards its target (half the way each frame), ramped within the frame.
        const float step0 = v.doppler;
        const float step1 = v.doppler + (v.dopplerTarget - v.doppler) * 0.5f;
        read(v, v.in0.data(), step0, step1);
        v.doppler = step1;

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

        // The near field: the nearer ear louder, the farther one quieter and duller (the head's shadow), by how near
        // (within a metre) and how much to the side.
        const float amount = za::min(1.f, v.closeness * qza::abs(v.lateral));
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
    }
}

void Mixer::render(int blocks, const Listener& l, const Features& f, const IPLReflectionEffectParams* reverb,
    float* outL, float* outR)
{
    if(!valid() || blocks <= 0)
    {
        return;
    }
    blocks = za::min(blocks, maxSamples / frame);
    const int count = blocks * frame;
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
    // Lane j renders the voices j, j + lanes, ... with its own HRTF (never two threads on one).
    const int used = za::min(active, lanes);
    if(used == 1)
    {
        for(int k = 0; k < active; k++)
        {
            process(voices[list[k]], blocks, f, laneHrtfs[0]);
        }
    }
    else if(used > 1)
    {
        jobs::parallelFor(static_cast<za::SizeT>(used), 1, [&](za::SizeT begin, za::SizeT end) {
            for(za::SizeT j = begin; j < end; j++)
            {
                for(int k = static_cast<int>(j); k < active; k += used)
                {
                    process(voices[list[k]], blocks, f, laneHrtfs[j]);
                }
            }
        });
    }
    for(int k = 0; k < active; k++)
    {
        Voice& v = voices[list[k]];
        // A voice that made not-numbers (it never should) is dropped from this call and its effects reset: one would
        // stay in the reverb's and the HRTF's convolutions and crackle on and on.
        float check = 0.f;
        for(int i = 0; i < count; i++)
        {
            check += v.outL[i] * 0.f + v.outR[i] * 0.f;
        }
        if(check != 0.f)
        {
            sa->iplBinauralEffectReset(v.binaural);
            sa->iplDirectEffectReset(v.direct);
            qza::fill(v.send.begin(), v.send.begin() + count, 0.f);
            continue;
        }
        for(int i = 0; i < count; i++)
        {
            outL[i] += v.outL[i];
            outR[i] += v.outR[i];
        }
    }

    if(f.reverb <= 0.f || !reflection || !decode || !reverb)
    {
        reverbSilence = 1 << 30;
        return;
    }
    IPLReflectionEffectParams p = *reverb;
    p.type = reverbType;
    p.numChannels = reverbChannels;
    p.irSize = irSize;
    orientation = coordinates(l.pos, l.fwd, l.right, l.up, f.unitsPerMetre);
    float* ambiPtr[16]{};
    for(int c = 0; c < reverbChannels && c < 16; c++)
    {
        ambiPtr[c] = reverbAmbi[c].data();
    }
    for(int b = 0; b < blocks; b++)
    {
        bool silent = true;
        za::fill(reverbIn.begin(), reverbIn.end(), 0.f);
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
        if(reverbSilence > irSize + frame)
        {
            continue;
        }
        float* inPtr[1] = {reverbIn.data()};
        IPLAudioBuffer in{1, frame, inPtr};
        IPLAudioBuffer ambi{reverbChannels, frame, ambiPtr};
        sa->iplReflectionEffectApply(reflection, &p, &in, &ambi, nullptr);
        IPLAmbisonicsDecodeEffectParams dp{};
        dp.order = reverbOrder;
        dp.hrtf = hrtf;
        dp.orientation = orientation;
        dp.binaural = f.hrtf ? IPL_TRUE : IPL_FALSE;
        float* stereoPtr[2] = {reverbL.data(), reverbR.data()};
        IPLAudioBuffer stereo{2, frame, stereoPtr};
        sa->iplAmbisonicsDecodeEffectApply(decode, &dp, &ambi, &stereo);
        const float mix = f.reverb;
        for(int i = 0; i < frame; i++)
        {
            outL[b * frame + i] += reverbL[i] * mix;
            outR[b * frame + i] += reverbR[i] * mix;
        }
    }
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

    za::Vector<float> mixL, mixR;
    za::Array<float, 1024> carryL{}, carryR{};
    int carryStart{0};
    int carryLen{0};

    double mixMs{0.0};   // the voices' render, averaged
    double lastReport{0.0};
    Capture capture;
};

Live* live{nullptr};

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
    za::fill(live->voiceOf.begin(), live->voiceOf.end(), -1);
    za::fill(live->channelOf.begin(), live->channelOf.end(), -1);
    live->mixL.clear();
    live->mixL.resize(Mixer::maxSamples, 0.f);
    live->mixR.clear();
    live->mixR.resize(Mixer::maxSamples, 0.f);
    createSim();
    live->ok = true;
    Con_DPrintf("Spatial audio: %d Hz, %d-sample frames, %.1f ms to start\n", live->rate, live->frame,
        (Sys_DoubleTime() - start) * 1000.0);
}

// The player's weapon channel's hand (QC VRGetGunChannel: CHAN_WEAPON the main hand, CHAN_WEAPON2 the off hand).
int handOf(const channel_t* ch)
{
    if(vr_snd_hands.value == 0.f || ch->entnum != cl.viewentity || cl.viewentity <= 0 || !VR_IsActive())
    {
        return -1;
    }
    if(ch->entchannel != 1 && ch->entchannel != 5)
    {
        return -1;
    }
    if(!hands::current().valid)
    {
        return -1;
    }
    return ch->entchannel == 5 ? HAND_OFF : HAND_MAIN;
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
        // ZANCLE-TODO: no selection algorithm (std::nth_element: the k kept, whatever the order of equal priorities)
        std::nth_element(L.candidates.begin(), L.candidates.begin() + k, L.candidates.begin() + n,
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
            pos = static_cast<double>(sc->length - (ch->end - time));
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
        Con_Printf("  %2d ch %3d ent %4d/%d %-24s %5.1f m  eq %.2f %.2f %.2f  doppler %.3f\n", v, c, ch->entnum,
            ch->entchannel, ch->sfx ? ch->sfx->name : "-", glm::length(d) / f.unitsPerMetre, eq[0], eq[1], eq[2],
            L.mixer.dopplerFactor(v));
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

void finishCapture()
{
    Capture& c = live->capture;
    c.running = false;
    const int rate = shm ? shm->speed : 44100;
    za::String path = za::String{com_gamedir} + "/sound_tests/capture_" + c.name + ".wav";
    COM_CreatePath(path.data());
    writeWav16(path.cStr(), c.left, c.right, rate);
    const Levels lv = measure(c.left.data(), c.right.data(), static_cast<int>(c.left.size()), rate);
    Con_Printf("vr_snd_capture %s: %d samples, rms %.1f dB, left %.1f dB, right %.1f dB, below 500 Hz %.1f dB, above 4 kHz "
               "%.1f dB (%s)\n",
        c.name.cStr(), static_cast<int>(c.left.size()), lv.rms, lv.left, lv.right, lv.low, lv.high, path.cStr());
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
    const mleaf_t* leaf = cl.worldmodel ? Mod_PointInLeaf(&L.listener.pos.x, cl.worldmodel) : nullptr;
    Con_Printf("  listener: %s at %.0f %.0f %.0f (leaf contents %d), speed %.0f units/s\n", L.head ? "the head" : "the view",
        L.listener.pos.x, L.listener.pos.y, L.listener.pos.z, leaf ? leaf->contents : 0, glm::length(L.listener.vel));
}

void capture_f()
{
    if(!live || !shm)
    {
        Con_Printf("vr_snd_capture: no sound (-nosound?)\n");
        return;
    }
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_snd_capture <seconds> [name]: the final mix to <game>/sound_tests/capture_<name>.wav, and its levels\n");
        return;
    }
    Capture& c = live->capture;
    c.wanted = static_cast<int>(za::clamp(Q_atof(Cmd_Argv(1)), 0.05f, 60.f) * static_cast<float>(shm->speed));
    c.name = Cmd_Argc() > 2 ? Cmd_Argv(2) : "capture";
    c.left.clear();
    c.right.clear();
    c.left.reserve(c.wanted);
    c.right.reserve(c.wanted);
    c.running = true;
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

// A point in a liquid (the first water, slime or lava leaf's middle): for the underwater tests.
void liquid_f()
{
    if(!cl.worldmodel)
    {
        return;
    }
    for(int i = 1; i <= cl.worldmodel->numleafs; i++)
    {
        const mleaf_t* leaf = cl.worldmodel->leafs + i;
        if(leaf->contents == CONTENTS_WATER || leaf->contents == CONTENTS_SLIME)
        {
            Con_Printf("liquid point: %.0f %.0f %.0f (contents %d)\n", (leaf->minmaxs[0] + leaf->minmaxs[3]) * 0.5f,
                (leaf->minmaxs[1] + leaf->minmaxs[4]) * 0.5f, (leaf->minmaxs[2] + leaf->minmaxs[5]) * 0.5f, leaf->contents);
            return;
        }
    }
    Con_Printf("liquid point: none\n");
}

} // namespace

void init()
{
    live = new Live{};
    za::fill(live->voiceOf.begin(), live->voiceOf.end(), -1);
    za::fill(live->channelOf.begin(), live->channelOf.end(), -1);
    Cmd_AddCommand("vr_snd_info", info_f);
    Cmd_AddCommand("vr_snd_test", test_f);
    Cmd_AddCommand("vr_snd_capture", capture_f);
    Cmd_AddCommand("vr_snd_bench_spawn", benchSpawn_f);
    Cmd_AddCommand("vr_snd_scene_obj", sceneObj_f);
    Cmd_AddCommand("vr_snd_liquid", liquid_f);
    Cmd_AddCommand("vr_snd_play", play_f);
}

void shutdown()
{
    if(live)
    {
        live->sim.destroy();
        live->mixer.destroy();
        delete live;
        live = nullptr;
    }
    steamaudio::shutdown();
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
        L.sim.update(coordinates(L.listener.pos, L.listener.fwd, L.listener.right, L.listener.up, f.unitsPerMetre),
            L.sources.data(), n, ss, realtime);
        IPLReflectionEffectParams p{};
        if(L.sim.reflections(p))
        {
            L.reverb = p;
            L.haveReverb = true;
        }
    }
    report();
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
        const hands::State& hs = hands::current();
        const glm::vec3 p = hs.muzzleValid[hand] ? hs.muzzle[hand] : hs.pos[hand];
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
    const int count = end - start;
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
        const channel_t* ch = &snd_channels[c];
        VoiceInput in;
        in.pos = glm::vec3{ch->origin[0], ch->origin[1], ch->origin[2]};
        const float falloff =
            za::max(0.f, 1.f - glm::length(in.pos - L.listener.pos) * ch->dist_mult * falloffScale());
        in.gain = static_cast<float>(ch->master_vol) * falloff * volume;
        in.attached = handOf(ch) >= 0;
        if(c < dynamicChannels && L.follow[c].active && vr_snd_follow.value != 0.f)
        {
            in.vel = L.follow[c].vel;
        }
        in.hasDirect = L.sim.direct(v, L.serial[v], in.direct);
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
        L.mixer.set(v, in);
    }
    L.voicesUsed = used;

    const int remain = count - written;
    const int frame = L.frame;
    const int blocks = (remain + frame - 1) / frame;
    const int rendered = blocks * frame;
    za::fill(L.mixL.begin(), L.mixL.begin() + rendered, 0.f);
    za::fill(L.mixR.begin(), L.mixR.begin() + rendered, 0.f);
    L.mixer.render(blocks, L.listener, f, L.haveReverb ? &L.reverb : nullptr, L.mixL.data(), L.mixR.data());

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
        ch->end = renderedEnd + static_cast<int>((length - pos) / za::max(0.5f, L.mixer.dopplerFactor(v)));
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
        finishCapture();
    }
}
