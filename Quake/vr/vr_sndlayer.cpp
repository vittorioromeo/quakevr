// vr_sndlayer.cpp -- see vr_sndlayer.hpp. (Moved out of vr_audio.cpp; the thump, the room tails and the reverb, the
// echo for the gunfire's layers: ROUND23.md, "Punchier gunfire".)

#include "vr_sndlayer.hpp"
#include "vr_alloccount.hpp"
#include "vr_engine.hpp"
#include "vr_gunsfx.hpp"
#include "vr_impacts.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"

#include <string.h>

namespace
{

using qvr::sndlayer::LayeredSound;
using qvr::sndlayer::SoundFx;
using qvr::sndlayer::SoundLayer;

constexpr int layeredRate = 22050;

constexpr LayeredSound layeredSounds[] = {
    // AD's Plasma Gun's shot without AD's data (QC AD_FirePlasma; AD's weapons/plasma_fire.wav when it is mounted): the
    // enforcer's laser deeper, the lightning gun's crackle starting, the grenade launcher's thump under them.
    {"vr/derived/plasma_fire.wav",
        {{"enforcer/enfire.wav", 0.72f, 0.9f, 0.f, 0.f, 0.f}, {"weapons/lstart.wav", 1.15f, 0.35f, 0.f, 0.22f, 0.12f},
            {"weapons/grenade.wav", 1.25f, 0.45f, 0.f, 0.3f, 0.15f}},
        1.3f},
    // The revolver's slug (QC W_FireRevolver): the super shotgun's blast lower and heavier, an explosion's first crack
    // for its punch, the action's clack; driven hard (his note 2026-10-29: powerful, intense, not the super shotgun's).
    {"vr/derived/revolver_shot.wav",
        {{"weapons/shotgn2.wav", 0.8f, 0.85f, 0.f, 0.f, 0.f}, {"weapons/r_exp3.wav", 1.35f, 0.45f, 0.f, 0.35f, 0.25f},
            {"weapons/guncock.wav", 0.72f, 0.45f, 0.f, 0.f, 0.f}},
        1.8f},
    // An armed humanoid's plate breaking (QC vr_agarmor.qc VR_AgArmor_Break): the armour pickup's chime backwards and
    // ~6 semitones down, so it is never taken for a pickup; barely driven (clean).
    {"vr/derived/armor_break.wav", {{"items/armor1.wav", 0.7f, 1.f, 0.f, 0.f, 0.f, true}}, 0.1f},
};

[[nodiscard]] int readLe(const byte* p, int bytes)
{
    int v = 0;
    for(int i = bytes - 1; i >= 0; i--)
    {
        v = (v << 8) | p[i];
    }
    return v;
}

// One of the game's sounds (sound/<source>) as floats -1..1 and its rate; false when it isn't there or isn't 8/16-bit
// mono PCM.
bool readPcm(const char* source, za::Vector<float>& samples, int& rate)
{
    samples.clear();
    char path[256];
    q_snprintf(path, sizeof(path), "sound/%s", source);
    byte* data = COM_LoadMallocFile(path, nullptr);
    if(!data)
    {
        return false;
    }
    const int size = static_cast<int>(com_filesize);
    int width = 0, channels = 0;
    rate = 0;
    if(size >= 12 && !memcmp(data, "RIFF", 4) && !memcmp(data + 8, "WAVE", 4))
    {
        int at = 12;
        while(at + 8 <= size)
        {
            const int len = readLe(data + at + 4, 4);
            const byte* body = data + at + 8;
            const int avail = za::min(len, size - at - 8);
            if(!memcmp(data + at, "fmt ", 4) && avail >= 16 && readLe(body, 2) == 1)
            {
                channels = readLe(body + 2, 2);
                rate = readLe(body + 4, 4);
                width = readLe(body + 14, 2) / 8;
            }
            else if(!memcmp(data + at, "data", 4) && channels == 1 && (width == 1 || width == 2) && rate > 0)
            {
                const int n = avail / width;
                samples.reserve(static_cast<za::SizeT>(n));
                for(int i = 0; i < n; i++)
                {
                    samples.pushBack(width == 1 ? (static_cast<float>(body[i]) - 128.f) / 128.f
                                                : static_cast<float>(static_cast<short>(readLe(body + i * 2, 2))) / 32768.f);
                }
                break;
            }
            at += 8 + len + (len & 1);
        }
    }
    VR_HeapFree(data);
    return !samples.empty();
}

// Two one-pole low-passes at `hz` (12 dB an octave): a thump's body, a distant echo's dullness.
void lowpass(za::Vector<float>& x, float hz)
{
    const float a = 1.f - za::exp(-2.f * 3.14159265f * hz / static_cast<float>(layeredRate));
    float s1 = 0.f, s2 = 0.f;
    for(float& v : x)
    {
        s1 += (v - s1) * a;
        s2 += (s1 - s2) * a;
        v = s2;
    }
}

// The sum replaced by its room's tail: four damped combs in parallel (Schroeder; their delays scaled with the decay: a
// small room's reflections close together, a big one's far apart), ringing out over fx.reverb s (T60), then two
// all-passes to smear them; after fx.predelay. With fx.echo, a distant wall's echo of the sum and its tail, dulled, and
// a fainter second one. The last tenth faded out.
void roomTail(za::Vector<float>& x, const SoundFx& fx)
{
    const int inN = static_cast<int>(x.size());
    const int pre = static_cast<int>(fx.predelay * layeredRate);
    const int echoN = static_cast<int>(fx.echo * layeredRate);
    const int total = inN + pre + static_cast<int>(fx.reverb * layeredRate) + 2 * echoN;
    za::Vector<float> wet;
    wet.resize(static_cast<za::SizeT>(total), 0.f);
    const float scale = za::clamp(fx.reverb / 1.2f, 0.4f, 1.6f);
    constexpr float combMs[4] = {29.7f, 37.1f, 41.1f, 43.7f};
    za::Vector<float> buf;
    for(const float ms : combMs)
    {
        const int d = za::max(1, static_cast<int>(ms * scale * 0.001f * layeredRate));
        const float g = za::pow(10.f, -3.f * static_cast<float>(d) / (fx.reverb * layeredRate));
        buf.clear();
        buf.resize(static_cast<za::SizeT>(d), 0.f);
        float damp = 0.f;
        for(int n = 0, at = 0; n < total; n++, at = at + 1 == d ? 0 : at + 1)
        {
            const int i = n - pre;
            const float in = i >= 0 && i < inN ? x[static_cast<za::SizeT>(i)] : 0.f;
            const float out = buf[static_cast<za::SizeT>(at)];
            damp += (out - damp) * 0.6f; // (the highs die first)
            buf[static_cast<za::SizeT>(at)] = in + damp * g;
            wet[static_cast<za::SizeT>(n)] += out * 0.25f;
        }
    }
    constexpr float allpassMs[2] = {5.f, 1.7f};
    for(const float ms : allpassMs)
    {
        const int d = za::max(1, static_cast<int>(ms * 0.001f * layeredRate));
        buf.clear();
        buf.resize(static_cast<za::SizeT>(d), 0.f);
        for(int n = 0, at = 0; n < total; n++, at = at + 1 == d ? 0 : at + 1)
        {
            const float in = wet[static_cast<za::SizeT>(n)];
            const float b = buf[static_cast<za::SizeT>(at)];
            const float y = b - 0.7f * in;
            buf[static_cast<za::SizeT>(at)] = in + 0.7f * y;
            wet[static_cast<za::SizeT>(n)] = y;
        }
    }
    if(echoN > 0)
    {
        za::Vector<float> src;
        src.resize(static_cast<za::SizeT>(total), 0.f);
        for(int n = 0; n < total; n++)
        {
            src[static_cast<za::SizeT>(n)] = (n < inN ? 0.6f * x[static_cast<za::SizeT>(n)] : 0.f) + 0.5f * wet[static_cast<za::SizeT>(n)];
        }
        lowpass(src, 1800.f);
        for(int n = total - 1; n >= echoN; n--)
        {
            const float second = n >= 2 * echoN ? 0.18f * src[static_cast<za::SizeT>(n - 2 * echoN)] : 0.f;
            wet[static_cast<za::SizeT>(n)] += 0.45f * src[static_cast<za::SizeT>(n - echoN)] + second;
        }
    }
    const int fadeN = za::max(1, total / 10);
    for(int n = total - fadeN; n < total; n++)
    {
        wet[static_cast<za::SizeT>(n)] *= static_cast<float>(total - n) / static_cast<float>(fadeN);
    }
    x = static_cast<za::Vector<float>&&>(wet);
}

} // namespace

// S_LoadSound, no file of `name`: one of the layered sounds made now, a WAV in memory (VR_HeapMalloc'd), or NULL.
extern "C" byte* VR_SndLayered(const char* name, int* size)
{
    const LayeredSound* sound = nullptr;
    for(const LayeredSound& s : layeredSounds)
    {
        if(!strcmp(s.name, name))
        {
            sound = &s;
        }
    }
    if(!sound)
    {
        sound = qvr::gunsfx::layered(name); // (the gunfire's layers, room tails and casings)
    }
    if(!sound)
    {
        sound = qvr::impacts::layered(name); // (the surfaces' hit sounds)
    }
    if(!sound)
    {
        return nullptr;
    }
    za::Vector<float> mix;
    za::Vector<float> pcm;
    for(const SoundLayer& layer : sound->layers)
    {
        int rate = 0;
        if(!layer.source || !readPcm(layer.source, pcm, rate))
        {
            if(layer.source)
            {
                Con_DPrintf("%s: no sound/%s to layer\n", name, layer.source);
            }
            continue;
        }
        if(layer.reversed)
        {
            for(za::SizeT a = 0, b = pcm.size() - 1; a < b; a++, b--)
            {
                const float t = pcm[a];
                pcm[a] = pcm[b];
                pcm[b] = t;
            }
        }
        const double step = static_cast<double>(layer.rate) * rate / layeredRate; // its samples an output sample
        int n = static_cast<int>(static_cast<double>(pcm.size() - 1) / step);
        if(layer.length > 0.f)
        {
            n = za::min(n, static_cast<int>(layer.length * layeredRate));
        }
        const int start = static_cast<int>(layer.delay * layeredRate);
        const int fadeN = static_cast<int>(layer.fade * layeredRate);
        if(static_cast<int>(mix.size()) < start + n)
        {
            mix.resize(static_cast<za::SizeT>(start + n), 0.f);
        }
        for(int i = 0; i < n; i++)
        {
            const double pos = i * step;
            const za::SizeT i0 = static_cast<za::SizeT>(pos);
            const float frac = static_cast<float>(pos - static_cast<double>(i0));
            const float s = pcm[i0] + (pcm[za::min(i0 + 1, pcm.size() - 1)] - pcm[i0]) * frac;
            const float env = fadeN > 0 && i > n - fadeN ? static_cast<float>(n - i) / static_cast<float>(fadeN) : 1.f;
            mix[static_cast<za::SizeT>(start + i)] += layer.gain * s * env;
        }
    }
    if(mix.empty())
    {
        return nullptr;
    }
    if(sound->fx.lowpass > 0.f)
    {
        lowpass(mix, sound->fx.lowpass);
    }
    if(sound->fx.reverb > 0.f)
    {
        roomTail(mix, sound->fx);
    }
    if(sound->fx.peak > 0.f)
    {
        float peak = 0.f;
        for(const float v : mix)
        {
            peak = za::max(peak, za::abs(v));
        }
        if(peak > 1e-6f)
        {
            for(float& v : mix)
            {
                v *= sound->fx.peak / peak;
            }
        }
    }
    // Soft saturation (tanh): the layers' sum kept under full scale, louder and denser the more it is driven.
    const auto softClip = [](float x) { return 1.f - 2.f / (za::exp(2.f * x) + 1.f); };
    const float drive = za::max(sound->drive, 0.1f);
    const float norm = 1.f / softClip(drive);
    const int samples = static_cast<int>(mix.size());
    const int dataBytes = samples * 2;
    byte* wav = static_cast<byte*>(VR_HeapMalloc(static_cast<size_t>(44 + dataBytes)));
    const auto put = [&](int at, int value, int bytes) {
        for(int i = 0; i < bytes; i++)
        {
            wav[at + i] = static_cast<byte>((value >> (8 * i)) & 0xff);
        }
    };
    memcpy(wav, "RIFF", 4);
    put(4, 36 + dataBytes, 4);
    memcpy(wav + 8, "WAVEfmt ", 8);
    put(16, 16, 4);
    put(20, 1, 2); // PCM
    put(22, 1, 2); // mono
    put(24, layeredRate, 4);
    put(28, layeredRate * 2, 4);
    put(32, 2, 2);
    put(34, 16, 2);
    memcpy(wav + 36, "data", 4);
    put(40, dataBytes, 4);
    for(int i = 0; i < samples; i++)
    {
        const float v = za::clamp(softClip(drive * mix[static_cast<za::SizeT>(i)]) * norm * 0.95f, -1.f, 1.f);
        put(44 + i * 2, static_cast<int>(v * 32767.f), 2);
    }
    *size = 44 + dataBytes;
    return wav;
}
