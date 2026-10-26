// vr_voice.hpp -- the player's voice: masculine (Quake's recordings) or feminine (vr_voice 1; Player Appearance >
// Voice). No new recordings: as each of the player's own voice sounds loads (S_LoadSound: VR_SndVoice), Quake's is
// raised vr_voice_pitch semitones by TD-PSOLA (pitch-synchronous grains laid closer together; the duration kept) and
// its formants vr_voice_formant percent by the same grains read faster (their spectral envelope stretched up), so it
// sounds like a woman's voice rather than a sped-up man's. Upsampled to the mix's rate first (windowed sinc), written
// back at it (16-bit). A change of the settings frees those sounds, so they load again as they next play.
// ROUND22.md, "Feminine voice".
//

#pragma once

#include "Zancle/Container/Vector.hpp"

// VR_SndVoice (snd_mem.c calls it) is declared in vr_api.h.

namespace qvr::voice
{

void init(); // the settings' callbacks, the commands (vr_voice_test, vr_voice_export, vr_voice_hear, vr_voice_info)

[[nodiscard]] bool isVoice(const char* name); // one of the player's own voice sounds (voiceSounds), "player/pain1.wav"

// `in` (-1..1 at inRate) pitched up `semitones` with its formants raised `formantPercent`, into `out` (at outRate,
// the same duration). Anything the main thread's: no shared state.
void transform(const za::Vector<float>& in, int inRate, int outRate, float semitones, float formantPercent,
    za::Vector<float>& out);

} // namespace qvr::voice
