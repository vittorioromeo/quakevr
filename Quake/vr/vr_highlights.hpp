// vr_highlights.hpp -- highlight markers for trailers (vr_highlights; docs/vr-port/TRAILER.md): while on, the game's
// cool moments (kills and gibs, multi-kills, parries and counters, bullet time, explosions and their chains, grenades
// set off by a shot, axes stuck in monsters, grappling swings, ...) are logged as they happen, each with its kind, a
// score, what it was done to and with, and its time in real seconds from the log's sync mark (right in slow motion:
// realtime, never the slowed game's clock). Starting a log makes the sync mark: a white flash over the desktop window
// (the mirror or the spectator camera a recording captures; never the headset) and a beep, to line the log up with
// the video.
//
// Files: <game dir>/highlights/<date>_<time>.csv (a row a moment, written as it happens), and as the log stops .json
// (the same, sorted, with the session's details) and .edl (DaVinci Resolve's timeline markers: Timelines > Import >
// Timeline Markers from EDL, the sync mark at the timeline's start, 01:00:00:00). Misc/quakevr/trailer/ makes markers
// for a recording's real sync offset and a rough cut from the logs.
//
// The QC reports its moments through the builtin highlight() (QC/vr_highlights.qc); bullet time and multi-kills (kills
// close together, vr_highlights_multikill) are the engine's own.

#pragma once

namespace qvr::highlights
{

void init(); // the commands, vr_highlights' callback

// Once a frame (VR_BeginFrame): the game's clock against realtime (for moments that began earlier), a multi-kill's end.
void frame();

// The desktop window's 2D pass, last (VR_End2D, in VR): the sync mark's flash over the whole window.
void drawFlash();

void shutdown(); // a log still open is finished (its JSON and EDL written)

[[nodiscard]] bool active();

// A moment: `kind` (a word: "gib", "parry", ...), `score` (0 or less: the kind's own), what it was done to (`subject`,
// a classname; may be empty) and with (`detail`, free text), and the game time it began (`since`; 0: now).
void event(const char* kind, float score, const char* subject, const char* detail, double since = 0.0);

// Bullet time started or ended (vr_bullettime.cpp): one moment, as long as it ran.
void bulletTime(bool on, float scale);

} // namespace qvr::highlights
