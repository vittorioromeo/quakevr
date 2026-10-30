// vr_toolgun.hpp -- the toolgun (QC WID_TOOLGUN, vr_toolgun.qc; docs/vr-port/TOOLGUN.md): a debug and sandbox tool held
// as a pistol, as Garry's Mod's. Its hand's buttons are its own: the trigger uses the chosen tool on what it points at
// (spawn, remove, physgun, scale, joint), the stick's press switches to the next tool (X/A held: the one before), B/Y
// opens its panel (vr_toolgun_panel.cpp: a small panel of its own over the gun, pointed at with the other hand's thin
// laser), X/A held turns both sticks into the tool's offsets and turn (no walking or turning meanwhile). Single player
// (a listen server's own edicts).

#pragma once

#include "vr_client.hpp"
#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_posing.hpp"
#include "vr_weapon_ids.h"

#include "Zancle/Container/Vector.hpp"

namespace qvr::toolgun
{

constexpr int weaponId = WID_TOOLGUN;

// The tools (vr_toolgun_tool).
enum class Tool : int
{
    Spawn,
    Remove,
    Physgun,
    Scale,
    Joint,
    Brush, // boxes of the map's kind (vr_toolgun_brush.cpp)
    Count
};

void registerCommands();

// A new world (vr_progs.cpp resetServerWorld): what the tools hold by entity number forgotten.
void reset();
// An entity removed (vr_builtins.cpp onEdictFree): the tools let go of it.
void forget(int num);

// The hand holding the toolgun (HAND_MAIN first), -1 none (or dead, or no local server).
[[nodiscard]] int heldHand();

// The controllers (vr_input.cpp): a press or release of hand `hand`'s button `b`; true: taken, no key sent. The sticks
// each frame while sticksTaken (the physgun holding something, or X/A held on the toolgun's hand): no walking, no
// turning.
bool button(int hand, posing::Button b, bool down);
[[nodiscard]] bool sticksTaken();
void sticks(const glm::vec2& off, const glm::vec2& main);

// Once a host frame, after the hands (vr_main.cpp): the aim, the tools' work, their lines and texts.
void frame(const hands::State& s);

// The client's temp entities (vr_client.cpp, after Quake's): the spawn tool's ghost.
void tempEntities();

// The ghost's networked scale and offsets (render::networkData), when `e` is the ghost drawn as an item will be once
// placed (its probe's: a box shrunk, a pickup scaled); null otherwise.
[[nodiscard]] const client::EntityVr* ghostVr(const entity_t* e);

// The tracked controller of `hand` in the world (as the menus' laser: vr_menuui.cpp pointerRay): its place and its aim's
// axes (forward, right, up), not the drawn hand's (which follows the held weapon's weight).
[[nodiscard]] bool controllerFrame(const hands::State& s, int hand, glm::vec3& origin, glm::vec3& fwd, glm::vec3& right,
    glm::vec3& up);

// How the renderers glow `e` (vr_fgfx.cpp's VR_EntityGlow): 0 none; 0..1 the force grab's hue (the physgun's, scale's
// and joint's targets); 2..3 red (the remover's target: 2 + strength).
[[nodiscard]] float entityGlow(const entity_t* e);

// The spawn list (the panel's grid): each entry's name (`spawnableLabel`: marked "> " when chosen), its section
// ("Monsters", "Props", ...), its model and skin (the thumbnail's); the entry chosen (-1: nothing, or one picked from the
// world).
[[nodiscard]] int spawnableCount();
[[nodiscard]] const char* spawnableLabel(int i);
[[nodiscard]] const char* spawnableName(int i);
[[nodiscard]] const char* spawnableSection(int i);
[[nodiscard]] const char* spawnableModel(int i);
[[nodiscard]] int spawnableSkin(int i);
[[nodiscard]] bool spawnableAvailable(int i); // AD support
// Why entry `i` is unavailable (the file missing, the rules it needs), "" when it is available: printed when its greyed
// tile is pressed, and by vr_toolgun_spawnlist.
[[nodiscard]] const char* spawnableUnavailableWhy(int i);
[[nodiscard]] bool spawnableAD(int i); // AD support
// The pack an entry comes from: its group on the panel ("" id's and Quake VR's own, "AD", "SoA", "DoE", "MG3", ...).
[[nodiscard]] const char* spawnablePack(int i);
[[nodiscard]] int spawnableDebugChoice(int i); // AD support
[[nodiscard]] const char* spawnableClass(int i); // AD support
// The spawn list's entry `i` made standing at `feet` (units) turned to `yaw` (degrees), as the spawn tool makes it (the
// ragdoll tool's subject: vr_ragtool.cpp); null if it is not available or could not be made.
edict_t* spawnEntryAt(int i, const glm::vec3& feet, float yaw);
[[nodiscard]] int selectedSpawnable();
void selectSpawnable(int i, bool say = true); // (-1: nothing; say: print what is selected)

// The Armed Humanoid page (the panel's; QC vr_armedmon_ui.qc; docs/vr-port/ARMED_MONSTERS.md): the armed monsters'
// table's rows (the kinds) and the weapons they may hold, asked of QC once a map (refresh: the host frame); the choice
// is vr_toolgun_armed_kind, _weapon (0 random from the pool, -1 unarmed, else a weapon's "weapon" code) and _weapon2
// (its left hand's: 0 none).
namespace armed
{
void refresh();
[[nodiscard]] int generation(); // counts the lists' rebuilds
[[nodiscard]] int kindCount();
[[nodiscard]] const char* kindName(int k);
[[nodiscard]] const char* kindModel(int k);
[[nodiscard]] const char* kindSpawn(int k); // its spawn function (classname: monster_vr_grunt)
[[nodiscard]] int weaponCount(); // every kind's weapons (melee first)
[[nodiscard]] int weaponCode(int i);
[[nodiscard]] const char* codeName(int code); // "Random from Pool", "Unarmed", a weapon's name
[[nodiscard]] int fits(int kind, int code);    // 0: not held by that kind; else 1, + 2 it pairs, + 4 * its class
[[nodiscard]] bool oneHanded(int kind, int code);          // it pairs (a one-handed melee weapon, a gun not too big)
[[nodiscard]] bool pairs(int kind, int code, int code2);   // the two held one in each hand (both melee or both guns)
[[nodiscard]] bool chosen(); // the spawn tool's choice is the page's
void choose();               // the page's choice made the spawn tool's (the cvars made valid for the kind first)
[[nodiscard]] const char* ghostModel(int hand); // the choice's held weapons' models (0 right, 1 left; "": none)
} // namespace armed

// The tools' and the joints' names (vr_toolgun_tool, vr_toolgun_joint), and the tool chosen (said on the console, a tick
// in the gun's hand).
[[nodiscard]] const char* toolName(int tool);
[[nodiscard]] int jointCount(); // the joint tool's choices (the last: Unjoin)
[[nodiscard]] const char* jointName(int joint);
void setTool(int tool);


// The panel's info line: the tool, the selection and the offsets; and the gun's screen's (the tool's name, and its
// choice).
[[nodiscard]] const char* stateLine();
[[nodiscard]] const char* screenText();

// The toolgun's panel (vr_toolgun_panel.cpp; docs/vr-port/TOOLGUN.md): a small panel of its own over the gun, B/Y
// opens and closes it, the other hand's thin laser points at it and its trigger clicks. Its top row the tools (a tap
// switches), under it the chosen tool's own choices: the spawn list as a grid of thumbnails in tabs, the joints as tiles,
// the physgun's and the scale tool's switches; More: the cheats and the panel's size.
namespace panel
{
// Open (B/Y), and shown: the toolgun in a hand, in the game (not under a menu).
[[nodiscard]] bool shown();
void toggle();
void close();
// The other hand's trigger (from button()): a press with its laser on the panel, and that press's release, are the
// panel's (true: taken).
bool trigger(int hand, bool down);
// Once a host frame (from frame()): where the laser meets it, what it lights up, a click's or a drag's work.
void frame(const hands::State& s);
// The 2D pass's end (vr_panel.cpp VR_End2D): its image, and the thumbnails waiting.
void render();
// In each eye, after the scene (vr_panel.cpp drawInEye): the panel and the laser.
void drawInEye(const hands::State& s);
void registerCommands();
} // namespace panel

} // namespace qvr::toolgun
