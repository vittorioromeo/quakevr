// vr_mgroups.inc -- the monster groups (vr_mgroups.cpp; Combat > Monster Groups; ROUND22.md, "Monster groups"): similar
// monsters of id1, the mission packs, Dawn of the Machine, Arcane Dimensions (and Quoth's, which it plays) share one
// group, and each group its own settings for every ragdoll trigger and every action on it. The members are
// vr_mgroups.cpp's (by classname); a classname not there is in Others.
//
// QVR_MGROUP(id, label, kdtrig, resist, shove, humanoid, throw, push, legs, poise): the group's defaults (each its effect before the groups):
// - kdtrig: the ragdoll triggers take it (vr_mgrp_<id>_kdtrig); resist: its resistance to them (vr_kdtrig_resist_<id>,
//   0: by its size); every trigger's own multiplier (vr_mgrp_<id>_ledge ... _blast) 1;
// - shove: a shove's base chance to knock it down (vr_knockdown_chance_<id>; only a monster with a get-up of its own);
//   hits unbalancing it (_stagger) 1;
// - humanoid: the lift, the neck snap and the manhandle take it (_lift,
//   _necksnap, _manhandle);
// - throw: the two-hand throw (_throw): 0 never, 1 only when hurt, 2 always; one hand's hold (_hold) -1: by its size;
// - push: the shove's, bash's and unbalancing hit's push on it (_push; 0: bosses stand their ground);
// - legs: the leg sweep and the leg grabs (_sweep, _leggrab; the skull wizard's robe has no legs: 0 though humanoid);
// - poise: its Parry Poise (_poise; ROUND22.md, "Parry balance"): a parry's strength over it decides blocked, flinch or
//   stagger (QC combat.qc VR_Parry_Outcome), and above 1 it also divides the melee's chances to knock it down or
//   unbalance it (VR_Poise_Factor); below 0: a parry never staggers it (Off).
// The cvars themselves: vr_cvars.inc (QVR_MGROUP there). Keep the ids: the first eleven's shove chances and the first six's
// resistances are the older per-kind settings' names.
QVR_MGROUP(army, "Grunts", 1, 1, 0.45, 1, 2, 1, 1, 0.3)
QVR_MGROUP(enforcer, "Enforcers", 1, 1.15, 0.3, 1, 2, 1, 1, 0.3)
QVR_MGROUP(knight, "Knights", 1, 1.15, 0.25, 1, 2, 1, 1, 0.7)
QVR_MGROUP(hknight, "Death Knights", 1, 1.6, 0.12, 1, 1, 1, 1, 1)
QVR_MGROUP(ogre, "Ogres", 1, 2.5, 0.15, 0, 1, 1, 0, 1.6)
QVR_MGROUP(zombie, "Zombies", 1, 1, 0.35, 1, 2, 1, 1, 0.5)
QVR_MGROUP(mummy, "Mummies", 1, 0, 0.2, 1, 2, 1, 1, 0.7)
QVR_MGROUP(dog, "Rottweilers", 0, 0, 0.35, 0, 2, 1, 0, 0.4)
QVR_MGROUP(demon, "Fiends", 0, 0, 0.1, 0, 1, 1, 0, 1.2)
QVR_MGROUP(shambler, "Shamblers", 0, 0, 0.03, 0, 0, 1, 0, -1)
QVR_MGROUP(gremlin, "Gremlins", 0, 0, 0.25, 0, 0, 1, 0, 0.3)
QVR_MGROUP(skullwiz, "Skull Wizards", 0, 0, 0.45, 1, 0, 1, 0, 0.6)
QVR_MGROUP(crawler, "Spiders and Crawlers", 0, 0, 0.35, 0, 0, 1, 0, 0.5)
QVR_MGROUP(giant, "Giants", 0, 0, 0.03, 0, 0, 1, 0, -1)
QVR_MGROUP(wizard, "Scrags and Gargoyles", 0, 0, 0, 0, 0, 1, 0, 0.4)
QVR_MGROUP(shalrath, "Vores", 0, 0, 0, 0, 0, 1, 0, 1.2)
QVR_MGROUP(tarbaby, "Spawns", 0, 0, 0, 0, 0, 1, 0, 0.6)
QVR_MGROUP(fish, "Fish and Eels", 0, 0, 0, 0, 0, 1, 0, 0.3)
QVR_MGROUP(floater, "Wraiths and Floaters", 0, 0, 0, 0, 0, 1, 0, 1.2)
QVR_MGROUP(machine, "Machines", 0, 0, 0, 0, 0, 1, 0, 1.6)
QVR_MGROUP(boss, "Bosses", 0, 0, 0, 0, 0, 0, 0, -1)
QVR_MGROUP(other, "Others", 0, 0, 0, 0, 0, 1, 0, 1)
