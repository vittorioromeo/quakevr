// vr_notectx.hpp -- a voice note's context beyond the moment's basics (vr_voicenotes.cpp; docs/vr-port/VOICENOTES.md):
// what the workers reading his notes need to find what he meant and reproduce it.
//
// - The checklist item last under the menu's cursor (or picked) and the scenario last run by its Test (testRun).
// - What each hand holds or reaches: the weapon (id, name, model, offsets slot, flags, clip, ammo), a carried prop, an
//   enemy held (vr_foegrab: where, the hold, the two-hand moves), a ragdoll's limb (its part, its struggle), the force
//   grab's target; the monsters within 5 m (class, distance, health, think function, enemy, armed grunt's weapon,
//   knocked down, held, struggling).
// - The settings changed this session (archived cvars: first value -> now, how many changes, when last).
// - The last vr_note_replay_seconds of console lines and frame times (the hitches), kept here in small rings; the
//   instant replay itself (poses, buttons, the player, nearby entities) is the motion recorder's (vr_motion.hpp
//   saveReplay).
//
// Everything is gathered on the main thread as the note starts (strings, copies of the rings: well under a millisecond)
// and written on the thread pool.

#pragma once

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

struct cvar_s;

namespace qvr::notectx
{

void frame(); // once a host frame (voicenotes::frame): the frame time ring

// The checklist's Test button ran `scenario` ("" : none): named in the notes that follow.
void testRun(const char* scenario);

// The context's lines ("key: value", one thing a line) appended to `out`. Main thread.
void describe(za::String& out);

// The last `seconds` of console lines and of host frames, copied out (main thread), and written (any thread):
// <note>_log.txt (each line with its time before the note) and <note>_frames.csv (each frame's time; the hitches
// summed up in its header). The summary (frames, the median, the worst, the hitches) for the note's .txt.
struct ConsoleLine
{
    double realtime{0.0};
    za::String text;
};
struct FrameTime
{
    double realtime{0.0}; // the engine's clock at the frame
    float dt{0.f};        // its frame time (host_rawframetime: the wall clock's, unclamped)
};
struct Logs
{
    double noteAt{0.0}; // realtime as the note started
    za::Vector<ConsoleLine> console;
    za::Vector<FrameTime> frames;
};
[[nodiscard]] Logs takeLogs(double seconds);
[[nodiscard]] za::String frameSummary(const Logs& logs);
[[nodiscard]] bool writeConsole(const za::String& path, const Logs& logs);
[[nodiscard]] bool writeFrames(const za::String& path, const Logs& logs);

// The rings' memory (bytes) and counts (vr_note_replay_stats).
void stats(za::String& out);

} // namespace qvr::notectx

// console.c Con_Printf (the main thread's prints): the console ring. cvar.c Cvar_SetQuick: an archived setting about to
// change from its string to `value`.
extern "C" void VR_NoteConsolePrint(const char* text);
extern "C" void VR_NoteCvarChanging(struct cvar_s* var, const char* value);
