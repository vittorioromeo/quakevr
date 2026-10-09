// vr_tips.hpp -- tips for new players (vr_tips): each shown once, the first time you come near what it is about and can
// see it (vr_tips_distance, vr_tips_view_angle, vr_tips_line_of_sight, for vr_tips_delay seconds). Shown as a CRT
// screen like the map boards' floating by it with a cable to it (vr_tips 1: over the scene; vr_tips_facing), or as one
// of the wrist gadget's hologram messages (vr_tips 2: it waits there until you look at the gadget, which chimes and
// buzzes your hand meanwhile; the screen when there is no hologram). The tips shown are kept in
// <gamedir>/tips_seen.txt (one key a line, no limit; an older config's vr_tips_seen is moved there once), emptied by
// vr_tips_reset (VR Settings > Tips > Show Tips Again).
//
// The built-in tips (vr_tips.cpp, `tips`): a wall torch on its wall (vr_walltorch.cpp: it can be taken and sets
// enemies on fire). A new one: its name, text and what it is about (a client entity's test); a tip for an action done
// the first time would call show() from where the action is noticed.
//
// The map's tips (`MapTip`, below): placed in a map as a func_vr_tip (QC/vr_tips.qc), with its text, range, and what
// it follows (an entity, or a fixed point) in its spawnkeys. Like the world texts (vr_worldtext.hpp), the server owns
// the list (made by the vr_tip_* builtins while the map spawns), broadcasts it, and replays the whole list to each
// client as it spawns; the client keeps a mirror and takes part in the frame loop above, after the built-in tips. A
// map tip shows as the floating screen whatever vr_tips is (1 or 2), unless its Hologram flag asks for the gadget. Its
// key in tips_seen.txt is <mapname>:<tipname> (or <mapname>#<index> when it has no name), so it is forgotten by
// vr_tips_reset with the rest, and cleared with the map.

#pragma once

#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

namespace qvr::tips
{

// func_vr_tip's spawnflags (the same bits in QC/vr_tips.qc and the entity definition).
enum Flags : int
{
    Repeat = 1 << 0,   // shown every time you come near, not remembered
    Hologram = 1 << 1, // in the wrist gadget's hologram, not the floating screen
    AnyAngle = 1 << 2, // shown even out of vr_tips_view_angle or hidden by the world
    Waiting = 1 << 3   // not shown yet: a TRIGGERED tip not used (QC VR_Tip_Use clears it; vrtutorial's)
};

// MapTip::ent for a tip whose entity is gone (freed, or its slot taken by another): it never shows again.
constexpr int goneEntity = -2;

// A tip the map supplies. The zero defaults mean the player's own settings (vr_tips_distance, vr_tips_size,
// vr_tips_delay), so a placed tip follows what new players set unless the map says otherwise.
struct MapTip
{
    za::String name;        // its name in tips_seen.txt, with the map's (<mapname>:<name>); empty: <mapname>#<index>
    za::String text;        // its lines (\n new lines)
    glm::vec3 pos{0.f};     // its fixed point (or where it started, when it follows an entity)
    int ent{-1};            // the client entity it follows (its origin and model box, live); -1: a fixed point;
                            // goneEntity: it followed one that is gone
    float distance{0.f};    // how near you must come, units (0: vr_tips_distance)
    float size{0.f};        // the screen's text size (0: vr_tips_size)
    float delay{-1.f};      // seconds you must stay near and see it (below 0: vr_tips_delay)
    int flags{0};           // Flags
    bool shownNear{false};  // client: a Repeat tip shown, and the player not yet gone out of its range since
    za::String followClass; // server: the classname of the entity it follows (another in its slot: gone)
};

// Server side (the vr_tip_* builtins, while the map spawns; QC/vr_tips.qc func_vr_tip).
void serverReset();
[[nodiscard]] int serverMake();
void serverSetName(int handle, const char* name);
void serverSetText(int handle, const char* text);
void serverSetPos(int handle, const glm::vec3& pos);
void serverSetEntity(int handle, int ent, const char* classname); // -1: a fixed point
void serverSetDistance(int handle, float distance);
void serverSetSize(int handle, float size);
void serverSetDelay(int handle, float delay);
void serverSetFlags(int handle, int flags);
void serverWriteAll(sizebuf_t* msg); // replay for a spawning client
void serverFrame(); // once a server frame: a followed entity freed or replaced makes its tip's entity goneEntity

// Client side.
void clientReset();
void clientParse(int subcmd); // QVR_SVC_TIP_*
void clientWriteAll(sizebuf_t* msg); // the client's list, into a demo recorded in the middle of a map

// Once a frame, after the frame's texts are cleared (VR_BeginFrame): a tip due shown, the one showing laid out.
void frame();

// vr_tips_reset: every tip as never shown (and the one showing gone).
void reset_f();

// vr_tips_test [name]: a tip shown now (the first one by default) on the nearest of what it is about in view, however
// far, as vr_tips shows it, without counting it as shown: to try the two ways and the settings. A map tip's name
// (its tipname; #<n> for an unnamed one) works too. vr_tips_test list: every tip here, and how it shows.
void test_f();

} // namespace qvr::tips
