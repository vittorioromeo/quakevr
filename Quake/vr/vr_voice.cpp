#include "vr_alloccount.h"
// vr_voice.cpp -- the player's voice, masculine or feminine (vr_voice; see vr_voice.hpp).
//
// The transform (transform), on one sound at a time as it loads:
// 1. Its pitch, frame by frame at the recording's own rate (analysePitch): YIN (the cumulative mean normalised
//    difference function, its first dip under vr_voice_voicing between 70 and 700 Hz), a 5-frame median against octave
//    slips, a voiced run shorter than three frames dropped. Unvoiced: silence, breath, hiss, the burning screams' noise.
// 2. Upsampled to the mix's rate (resample: windowed sinc, Blackman, 12 zero crossings; cut at 92% of the source's
//    Nyquist), so the grains below read between samples cleanly and the raised formants have room above 5.5 kHz.
// 3. Pitch marks on it (placeMarks): in a voiced run one per period, each at the period's largest peak of the run's
//    polarity within 20% of where the last period says; unvoiced, one every unvoicedSeconds.
// 4. TD-PSOLA with formant scaling (synthesise): output grains laid every period / 2^(semitones/12) in voiced runs
//    (every unvoicedSeconds in unvoiced ones, so noise keeps its texture), each the nearest analysis mark's grain
//    (Hann, two periods) read 1 + formant/100 times faster: the grain's spectral envelope (the formants) is
//    stretched up by that, the pitch is the grains' spacing alone, and the time map is 1:1 (the duration kept).
//    Overlap-add normalised by the windows' sum; the peak never above the original's (no clipping).

#include "vr_voice.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"

#include "Zancle/Algorithm/NthElement.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Constants.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Exp2.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Log10.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"

#include <stdio.h>
#include <string.h>

extern "C" qboolean Image_WritePNGPath(const char* pathname, byte* data, int width, int height, int bpp, qboolean upsidedown); // image.c

using namespace qvr;

namespace
{

// The player's own voice: every sound his character makes with his throat. QC: player.qc / ad_player.qc (pain,
// death, drowning, the hard landing's "oof"), client.qc / ad_client.qc (the jump's grunt, gasps out of the water,
// lava and slime pain), vr_burning.qc (burning), vr_melee.qc (the gasps warning of a blow); the engine: vr_climb.cpp's
// mantle and judo-throw grunts (from these, and vr_audio.cpp's two derived from them). Not his body's other sounds
// (player/land.wav's thud, the axe's axhit1/2, gibs, splashes, telefrags, the heartbeat), nor anyone else's.
constexpr const char* voiceSounds[] = {
    "player/pain1.wav", "player/pain2.wav", "player/pain3.wav", "player/pain4.wav", "player/pain5.wav",
    "player/pain6.wav", "player/death1.wav", "player/death2.wav", "player/death3.wav", "player/death4.wav",
    "player/death5.wav", "player/plyrjmp8.wav", "player/land2.wav", "player/gasp1.wav", "player/gasp2.wav",
    "player/drown1.wav", "player/drown2.wav", "player/lburn1.wav", "player/lburn2.wav", "player/slimbrn2.wav",
    "player/h2odeath.wav", "vr/derived/plyrjmp8_low.wav", "vr/derived/land2_low.wav",
};

// The pitch search's range (its threshold, YIN's: vr_voice_voicing).
constexpr float minF0 = 70.f;
constexpr float maxF0 = 700.f;
constexpr float hopSeconds = 0.005f;      // the pitch frames' spacing
constexpr float unvoicedSeconds = 0.005f; // the unvoiced grains' spacing (and half length)
constexpr float silenceGate = 0.03f;      // a frame quieter than this (RMS, of the sound's peak) unvoiced

struct Frame
{
    float period{0.f}; // in the analysed signal's samples
    bool voiced{false};
    bool loud{false}; // above the silence gate
};

struct Mark
{
    double pos{0.0};
    double period{0.0}; // in output samples
    bool voiced{false};
};

// The loads transformed this session (VR_SndVoice; the main thread's): vr_voice_info.
struct LoadStats
{
    int count{0};
    double seconds{0.0};       // their transforms' time
    double audioSeconds{0.0};  // the sounds' length
};
LoadStats loadStats;

// vr_voice_hear's next sound (the main thread's).
int hearNext{0};
constexpr const char* hearSounds[] = {"player/pain2.wav", "player/death1.wav", "player/plyrjmp8.wav",
    "player/pain4.wav", "player/land2.wav", "player/gasp2.wav", "player/death3.wav", "player/pain6.wav"};

[[nodiscard]] float semitonesSetting()
{
    return za::clamp(vr_voice_pitch.value, -12.f, 12.f);
}

[[nodiscard]] float formantSetting()
{
    return za::clamp(vr_voice_formant.value, -30.f, 50.f);
}

// ----------------------------------------------------------------------------
// The transform's steps

[[nodiscard]] za::Vector<Frame> analysePitch(const za::Vector<float>& x, const int rate)
{
    const int n = static_cast<int>(x.size());
    const int maxLag = static_cast<int>(static_cast<float>(rate) / minF0);
    const int minLag = za::max(2, static_cast<int>(static_cast<float>(rate) / maxF0));
    const int window = maxLag;
    const int span = window + maxLag;
    const int hop = za::max(1, static_cast<int>(static_cast<float>(rate) * hopSeconds + 0.5f));
    const float threshold = za::clamp(vr_voice_voicing.value, 0.05f, 0.9f);
    float peak = 0.f;
    for(const float v : x)
    {
        peak = za::max(peak, za::abs(v));
    }
    const float gate = peak * silenceGate;

    za::Vector<Frame> frames(static_cast<za::SizeT>(n / hop + 1));
    za::Vector<float> seg(static_cast<za::SizeT>(span), 0.f);
    za::Vector<float> diff(static_cast<za::SizeT>(maxLag + 1), 1.f);
    for(za::SizeT f = 0; f < frames.size(); f++)
    {
        const int start = static_cast<int>(f) * hop - span / 2;
        float energy = 0.f;
        for(int j = 0; j < span; j++)
        {
            const int k = start + j;
            seg[j] = k >= 0 && k < n ? x[k] : 0.f;
            energy += seg[j] * seg[j];
        }
        if(peak <= 0.f || za::sqrt(energy / static_cast<float>(span)) < gate)
        {
            continue;
        }
        frames[f].loud = true;
        float running = 0.f;
        for(int tau = 1; tau <= maxLag; tau++)
        {
            float d = 0.f;
            for(int j = 0; j < window; j++)
            {
                const float e = seg[j] - seg[j + tau];
                d += e * e;
            }
            running += d;
            diff[tau] = running > 0.f ? d * static_cast<float>(tau) / running : 1.f;
        }
        int best = -1;
        for(int tau = minLag; tau <= maxLag; tau++)
        {
            if(diff[tau] < threshold)
            {
                while(tau + 1 <= maxLag && diff[tau + 1] < diff[tau])
                {
                    tau++;
                }
                best = tau;
                break;
            }
        }
        if(best < 0)
        {
            continue;
        }
        float period = static_cast<float>(best);
        if(best > minLag && best < maxLag)
        {
            const float a = diff[best - 1];
            const float b = diff[best];
            const float c = diff[best + 1];
            const float den = a - 2.f * b + c;
            if(den > 0.f)
            {
                period += 0.5f * (a - c) / den;
            }
        }
        frames[f] = {period, true, true};
    }

    // A 5-frame median of the voiced periods (an octave slip in one frame taken out), then runs under 3 frames dropped.
    za::Vector<Frame> smoothed = frames;
    for(za::SizeT f = 0; f < frames.size(); f++)
    {
        if(!frames[f].voiced)
        {
            continue;
        }
        za::Array<float, 5> near{};
        int count = 0;
        for(za::SizeT g = f >= 2 ? f - 2 : 0; g <= f + 2 && g < frames.size(); g++)
        {
            if(frames[g].voiced)
            {
                near[count++] = frames[g].period;
            }
        }
        for(int i = 1; i < count; i++) // (insertion sort)
        {
            for(int j = i; j > 0 && near[j - 1] > near[j]; j--)
            {
                const float t = near[j];
                near[j] = near[j - 1];
                near[j - 1] = t;
            }
        }
        smoothed[f].period = near[count / 2];
    }
    for(za::SizeT f = 0; f < smoothed.size();)
    {
        if(!smoothed[f].voiced)
        {
            f++;
            continue;
        }
        za::SizeT end = f;
        while(end < smoothed.size() && smoothed[end].voiced)
        {
            end++;
        }
        if(end - f < 3)
        {
            for(za::SizeT g = f; g < end; g++)
            {
                smoothed[g].voiced = false;
            }
        }
        f = end;
    }
    return smoothed;
}

void resample(const za::Vector<float>& x, const double ratio, za::Vector<float>& y)
{
    const int n = static_cast<int>(x.size());
    const int m = static_cast<int>(static_cast<double>(n) * ratio + 0.5);
    y.assign(static_cast<za::SizeT>(m), 0.f);
    if(n == 0)
    {
        return;
    }
    if(ratio == 1.0)
    {
        y = x;
        return;
    }
    constexpr double zeroCrossings = 12.0;
    constexpr int tableSteps = 128; // the kernel's table: entries per source sample
    const double cutoff = za::min(1.0, ratio) * 0.92;
    const double halfWidth = zeroCrossings / cutoff;
    const int tableSize = static_cast<int>(halfWidth * tableSteps) + 2;
    za::Vector<float> table(static_cast<za::SizeT>(tableSize), 0.f);
    const double pi = static_cast<double>(za::pi);
    for(int i = 0; i < tableSize; i++)
    {
        const double d = static_cast<double>(i) / tableSteps;
        const double u = d / halfWidth;
        if(u >= 1.0)
        {
            break;
        }
        const double s = d == 0.0 ? 1.0 : za::sin(pi * cutoff * d) / (pi * cutoff * d);
        const double w = 0.42 + 0.5 * za::cos(pi * u) + 0.08 * za::cos(2.0 * pi * u);
        table[i] = static_cast<float>(cutoff * s * w);
    }
    for(int o = 0; o < m; o++)
    {
        const double t = static_cast<double>(o) / ratio;
        const int k0 = za::max(0, static_cast<int>(za::ceil(t - halfWidth)));
        const int k1 = za::min(n - 1, static_cast<int>(za::floor(t + halfWidth)));
        float acc = 0.f;
        for(int k = k0; k <= k1; k++)
        {
            const double d = za::abs(t - static_cast<double>(k)) * tableSteps;
            const int i = static_cast<int>(d);
            if(i + 1 >= tableSize)
            {
                continue;
            }
            const float fr = static_cast<float>(d - static_cast<double>(i));
            acc += x[k] * (table[i] + (table[i + 1] - table[i]) * fr);
        }
        y[o] = acc;
    }
}

// Catmull-Rom between samples (the signal is oversampled: smooth enough), zero outside.
[[nodiscard]] float sampleAt(const za::Vector<float>& y, const double pos)
{
    const int n = static_cast<int>(y.size());
    const double fl = za::floor(pos);
    const int i = static_cast<int>(fl);
    const float t = static_cast<float>(pos - fl);
    const auto at = [&](int k) { return k >= 0 && k < n ? y[k] : 0.f; };
    const float a = at(i - 1);
    const float b = at(i);
    const float c = at(i + 1);
    const float d = at(i + 2);
    return ((((-0.5f * a + 1.5f * b - 1.5f * c + 0.5f * d) * t) + (a - 2.5f * b + 2.f * c - 0.5f * d)) * t +
               (-0.5f * a + 0.5f * c)) * t + b;
}

[[nodiscard]] za::Vector<Mark> placeMarks(const za::Vector<float>& y, const za::Vector<Frame>& frames, const double ratio,
    const int inRate, const int outRate)
{
    za::Vector<Mark> marks;
    const int n = static_cast<int>(y.size());
    const double hop = za::max(1.0, static_cast<double>(static_cast<int>(static_cast<float>(inRate) * hopSeconds + 0.5f)));
    const double unvoiced = static_cast<double>(outRate) * unvoicedSeconds;
    const auto frameAt = [&](double t) -> Frame {
        if(frames.empty())
        {
            return {};
        }
        const int f = za::clamp(static_cast<int>(t / ratio / hop + 0.5), 0, static_cast<int>(frames.size()) - 1);
        return frames[f];
    };
    double t = 0.0;
    double last = 0.0;
    bool lastVoiced = false;
    float polarity = 1.f;
    while(t < static_cast<double>(n))
    {
        const Frame fr = frameAt(t);
        if(fr.voiced)
        {
            const double period = static_cast<double>(fr.period) * ratio;
            double lo = t;
            double hi = t + period;
            if(lastVoiced)
            {
                lo = last + 0.8 * period;
                hi = last + 1.2 * period;
            }
            else // a run's start: its polarity, the larger of its extremes over two periods
            {
                float top = 0.f;
                float bottom = 0.f;
                for(int i = static_cast<int>(t); i < za::min(n, static_cast<int>(t + 2.0 * period)); i++)
                {
                    top = za::max(top, y[i]);
                    bottom = za::min(bottom, y[i]);
                }
                polarity = top >= -bottom ? 1.f : -1.f;
            }
            const int i0 = za::max(0, static_cast<int>(za::ceil(lo)));
            const int i1 = za::min(n - 1, static_cast<int>(za::floor(hi)));
            if(i0 > i1)
            {
                break;
            }
            int best = i0;
            for(int i = i0; i <= i1; i++)
            {
                if(polarity * y[i] > polarity * y[best])
                {
                    best = i;
                }
            }
            marks.pushBack({static_cast<double>(best), period, true});
            last = static_cast<double>(best);
            lastVoiced = true;
            t = last + period;
        }
        else
        {
            marks.pushBack({t, unvoiced, false});
            last = t;
            lastVoiced = false;
            t += unvoiced;
        }
    }
    return marks;
}

void synthesise(const za::Vector<float>& y, const za::Vector<Mark>& marks, const double pitch, const double formant,
    za::Vector<float>& out)
{
    const int n = static_cast<int>(y.size());
    out.assign(y.size(), 0.f);
    if(marks.empty())
    {
        out = y;
        return;
    }
    za::Vector<float> weight(y.size(), 0.f);
    const double pi = static_cast<double>(za::pi);
    za::SizeT k = 0;
    double ts = 0.0;
    while(ts < static_cast<double>(n))
    {
        while(k + 1 < marks.size() && za::abs(marks[k + 1].pos - ts) <= za::abs(marks[k].pos - ts))
        {
            k++;
        }
        const Mark& m = marks[k];
        // Voiced: the grains closer (the pitch), each two of its periods read `formant` times faster (half of them
        // when the pitch is raised less than the formants: no gap). Unvoiced: as far apart as they were, read faster.
        const double spacing = m.voiced ? m.period / pitch : m.period;
        const double half = m.voiced ? za::max(m.period / formant, spacing) : spacing;
        const int j0 = za::max(0, static_cast<int>(za::ceil(ts - half)));
        const int j1 = za::min(n - 1, static_cast<int>(za::floor(ts + half)));
        for(int j = j0; j <= j1; j++)
        {
            const double d = static_cast<double>(j) - ts;
            const float w = static_cast<float>(0.5 + 0.5 * za::cos(pi * d / half));
            out[j] += w * sampleAt(y, m.pos + d * formant);
            weight[j] += w;
        }
        ts += za::max(1.0, spacing);
    }
    float inPeak = 0.f;
    float outPeak = 0.f;
    for(int j = 0; j < n; j++)
    {
        out[j] /= za::max(weight[j], 0.25f);
        inPeak = za::max(inPeak, za::abs(y[j]));
        outPeak = za::max(outPeak, za::abs(out[j]));
    }
    const float limit = za::min(inPeak, 32767.f / 32768.f);
    if(outPeak > limit && outPeak > 0.f)
    {
        const float scale = limit / outPeak;
        for(float& v : out)
        {
            v *= scale;
        }
    }
}

// A WAV's samples (8-bit unsigned or 16-bit little-endian) as -1..1.
void decode(const byte* pcm, const int width, const int samples, za::Vector<float>& out)
{
    out.resize(static_cast<za::SizeT>(za::max(0, samples)));
    for(int i = 0; i < samples; i++)
    {
        out[i] = width == 2 ? static_cast<float>(static_cast<short>(pcm[2 * i] | (pcm[2 * i + 1] << 8))) / 32768.f
                            : static_cast<float>(static_cast<int>(pcm[i]) - 128) / 128.f;
    }
}

[[nodiscard]] short toShort(const float v)
{
    const float s = v * 32768.f;
    return static_cast<short>(za::clamp(s >= 0.f ? s + 0.5f : s - 0.5f, -32768.f, 32767.f));
}

// The mix's rate (the transformed sounds are written at it), 44.1 kHz without sound.
[[nodiscard]] int mixRate()
{
    return shm && shm->speed > 0 ? shm->speed : 44100;
}

qboolean isVoiceC(const char* name)
{
    return voice::isVoice(name) ? true : false;
}

void onSettingChanged(cvar_t*)
{
    S_FlushSfxNamed(isVoiceC);
}

// ----------------------------------------------------------------------------
// The tests and the debug tools

// A game sound's samples (its file, or the one a derived sound is made from at its rate), -1..1. False: not found.
bool loadGameSound(const char* name, za::Vector<float>& samples, int& rate)
{
    float scale = 1.f;
    const char* file = name;
    byte* data = COM_LoadMallocFile(va("sound/%s", name), nullptr);
    if(!data)
    {
        file = VR_SndDerived(name, &scale);
        data = file ? COM_LoadMallocFile(va("sound/%s", file), nullptr) : nullptr;
    }
    if(!data)
    {
        return false;
    }
    const wavinfo_t info = GetWavinfo(name, data, static_cast<int>(com_filesize));
    const bool ok = info.channels == 1 && (info.width == 1 || info.width == 2) && info.samples > 0;
    if(ok)
    {
        decode(data + info.dataofs, info.width, info.samples, samples);
        rate = static_cast<int>(static_cast<float>(info.rate) * scale + 0.5f);
    }
    VR_HeapFree(data);
    return ok;
}

// The middle 40% of a sound's voiced frames' median pitch (Hz), 0 if none.
[[nodiscard]] float medianPitch(const za::Vector<float>& x, const int rate)
{
    const za::Vector<Frame> frames = analysePitch(x, rate);
    za::Vector<float> periods;
    for(za::SizeT f = frames.size() * 3 / 10; f < frames.size() * 7 / 10; f++)
    {
        if(frames[f].voiced)
        {
            periods.pushBack(frames[f].period);
        }
    }
    if(periods.empty())
    {
        return 0.f;
    }
    float* p = periods.data();
    za::nthElement(p, p + periods.size() / 2, p + periods.size());
    return static_cast<float>(rate) / periods[periods.size() / 2];
}

// The time from the first to the last 5 ms block louder than a tenth of the loudest (seconds).
[[nodiscard]] float activeSeconds(const za::Vector<float>& x, const int rate)
{
    const int block = za::max(1, rate / 200);
    za::Vector<float> rms;
    for(za::SizeT i = 0; i < x.size(); i += static_cast<za::SizeT>(block))
    {
        float e = 0.f;
        int c = 0;
        for(za::SizeT j = i; j < x.size() && j < i + static_cast<za::SizeT>(block); j++, c++)
        {
            e += x[j] * x[j];
        }
        rms.pushBack(za::sqrt(e / static_cast<float>(za::max(1, c))));
    }
    float top = 0.f;
    for(const float r : rms)
    {
        top = za::max(top, r);
    }
    int first = -1;
    int lastBlock = -1;
    for(int i = 0; i < static_cast<int>(rms.size()); i++)
    {
        if(rms[i] > 0.1f * top)
        {
            first = first < 0 ? i : first;
            lastBlock = i;
        }
    }
    return first < 0 ? 0.f : static_cast<float>((lastBlock - first + 1) * block) / static_cast<float>(rate);
}

// In-place radix-2 FFT (size a power of two).
void fft(za::Vector<float>& re, za::Vector<float>& im)
{
    const za::SizeT n = re.size();
    for(za::SizeT i = 1, j = 0; i < n; i++)
    {
        za::SizeT bit = n >> 1;
        for(; j & bit; bit >>= 1)
        {
            j ^= bit;
        }
        j ^= bit;
        if(i < j)
        {
            const float tr = re[i];
            re[i] = re[j];
            re[j] = tr;
            const float ti = im[i];
            im[i] = im[j];
            im[j] = ti;
        }
    }
    for(za::SizeT len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * static_cast<double>(za::pi) / static_cast<double>(len);
        for(za::SizeT i = 0; i < n; i += len)
        {
            for(za::SizeT j = 0; j < len / 2; j++)
            {
                const float wr = static_cast<float>(za::cos(ang * static_cast<double>(j)));
                const float wi = static_cast<float>(za::sin(ang * static_cast<double>(j)));
                const za::SizeT a = i + j;
                const za::SizeT b = i + j + len / 2;
                const float xr = re[b] * wr - im[b] * wi;
                const float xi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - xr;
                im[b] = im[a] - xi;
                re[a] += xr;
                im[a] += xi;
            }
        }
    }
}

// The average magnitude spectrum's centroid between 150 Hz and 5 kHz over the middle 40% (Hz): where the formants'
// weight is.
[[nodiscard]] float spectralCentroid(const za::Vector<float>& x, const int rate)
{
    constexpr za::SizeT size = 4096;
    za::Vector<float> mag(size / 2, 0.f);
    za::Vector<float> re(size);
    za::Vector<float> im(size);
    const double pi = static_cast<double>(za::pi);
    for(za::SizeT start = x.size() * 3 / 10; start + size <= x.size() * 7 / 10 + size / 2 && start + size <= x.size();
        start += size / 4)
    {
        for(za::SizeT i = 0; i < size; i++)
        {
            re[i] = x[start + i] * static_cast<float>(0.5 - 0.5 * za::cos(2.0 * pi * static_cast<double>(i) / size));
            im[i] = 0.f;
        }
        fft(re, im);
        for(za::SizeT i = 0; i < size / 2; i++)
        {
            mag[i] += za::sqrt(re[i] * re[i] + im[i] * im[i]);
        }
    }
    double sum = 0.0;
    double weighted = 0.0;
    for(za::SizeT i = 0; i < size / 2; i++)
    {
        const double hz = static_cast<double>(i) * rate / static_cast<double>(size);
        if(hz >= 150.0 && hz <= 5000.0)
        {
            sum += mag[i];
            weighted += mag[i] * hz;
        }
    }
    return sum > 0.0 ? static_cast<float>(weighted / sum) : 0.f;
}

// A male /a/: a 120 Hz pulse train (a one-pole glottal tilt, steady: no vibrato or jitter) through three formant
// resonators (730, 1090, 2440 Hz), 1 s with 30 ms fades, 8-bit as Quake's recordings are.
void synthVowel(const int rate, const float f0, za::Vector<byte>& pcm)
{
    const int n = rate;
    za::Vector<float> x(static_cast<za::SizeT>(n), 0.f);
    double phase = 0.0;
    float tilt = 0.f;
    for(int i = 0; i < n; i++)
    {
        phase += static_cast<double>(f0) / rate;
        float pulse = 0.f;
        if(phase >= 1.0)
        {
            phase -= 1.0;
            pulse = 1.f;
        }
        tilt = 0.9f * tilt + pulse;
        x[i] = tilt;
    }
    constexpr float formants[3][2] = {{730.f, 90.f}, {1090.f, 110.f}, {2440.f, 170.f}};
    const double pi = static_cast<double>(za::pi);
    for(const auto& fm : formants)
    {
        const double r = za::exp(-pi * fm[1] / rate);
        const double c = 2.0 * r * za::cos(2.0 * pi * fm[0] / rate);
        double y1 = 0.0;
        double y2 = 0.0;
        for(int i = 0; i < n; i++)
        {
            const double v = (1.0 - r) * x[i] + c * y1 - r * r * y2;
            y2 = y1;
            y1 = v;
            x[i] = static_cast<float>(v);
        }
    }
    float peak = 0.f;
    float mean = 0.f;
    for(const float v : x)
    {
        mean += v;
    }
    mean /= static_cast<float>(n);
    for(float& v : x)
    {
        v -= mean;
        peak = za::max(peak, za::abs(v));
    }
    const int fade = rate * 3 / 100;
    pcm.resize(static_cast<za::SizeT>(n));
    for(int i = 0; i < n; i++)
    {
        const int edge = za::min(i, n - 1 - i);
        const float env = edge < fade ? 0.5f - 0.5f * static_cast<float>(za::cos(pi * edge / fade)) : 1.f;
        const float v = x[i] / peak * 0.9f * env;
        pcm[i] = static_cast<byte>(za::clamp(static_cast<int>(v * 128.f + (v >= 0.f ? 0.5f : -0.5f)) + 128, 0, 255));
    }
}

// The transform as the load runs it: 16-bit at outRate, *clipped the samples past full scale before the clamp.
void transformPcm(const byte* pcm, const int width, const int samples, const int rate, const int outRate,
    za::Vector<float>& out, int* clipped)
{
    za::Vector<float> in;
    decode(pcm, width, samples, in);
    voice::transform(in, rate, outRate, semitonesSetting(), formantSetting(), out);
    if(clipped)
    {
        *clipped = 0;
        for(const float v : out)
        {
            *clipped += v * 32768.f > 32767.5f || v * 32768.f < -32768.5f ? 1 : 0;
        }
    }
}

void test_f()
{
    int passed = 0;
    int failed = 0;
    const auto check = [&](bool ok, const char* what) {
        (ok ? passed : failed)++;
        Con_Printf("vr_voice_test: %s %s\n", ok ? "PASS" : "FAIL", what);
    };
    const int outRate = mixRate();
    const float semitones = semitonesSetting();
    const float formant = formantSetting();
    Con_Printf("vr_voice_test: pitch +%.2f semitones, formants +%.0f%%, output %d Hz\n", semitones, formant, outRate);

    // The synthetic vowel.
    za::Vector<byte> pcm;
    constexpr int vowelRate = 11025;
    constexpr float vowelF0 = 120.f;
    synthVowel(vowelRate, vowelF0, pcm);
    za::Vector<float> in;
    decode(pcm.data(), 1, static_cast<int>(pcm.size()), in);
    za::Vector<float> out;
    int clipped = 0;
    const double t0 = Sys_DoubleTime();
    transformPcm(pcm.data(), 1, static_cast<int>(pcm.size()), vowelRate, outRate, out, &clipped);
    const double ms = (Sys_DoubleTime() - t0) * 1000.0;
    const float f0In = medianPitch(in, vowelRate);
    const float f0Out = medianPitch(out, outRate);
    const float expected = f0In * za::exp2(semitones / 12.f);
    const float pitchError = expected > 0.f ? za::abs(f0Out / expected - 1.f) : 1.f;
    const float durIn = activeSeconds(in, vowelRate);
    const float durOut = activeSeconds(out, outRate);
    const float durError = durIn > 0.f ? za::abs(durOut / durIn - 1.f) : 1.f;
    float peak = 0.f;
    for(const float v : out)
    {
        peak = za::max(peak, za::abs(v));
    }
    za::Vector<float> inUp;
    resample(in, static_cast<double>(outRate) / vowelRate, inUp);
    const float centroidIn = spectralCentroid(inUp, outRate);
    const float centroidOut = spectralCentroid(out, outRate);
    const float centroidRatio = centroidIn > 0.f ? centroidOut / centroidIn : 0.f;
    Con_Printf("vr_voice_test: vowel f0 %.1f -> %.1f Hz (expected %.1f, error %.2f%%), duration %.3f -> %.3f s (error "
               "%.2f%%), peak %.3f, %d clipped, centroid %.0f -> %.0f Hz (x%.3f), %.1f ms\n",
        f0In, f0Out, expected, pitchError * 100.f, durIn, durOut, durError * 100.f, peak, clipped, centroidIn,
        centroidOut, centroidRatio, ms);
    check(za::abs(f0In / vowelF0 - 1.f) < 0.02f, "the vowel's own pitch found (120 Hz within 2%)");
    check(pitchError < 0.02f, "the fundamental moved by the semitones asked (within 2%)");
    check(durError < 0.02f, "the duration kept (within 2%)");
    check(clipped == 0 && peak < 1.f, "no clipping");
    check(formant <= 1.f || (centroidRatio > 1.f + formant * 0.005f && centroidRatio < 1.f + formant * 0.015f),
        "the formants raised (the spectral centroid by half to one and a half times the setting)");

    // Each of the player's voice sounds.
    double totalMs = 0.0;
    double totalAudio = 0.0;
    for(const char* name : voiceSounds)
    {
        za::Vector<float> sound;
        int rate = 0;
        if(!loadGameSound(name, sound, rate))
        {
            check(false, va("%s found", name));
            continue;
        }
        za::Vector<byte> bytes(sound.size() * 2);
        for(za::SizeT i = 0; i < sound.size(); i++)
        {
            const short s = toShort(sound[i]);
            bytes[2 * i] = static_cast<byte>(s & 0xff);
            bytes[2 * i + 1] = static_cast<byte>((s >> 8) & 0xff);
        }
        const double s0 = Sys_DoubleTime();
        transformPcm(bytes.data(), 2, static_cast<int>(sound.size()), rate, outRate, out, &clipped);
        const double soundMs = (Sys_DoubleTime() - s0) * 1000.0;
        totalMs += soundMs;
        const double seconds = static_cast<double>(sound.size()) / rate;
        totalAudio += seconds;
        bool finite = true;
        for(const float v : out)
        {
            finite = finite && v == v && v < 2.f && v > -2.f;
        }
        const double outSeconds = static_cast<double>(out.size()) / outRate;
        const bool ok = finite && clipped == 0 && !out.empty() && za::abs(outSeconds / seconds - 1.0) < 0.02;
        // (its pitch before and after, for the record: the output brought back to the sound's rate to measure)
        za::Vector<float> down;
        resample(out, static_cast<double>(rate) / outRate, down);
        const float f0Before = medianPitch(sound, rate);
        const float f0After = medianPitch(down, rate);
        int loud = 0;
        int voiced = 0;
        for(const Frame& fr : analysePitch(sound, rate))
        {
            loud += fr.loud ? 1 : 0;
            voiced += fr.voiced ? 1 : 0;
        }
        check(ok, va("%s (%d Hz, %.2f s -> %.2f s, %.0f%% of it voiced, pitch %.0f -> %.0f Hz, %.1f ms)", name, rate,
                      seconds, outSeconds, 100.f * static_cast<float>(voiced) / static_cast<float>(za::max(1, loud)),
                      f0Before, f0After, soundMs));
    }
    Con_Printf("vr_voice_test: the %d voice sounds: %.2f s of sound transformed in %.1f ms\n",
        static_cast<int>(sizeof voiceSounds / sizeof voiceSounds[0]), totalAudio, totalMs);
    Con_Printf("vr_voice_test: %d passed, %d failed\n", passed, failed);
}

void writeWav(const char* path, const za::Vector<float>& x, const int rate)
{
    const za::SizeT bytes = x.size() * 2;
    za::Vector<byte> file(44 + bytes);
    const auto u32 = [&](za::SizeT at, unsigned v) {
        for(int i = 0; i < 4; i++)
        {
            file[at + i] = static_cast<byte>((v >> (8 * i)) & 0xff);
        }
    };
    const auto u16 = [&](za::SizeT at, unsigned v) {
        file[at] = static_cast<byte>(v & 0xff);
        file[at + 1] = static_cast<byte>((v >> 8) & 0xff);
    };
    memcpy(file.data(), "RIFF", 4);
    u32(4, static_cast<unsigned>(36 + bytes));
    memcpy(file.data() + 8, "WAVEfmt ", 8);
    u32(16, 16);
    u16(20, 1);
    u16(22, 1);
    u32(24, static_cast<unsigned>(rate));
    u32(28, static_cast<unsigned>(rate * 2));
    u16(32, 2);
    u16(34, 16);
    memcpy(file.data() + 36, "data", 4);
    u32(40, static_cast<unsigned>(bytes));
    for(za::SizeT i = 0; i < x.size(); i++)
    {
        u16(44 + 2 * i, static_cast<unsigned short>(toShort(x[i])));
    }
    if(!files::writeBytes(path, file.data(), file.size()))
    {
        Con_Printf("vr_voice_export: couldn't write %s\n", path);
    }
}

// Spectrograms of `before` over `after` (both at `rate`), 0..8 kHz, -80..0 dB of the louder's peak, a tick every kHz.
void writeSpectrogram(const char* path, const za::Vector<float>& before, const za::Vector<float>& after, const int rate)
{
    constexpr za::SizeT size = 1024;
    constexpr int gap = 4;
    const int bins = za::min(static_cast<int>(size / 2), static_cast<int>(8000.0 * size / rate));
    const int rowsEach = bins * 2;
    const za::SizeT longest = za::max(before.size(), after.size());
    const za::SizeT hop = za::clamp(longest / 600, static_cast<za::SizeT>(16), static_cast<za::SizeT>(128)); // (~600 wide)
    const int width = static_cast<int>(longest / hop) + 1;
    const int height = rowsEach * 2 + gap;
    za::Vector<float> db(static_cast<za::SizeT>(width * bins * 2), -200.f);
    za::Vector<float> re(size);
    za::Vector<float> im(size);
    const double pi = static_cast<double>(za::pi);
    float top = -200.f;
    for(int which = 0; which < 2; which++)
    {
        const za::Vector<float>& x = which == 0 ? before : after;
        for(int c = 0; c < width; c++)
        {
            for(za::SizeT i = 0; i < size; i++)
            {
                const za::SizeT k = static_cast<za::SizeT>(c) * hop + i;
                const float w = static_cast<float>(0.5 - 0.5 * za::cos(2.0 * pi * static_cast<double>(i) / size));
                re[i] = k < x.size() ? x[k] * w : 0.f;
                im[i] = 0.f;
            }
            fft(re, im);
            for(int b = 0; b < bins; b++)
            {
                const float m = re[b] * re[b] + im[b] * im[b];
                const float v = 10.f * za::log10(za::max(m, 1e-12f));
                db[static_cast<za::SizeT>((which * bins + b) * width + c)] = v;
                top = za::max(top, v);
            }
        }
    }
    za::Vector<byte> rgb(static_cast<za::SizeT>(width * height * 3), 0);
    for(int row = 0; row < height; row++)
    {
        for(int c = 0; c < width; c++)
        {
            byte* p = rgb.data() + (row * width + c) * 3;
            if(row >= rowsEach && row < rowsEach + gap)
            {
                p[0] = p[1] = p[2] = 255;
                continue;
            }
            const int which = row < rowsEach ? 0 : 1;
            const int local = row - (which == 0 ? 0 : rowsEach + gap);
            const int b = bins - 1 - local / 2; // (low frequencies at the bottom)
            const float v = za::clamp((db[static_cast<za::SizeT>((which * bins + b) * width + c)] - top + 80.f) / 80.f, 0.f, 1.f);
            p[0] = static_cast<byte>(za::clamp(3.f * v, 0.f, 1.f) * 255.f);
            p[1] = static_cast<byte>(za::clamp(3.f * v - 1.f, 0.f, 1.f) * 255.f);
            p[2] = static_cast<byte>(za::clamp(3.f * v - 2.f, 0.f, 1.f) * 255.f);
            const double hz = static_cast<double>(b) * rate / size;
            const double nextHz = static_cast<double>(b + 1) * rate / size;
            if(c < 8 && static_cast<int>(hz / 1000.0) != static_cast<int>(nextHz / 1000.0))
            {
                p[0] = p[1] = p[2] = 200;
            }
        }
    }
    if(!Image_WritePNGPath(path, rgb.data(), width, height, 24, true))
    {
        Con_Printf("vr_voice_export: couldn't write %s\n", path);
    }
}

// vr_voice_export <directory>: five of the voice sounds before (Quake's, at its rate) and after (the feminine voice
// at the settings, at the mix's rate) as WAV files, and two spectrogram pairs (before above, after below).
void export_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_voice_export <directory>: before/after WAVs of five voice sounds, two spectrograms\n");
        return;
    }
    constexpr const char* exported[] = {
        "player/pain2.wav", "player/death1.wav", "player/plyrjmp8.wav", "player/land2.wav", "player/gasp2.wav"};
    const char* arg = Cmd_Argv(1);
    const bool absolute = arg[0] == '/' || arg[0] == '\\' || (arg[0] != 0 && arg[1] == ':');
    const za::String dir{absolute ? arg : va("%s/%s", com_gamedir, arg)}; // (a relative one in the game's folder)
    files::createDirectories(dir.cStr());
    const int outRate = mixRate();
    int written = 0;
    for(za::SizeT e = 0; e < sizeof exported / sizeof exported[0]; e++)
    {
        za::Vector<float> sound;
        int rate = 0;
        if(!loadGameSound(exported[e], sound, rate))
        {
            Con_Printf("vr_voice_export: %s not found\n", exported[e]);
            continue;
        }
        za::Vector<float> out;
        voice::transform(sound, rate, outRate, semitonesSetting(), formantSetting(), out);
        const char* base = strrchr(exported[e], '/') + 1;
        char stem[64];
        q_strlcpy(stem, base, sizeof stem);
        if(char* dot = strrchr(stem, '.'))
        {
            *dot = 0;
        }
        writeWav(va("%s/%s_before.wav", dir.cStr(), stem), sound, rate);
        writeWav(va("%s/%s_after.wav", dir.cStr(), stem), out, outRate);
        if(e < 2)
        {
            za::Vector<float> up;
            resample(sound, static_cast<double>(outRate) / rate, up);
            writeSpectrogram(va("%s/%s_spectrogram.png", dir.cStr(), stem), up, out, outRate);
        }
        written++;
    }
    Con_Printf("vr_voice_export: %d sounds written to %s (pitch +%.2f semitones, formants +%.0f%%)\n", written,
        dir.cStr(), semitonesSetting(), formantSetting());
}

// vr_voice_hear [sound]: one of the player's voice sounds played to you (the next of a few each time), as set now.
void hear_f()
{
    const char* name = Cmd_Argc() >= 2 ? Cmd_Argv(1) : hearSounds[hearNext];
    if(Cmd_Argc() < 2)
    {
        hearNext = (hearNext + 1) % static_cast<int>(sizeof hearSounds / sizeof hearSounds[0]);
    }
    S_LocalSound(name);
}

void info_f()
{
    Con_Printf("vr_voice %s (pitch +%.2f semitones, formants +%.0f%%): %d sounds transformed as they loaded, %.2f s of "
               "sound in %.1f ms\n",
        vr_voice.value >= 0.5f ? "feminine" : "masculine", semitonesSetting(), formantSetting(), loadStats.count,
        loadStats.audioSeconds, loadStats.seconds * 1000.0);
}

} // namespace

namespace qvr::voice
{

bool isVoice(const char* name)
{
    for(const char* v : voiceSounds)
    {
        if(q_strcasecmp(name, v) == 0)
        {
            return true;
        }
    }
    return false;
}

void transform(const za::Vector<float>& in, const int inRate, const int outRate, const float semitones,
    const float formantPercent, za::Vector<float>& out)
{
    const double ratio = static_cast<double>(outRate) / static_cast<double>(inRate);
    za::Vector<float> up;
    resample(in, ratio, up);
    const za::Vector<Frame> frames = analysePitch(in, inRate);
    const za::Vector<Mark> marks = placeMarks(up, frames, ratio, inRate, outRate);
    synthesise(up, marks, static_cast<double>(za::exp2(semitones / 12.f)), 1.0 + formantPercent / 100.0, out);
}

void init()
{
    Cvar_SetCallback(&vr_voice, onSettingChanged);
    Cvar_SetCallback(&vr_voice_pitch, onSettingChanged);
    Cvar_SetCallback(&vr_voice_formant, onSettingChanged);
    Cvar_SetCallback(&vr_voice_voicing, onSettingChanged);
    Cmd_AddCommand("vr_voice_test", test_f);
    Cmd_AddCommand("vr_voice_export", export_f);
    Cmd_AddCommand("vr_voice_hear", hear_f);
    Cmd_AddCommand("vr_voice_info", info_f);
}

} // namespace qvr::voice

extern "C" short* VR_SndVoice(const char* name, const byte* pcm, int width, int samples, int rate, int loopstart,
    int outRate, int* outSamples, int* outLoop)
{
    if(vr_voice.value < 0.5f || !qvr::voice::isVoice(name) || samples <= 0 || rate <= 0 || outRate <= 0)
    {
        return nullptr;
    }
    const double t0 = Sys_DoubleTime();
    za::Vector<float> in;
    decode(pcm, width, samples, in);
    za::Vector<float> out;
    qvr::voice::transform(in, rate, outRate, semitonesSetting(), formantSetting(), out);
    if(out.empty())
    {
        return nullptr;
    }
    short* data = static_cast<short*>(VR_HeapMalloc(out.size() * sizeof(short)));
    for(za::SizeT i = 0; i < out.size(); i++)
    {
        data[i] = toShort(out[i]);
    }
    *outSamples = static_cast<int>(out.size());
    *outLoop = loopstart < 0 ? -1 : static_cast<int>(static_cast<double>(loopstart) * outRate / rate);
    loadStats.count++;
    loadStats.seconds += Sys_DoubleTime() - t0;
    loadStats.audioSeconds += static_cast<double>(samples) / rate;
    return data;
}
