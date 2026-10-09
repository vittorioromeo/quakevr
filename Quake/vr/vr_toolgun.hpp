// vr_toolgun.hpp -- the toolgun (QC WID_TOOLGUN, vr_toolgun.qc; docs/vr-port/TOOLGUN.md): a debug and sandbox tool held
// as a pistol, as Garry's Mod's. Its hand's buttons are its own: the trigger uses the chosen tool on what it points at
// (spawn, remove, physgun, scale, joint), B/Y opens its menu (VR Settings pages shown on a small panel over the gun,
// pointed at with the other hand's laser), X/A held turns both sticks into the tool's offsets and turn (no walking or
// turning meanwhile). Single player (a listen server's own edicts).

#pragma once

#include "vr_engine.hpp"
#include "vr_hands.hpp"
#include "vr_posing.hpp"

#include "Zancle/Container/Vector.hpp"

namespace qvr::toolgun
{

constexpr int weaponId = 19; // QC WID_TOOLGUN

// The tools (vr_toolgun_tool).
enum class Tool : int
{
    Spawn,
    Remove,
    Physgun,
    Scale,
    Joint,
    Count
};

void registerCommands();

// The hand holding the toolgun (HAND_MAIN first), -1 none (or dead, or no local server).
[[nodiscard]] int heldHand();

// The controllers (vr_input.cpp): a press or release of hand `hand`'s button `b`; true: taken, no key sent. The sticks
// each frame while sticksTaken (X/A held on the toolgun's hand): no walking, no turning.
bool button(int hand, posing::Button b, bool down);
[[nodiscard]] bool sticksTaken();
void sticks(const glm::vec2& off, const glm::vec2& main);

// Once a host frame, after the hands (vr_main.cpp): the aim, the tools' work, their lines and texts.
void frame(const hands::State& s);

// The client's temp entities (vr_client.cpp, after Quake's): the spawn tool's ghost.
void tempEntities();

// The menu on the gun: whether a toolgun page is shown with the toolgun in a hand (VR Settings on its panel), and the
// panel's centre and axes there (unit right and up, its normal towards the player) from the tracked controller.
[[nodiscard]] bool menuOnGun();
[[nodiscard]] bool menuFrame(const hands::State& s, glm::vec3& centre, glm::vec3& right, glm::vec3& up);

// How the renderers glow `e` (vr_fgfx.cpp's VR_EntityGlow): 0 none; 0..1 the force grab's hue (the physgun's, scale's
// and joint's targets); 2..3 red (the remover's target: 2 + strength).
[[nodiscard]] float entityGlow(const entity_t* e);

// The spawn list (the menu's Toolgun - Spawn page): each entry's label, and its section ("Monsters", "Props", ...: a
// new one where it changes).
[[nodiscard]] int spawnableCount();
[[nodiscard]] const char* spawnableLabel(int i);
[[nodiscard]] const char* spawnableSection(int i);
void selectSpawnable(int i); // (-1: nothing)

// The menu's info line: the tool, the selection and the offsets.
[[nodiscard]] const char* stateLine();

} // namespace qvr::toolgun
