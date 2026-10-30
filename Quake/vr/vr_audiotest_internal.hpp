#pragma once

// vr_audiotest_internal.hpp -- what vr_audiotest.cpp (vr_snd_test: its measurements and the command) shares with
// its tests: vr_audiotest_spatial.cpp (circle, ild, clicks), vr_audiotest_room.cpp (occlusion, reverb, doppler,
// near field, distance, hands) and vr_audiotest_mix.cpp (bench, full band, golden). Internal: included by those
// alone. What was the file's anonymous namespace and more than one part uses is namespace qvr::audio::testkit.

#include "vr_audio.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_units.hpp"
#include "Zancle/Algorithm/Fill.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/Log10.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"
#include <stdio.h>

namespace qvr::audio::testkit
{

constexpr int testRate = 44100;

// A made mono sound, as Quake's cache holds one (16-bit).
struct Sound
{
    za::Vector<unsigned char> bytes;
    [[nodiscard]] const sfxcache_t* cache() const
    {
        return reinterpret_cast<const sfxcache_t*>(bytes.data());
    }
};

struct Render
{
    za::Vector<float> l, r;
};

// Octave levels (dB, both ears' power) of 1-2, 2-4, 4-8 and 8-16 kHz over 4096 samples from `from`: Goertzel at six
// frequencies an octave, Hann-windowed, averaged over 4 windows.
struct Octaves
{
    float band[4]{};
};

struct Result
{
    int passed{0};
    int failed{0};
    void check(const char* test, bool ok)
    {
        Con_Printf("snd_test %s: %s\n", test, ok ? "PASS" : "FAIL");
        (ok ? passed : failed)++;
    }
};

// The measurements and made sounds (vr_audiotest.cpp), the shared pieces of the tests and the tests (their files).
float db(double meanSquare);
Sound makeSound(int length, bool loop, float (*gen)(int i, unsigned& seed));
float noise(int, unsigned& seed);
float sine1k(int i, unsigned&);
float click(int i, unsigned&);
Render renderVoice(Mixer& m, const sfxcache_t* sc, const Listener& lis, const Features& f,
    const IPLReflectionEffectParams* reverb, float seconds, jobs::FunctionRef<void(double, VoiceInput&)> move);
void writeWav(const char* name, const Render& r);
int itd(const Render& r, int from, int n);
Octaves octaves(const Render& r, int from);
double pitch(const za::Vector<float>& x, int from, int n);
float rt60(const Render& r, int from);
Listener centred();
Features baseFeatures();
int testFrame();
float earBand(const za::Vector<float>& x, int from, float f0, float f1);
void addWall(Mesh& mesh, float x, float upm);
void testCircle(Result& res);
void testIld(Result& res);
void testClicks(Result& res);
void testOcclusion(Result& res);
void testReverb(Result& res);
void testReverbSide(Result& res);
void testDoppler(Result& res);
void testNearField(Result& res);
void testDistance(Result& res);
void testHands(Result& res);
void bench(Result& res);
void testFullBand(Result& res);
void testGolden(Result& res);

} // namespace qvr::audio::testkit
