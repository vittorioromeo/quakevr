// vr_audio_io.cpp -- part of vr_audio.cpp (see vr_audio.hpp, vr_audio_internal.hpp): the sounds made from others
// (derived and layered sounds, their PCM read), the band-limit tables and the full-band resampler.

#include "vr_audio_internal.hpp"

using namespace qvr;
using namespace qvr::audio;
using namespace qvr::audio::detail;

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

namespace
{

// Sounds layered from Quake's own as they load (S_LoadSound, no file of the name: VR_SndLayered): each layer one of id's
// recordings at its own rate (slower: lower and longer), gain and start, cut short with a fade; mixed, softly saturated
// (`drive`: denser, louder), made a 16-bit WAV in memory. Nothing of id's is shipped or written; a file of the name wins.
// (His decision 2026-10-29: the new weapon sounds derived at runtime from the game's own, no downloads.)
struct SoundLayer
{
    const char* source{nullptr};
    float rate{1.f};   // its speed (0.8: ~4 semitones down)
    float gain{1.f};
    float delay{0.f};  // s into the sound it starts
    float length{0.f}; // s of it kept (0: all of it), its last `fade` s faded out
    float fade{0.f};
    bool reversed{false}; // played backwards
};

struct LayeredSound
{
    const char* name;
    SoundLayer layers[4];
    float drive;
};

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

} // namespace

// S_LoadSound, no file of `name`: one of layeredSounds made now, a WAV in memory (VR_HeapMalloc'd), or NULL.
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
        const double step =static_cast<double>(layer.rate) * rate / layeredRate; // its samples an output sample
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
