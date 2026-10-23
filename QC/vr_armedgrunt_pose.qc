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
    vr_agp_wr = 1 - k;
    vr_agp_v = r * k;
    vr_agp_flags = VR_AGP_LEFT;
    vr_agp_wl = 1 - k;
    vr_agp_vl = l * k;
};

// A melee weapon: ready (blade up, mostly its own place), a swing's arc through the target; a chainsaw at the hip,
// running, thrust into the target and held there cutting.
void(entity g, entity p, vector handL, vector fwCarry, vector upCarry) VR_AGP_Melee =
{
    float saw = p.weapon == WID_CHAINSAW;
    g.vr_ag_readyw = VR_AGP_Ease(g.vr_ag_readyw, 0.7, 3);
    float k = g.vr_ag_readyw;
    vector ready = saw ? VR_AGP_SAW_READY : VR_AGP_READY;
    vector readyFw = normalize(saw ? VR_AGP_SAW_FW : VR_AGP_READY_FW);
    vector pos = ready;
    vector fw = readyFw;
    vector edge = '1 0 0';
    float s = time - g.vr_ag_act_at;
    if(g.vr_ag_act == VR_AGP_SWING || g.vr_ag_act == VR_AGP_SAW)
    {
        VR_AGP_Times(g);
        vector tl = VR_AGP_TargetLocal(g);
        float dz = min(12, max(-12, tl_z - 8));
        vector w; vector wfw; vector c; vector e; vector efw;
        float kind = g.vr_ag_act_kind;
        if(saw)
        {
            w = '5 -8 4'; wfw = '0.95 0 0.3';
            e = '16 -4 0'; e_z = e_z + dz;
            efw = normalize(tl - e);
            c = (w + e) * 0.5;
        }
        else if(kind == 0) { w = '0 -7 24'; wfw = '-0.55 0.05 0.83'; c = '16 -6 22'; e = '17 -3 2'; efw = '0.6 0 -0.8'; }
        else if(kind == 1) { w = '1 -14 21'; wfw = '-0.4 -0.55 0.73'; c = '17 -10 16'; e = '14 7 0'; efw = '0.5 0.55 -0.67'; }
        else if(kind == 2) { w = '3 -17 12'; wfw = '-0.45 -0.85 0.25'; c = '20 -6 12'; e = '9 13 10'; efw = '0.25 0.95 0.1'; }
        else               { w = '8 12 6'; wfw = '0.1 0.8 -0.55'; c = '19 2 12'; e = '10 -14 20'; efw = '0.3 -0.7 0.65'; }
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
    // Carried (k 0): along the animation's rifle line the other way (its rear over the shoulder: the old carry);
    // ready and swinging (k 1): its own.
    vector fwW = VR_AGP_Nlerp(fwCarry, VR_AGP_ToWorld(fw), k);
    vector upW = VR_AGP_Nlerp(upCarry, VR_AGP_ToWorld(edge), k);
    vr_agp_wr = 1 - k;
    vr_agp_v = pos * k;
    vr_agp_fw = fwW;
    vr_agp_up = upW;
    vr_agp_wo = 0;
    if(!VR_AGP_TwoHanded(p))
    {
        vr_agp_flags = VR_AGP_LEFT;
        vr_agp_wl = 1 - k;
        vr_agp_vl = VR_AGP_FREE_L * k;
    }
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
    if(cutting && inst.wi_clip > 0 && time >= g.vr_ag_saw_next)
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

// A gun: carried along its animation's rifle line; aimed (VR_AG_AtkFrame sets vr_ag_aim), turned at its target
// (eased); a shot's kick back and up.
void(entity g, vector gx, vector gz) VR_AGP_Gun =
{
    if(g.vr_ag_aim != '0 0 0')
    {
        g.vr_ag_aimdir = g.vr_ag_aim;
    }
    g.vr_ag_aimw = VR_AGP_Ease(g.vr_ag_aimw, g.vr_ag_aim != '0 0 0' ? 1 : 0, g.vr_ag_aim != '0 0 0' ? 7 : 4);
    float a = VR_AGP_Smooth(g.vr_ag_aimw);
    vector fw = gx;
    if(a > 0 && g.vr_ag_aimdir != '0 0 0')
    {
        fw = VR_AGP_Nlerp(gx, g.vr_ag_aimdir, a);
    }
    vector up = normalize(gz - fw * (gz * fw));
    if(up == '0 0 0') { up = '0 0 1'; }
    vector right = VR_AGP_Cross(fw, up);
    float k = 1 - (time - g.vr_ag_kick_at) / 0.2;
    vector kick = '0 0 0';
    if(g.vr_ag_kick_at > 0 && k > 0)
    {
        float big = VR_AG_Class(g.vr_ag_wid) == VR_AG_CLASS_EXPLOSIVE ? 1.5 : 1;
        float kk = k * k * big;
        kick = -fw * (4 * kk) + '0 0 1' * kk;
        fw = VR_AG_Rotate(fw, right, 14 * kk);
        up = VR_AG_Rotate(up, right, 14 * kk);
    }
    vr_agp_wr = 1;
    vr_agp_v = VR_AGP_ToLocal(kick);
    vr_agp_fw = fw;
    vr_agp_up = up;
    vr_agp_wo = 1 - a;
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
    vector hand = (modelvertex(g, VR_AG_HAND_V1) + modelvertex(g, VR_AG_HAND_V2) + modelvertex(g, VR_AG_HAND_V3)) * (1 / 3);
    if(trace_fraction == 0)
    {
        return; // (no such model now: a ragdoll's, another's; it stays where it was)
    }
    vector rear = (modelvertex(g, VR_AG_GUN_REAR1) + modelvertex(g, VR_AG_GUN_REAR2)) * 0.5;
    vector muzzle = (modelvertex(g, VR_AG_GUN_MUZZLE1) + modelvertex(g, VR_AG_GUN_MUZZLE2)) * 0.5;
    vector gx = normalize(muzzle - rear);
    vector gz = modelvertex(g, VR_AG_GUN_TOP) - modelvertex(g, VR_AG_GUN_BOTTOM);
    gz = normalize(gz - gx * (gz * gx));
    if(gz == '0 0 0') { gz = '0 0 1'; }
    vector gy = VR_AGP_Cross(gz, gx);
    if(g.vr_ag_act && time - g.vr_ag_act_tick > 0.25)
    {
        g.vr_ag_act = 0; // (its attack's frames stopped: a pain, a knockdown)
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
        // The pose is the blade's (or the head's, the bar's) line; the model is drawn turned up from its entity's
        // forward by its Weapon Offsets (a hand's hold): the entity turned down by that so the blade lies where wanted.
        float tilt = VR_AGP_BladeTilt(p);
        vector right = VR_AGP_Cross(vr_agp_fw, vr_agp_up);
        vr_agp_fw = VR_AG_Rotate(vr_agp_fw, right, -tilt);
        vr_agp_up = VR_AG_Rotate(vr_agp_up, right, -tilt);
    }
    else
    {
        VR_AGP_Gun(g, gx, gz);
    }
    VR_AGP_Body(g);
    vector posL = handL * vr_agp_wr + vr_agp_v;
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
        setorigin(p, g.origin + VR_AGP_ToWorld(posL));
        if(p.weapon == WID_CHAINSAW)
        {
            VR_AGP_Saw(g, p);
            VR_AGP_Body(g);
        }
    }

    WriteByte(MSG_BROADCAST, 39); // svc_quakevr
    WriteByte(MSG_BROADCAST, 36); // QVR_SVC_ARMPOSE
    WriteEntity(MSG_BROADCAST, g);
    WriteEntity(MSG_BROADCAST, p);
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
    }
    else
    {
        self.vr_ag_act_kind = 0;
    }
    self.vr_ag_act_at = time;
    self.vr_ag_act_hit = 0;
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
    self.frame = floor(s * 10) & 7; // (the stand frames: its legs under the arms)
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
    VR_AG_AfterAttack();
};
