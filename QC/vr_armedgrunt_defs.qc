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
.float vr_ag_refire_at;    // (a held weapon) its next shot no sooner (VR_AG_RefireReady: the player's interval)
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
.float vr_ag_ff_until;     // any monster: stepping aside (a friend in its line of fire) until then (VR_AI_FriendCare; VR_AG_Run, ai_run)
.float vr_ag_ff_since;     // ... held for friends since then (0: its line clear)
.float vr_ag_ff_last;      // ... the last time it held
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
// The weapon classes and pose numbers (VR_AG_CLASS_*, VR_AGK_*), shared with the engine:
#include "../Quake/vr/vr_agpose_shared.h"
float() VR_AG_CheckAttack;                                                 // ai.qc CheckAnyAttack: an armed grunt's
void() VR_AGAI_React;                                                      // ai.qc HuntTarget: its reaction time (vr_armedgrunt_ai.qc)
float vr_agai_reacts;                                                      // ... the times it ran (the tests)
float(float dist) VR_AG_RangeMove;                                         // ai.qc ai_run: its fighting distance kept (TRUE: moved)
float() VR_AG_Speed;                                                       // ai.qc ai_run: its steps by its speed sliders (vr_armedgrunt_ai.qc)
float(float yaw) VR_AG_StepDir;                                            // ... a step's backpedal and strafe factor
float(entity targ, entity inflictor, entity attacker, float damage) VR_AG_DamageScale; // T_DamageDeal: its weapon's damage to players
void(entity targ, entity inflictor, entity attacker, float take) VR_AG_Hit; // T_DamageDeal: a hit on it (disarming)
float() VR_AG_SoldierDrop;                                                 // SoldierDrop: its own loot (TRUE: done)
float() VR_AG_Idle;                                                        // ai_stand, ai_walk: unarmed, off to a weapon (TRUE: gone)
float() VR_AG_Dying;                                                       // army_die: its weapon falls (TRUE: an armed grunt)
void(entity attacker, entity targ) VR_AG_Parried;                          // VR_Parry: its blow parried (a chance to disarm)
float(entity e) VR_AG_Striking;                                            // VR_Parry_MeleeMonster: its melee blow under way

// Debug > Show Disarm Volumes (vr_disarmviz.qc): the disarm tests mark their hits (guarded by vr_dv_on: nothing built
// while it is off).
float vr_dv_on;          // vr_debug_disarm this frame (VR_DV_Frame)
float VR_DV_PARRY = 1;   // a parry on its weapon
float VR_DV_DISARM = 2;  // knocked out of its hand
float VR_DV_BODY = 3;    // a blow on its body that didn't disarm
float VR_DV_SHOT = 4;    // a shot counted on its weapon or hand
float VR_DV_BLOCKED = 5; // a pellet or nail its weapon stopped
void(entity e, vector at, float what, string why) VR_DV_Mark;

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
float vr_agbc_stopped;     // ... stopped on its blade (VR_AG_BladeContact: T_Damage_VRMelee deals nothing)
.entity vr_agbc_by;        // the player whose blow its blade met last (VR_AG_BladeContact), and
.vector vr_agbc_mark;      // ... [that hand + 1, when]: that motion's later blows on it stop there too
float(entity targ, entity inflictor, entity attacker, float damage) VR_AGPar_Blow; // T_Damage, T_Damage_VRMelee
float(entity g) VR_AGPar_Guards;                                           // VR_Guard_Hand: it parries instead
// Directional parry (vr_dirparry.qc; vr_dirparry, experimental): your weapon held across an armed humanoid's swing.
.vector vr_drp_a0;         // (a held weapon) its drawn line's two ends last frame, and when (0: none)
.vector vr_drp_b0;
.float vr_drp_t0;
.float vr_drp_swing;       // (an armed humanoid) the swing (its vr_ag_act_at) a directional parry stopped (0: none)
entity vr_drp_w;           // the weapon whose blow VR_AGP_Strike deals now (world: none)
float(entity g) VR_DRP_Applies;                                            // VR_Parry: its blow is judged by the direction
float(entity g) VR_DRP_Done;                                               // VR_Parry: this swing was stopped already
float(entity pl, entity g, float damage) VR_DRP_Blow;                      // VR_Parry: the parrying hand (-1: none)
float vr_drp_deferred;     // VR_DRP_Blow held the blow back (VR_Parry deals nothing now)
void(entity pl, entity g, float h) VR_DRP_Effects;                         // VR_Parry: sparks and the clang at the meeting
void(entity g, entity w) VR_DRP_Frame;                                     // VR_AGP_Pose: its swing met your weapon
float vr_drp_cut;          // VR_DRP_Blow's partial parry: the share of the blow taken off (0: none)
void(entity g) VR_AGPar_Frame;                                             // VR_AG_PropThink: a blow coming
void(entity g, entity p) VR_AGPar_Pose;                                    // VR_AGP_Pose: the parry's pose
vector vr_agpar_mid;       // ... its weapon's middle's target (body space), VR_AGPar_Pose's this pose
float vr_agpar_mw;         // ... its share then (0: no parry pose this pose)
vector(entity g, entity p) VR_AGPar_Meet;                                  // VR_AGP_Pose: its weapon put on that target
float(entity g) VR_AGPar_WeaponFrame;                                      // VR_AG_WeaponStruck: a blow on its weapon
// Its guard (vr_armedgrunt_guard.qc: the preventive parry, vr_agrunt_preparry, and the wind-up parry,
// vr_agrunt_windup_parry): raised for a while before any blow, then dropped (a recovery: the opening).
.float vr_agg_state;       // VR_AGG_*: 0 idle, 1 reacting (raised at vr_agg_until), 2 raised, 3 recovering
.float vr_agg_kind;        // what raised it: 1 you up close (preventive), 2 your wind-up
.float vr_agg_until;       // the state's end
.float vr_agg_from;        // the guard raised then
.float vr_agg_next;        // the preventive parry's next check
.entity vr_agg_pl;         // the player it guards against
.float vr_agg_hand;        // ... the hand it watches (+1; 0 none)
.float vr_agg_wup0;        // your hands' raise first seen (0: not raised); a raise read (needs lowering: rearm)
.float vr_agg_wup1;
.float vr_agg_wread0;
.float vr_agg_wread1;
float(entity g) VR_AGG_Frame;                                              // VR_AGPar_Frame: its guard's state
float(entity g) VR_AGG_HoldFire;                                           // VR_AG_CheckAttack: guarding, no attack
float(entity pl, float h) VR_AGG_Cat;                                      // what your hand holds (the categories)
float(float cat, float k) VR_AGG_Mult;                                     // ... its multiplier
float(float c, float m) VR_AGG_Odds;                                       // a chance scaled, its ends kept
string(entity g) VR_AGG_Label;                                             // Show Disarm Volumes: its guard's state
// A deliberate strike on its weapon or hand (vr_agrunt_strike_first; vr_armedgrunt_parry.qc):
float(entity g, entity p, entity pl, float h) VR_AG_StrikeMeets;           // VR_AG_HandStrikeSpeed: the swept test
float(entity g, entity p) VR_AG_StrikeFrame;                               // VR_AG_WeaponStruck: strikes, then parries
float(entity g, entity p) VR_AG_TugStrike;                                 // VR_AG_TugFrame: the other fist's strike in a tug
float(entity g, entity e, entity by) VR_AG_ThrowOnWeapon;                             // VR_AG_Hit: a throw that touched its body
float(entity g, entity p, vector at) VR_AG_HandLineDist;                   // VR_AG_Hit: a body blow by its hand
float(entity p) VR_AG_DrawnFrame;                                          // VR_AG_Hit: its grip as drawn (vr_armedgrunt_play.qc)
vector vr_ag_df_o;                                                         // ... VR_AG_DrawnFrame's: the weapon's origin as drawn

// The tug (vr_armedgrunt_play.qc; vr_knockdown.qc keeps its stagger on while it lasts), a weapon on a ragdoll's
// limb (VR_AG_RagProp), a weapon taken up from the floor (vr_armedgrunt.qc VR_AG_TakeBegin).
.entity vr_ag_tug_pl;      // the player tugging at its weapon (world: none)
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
void(entity g, entity pl, float h) VR_AG_RagWeaponGrab; // VR_Ragdoll_Handtouch: the grab query gave the hand to its weapon
float vr_ag_raggrabs; // the ragdoll touches handed to the weapon's grab (the tests)
void(entity g, float won, string why) VR_AG_TugEnd;
void(entity g, entity by, vector dir, float speed, string why) VR_AG_DisarmBy; // (vr_armedgrunt.qc)
float(entity g, string why) VR_AG_TugDisarm; // VR_AG_Disarm in a tug: into the holding hand (TRUE)

// Gameplay (vr_armedgrunt_play.qc: disarming, falls, reach), called from vr_armedgrunt.qc.
float(entity g, entity p) VR_AG_TugFrame;
float(entity g) VR_AG_FallDrop;
void(entity m, float xSpeed) VR_AG_FallLanded; // VR_Monster_Fall: a landing that hurt it lets go of its weapon
void(entity g, entity pl, float h) VR_AG_GrabBy;
void(entity p, entity pl, float h) VR_AG_HandGrab; // (vr_armedgrunt.qc: a hand grabbing its weapon, refused when busy)
float(entity g) VR_AG_DisarmDamage;
float(entity g, entity w) VR_AG_Shunned;
float(entity g, entity w) VR_AG_Reachable;
float(entity g) VR_AG_SeekCheck;
entity(entity g) VR_AG_OwnBack;
void(entity g) VR_AG_SeekStart;
void(entity g, float wid) VR_AG_DisarmFeedback;
// A try at its weapon (a blow, a shot, a throw on its weapon or the hand holding it) by `by`, its outcome one of these:
float VR_AG_TRY_MET = 1;      // ... met it: counts towards a roll (a shot's damage adding up)
float VR_AG_TRY_HELD = 2;     // ... it held on (a shot's roll lost, a blow on its blade, a throw too light)
float VR_AG_TRY_PARRIED = 3;  // ... its parry took it
float VR_AG_TRY_DISARMED = 4; // ... knocked out of its hand (VR_AG_DisarmBy)
void(entity g, entity by, float outcome) VR_AG_DisarmTry;
.float vr_ag_struck_at; // a blow (any damage: VR_AG_Hit) or a try at its weapon (VR_AG_DisarmTry) on it lately: lying, its
                        // body's knock then is the blow's, not the ground's (VR_AG_FallImpact)
void() VR_AG_PlayPrecache;
float(entity g, entity p) VR_AG_WeaponStruck;
// Armed humanoid replacements (vr_armedrepl.qc; vr_agrunt_repl): a map's grunts and enforcers as armed ones.
.float vr_agr;             // 1: a map's grunt or enforcer (or a spawner's) made its armed version (VR_AGR_Replace)
.float vr_ag_native;       // a held weapon's prop (or its laser): the monster's own gun, fired as the monster fires it
.float vr_ag_gunaxe;       // it holds a gun and an axe (vr_agrunt_repl_gunaxe): the gun, the axe up close
float vr_am_spawning;      // VR_AM_Spawn is running its monster's own spawn function (no replacement then)
float vr_agr_hold;         // > 0: spawns now are never replaced (the training dummies: vr_dummy_types.qc; the tools: VR_Test_Spawn, the engine's progs::ToolSpawn)
float vr_agr_native_fire;  // a native gun's bullets being fired (VR_AG_DamageScale: the monster's own damage)
vector vr_agr_native_from; // ... from this muzzle (a gun in each hand: its own; '0 0 0' the monster's own place, EnemyFireBullets)
float vr_agr_shots_grunt;  // grunts' shots (rounds; a shotgun blast one), the native guns' among them (the tests)
float vr_agr_shots_enf;    // enforcers' lasers, ...
float vr_agr_pl_hits;      // monsters' hits on players (VR_AG_DamageScale: the tests)
float vr_agr_pl_dmg;       // ... their damage
float vr_agr_pl_raw;       // ... their damage before vr_agrunt_damage_scale (vr_armedgrunt_ammo_test.qc)
float(float kind) VR_AGR_Replace;                                          // monster_army, monster_enforcer: TRUE: done
float() VR_AKN_Replace;                                                    // monster_knight: its armed version (vr_agknight): TRUE: done
void() VR_AGR_Precache;                                                    // worldspawn: the replacements' models
void() VR_AGR_Frame;                                                       // StartFrame: loadouts rolled once a player is in
void(entity p) VR_AGR_NativeShot;                                          // VR_AG_ShootProp: a native gun's shot
float(entity g) VR_AGR_NativeAlone;                                        // its one weapon its own gun (its monster's attacks)
float(float r) VR_AGR_NativeChance;                                        // VR_AG_CheckAttack: its monster's chance at range r
void() VR_AGR_NativeFinished;                                              // ... and its pause after
void() VR_AGR_NativeAttack;                                                // VR_AG_Missile: its monster's attack frames
float(entity g, float d) VR_AGR_AxeNear;
void(float d) VR_AGR_GunAxe;                                               // VR_AG_CheckAttack: the gun or the axe now
.float vr_ag_born_ranged; // it spawned holding a ranged weapon (VR_AM_Spawn): takes ranged weapons up whatever its row says (VR_AM_Takes)
.string vr_agr_mdl;        // a held weapon's model while its holder waits unseen (MG_deferred: VR_AG_PropThink)
.float vr_agk_live; // run live by the Pose Studio (its real loops where it stands: vr_armedgrunt_pose.qc VR_AGK_Live)
// Held still (vr_agrunt_freeze: the Grip Editor, the Pose Studio; not the studio's Live subject): no AI, no tug, no blow.
float(entity g) VR_AG_Still = { return cvar_hget(cvarh_vr_agrunt_freeze) && g.health > 0 && !g.vr_agk_live; };

// Armour for armed humanoids (vr_agarmor.qc; ROUND23.md, "Armour for armed humanoids, P1"): the fields (the engine sends
// .vr_ar_type and .vr_ar_mask to the clients: vr_agarmor.cpp) and the hooks combat.qc, weapons.qc and player.qc call.
.float vr_ar_type;     // 0 none, 1 green, 2 yellow, 3 red
.float vr_ar_mask;     // the pieces still on (bits: 1 the cuirass, 2 the helmet, 4/8 pauldrons, 16/32 bracers, 64/128 thighs, 256/512 shins)
.float vr_ar_hp[10];   // each piece's pool left
.float vr_ar_wear;     // the damage marks (P2; sent with the rest): bits 0-9 the pieces at two thirds of their pool or under, 10-19 at a third or under
void(entity e, float armour, float pieces) VR_AgArmor_Toolgun; // the toolgun's Armed Humanoid page's armour on what it spawned
float(entity targ, entity inflictor, entity attacker, float damage, float isVRMelee) VR_AgArmor_Damage; // T_DamageDeal: the damage left
float(entity targ, vector at, vector dir) VR_AgArmor_PushScale; // VR_ShotPush: a shot a plate meets pushes by vr_agarmor_impulse
void(entity targ) VR_AgArmor_Guard;      // a cut or a pop armed at a limb or the head under an unbroken plate: off
void() VR_AgArmor_PelletsClear;          // ClearMultiDamagePellets
void(entity hit, float damage) VR_AgArmor_PelletAdd; // AddMultiDamage: the pellet's damage counted on its piece
void(entity targ, vector at, vector dir) VR_AgArmor_Shot; // PositionalDamage: the plate a shot's line met first
void(entity e, float dm) VR_AgArmor_Shed; // gibbed (ThrowHead, VR_Corpse_Gib): every piece left flies off as debris
void(entity e) VR_AgArmor_Spawn;         // VR_AM_Spawn: an armed grunt's armour rolled by the skill
float(entity targ, vector org, vector vel, float damage) VR_AgArmor_HitFx; // a hit's sparks on a plate; the blood's share returned
void(entity e, float takes) VR_AgArmor_CutPrepare;        // VR_Limb_Cut, VR_Decap_Cut: the plates on the bones a cut takes
void(entity e, float done, vector vel) VR_AgArmor_CutDone; // and after it: those plates dislodged as debris
float(entity e, float joint) VR_AgArmor_Holds;            // VR_Limb_WearInto: a corpse's piece kept on by an unbroken plate
void(entity h) VR_AgArmor_DebrisGrabbed;                  // VR_Carry_Start: a plate's debris taken, its fade put off

#define VR_WP_RIP_SOUND_FILE "vr/pack_rip.wav"
// Worn backpacks (vr_wornpack.qc; ROUND23.md, "Worn backpacks"; armed ones with a ranged weapon): the fields and the hooks.
.float vr_wp_worn;    // a wearer's: 1 its pack on its back (the engine reads it: its ragdoll made with the pack on its chest)
.entity vr_wp_pack;   // a wearer's pack (on it or torn off; world: none)
.float vr_wp_had;     // a wearer's: it was given one (it drops no backpack as it dies)
.float vr_wp_state;   // a pack's: 1 worn by the living, 2 worn by the dead (takeable), 3 off
.entity vr_wp_wearer; // a pack's wearer
.float vr_wp_peak;    // a pack's: the hardest pull on it so far (N; the tests)
float(entity e) VR_WP_Packless;                                          // VR_AG_SoldierDrop: left without a pack by the rule: no backpack
void(entity e) VR_WP_Refit;                                              // VR_AGR_Resolve: its loadout changed unseen
float(entity e) VR_WP_CarriesRanged;
void(entity e) VR_WP_Spawn;                                              // VR_AM_Spawn: an armed grunt's pack put on
void(entity targ, entity inflictor, entity attacker, float take) VR_WP_Hit; // T_Damage: a hit on the pack may tear it off
void(entity m, float xSpeed, vector xInto) VR_AG_FallTouch;                // VR_RagImpact_Hit: its body met the level (VR_AG_FallImpact)
#define VR_AG_FALL_RING 4                                                  // velocities the impact rule keeps (VR_AG_FallImpact)
void(entity m, vector xAt, float xSpeed) VR_WP_Impact;                    // VR_RagImpact_Hit: a hard landing tears it off
void(entity e) VR_WP_Taken;                                              // VR_Carry_Start, VR_Forcegrab_Launch: a dead one's pack taken
float(entity m, entity pl, float hand) VR_WP_HandOnPack;                  // VR_Ragdoll_Handtouch: the hand on a dead one's pack takes the pack
float() VR_WP_DeathLoot;                                                 // VR_AG_SoldierDrop: its loot in its pack (TRUE: no backpack dropped)
