#pragma once
// vr_mgroups.hpp -- monster groups (Combat > Monster Groups; ROUND22.md, "Monster groups"): similar monsters of id1, the
// mission packs, Dawn of the Machine and Arcane Dimensions (Quoth's through it) share a group (vr_mgroups.inc: the groups
// and their defaults; vr_mgroups.cpp: the members, by classname), and each group has its own setting for every ragdoll
// trigger (QC vr_kdtrig.qc) and every action on it: the shove's knockdown and push, hits unbalancing it, the leg sweep,
// the leg grabs, the lift, the neck snap, the manhandle, the two-hand throw, one hand's hold (QC and vr_foegrab.cpp).
// The QC asks through monstergroup and monstergroupget (its MGP_* numbers are Param's: QC builtins.qc). vr_mgroup_list
// prints every classname's group and the map's monsters' (Debug > Combat).

#include "vr_engine.hpp"

namespace qvr::mgroups
{

// A group's settings, in this order (QC builtins.qc MGP_*: keep them alike).
enum class Param : int
{
    KdTrig,    // the ragdoll triggers take it (vr_mgrp_<g>_kdtrig)
    Resist,    // its resistance to them (vr_kdtrig_resist_<g>; 0: by its size)
    Ledge,     // each trigger's chance times this (vr_mgrp_<g>_ledge ... _blast), in vr_kdtrig.qc's table order
    Stairs,
    Slope,
    Shotgun,
    Wounded,
    Blow,
    Blast,
    Thrown,    // the thrown body's chance times this (vr_mgrp_<g>_thrown), and the tackle's (_tackle): no hits, their own
    Tackle,    // entries (QC VR_KdTrig_BodyHit, VR_KdTrig_Tackle), the table's last two
    Shove,    // a shove's base chance to knock it down (vr_knockdown_chance_<g>)
    Push,      // how far shoves, bashes and unbalancing hits move it, times
    Stagger,   // hits' chance to unbalance it, times
    Sweep,     // the leg sweep takes it
    LegGrab,   // the leg pull and the leg lift
    Lift,      // the lift
    NeckSnap,  // the neck snap
    Manhandle, // grabbed staggered or weak, it goes down in the hand
    Throw,     // the two-hand throw: 0 never, 1 only when hurt, 2 always
    Hold,      // one hand's hold on it, 0..1 (-1: by its size)
    Poise,     // its Parry Poise: a parry's strength over it decides its stagger (below 0: never staggered)
    Health,    // its health times this as it appears (with Monster Health's curve: vr_monster_health)
    KdScale,   // every chance to knock it down times this (a sure knockdown by this chance too)
    DownTime,  // its time down times this (its own timer, the most it is kept down)
    GetUp,     // its get-up's speed times this
    SnapKills, // the neck snap kills it (0: it only hurts)
    Struggle,         // held while knocked down, it struggles (its class's profile: struggleProfile)
    StruggleStrength, // its struggle's strength times this
    StruggleSpeed,    // its struggle's speed times this
    StruggleArms,     // its arms' drive while it struggles times this
    StruggleHurt,     // it struggles weaker as it is hurt (0: always at full effort)
    Count
};
inline constexpr int paramCount = static_cast<int>(Param::Count);

enum GroupId : int
{
#define QVR_MGROUP(g, ...) g##Group,
#include "vr_mgroups.inc"
#undef QVR_MGROUP
    groupCount
};

// The group of a monster with classname `classname` (Others when it is in none).
[[nodiscard]] int groupOfClass(const char* classname);
[[nodiscard]] int groupOf(edict_t* m);
[[nodiscard]] const char* label(int g);
[[nodiscard]] const char* id(int g);
// Its members' classnames, comma-separated (the menu's help).
[[nodiscard]] const char* members(int g);
[[nodiscard]] cvar_t& setting(int g, Param p);
[[nodiscard]] float value(edict_t* m, Param p);
// Whether an on/off setting of `m`'s group is on.
[[nodiscard]] bool on(edict_t* m, Param p);

// float(entity e) monstergroup: e's group (its classname's).
void PF_monstergroup();
// float(entity e, float param) monstergroupget: the setting `param` (MGP_*) of e's group.
void PF_monstergroupget();

// Monster health (Combat > Monster Health; vr_cvars.inc vr_monster_health*): the curve's cut (0..1) for a monster of
// full health `full`, and the multiplier a monster of `classname` and that full health gets (its group's times; 1 with
// Monster Health off, for the scripted bosses and for 20000 health or more: a scripted invulnerability).
[[nodiscard]] float healthCut(float full);
[[nodiscard]] float healthMultiplier(const char* classname, float full);
// The menu's examples: line `i` (an example monster's health now), or nullptr past the last.
[[nodiscard]] const char* healthExampleLine(int i);

// A class's struggle in a player's hands (vr_box3d_struggle.cpp; STRUGGLE.md, "Classes"): its strength, speed, full
// health (for its effort as it is hurt; 0: always full) and its arms' springs that many times stiffer (1..2.5).
struct StruggleProfile
{
    const char* classname;
    float strength, speed, health, arms;
};
// Its own profile, or nullptr: a class without one never struggles (ogres, fiends...).
[[nodiscard]] const StruggleProfile* struggleProfile(const char* classname);
// A class's struggle as its group sets it (vr_box3d_struggle.cpp): its profile with the group's Struggle on, strength,
// speed and arms times the group's (clamped 0..3, 0.2..3, 1..2.5), its full health 0 without Weaker When Hurt. False:
// it does not struggle.
[[nodiscard]] bool struggleOfClass(const char* classname, StruggleProfile& out);
// Group `g`'s members with a profile, each "name strength/speed/health/arms", comma-separated (its menu page's help).
[[nodiscard]] const char* struggleProfilesText(int g);
// A config older than 190: its vr_struggle_classes (retired; the value its line had) into the groups' Struggle settings.
void migrateStruggleClasses(const char* classes);

void registerCommands();

} // namespace qvr::mgroups
