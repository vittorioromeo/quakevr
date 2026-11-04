// vr_voicenotes.hpp -- voice notes for playtesting (vr_notes): talk into the headset's microphone
// while playing, and each note is kept with where and what you were doing.
//
// Raise the off hand to your mouth, like a radio, and hold its upper face button (Y on Quest
// controllers) to talk; let go to save. With the hand elsewhere the button does what it is bound to.
// "+vr_note" does the same from a bound key (the flat screen, a keyboard).
//
// Each note is <game dir>/notes/<map>_<date>_<time>.wav (the microphone, 16-bit mono), a .txt
// beside it with the moment's context (map, position, view, health, armour, ammo, what each hand
// holds, the graphics preset, skill), and a screenshot taken as the note starts (Ironwail's own,
// in screenshots/, named by map and time). Misc/quakevr/transcribe_notes.py turns the notes into
// text with Whisper and collects them into notes/NOTES.md.
//
// The microphone is SDL's default capture device, or the first whose name contains vr_note_device
// (e.g. "Virtual Desktop"); "vr_note_devices" lists them. While recording, "REC" and the note's
// length float above the off hand, and the hand buzzes as a note starts and ends.

#pragma once

#include <glm/glm.hpp>

namespace qvr::voicenotes
{

void init(); // commands

// Once a frame (VR_BeginFrame): takes the microphone's samples, draws the indicator.
void frame();

// The off hand's upper face button, as input sees it: true if it starts or ends a note (the
// button's own key then stays untouched).
[[nodiscard]] bool offhandButton(bool pressed);

// The microphone's level for whoever listens (blowing on smoke): the same device as the notes', opened on its own while
// wanted. Call wantMicLevel() each frame it is needed: it opens on the first and closes some seconds after the last; no
// microphone: nothing said, the level stays 0. micLevel(): its RMS (0..1), smoothed over a fraction of a second
// (vr_test_mic_level: a fake one, none opened).
void wantMicLevel();
[[nodiscard]] float micLevel();

// Your mouth and the way you face, in the world: false with no head (not in VR).
[[nodiscard]] bool mouth(glm::vec3& at, glm::vec3& facing);

void shutdown();
// Waits for the files still being saved on the pool (notes' sounds, screenshots) and reports them (VR_Shutdown).
void finishWrites();

} // namespace qvr::voicenotes
