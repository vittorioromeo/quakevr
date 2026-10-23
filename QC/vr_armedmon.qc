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
// Adding one: a row (VR_AM_Row), its stripped model (Misc/quakevr/make_vrgrunt.py), its spawn function
// (monster_vr_<name> = VR_AM_Spawn(kind)), the hooks in its own code (its death: VR_AG_Dying before its gun falls; its
// loot: VR_AG_SoldierDrop; CheckAnyAttack: VR_AG_CheckAttack for any .vr_ag), the engine's row (vr_hitreact.cpp
// armedRigs: its hands' IK) and its model id (vr_modelmetadata.inc, the ragdoll's seeds in vr_ragdoll.cpp).

float VR_AM_GRUNT = 0;    // monster_vr_grunt: a grunt (monster_army)
float VR_AM_ENFORCER = 1; // monster_vr_enforcer: an enforcer (monster_enforcer)
float VR_AM_COUNT = 2;

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
float vr_am_f_shove;        // its shove frames (6: a gun's butt): the first
float vr_am_f_reload;       // its reload frames (11 drawn, these repeating): the first,
float vr_am_n_reload;       // ... how many
float vr_am_f_take;         // taking up a weapon (bending down: this and the next three frames, and back)
string vr_am_pain1;         // its pain sounds (a tug, a disarm)
string vr_am_pain2;
float vr_am_loot_aid;       // its backpack with no reserve: this ammo (AID_*),
float vr_am_loot_n;         // ... this many
float vr_am_classes;        // the weapon classes it may hold (VR_AM_CLASSES_ALL: any; others: its fists)
vector vr_am_fit_scale;     // the poses' body space (vr_armedgrunt_pose.qc, made on the grunt) to its: scaled,
vector vr_am_fit_shift;     // ... then moved (its shoulders against the grunt's)
vector vr_am_back_lo;       // its back's vertices (two, x y: their middle) low (the waist) and high (the shoulder blades):
vector vr_am_back_hi;       // ... a flamethrower's or poison sprayer's tank worn there, upright along them (vr_adtankworn.qc)

void(float k) VR_AM_Row =
{
    vr_am_kind = k;
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
        vr_am_f_shove = 102; // shove1..6
        vr_am_f_reload = 0;  // (no load frames: its stand)
        vr_am_n_reload = 7;
        vr_am_f_take = 87;   // paind5..8: down on a knee
        vr_am_pain1 = "enforcer/pain1.wav";
        vr_am_pain2 = "enforcer/pain2.wav";
        vr_am_loot_aid = AID_CELLS;
        vr_am_loot_n = 5;
        vr_am_classes = VR_AM_CLASSES_ALL;
        // Its shoulders (the rig's upperarm joints: x -4.3 .. -5.9, y +-13.4, z 19.4 .. 21.7) against the grunt's
        // (x 0, y +-9.7, z 12).
        vr_am_fit_scale = '1 1.35 1.1';
        vr_am_fit_shift = '-3 0 7';
        vr_am_back_lo = '328 332 0'; // (its power pack's back, low: x -20.3, z 7.8, standing)
        vr_am_back_hi = '343 351 0'; // (... high: x -22.8, z 22.7)
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
    vr_am_f_shove = 114; // shove1..6
    vr_am_f_reload = 29; // load1..11 (unused by id's)
    vr_am_n_reload = 11;
    vr_am_f_take = 29;   // load1..4
    vr_am_pain1 = "soldier/pain1.wav";
    vr_am_pain2 = "soldier/pain2.wav";
    vr_am_loot_aid = AID_SHELLS;
    vr_am_loot_n = 5;
    vr_am_classes = VR_AM_CLASSES_ALL;
    vr_am_fit_scale = '1 1 1';
    vr_am_fit_shift = '0 0 0';
    vr_am_back_lo = '93 95 0';   // (its back at the waist, x -8.9 / -8.0, z 5.3, standing)
    vr_am_back_hi = '160 162 0'; // (... at the shoulder blades, x -6.2, z 14.8)
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

// Self's frame `i` of animation `a`, and its next think (as a [frame, next] function's: then its body runs).
void(float a, float i, void() next) VR_AM_Anim =
{
    VR_AM_Row(self.vr_ag_kind);
    float f = vr_am_f_take + i;
    if(a == VR_AM_A_RUN)         { f = vr_am_f_run + i; }
    else if(a == VR_AM_A_ATK)    { f = vr_am_f_atk + i; }
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

string(entity g) VR_AM_Pain =
{
    VR_AM_Row(g.vr_ag_kind);
    return random() < 0.5 ? vr_am_pain1 : vr_am_pain2;
};
