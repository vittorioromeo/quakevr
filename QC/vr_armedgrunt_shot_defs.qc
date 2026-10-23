//
//
//
// ----------------------------------------------------------------------------
// Shooting an armed grunt's weapon out of its hand: the hooks weapons.qc and combat.qc call (vr_armedgrunt_shot.qc
// defines them; ROUND22.md, "Armed grunts: shooting the weapon out").
// ----------------------------------------------------------------------------

float QVR_AGS_PELLET = 0;    // a shotgun's pellet (FireBulletsImpl)
float QVR_AGS_NAIL = 1;      // a nail (its flight: VR_AGS_Frame)
float QVR_AGS_LIGHTNING = 2; // a lightning bolt (LightningDamage)
float QVR_AGS_BLAST = 3;     // a rocket's or a grenade's flight through it (its direct hit)
float QVR_AGS_SPLASH = 4;    // an explosion's damage to the grunt

// A player's (`xFrom`) hitscan path xA..xB (where it stopped), going `xDir`, of `xDamage`: what it passed through of a held
// weapon or its hand adds up. TRUE: it stopped on the weapon (vr_agrunt_shotdisarm_absorb: no damage beyond).
float(entity xFrom, vector xA, vector xB, vector xDir, float xDamage, float xKind) VR_AGS_Shot;

// T_DamageDeal, before VR_AG_Hit: an explosion's share (vr_agrunt_shotdisarm_w_splash). TRUE: a player's shot, which the
// shooting rule decides (VR_AG_Hit's blow rule skipped).
float(entity targ, entity inflictor, entity attacker, float take) VR_AGS_Hit;

// VR_AG_WeaponStruck (its weapon's think): the flights through it, its roll. TRUE: shot out of its hand.
float(entity g, entity p) VR_AGS_Frame;

// vr_agrunt_test 50..59 (vr_armedgrunt_shot_test.qc).
void(float t) VR_AGS_TestFrame;
