#pragma once

// vr_sndlayer.hpp -- sounds layered from Quake's own as they load (S_LoadSound, no file of the name: VR_SndLayered,
// vr_sndlayer.cpp): each layer one of the game's recordings at its own rate (slower: lower and longer), gain and start,
// cut short with a fade; mixed, optionally low-passed (a thump) or made a room's tail (a reverb, a distant echo), softly
// saturated, made a 16-bit WAV in memory. Nothing of id's is shipped or written; a file of the name wins. (His decision
// 2026-10-29: the new weapon sounds derived at runtime from the game's own, no downloads.) The tables: vr_sndlayer.cpp's
// own (the plasma gun's, the revolver's shots, an armour plate breaking) and the gunfire's (vr_gunsfx.cpp layered).

namespace qvr::sndlayer
{

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

// What is done to the layers' sum before it is saturated (all off by default: the sum as it is).
struct SoundFx
{
    float lowpass{0.f};  // Hz: a two-pole low-pass at it (a thump's body; 0 none)
    float reverb{0.f};   // s: the sum replaced by its reverb (wet only: a room's tail), ringing out over this (T60; 0 none)
    float predelay{0.f}; // s before the reverb starts
    float echo{0.f};     // s: a distant echo of the sum (and its reverb) this late, and a fainter one twice as late (0 none)
    float peak{0.f};     // the result scaled to this peak before the saturation (0: as it comes)
};

struct LayeredSound
{
    const char* name{nullptr};
    SoundLayer layers[4]{};
    float drive{1.f}; // the saturation: denser, louder the more (0.1: clean)
    SoundFx fx{};
};

} // namespace qvr::sndlayer
