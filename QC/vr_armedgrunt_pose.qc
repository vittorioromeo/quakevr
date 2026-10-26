// vr_armedgrunt_pose.qc -- armed grunts' presentation and attacks (ROUND22.md, "Armed grunts: presentation"): where
// their hands and their weapon are each server frame, their punches, swings and chainsaw thrusts, sent to the clients.
//
// Each server frame an armed grunt's pose is worked out here in its body's space (x ahead, y its left, z up, from its
// origin; its yaw) and sent (QVR_SVC_ARMPOSE, unreliable) for the client to put on its body as drawn (vr_hitreact.cpp:
// the weapon drawn there, the hands' two-bone IK reaching it), so weapon and hands move with the drawn body, not with the
// server's step and pose timing (the jitter). The held weapon entity is placed the same way for the game (its hand
// touch, its muzzle, a blow's line).
// - The right hand (the weapon's grip, a fist) at `wr` times its animation's hand plus `v`: a gun follows its animation
//   (wr 1, `v` its recoil); a ready melee weapon or a fists' guard mostly holds its own place (wr 0.3), an attack all
//   of it (wr 0).
// - The weapon's turn in its body's space and in its animation's rifle line's (QC VR_AG_GUN_*), `wo` the second's
//   share: a gun carried along its rifle line, aimed (vr_ag_aimw) in its body's.
// - The left hand: on the weapon's second grip (two-handed), or its own target (a guard, a jab, a one-handed weapon's
//   free hand).
// Attacks (VR_AGP_Attack, its th_melee): a wind-up, the strike, a recovery, timed here per frame, the blow struck as
// the fist or the blade meets the target (that weapon's own melee: W_FistMelee, W_FireAxe, W_SwordMelee,
// W_CrowbarMelee, HIP_FireMjolnir, W_SuperAxeMelee), its legs on the grunt's stand frames stepping in. A chainsaw runs
// (fuel: its clip, VR_CHAINSAW_TANK; reserve tanks refuel it as a reload), revs, is thrust into the target and held
// there cutting (VR_Chainsaw_Cut's ticks) and pulled back.

float VR_AGP_PUNCH = 1;
float VR_AGP_SWING = 2;
float VR_AGP_SAW = 3;
float VR_AGP_LEFT = 2;      // QVR_SVC_ARMPOSE flags: the left hand to its own target
float VR_AGP_LEFTWPN = 8;   // ... holding its own weapon there (dual wielding: the message goes on with it and its turn)
float VR_AGP_LEFTMIRROR = 16; // (QC only, never sent) two guns: the left's target wl x the right hand mirrored + vl
float VR_AGP_BODY = 32;     // ... its body's turns in a strike (VR_AGP_TorsoNow: the torso's, the elbows' poles)
float VR_AGP_FIXED = 1;     // ... the body's turns alone, its hands' targets not turned with it (a take's bend)
float VR_AGP_TAKE_BEND = 28; // degrees its chest bends forward at the waist taking a weapon up from the floor (vr_agarms; its take frames' kneel)
float VR_AGP_TAKE_BEND_KNEEL = 45; // ... on one knee (vr_aglegs: its pelvis higher, the rest pose's chest on it)
float VR_AGP_ARM_REACH = 21; // a fist's reach from its shoulder (units, about the rig's arm)
vector VR_AGP_SHOULDER_R = '0 -9.7 12';
vector VR_AGP_SHOULDER_L = '0 9.7 12';
vector VR_AGP_GUARD_R = '9 -5 13';
vector VR_AGP_GUARD_L = '11 5 15';
vector VR_AGP_READY = '10 -7 4';        // a melee weapon's grip, ready: blade up
vector VR_AGP_READY_FW = '0.3 0.1 0.95';
vector VR_AGP_FREE_L = '7 9 4';         // a one-handed weapon's free hand
vector VR_AGP_SAW_READY = '9 -7 1';     // a chainsaw at the hip, bar ahead
vector VR_AGP_SAW_FW = '0.95 0.05 0.25';
vector VR_AGP_CARRY_R = '6 -9 3';       // vr_agarms: a one-handed melee weapon carried by its side, the blade ahead, a little down (the fist hanging)
vector VR_AGP_CARRY_R_FW = '0.95 -0.1 0.15';
vector VR_AGP_PORT = '9 -5 3';          // ... a two-handed one at port arms, across the body
vector VR_AGP_PORT_FW = '0.35 0.55 0.75';

.float vr_ag_act;      // its attack under way: VR_AGP_* (0 none)
.float vr_ag_act_kind; // which: a punch's or a swing's variation
.float vr_ag_act_at;   // when it began
.float vr_ag_act_tick; // its frames' last think (stale: interrupted, a pain)
.float vr_ag_act_hit;  // its blow struck
.float vr_ag_act_combo; // punches in a row
.float vr_ag_act_var;  // its variation (0..1, each attack: VR_AGP_TorsoNow's amounts)
.float vr_ag_demo;     // its attack is vr_agrunt_strike_demo's (at the air, standing)
.float vr_ag_demo_next;
.float vr_ag_aimw;     // a gun's aim's share (0 its rifle line .. 1 at the target), eased
.vector vr_ag_aimdir;  // the last aim
.float vr_ag_readyw;   // its stance's share (a guard, a melee weapon ready), eased
.float vr_ag_saw_snd;  // its chainsaw's loop playing (VR_SAW_LOOP_*)
.float vr_ag_saw_next; // its next cut tick, its next puff of smoke
.float vr_ag_saw_puff;
.entity vr_ag_poser;   // the fists' pose sender (a held weapon sends it otherwise)
.float vr_ag_tilt;     // a held melee weapon's blade's turn up from its entity's forward (degrees; VR_AGP_BladeTilt)
.float vr_ag_tilt_set;
// vr_agarms (VR_AGP_ArmsGun and below):
float VR_AGP_ANCHOR = 64;     // QVR_SVC_ARMPOSE flags: wr and wl weigh its chest's anchor (vr_am_anchor), not its hands
float VR_AGP_REACH = 128;     // ... its right hand reaching for a weapon lying (sent as none): its grip frame to the weapon's (fw, up)
.float vr_agp_geo;            // its points found (a weapon entity: once)
.vector vr_agp_muz;           // its muzzle (a blade's tip) in its own frame (weapondrawnpose 8)
.vector vr_agp_butt;          // its butt (9)
.vector vr_agp_sec;           // its second grip (10; z 1000: a Cup; '0 0 0' none)
.float vr_agp_runw;           // its run's sway's share (eased)
.float vr_agp_rdy;            // a melee weapon or the fists ready (an enemy) or carried (eased)
.float vr_agp_rl_at;          // its reload's start (vr_ag_reload1; 0 none)
.vector vr_agp_twist;        // vr_agrunt_upper: its chest's turn towards its gun's aim (yaw, pitch; eased)
float vr_agp_arms;            // VR_AGP_Pose: vr_agarms this frame
float vr_agp_gunpose;         // VR_AGP_Pose: a gun (or two) held as a player holds it this frame (VR_AGP_ArmsGun)

// How far up from its entity's forward a weapon model's muzzle (a melee weapon's tip) is drawn: its angles zeroed a
// moment, its drawn muzzle measured (weapondrawnpose), found once a weapon.
float(entity p) VR_AGP_BladeTilt =
{
    if(p.vr_ag_tilt_set)
    {
        return p.vr_ag_tilt;
    }
    vector keep = p.angles;
    p.angles = '0 0 0';
    vector m = weaponmuzzle(p) - p.origin;
    p.angles = keep;
    p.vr_ag_tilt_set = 1;
    p.vr_ag_tilt = (m_x * m_x + m_z * m_z) > 1 ? atan2(m_z, m_x) * (180 / 3.14159265) : 0;
    VR_AG_Log(sprintf("agp: %s's tip %v from its grip: tilted %.1f degrees", p.netname, m, p.vr_ag_tilt));
    return p.vr_ag_tilt;
};

float vr_agp_wr;
vector vr_agp_v;
vector vr_agp_fw;    // world
vector vr_agp_up;
float vr_agp_wo;
float vr_agp_flags;
float vr_agp_wl;
vector vr_agp_vl;
vector vr_agp_f;     // the body's flat forward and right (VR_AGP_Body)
vector vr_agp_r;
float vr_agp_events; // blows and cuts struck (the tests)
float vr_agp_quiet;  // the pose only placed, not sent nor run (the toolgun's ghost: vr_armedmon_ui.qc VR_AM_UiGhost)
// The Pose Studio's keys this frame (VR_AGK_Keys, below: its body's space, units and degrees): the right target's place
// and turn (a rotation vector), the left's, the elbows' poles, the torso's turn; vr_agk_on: any.
vector vr_agk_rp;
vector vr_agk_rr;
vector vr_agk_lp;
vector vr_agk_lr;
vector vr_agk_er;
vector vr_agk_el;
vector vr_agk_t;
float vr_agk_on;

void(entity g) VR_AGP_Body = { makevectors([0, g.angles_y, 0]); vr_agp_f = v_forward; vr_agp_r = v_right; };
vector(vector l) VR_AGP_ToWorld = { return vr_agp_f * l_x - vr_agp_r * l_y + '0 0 1' * l_z; };
vector(vector w) VR_AGP_ToLocal =
{
    vector o;
    o_x = w * vr_agp_f;
    o_y = -(w * vr_agp_r);
    o_z = w_z;
    return o;
};
vector(vector a, vector b) VR_AGP_Cross =
{
    vector c;
    c_x = a_y * b_z - a_z * b_y;
    c_y = a_z * b_x - a_x * b_z;
    c_z = a_x * b_y - a_y * b_x;
    return c;
};
float(float x) VR_AGP_Smooth = { x = min(1, max(0, x)); return x * x * (3 - 2 * x); };
vector(vector a, vector b, float t) VR_AGP_Lerp = { return a + (b - a) * t; };
vector(vector a, vector b, float t) VR_AGP_Nlerp =
{
    vector m = a + (b - a) * t;
    return vlen(m) > 0.0001 ? normalize(m) : b;
};
vector(vector a, vector c, vector b, float t) VR_AGP_Bezier = { return a * ((1 - t) * (1 - t)) + c * (2 * t * (1 - t)) + b * (t * t); };
float(float x, float to, float rate) VR_AGP_Ease = { return x < to ? min(to, x + rate * frametime) : max(to, x - rate * frametime); };

// Two-handed melee weapons (the left hand on the weapon's second grip); the others are swung in the right hand.
float(entity p) VR_AGP_TwoHanded =
{
    if(p.owner.vr_ag_prop2)
    {
        return FALSE; // (two held: one in each hand)
    }
    float wid = p.weapon;
    if(wid == WID_SWORD)
    {
        return VR_ADMelee_Heavy(VR_ADMelee_Kind(p.weaponflags)); // (AD's hammer and mace; swords one-handed)
    }
    return wid == WID_AXE || wid == WID_MJOLNIR || wid == WID_CHAINSAW || wid == WID_SUPERAXE;
};

// An attack's times: its wind-up, strike, hold (a chainsaw's cut) and recovery.
float vr_agp_tw;
float vr_agp_ts;
float vr_agp_th;
float vr_agp_tr;
void(entity g) VR_AGP_Times =
{
    vr_agp_th = 0;
    if(g.vr_ag_act == VR_AGP_PUNCH)
    {
        float k = g.vr_ag_act_kind;
        vr_agp_tw = k == 0 ? 0.08 : k == 1 ? 0.12 : 0.14;
        vr_agp_ts = k == 0 ? 0.1 : k == 1 ? 0.11 : 0.13;
        vr_agp_tr = k == 0 ? 0.18 : 0.24;
    }
    else if(g.vr_ag_act == VR_AGP_SAW)
    {
        vr_agp_tw = 0.3;
        vr_agp_ts = 0.15;
        vr_agp_th = 0.7;
        vr_agp_tr = 0.3;
    }
    else
    {
        float heavy = g.vr_ag_prop && VR_AGP_TwoHanded(g.vr_ag_prop);
        vr_agp_tw = heavy ? 0.36 : 0.28;
        vr_agp_ts = heavy ? 0.18 : 0.15;
        vr_agp_tr = heavy ? 0.36 : 0.3;
    }
};
float(entity g) VR_AGP_Length = { VR_AGP_Times(g); return vr_agp_tw + vr_agp_ts + vr_agp_th + vr_agp_tr; };

// Its target's point in its body's space (a player's chest, a monster's middle).
vector(entity g) VR_AGP_TargetLocal =
{
    entity t = g.enemy;
    if(!t)
    {
        return '40 0 10';
    }
    vector at = VR_AI_Origin(t) + '0 0 1' * ((t.flags & FL_CLIENT) ? 8 : (t.mins_z + t.maxs_z) * 0.5);
    return VR_AGP_ToLocal(at - g.origin);
};

// A blow from `from` along `dir` (world), with its weapon `p` (world: a punch): that weapon's own melee, `g` striking.
void(entity g, entity p, vector from, vector dir) VR_AGP_Strike =
{
    if(vr_agk_on)
    {
        // (the Pose Studio's keys move the blow's line with the pose: its hand's place and turn)
        float kl = p ? p.vr_ag_left : (g.vr_ag_act_kind == 0 || g.vr_ag_act_kind == 2);
        vector kr = kl ? vr_agk_lr : vr_agk_rr;
        VR_AGP_Body(g);
        from = from + VR_AGP_ToWorld(kl ? vr_agk_lp : vr_agk_rp);
        if(vlen(kr) > 0.01)
        {
            dir = VR_AG_Rotate(dir, normalize(VR_AGP_ToWorld(kr)), vlen(kr));
        }
    }
    entity oldself = self;
    self = g;
    float h = cVR_MainHand;
    self.handpos = from;
    vector a = vectoangles(dir);
    a_x = -a_x;
    self.handrot = a;
    self.shotrot = '0 0 0';
    self.handvel = dir * 300;
    self.muzzlepos = p ? weaponmuzzle(p) : from + dir * 4;
    if(p && vlen(self.muzzlepos - from) < 4)
    {
        self.muzzlepos = from + dir * 24;
    }
    self.weapon = p ? p.weapon : WID_FIST;
    self.weaponflags = p ? p.weaponflags : 0;
    self.weaponinst = p ? p.weaponinst : world;
    makevectors(a);
    VRSetMeleeHitSoundPlayed(h, FALSE);
    float wid = self.weapon;
    float cells = self.ammo_cells;
    self.ammo_cells = 0; // (Mjolnir as the hammer: no lightning)
    if(wid == WID_MJOLNIR)             { HIP_FireMjolnir(h); }
    else if(wid == WID_FIST)           { W_FistMelee(h); }
    else if(wid == WID_AXE)            { W_FireAxe(h); }
    else if(wid == WID_SWORD)          { W_SwordMelee(h); }
    else if(wid == WID_CROWBAR)        { W_CrowbarMelee(h); }
    else if(wid == WID_SUPERAXE)       { W_SuperAxeMelee(h); }
    else if(wid == WID_CHAINSAW)       { W_ChainsawMelee(h); }
    else                               { W_GunMelee(h); }
    self.ammo_cells = cells;
    self.weaponinst = world;
    self.vr_ag_strikes = self.vr_ag_strikes + 1;
    vr_agp_events = vr_agp_events + 1;
    VR_AG_Log(sprintf("agp: %s blow (%s) at %s", p ? p.netname : "a fist's", vtos(from), self.enemy.classname));
    self = oldself;
};

// The fists: a guard with an enemy (their own place), else the animation's hands; a punch's fist to its target.
void(entity g, vector handL) VR_AGP_Fists =
{
    g.vr_ag_readyw = VR_AGP_Ease(g.vr_ag_readyw, g.enemy ? 0.75 : 0, 3);
    float k = g.vr_ag_readyw;
    vector r = VR_AGP_GUARD_R;
    vector l = VR_AGP_GUARD_L;
    float s = time - g.vr_ag_act_at;
    if(g.vr_ag_act == VR_AGP_PUNCH)
    {
        VR_AGP_Times(g);
        float kind = g.vr_ag_act_kind; // 0 a jab (left), 1 a cross (right), 2 a hook (left), 3 a hook (right), 4 an uppercut (right)
        float left = kind == 0 || kind == 2;
        vector sh = left ? VR_AGP_SHOULDER_L : VR_AGP_SHOULDER_R;
        vector guard = left ? l : r;
        vector tl = VR_AGP_TargetLocal(g);
        if(kind == 4)
        {
            tl_z = tl_z + 4;
        }
        vector d = tl - sh;
        vector hit = sh + normalize(d) * min(vlen(d) - 2, VR_AGP_ARM_REACH);
        vector wind = guard - '3 0 0';
        vector ctl = (guard + hit) * 0.5;
        if(kind == 2 || kind == 3)
        {
            wind = guard + (left ? '-4 8 0' : '-4 -8 0');
            ctl = sh + (left ? '12 12 3' : '12 -12 3');
        }
        else if(kind == 4)
        {
            wind = '6 -6 2';
            ctl = '13 -5 3';
        }
        vector fist;
        float m = 1; // (the stance's share: all of it in a punch)
        if(s < vr_agp_tw)
        {
            fist = VR_AGP_Lerp(guard, wind, VR_AGP_Smooth(s / vr_agp_tw));
            m = k + (1 - k) * VR_AGP_Smooth(s / 0.06);
        }
        else if(s < vr_agp_tw + vr_agp_ts)
        {
            float u = (s - vr_agp_tw) / vr_agp_ts;
            // (vr_agrunt_punch_ease: eased out of the wind-up and into the blow, no stop dead at the target; before,
            // u * u * (2 - u): still moving at the target, stopped there as the recovery began)
            float pe = cvar_hget(cvarh_vr_agrunt_punch_ease) ? VR_AGP_Smooth(u) : u * u * (2 - u);
            fist = VR_AGP_Bezier(wind, ctl, hit, pe);
            if(!g.vr_ag_act_hit && u >= 0.8)
            {
                g.vr_ag_act_hit = 1;
                VR_AGP_Body(g);
                vector fw = VR_AGP_ToWorld(fist - wind);
                VR_AGP_Strike(g, world, g.origin + VR_AGP_ToWorld(fist) - normalize(fw) * 6, normalize(fw));
                VR_AGP_Body(g);
            }
        }
        else
        {
            float u2 = (s - vr_agp_tw - vr_agp_ts) / vr_agp_tr;
            fist = VR_AGP_Lerp(hit, guard, VR_AGP_Smooth(u2));
            m = 1 - (1 - k) * VR_AGP_Smooth((u2 - 0.6) / 0.4);
        }
        k = m;
        if(left) { l = fist; } else { r = fist; }
    }
    if(vr_agp_arms)
    {
        // (vr_agarms: hanging free, raised into the guard, the punches: its body's own places, from its anchor)
        VR_AM_Row(g.vr_ag_kind);
        float kg = min(1, k / 0.75);
        vr_agp_wr = 1;
        vr_agp_v = VR_AGP_Lerp(VR_AGP_Hang(0), VR_AM_Fit(g, r), kg) - vr_am_anchor_at;
        vr_agp_flags = VR_AGP_LEFT;
        vr_agp_wl = 1;
        vr_agp_vl = VR_AGP_Lerp(VR_AGP_Hang(1), VR_AM_Fit(g, l), kg) - vr_am_anchor_at;
        return;
    }
    vr_agp_wr = 1 - k;
    vr_agp_v = VR_AM_Fit(g, r) * k; // (the grunt-made pose on its body: vr_armedmon.qc)
    vr_agp_flags = VR_AGP_LEFT;
    vr_agp_wl = 1 - k;
    vr_agp_vl = VR_AM_Fit(g, l) * k;
};

// A swing's keys (the right hand's; the left's: y the other way): its wind-up (vr_agp_kw, the blade vr_agp_kwfw), its
// arc's control (vr_agp_kc), its end (vr_agp_ke, vr_agp_kefw). Kinds: 0 overhead, 1 diagonal, 2 horizontal, 3 backhand.
vector vr_agp_kw;
vector vr_agp_kwfw;
vector vr_agp_kc;
vector vr_agp_ke;
vector vr_agp_kefw;
void(float kind) VR_AGP_SwingKeys =
{
    if(kind == 0)      { vr_agp_kw = '0 -7 24'; vr_agp_kwfw = '-0.55 0.05 0.83'; vr_agp_kc = '16 -6 22'; vr_agp_ke = '17 -3 2'; vr_agp_kefw = '0.6 0 -0.8'; }
    else if(kind == 1) { vr_agp_kw = '1 -14 21'; vr_agp_kwfw = '-0.4 -0.55 0.73'; vr_agp_kc = '17 -10 16'; vr_agp_ke = '14 7 0'; vr_agp_kefw = '0.5 0.55 -0.67'; }
    else if(kind == 2) { vr_agp_kw = '3 -17 12'; vr_agp_kwfw = '-0.45 -0.85 0.25'; vr_agp_kc = '20 -6 12'; vr_agp_ke = '9 13 10'; vr_agp_kefw = '0.25 0.95 0.1'; }
    else               { vr_agp_kw = '8 12 6'; vr_agp_kwfw = '0.1 0.8 -0.55'; vr_agp_kc = '19 2 12'; vr_agp_ke = '10 -14 20'; vr_agp_kefw = '0.3 -0.7 0.65'; }
};
vector(vector v) VR_AGP_Mirror = { v_y = -v_y; return v; };

// Whether the right hand's weapon swings in its attack (two held: VR_AGP_Begin's hands).
float(entity g) VR_AGP_RightSwings = { return !g.vr_ag_prop2 || g.vr_ag_act_hand != 1; };

// vr_agrunt_swing_one_hand: a two-handed melee weapon (not the chainsaw's thrust) swung in the right hand alone, its
// off hand off the second grip while it swings (the swings' arcs are one hand's: the off hand chased a grip it could
// not keep, ROUND22.md "Armed humanoids in motion, round 2"); the engine lets go and takes it again over 0.25 s.
float(entity g, entity p) VR_AGP_SwingOneHand =
{
    return cvar_hget(cvarh_vr_agrunt_swing_one_hand) && g.vr_ag_act == VR_AGP_SWING && p && p.weapon != WID_CHAINSAW &&
           !g.vr_ag_prop2;
};

vector vr_agp_sawat; // (VR_AGP_SawHold's out: the grip, VR_AGP_Melee's space; the bar's way, its body's)
vector vr_agp_sawfw;
void(entity g, float rdy) VR_AGP_SawHold;
vector(entity p, vector at, vector fw) VR_AGP_SawReach;

// A melee weapon: ready (blade up, mostly its own place), a swing's arc through the target; a chainsaw at the hip,
// running, thrust into the target and held there cutting.
void(entity g, entity p, vector handL, vector fwCarry, vector upCarry) VR_AGP_Melee =
{
    float saw = p.weapon == WID_CHAINSAW;
    g.vr_ag_readyw = VR_AGP_Ease(g.vr_ag_readyw, 0.7, 3);
    float k = g.vr_ag_readyw;
    vector ready = saw ? VR_AGP_SAW_READY : VR_AGP_READY;
    vector readyFw = normalize(saw ? VR_AGP_SAW_FW : VR_AGP_READY_FW);
    float rk = 0;
    if(vr_agp_arms)
    {
        // (vr_agarms: carried by its side, a two-handed one at port arms, raised ready with an enemy)
        g.vr_agp_rdy = VR_AGP_Ease(g.vr_agp_rdy, g.enemy ? 1 : 0, 2.5);
        rk = VR_AGP_Smooth(g.vr_agp_rdy);
        float two = VR_AGP_TwoHanded(p);
        if(saw)
        {
            // (a chainsaw as its own rifle at rest, the bar ahead and across the body (the long guns' low carry: its
            // front handle in the off hand's reach), raised level towards the target when ready: VR_AGP_SawHold)
            VR_AGP_SawHold(g, 0);
            vector sawCarry = vr_agp_sawat;
            vector sawCarryFw = vr_agp_sawfw;
            VR_AGP_SawHold(g, 1);
            ready = VR_AGP_Lerp(sawCarry, vr_agp_sawat, rk);
            readyFw = VR_AGP_Nlerp(sawCarryFw, vr_agp_sawfw, rk);
        }
        else
        {
            ready = VR_AGP_Lerp(two ? VR_AGP_PORT : VR_AGP_CARRY_R, ready, rk);
            readyFw = VR_AGP_Nlerp(normalize(two ? VR_AGP_PORT_FW : VR_AGP_CARRY_R_FW), readyFw, rk);
        }
    }
    vector pos = ready;
    vector fw = readyFw;
    vector edge = '1 0 0';
    float s = time - g.vr_ag_act_at;
    if((g.vr_ag_act == VR_AGP_SWING || g.vr_ag_act == VR_AGP_SAW) && VR_AGP_RightSwings(g))
    {
        VR_AGP_Times(g);
        vector tl = VR_AGP_TargetLocal(g);
        float dz = min(12, max(-12, tl_z - 8));
        vector w; vector wfw; vector c; vector e; vector efw;
        float kind = g.vr_ag_act_kind;
        if(saw)
        {
            w = '5 -8 4'; wfw = '0.95 0 0.3';
            e = vr_agp_arms ? '14 -2 0' : '16 -4 0'; e_z = e_z + dz; // (vr_agarms: its front handle kept in reach)
            efw = normalize(tl - e);
            c = (w + e) * 0.5;
        }
        else
        {
            VR_AGP_SwingKeys(kind);
            w = vr_agp_kw; wfw = vr_agp_kwfw; c = vr_agp_kc; e = vr_agp_ke; efw = vr_agp_kefw;
        }
        if(!saw)
        {
            c_z = c_z + dz * 0.5;
            e_z = e_z + dz;
        }
        wfw = normalize(wfw);
        efw = normalize(efw);
        vector cfw = normalize(c - VR_AGP_SHOULDER_R) * 2;
        edge = normalize(efw - wfw);
        float tStrike = vr_agp_tw + vr_agp_ts;
        if(s < vr_agp_tw)
        {
            float u = VR_AGP_Smooth(s / vr_agp_tw);
            pos = VR_AGP_Lerp(ready, w, u);
            fw = VR_AGP_Nlerp(readyFw, wfw, u);
            k = k + (1 - k) * VR_AGP_Smooth(s / 0.1);
        }
        else if(s < tStrike)
        {
            float u1 = VR_AGP_Smooth((s - vr_agp_tw) / vr_agp_ts);
            pos = VR_AGP_Bezier(w, c, e, u1);
            fw = normalize(VR_AGP_Bezier(wfw, cfw, efw, u1));
            k = 1;
            if(!g.vr_ag_act_hit && u1 >= 0.55 && !saw)
            {
                g.vr_ag_act_hit = 1;
                VR_AGP_Body(g);
                // The blade's line through the target now: from the grip, towards the target's point.
                vector from = g.origin + VR_AGP_ToWorld(pos);
                VR_AGP_Strike(g, p, from, normalize(VR_AGP_ToWorld(tl - pos)));
                VR_AGP_Body(g);
            }
        }
        else if(s < tStrike + vr_agp_th)
        {
            // The chainsaw held in the target, sawing a little (its chain cuts each tick: VR_AGP_Saw).
            float u3 = (s - tStrike) / max(vr_agp_th, 0.01);
            pos = e + efw * (1.5 * sin(u3 * 6.2832 * 3));
            fw = efw;
            k = 1;
        }
        else
        {
            float u2 = VR_AGP_Smooth((s - tStrike - vr_agp_th) / vr_agp_tr);
            pos = VR_AGP_Lerp(e, ready, u2);
            fw = VR_AGP_Nlerp(efw, readyFw, u2);
            k = 1 - (1 - k) * VR_AGP_Smooth((u2 - 0.6) / 0.4);
        }
    }
    edge = normalize(edge - fw * (edge * fw));
    if(edge == '0 0 0') { edge = '0 0 1'; }
    VR_AGP_Body(g);
    if(vr_agp_arms)
    {
        // (vr_agarms: its own place, from its anchor; the blade's line and edge (its hand's frame from them after the
        // parry: VR_AGP_BladeHand); the free hand hanging, raised into a guard when ready)
        VR_AM_Row(g.vr_ag_kind);
        vr_agp_wr = 1;
        vr_agp_v = VR_AM_Fit(g, pos) - vr_am_anchor_at;
        if(saw)
        {
            vr_agp_v = VR_AGP_SawReach(p, vr_agp_v + vr_am_anchor_at, fw) - vr_am_anchor_at;
        }
        vr_agp_fw = VR_AGP_ToWorld(fw);
        vr_agp_up = VR_AGP_ToWorld(edge);
        vr_agp_wo = 0;
        if(!VR_AGP_TwoHanded(p) || VR_AGP_SwingOneHand(g, p))
        {
            vr_agp_flags = VR_AGP_LEFT;
            vr_agp_wl = 1;
            vr_agp_vl = VR_AGP_Lerp(VR_AGP_Hang(1), VR_AM_Fit(g, VR_AGP_FREE_L), rk) - vr_am_anchor_at;
        }
        return;
    }
    // Carried (k 0): along the animation's rifle line the other way (its rear over the shoulder: the old carry);
    // ready and swinging (k 1): its own.
    vector fwW = VR_AGP_Nlerp(fwCarry, VR_AGP_ToWorld(fw), k);
    vector upW = VR_AGP_Nlerp(upCarry, VR_AGP_ToWorld(edge), k);
    vr_agp_wr = 1 - k;
    vr_agp_v = VR_AM_Fit(g, pos) * k;
    vr_agp_fw = fwW;
    vr_agp_up = upW;
    vr_agp_wo = 0;
    if(!VR_AGP_TwoHanded(p))
    {
        vr_agp_flags = VR_AGP_LEFT;
        vr_agp_wl = 1 - k;
        vr_agp_vl = VR_AM_Fit(g, VR_AGP_FREE_L) * k;
    }
};

// Dual wielding: the left hand's weapon (its own target: wl 0, vl its grip), ready as the right's (mirrored), hanging
// forward at its side as it carries it; swung in its attacks (VR_AGP_Begin: vr_ag_act_hand 1 or 2) on the right's keys
// mirrored. Out: vr_agp_vl (its grip, its body's space), vr_agp_fwl and vr_agp_upl (the weapon's turn, world).
vector VR_AGP_READY_L = '10 7 4';
vector VR_AGP_READY_L_FW = '0.3 -0.1 0.95';
vector VR_AGP_CARRY_L = '5 10 -1';
vector VR_AGP_CARRY_L_FW = '0.45 0.1 -0.9';
vector vr_agp_fwl;
vector vr_agp_upl;
void(entity g, entity p) VR_AGP_MeleeLeft =
{
    float r = vr_agp_arms ? VR_AGP_Smooth(g.vr_agp_rdy) : VR_AGP_Smooth(g.vr_ag_readyw / 0.7);
    // (vr_agarms: carried as the right hand carries its own, mirrored: the blade ahead, a little up, the fist hanging. The
    // old carry's blade hanging down bent the left wrist to its limit)
    vector carry = vr_agp_arms ? VR_AGP_Mirror(VR_AGP_CARRY_R) : VR_AGP_CARRY_L;
    vector carryFw = vr_agp_arms ? VR_AGP_Mirror(VR_AGP_CARRY_R_FW) : VR_AGP_CARRY_L_FW;
    vector pos = VR_AGP_Lerp(carry, VR_AGP_READY_L, r);
    vector readyFw = VR_AGP_Nlerp(normalize(carryFw), normalize(VR_AGP_READY_L_FW), r);
    vector fw = readyFw;
    vector ready = pos;
    vector edge = '1 0 0';
    float s = time - g.vr_ag_act_at;
    if(g.vr_ag_act == VR_AGP_SWING && g.vr_ag_act_hand != 0)
    {
        VR_AGP_Times(g);
        vector tl = VR_AGP_TargetLocal(g);
        float dz = min(12, max(-12, tl_z - 8));
        VR_AGP_SwingKeys(g.vr_ag_act_kind);
        vector w = VR_AGP_Mirror(vr_agp_kw);
        vector wfw = normalize(VR_AGP_Mirror(vr_agp_kwfw));
        vector c = VR_AGP_Mirror(vr_agp_kc);
        vector e = VR_AGP_Mirror(vr_agp_ke);
        vector efw = normalize(VR_AGP_Mirror(vr_agp_kefw));
        c_z = c_z + dz * 0.5;
        e_z = e_z + dz;
        vector cfw = normalize(c - VR_AGP_SHOULDER_L) * 2;
        edge = normalize(efw - wfw);
        float tStrike = vr_agp_tw + vr_agp_ts;
        if(s < vr_agp_tw)
        {
            float u = VR_AGP_Smooth(s / vr_agp_tw);
            pos = VR_AGP_Lerp(ready, w, u);
            fw = VR_AGP_Nlerp(readyFw, wfw, u);
        }
        else if(s < tStrike)
        {
            float u1 = VR_AGP_Smooth((s - vr_agp_tw) / vr_agp_ts);
            pos = VR_AGP_Bezier(w, c, e, u1);
            fw = normalize(VR_AGP_Bezier(wfw, cfw, efw, u1));
            if(!g.vr_ag_act_hitl && g.vr_ag_act_hit != 2 && u1 >= 0.55)
            {
                g.vr_ag_act_hitl = 1;
                VR_AGP_Body(g);
                VR_AGP_Strike(g, p, g.origin + VR_AGP_ToWorld(pos), normalize(VR_AGP_ToWorld(tl - pos)));
                g.vr_ag_strikes_l = g.vr_ag_strikes_l + 1;
            }
        }
        else
        {
            float u2 = VR_AGP_Smooth((s - tStrike) / vr_agp_tr);
            pos = VR_AGP_Lerp(e, ready, u2);
            fw = VR_AGP_Nlerp(efw, readyFw, u2);
        }
    }
    edge = normalize(edge - fw * (edge * fw));
    if(edge == '0 0 0') { edge = '0 0 1'; }
    VR_AGP_Body(g);
    vr_agp_vl = VR_AM_Fit(g, pos);
    vr_agp_fwl = VR_AGP_ToWorld(fw);
    vr_agp_upl = VR_AGP_ToWorld(edge);
};

// A chainsaw's engine: idling (its fuel), revving through an attack, cutting what its bar is in (the player's chain's
// ticks: VR_Chainsaw_Cut), its smoke; stalled with an empty tank.
void(entity g, entity p) VR_AGP_Saw =
{
    entity inst = p.weaponinst;
    float act = g.vr_ag_act == VR_AGP_SAW;
    VR_AGP_Times(g);
    float s = time - g.vr_ag_act_at;
    float revving = act && s >= vr_agp_tw * 0.5 && s < vr_agp_tw + vr_agp_ts + vr_agp_th;
    float cutting = act && s >= vr_agp_tw + vr_agp_ts * 0.6 && s < vr_agp_tw + vr_agp_ts + vr_agp_th;
    if(inst.wi_clip > 0)
    {
        inst.wi_clip = max(0, inst.wi_clip - frametime * (cvar_hget(cvarh_vr_chainsaw_idle_fuel_use) +
                                                          (revving ? cvar_hget(cvarh_vr_chainsaw_fuel_use) : 0)));
    }
    float want = inst.wi_clip <= 0 ? VR_SAW_LOOP_NONE : revving ? VR_SAW_LOOP_RUN : VR_SAW_LOOP_IDLE;
    if(cutting && inst.wi_clip > 0 && time >= g.vr_ag_saw_next && g.vr_ag_act_hit != 2) // (2: a test holding the pose)
    {
        g.vr_ag_saw_next = time + VR_SAW_TICK;
        entity oldself = self;
        self = g;
        self.handpos = p.origin;
        self.muzzlepos = weaponmuzzle(p);
        self.weapon = WID_CHAINSAW;
        self.weaponflags = p.weaponflags | QVR_WPNFLAG_SAW_RUNNING;
        self.weaponinst = inst;
        makevectors(p.v_angle);
        if(VR_Chainsaw_Cut(cVR_MainHand))
        {
            want = VR_SAW_LOOP_CUT;
            g.vr_ag_saw_snd = g.vr_ag_saw_snd == VR_SAW_LOOP_CUT ? VR_SAW_LOOP_CUT : -1; // (the cut's loop, at once)
            vr_agp_events = vr_agp_events + 1;
            self.vr_ag_strikes = self.vr_ag_strikes + 1;
        }
        self.weaponinst = world;
        self = oldself;
    }
    else if(g.vr_ag_saw_snd == VR_SAW_LOOP_CUT && cutting)
    {
        want = VR_SAW_LOOP_CUT; // (between ticks)
    }
    if(want != g.vr_ag_saw_snd)
    {
        if(want == VR_SAW_LOOP_NONE && g.vr_ag_saw_snd > 0)
        {
            sound(p, CHAN_WEAPON, "vr/saw_stall.wav", 1, ATTN_NORM);
        }
        else if(want != VR_SAW_LOOP_NONE)
        {
            sound(p, CHAN_WEAPON, want == VR_SAW_LOOP_CUT ? "vr/saw_cut.wav" : want == VR_SAW_LOOP_RUN ? "vr/saw_run.wav" :
                "vr/saw_idle.wav", 1, ATTN_NORM);
        }
        g.vr_ag_saw_snd = want;
    }
    if(want != VR_SAW_LOOP_NONE && time >= g.vr_ag_saw_puff)
    {
        g.vr_ag_saw_puff = time + (revving ? 0.12 : 0.35);
        vector at = modelpoint(p, p.angles, VR_SAW_EXHAUST);
        particle2(p.origin + at, '0 0 20', QVR_PARTICLE_PRESET_SAWSMOKE, 1);
    }
};

// A gun `p`'s turn: carried along `gx` (its up `gz`), aimed as its holder aims (vr_ag_aimw, vr_ag_aimdir), a shot's kick
// (fired at `kickAt`) back and up, and what is left of a heavy gun's kick up in one hand (VR_AG_KickLeft: dual wielding).
// Out: vr_agp_gfw, vr_agp_gup (world), vr_agp_gk (the kick's move, world).
vector vr_agp_gfw;
vector vr_agp_gup;
vector vr_agp_gk;
void(entity g, entity p, vector gx, vector gz, float kickAt) VR_AGP_GunTurn =
{
    float a = VR_AGP_Smooth(g.vr_ag_aimw);
    vector fw = gx;
    if(a > 0 && g.vr_ag_aimdir != '0 0 0')
    {
        fw = VR_AGP_Nlerp(gx, g.vr_ag_aimdir, a);
    }
    vector up = normalize(gz - fw * (gz * fw));
    if(up == '0 0 0') { up = '0 0 1'; }
    vector right = VR_AGP_Cross(fw, up);
    float k = 1 - (time - kickAt) / 0.2;
    vector kick = '0 0 0';
    if(kickAt > 0 && k > 0)
    {
        float big = VR_AG_Class(p.weapon) == VR_AG_CLASS_EXPLOSIVE ? 1.5 : 1;
        float kk = k * k * big;
        kick = -fw * (4 * kk) + '0 0 1' * kk;
        fw = VR_AG_Rotate(fw, right, 14 * kk);
        up = VR_AG_Rotate(up, right, 14 * kk);
    }
    float hk = VR_AG_KickLeft(p);
    if(hk > 0)
    {
        fw = VR_AG_Rotate(fw, right, hk);
        up = VR_AG_Rotate(up, right, hk);
    }
    vr_agp_gfw = fw;
    vr_agp_gup = up;
    vr_agp_gk = kick;
};

// A gun: carried along its animation's rifle line; aimed (VR_AG_AtkFrame sets vr_ag_aim), turned at its target
// (eased); a shot's kick back and up. Two guns (`pL` the left's: dual wielding, ROUND22.md "Armed monsters: dual-wielded
// guns"): each in one hand, the rifle line's pitch straight ahead (no hand across to hold it), the left one at the right
// hand mirrored (VR_AGP_LEFTMIRROR, made whole in VR_AGP_Pose), each kicked by its own shots.
void(entity g, entity p, entity pL, vector gx, vector gz) VR_AGP_Gun =
{
    if(g.vr_ag_aim != '0 0 0')
    {
        g.vr_ag_aimdir = g.vr_ag_aim;
    }
    g.vr_ag_aimw = VR_AGP_Ease(g.vr_ag_aimw, g.vr_ag_aim != '0 0 0' ? 1 : 0, g.vr_ag_aim != '0 0 0' ? 7 : 4);
    float a = VR_AGP_Smooth(g.vr_ag_aimw);
    vector lx = '1 0 0';
    vector lz = '0 0 1';
    if(pL)
    {
        VR_AGP_Body(g);
        lx = VR_AGP_ToLocal(gx);
        lx_y = 0;
        lx = normalize(lx);
        if(lx == '0 0 0') { lx = '1 0 0'; }
        lz = VR_AGP_ToLocal(gz);
        gx = VR_AGP_ToWorld(lx);
        gz = VR_AGP_ToWorld(lz);
    }
    VR_AGP_GunTurn(g, p, gx, gz, pL ? p.vr_ag_kick_at : g.vr_ag_kick_at);
    VR_AGP_Body(g);
    vr_agp_wr = 1;
    vr_agp_v = VR_AGP_ToLocal(vr_agp_gk);
    vr_agp_fw = vr_agp_gfw;
    vr_agp_up = vr_agp_gup;
    vr_agp_wo = 1 - a;
    if(pL)
    {
        VR_AGP_GunTurn(g, pL, VR_AGP_ToWorld(VR_AGP_Mirror(lx)), VR_AGP_ToWorld(VR_AGP_Mirror(lz)), pL.vr_ag_kick_at);
        VR_AGP_Body(g);
        vr_agp_fwl = vr_agp_gfw;
        vr_agp_upl = vr_agp_gup;
        vr_agp_wl = 1;
        vr_agp_vl = VR_AGP_ToLocal(vr_agp_gk);
        vr_agp_flags = VR_AGP_LEFT | VR_AGP_LEFTWPN | VR_AGP_LEFTMIRROR;
    }
};

// ----------------------------------------------------------------------------
// vr_agarms (the engine's vr_agarms.cpp; ROUND22.md, "Armed humanoids' arms: posed and skinned"): each weapon held as a
// player holds it. The weapon entity's frame is the hand's, as your controller's is to the weapon you hold (its Weapon
// Offsets draw the weapon off it), so its barrel is the hand's forward and its points (its muzzle or tip, its butt, its
// second grip) are where your hand finds them. The poses are its body's own (not its animation's hands): placed from
// its chest's anchor vertex as drawn (VR_AGP_ANCHOR), the engine's IK bending its arms onto them.
// ----------------------------------------------------------------------------
void(entity p) VR_AGP_Geo =
{
    if(p.vr_agp_geo)
    {
        return;
    }
    p.vr_agp_geo = 1;
    p.vr_agp_muz = weapondrawnpose(p, 0, 8);
    p.vr_agp_butt = weapondrawnpose(p, 0, 9);
    p.vr_agp_sec = weapondrawnpose(p, 0, 10);
    VR_AG_Log(sprintf("agarms: %s muzzle %v butt %v second %v", p.netname, p.vr_agp_muz, p.vr_agp_butt, p.vr_agp_sec));
};

// Held shouldered (a long gun): a second grip, or a stock well behind the hand.
float(entity p) VR_AGP_Long =
{
    VR_AGP_Geo(p);
    return (p.vr_agp_sec != '0 0 0' && p.vr_agp_sec_z < 999) || p.vr_agp_butt_x < -7;
};

vector(entity g) VR_AM_Anchor =
{
    VR_AM_Row(g.vr_ag_kind);
    return modelvertex(g, vr_am_anchor);
};

// `v` turned by the turn taking unit `a` to unit `b` the shortest way.
vector(vector v, vector a, vector b) VR_AGP_Arc =
{
    vector k = VR_AGP_Cross(a, b);
    float s = vlen(k);
    float c = a * b;
    if(s < 0.00001)
    {
        if(c > 0)
        {
            return v;
        }
        k = VR_AGP_Cross(a, fabs(a_z) < 0.9 ? '0 0 1' : '1 0 0');
        return VR_AG_Rotate(v, normalize(k), 180);
    }
    return VR_AG_Rotate(v, k * (1 / s), atan2(s, c) * (180 / 3.14159265));
};

// vr_agrunt_lift_turn: the frame (fw0, up0) turned towards (fw1, up1) by share `u`, the short way: its line swung onto
// fw1's about their common normal, then rolled about it, each the shorter way round (the two axes blended apart spun
// a weapon taken up lying on its side through 160 degrees). Out: vr_agp_sfw, vr_agp_sup (unit, square).
vector vr_agp_sfw;
vector vr_agp_sup;
void(vector fw0, vector up0, vector fw1, vector up1, float u) VR_AGP_TurnFrame =
{
    fw0 = normalize(fw0);
    fw1 = normalize(fw1);
    vector k = VR_AGP_Cross(fw0, fw1);
    float s = vlen(k);
    float th = atan2(s, fw0 * fw1) * (180 / 3.14159265);
    if(s < 0.00001)
    {
        k = VR_AGP_Cross(fw0, fabs(fw0_z) < 0.9 ? '0 0 1' : '1 0 0');
        s = vlen(k);
    }
    k = k * (1 / s);
    vector upArc = VR_AG_Rotate(up0, k, th);
    upArc = normalize(upArc - fw1 * (upArc * fw1));
    vector u1 = normalize(up1 - fw1 * (up1 * fw1));
    float r = atan2(VR_AGP_Cross(upArc, u1) * fw1, upArc * u1) * (180 / 3.14159265);
    vr_agp_sfw = normalize(VR_AG_Rotate(fw0, k, th * u));
    vector up = VR_AG_Rotate(VR_AG_Rotate(up0, k, th * u), vr_agp_sfw, r * u);
    vr_agp_sup = normalize(up - vr_agp_sfw * (up * vr_agp_sfw));
};

// vr_agarms: a chainsaw's hold (VR_AGP_Melee: `rdy` 0 carried, 1 ready): its rear handle where its own rifle's grip is,
// turned with the bar as the long guns' low carry turns it (VR_AGP_ArmsGun): across its body and down, so its front
// handle (on top, ahead) is in the off hand's reach; ready, the bar raised level and more ahead.
vector VR_AGP_SAW_RDY_FW = '0.88 0.45 0.1';
void(entity g, float rdy) VR_AGP_SawHold =
{
    VR_AM_Row(g.vr_ag_kind);
    vector g0 = normalize(vr_am_gun_fw);
    vector d = rdy ? normalize(VR_AGP_SAW_RDY_FW) : normalize(g0 + '0.5 0 -0.25');
    vector at = vr_am_sh_r + VR_AGP_Arc(vr_am_grip_r - vr_am_sh_r, g0, d);
    // (VR_AGP_Melee's places are fitted to its body by VR_AM_Fit: undone here)
    vr_agp_sawat_x = (at_x - vr_am_fit_shift_x) / vr_am_fit_scale_x;
    vr_agp_sawat_y = (at_y - vr_am_fit_shift_y) / vr_am_fit_scale_y;
    vr_agp_sawat_z = (at_z - vr_am_fit_shift_z) / vr_am_fit_scale_z;
    vr_agp_sawfw = d;
};

// A chainsaw's grip `at` (its body's space as it is: VR_AM_Row's) with the bar along `fw`, brought in towards its left
// shoulder as far as its front handle (its second grip, on top) is past VR_AGP_SAW_REACH of its arm from it: the off hand
// holds it (the engine takes a second grip within vr_agarms_second of the arm; else it slides back along the bar).
float VR_AGP_SAW_REACH = 0.9;
float VR_AGP_SAW_REACH_R = 0.92;
vector(entity p, vector at, vector fw) VR_AGP_SawReach =
{
    VR_AGP_Geo(p);
    vector sec = p.vr_agp_sec;
    if(sec == '0 0 0' || sec_z >= 999)
    {
        return at;
    }
    fw = normalize(fw);
    vector up = normalize('0 0 1' - fw * fw_z);
    vector side = VR_AGP_Cross(up, fw);
    vector off = fw * sec_x + side * sec_y + up * sec_z;
    float most = VR_AGP_SAW_REACH * vr_am_arm;
    // (and its rear handle within VR_AGP_SAW_REACH_R of the right arm: a thrust's grip had been out of the right hand's
    // reach and its front handle out of the left's in half its frames; both kept, a few rounds)
    float rounds = cvar_hget(cvarh_vr_agrunt_saw_reach) ? 3 : 1;
    for(float i = 0; i < rounds; i++)
    {
        vector h = at + off;
        float d = vlen(h - vr_am_sh_l);
        if(d > most)
        {
            at = at - normalize(h - vr_am_sh_l) * (d - most);
        }
        if(rounds > 1)
        {
            float mr = VR_AGP_SAW_REACH_R * vr_am_arm;
            float dr = vlen(at - vr_am_sh_r);
            if(dr > mr)
            {
                at = vr_am_sh_r + (at - vr_am_sh_r) * (mr / dr);
            }
        }
    }
    return at;
};

// A weapon's frame (vr_agp_fw its x, vr_agp_up its z) turning its own unit `t` onto `b`, its own `r` as near `hint` as
// that allows (a blade's tip onto its line, the fist's forward towards its edge's way; a barrel onto its aim, its up
// level). Any space, as `b` and `hint` are.
void(vector t, vector b, vector r, vector hint) VR_AGP_Orient =
{
    b = normalize(b);
    vector x0 = VR_AGP_Arc('1 0 0', t, b);
    vector z0 = VR_AGP_Arc('0 0 1', t, b);
    vector r0 = VR_AGP_Arc(r, t, b);
    vector rp = r0 - b * (r0 * b);
    vector hp = hint - b * (hint * b);
    if(vlen(rp) > 0.001 && vlen(hp) > 0.001)
    {
        rp = normalize(rp);
        hp = normalize(hp);
        float ang = atan2(VR_AGP_Cross(rp, hp) * b, rp * hp) * (180 / 3.14159265);
        x0 = VR_AG_Rotate(x0, b, ang);
        z0 = VR_AG_Rotate(z0, b, ang);
    }
    vr_agp_fw = normalize(x0);
    vr_agp_up = normalize(z0 - vr_agp_fw * (z0 * vr_agp_fw));
};

// A point of a weapon's own frame (vr_agp_fw, vr_agp_up: its x and z) in the frame's space.
vector(vector l) VR_AGP_InFrame = { return vr_agp_fw * l_x + VR_AGP_Cross(vr_agp_up, vr_agp_fw) * l_y + vr_agp_up * l_z; };

// Its run's sway now (its body's space: a bob up and down each step), eased in and out with its run's frames.
vector(entity g) VR_AGP_RunBob =
{
    VR_AM_Row(g.vr_ag_kind);
    float running = g.frame >= vr_am_f_run && g.frame < vr_am_f_run + 8;
    g.vr_agp_runw = VR_AGP_Ease(g.vr_agp_runw, running ? 1 : 0, 4);
    float ph = time * 1.25 * 6.2832;
    return '0 0 1' * (0.8 * g.vr_agp_runw * sin(ph * 2)) + '0 1 0' * (0.6 * g.vr_agp_runw * sin(ph));
};

// A melee weapon's frame from its blade's line `fw` and its edge's way `edge` (VR_AGP_Melee's, VR_AGPar_Pose's, any
// space): its tip on the line, the fist's forward towards the edge's way (the edge leads as the knuckles do).
void(entity p, vector fw, vector edge) VR_AGP_BladeHand =
{
    VR_AGP_Geo(p);
    vector t = vlen(p.vr_agp_muz) > 1 ? normalize(p.vr_agp_muz) : '1 0 0';
    VR_AGP_Orient(t, fw, '1 0 0', edge);
};

// The hand hanging free (its grip: a fist's point), its body's space at rest.
vector(float left) VR_AGP_Hang =
{
    vector sh = left ? vr_am_sh_l : vr_am_sh_r;
    vector d = normalize(left ? '0.3 0.15 -1' : '0.3 -0.15 -1');
    return sh + d * (0.8 * vr_am_arm + 2.5); // (its elbow bent as at rest)
};

// vr_agarms: a gun (or two) held as a player holds it, its body's space at rest; out: vr_agp_v, vr_agp_fw, vr_agp_up
// (world), and the left's (vr_agp_vl, vr_agp_fwl, vr_agp_upl, flags) for a second gun or a reload's hand.
// - Long guns shouldered: the butt in the shoulder's pocket, the barrel (the hand's forward) on the target through the
//   muzzle; carried at a low ready (its muzzle down and across) when not aiming.
// - One-handed guns, and each of two: out from its shoulder towards the target, lowered by its side when carried.
// - Its kick (back and up), a heavy gun's kick left in one hand, a reload's tilt (the off hand to its belt and back).
vector vr_agp_gunat; // (VR_AGP_ArmsOne's out: the grip)
void(entity g, entity p, float left, float a, vector tl) VR_AGP_ArmsOne =
{
    VR_AGP_Geo(p);
    vector sh = left ? vr_am_sh_l : vr_am_sh_r;
    float reach = vr_am_arm + 2;
    vector side = left ? '0 1 0' : '0 -1 0';
    vector sh2 = sh - '0 0 3' * (vr_am_arm / 16.7); // (the gun's sights at its eyes' line, the hand below them)
    vector hold = sh2 + normalize(tl - sh2) * (0.85 * reach);
    vector low = sh + normalize('0.35 0 -0.94' + side * 0.15) * (0.9 * reach);
    if(cvar_hget(cvarh_vr_agarms_arm_clear))
    {
        // (vr_agarms_arm_clear: carried out from its side as the rest pose's arms hang, 19 degrees, and within its
        // reach: its upper arm clear of its torso with the gun in its hand, not pulled straight against it)
        low = sh + normalize('0.35 0 -0.94' + side * 0.35) * (0.8 * reach);
    }
    vector at = VR_AGP_Lerp(low, hold, a);
    vector d = normalize(VR_AGP_Lerp(normalize('0.75 0 -0.65' + side * 0.1), normalize(tl - at), a));
    VR_AGP_Orient('1 0 0', d, '0 0 1', '0 0 1');
    if(a > 0)
    {
        // (the barrel through its muzzle onto the target)
        d = normalize(VR_AGP_Lerp(d, normalize(tl - (at + VR_AGP_InFrame(p.vr_agp_muz))), a));
        VR_AGP_Orient('1 0 0', d, '0 0 1', '0 0 1');
    }
    vr_agp_gunat = at;
};

float vr_agp_onehand; // VR_AGP_ArmsGun: its gun in one hand (a blade in the other: vr_ag_gunaxe)
void(entity g, entity p, entity pL) VR_AGP_ArmsGun =
{
    VR_AM_Row(g.vr_ag_kind);
    if(g.vr_ag_aim != '0 0 0')
    {
        g.vr_ag_aimdir = g.vr_ag_aim;
    }
    g.vr_ag_aimw = VR_AGP_Ease(g.vr_ag_aimw, g.vr_ag_aim != '0 0 0' ? 1 : 0, g.vr_ag_aim != '0 0 0' ? 7 : 4);
    float a = VR_AGP_Smooth(g.vr_ag_aimw);
    VR_AGP_Body(g);
    // Where it aims (its body's space at rest): its aim's line from its gun now, else its target's point, else ahead.
    vector tl = g.enemy ? VR_AGP_TargetLocal(g) : '200 0 10';
    if(g.vr_ag_aimdir != '0 0 0' && p)
    {
        tl = VR_AGP_ToLocal(p.origin - g.origin) + VR_AGP_ToLocal(g.vr_ag_aimdir) * max(64, vlen(tl));
    }
    // (its gun turned at most 30 degrees off its body's way: further, its arms would cross its chest; its body turns to
    // its target anyway, and its shots go along its aim)
    float fl = vlen([tl_x, tl_y, 0]);
    if(fl > 1 && tl_x < fl * 0.866)
    {
        float ty = tl_y > 0 ? 0.5 : -0.5;
        tl_x = fl * 0.866;
        tl_y = fl * ty;
    }
    float scale = vr_am_arm / 16.7;
    vector gR;
    vector fwR;
    vector upR;
    if(pL || vr_agp_onehand || !VR_AGP_Long(p))
    {
        VR_AGP_ArmsOne(g, p, 0, a, tl);
        gR = vr_agp_gunat;
    }
    else
    {
        // A long gun: carried as its own rifle is at rest (its rest pose's hold turned with the gun: its arms and wrists
        // as the artist made them), raised to its shoulder as it aims by vr_agarms_shoulder (its butt in the pocket before
        // the shoulder, the barrel on the target; your guns have next to no stock behind the hand: a stock half its arm
        // long taken, so its hand is out before its shoulder, a rifleman's).
        vector g0 = normalize(vr_am_gun_fw);
        vector butt = p.vr_agp_butt;
        butt_x = min(butt_x, -0.5 * vr_am_arm);
        vector pocket = vr_am_sh_r + '1.5 2.5 -1.5' * scale;
        float sh = a * min(1, max(0, cvar_hget(cvarh_vr_agarms_shoulder)));
        vector d = normalize(VR_AGP_Lerp(normalize(g0 + '0.5 0 -0.25'), normalize(tl - VR_AGP_Lerp(vr_am_grip_r, pocket, sh)), a));
        VR_AGP_Orient('1 0 0', d, '0 0 1', '0 0 1');
        gR = VR_AGP_Lerp(vr_am_sh_r + VR_AGP_Arc(vr_am_grip_r - vr_am_sh_r, g0, d), pocket - VR_AGP_InFrame(butt), sh);
        if(a > 0)
        {
            d = normalize(VR_AGP_Lerp(d, normalize(tl - (gR + VR_AGP_InFrame(p.vr_agp_muz))), a));
            VR_AGP_Orient('1 0 0', d, '0 0 1', '0 0 1');
            gR = VR_AGP_Lerp(vr_am_sh_r + VR_AGP_Arc(vr_am_grip_r - vr_am_sh_r, g0, d), pocket - VR_AGP_InFrame(butt), sh);
        }
        // (a long butt: the hand brought within its reach, the butt past the shoulder)
        vector fromSh = gR - vr_am_sh_r;
        float most = 0.92 * (vr_am_arm + 2.5);
        if(vlen(fromSh) > most)
        {
            gR = vr_am_sh_r + normalize(fromSh) * most;
        }
    }
    fwR = vr_agp_fw;
    upR = vr_agp_up;
    // Its shot's kick: back and up about its right, fading over 0.2 s; a heavy gun's kick left (one hand).
    float k = 1 - (time - (pL ? p.vr_ag_kick_at : g.vr_ag_kick_at)) / 0.2;
    vector right = VR_AGP_Cross(fwR, upR);
    if(k > 0 && (pL ? p.vr_ag_kick_at : g.vr_ag_kick_at) > 0)
    {
        float kk = k * k * (VR_AG_Class(p.weapon) == VR_AG_CLASS_EXPLOSIVE ? 1.5 : 1);
        gR = gR - fwR * (3 * kk) + '0 0 1' * kk;
        fwR = VR_AG_Rotate(fwR, right, 12 * kk);
        upR = VR_AG_Rotate(upR, right, 12 * kk);
    }
    float hk = VR_AG_KickLeft(p);
    if(hk > 0)
    {
        fwR = VR_AG_Rotate(fwR, right, hk);
        upR = VR_AG_Rotate(upR, right, hk);
    }
    // A reload: the gun lowered and rolled in, the off hand to its belt and back to the gun.
    float rl = g.vr_agp_rl_at > 0 ? (time - g.vr_agp_rl_at) / max(0.1, cvar_hget(cvarh_vr_agrunt_reload_time)) : -1;
    if((rl >= 0) && (rl <= 1) && (!pL))
    {
        float s = sin(rl * 3.14159265);
        fwR = VR_AG_Rotate(fwR, right, -25 * s);
        upR = VR_AG_Rotate(upR, right, -25 * s);
        upR = VR_AG_Rotate(upR, fwR, -30 * s);
        gR = gR + '1 2 -1' * s;
        vector belt = vr_am_sh_l + '3 -4 -15' * scale;
        vr_agp_fw = fwR;
        vr_agp_up = upR;
        vector magL = p.vr_agp_muz * 0.4;
        if((p.vr_agp_sec != '0 0 0') && (p.vr_agp_sec_z < 999))
        {
            magL = p.vr_agp_sec;
        }
        vector mag = gR + VR_AGP_InFrame(magL);
        float u = rl < 0.4 ? VR_AGP_Smooth(rl / 0.4) : 1 - VR_AGP_Smooth((rl - 0.4) / 0.45);
        vr_agp_flags = vr_agp_flags | VR_AGP_LEFT;
        vr_agp_wl = 1;
        vr_agp_vl = VR_AGP_Lerp(mag, belt, u) - vr_am_anchor_at;
    }
    vr_agp_wr = 1;
    vr_agp_v = gR - vr_am_anchor_at;
    vr_agp_fw = VR_AGP_ToWorld(fwR);
    vr_agp_up = VR_AGP_ToWorld(upR);
    vr_agp_wo = 0;
    if(pL)
    {
        VR_AGP_ArmsOne(g, pL, 1, a, tl);
        vector gL = vr_agp_gunat;
        vector fwL = vr_agp_fw;
        vector upL = vr_agp_up;
        float kl = 1 - (time - pL.vr_ag_kick_at) / 0.2;
        if(kl > 0 && pL.vr_ag_kick_at > 0)
        {
            vector rightL = VR_AGP_Cross(fwL, upL);
            float kkl = kl * kl;
            gL = gL - fwL * (3 * kkl) + '0 0 1' * kkl;
            fwL = VR_AG_Rotate(fwL, rightL, 12 * kkl);
            upL = VR_AG_Rotate(upL, rightL, 12 * kkl);
        }
        vr_agp_fwl = VR_AGP_ToWorld(fwL);
        vr_agp_upl = VR_AGP_ToWorld(upL);
        vr_agp_fw = VR_AGP_ToWorld(fwR);
        vr_agp_up = VR_AGP_ToWorld(upR);
        vr_agp_wl = 1;
        vr_agp_vl = gL - vr_am_anchor_at;
        vr_agp_flags = VR_AGP_LEFT | VR_AGP_LEFTWPN;
    }
};

void(vector d) VR_AGP_WriteDir =
{
    WriteShort(MSG_BROADCAST, rint(d_x * 32000));
    WriteShort(MSG_BROADCAST, rint(d_y * 32000));
    WriteShort(MSG_BROADCAST, rint(d_z * 32000));
};

void(vector v) VR_AGP_WriteVec =
{
    WriteCoord(MSG_BROADCAST, v_x);
    WriteCoord(MSG_BROADCAST, v_y);
    WriteCoord(MSG_BROADCAST, v_z);
};

// ----------------------------------------------------------------------------
// The body in a strike (vr_agrunt_strike_*; ROUND22.md, "Armed monsters: the body in their strikes")
// ----------------------------------------------------------------------------
// Wound up (the torso twisted back, the striking shoulder up and back, its hand drawn back by the head, its elbow
// raised), driven through the blow (the torso twisting and leaning the other way, the shoulder forward, the arm
// extending), followed through and recovered. The torso's turn (yaw, pitch: leaning forward, roll: bent to its right;
// degrees, about its waist) carries both hands' targets and the weapons (the arms ride it: the client turns the rig's
// chest by the same, its hips and head by their shares, before the arms' IK). Both shares pass through nothing as the
// blow is struck and the blow is worked out from the pose without them: the blow's frame, its line, its parry and damage
// are the arms-only pose's.

float vr_agp_ww;     // the wind-up's share: 1 at its end, back to 0 by the blow
float vr_agp_wf;     // the follow-through's: from the blow, 1 at the strike's end, back to 0 over the recovery
vector vr_agp_tq;    // the torso's turn now: yaw, pitch, roll (degrees)
vector vr_agp_drawr; // the hands drawn back in the wind-up (its body's space, before the turn)
vector vr_agp_drawl;
vector vr_agp_elr;   // the elbows' poles (its body's space; their length the share: 0 the animation's)
vector vr_agp_ell;
vector VR_AGP_WAIST = '-4.7 0.9 4.8'; // the grunt's waist (its rig's pelvis's end: the chest's turn's pivot)

// The shares at `g`'s attack's time now (its strike's parameter as its pose's: a swing's smoothed, a punch's not).
void(entity g) VR_AGP_Phase =
{
    VR_AGP_Times(g);
    float s = time - g.vr_ag_act_at;
    float tS = vr_agp_tw + vr_agp_ts;
    float punch = g.vr_ag_act == VR_AGP_PUNCH;
    float follow = g.vr_ag_act == VR_AGP_SAW ? 0 : 1; // (the saw's bar held still in its cut)
    float h = punch ? 0.8 : 0.55;                      // (the blow: VR_AGP_Fists', VR_AGP_Melee's)
    vr_agp_ww = 0;
    vr_agp_wf = 0;
    if(s < vr_agp_tw)
    {
        vr_agp_ww = VR_AGP_Smooth(s / vr_agp_tw);
    }
    else if(s < tS)
    {
        float u = (s - vr_agp_tw) / vr_agp_ts;
        if(!punch)
        {
            u = VR_AGP_Smooth(u);
        }
        float pe = punch && cvar_hget(cvarh_vr_agrunt_punch_ease); // (vr_agrunt_punch_ease: the body's turn eased too)
        if(u < h)
        {
            vr_agp_ww = pe ? 1 - VR_AGP_Smooth(u / h) : cos(1.5708 * u / h); // (fastest as it passes the blow)
        }
        else
        {
            vr_agp_wf = follow * (pe ? VR_AGP_Smooth((u - h) / (1 - h)) : sin(1.5708 * (u - h) / (1 - h)));
        }
    }
    else if(s < tS + vr_agp_th)
    {
        vr_agp_wf = follow;
    }
    else
    {
        vr_agp_wf = follow * (1 - VR_AGP_Smooth((s - tS - vr_agp_th) / max(vr_agp_tr, 0.01)));
    }
};

// `g`'s body now (out: vr_agp_tq, vr_agp_drawr/l, vr_agp_elr/l). False: no attack, nothing turned.
float(entity g) VR_AGP_TorsoNow =
{
    vr_agp_tq = '0 0 0';
    vr_agp_drawr = '0 0 0';
    vr_agp_drawl = '0 0 0';
    vr_agp_elr = '0 0 0';
    vr_agp_ell = '0 0 0';
    float act = g.vr_ag_act;
    if(act != VR_AGP_PUNCH && act != VR_AGP_SWING && act != VR_AGP_SAW)
    {
        return FALSE;
    }
    float torso = cvar_hget(cvarh_vr_agrunt_strike_torso);
    float shoulder = cvar_hget(cvarh_vr_agrunt_strike_shoulder);
    if(torso <= 0 && shoulder <= 0)
    {
        return FALSE;
    }
    VR_AGP_Phase(g);
    // Each kind's turns wound up (w) and followed through (f) as the right hand strikes (yaw, pitch, roll); its hand's
    // draw back and its elbow's pole in the wind-up. The left's mirrored.
    vector w; vector f; vector draw; vector elbow;
    float left = FALSE;
    float both = FALSE;
    float kind = g.vr_ag_act_kind;
    if(act == VR_AGP_PUNCH)
    {
        left = (kind == 0 || kind == 2);
        if(kind == 0)      { w = '-6 -1 -1';  f = '12 4 2';   draw = '-2 0 1';  elbow = '-1 -0.3 -0.5'; } // a jab
        else if(kind == 1) { w = '-16 -3 -3'; f = '26 6 4';   draw = '-4 0 1';  elbow = '-1 -0.4 -0.3'; } // a cross
        else if(kind == 4) { w = '-12 8 5';   f = '16 -7 -4'; draw = '-2 0 -4'; elbow = '-0.6 -0.3 -1'; } // an uppercut
        else               { w = '-22 0 -4';  f = '30 4 5';   draw = '-2 -3 1'; elbow = '-0.3 -1 0.5';  } // a hook
    }
    else if(act == VR_AGP_SAW)
    {
        w = '-14 -5 -2'; f = '0 0 0'; draw = '-4 0 0'; elbow = '-1 -0.5 -0.3';
    }
    else
    {
        if(kind == 0)      { w = '-14 -9 -7'; f = '12 15 6';   draw = '0 0 0';   elbow = '-0.4 -0.7 0.8'; } // overhead (raised high already)
        else if(kind == 1) { w = '-28 -5 -8'; f = '24 10 8';   draw = '-5 -1 4'; elbow = '-0.5 -0.8 0.6'; } // diagonal
        else if(kind == 2) { w = '-32 0 -3';  f = '28 4 3';    draw = '-5 -2 1'; elbow = '-0.6 -0.8 0.3'; } // horizontal
        else               { w = '16 4 5';    f = '-20 -5 -6'; draw = '3 3 1';   elbow = '0.3 -0.6 0.5';  } // backhand (its hand out, off the chest)
        if(g.vr_ag_prop && VR_AGP_TwoHanded(g.vr_ag_prop))
        {
            w = w * 1.15;
            f = f * 1.15;
        }
        if(g.vr_ag_prop2)
        {
            left = g.vr_ag_act_hand == 1;
            both = g.vr_ag_act_hand == 2; // (a scissor: the twists cancel, the lean stays)
        }
    }
    // Each attack its own: the twist and the lean apart
    float v = g.vr_ag_act_var;
    float a = 0.8 + 0.4 * v;
    float v2 = v * 7.31;
    float b = 0.8 + 0.4 * (v2 - floor(v2));
    float side = left ? -1 : 1;
    float twist = both ? 0 : 1;
    vr_agp_tq_x = (w_x * vr_agp_ww + f_x * vr_agp_wf) * a * torso * side * twist;
    vr_agp_tq_y = (w_y * vr_agp_ww + f_y * vr_agp_wf) * b * torso;
    vr_agp_tq_z = (w_z * vr_agp_ww + f_z * vr_agp_wf) * b * shoulder * side * twist;
    draw = draw * (vr_agp_ww * shoulder * b);
    elbow = normalize(elbow) * (0.8 * vr_agp_ww * min(1, shoulder));
    if(!left || both)
    {
        vr_agp_drawr = draw;
        vr_agp_elr = elbow;
    }
    if(left || both)
    {
        vr_agp_drawl = VR_AGP_Mirror(draw);
        vr_agp_ell = VR_AGP_Mirror(elbow);
    }
    return TRUE;
};

// A direction in its body's space turned by vr_agp_tq (roll about x, pitch about y, yaw about z: the client's chest turn).
vector(vector d) VR_AGP_TurnDir =
{
    float r = vr_agp_tq_z * (3.14159265 / 180);
    float p = vr_agp_tq_y * (3.14159265 / 180);
    float y = vr_agp_tq_x * (3.14159265 / 180);
    vector o;
    o_x = d_x;
    o_y = d_y * cos(r) - d_z * sin(r);
    o_z = d_y * sin(r) + d_z * cos(r);
    d = o;
    o_x = d_x * cos(p) + d_z * sin(p);
    o_z = -d_x * sin(p) + d_z * cos(p);
    d = o;
    o_x = d_x * cos(y) - d_y * sin(y);
    o_y = d_x * sin(y) + d_y * cos(y);
    return o;
};

// `g`'s pose (vr_agp_*: computed, its right grip `posL`) carried by its torso's turn now: the hands' targets drawn back,
// turned about its waist; the weapons' turns. Its right grip as turned (vr_agp_v moved by as much: the client's target is
// its drawn hand, or its chest's anchor with vr_agarms, `handL`, times wr plus v; the left's the same with wl and vl).
vector(entity g, vector posL, vector handL) VR_AGP_TurnPose =
{
    vector piv = VR_AM_Fit(g, VR_AGP_WAIST);
    vector o = VR_AM_Fit(g, '0 0 0');
    float fr = 1;
    float fl = 1;
    if(vr_agp_arms)
    {
        // (the draw back fading out as the arms-only pose's hand nears its arm's reach: its wrist kept in reach)
        VR_AM_Row(g.vr_ag_kind);
        vector mv = handL - vr_am_anchor_at;
        fr = min(1, max(0, (0.85 * vr_am_arm - vlen(posL - (vr_am_sh_r + mv))) / (0.2 * vr_am_arm)));
        fl = min(1, max(0, (0.85 * vr_am_arm - vlen(handL * vr_agp_wl + vr_agp_vl - (vr_am_sh_l + mv))) / (0.2 * vr_am_arm)));
    }
    vector pr = posL + (VR_AM_Fit(g, vr_agp_drawr) - o) * fr;
    pr = piv + VR_AGP_TurnDir(pr - piv);
    vector pl = handL * vr_agp_wl + vr_agp_vl + (VR_AM_Fit(g, vr_agp_drawl) - o) * fl;
    pl = piv + VR_AGP_TurnDir(pl - piv);
    if(vr_agp_arms)
    {
        // (the hands drawn back never further from their shoulders, turned with it, than the arms-only pose has them:
        // the draw moves them back and up, never out of its arms' reach; vr_agarms' wrists reach as before)
        VR_AM_Row(g.vr_ag_kind);
        vector moved = handL - vr_am_anchor_at;
        float far = vlen(posL - (vr_am_sh_r + moved));
        vector sh = piv + VR_AGP_TurnDir(vr_am_sh_r + moved - piv);
        if(vlen(pr - sh) > far)
        {
            pr = sh + normalize(pr - sh) * far;
        }
        far = vlen(handL * vr_agp_wl + vr_agp_vl - (vr_am_sh_l + moved));
        sh = piv + VR_AGP_TurnDir(vr_am_sh_l + moved - piv);
        if((vr_agp_flags & VR_AGP_LEFT) && vlen(pl - sh) > far)
        {
            pl = sh + normalize(pl - sh) * far;
        }
    }
    vr_agp_v = vr_agp_v + (pr - posL);
    vr_agp_vl = pl - handL * vr_agp_wl;
    VR_AGP_Body(g);
    vr_agp_fw = VR_AGP_ToWorld(VR_AGP_TurnDir(VR_AGP_ToLocal(vr_agp_fw)));
    vr_agp_up = VR_AGP_ToWorld(VR_AGP_TurnDir(VR_AGP_ToLocal(vr_agp_up)));
    if(vr_agp_flags & VR_AGP_LEFTWPN)
    {
        vr_agp_fwl = VR_AGP_ToWorld(VR_AGP_TurnDir(VR_AGP_ToLocal(vr_agp_fwl)));
        vr_agp_upl = VR_AGP_ToWorld(VR_AGP_TurnDir(VR_AGP_ToLocal(vr_agp_upl)));
    }
    return pr;
};

// ----------------------------------------------------------------------------
// The Pose Studio's keys (Quake/vr/vr_agpose.hpp; ROUND22.md, "Pose Studio"): changes laid on the poses above by hand,
// one key per kind, weapon class, pose and phase (its code: kind * 10000 + class * 1000 + pose * 10 + phase), read
// through weapondrawnpose(world, code, 11..18) and blended by this pose's weights: the stance (carried or aimed/ready),
// the run, the pickup, a strike's wind-up, blow and follow-through. And the studio's hold: its subject shown in the pose
// picked on its panel (weapondrawnpose(g, kind * 100 + class, 19)).
// ----------------------------------------------------------------------------
float VR_AGK_FISTS = 0;     // the classes (agpose::Class)
float VR_AGK_GUN = 1;
float VR_AGK_MELEE = 2;
float VR_AGK_MELEE2 = 3;
float VR_AGK_SAW = 4;
float VR_AGK_TWOMELEE = 5;
float VR_AGK_TWOGUNS = 6;
float vr_agk_ww;            // a strike's phases' shares now (VR_AGK_Phases): its wind-up, its blow, its follow-through
float vr_agk_wb;
float vr_agk_wf;
float vr_agk_hpose;         // the studio's hold this frame (VR_AGK_Query): the pose (-1 none), its phase, Play's count
float vr_agk_hphase;
float vr_agk_hplay;
.float vr_agk_held;         // held in a pose by the studio (put back when let go)
.float vr_agk_play;         // its Play count seen
.float vr_agk_play_end;     // its strike played until
entity vr_agk_tgt;          // the studio's target ahead (an aim, a guard, a strike's)

float(entity g, entity p) VR_AGK_Class =
{
    if(!p)
    {
        return VR_AGK_FISTS;
    }
    float c = VR_AG_Class(p.weapon);
    if(g.vr_ag_prop2)
    {
        return c == VR_AG_CLASS_MELEE ? VR_AGK_TWOMELEE : VR_AGK_TWOGUNS;
    }
    if(c == VR_AG_CLASS_MELEE)
    {
        return p.weapon == WID_CHAINSAW ? VR_AGK_SAW : (VR_AGP_TwoHanded(p) ? VR_AGK_MELEE2 : VR_AGK_MELEE);
    }
    return VR_AGK_GUN;
};

// Key `code` at share `w` added (`mirror`: a left-handed strike on the right's key, the hands swapped and mirrored).
float vr_agk_rep;            // VR_AGK_Keys: the keys laid on told to the engine (the Pose Studio's Live subject), on
entity vr_agk_repent;
void(float code, float w, float mirror) VR_AGK_Add =
{
    if(w <= 0.001)
    {
        return;
    }
    if(vr_agk_rep)
    {
        // (the Pose Studio's Live tape: this key at this share, negative mirrored; vr_poselive.hpp)
        float wr = min(w, 0.999);
        weapondrawnpose(vr_agk_repent, mirror ? -(code + wr) : code + wr, 23);
    }
    vector has = weapondrawnpose(world, code, 18);
    if(has_x == 0)
    {
        return;
    }
    vector rp = weapondrawnpose(world, code, 11);
    vector rr = weapondrawnpose(world, code, 12);
    vector lp = weapondrawnpose(world, code, 13);
    vector lr = weapondrawnpose(world, code, 14);
    vector er = weapondrawnpose(world, code, 15);
    vector el = weapondrawnpose(world, code, 16);
    vector t = weapondrawnpose(world, code, 17);
    if(mirror)
    {
        // (a place's y the other way; a turn's rotation vector: its x and z; the torso's yaw and roll)
        vector k = rp;
        rp = VR_AGP_Mirror(lp);
        lp = VR_AGP_Mirror(k);
        k = rr;
        rr = [-lr_x, lr_y, -lr_z];
        lr = [-k_x, k_y, -k_z];
        k = er;
        er = VR_AGP_Mirror(el);
        el = VR_AGP_Mirror(k);
        t = [-t_x, t_y, -t_z];
    }
    vr_agk_rp = vr_agk_rp + rp * w;
    vr_agk_rr = vr_agk_rr + rr * w;
    vr_agk_lp = vr_agk_lp + lp * w;
    vr_agk_lr = vr_agk_lr + lr * w;
    vr_agk_er = vr_agk_er + er * w;
    vr_agk_el = vr_agk_el + el * w;
    vr_agk_t = vr_agk_t + t * w;
    vr_agk_on = TRUE;
};

// A strike's phases' shares at its time now: the wind-up's (1 at its end), the blow's (1 as it is struck: VR_AGP_Phase's
// turning point), the follow-through's (1 at the strike's end, held through a cut, back to 0 over the recovery).
void(entity g) VR_AGK_Phases =
{
    VR_AGP_Times(g);
    float s = time - g.vr_ag_act_at;
    float tS = vr_agp_tw + vr_agp_ts;
    float punch = g.vr_ag_act == VR_AGP_PUNCH;
    float h = punch ? 0.8 : 0.55;
    vr_agk_ww = 0;
    vr_agk_wb = 0;
    vr_agk_wf = 0;
    if(s < vr_agp_tw)
    {
        vr_agk_ww = VR_AGP_Smooth(s / vr_agp_tw);
    }
    else if(s < tS)
    {
        float u = (s - vr_agp_tw) / vr_agp_ts;
        if(!punch)
        {
            u = VR_AGP_Smooth(u);
        }
        float pe = punch && cvar_hget(cvarh_vr_agrunt_punch_ease); // (vr_agrunt_punch_ease: no kink at the blow)
        if(u < h)
        {
            vr_agk_ww = pe ? 1 - VR_AGP_Smooth(u / h) : cos(1.5708 * u / h);
        }
        else
        {
            vr_agk_wf = pe ? VR_AGP_Smooth((u - h) / (1 - h)) : sin(1.5708 * (u - h) / (1 - h));
        }
        vr_agk_wb = max(0, 1 - vr_agk_ww - vr_agk_wf);
    }
    else if(s < tS + vr_agp_th)
    {
        vr_agk_wf = 1;
    }
    else
    {
        vr_agk_wf = 1 - VR_AGP_Smooth((s - tS - vr_agp_th) / max(vr_agp_tr, 0.01));
    }
};

// `g`'s keys now (vr_agk_*), its right weapon `p` (world: its fists).
void(entity g, entity p) VR_AGK_Keys =
{
    vr_agk_rp = '0 0 0';
    vr_agk_rr = '0 0 0';
    vr_agk_lp = '0 0 0';
    vr_agk_lr = '0 0 0';
    vr_agk_er = '0 0 0';
    vr_agk_el = '0 0 0';
    vr_agk_t = '0 0 0';
    vr_agk_on = FALSE;
    vr_agk_rep = g.vr_agk_live;
    vr_agk_repent = g;
    float cls = VR_AGK_Class(g, p);
    float base = (g.vr_ag_kind == VR_AM_ENFORCER ? 10000 : 0) + cls * 1000;
    float wp = 0; // (the pickup's: its bend's)
    if(g.vr_ag_took_at > 0 && p)
    {
        if(time < g.vr_ag_took_grab)
        {
            wp = VR_AGP_Smooth((time - g.vr_ag_took_at) / max(0.01, g.vr_ag_took_grab - g.vr_ag_took_at));
        }
        else
        {
            wp = 1 - VR_AGP_Smooth((time - g.vr_ag_took_grab) / VR_AG_LIFT_TIME);
        }
    }
    float wa; // (aimed or ready: the stance's)
    if(cls == VR_AGK_FISTS)
    {
        wa = min(1, g.vr_ag_readyw / 0.75);
    }
    else if(cls == VR_AGK_GUN || cls == VR_AGK_TWOGUNS)
    {
        wa = VR_AGP_Smooth(g.vr_ag_aimw);
    }
    else
    {
        wa = vr_agp_arms ? VR_AGP_Smooth(g.vr_agp_rdy) : min(1, g.vr_ag_readyw / 0.7);
    }
    VR_AGK_Add(base, (1 - wp) * (1 - wa), FALSE);
    VR_AGK_Add(base + 10, (1 - wp) * wa, FALSE);
    VR_AGK_Add(base + 20, (1 - wp) * g.vr_agp_runw, FALSE);
    VR_AGK_Add(base + 30, wp, FALSE);
    float act = g.vr_ag_act;
    if(act == VR_AGP_PUNCH || act == VR_AGP_SWING || act == VR_AGP_SAW)
    {
        float k = g.vr_ag_act_kind;
        float pose = 18;
        float mir = FALSE;
        if(act == VR_AGP_PUNCH)
        {
            pose = k == 0 ? 10 : k == 1 ? 11 : k == 4 ? 13 : 12;
            mir = k == 2; // (a left hook: the right's mirrored)
        }
        else if(act == VR_AGP_SWING)
        {
            pose = 14 + min(3, k);
            if(g.vr_ag_prop2 && g.vr_ag_act_hand == 2)
            {
                pose = 19;
            }
            else if(g.vr_ag_prop2)
            {
                mir = g.vr_ag_act_hand == 1;
            }
        }
        VR_AGK_Phases(g);
        VR_AGK_Add(base + pose * 10, vr_agk_ww, mir);
        VR_AGK_Add(base + pose * 10 + 1, vr_agk_wb, mir);
        VR_AGK_Add(base + pose * 10 + 2, vr_agk_wf, mir);
    }
};

// The studio's hold this frame on `g` (its weapon `p`, pre-sorted: the class's the same): asked, and its kind and class
// told.
void(entity g, entity p) VR_AGK_Query =
{
    vector h = weapondrawnpose(g, (g.vr_ag_kind == VR_AM_ENFORCER ? 100 : 0) + VR_AGK_Class(g, p), 19);
    vr_agk_hpose = h_x;
    vr_agk_hphase = h_y;
    vr_agk_hplay = h_z;
};

// `g` (frozen: VR_AGP_Pose) held in the studio's pose: its target ahead, its stance's shares set at once, its frame (a
// run's mid-stride, the take's lowest), a strike at its phase's key (no blow dealt; Play: once from its start).
void(entity g, entity p) VR_AGK_Hold =
{
    float pose = vr_agk_hpose;
    float cls = VR_AGK_Class(g, p);
    float was = g.vr_agk_held;
    g.vr_agk_held = 1;
    VR_AM_Row(g.vr_ag_kind);
    makevectors([0, g.angles_y, 0]);
    if(!vr_agk_tgt)
    {
        vr_agk_tgt = spawn();
        vr_agk_tgt.classname = "vr_agk_target";
    }
    float gun = cls == VR_AGK_GUN || cls == VR_AGK_TWOGUNS;
    float aimed = pose == 1 || pose >= 10;
    setorigin(vr_agk_tgt, g.origin + v_forward * (gun ? 200 : 40) + '0 0 10');
    if(g.vr_rs_aimon)
    {
        setorigin(vr_agk_tgt, g.vr_rs_aim); // (the Rig Studio's aim target: an enemy's point standing there)
    }
    g.enemy = aimed ? vr_agk_tgt : world;
    g.vr_ag_aim = (aimed && gun) ? v_forward : '0 0 0';
    if(aimed && gun && g.vr_rs_aimon)
    {
        g.vr_ag_aim = normalize(g.vr_rs_aim - (p ? p.origin : g.origin)); // (as VR_AG_AimDir from its gun, no error)
    }
    g.vr_ag_aimw = aimed ? 1 : 0;
    g.vr_agp_rdy = aimed ? 1 : 0;
    g.vr_ag_readyw = cls == VR_AGK_FISTS ? (aimed ? 0.75 : 0) : 0.7;
    g.vr_agp_runw = pose == 2 ? 1 : 0;
    if(pose == 2)
    {
        g.frame = vr_am_f_run + 2;
    }
    if(pose == 3 && p)
    {
        // (as test 41's take: its hand just closed on a weapon lying before it, down on its knee)
        g.frame = vr_am_f_take + max(1, vr_am_n_take) - 1;
        g.vr_ag_took_at = time - 5;
        g.vr_ag_took_grab = time - 0.001;
        g.vr_ag_took_done = 1; // (its weapon in its hand already: no take run, VR_AG_PlaceProp)
        g.vr_ag_took_org = g.origin + v_forward * 18 + v_right * 4 + '0 0 -22';
        g.vr_ag_took_fw = v_forward;
        g.vr_ag_took_up = '0 0 1';
    }
    else
    {
        g.vr_ag_took_at = 0;
    }
    if(pose < 10)
    {
        g.vr_agk_play = vr_agk_hplay;
        return;
    }
    g.vr_ag_act = pose <= 13 ? VR_AGP_PUNCH : pose == 18 ? VR_AGP_SAW : VR_AGP_SWING;
    g.vr_ag_act_kind = pose == 10 ? 0 : pose == 11 ? 1 : pose == 12 ? 3 : pose == 13 ? 4 : pose == 18 ? 0 : pose == 19 ? 2 : pose - 14;
    g.vr_ag_act_hand = pose == 19 ? 2 : 0;
    g.vr_ag_act_var = 0.5;
    g.vr_ag_act_hit = 2; // (no blow dealt, no cut)
    g.vr_ag_act_hitl = 1;
    g.vr_ag_act_tick = time;
    VR_AGP_Times(g);
    if(was && vr_agk_hplay != g.vr_agk_play)
    {
        g.vr_ag_act_at = time; // (Play: the strike from its start, in place)
        g.vr_agk_play_end = time + VR_AGP_Length(g) + 0.4;
    }
    g.vr_agk_play = vr_agk_hplay;
    if(time >= g.vr_agk_play_end)
    {
        float s = vr_agp_tw * 0.999;
        if(vr_agk_hphase == 1)
        {
            s = vr_agp_tw + vr_agp_ts * (g.vr_ag_act == VR_AGP_PUNCH ? 0.8 : 0.5334); // (Smooth(0.5334) = 0.55: a swing's blow)
        }
        else if(vr_agk_hphase == 2)
        {
            s = vr_agp_tw + vr_agp_ts + vr_agp_th * 0.5 + 0.001;
        }
        g.vr_ag_act_at = time - s;
    }
};

// Let go by the studio: its take, its attack and its target dropped.
void(entity g) VR_AGK_Release =
{
    g.vr_agk_held = 0;
    g.vr_ag_took_at = 0;
    g.vr_ag_act = 0;
    g.vr_ag_aim = '0 0 0';
    g.enemy = world;
};

// ----------------------------------------------------------------------------
// The Pose Studio's Live mode (Quake/vr/vr_poselive.hpp): its subject plays its real loops where it stands, the others
// held still: its own AI against a target moving round it as the glitch finder's (vr_armedgrunt_glitch_test.qc), held
// on its spot (a run in place) but for a pickup's walk to its weapon. The loop (the studio's hold's x less 100): 0 the
// finder's 19 s (aim and fire round it, a run, strikes up close, pain, a pickup), 1 aim and fire, 2 reloads (its
// magazine emptied every 3 s), 3 a run, 4 strikes, 5 a pickup; z its restarts (a change: from its start). The player is
// god and notarget while it plays (its shots go at the target past you), put back after.
// ----------------------------------------------------------------------------
.float vr_agk_live_at;    // its loop's start
.float vr_agk_live_t;     // the server frame it last ran on
.float vr_agk_live_n;     // the restarts seen
.vector vr_agk_home;      // where it stands, its way
.float vr_agk_home_yaw;
.float vr_agk_live_hits;  // pain dealt this loop
.float vr_agk_live_knock; // its weapon knocked out this loop
.float vr_agk_live_hp;    // its health before (put back)
.float vr_agk_live_armed; // it held a weapon when it began
float vr_agk_live_cheats; // the player's god (1) and notarget (2) set by it
entity vr_agk_ltgt;

void(entity attacker, float damage) VR_AGK_TargetPain = { self.health = 100000; };
void() VR_AGK_TargetDie = { self.health = 100000; }; // (th_die is a void(): no arguments)
entity() VR_AGK_LiveTarget =
{
    if(!vr_agk_ltgt)
    {
        vr_agk_ltgt = spawn();
        vr_agk_ltgt.classname = "vr_agk_live_target";
        vr_agk_ltgt.netname = "the Pose Studio's target";
        vr_agk_ltgt.solid = SOLID_BBOX; // (its AI shoots only with a clear shot at it: CheckAttack)
        vr_agk_ltgt.movetype = MOVETYPE_NONE;
        vr_agk_ltgt.takedamage = DAMAGE_AIM;
        vr_agk_ltgt.th_pain = VR_AGK_TargetPain;
        vr_agk_ltgt.th_die = VR_AGK_TargetDie;
        vr_agk_ltgt.view_ofs = '0 0 22';
        setsize(vr_agk_ltgt, '-8 -8 -24', '8 8 24');
    }
    vr_agk_ltgt.health = 100000;
    return vr_agk_ltgt;
};

// Live off: back on its spot, its health and the player's cheats as they were.
void(entity g) VR_AGK_LiveEnd =
{
    g.vr_agk_live = 0;
    g.enemy = world;
    g.vr_ag_act = 0;
    g.velocity = '0 0 0';
    setorigin(g, g.vr_agk_home);
    g.angles_y = g.vr_agk_home_yaw;
    if(g.vr_agk_live_hp > 0)
    {
        g.health = g.vr_agk_live_hp;
    }
    entity pl = find(world, classname, "player");
    if(pl && (vr_agk_live_cheats & 1))
    {
        pl.flags = pl.flags - (pl.flags & FL_GODMODE);
    }
    if(pl && (vr_agk_live_cheats & 2))
    {
        pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
    }
    vr_agk_live_cheats = 0;
    if(vr_agk_ltgt)
    {
        setorigin(vr_agk_ltgt, g.vr_agk_home + '0 0 4000');
    }
};

// `g` (its right weapon `p`) live this server frame, playing loop `loop`.
void(entity g, entity p, float loop) VR_AGK_Live =
{
    if(g.vr_agk_live_t == time)
    {
        return; // (two weapons: called for each)
    }
    g.vr_agk_live_t = time;
    entity tg = VR_AGK_LiveTarget();
    entity oldself;
    if(!g.vr_agk_live || g.vr_agk_live_n != vr_agk_hplay)
    {
        if(!g.vr_agk_live)
        {
            g.vr_agk_home = g.origin;
            g.vr_agk_home_yaw = g.angles_y;
            g.vr_agk_live_hp = g.health;
            g.vr_agk_live_armed = g.vr_ag_prop != world;
            entity pl = find(world, classname, "player");
            if(pl && !(pl.flags & FL_GODMODE))
            {
                pl.flags = pl.flags | FL_GODMODE;
                vr_agk_live_cheats = vr_agk_live_cheats | 1;
            }
            if(pl && !(pl.flags & FL_NOTARGET))
            {
                pl.flags = pl.flags | FL_NOTARGET;
                vr_agk_live_cheats = vr_agk_live_cheats | 2;
            }
        }
        else if(g.vr_ag_prop || !g.vr_agk_live_armed)
        {
            setorigin(g, g.vr_agk_home);
        }
        g.vr_agk_live = 1;
        g.vr_agk_live_n = vr_agk_hplay;
        g.vr_agk_live_at = time;
        g.vr_agk_live_hits = 0;
        g.vr_agk_live_knock = 0;
        g.nextthink = time + 0.05; // (its AI on again)
    }
    float len = loop == 0 ? 19 : loop == 2 ? 3 : loop == 4 ? 3.5 : loop == 5 ? 5 : 7;
    float s = time - g.vr_agk_live_at;
    if(s >= len)
    {
        g.vr_agk_live_at = time;
        s = 0;
        g.vr_agk_live_hits = 0;
        g.vr_agk_live_knock = 0;
        if(g.vr_ag_prop || !g.vr_agk_live_armed)
        {
            setorigin(g, g.vr_agk_home); // (back on its spot after a pickup's walk)
        }
    }
    // (the finder's schedule's time for this loop)
    float u = loop == 3 ? 8 : loop == 4 ? 9.5 + s : loop == 5 ? 15 + s : s;
    if(loop == 2 && s == 0 && p && p.weaponinst && VR_AG_ClipOf(p.weapon, p.weaponinst) > 0)
    {
        p.weaponinst.wi_clip = 0; // (its next shot a reload)
    }
    g.health = 1000;
    g.vr_ag_reserve = max(g.vr_ag_reserve, 50);
    vector home = g.vr_agk_home;
    makevectors([0, g.vr_agk_home_yaw, 0]);
    traceline(home + '0 0 16', home + v_forward * 600 + '0 0 16', TRUE, g);
    vector far = trace_endpos - v_forward * 40 - '0 0 16';
    vector at;
    if(u < 7)
    {
        float a = u * (u < 3.5 ? 50 : -50) + (u < 3.5 ? 0 : 350);
        makevectors([0, a, 0]);
        at = home + v_forward * 220 + '0 0 1' * (130 * sin(u * 0.9));
    }
    else if(u < 9.5)
    {
        at = far;
    }
    else if(u < 13)
    {
        makevectors([0, (u - 9.5) * 40, 0]);
        at = g.origin + v_forward * 44; // (within its reach: its strikes)
    }
    else
    {
        makevectors([0, g.angles_y, 0]);
        at = g.origin + v_forward * (u < 15 ? 160 : 300);
    }
    setorigin(tg, at);
    if(g.enemy != tg)
    {
        oldself = self;
        self = g;
        self.enemy = tg;
        FoundTarget();
        self = oldself;
    }
    if(loop == 0 && u >= 13.2 && g.vr_agk_live_hits < 1 + floor((u - 13.2) / 0.7) && g.vr_agk_live_hits < 3)
    {
        g.vr_agk_live_hits = g.vr_agk_live_hits + 1;
        T_Damage(g, tg, tg, 3);
        g.health = 1000;
    }
    if(u >= 15 && !g.vr_agk_live_knock && g.vr_ag_prop)
    {
        g.vr_agk_live_knock = 1;
        makevectors([0, g.angles_y + 60, 0]);
        VR_AG_Disarm(g, v_forward, 120, "the Pose Studio");
    }
    // (held on its spot, a run in place; not while it goes for its weapon knocked out)
    if(!g.vr_agk_live_knock && (g.vr_ag_prop || !g.vr_agk_live_armed))
    {
        setorigin(g, home);
        g.velocity = '0 0 0';
    }
};

// The left hand's blade (dual wielding: beside a blade in the right; vr_ag_gunaxe: beside a gun): carried at its side,
// swung on its attacks' left-hand keys (VR_AGP_MeleeLeft), its hand's frame on the blade's line (vr_agarms).
void(entity g, entity pL) VR_AGP_LeftBlade =
{
    VR_AGP_MeleeLeft(g, pL);
    if(vr_agp_arms)
    {
        vector keepFw = vr_agp_fw;
        vector keepUp = vr_agp_up;
        VR_AGP_BladeHand(pL, vr_agp_fwl, vr_agp_upl);
        vr_agp_fwl = vr_agp_fw;
        vr_agp_upl = vr_agp_up;
        vr_agp_fw = keepFw;
        vr_agp_up = keepUp;
        VR_AM_Row(g.vr_ag_kind);
        vr_agp_vl = vr_agp_vl - vr_am_anchor_at; // (MeleeLeft's grip: fitted to its body already)
        vr_agp_wl = 1;
    }
    else
    {
        float tiltL = VR_AGP_BladeTilt(pL);
        vector rightL = VR_AGP_Cross(vr_agp_fwl, vr_agp_upl);
        vr_agp_fwl = VR_AG_Rotate(vr_agp_fwl, rightL, -tiltL);
        vr_agp_upl = VR_AG_Rotate(vr_agp_upl, rightL, -tiltL);
        vr_agp_wl = 0;
    }
    vr_agp_flags = VR_AGP_LEFT | VR_AGP_LEFTWPN;
};

// `g`'s pose this server frame, its weapon `p` (world: its fists) placed by it and the pose sent (QVR_SVC_ARMPOSE).
void(entity g, entity p) VR_AGP_Pose =
{
    vr_agk_hpose = -1;
    if(!vr_agp_quiet)
    {
        VR_AGK_Query(g, p); // (the Pose Studio's hold; it learns its kind and class)
    }
    float frozen = VR_AG_Still(g);
    if(frozen)
    {
        // (the Grip Editor: held still in its first idle frame, its weapons ready, its AI held)
        g.nextthink = time + 0.5;
        g.enemy = world;
        g.vr_ag_act = 0;
        g.velocity = '0 0 0';
        g.frame = VR_AM_StandFrame(g, 0);
    }
    if(frozen && vr_agk_hpose >= 0 && vr_agk_hpose < 100)
    {
        VR_AGK_Hold(g, p); // (the Pose Studio's pose picked on its panel)
    }
    else if(g.vr_agk_held && !vr_agp_quiet)
    {
        VR_AGK_Release(g);
    }
    if(!vr_agp_quiet && vr_agk_hpose >= 100 && g.health > 0)
    {
        VR_AGK_Live(g, p, vr_agk_hpose - 100); // (the Pose Studio's Live mode: its loops)
    }
    else if(!vr_agp_quiet && g.vr_agk_live)
    {
        VR_AGK_LiveEnd(g);
    }
    vr_agp_arms = cvar_hget(cvarh_vr_agarms) != 0;
    vector hand = VR_AM_Hand(g); // (its row's vertices: vr_armedmon.qc)
    if(trace_fraction == 0)
    {
        return; // (no such model now: a ragdoll's, another's; it stays where it was)
    }
    if(vr_agp_arms)
    {
        hand = VR_AM_Anchor(g); // (vr_agarms: its poses placed from its chest, not its animation's hand)
    }
    vector rear = VR_AM_GunRear(g);
    vector muzzle = VR_AM_GunMuzzle(g);
    vector gx = normalize(muzzle - rear);
    vector gz = VR_AM_GunUp(g);
    gz = normalize(gz - gx * (gz * gx));
    if(gz == '0 0 0') { gz = '0 0 1'; }
    vector gy = VR_AGP_Cross(gz, gx);
    if(g.vr_ag_act && time - g.vr_ag_act_tick > 0.25)
    {
        g.vr_ag_act = 0; // (its attack's frames stopped: a pain, a knockdown)
    }
    entity pL = world; // (two held: the right hand's in `p`, the left's here; the slots don't say which)
    if(p && g.vr_ag_prop2)
    {
        entity pa = g.vr_ag_prop;
        entity pb = g.vr_ag_prop2;
        p = pa.vr_ag_left ? pb : pa;
        pL = pa.vr_ag_left ? pa : pb;
    }
    VR_AGP_Body(g);
    vector handL = VR_AGP_ToLocal(hand - g.origin);
    vr_agp_flags = 0;
    vr_agp_gunpose = 0;
    vr_agp_wl = 0;
    vr_agp_vl = '0 0 0';
    vr_agp_fw = gx;
    vr_agp_up = gz;
    vr_agp_wo = 1;
    float c = p ? VR_AG_Class(p.weapon) : VR_AG_CLASS_FISTS;
    VR_AGP_Demo(g, c);
    VR_AGK_Keys(g, p); // (the Pose Studio's keys: laid on below; the blows' lines moved with them, VR_AGP_Strike)
    if(c == VR_AG_CLASS_FISTS)
    {
        VR_AGP_Fists(g, handL);
    }
    else if(c == VR_AG_CLASS_MELEE)
    {
        VR_AGP_Melee(g, p, handL, -gx, -gz);
        VR_AGPar_Pose(g, p); // (its parry raised into a blow's way: vr_armedgrunt_parry.qc)
        if(vr_agp_arms && p.weapon == WID_CHAINSAW)
        {
            // (vr_agarms: a chainsaw held as a gun is, its bar the hand's forward, upright: its front handle on top)
            VR_AGP_Orient('1 0 0', vr_agp_fw, '0 0 1', VR_AGP_ToWorld('0 0 1'));
        }
        else if(vr_agp_arms)
        {
            // (vr_agarms: the hand's frame holding the blade on that line, the edge leading as the knuckles do)
            VR_AGP_BladeHand(p, vr_agp_fw, vr_agp_up);
        }
        else
        {
            // The pose is the blade's (or the head's, the bar's) line; the model is drawn turned up from its entity's
            // forward by its Weapon Offsets (a hand's hold): the entity turned down by that so the blade lies where wanted.
            float tilt = VR_AGP_BladeTilt(p);
            vector right = VR_AGP_Cross(vr_agp_fw, vr_agp_up);
            vr_agp_fw = VR_AG_Rotate(vr_agp_fw, right, -tilt);
            vr_agp_up = VR_AG_Rotate(vr_agp_up, right, -tilt);
        }
        if(pL)
        {
            VR_AGP_LeftBlade(g, pL);
        }
    }
    else if(vr_agp_arms)
    {
        float bladeL = pL && VR_AG_Class(pL.weapon) == VR_AG_CLASS_MELEE; // (a gun and an axe: vr_ag_gunaxe)
        vr_agp_onehand = bladeL; // (the gun one-handed beside a blade: as beside a second gun)
        VR_AGP_ArmsGun(g, p, bladeL ? world : pL);
        vr_agp_onehand = 0;
        vr_agp_gunpose = 1;
        VR_AGPar_Pose(g, p);
        if(bladeL)
        {
            vector keepFwR = vr_agp_fw;
            vector keepUpR = vr_agp_up;
            vector keepVR = vr_agp_v;
            float keepWR = vr_agp_wr;
            float keepFl = vr_agp_flags - (vr_agp_flags & (VR_AGP_LEFT | VR_AGP_LEFTWPN));
            VR_AGP_LeftBlade(g, pL);
            vr_agp_fw = keepFwR;
            vr_agp_up = keepUpR;
            vr_agp_v = keepVR;
            vr_agp_wr = keepWR;
            vr_agp_flags = keepFl | VR_AGP_LEFT | VR_AGP_LEFTWPN;
        }
    }
    else
    {
        float bladeL2 = pL && VR_AG_Class(pL.weapon) == VR_AG_CLASS_MELEE;
        VR_AGP_Gun(g, p, bladeL2 ? world : pL, gx, gz);
        VR_AGPar_Pose(g, p);
        if(bladeL2)
        {
            vector keepFwR2 = vr_agp_fw;
            vector keepUpR2 = vr_agp_up;
            VR_AGP_LeftBlade(g, pL);
            vr_agp_fw = keepFwR2;
            vr_agp_up = keepUpR2;
        }
    }
    if(vr_agp_arms)
    {
        // (its run's sway on its hands; placed from its anchor)
        vector bob = VR_AGP_RunBob(g);
        vr_agp_v = vr_agp_v + bob;
        vr_agp_vl = vr_agp_vl + bob * vr_agp_wl;
        vr_agp_flags = vr_agp_flags | VR_AGP_ANCHOR;
        // (its hands' grips within its arms' reach of its shoulders as its chest has them: a swing's arc past them, a
        // bent pain's, drawn where its hand can hold it, and the game's weapon line with it)
        VR_AM_Row(g.vr_ag_kind);
        vector moved = handL - vr_am_anchor_at;
        float most = 0.93 * vr_am_arm + 2.5;
        vector grip = handL * vr_agp_wr + vr_agp_v;
        vector fromSh = grip - (vr_am_sh_r + moved);
        if(vlen(fromSh) > most)
        {
            vr_agp_v = vr_am_sh_r + moved + normalize(fromSh) * most - handL * vr_agp_wr;
        }
        if(vr_agp_flags & VR_AGP_LEFT)
        {
            grip = handL * vr_agp_wl + vr_agp_vl;
            fromSh = grip - (vr_am_sh_l + moved);
            if(vlen(fromSh) > most)
            {
                vr_agp_vl = vr_am_sh_l + moved + normalize(fromSh) * most - handL * vr_agp_wl;
            }
        }
    }
    VR_AGP_Body(g);
    vector posL = handL * vr_agp_wr + vr_agp_v;
    if(vr_agp_flags & VR_AGP_LEFTMIRROR)
    {
        // Two guns: the left's target is the right hand (as the server has it) mirrored, sent whole (wl 0: the client's
        // left target as for a blade in the left hand; the hands' IK is the engine's, vr_hitreact.cpp, untouched).
        vr_agp_vl = VR_AGP_Mirror(handL) * vr_agp_wl + vr_agp_vl;
        vr_agp_wl = 0;
        vr_agp_flags = vr_agp_flags - VR_AGP_LEFTMIRROR;
    }
    float reaching = FALSE; // (its hand reaching down to a weapon on the floor: sent as no weapon)
    float takeBend = 0;     // (its chest's bend forward taking a weapon up: degrees)
    float takeW = 0;        // (... the take's weight: 0 standing, 1 at its lowest; the client's kneel, vr_aglegs)
    if(g.vr_ag_took_at > 0 && p && pL && vr_agp_wl > 0)
    {
        vr_agp_vl = handL * vr_agp_wl + vr_agp_vl; // (vr_agarms: the left weapon's place whole, as the lay it eases from)
        vr_agp_wl = 0;
    }
    if(g.vr_ag_took_at > 0 && p)
    {
        // A weapon just taken up from the floor (vr_armedgrunt.qc VR_AG_TakeBegin): where it lay until its hand closes on
        // it (the hand's IK reaching down to it), then carried up to its hold.
        float u = min(1, max(0, (time - g.vr_ag_took_grab) / VR_AG_LIFT_TIME));
        if(u >= 1 && !vr_agp_quiet)
        {
            g.vr_ag_took_at = 0;
        }
        u = u * u * (3 - 2 * u);
        vector lay = VR_AGP_ToLocal(g.vr_ag_took_org - g.origin);
        // (vr_agarms: its chest bent forward at the waist as it goes down, held through the grab, straightened over the
        // lift: its hand reaches the handle; the take frames' crouch alone leaves it 5 units short)
        float dk = (time - g.vr_ag_took_at) / max(0.01, g.vr_ag_took_grab - g.vr_ag_took_at);
        dk = min(1, max(0, dk));
        takeW = time < g.vr_ag_took_grab ? dk * dk * (3 - 2 * dk) : 1 - u;
        takeBend = (cvar_hget(cvarh_vr_aglegs) ? VR_AGP_TAKE_BEND_KNEEL : VR_AGP_TAKE_BEND) * takeW;
        if(!pL && time < g.vr_ag_took_grab)
        {
            // Bending down: its hand reaches down with the bend (from where its frame has it to the weapon's grip as the
            // lowest frame is drawn in full), the weapon lying where it lay (sent as no weapon: the client draws it where
            // the server has it, not at the hand's target). It closes on it there (the author's note 2026-10-24 01:15:28:
            // the hand was on it from the first frame, the grunt barely bent).
            float d = (time - g.vr_ag_took_at) / max(0.01, g.vr_ag_took_grab - g.vr_ag_took_at);
            d = min(1, max(0, d));
            d = d * d * (3 - 2 * d);
            vr_agp_wr = 1 - d;
            vr_agp_v = lay * d;
            if(vr_agp_arms)
            {
                // (vr_agarms: from its hand hanging free, from its anchor; its hand turned to the weapon's handle as it
                // reaches, VR_AGP_REACH: the client's IK closes it on the grip as you would)
                VR_AM_Row(g.vr_ag_kind);
                vr_agp_v = (VR_AGP_Hang(0) - vr_am_anchor_at) * (1 - d) + lay * d;
                vr_agp_flags = vr_agp_flags | VR_AGP_REACH;
            }
            reaching = TRUE;
            posL = lay;
            vr_agp_fw = g.vr_ag_took_fw;
            vr_agp_up = g.vr_ag_took_up;
        }
        else if(pL)
        {
            vr_agp_vl = lay + (vr_agp_vl - lay) * u;
            if(cvar_hget(cvarh_vr_agrunt_lift_turn))
            {
                VR_AGP_TurnFrame(g.vr_ag_took_fw, g.vr_ag_took_up, vr_agp_fwl, vr_agp_upl, u);
                vr_agp_fwl = vr_agp_sfw;
                vr_agp_upl = vr_agp_sup;
            }
            else
            {
                vr_agp_fwl = normalize(g.vr_ag_took_fw + (vr_agp_fwl - g.vr_ag_took_fw) * u);
                vr_agp_upl = normalize(g.vr_ag_took_up + (vr_agp_upl - g.vr_ag_took_up) * u);
                vr_agp_upl = normalize(vr_agp_upl - vr_agp_fwl * (vr_agp_upl * vr_agp_fwl));
            }
        }
        else
        {
            posL = lay + (posL - lay) * u;
            vr_agp_wr = 0;
            vr_agp_v = posL;
            if(cvar_hget(cvarh_vr_agrunt_lift_turn))
            {
                VR_AGP_TurnFrame(g.vr_ag_took_fw, g.vr_ag_took_up, vr_agp_fw, vr_agp_up, u);
                vr_agp_fw = vr_agp_sfw;
                vr_agp_up = vr_agp_sup;
            }
            else
            {
                vr_agp_fw = normalize(g.vr_ag_took_fw + (vr_agp_fw - g.vr_ag_took_fw) * u);
                vr_agp_up = normalize(g.vr_ag_took_up + (vr_agp_up - g.vr_ag_took_up) * u);
                vr_agp_up = normalize(vr_agp_up - vr_agp_fw * (vr_agp_up * vr_agp_fw));
            }
        }
    }
    float body = g.vr_ag_took_at <= 0 && VR_AGP_TorsoNow(g);
    // (the Pose Studio's keys: a torso's turn, elbows' poles, outside a strike too; TorsoNow zeroed the rest)
    float keyBody = vr_agk_on && (vr_agk_t != '0 0 0' || vr_agk_er != '0 0 0' || vr_agk_el != '0 0 0');
    if(!body && keyBody && g.vr_ag_took_at <= 0)
    {
        body = TRUE;
    }
    if(body)
    {
        vr_agp_tq = vr_agp_tq + vr_agk_t;
        vr_agp_elr = vr_agp_elr + vr_agk_er;
        vr_agp_ell = vr_agp_ell + vr_agk_el;
        posL = VR_AGP_TurnPose(g, posL, handL);
        vr_agp_flags = vr_agp_flags | VR_AGP_BODY;
    }
    else if(vr_agp_arms && (takeBend > 0.1 || keyBody))
    {
        // A take's bend: the body only (its hands' targets the lying weapon's, where it lies, not turned with it)
        vr_agp_tq = vr_agk_t;
        vr_agp_tq_y = vr_agp_tq_y + takeBend;
        vr_agp_elr = vr_agk_er;
        vr_agp_ell = vr_agk_el;
        vr_agp_flags = vr_agp_flags | VR_AGP_BODY | VR_AGP_FIXED;
        body = TRUE;
    }
    if(vr_agp_gunpose && !(vr_agp_flags & VR_AGP_FIXED) && cvar_hget(cvarh_vr_agrunt_upper))
    {
        // The procedural upper body's gun motions (vr_agrunt_upper; the engine holds the chest on the hips, these turn
        // it): the chest turned towards its aim (vr_agrunt_aim_twist of the aim's yaw and pitch off its body's way), leant
        // back by a shot's kick, turned and bent towards its off hand in a reload. Its hands' targets stay where they are
        // (on the aim): only the body turns under them.
        vector tw = '0 0 0';
        float aw = VR_AGP_Smooth(g.vr_ag_aimw);
        if(aw > 0 && g.vr_ag_aimdir != '0 0 0')
        {
            vector al = VR_AGP_ToLocal(g.vr_ag_aimdir);
            float share = cvar_hget(cvarh_vr_agrunt_aim_twist) * aw;
            tw_x = max(-25, min(25, share * atan2(al_y, al_x) * (180 / 3.14159265)));
            tw_y = max(-20, min(20, -share * atan2(al_z, vlen([al_x, al_y, 0])) * (180 / 3.14159265)));
        }
        // (eased: a shot's own aim error, a target darting about, don't jerk the chest)
        g.vr_agp_twist_x = VR_AGP_Ease(g.vr_agp_twist_x, tw_x, 60);
        g.vr_agp_twist_y = VR_AGP_Ease(g.vr_agp_twist_y, tw_y, 60);
        vector tq = [g.vr_agp_twist_x, g.vr_agp_twist_y, 0];
        float ke = time - g.vr_ag_kick_at;
        if(ke >= 0 && ke < 0.4 && g.vr_ag_kick_at > 0)
        {
            tq_y = tq_y - 3 * sin(ke / 0.4 * 3.14159265); // (back and forward again, no snap)
        }
        float rlp = g.vr_agp_rl_at > 0 ? (time - g.vr_agp_rl_at) / max(0.1, cvar_hget(cvarh_vr_agrunt_reload_time)) : -1;
        if(rlp >= 0 && rlp <= 1)
        {
            float rs = sin(rlp * 3.14159265);
            tq_x = tq_x + 8 * rs;
            tq_y = tq_y + 6 * rs;
        }
        if(tq != '0 0 0')
        {
            vr_agp_tq = (vr_agp_flags & VR_AGP_BODY) ? vr_agp_tq + tq : tq;
            vr_agp_flags = vr_agp_flags | VR_AGP_BODY;
            body = TRUE;
        }
    }
    if(vlen(vr_agp_elr) > 1) { vr_agp_elr = normalize(vr_agp_elr); }
    if(vlen(vr_agp_ell) > 1) { vr_agp_ell = normalize(vr_agp_ell); }
    if(vr_agk_on)
    {
        // The Pose Studio's keys: its targets' places and turns (its body's space, after the torso's turn; a weapon
        // lying while its hand reaches for it stays where it lies)
        VR_AGP_Body(g);
        if(!reaching)
        {
            posL = posL + vr_agk_rp;
        }
        vr_agp_v = vr_agp_v + vr_agk_rp;
        if(vlen(vr_agk_rr) > 0.01)
        {
            vector kax = normalize(VR_AGP_ToWorld(vr_agk_rr));
            vr_agp_fw = VR_AG_Rotate(vr_agp_fw, kax, vlen(vr_agk_rr));
            vr_agp_up = VR_AG_Rotate(vr_agp_up, kax, vlen(vr_agk_rr));
        }
        if(vr_agp_flags & VR_AGP_LEFT)
        {
            vr_agp_vl = vr_agp_vl + vr_agk_lp;
        }
        if((vr_agp_flags & VR_AGP_LEFTWPN) && vlen(vr_agk_lr) > 0.01)
        {
            vector kaxl = normalize(VR_AGP_ToWorld(vr_agk_lr));
            vr_agp_fwl = VR_AG_Rotate(vr_agp_fwl, kaxl, vlen(vr_agk_lr));
            vr_agp_upl = VR_AG_Rotate(vr_agp_upl, kaxl, vlen(vr_agk_lr));
        }
    }
    if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
    {
        dprint(sprintf("agpose %s act %g kind %g s %.3f wr %.2f grip %v", p ? p.netname : "fists", g.vr_ag_act,
            g.vr_ag_act_kind, time - g.vr_ag_act_at, vr_agp_wr, posL)); // (sprintf: 7 arguments at most)
        dprint(sprintf(" fw %v up %v torso %v wind %.2f follow %.2f\n", VR_AGP_ToLocal(vr_agp_fw), VR_AGP_ToLocal(vr_agp_up),
            vr_agp_tq, body ? vr_agp_ww : 0, body ? vr_agp_wf : 0));
    }
    if(p)
    {
        vector a = VR_AxeStick_Angles(vr_agp_fw, vr_agp_up);
        p.v_angle = a;
        p.angles = a;
        p.angles_x = -a_x; // (a model's pitch)
        p.vr_ag_pfw = vr_agp_fw; // (a ragdoll's weapon rides its limb from here: VR_AG_RagProp)
        p.vr_ag_pup = vr_agp_up;
        setorigin(p, g.origin + VR_AGP_ToWorld(posL));
        if(vr_agp_flags & VR_AGP_LEFTWPN)
        {
            vector al = VR_AxeStick_Angles(vr_agp_fwl, vr_agp_upl);
            pL.v_angle = al;
            pL.angles = al;
            pL.angles_x = -al_x;
            pL.vr_ag_pfw = vr_agp_fwl;
            pL.vr_ag_pup = vr_agp_upl;
            setorigin(pL, g.origin + VR_AGP_ToWorld(handL * vr_agp_wl + vr_agp_vl)); // (wl: vr_agarms' anchor; 0 before)
        }
        if(p.weapon == WID_CHAINSAW && !vr_agp_quiet)
        {
            VR_AGP_Saw(g, p);
            VR_AGP_Body(g);
        }
    }
    vr_agk_on = FALSE; // (another's blow, later, unmoved)
    if(vr_agp_quiet)
    {
        return;
    }

    WriteByte(MSG_BROADCAST, 39); // svc_quakevr
    WriteByte(MSG_BROADCAST, 36); // QVR_SVC_ARMPOSE
    WriteEntity(MSG_BROADCAST, g);
    WriteEntity(MSG_BROADCAST, reaching ? world : p);
    WriteByte(MSG_BROADCAST, vr_agp_flags);
    WriteByte(MSG_BROADCAST, rint(min(1, max(0, vr_agp_wr)) * 255));
    VR_AGP_WriteVec(vr_agp_v);
    VR_AGP_WriteDir(VR_AGP_ToLocal(vr_agp_fw));
    VR_AGP_WriteDir(VR_AGP_ToLocal(vr_agp_up));
    vector f;
    f_x = vr_agp_fw * gx; f_y = vr_agp_fw * gy; f_z = vr_agp_fw * gz;
    VR_AGP_WriteDir(f);
    f_x = vr_agp_up * gx; f_y = vr_agp_up * gy; f_z = vr_agp_up * gz;
    VR_AGP_WriteDir(f);
    WriteByte(MSG_BROADCAST, rint(min(1, max(0, vr_agp_wo)) * 255));
    WriteByte(MSG_BROADCAST, rint(min(1, max(0, vr_agp_wl)) * 255));
    VR_AGP_WriteVec(vr_agp_vl);
    if(vr_agp_flags & VR_AGP_LEFTWPN)
    {
        WriteEntity(MSG_BROADCAST, pL);
        VR_AGP_WriteDir(VR_AGP_ToLocal(vr_agp_fwl));
        VR_AGP_WriteDir(VR_AGP_ToLocal(vr_agp_upl));
    }
    if(body)
    {
        // The torso's turn (degrees x 100), the elbows' poles (x 32000)
        WriteShort(MSG_BROADCAST, rint(vr_agp_tq_x * 100));
        WriteShort(MSG_BROADCAST, rint(vr_agp_tq_y * 100));
        WriteShort(MSG_BROADCAST, rint(vr_agp_tq_z * 100));
        VR_AGP_WriteDir(vr_agp_elr);
        VR_AGP_WriteDir(vr_agp_ell);
        VR_AGP_WriteVec(VR_AM_Fit(g, VR_AGP_WAIST)); // (the waist they were turned about)
        if(vr_agp_flags & VR_AGP_FIXED)
        {
            // (a take's weight x 127, its left hand taking it 128: the client's kneel, vr_aglegs)
            WriteByte(MSG_BROADCAST, rint(min(1, max(0, takeW)) * 127) + (pL ? 128 : 0));
        }
    }
};

// The fists' pose sender: every server frame while its grunt lives and holds nothing (a weapon sends its own).
void() VR_AGP_PoserThink =
{
    entity g = self.owner;
    if(!g || g.vr_ag_poser != self || !g.vr_ag || g.health <= 0 || g.modelindex == 0)
    {
        if(g.vr_ag_poser == self) { g.vr_ag_poser = world; }
        remove(self);
        return;
    }
    self.nextthink = time;
    if(g.vr_ag_prop || g.vr_knockdown)
    {
        return;
    }
    VR_AGP_Pose(g, world);
};

void(entity g) VR_AGP_Spawn =
{
    if(g.vr_ag_poser)
    {
        return;
    }
    entity e = spawn();
    e.classname = "vr_ag_poser";
    e.owner = g;
    e.think = VR_AGP_PoserThink;
    e.nextthink = time;
    g.vr_ag_poser = e;
};

// ----------------------------------------------------------------------------
// The attack's frames: the grunt's stand frames under it, stepping in; `self` the grunt
// ----------------------------------------------------------------------------

void() VR_AGP_Frame;
void() vr_agp_act = [0, vr_agp_act] { VR_AGP_Frame(); };

void(float act) VR_AGP_Begin =
{
    float last = self.vr_ag_act_kind;
    self.vr_ag_act = act;
    if(act == VR_AGP_PUNCH)
    {
        // A jab, a cross, hooks, an uppercut; a combo goes on to the other hand.
        float r = random();
        float kind = r < 0.3 ? 0 : r < 0.55 ? 1 : r < 0.72 ? 2 : r < 0.9 ? 3 : 4;
        if(self.vr_ag_act_combo > 0)
        {
            float wasLeft = last == 0 || last == 2;
            kind = wasLeft ? (random() < 0.6 ? 1 : (random() < 0.5 ? 3 : 4)) : (random() < 0.6 ? 0 : 2);
        }
        self.vr_ag_act_kind = kind;
    }
    else if(act == VR_AGP_SWING)
    {
        float k2 = floor(random() * 4);
        if(k2 == last) { k2 = (k2 + 1 + floor(random() * 3)) & 3; }
        self.vr_ag_act_kind = min(3, k2);
        // Two held: the hands in turn, now and then both at once (mirrored: a scissor).
        float hand = 0;
        if(self.vr_ag_prop2 && VR_AG_Class(self.vr_ag_wid2) != VR_AG_CLASS_MELEE)
        {
            hand = self.vr_ag_prop.vr_ag_left ? 1 : 0; // (a gun and an axe: the blade's hand alone)
        }
        else if(self.vr_ag_prop2)
        {
            hand = self.vr_ag_act_hand == 0 ? 1 : 0;
            if(random() < 0.25)
            {
                hand = 2;
            }
        }
        self.vr_ag_act_hand = hand;
    }
    else
    {
        self.vr_ag_act_kind = 0;
    }
    self.vr_ag_act_at = time;
    self.vr_ag_act_var = random();
    self.vr_ag_act_hit = 0;
    self.vr_ag_act_hitl = 0;
    self.vr_ag_act_tick = time;
    self.vr_ag_aim = '0 0 0';
    if(act == VR_AGP_SWING)
    {
        float blade = (self.vr_ag_wid == WID_SWORD) && !VR_AGP_TwoHanded(self.vr_ag_prop);
        string w = blade ? "knight/sword1.wav" : "weapons/ax1.wav";
        VR_SoundVaried(self, CHAN_WEAPON, w, 1, ATTN_NORM); // (the swing's whoosh; its hit sounds are the weapon's)
    }
    vr_agp_act();
};

// Its th_melee: a punch, a swing or the chainsaw's thrust (VR_AG_CheckAttack: its target within reach).
void() VR_AGP_Attack =
{
    entity p = self.vr_ag_prop;
    if(p && p.weapon == WID_CHAINSAW && p.weaponinst.wi_clip > 0)
    {
        VR_AGP_Begin(VR_AGP_SAW);
    }
    else if(p && VR_AG_Class(p.weapon) == VR_AG_CLASS_MELEE)
    {
        VR_AGP_Begin(VR_AGP_SWING);
    }
    else if(!p)
    {
        self.vr_ag_act_combo = 0;
        VR_AGP_Begin(VR_AGP_PUNCH);
    }
    else
    {
        vr_ag_swing1(); // (a gun's butt: the shove's frames, W_GunMelee)
    }
};

void() VR_AGP_Frame =
{
    self.vr_ag_act_tick = time;
    float demo = self.vr_ag_demo && !self.enemy; // (vr_agrunt_strike_demo's: at the air, standing)
    if(!demo)
    {
        ai_face();
    }
    float s = time - self.vr_ag_act_at;
    VR_AGP_Times(self);
    self.frame = VR_AM_StandFrame(self, floor(s * 10)); // (its stand frames: its legs under the arms)
    if(s < vr_agp_tw + vr_agp_ts && !demo)
    {
        ai_charge(s < vr_agp_tw ? 1 : 3);
    }
    if(s < VR_AGP_Length(self) || !self.vr_ag_act)
    {
        if(self.vr_ag_act)
        {
            return;
        }
    }
    float was = self.vr_ag_act;
    self.vr_ag_act = 0;
    if(was == VR_AGP_PUNCH && self.enemy && self.enemy.health > 0 && self.vr_ag_act_combo < 2 && random() < 0.55 &&
       vlen(VR_AI_Origin(self.enemy) - self.origin) < VR_AG_GRIP_REACH + 8)
    {
        self.vr_ag_act_combo = self.vr_ag_act_combo + 1;
        VR_AGP_Begin(VR_AGP_PUNCH);
        return;
    }
    if(was == VR_AGP_SWING && self.vr_ag_prop2 && self.vr_ag_act_hand != 2 && self.enemy && self.enemy.health > 0 &&
       random() < 0.45 && vlen(VR_AI_Origin(self.enemy) - self.origin) < VR_AG_GRIP_REACH + 10)
    {
        VR_AGP_Begin(VR_AGP_SWING); // (two held: the other hand follows up)
        return;
    }
    if(demo)
    {
        self.vr_ag_demo = 0;
        self.th_stand();
        return;
    }
    self.vr_ag_demo = 0;
    VR_AG_AfterAttack();
};

// vr_agrunt_strike_demo: `g` (a melee weapon or its fists, no enemy, standing) strikes the air ahead every 2 s.
void(entity g, float c) VR_AGP_Demo =
{
    if(!cvar_hget(cvarh_vr_agrunt_strike_demo) || vr_agp_quiet || g.enemy || g.vr_ag_act || g.health <= 0 ||
       g.vr_knockdown || g.vr_ag_took_at > 0 || (c != VR_AG_CLASS_FISTS && c != VR_AG_CLASS_MELEE))
    {
        return;
    }
    if(g.vr_ag_demo_next <= 0 || g.vr_ag_demo_next > time + 4)
    {
        g.vr_ag_demo_next = time + 1 + random();
        return;
    }
    if(time < g.vr_ag_demo_next)
    {
        return;
    }
    g.vr_ag_demo_next = time + 1.6 + random() * 0.8;
    entity oldself = self;
    self = g;
    self.vr_ag_demo = 1;
    VR_AGP_Attack();
    self.vr_ag_act_hit = 2; // (no blow dealt: the air, never a neighbour in the row)
    self.vr_ag_act_hitl = 1;
    self = oldself;
};
