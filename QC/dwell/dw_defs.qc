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

// Dwell's weapons (placeholders until P3, DWELL.md): a variant of one of our weapons, on its weapon instance (the
// Rotary Shotgun a super shotgun, the Crystal Lance a lightning gun, the Rifle a shotgun), set by its pickup.
.float wi_dwvar;   // the weapon instance's variant (DW_VAR_*)
.float vr_dw_var;  // a pickup's: the variant it gives
const float DW_VAR_ROTARY = 1;
const float DW_VAR_LANCE = 2;
const float DW_VAR_RIFLE = 3;

float(entity targ, entity attacker, float damage, float isVRMelee) DW_DamageScale; // T_DamageImpl: berserk
void(entity xPickup, entity p, float xHand) DW_VariantGiven;                      // weapon_touch
float(float xHand, float* xOutAttackFinished) DW_FireVariant;                     // W_Fire_*: TRUE fired
void() DW_PowerupFrame;                                                           // CheckPowerups
float(entity e) DW_PositionalHead;                                                // PositionalHead
