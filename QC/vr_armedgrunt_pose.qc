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
float vr_agp_arms;            // VR_AGP_Pose: vr_agarms this frame

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
            fist = VR_AGP_Bezier(wind, ctl, hit, u * u * (2 - u)); // (fast out, slowing into the target)
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
        if(!VR_AGP_TwoHanded(p))
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
    vector h = at + fw * sec_x + side * sec_y + up * sec_z;
    float most = VR_AGP_SAW_REACH * vr_am_arm;
    float d = vlen(h - vr_am_sh_l);
    if(d > most)
    {
        at = at - normalize(h - vr_am_sh_l) * (d - most);
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
    if(pL || !VR_AGP_Long(p))
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

// `g`'s pose this server frame, its weapon `p` (world: its fists) placed by it and the pose sent (QVR_SVC_ARMPOSE).
void(entity g, entity p) VR_AGP_Pose =
{
    if(cvar("vr_agrunt_freeze") && g.health > 0)
    {
        // (the Grip Editor: held still in its first idle frame, its weapons ready, its AI held)
        g.nextthink = time + 0.5;
        g.enemy = world;
        g.vr_ag_act = 0;
        g.velocity = '0 0 0';
        g.frame = VR_AM_StandFrame(g, 0);
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
    vr_agp_wl = 0;
    vr_agp_vl = '0 0 0';
    vr_agp_fw = gx;
    vr_agp_up = gz;
    vr_agp_wo = 1;
    float c = p ? VR_AG_Class(p.weapon) : VR_AG_CLASS_FISTS;
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
        }
    }
    else if(vr_agp_arms)
    {
        VR_AGP_ArmsGun(g, p, pL);
        VR_AGPar_Pose(g, p);
    }
    else
    {
        VR_AGP_Gun(g, p, pL, gx, gz);
        VR_AGPar_Pose(g, p);
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
            vr_agp_fwl = normalize(g.vr_ag_took_fw + (vr_agp_fwl - g.vr_ag_took_fw) * u);
            vr_agp_upl = normalize(g.vr_ag_took_up + (vr_agp_upl - g.vr_ag_took_up) * u);
            vr_agp_upl = normalize(vr_agp_upl - vr_agp_fwl * (vr_agp_upl * vr_agp_fwl));
        }
        else
        {
            posL = lay + (posL - lay) * u;
            vr_agp_wr = 0;
            vr_agp_v = posL;
            vr_agp_fw = normalize(g.vr_ag_took_fw + (vr_agp_fw - g.vr_ag_took_fw) * u);
            vr_agp_up = normalize(g.vr_ag_took_up + (vr_agp_up - g.vr_ag_took_up) * u);
            vr_agp_up = normalize(vr_agp_up - vr_agp_fw * (vr_agp_up * vr_agp_fw));
        }
    }
    if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
    {
        dprint(sprintf("agpose %s act %g kind %g s %.3f wr %.2f grip %v", p ? p.netname : "fists", g.vr_ag_act,
            g.vr_ag_act_kind, time - g.vr_ag_act_at, vr_agp_wr, posL)); // (sprintf: 7 arguments at most)
        dprint(sprintf(" fw %v up %v\n", VR_AGP_ToLocal(vr_agp_fw), VR_AGP_ToLocal(vr_agp_up)));
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
        if(self.vr_ag_prop2)
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
    ai_face();
    float s = time - self.vr_ag_act_at;
    VR_AGP_Times(self);
    self.frame = VR_AM_StandFrame(self, floor(s * 10)); // (its stand frames: its legs under the arms)
    if(s < vr_agp_tw + vr_agp_ts)
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
    VR_AG_AfterAttack();
};
