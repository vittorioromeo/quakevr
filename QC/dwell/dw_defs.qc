// dw_defs.qc -- Dwell support (DWELL.md P2a): the early declarations of Dwell's own entities (QC/dwell/dw_*.qc, after
// Copper's import in progs.src), read by our shared code before them (combat.qc, weapons.qc, items.qc, ai.qc, client.qc).
//
// Copyright (C) the Dwell team (2023: Fairweather, Khreathor, bmFbr, Shamblernaut, VoidForce and the others), whose
// entities these reimplement; Quake VR's reimplementation on Copper (Lunaran, GPL-2) and Quake VR's own code. This
// program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License
// version 2 or later. Dwell's models and sounds are read from the player's install, never shipped.

// A monster's own refusal to die (Killed, combat.qc): TRUE keeps it alive (a Dwell boss's phase ends instead).
.float() th_survive;
// A monster's own attack choice (CheckAnyAttack, ai.qc), as id's per-class ones; 0: CheckAttack.
.float() vr_th_checkattack;

// A Dwell monster's animation sequence (dw_common.qc DW_Seq): its frames, its AI each frame and how far, its event (each
// frame, by self.frame) and what follows the last frame (0: it loops).
.float dw_f0, dw_f1, dw_ai, dw_dist, dw_seqid;
.void() dw_ev;
.void() dw_after;
.float dw_mon; // a Dwell monster (DW_MON_*: its kind)
.float dw_marked; // the Sculptor marked by trigger_killboss: it dies next time

// Dwell's powerups on the player (dw_items.qc): when each ends.
.float dw_haste_finished, dw_berserk_finished, dw_wetsuit_finished;
.float dw_warned; // (the "wearing off" lines said: DW_PW_* bits)
.float vr_dw_hud; // their seconds left for the wrist gadget (the engine's STAT_QVR_DWPOWER: 8 bits each)
// An armed grunt's shot with a Dwell weapon (vr_armedgrunt.qc VR_AG_ShootProp: DW_FireVariant as yours): the prop the
// Lance's beam is drawn from (world: the player's hand).
entity dw_beam_prop;
float dw_variant_shots; // Dwell's shots fired, anyone's (vr_ad_test 204)
.float vr_dw_tinted; // Haste's or Berserk's screen tint set (AD's tint, VR_AD_Tint): cleared when they end

// Dwell's weapons (DWELL.md P3): a variant of one of our weapons, on its weapon instance (.wi_dwvar, vr_weaponinst.qc:
// the Rotary Shotgun a super shotgun, the Crystal Lance a lightning gun), set by its pickup. 3 was Dwell's Rifle
// (removed 2026-11-23, his note: no Dwell map places weapon_rifle, a cheat impulse only); a record still holding 3 (an
// older save) is a plain shotgun: every reader asks DW_VariantFor(its weapon) first.
.float vr_dw_var;  // a pickup's: the variant it gives
const float DW_VAR_ROTARY = 1;
const float DW_VAR_LANCE = 2;
nosave float vr_grab_dwvar; // VR_WeaponGrabbable_Make: the variant the placed weapon's record gets (the toolgun's props)

float(entity targ, entity attacker, float damage, float isVRMelee) DW_DamageScale; // T_DamageImpl: berserk
void(entity xPickup, entity p, float xHand) DW_VariantGiven;                      // weapon_touch
void(entity xPickup, entity p) DW_PickupOwned;                                    // ...: its variant owned
float(entity xPickup, entity p) DW_PickupNewVariant;                              // ...: coop's "has it" skip
float(float xHand, float* xOutAttackFinished) DW_FireVariant;                     // W_Fire_*: TRUE fired
float(float xHand) DW_LanceInHand;                                                // FrameDrawLightningTrail: no stream
float(float xHand) DW_LanceCharged;                                               // W_WeaponFrameImpl: charged shots
float(float xHand, float xPressed) DW_LanceChargeFrame;                           // ...: TRUE fire now
// The Crystal Lance's charge (lancecharge, dw_items.qc): vr_ad_test 224 holds the trigger itself (dw_lc_test: its
// press in dw_lc_test_press, every hand); the charges begun, let go short and fired (the tests).
float dw_lc_test, dw_lc_test_press;
float dw_lc_starts, dw_lc_cancels, dw_lc_fires;
float(float xWid) DW_VariantFor;                                                  // VR_AG_ShootProp
string(float xVar) DW_VariantName;                                                // VR_AG_NameOf
string(float xWid, entity xInst) DW_WeaponModel;                                  // AD_WeaponModel: its VR model or ""
string(float xWid) DW_VRModelOf;                                                  // AD_SyncModel
void() DW_PowerupFrame;                                                           // CheckPowerups
float(entity p) DW_PackWeaponVariants;                                            // SetChangeParms: parm76
void(entity p, float xPacked) DW_UnpackWeaponVariants;                            // DecodeLevelParms
void() dw_monster_kamikaze; // the debug spawner's 70..73 (buttons.qc VR_EnemyDispenser_Spawn; DWELL.md P2b)
void() dw_monster_brute;
void() dw_monster_excavator;
void() dw_monster_d1boss;
