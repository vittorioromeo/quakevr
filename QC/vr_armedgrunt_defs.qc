//
//
//
// ----------------------------------------------------------------------------
// Armed grunts' fields and hooks (vr_armedgrunt.qc defines them; ai.qc, combat.qc and orig_mon_soldier.qc call them)
// ----------------------------------------------------------------------------

.float vr_ag;              // 1: an armed grunt (monster_vr_grunt; its classname is monster_army: a grunt to the rest)
.float vr_ag_kind;         // its row in the armed monsters' table (vr_armedmon.qc VR_AM_*: 0 the grunt)
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
// Dual wielding (vr_armedgrunt.qc VR_AG_Swap): the second slot, the same fields' (the layer acts on the first: a hand's
// event on the second's weapon swaps the slots first). Which hand a weapon is in is its prop's .vr_ag_left.
.float vr_ag_wid2;         // the second weapon (one-handed: VR_AG_OneHanded), or -1: none
.float vr_ag_wflags2;
.entity vr_ag_prop2;
.float vr_ag_reserve2;
.float vr_ag_left;         // a held weapon's prop: in its left hand
.vector vr_ag_was_o1;      // ... its place and turn a frame before (VR_AG_PropWas: the grab's sweep, the engine's handOn,
.vector vr_ag_was_a1;      //     vr_agrunt_grab_sweep: the drawn weapon lags the server's),
.vector vr_ag_was_o2;      // ... two frames before
.vector vr_ag_was_a2;
.float vr_ag_was_n;        // ... how many of them are known (0..2)
.float vr_ag_saw_fought;   // its chainsaw run while it was after you, alive (VR_AGP_Saw): dropped, it runs on (VR_AG_DropHeld)
.float vr_ag_dual;         // spawned with two: it goes for a second one-handed weapon when it lost one
.float vr_ag_act_hand;     // a dual swing's hands: 0 the right, 1 the left, 2 both
.float vr_ag_act_hitl;     // ... the left's blow struck
.float vr_ag_strikes_l;    // its left hand's blows (the tests)
// Dual-wielded guns (vr_armedgrunt.qc VR_AG_ShootProp): a shot's pellet spread times 1 + this while it fires (FireBullets,
// AD_FirePellets; 0 otherwise); every pellet's angle off its shot's line summed (the tests' spread measure).
float vr_fire_spread_extra;
float vr_fire_dev_sum;
float vr_fire_dev_n;

float(float wid) VR_AG_Class;                                              // vr_enemyshove.qc: fists and melee never shove
float VR_AG_CLASS_MELEE_DEF = 1;                                           // (vr_armedgrunt.qc VR_AG_CLASS_MELEE; fists 0)
float() VR_AG_CheckAttack;                                                 // ai.qc CheckAnyAttack: an armed grunt's
void() VR_AGAI_React;                                                      // ai.qc HuntTarget: its reaction time (vr_armedgrunt_ai.qc)
float vr_agai_reacts;                                                      // ... the times it ran (the tests)
float(float dist) VR_AG_RangeMove;                                         // ai.qc ai_run: its fighting distance kept (TRUE: moved)
float(entity targ, entity inflictor, entity attacker, float damage) VR_AG_DamageScale; // T_DamageDeal: its weapon's damage to players
void(entity targ, entity inflictor, entity attacker, float take) VR_AG_Hit; // T_DamageDeal: a hit on it (disarming)
float() VR_AG_SoldierDrop;                                                 // SoldierDrop: its own loot (TRUE: done)
float() VR_AG_Idle;                                                        // ai_stand, ai_walk: unarmed, off to a weapon (TRUE: gone)
float() VR_AG_Dying;                                                       // army_die: its weapon falls (TRUE: an armed grunt)
void(entity attacker, entity targ) VR_AG_Parried;                          // VR_Parry: its blow parried (a chance to disarm)
float(entity e) VR_AG_Striking;                                            // VR_Parry_MeleeMonster: its melee blow under way

// Parrying melee blows with its weapon (vr_armedgrunt_parry.qc; vr_agrunt_parry).
.float vr_agpar_w;         // the parry pose's share, eased
.vector vr_agpar_t;        // the blow coming: its point (body space, eased)
.float vr_agpar_since;     // when that blow was first seen (0: none)
.float vr_agpar_seen;      // ... last seen
.float vr_agpar_ready;     // ... when its parry rises (the reaction; -1: caught out, this blow not parried)
.entity vr_agpar_tby;      // ... whose
.float vr_agpar_load;      // stamina spent (parries' worth, recovering since vr_agpar_load_at)
.float vr_agpar_load_at;
.entity vr_agpar_by;       // its last parry: whose blow, when, the hand (+1; 0 a monster's)
.float vr_agpar_last;
.float vr_agpar_hand;
float vr_agpar_now;        // the blow being dealt was parried (T_Damage, T_Damage_VRMelee: no pain, no zone, no disarm by it)
float(entity targ, entity inflictor, entity attacker, float damage) VR_AGPar_Blow; // T_Damage, T_Damage_VRMelee
float(entity g) VR_AGPar_Guards;                                           // VR_Guard_Hand: it parries instead
void(entity g) VR_AGPar_Frame;                                             // VR_AG_PropThink: a blow coming
void(entity g, entity p) VR_AGPar_Pose;                                    // VR_AGP_Pose: the parry's pose
float(entity g) VR_AGPar_WeaponFrame;                                      // VR_AG_WeaponStruck: a blow on its weapon

// The physical tug (vr_armedgrunt_play.qc; vr_knockdown.qc keeps its stagger on while it lasts), a weapon on a ragdoll's
// limb (VR_AG_RagProp), a weapon taken up from the floor (vr_armedgrunt.qc VR_AG_TakeBegin).
.float vr_ag_tug_rag;      // the physical tug on: it stands as an active ragdoll, a hand holding its weapon
.float vr_ag_rb;           // a held weapon's prop on a ragdoll: the limb it rides (+1; 0: none yet)
.vector vr_ag_rbo;         // ... its origin, a point ahead and a point above it, in that limb's space
.vector vr_ag_rbf;
.vector vr_ag_rbu;
.vector vr_ag_pfw;         // a held weapon's prop: its forward and up as last posed
.vector vr_ag_pup;
.float vr_ag_took_at;      // a weapon taken up from the floor: its reach began (0: none),
.float vr_ag_took_grab;    // ... its hand closes on it then (it lies where it was until then)
.float vr_ag_took_done;    // ... its hand closed (counted: VR_AG_TakeGrab)
.vector vr_ag_took_org;    // ... where it lay, its forward and up there
.vector vr_ag_took_fw;
.vector vr_ag_took_up;
float vr_ag_grab_fast;     // grabs of its weapon refused as too fast (vr_agrunt_grab_speed; the tests)
void(entity g, entity p) VR_AG_RagProp;
void(entity g, float won, string why) VR_AG_TugEnd;

// Gameplay (vr_armedgrunt_play.qc: disarming, falls, reach), called from vr_armedgrunt.qc.
float(entity g, entity p) VR_AG_TugFrame;
float(entity g) VR_AG_FallDrop;
void(entity g, entity pl, float h) VR_AG_GrabBy;
float(entity g) VR_AG_DisarmDamage;
float(entity g, entity w) VR_AG_Shunned;
float(entity g, entity w) VR_AG_Reachable;
float(entity g) VR_AG_SeekCheck;
entity(entity g) VR_AG_OwnBack;
void(entity g) VR_AG_SeekStart;
float() VR_AG_TugResume;
void(entity g, float wid) VR_AG_DisarmFeedback;
void() VR_AG_PlayPrecache;
float(entity g, entity p) VR_AG_WeaponStruck;
// Armed humanoid replacements (vr_armedrepl.qc; vr_agrunt_repl): a map's grunts and enforcers as armed ones.
.float vr_agr;             // 1: a map's grunt or enforcer (or a spawner's) made its armed version (VR_AGR_Replace)
.float vr_ag_native;       // a held weapon's prop (or its laser): the monster's own gun, fired as the monster fires it
.float vr_ag_gunaxe;       // it holds a gun and an axe (vr_agrunt_repl_gunaxe): the gun, the axe up close
float vr_am_spawning;      // VR_AM_Spawn is running its monster's own spawn function (no replacement then)
float vr_agr_hold;         // > 0: spawns now are never replaced (the training dummies: vr_dummy_types.qc)
float vr_agr_native_fire;  // a native gun's bullets being fired (VR_AG_DamageScale: the monster's own damage)
float vr_agr_shots_grunt;  // grunts' shots (rounds; a shotgun blast one), the native guns' among them (the tests)
float vr_agr_shots_enf;    // enforcers' lasers, ...
float vr_agr_pl_hits;      // monsters' hits on players (VR_AG_DamageScale: the tests)
float vr_agr_pl_dmg;       // ... their damage
float vr_agr_pl_raw;       // ... their damage before vr_agrunt_damage_scale (vr_armedgrunt_ammo_test.qc)
float(float kind) VR_AGR_Replace;                                          // monster_army, monster_enforcer: TRUE: done
void() VR_AGR_Precache;                                                    // worldspawn: the replacements' models
void() VR_AGR_Frame;                                                       // StartFrame: loadouts rolled once a player is in
void(entity p) VR_AGR_NativeShot;                                          // VR_AG_ShootProp: a native gun's shot
float(entity g) VR_AGR_NativeAlone;                                        // its one weapon its own gun (its monster's attacks)
float(float r) VR_AGR_NativeChance;                                        // VR_AG_CheckAttack: its monster's chance at range r
void() VR_AGR_NativeFinished;                                              // ... and its pause after
void() VR_AGR_NativeAttack;                                                // VR_AG_Missile: its monster's attack frames
void(float d) VR_AGR_GunAxe;                                               // VR_AG_CheckAttack: the gun or the axe now
.string vr_agr_mdl;        // a held weapon's model while its holder waits unseen (MG_deferred: VR_AG_PropThink)
.float vr_agk_live; // run live by the Pose Studio (its real loops where it stands: vr_armedgrunt_pose.qc VR_AGK_Live)
// Held still (vr_agrunt_freeze: the Grip Editor, the Pose Studio; not the studio's Live subject): no AI, no tug, no blow.
float(entity g) VR_AG_Still = { return cvar("vr_agrunt_freeze") && g.health > 0 && !g.vr_agk_live; };

// Armour for armed humanoids (vr_agarmor.qc; ROUND23.md, "Armour for armed humanoids, P1"): the fields (the engine sends
// .vr_ar_type and .vr_ar_mask to the clients: vr_agarmor.cpp) and the hooks combat.qc, weapons.qc and player.qc call.
.float vr_ar_type;     // 0 none, 1 green, 2 yellow, 3 red
.float vr_ar_mask;     // the pieces still on (bits: 1 the cuirass, 2 the helmet, 4/8 pauldrons, 16/32 bracers, 64/128 thighs, 256/512 shins)
.float vr_ar_hp[10];   // each piece's pool left
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) VR_AgArmor_Damage; // T_DamageDeal: the damage left
void(entity targ) VR_AgArmor_Guard;      // a cut or a pop armed at a limb or the head under an unbroken plate: off
void() VR_AgArmor_PelletsClear;          // ClearMultiDamagePellets
void(entity hit) VR_AgArmor_PelletAdd;   // AddMultiDamage: the pellet's piece counted
void(entity e) VR_AgArmor_Shed;          // gibbed (ThrowHead): every piece left falls as debris
void(entity e) VR_AgArmor_Spawn;         // VR_AM_Spawn: an armed grunt's armour rolled by the skill
