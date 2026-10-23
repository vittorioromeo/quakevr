//
//
//
// ----------------------------------------------------------------------------
// Armed grunts' fields and hooks (vr_armedgrunt.qc defines them; ai.qc, combat.qc and orig_mon_soldier.qc call them)
// ----------------------------------------------------------------------------

.float vr_ag;              // 1: an armed grunt (monster_vr_grunt; its classname is monster_army: a grunt to the rest)
.float vr_ag_wid;          // the weapon it holds (WID_*), or -1: none (its fists)
.float vr_ag_wflags;       // that weapon's flags
.entity vr_ag_prop;        // the weapon as it holds it (classname vr_ag_weapon: its record in .weaponinst), or world
.float vr_ag_reserve;      // its rounds beyond the clip in the gun (vr_agrunt_reserve clips at its spawn)
.float vr_ag_kick;         // the held weapon's recoil, 1 at a shot, dying out (drawn: the prop)
.float vr_ag_kick_at;      // when it last fired
.float vr_ag_swing_at;     // when its last swing (a melee blow) began
.float vr_ag_seek_next;    // disarmed: when it next looks for a weapon
.entity vr_ag_seek;        // the weapon lying about it goes for (world: none)
.float vr_ag_shots;        // the shots of this attack so far
.float vr_ag_total_shots;  // ... of all its attacks (the tests)
.float vr_ag_strikes;      // its melee blows (the tests)
.float vr_ag_reloads;      // reloads (the tests)
.float vr_ag_disarms;      // times it was disarmed (the tests)
.float vr_ag_pickups;      // weapons it picked up (the tests)

float(float wid) VR_AG_Class;                                              // vr_enemyshove.qc: fists and melee never shove
float VR_AG_CLASS_MELEE_DEF = 1;                                           // (vr_armedgrunt.qc VR_AG_CLASS_MELEE; fists 0)
float() VR_AG_CheckAttack;                                                 // ai.qc CheckAnyAttack: an armed grunt's
float(entity targ, entity attacker, float damage) VR_AG_DamageScale;       // T_DamageDeal: its weapon's damage to players
void(entity targ, entity inflictor, entity attacker, float take) VR_AG_Hit; // T_DamageDeal: a hit on it (disarming)
float() VR_AG_SoldierDrop;                                                 // SoldierDrop: its own loot (TRUE: done)
float() VR_AG_Idle;                                                        // ai_stand, ai_walk: unarmed, off to a weapon (TRUE: gone)
