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

namespace qvr::voicenotes
{

void init(); // commands

// Once a frame (VR_BeginFrame): takes the microphone's samples, draws the indicator.
void frame();

// The off hand's upper face button, as input sees it: true if it starts or ends a note (the
// button's own key then stays untouched).
[[nodiscard]] bool offhandButton(bool pressed);

void shutdown();

} // namespace qvr::voicenotes
