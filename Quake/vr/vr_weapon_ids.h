// vr_weapon_ids.h -- the weapon ids (a hand's .weapon, STAT_QVR_WEAPON, a weapon prop's record) both the engine and the
// QuakeC (QC/vr_defs.qc) use, defined here only. Included by both: only #defines and // comments (FTEQCC's
// preprocessor reads them as C's). The QC reads the values as floats; the engine as ints, the same values.

#ifndef VR_WEAPON_IDS_H
#define VR_WEAPON_IDS_H

#define WID_FIST 0
#define WID_GRAPPLE 1
#define WID_AXE 2
#define WID_MJOLNIR 3 // HIPNOTIC.
#define WID_SHOTGUN 4
#define WID_SUPER_SHOTGUN 5
#define WID_NAILGUN 6
#define WID_SUPER_NAILGUN 7
#define WID_GRENADE_LAUNCHER 8
#define WID_PROXIMITY_GUN 9 // HIPNOTIC.
#define WID_ROCKET_LAUNCHER 10
#define WID_LIGHTNING 11
#define WID_LASER_CANNON 12 // HIPNOTIC.
#define WID_SWORD 13        // VR: a knight's sword (weaponflags QVR_WPNFLAG_USE_SECONDARY_AMMO: the hell knight's)
#define WID_CHAINSAW 14     // VR: an ogre's chainsaw (vr_chainsaw.qc; its clip is its fuel)
#define WID_GRUNTGUN 15     // VR: a grunt's shotgun (vr_enemyguns.qc; its clip is its only ammo)
#define WID_ENFRIFLE 16     // VR: an enforcer's laser rifle (vr_enemyguns.qc; its clip is its only ammo)
#define WID_CROWBAR 17      // VR: the crowbar (vr_crowbar.qc; a melee weapon, one hand or two, as the swords)
#define WID_SUPERAXE 18     // MG3: Dawn of the Machine's Super Axe (vr_mg3_weapons.qc; MG3's model read in place, any campaign)
#define WID_TOOLGUN 19      // VR: the toolgun (vr_toolgun.qc, the engine's vr_toolgun.cpp: no ammo, its tools the engine's)
// AD support: Arcane Dimensions' armed soldiers' own guns, dropped as they die (vr_ad_enemyguns.qc; single use, their
// clip their only ammo, as WID_GRUNTGUN's). Their models are made from AD's monster models in place (the engine's
// vr_adguns.cpp: "<monster model>#gun<skin>"), so they exist only while AD's data is installed.
#define WID_ADROCKET 20    // AD: the rocket soldier's launcher
#define WID_ADGRENADE 21   // AD: the grenade soldier's launcher
#define WID_ADPLASMA 22    // AD: the plasma soldier's rifle
#define WID_ADDEFENDER 23  // AD: the defender's shotgun
#define WID_ADELIMINATOR 24 // AD: gone (an older save's eliminator's plasma gun: the centurion's, VR_ADGun_LegacyIds)
#define WID_ADCENTURION 25 // AD: the centurion's plasma gun
#define WID_ADPYRO 26      // AD: the pyro's flamethrower
#define WID_ADFUMIGATOR 27 // AD: the fumigator's poison sprayer
#define WID_ADCROSSBOW 28  // AD: the crossbow knight's crossbow (vr_ad_enemyguns.qc: AD's bolts; its own bolts only)
// VR: the nail pistol (vr_nailpistol.qc; Misc/quakevr/models_src/nailpistol): one-handed, semi-automatic, a 12-round
// magazine in its grip fed from your nails; weapon settings slot 36 (vr_weapons.inc); weapon_nailpistol.
#define WID_NAILPISTOL 29
// VR: the flare gun (vr_flaregun.qc; Misc/quakevr/models_src/flaregun): disposable, two flares (one a barrel), its clip
// its only ammo (AID_NONE), thrown away spent; weapon settings slot 37 (vr_weapons.inc); weapon_flaregun.
#define WID_FLAREGUN 30
// VR: the heavy revolver (vr_revolver.qc; Misc/quakevr/models_src/revolver): one-handed, double action, six chambers
// of shells fired as slugs; its cylinder swings out to load by hand; weapon settings slot 38 (vr_weapons.inc).
#define WID_REVOLVER 31
#define WID_LAST 31 // the last weapon id

#endif
