// vr_gunfx.hpp -- gunfire particles (docs/vr-port/ROUND23.md, "Punchier gunfire"): per shot, by the weapon's id
// (vr_weapon_ids.h), one path for your guns, the armed grunts' held weapons and the grunts' and enforcers' own guns:
// - a cone of sparks out of the muzzle (each barrel of the super shotgun's; few for the launchers, small for the
//   nailguns; the energy guns throw electric arcs instead);
// - a fast-dissipating cone of smoke out of the muzzle, bigger than the QC's gun smoke puff, drifting with the shooter,
//   the flash's warm glow in it for a moment (the smoke is lit by the place and the shot's light); never the energy guns';
// - a smaller quick puff out of a shotgun's ejection port: as each casing leaves it (vr_shells.cpp: the shotgun's pump,
//   the super shotgun's open breech, the Widowmaker's slot), at the shot for the guns that throw none (the grunts');
// - the rocket launchers' backblast: smoke behind the launcher and dust off the floor under it;
// - barrel heat: sustained fire (the nailguns) leaves the muzzle smoking in thin wisps until it cools;
// - every spent casing's smoke trail stronger (vr_gunfx_shell_trail, vr_shells.cpp).
// Per-shot budgets shrink at high fire rates (each shooter's time since its last shot); nothing allocated (the
// particles' pool). Settings: Weapons > Weapon Effects > Gunfire Particles (vr_gunfx_*). Tests: vr_debug_gunfx 1 prints
// each shot's particles ("gunfx shot ..."); Misc/quakevr/gunfx_test.py fires every weapon class (yours and the armed
// grunts') and checks them against the table; vr_gunfx_test <wid> [count] fires that weapon's effects ahead of you.
// Not affected: the flamethrowers, the crossbow, melee, the grapple, the toolgun, Mjolnir, the monsters' own attacks.

#pragma once

#include "vr_view.hpp"

struct qmodel_s;

namespace qvr::gunfx
{

// A gun's muzzle as drawn: where, the way it points and its up (unit vectors), the shooter's motion (units a second)
// and the gun's length (units; 0 unknown).
struct Muzzle
{
    glm::vec3 at{0.f};
    glm::vec3 fwd{1.f, 0.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    glm::vec3 drift{0.f};
    float gunLength{0.f};
};

// The weapon id a gun model is drawn with (our weapons' models by name, an AD gun variant's), else -1.
[[nodiscard]] int weaponIdOf(const qmodel_t* model);

// A shot by weapon `wid` out of `m`. `shooter`: 0, 1 your hands, else the monster's entity (its fire rate's key).
// `adBurst`: an AD gun's own sparks and backblast made already (vr_weaponfx.cpp adGunBurst): not again. `ports`: the
// gun throws casings out of its port itself (vr_shells.cpp: its port's puff then), else a shotgun's puff now.
void shot(int wid, const Muzzle& m, int shooter, bool adBurst, bool ports);

// Your hand `hand` fired (vr_weaponfx.cpp): its particles at its drawn muzzle at the next handFrame.
void playerFired(int hand);
// Once a frame, each hand's drawn weapon `w` (vr_weaponfx.cpp frame; `slot` its weapon settings, -1 none): its shot due,
// its barrel's heat.
void handFrame(int hand, const view::ViewEntity& w, int slot);

// A monster's gun fired, its muzzle `m` (vr_weaponfx.cpp: a grunt's, an enforcer's, an armed grunt's held weapon of
// model `model`, null for a monster's own gun of weapon `wid`).
void enemyFired(int ent, const qmodel_t* model, int wid, const Muzzle& m);

// A casing thrown out of a shotgun's port at `at` going `dir` (vr_shells.cpp): the port's quick puff (`hot`: a flick
// reload's, more).
void portSmoke(const glm::vec3& at, const glm::vec3& dir, bool hot);
// The casings' smoke trails' strength, times (vr_gunfx_shell_trail).
[[nodiscard]] float shellTrail();

// Everything forgotten (a new map, a disconnect).
void clear();

// vr_gunfx_test, vr_gunfx_report.
void registerCommands();

} // namespace qvr::gunfx
