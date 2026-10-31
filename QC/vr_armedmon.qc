//
//
//
// ----------------------------------------------------------------------------
// Armed monsters: the table (docs/vr-port/ARMED_MONSTERS.md; ROUND22.md, "Armed monsters")
// ----------------------------------------------------------------------------
//
// The armed grunts' code (vr_armedgrunt.qc: the pool, picking, holding and drawing the prop, firing through your
// weapons' code, melee, ammo, reloads, drops, disarming, re-arming; vr_armedgrunt_pose.qc: the pose and its attacks;
// vr_armedgrunt_play.qc: tugs, falls, reach) knows nothing of the monster that holds the weapon: what it needs of it is
// its row here (`.vr_ag_kind`: VR_AM_Row fills the vr_am_* globals). The monster keeps its own classname, health,
// sounds, pains, deaths, ragdoll and every system keyed on it: it is that monster to the rest of the game.
//
// Adding one: a row (VR_AM_Row; its back vertices too: a flamethrower's worn tank, vr_adtankworn.qc), its stripped
// model (Misc/quakevr/make_vrgrunt.py), its spawn function
// (monster_vr_<name> = VR_AM_Spawn(kind)), the hooks in its own code (its death: VR_AG_Dying before its gun falls; its
// loot: VR_AG_SoldierDrop; CheckAnyAttack: VR_AG_CheckAttack for any .vr_ag), the engine's row (vr_hitreact.cpp
// armedRigs: its hands' IK) and its model id (vr_modelmetadata.inc, the ragdoll's seeds in vr_ragdoll.cpp).

float VR_AM_GRUNT = 0;    // monster_vr_grunt: a grunt (monster_army)
float VR_AM_ENFORCER = 1; // monster_vr_enforcer: an enforcer (monster_enforcer)
float VR_AM_KNIGHT = 2;   // monster_vr_knight: a knight (monster_knight) with a sword (vr_agknight: vr_armedknight.qc)
float VR_AM_DKNIGHT = 3;  // monster_vr_hknight: a death knight (monster_hell_knight) with his sword and his spell (vr_agdknight)
float VR_AM_COUNT = 4;
float VR_AM_GUN_KINDS = 2; // (the kinds before it: the gun holders, the replacements' and the pose categories')

// Weapon classes a row may hold (vr_am_classes; VR_AG_CLASS_* bits: 1 << class).
float VR_AM_CLASSES_ALL = 63;

// The row (VR_AM_Row):
float vr_am_kind;           // the row filled
string vr_am_name;          // "Grunt"
string vr_am_spawnname;     // its spawn function's name (the toolgun's spawn page makes it by this: monster_vr_grunt)
string vr_am_classname;     // its classname as spawned: the monster it is to the rest of the game
string vr_am_model;         // its model without the gun (make_vrgrunt.py)
var void() vr_am_spawn;     // the monster's own spawn function (its health, sounds, th_* the layer doesn't replace)
vector vr_am_hand;          // its right hand's vertices (three: their middle holds the weapon's grip)
vector vr_am_gun_rear;      // its hidden gun's vertices: two at its rear (x, y),
vector vr_am_gun_muzzle;    // ... two at its muzzle (x, y),
vector vr_am_gun_roll;      // ... its top and its bottom (x, y): the gun's line and roll in every frame
float vr_am_f_stand;        // its stand frames (the punches' and swings' legs): the first,
float vr_am_n_stand;        // ... how many
float vr_am_f_run;          // its run frames (8): the first
float vr_am_rund[8];        // ... each frame's step (its own run's)
float vr_am_f_atk;          // its aiming frames (9: the shots in the 4th..8th, one shot in the 5th): the first
float vr_am_atk_stand;      // 1: with vr_agarms and vr_agrunt_atk_stand its aiming frames drawn as its stand frames (the enforcer's crouch and lean back for a rifle at the hip: its posed arms aim)
float vr_am_f_shove;        // its shove frames (6: a gun's butt): the first
float vr_am_f_reload;       // its reload frames (11 drawn, these repeating): the first,
float vr_am_n_reload;       // ... how many
float vr_am_f_take;         // taking up a weapon: bending down from this frame,
float vr_am_n_take;         // ... this many frames, its lowest last (its hand closes there), and back up
string vr_am_pain1;         // its pain sounds (a tug, a disarm)
string vr_am_pain2;
float vr_am_loot_aid;       // its backpack with no reserve: this ammo (AID_*),
float vr_am_loot_n;         // ... this many
float vr_am_classes;        // the weapon classes it may hold (VR_AM_CLASSES_ALL: any; others: its fists)
float vr_am_takes_ranged;   // it takes up ranged weapons lying about (VR_AM_Takes; else melee ones only: spawned with a gun it holds it)
vector vr_am_fit_scale;     // the poses' body space (vr_armedgrunt_pose.qc, made on the grunt) to its: scaled,
vector vr_am_fit_shift;     // ... then moved (its shoulders against the grunt's)
vector vr_am_back_lo;       // its back's vertices (two, x y: their middle) low (the waist) and high (the shoulder blades):
vector vr_am_back_hi;       // ... a flamethrower's or poison sprayer's tank worn there, upright along them (vr_adtankworn.qc)
// vr_agarms (vr_armedgrunt_pose.qc VR_AGP_Arms; the engine's vr_agarms.cpp, its rig as vr_agarms_info prints it):
float vr_am_anchor;         // its chest's vertex its poses are placed from (the engine's armedRigs row's too),
vector vr_am_anchor_at;     // ... where it is at rest (its body's space: x ahead, y its left, z up, from its origin)
vector vr_am_sh_r;          // its shoulders at rest
vector vr_am_sh_l;
float vr_am_arm;            // ... an arm from its shoulder to its wrist (units)
vector vr_am_grip_r;        // its hands' grips at rest (on its own gun: its rest pose's hold, the wrists as the artist made them)
vector vr_am_grip_l;
vector vr_am_gun_fw;        // ... its own gun's way at rest
// Its fighting distance (vr_armedgrunt.qc VR_AG_Ranges, vr_agrunt_range):
float vr_am_range;          // its weapons' distances times this (under 1: it holds its ground nearer),
float vr_am_range_threat;   // ... its least and best times this while you hold a melee weapon or charge it
// Its own spell beside its weapon (the death knight's fireball volley): its monster's ranged attack, cast at range while
// it holds a melee weapon or none (VR_AG_CheckAttack: VR_AM_SpellTry); a gun's holder uses the gun.
var void() vr_am_spell;     // its frames' first function (SUB_Null: none)

void(float k) VR_AM_Row =
{
    vr_am_kind = k;
    vr_am_spell = SUB_Null;
    if(k == VR_AM_DKNIGHT)
    {
        // Quake VR's hknight.mdl: his sword's blade and guard 42..58, 332..365, 525..532 (vr_monstermods.cpp knownSwords),
        // its pommel 55/56, its tip 46/45, its guard's ends 351/342; his fist on its grip (79, 67, 75: the stillest in the
        // sword's space over his living frames, 2.2-2.6 units rms); ROUND23.md, "Death knights as armed humanoids". Every
        // weapon class, as the knight's; his fireball volley kept (vr_am_spell: his magicc frames, his shots, his sounds).
        vr_am_name = "Death Knight";
        vr_am_spawnname = "monster_vr_hknight";
        vr_am_classname = "monster_hell_knight";
        vr_am_model = "progs/vrhknight.mdl";
        vr_am_spawn = monster_hell_knight;
        vr_am_hand = '79 67 75';
        vr_am_gun_rear = '55 56 0';
        vr_am_gun_muzzle = '46 45 0';
        vr_am_gun_roll = '351 342 0';
        vr_am_f_stand = 0; vr_am_n_stand = 9; // stand1..9
        vr_am_f_run = 29; // run1..8
        vr_am_rund[0] = 20; vr_am_rund[1] = 25; vr_am_rund[2] = 18; vr_am_rund[3] = 16;
        vr_am_rund[4] = 14; vr_am_rund[5] = 25; vr_am_rund[6] = 21; vr_am_rund[7] = 13;
        vr_am_f_atk = 0;     // (no aiming frames: its stand, its posed arms aim; the grunt's shot timing)
        vr_am_atk_stand = 1;
        vr_am_f_shove = 112; // slice1..6 (a gun's butt: his slash's lunge under his posed arms)
        vr_am_f_reload = 0;  // (no reloads: its stand)
        vr_am_n_reload = 9;
        vr_am_f_take = 43;   // death2..6: down on his knees (no kneel of his own: his top 36, 31, 26, 22, 20 units; his
        vr_am_n_take = 5;    // hand 12 units over the floor at the last)
        vr_am_pain1 = "hknight/pain1.wav";
        vr_am_pain2 = "hknight/pain1.wav";
        vr_am_loot_aid = AID_SHELLS;
        vr_am_loot_n = 0;
        vr_am_classes = VR_AM_CLASSES_ALL;
        vr_am_takes_ranged = cvar_hget(cvarh_vr_agknight_ranged_pickup); // (the knights' one setting: 0, a blade)
        vr_am_back_lo = '105 106 0'; // (his back at the belt, x -5.3, z 11.8)
        vr_am_back_hi = '327 330 0'; // (... at the shoulder blades, x -4, z 20)
        vr_am_anchor = 74;
        vr_am_anchor_at = '9 -0.4 20.6';
        // The authored rig's shoulders (quakevr/rigs/vrhknight.rig2.txt: his upper arms' joints, under the pauldrons);
        // his right arm shoulder to wrist (upper arm 8.2, forearm 8.2).
        vr_am_sh_r = '0.5 -13.5 24';
        vr_am_sh_l = '0.5 13 24';
        vr_am_arm = 16.4;
        // The grunt-made poses moved so the grunt's shoulders land on his (half as wide again, 8.8 higher).
        vr_am_fit_scale = '1 1.44 1';
        vr_am_fit_shift = '0.2 0.04 8.8';
        vr_am_grip_r = '6.3 -20.3 9.3';
        vr_am_grip_l = '6.2 14.8 8';
        vr_am_gun_fw = '0.674 -0.376 0.636';
        vr_am_range = 0.85; // (armoured, as the enforcer: he holds his ground)
        vr_am_range_threat = 1.3;
        vr_am_spell = hknight_magicc1;
        return;
    }
    if(k == VR_AM_KNIGHT)
    {
        // Quake VR's knight.mdl: his sword's vertices 334..336, 363..365, 524, 535..540, 557..648 (vr_monstermods.cpp
        // knownSwords), its pommel 568/571, its tip 561/564, its guard's ends 537/594; his fist on its grip (106, 110,
        // 114: the stillest in the sword's space over his living frames, 2.3-2.9 units rms: his frames slide it in his
        // hand); ROUND23.md, "Knights as armed humanoids". Every weapon class, as the grunt's (his guns aimed and fired by
        // his posed arms over his stand frames: no aiming frames of his own).
        vr_am_name = "Knight";
        vr_am_spawnname = "monster_vr_knight";
        vr_am_classname = "monster_knight";
        vr_am_model = "progs/vrknight.mdl";
        vr_am_spawn = monster_knight;
        vr_am_hand = '106 110 114';
        vr_am_gun_rear = '568 571 0';
        vr_am_gun_muzzle = '561 564 0';
        vr_am_gun_roll = '537 594 0';
        vr_am_f_stand = 0; vr_am_n_stand = 9; // stand1..9
        vr_am_f_run = 9; // runb1..8
        vr_am_rund[0] = 16; vr_am_rund[1] = 20; vr_am_rund[2] = 13; vr_am_rund[3] = 7;
        vr_am_rund[4] = 16; vr_am_rund[5] = 20; vr_am_rund[6] = 14; vr_am_rund[7] = 6;
        vr_am_f_atk = 0;     // (no aiming frames: its stand, its posed arms aim; the grunt's shot timing)
        vr_am_atk_stand = 1;
        vr_am_f_shove = 42;  // attackb1..6 (a gun's butt: his slash's lunge under his posed arms)
        vr_am_f_reload = 0;  // (no reloads: its stand)
        vr_am_n_reload = 9;
        vr_am_f_take = 66;   // kneel1..5: down on a knee
        vr_am_n_take = 5;
        vr_am_pain1 = "knight/khurt.wav";
        vr_am_pain2 = "knight/khurt.wav";
        vr_am_loot_aid = AID_SHELLS;
        vr_am_loot_n = 0;
        vr_am_classes = VR_AM_CLASSES_ALL;
        vr_am_takes_ranged = cvar_hget(cvarh_vr_agknight_ranged_pickup); // (0, the game's: a knight's a blade)
        vr_am_back_lo = '145 172 0'; // (his back at the belt, x -2, z 1.9)
        vr_am_back_hi = '180 320 0'; // (... at the shoulder blades, x -1.4, z 13.5)
        vr_am_anchor = 224;
        vr_am_anchor_at = '7.3 2 15.6';
        // The authored rig's shoulders (quakevr/rigs/vrknight.rig2.txt: his upper arms' joints, under the pads); his
        // right arm shoulder to wrist (upper arm 8.4, forearm 8.3).
        vr_am_sh_r = '4 -8.5 13';
        vr_am_sh_l = '3 11 13';
        vr_am_arm = 16.7;
        // The grunt-made poses moved so the grunt's shoulders land on his (his a little wider, lower and forward).
        vr_am_fit_scale = '1 1.06 1';
        vr_am_fit_shift = '3.2 1.46 -2.2';
        vr_am_grip_r = '12 -6.7 1.1';
        vr_am_grip_l = '-2.4 14.6 -1.1';
        vr_am_gun_fw = '0.497 0.277 0.823';
        vr_am_range = 1;
        vr_am_range_threat = 1.2;
        return;
    }
    if(k == VR_AM_ENFORCER)
    {
        // Quake VR's enforcer.mdl: the laser rifle's vertices 22, 23, 100, 400..430, 455..478 (vr_monstermods.cpp);
        // its right hand on the rifle's grip (vertices 41, 364, 365: still in the gun's space over every living frame,
        // within 1-1.6 units rms; scratch analysis, ROUND22.md).
        vr_am_name = "Enforcer";
        vr_am_spawnname = "monster_vr_enforcer";
        vr_am_classname = "monster_enforcer";
        vr_am_model = "progs/vrenforcer.mdl";
        vr_am_spawn = monster_enforcer;
        vr_am_hand = '41 364 365';
        vr_am_gun_rear = '425 420 0';
        vr_am_gun_muzzle = '407 100 0';
        vr_am_gun_roll = '409 412 0';
        vr_am_f_stand = 0; vr_am_n_stand = 7;
        vr_am_f_run = 23; // run1..8
        vr_am_rund[0] = 18; vr_am_rund[1] = 14; vr_am_rund[2] = 7; vr_am_rund[3] = 12;
        vr_am_rund[4] = 14; vr_am_rund[5] = 14; vr_am_rund[6] = 7; vr_am_rund[7] = 11;
        vr_am_f_atk = 32;    // attack2..10 (its shot in attack6, as id's)
        vr_am_atk_stand = 1; // (vr_agrunt_atk_stand: its stand frames under its posed arms as it fires)
        vr_am_f_shove = 102; // shove1..6
        vr_am_f_reload = 0;  // (no load frames: its stand)
        vr_am_n_reload = 7;
        vr_am_f_take = 85;   // paind3..7: down on a knee (its top 34, 34, 30, 22, 18 units; its hand 6 units over the floor)
        vr_am_n_take = 5;
        vr_am_pain1 = "enforcer/pain1.wav";
        vr_am_pain2 = "enforcer/pain2.wav";
        vr_am_loot_aid = AID_CELLS;
        vr_am_loot_n = 5;
        vr_am_classes = VR_AM_CLASSES_ALL;
        vr_am_takes_ranged = 1;
        vr_am_back_lo = '328 332 0'; // (its power pack's back, low: x -20.3, z 7.8, standing)
        vr_am_back_hi = '343 351 0'; // (... high: x -22.8, z 22.7)
        vr_am_anchor = 322;
        vr_am_anchor_at = '-9.9 -1.6 21.9';
        // The authored rig's shoulders, its upper arms' joints (quakevr/rigs/vrenforcer.rig2.txt, inside the pads); its
        // right arm shoulder to wrist (upper arm 8.3, forearm 6.7; the grunt's 15.2).
        vr_am_sh_r = '-5.5 -12.5 21';
        vr_am_sh_l = '-5.5 11.5 21';
        vr_am_arm = 15;
        // The grunt-made poses fitted so the grunt's authored shoulders land on these, drawn in ahead and down 15 and
        // 10% (its arm reaches 18.9 to a grip: the goals a plain shift put were out of reach; glitch finder reach p95
        // 3.7 -> 2.6; ROUND22.md, "The enforcer's rig fixed").
        vr_am_fit_scale = '0.85 1.3 0.9';
        vr_am_fit_shift = '-5.75 0 7.3';
        vr_am_grip_r = '3.3 -5.5 11';
        vr_am_grip_l = '8.7 11.6 13.4';
        vr_am_gun_fw = '0 0.984 0.18';
        vr_am_range = 0.85; // (armoured, it holds its ground more than a grunt)
        vr_am_range_threat = 1.3;
        return;
    }
    // The grunt (the first row; k 0, an armed grunt from before the table): Quake VR's soldier.mdl, the gun's vertices
    // 463..548.
    vr_am_kind = VR_AM_GRUNT;
    vr_am_name = "Grunt";
    vr_am_spawnname = "monster_vr_grunt";
    vr_am_classname = "monster_army";
    vr_am_model = "progs/vrgrunt.mdl";
    vr_am_spawn = monster_army;
    vr_am_hand = '32 26 31';
    vr_am_gun_rear = '497 545 0';
    vr_am_gun_muzzle = '498 514 0';
    vr_am_gun_roll = '529 496 0';
    vr_am_f_stand = 0; vr_am_n_stand = 8;
    vr_am_f_run = 73; // run1..8
    vr_am_rund[0] = 11; vr_am_rund[1] = 15; vr_am_rund[2] = 10; vr_am_rund[3] = 10;
    vr_am_rund[4] = 8; vr_am_rund[5] = 15; vr_am_rund[6] = 10; vr_am_rund[7] = 8;
    vr_am_f_atk = 81;    // shoot1..9
    vr_am_atk_stand = 0;
    vr_am_f_shove = 114; // shove1..6
    vr_am_f_reload = 29; // load1..11 (unused by id's)
    vr_am_n_reload = 11;
    vr_am_f_take = 46;   // painb1..6: down on a knee, bent over (its top 28, 22, 18, 17, 10, 2 units; its hand 9 units over
    vr_am_n_take = 6;    // the floor. painc4..7, a crouch to 15 units, its hand 18 over it, was too shallow: note 2026-10-24 01:15:28)
    vr_am_pain1 = "soldier/pain1.wav";
    vr_am_pain2 = "soldier/pain2.wav";
    vr_am_loot_aid = AID_SHELLS;
    vr_am_loot_n = 5;
    vr_am_classes = VR_AM_CLASSES_ALL;
    vr_am_takes_ranged = 1;
    vr_am_fit_scale = '1 1 1';
    vr_am_fit_shift = '0 0 0';
    vr_am_back_lo = '93 95 0';   // (its back at the waist, x -8.9 / -8.0, z 5.3, standing)
    vr_am_back_hi = '160 162 0'; // (... at the shoulder blades, x -6.2, z 14.8)
    vr_am_anchor = 148;
    vr_am_anchor_at = '-0.1 0 19.3';
    // The authored rig's shoulders, its upper arms' joints (quakevr/rigs/vrgrunt.rig2.txt): its weapons' second grips
    // brought within reach of these (VR_AGP_SecondReach), as its arms' solve has them.
    vr_am_sh_r = '-0.3 -9.4 15.2';
    vr_am_sh_l = '0.9 9.0 15.2';
    vr_am_arm = 15.2; // (its right arm, shoulder to wrist: upper arm 8.2, forearm 7.0)
    vr_am_grip_r = '5.7 -3.6 4.8';
    vr_am_grip_l = '11.8 5.5 1.9';
    vr_am_gun_fw = '0.394 0.888 -0.235';
    vr_am_range = 1;
    vr_am_range_threat = 1.5;
};

// The right hand's middle and the hidden gun's points of `g` as drawn now (modelvertex; trace_fraction 0: not now).
vector(entity g) VR_AM_Hand =
{
    VR_AM_Row(g.vr_ag_kind);
    vector h = modelvertex(g, vr_am_hand_x) + modelvertex(g, vr_am_hand_y) + modelvertex(g, vr_am_hand_z);
    return h * (1 / 3);
};
vector(entity g) VR_AM_GunRear =
{
    VR_AM_Row(g.vr_ag_kind);
    return (modelvertex(g, vr_am_gun_rear_x) + modelvertex(g, vr_am_gun_rear_y)) * 0.5;
};
vector(entity g) VR_AM_GunMuzzle =
{
    VR_AM_Row(g.vr_ag_kind);
    return (modelvertex(g, vr_am_gun_muzzle_x) + modelvertex(g, vr_am_gun_muzzle_y)) * 0.5;
};
vector(entity g) VR_AM_GunUp =
{
    VR_AM_Row(g.vr_ag_kind);
    return modelvertex(g, vr_am_gun_roll_x) - modelvertex(g, vr_am_gun_roll_y);
};

// A point of the poses' body space (made on the grunt: vr_armedgrunt_pose.qc) on `g`'s body.
vector(entity g, vector l) VR_AM_Fit =
{
    VR_AM_Row(g.vr_ag_kind);
    vector o;
    o_x = l_x * vr_am_fit_scale_x + vr_am_fit_shift_x;
    o_y = l_y * vr_am_fit_scale_y + vr_am_fit_shift_y;
    o_z = l_z * vr_am_fit_scale_z + vr_am_fit_shift_z;
    return o;
};

// Its animations (VR_AM_Anim): the frame of each from its row.
float VR_AM_A_RUN = 0;    // 0..7
float VR_AM_A_ATK = 1;    // 0..8
float VR_AM_A_SHOVE = 2;  // 0..5
float VR_AM_A_RELOAD = 3; // 0..10
float VR_AM_A_TAKE = 4;   // 0..3

float(entity g, float n) VR_AM_StandFrame;

// Self's frame `i` of animation `a`, and its next think (as a [frame, next] function's: then its body runs).
void(float a, float i, void() next) VR_AM_Anim =
{
    VR_AM_Row(self.vr_ag_kind);
    float f = vr_am_f_take + i;
    if(a == VR_AM_A_RUN)         { f = vr_am_f_run + i; }
    else if(a == VR_AM_A_ATK)
    {
        f = vr_am_f_atk + i;
        if(vr_am_atk_stand && cvar_hget(cvarh_vr_agrunt_atk_stand))
        {
            f = VR_AM_StandFrame(self, i); // (vr_agrunt_atk_stand: its legs and hips stand, its posed arms aim and fire)
        }
    }
    else if(a == VR_AM_A_SHOVE)  { f = vr_am_f_shove + i; }
    else if(a == VR_AM_A_RELOAD) { f = vr_am_f_reload + i - floor(i / vr_am_n_reload) * vr_am_n_reload; }
    self.frame = f;
    self.nextthink = time + 0.1;
    self.think = next;
};

// Its run's step in frame `i` (VR_AM_A_RUN).
float(float i) VR_AM_RunStep = { VR_AM_Row(self.vr_ag_kind); return vr_am_rund[i]; };

// Its stand frame `n` (counting on: it cycles).
float(entity g, float n) VR_AM_StandFrame =
{
    VR_AM_Row(g.vr_ag_kind);
    return vr_am_f_stand + n - floor(n / vr_am_n_stand) * vr_am_n_stand;
};

// May `g` hold a weapon of class `c` (VR_AG_CLASS_*)?
float(entity g, float c) VR_AM_Holds =
{
    VR_AM_Row(g.vr_ag_kind);
    return (vr_am_classes & (1 << c)) != 0;
};

// May `g` take up weapon `wid` lying about (its class held; a ranged one only by a row that takes them)?
float(entity g, float wid) VR_AM_Takes =
{
    float c = VR_AG_Class(wid);
    return VR_AM_Holds(g, c) && (c == VR_AG_CLASS_MELEE || vr_am_takes_ranged);
};

string(entity g) VR_AM_Pain =
{
    VR_AM_Row(g.vr_ag_kind);
    return random() < 0.5 ? vr_am_pain1 : vr_am_pain2;
};
