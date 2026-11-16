// vr_audiotest_out.cpp -- vr_snd_outtest: the sound's output stage, as the mix writes the device's buffer (snd_mix.c's
// S_TransferPaintBuffer, through S_TransferTest). A 1 kHz sine at -6 dBFS, at 44.1 and 48 kHz (Virtual Desktop's device
// rate), in 16-bit and in 8-bit unsigned (the format every start opened from 2026-11-13, when nohunk left the heap's
// size 0 and snd_dma.c took that for a small heap: ROUND25.md, "Quest Pro: hissy sound"); the added noise and
// distortion (THD+N: the output against the exact sine) and the largest single-sample error. The live device's
// format checked too (16-bit unless loadas8bit). Needs no Steam Audio; ends with "vr_snd_outtest: N passed, M failed".

#include "vr_audio.hpp"
#include "vr_testkit.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Log10.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

namespace qvr::audio
{

namespace
{

constexpr int outFrames = 2048;  // snd_mix.c's PAINTBUFFER_SIZE
constexpr int outSamples = 4096; // the test buffer's samples (both channels): a power of two
constexpr float outAmp = 16383.f; // -6 dBFS

struct OutResult
{
    float thdn{0.f};    // dB below the sine
    float worst{0.f};   // the largest error of a sample, in 16-bit steps
};

// One format's run: the sine through the output stage, read back as 16-bit values.
OutResult runFormat(int rate, int bits)
{
    za::Vector<portable_samplepair_t> paint;
    za::Vector<float> ideal;
    paint.resize(outFrames);
    ideal.resize(outFrames);
    for(int i = 0; i < outFrames; ++i)
    {
        const float s = outAmp * za::sin(2.f * 3.14159265f * 1000.f * static_cast<float>(i) / static_cast<float>(rate));
        ideal[i] = s;
        const int p = static_cast<int>(s * 256.f + (s >= 0.f ? 0.5f : -0.5f));
        paint[i].left = p;
        paint[i].right = -p; // the other channel inverted: a channel mix-up shows
    }
    za::Vector<unsigned char> buffer;
    buffer.resize(static_cast<za::SizeT>(outSamples * (bits / 8)));
    dma_t fmt{};
    fmt.channels = 2;
    fmt.samples = outSamples;
    fmt.samplebits = bits;
    fmt.speed = rate;
    fmt.buffer = buffer.data();
    S_TransferTest(&fmt, paint.data(), outFrames);

    double signal = 0.0;
    double error = 0.0;
    float worst = 0.f;
    for(int i = 0; i < outFrames; ++i)
    {
        for(int c = 0; c < 2; ++c)
        {
            const int k = i * 2 + c;
            const float out = bits == 16 ? static_cast<float>(reinterpret_cast<const short*>(buffer.data())[k])
                                         : static_cast<float>((static_cast<int>(buffer[k]) - 128) * 256);
            const float want = c == 0 ? ideal[i] : -ideal[i];
            const float d = out - want;
            signal += static_cast<double>(want) * want;
            error += static_cast<double>(d) * d;
            worst = za::max(worst, za::abs(d));
        }
    }
    OutResult r;
    r.thdn = error > 0.0 ? static_cast<float>(10.0 * za::log10(error / signal)) : -200.f;
    r.worst = worst;
    return r;
}

} // namespace

void outTest_f()
{
    testkit::Tally tally;
    auto check = [&](const char* what, bool ok) {
        Con_Printf("vr_snd_outtest %s: %s\n", what, ok ? "PASS" : "FAIL");
        tally.check(ok);
    };

    // The live device: 16-bit unless asked for 8 (loadas8bit), and the sounds not loaded as 8-bit behind one's back.
    int devRate = 0;
    int devChannels = 0;
    const char* dev = SNDDMA_DefaultDevice(&devRate, &devChannels);
    if(shm)
    {
        Con_Printf("vr_snd_outtest: live output %d-bit %s %d Hz; the default device %s: %d Hz, %d channels; "
                   "loadas8bit %g\n",
            shm->samplebits, shm->channels == 2 ? "stereo" : "mono", shm->speed, dev, devRate, devChannels,
            loadas8bit.value);
        check("live 16-bit", shm->samplebits == 16 || loadas8bit.value != 0.f);
    }
    else
    {
        Con_Printf("vr_snd_outtest: no live device (nosound): its format not checked\n");
    }
    check("sounds not forced to 8-bit", loadas8bit.value == 0.f);

    // The output stage: 16-bit adds only its truncation to 16 bits (-86 dB for this sine, under a step), at either
    // rate; 8-bit unsigned (the regression's format) adds Quake's truncation to 8 bits (-37 dB), and the measure has
    // to see it.
    const int rates[2] = {44100, 48000};
    for(const int rate : rates)
    {
        const OutResult r16 = runFormat(rate, 16);
        const OutResult r8 = runFormat(rate, 8);
        Con_Printf("vr_snd_outtest: %d Hz: 16-bit THD+N %.1f dB (worst sample %.2f steps); 8-bit THD+N %.1f dB "
                   "(worst %.0f steps)\n",
            rate, r16.thdn, r16.worst, r8.thdn, r8.worst);
        char what[64];
        q_snprintf(what, sizeof(what), "16-bit %d Hz clean", rate);
        check(what, r16.thdn < -80.f && r16.worst <= 1.f);
        q_snprintf(what, sizeof(what), "8-bit %d Hz hiss seen", rate);
        check(what, r8.thdn > -60.f);
    }
    tally.verdict("vr_snd_outtest");
}

} // namespace qvr::audio
