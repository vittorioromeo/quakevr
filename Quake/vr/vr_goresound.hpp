// vr_goresound.hpp -- the gore's sounds (vr_gore_sound; docs/vr-port/ROUND22.md, "Gore sounds"): a stump's spurt with
// each pulse of its fountain, a blood stream's splatter where it lands, a beheaded or face-smashed monster's wet choke
// (instead of its cry), the wet rip and bone crack of a limb or a head torn off, a hand's squelch taking a gib or a dead
// body, wet steps after a pool of blood, a gib slapping into a wall. Cut from CC0 recordings (quakevr/sound/vr/gore/,
// make_gore_sounds.py; docs/vr-port/SOUNDS_OURS.md, "Gore").
//
// The client plays them (S_StartSoundPitch): each kind at most so many at once (the nearest kept: a nearer one takes the
// farthest's place, a farther one is dropped) and at most one every so often (a much nearer one still plays), lower in
// bullet time, vr_gore_sound loud. The QC's events (spurts, chokes, tears) and a ragdoll grabbed reach every client as
// QVR_SVC_GORESOUND (QC goresound); the streams, the pools, the steps, the held gibs and the gibs' wall hits are the
// client's own. Nothing with vr_gore 0 or vr_gore_sound 0; each kind times its own volume (vr_gore_sound_vol_<kind>).
// With vr_gore_sound_choke 1 (0 shipped: never, the cries never muted) a choke mutes its monster's voice channel on the
// server for a moment (its pain or death cry dropped: SV_StartSoundPitch asks voiceMuted), and plays on that channel (a
// cry already playing cut). Stepping into a pool of blood splashes (Splash), the steps after it wet.
#pragma once

#include <glm/glm.hpp>

namespace qvr::goresound
{

// Its kinds (QVR_SVC_GORESOUND's byte; QC's QVR_GORESND_*).
enum class Kind : int
{
    Spurt,   // a stump's fountain's pulse (QC vr_decap.qc)
    Pour,    // a blood stream landing on the floor (vr_bloodstream.cpp): grains back to back while it lands
    Choke,   // a beheaded or face-smashed monster (QC: on its voice channel)
    Tear,    // a limb or a head cut off (QC)
    Squelch, // a hand taking a gib or a dead body
    Step,    // your wet steps
    Slap,    // a gib into a wall or a ceiling (vr_gore.cpp gibImpact)
    Splash,  // your foot into a pool of blood (the client's own: stepFrame)
    Drip,    // a drop of blood hitting the floor (the client's own: drip)
    Count
};

// --- Server side ---

// QC goresound(kind, e, at, strength): sent to every client (false: not sent: off, or a choke too soon after the last).
// A choke mutes `ent`'s voice channel for a moment (voiceMuted).
bool send(Kind kind, int ent, const float* at, float strength);

// SV_StartSoundPitch: whether `ent`'s sound on `channel` is dropped (its cry while it chokes).
[[nodiscard]] bool voiceMuted(int ent, int channel);

// --- Client side ---

// QVR_SVC_GORESOUND (vr_client.cpp's dispatch).
void parseEvent();

// One of `kind` from `at` (`ent` its entity, 0 none), `strength` 0..1 (the volume's share).
void play(Kind kind, int ent, const glm::vec3& at, float strength);

// A drop of blood landing at `at` (yours, vr_bodyblood.cpp; a ceiling's or a stuck gib's, vr_gore.cpp), `strength` 0..1:
// a quiet drip, vr_gore_sound_drips of the drops (1: each), capped and rate-limited as the others.
void drip(const glm::vec3& at, float strength);

// A stream (`key`: its slot) pouring onto the floor at `at` this frame, `flow` 0..1 (vr_bloodstream.cpp).
void pour(int key, const glm::vec3& at, float flow);

// A pool of blood laid (decals::place): wet underfoot for a while.
void notePool(const glm::vec3& at, const glm::vec3& normal, float size);

// Whether `feet` stand in a pool still wet (notePool's, the last 64; `margin` units round each): the wet steps', the
// bloody footprints' (vr_bloodprints.cpp). A few dozen distances.
[[nodiscard]] bool poolUnder(const glm::vec3& feet, float margin = 6.f);

// A gib hit the world at `where` (normal `normal`) at `speed` units/s (gore::gibImpact): a wall's or a ceiling's slap.
void gibHit(const glm::vec3& where, const glm::vec3& normal, float speed);

// A map's load (VR_NewMap): the sounds loaded.
void prepare();

// Once a frame, after the streams (vr_view.cpp): the streams' grains, the steps, the hands, the tests.
void frame();

// All gone (a new map).
void clear();

// The console commands (vr_gore_sound_stats, vr_gore_sound_test).
void registerCommands();

} // namespace qvr::goresound
