// vr_mgroups.inc -- the monster groups (vr_mgroups.cpp; Combat > Monster Groups; ROUND22.md, "Monster groups"): similar
// monsters of id1, the mission packs, Dawn of the Machine, Arcane Dimensions (and Quoth's, which it plays) share one
// group, and each group its own settings for every ragdoll trigger and every action on it. The members are
// vr_mgroups.cpp's (by classname); a classname not there is in Others.
//
// QVR_MGROUP(id, label): a group, in the menus' order. Its values (one of every vr_mgroups.hpp Param; no cvars):
// quakevr/data/groups.txt's `group <id>` block (design review 3, step 4; ROUND23.md, "Monster groups as data"), read
// into vr_mgroups.cpp's table, each reached by its old cvar name: vr_mgrp_<id>_<key>, vr_kdtrig_resist_<id> (resist),
// vr_knockdown_chance_<id> (shove). A new group: its line here and its block there (Misc/quakevr/groups_data.py
// check). Keep the ids: they are the values' names (the first eleven's shove chances and the first six's resistances
// the older per-kind settings', the QC's cvarh_vr_knockdown_chance_<id>).
QVR_MGROUP(army, "Grunts")
QVR_MGROUP(enforcer, "Enforcers")
QVR_MGROUP(knight, "Knights")
QVR_MGROUP(hknight, "Death Knights")
// The rare strong humanoids (the author, 2026-10-23: "you cannot just cheese it by turning it into a ragdoll"): the
// death knights' settings, then half every knockdown chance, half the time down, a faster get-up and a neck snap that
// only hurts (Mini Bosses); about a quarter less of every chance and a faster get-up (Elite Knights).
QVR_MGROUP(miniboss, "Mini Bosses")
QVR_MGROUP(eliteknight, "Elite Knights")
QVR_MGROUP(ogre, "Ogres")
QVR_MGROUP(zombie, "Zombies")
QVR_MGROUP(mummy, "Mummies")
QVR_MGROUP(dog, "Rottweilers")
QVR_MGROUP(demon, "Fiends")
QVR_MGROUP(shambler, "Shamblers")
QVR_MGROUP(gremlin, "Gremlins")
QVR_MGROUP(skullwiz, "Skull Wizards")
QVR_MGROUP(crawler, "Spiders and Crawlers")
QVR_MGROUP(giant, "Giants")
QVR_MGROUP(wizard, "Scrags and Gargoyles")
QVR_MGROUP(shalrath, "Vores")
QVR_MGROUP(tarbaby, "Spawns")
QVR_MGROUP(fish, "Fish and Eels")
QVR_MGROUP(floater, "Wraiths and Floaters")
QVR_MGROUP(machine, "Machines")
QVR_MGROUP(boss, "Bosses")
QVR_MGROUP(other, "Others")
