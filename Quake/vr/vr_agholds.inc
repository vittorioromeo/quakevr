// Armed humanoids' weapon holds (Combat > Armed Grunts > Weapon Holds; vr_aggrip.cpp agholds; QC vr_armedgrunt_pose.qc
// VR_AGH_*; ROUND22.md, "Armed humanoids' weapon holds"): a row per weapon an armed humanoid can hold, each kind's hold
// of it one setting, vr_agrunt_hold_<kind>_<key> (grunt, enforcer): 0 Auto (the row's own below), 1 One-Handed (the
// off hand free, in its idle or carry pose; the weapon aimed and kicked by the one hand), 2 Two-Handed (the off hand on
// the weapon's second grip, else under and round the hand on its handle). Live, archived. Dual wielding never asks.
//
// QVR_AGHOLD(key, code, label, grunt, enforcer)
//   key       the settings' suffix: never renamed (a config keeps its value by it); a row dropped has its two names
//             put in vr_cvars.cpp's retiredCvars
//   code      the weapon as QC names it (VR_AGH_Code): its weapon id (WID_*), AD's upgrades by the armed codes (101 the
//             Shadow Axe, 102 the Widowmaker, 103 the Plasma Gun), AD's heavy blades 110 + their kind (115 the hammer,
//             116 the mace, 117 the Phantom Swordsman's sword; AD's other swords are the Sword row's, 13)
//   grunt, enforcer   Auto's hands for that kind (as they were before these settings, but: the axe in one hand for
//             every kind, the enforcer's pistol-sized guns in one hand)
//
// Melee weapons.
QVR_AGHOLD(axe, 2, "Axe", 1, 1)
QVR_AGHOLD(shadowaxe, 101, "Shadow Axe (AD)", 1, 1)
QVR_AGHOLD(crowbar, 17, "Crowbar", 1, 1)
QVR_AGHOLD(sword, 13, "Sword", 1, 1)
QVR_AGHOLD(mjolnir, 3, "Mjolnir", 2, 2)
QVR_AGHOLD(superaxe, 18, "Super Axe", 2, 2)
QVR_AGHOLD(chainsaw, 14, "Chainsaw", 2, 2)
QVR_AGHOLD(adhammer, 115, "Hammer (AD)", 2, 2)
QVR_AGHOLD(admace, 116, "Mace (AD)", 2, 2)
QVR_AGHOLD(phantomsword, 117, "Phantom Sword", 2, 2)
// Pistols.
QVR_AGHOLD(nailpistol, 29, "Nail Pistol", 2, 1)
QVR_AGHOLD(revolver, 31, "Revolver", 1, 1)
QVR_AGHOLD(flaregun, 30, "Flare Gun", 1, 1)
// Long guns.
QVR_AGHOLD(shotgun, 4, "Shotgun", 2, 2)
QVR_AGHOLD(supershotgun, 5, "Double-Barrelled Shotgun", 2, 2)
QVR_AGHOLD(widowmaker, 102, "Widowmaker (AD)", 2, 2)
QVR_AGHOLD(gruntgun, 15, "Grunt's Shotgun", 2, 2)
QVR_AGHOLD(enfrifle, 16, "Enforcer's Laser Rifle", 2, 2)
QVR_AGHOLD(nailgun, 6, "Nailgun", 2, 2)
QVR_AGHOLD(supernailgun, 7, "Super Nailgun", 2, 2)
QVR_AGHOLD(grenade, 8, "Grenade Launcher", 2, 2)
QVR_AGHOLD(proximity, 9, "Proximity Gun", 2, 2)
QVR_AGHOLD(rocket, 10, "Rocket Launcher", 2, 2)
QVR_AGHOLD(lightning, 11, "Thunderbolt", 2, 2)
QVR_AGHOLD(plasmagun, 103, "Plasma Gun (AD)", 2, 2)
QVR_AGHOLD(lasercannon, 12, "Laser Cannon", 2, 2)
QVR_AGHOLD(adrocket, 20, "Rocket Soldier's Launcher", 2, 2)
QVR_AGHOLD(adgrenade, 21, "Grenade Soldier's Launcher", 2, 2)
QVR_AGHOLD(adplasma, 22, "Plasma Soldier's Rifle", 2, 2)
QVR_AGHOLD(addefender, 23, "Defender's Shotgun", 2, 2)
QVR_AGHOLD(adcenturion, 25, "Centurion's Plasma Gun", 2, 2)
QVR_AGHOLD(adpyro, 26, "Pyro's Flamethrower", 2, 2)
QVR_AGHOLD(adfumigator, 27, "Fumigator's Poison Sprayer", 2, 2)
QVR_AGHOLD(adcrossbow, 28, "Crossbow Knight's Crossbow", 2, 2)
