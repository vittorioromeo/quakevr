//
//
//
// ----------------------------------------------------------------------------
// Armed grunts: shooting the weapon out of its hand (ROUND22.md, "Armed grunts: shooting the weapon out";
// vr_agrunt_shotdisarm*, VR Settings > Armed Grunts > Shooting It Out)
// ----------------------------------------------------------------------------
//
// - What a shot hits: a player's pellet (its traced path up to where it stopped: FireBulletsImpl), lightning bolt
//   (LightningDamage) or nail, rocket or grenade (its flight each frame, VR_AGS_Frame: a frame back and a frame ahead, the
//   grunt's own model in the way first: not counted) meeting a held weapon (its line from 8 units behind the grip to the
//   muzzle, VR_AG_WeaponLine, VR_AGS_W_RADIUS round) or the hand holding it (the grip, VR_AGS_H_RADIUS round, and the
//   forearm: VR_AGS_FOREARM units on towards its shoulder). The first of them along the path counts; the volumes are
//   times vr_agrunt_shotdisarm_size, and a path within vr_agrunt_shotdisarm_near units beyond them counts at half.
//   vr_agrunt_shotdisarm_parts: 0 the weapon only, 1 the hand only, 2 either. Each weapon (two held: each hand's) on its own.
// - What it counts: its damage (a pellet's, a nail's, a bolt's, a rocket's direct hit; not its positional factor) times
//   its kind's weight (_w_pellet, _w_nail, _w_lightning, _w_blast) and the part's (_weapon_w, _hand_w). An explosion
//   (_w_splash, 0: never) counts its damage to the grunt on each weapon it holds.
// - Adding up: over vr_agrunt_shotdisarm_window s from the first hit (then it starts over). Reaching
//   vr_agrunt_shotdisarm_damage, it rolls once (the next frame: a blast's pellets all count first) with the chance
//   vr_agrunt_shotdisarm_chance plus _chance_scale for each threshold past it (at most 0.95), and starts over; failed, no
//   hits add up for vr_agrunt_shotdisarm_cooldown s. Won: the weapon flies off along the shots' way (VR_AG_Disarm: its
//   clatter, its grunt), sparks and a ricochet where it was.
// - The shots themselves: a hit on the hand hurts the grunt as ever (it is its body). A pellet or a nail whose first hit
//   along its way is the weapon itself (its volume at size 1, not near it; crossing the weapon's line before it reaches the
//   body: VR_AGS_Blocks) stops there with vr_agrunt_shotdisarm_block (default 1; even with shot disarms off): sparks, a
//   tink, no damage and no blood on the holder; each pellet of a blast on its own. Off: on into whatever is behind it.
//   Lightning (a wide beam) and explosions (their blast hurts it either way) are not stopped. A player's shot no longer
//   disarms by VR_AG_Hit's blow rule while this is on (VR_AGS_Hit): only by this one.
// - vr_agrunt_shotdisarm_debug: 1 prints each player's shot on an armed grunt (`shotdisarm:` its part: weapon, hand or
//   body; the damage adding up; each roll and its result); 2 also shows each held weapon's and hand's volume (sparks).

float VR_AGS_W_RADIUS = 3;   // units round a held weapon's line
float VR_AGS_H_RADIUS = 3.5; // ... round its hand and forearm
float VR_AGS_FOREARM = 11;   // units from the grip towards its shoulder
float VR_AGS_MAX_CHANCE = 0.95;

.float vr_ags_acc;      // a held weapon (its prop): the shots' damage adding up on it or its hand
.float vr_ags_start;    // ... since
.vector vr_ags_dir;     // ... their ways, by their damage
.float vr_ags_pend;     // ... past the threshold: its roll due (VR_AGS_Frame)
.float vr_ags_cool;     // ... nothing adds up before this (a roll failed)
.float vr_ags_shown;    // ... its volumes last shown (debug 2)
.entity vr_ags_by;      // a projectile: the held weapon it last counted on (once each)

float vr_ags_s;         // VR_AGS_Closest's: where along the first segment (0..1)
vector vr_ags_ha;       // VR_AGS_HandLine's: the grip,
vector vr_ags_hb;       // ... the forearm's far end
entity vr_ags_mp;       // VR_AGS_Meet's: the held weapon met (world: none),
float vr_ags_mpart;     // ... 1 the weapon, 2 the hand
float vr_ags_mf;        // ... 1 on it, 0.5 near it
float vr_ags_ms;        // ... where along the path
float vr_ags_rolls;     // the tests: rolls,
float vr_ags_disarms;   // ... weapons shot out,
float vr_ags_hits_w;    // ... hits counted on weapons,
float vr_ags_hits_h;    // ... on hands,
float vr_ags_absorbed;  // ... pellets and nails a weapon stopped
float vr_ags_last_acc;  // ... the last roll's damage
float vr_ags_exact;     // VR_AGS_MeetOne: the true volumes (both parts, size 1, no near misses: VR_AGS_Blocks)
.float vr_ags_tink_at;  // a held weapon: its last blocked shot's tink (one a blast)

void(string s) VR_AGS_Log =
{
    if(cvar_hget(cvarh_vr_agrunt_shotdisarm_debug))
    {
        bprint(sprintf("shotdisarm: %s\n", s)); // (without developer 1: a debug view)
    }
};

string(float kind) VR_AGS_KindName =
{
    if(kind == QVR_AGS_PELLET) { return "pellet"; }
    if(kind == QVR_AGS_NAIL) { return "nail"; }
    if(kind == QVR_AGS_LIGHTNING) { return "bolt"; }
    if(kind == QVR_AGS_BLAST) { return "blast"; }
    return "splash";
};

float(float kind) VR_AGS_KindWeight =
{
    if(kind == QVR_AGS_PELLET) { return cvar_hget(cvarh_vr_agrunt_shotdisarm_w_pellet); }
    if(kind == QVR_AGS_NAIL) { return cvar_hget(cvarh_vr_agrunt_shotdisarm_w_nail); }
    if(kind == QVR_AGS_LIGHTNING) { return cvar_hget(cvarh_vr_agrunt_shotdisarm_w_lightning); }
    if(kind == QVR_AGS_BLAST) { return cvar_hget(cvarh_vr_agrunt_shotdisarm_w_blast); }
    return cvar_hget(cvarh_vr_agrunt_shotdisarm_w_splash);
};

// The least distance between the segments a0..a1 and b0..b1; vr_ags_s: where along a0..a1 (VR_AG_SegDist's, with it).
float(vector a0, vector a1, vector b0, vector b1) VR_AGS_Closest =
{
    vector d1 = a1 - a0;
    vector d2 = b1 - b0;
    vector r = a0 - b0;
    float a = d1 * d1;
    float e = d2 * d2;
    float f = d2 * r;
    float s = 0;
    float t = 0;
    if(a <= 0.001 && e <= 0.001)
    {
        vr_ags_s = 0;
        return vlen(r);
    }
    if(a <= 0.001)
    {
        t = min(1, max(0, f / e));
    }
    else
    {
        float c = d1 * r;
        if(e <= 0.001)
        {
            s = min(1, max(0, -c / a));
        }
        else
        {
            float b = d1 * d2;
            float den = a * e - b * b;
            s = den != 0 ? min(1, max(0, (b * f - c * e) / den)) : 0;
            t = (b * s + f) / e;
            if(t < 0)
            {
                t = 0;
                s = min(1, max(0, -c / a));
            }
            else if(t > 1)
            {
                t = 1;
                s = min(1, max(0, (b - c) / a));
            }
        }
    }
    vr_ags_s = s;
    return vlen(a0 + d1 * s - (b0 + d2 * t));
};

// The hand holding `p` and its forearm (vr_ags_ha the grip, vr_ags_hb VR_AGS_FOREARM on towards its shoulder).
void(entity g, entity p) VR_AGS_HandLine =
{
    vector sh = '0 0 0';
    float rowed = FALSE;
    if(cvar_hget(cvarh_vr_agarms))
    {
        // (vr_agarms: its row's shoulder, carried by its chest's anchor as its arms are drawn from it)
        vector an = VR_AM_Anchor(g);
        if(trace_fraction != 0)
        {
            VR_AM_Row(g.vr_ag_kind);
            vector l = (p.vr_ag_left ? vr_am_sh_l : vr_am_sh_r) - vr_am_anchor_at;
            makevectors('0 1 0' * g.angles_y);
            sh = an + v_forward * l_x - v_right * l_y + '0 0 1' * l_z;
            // (its forearm towards its elbow, the engine's IK's: down and out between its shoulder and its hand)
            float side = p.vr_ag_left ? -1 : 1;
            sh = (sh + p.origin) * 0.5 + ('0 0 -0.3' * vr_am_arm) + v_right * (side * 0.2 * vr_am_arm);
            rowed = TRUE;
        }
    }
    if(!rowed)
    {
        makevectors('0 1 0' * g.angles_y);
        sh = g.origin + v_right * (p.vr_ag_left ? -7 : 7) + '0 0 1' * (g.maxs_z * 0.45);
    }
    vector d = sh - p.origin;
    vr_ags_ha = p.origin;
    vr_ags_hb = vlen(d) > 1 ? p.origin + normalize(d) * min(VR_AGS_FOREARM, vlen(d)) : p.origin;
};

// Whether the path a..b meets the held weapon `p` (of `g`) or its hand before vr_ags_ms: if so, it is the one met
// (vr_ags_mp, vr_ags_mpart, vr_ags_mf, vr_ags_ms). The weapon first: a tie is the weapon's.
void(entity g, entity p, vector a, vector b) VR_AGS_MeetOne =
{
    if(VR_AGS_Closest(a, b, p.origin, p.origin) > 96)
    {
        return;
    }
    float parts = vr_ags_exact ? 2 : cvar_hget(cvarh_vr_agrunt_shotdisarm_parts);
    float size = vr_ags_exact ? 1 : max(0, cvar_hget(cvarh_vr_agrunt_shotdisarm_size));
    float near = vr_ags_exact ? 0 : max(0, cvar_hget(cvarh_vr_agrunt_shotdisarm_near));
    float d;
    float r;
    if(parts != 1)
    {
        VR_AG_WeaponLine(p);
        d = VR_AGS_Closest(a, b, vr_ag_wa, vr_ag_wb);
        r = VR_AGS_W_RADIUS * size;
        if(d <= r + near && vr_ags_s < vr_ags_ms)
        {
            vr_ags_mp = p;
            vr_ags_mpart = 1;
            vr_ags_mf = d <= r ? 1 : 0.5;
            vr_ags_ms = vr_ags_s;
        }
    }
    if(parts != 0)
    {
        VR_AGS_HandLine(g, p);
        d = VR_AGS_Closest(a, b, vr_ags_ha, vr_ags_hb);
        r = VR_AGS_H_RADIUS * size;
        if(d <= r + near && vr_ags_s < vr_ags_ms)
        {
            vr_ags_mp = p;
            vr_ags_mpart = 2;
            vr_ags_mf = d <= r ? 1 : 0.5;
            vr_ags_ms = vr_ags_s;
        }
    }
};

// Whether `p` is a weapon a living armed grunt holds now (not in a tug-of-war).
float(entity p) VR_AGS_Held =
{
    entity g = p.owner;
    return g.vr_ag && g.health > 0 && !g.vr_ag_tug_pl && (g.vr_ag_prop == p || g.vr_ag_prop2 == p);
};

// The path a..b against every held weapon and hand: the first met (vr_ags_mp; world none).
void(vector a, vector b) VR_AGS_Meet =
{
    vr_ags_mp = world;
    vr_ags_ms = 2;
    entity p = find(world, classname, "vr_ag_weapon");
    while(p)
    {
        if(VR_AGS_Held(p))
        {
            VR_AGS_MeetOne(p.owner, p, a, b);
        }
        p = find(p, classname, "vr_ag_weapon");
    }
};

// `amount` more on the held weapon `p` (its `part`, 1 the weapon or 2 the hand, met at `at` going `dir`).
void(entity p, float amount, vector dir, float part, vector at, float kind) VR_AGS_Add =
{
    if(amount <= 0)
    {
        return;
    }
    string what = sprintf("%s on its %s%s (%s)", VR_AGS_KindName(kind), part == 1 ? "weapon" : "hand",
        (vr_ags_mf < 1 && kind != QVR_AGS_SPLASH) ? ", near" : "", p.netname);
    if(time < p.vr_ags_cool)
    {
        VR_AGS_Log(sprintf("%s: %.1f, cooling down (%.2f s)", what, amount, p.vr_ags_cool - time));
        return;
    }
    if(p.vr_ags_acc <= 0 || time - p.vr_ags_start > cvar_hget(cvarh_vr_agrunt_shotdisarm_window))
    {
        p.vr_ags_acc = 0;
        p.vr_ags_start = time;
        p.vr_ags_dir = '0 0 0';
    }
    p.vr_ags_acc = p.vr_ags_acc + amount;
    p.vr_ags_dir = p.vr_ags_dir + normalize(dir) * amount;
    if(part == 1) { vr_ags_hits_w = vr_ags_hits_w + 1; } else { vr_ags_hits_h = vr_ags_hits_h + 1; }
    if(part == 1 && kind != QVR_AGS_SPLASH)
    {
        particle2(at, '0 0 20', QVR_PARTICLE_PRESET_SPARKS, 2); // (metal struck)
    }
    float need = max(0.01, cvar_hget(cvarh_vr_agrunt_shotdisarm_damage));
    if(p.vr_ags_acc >= need)
    {
        p.vr_ags_pend = TRUE;
    }
    VR_AGS_Log(sprintf("%s: %.1f, %.1f of %g in %.2f s", what, amount, p.vr_ags_acc, need, time - p.vr_ags_start));
};

// Whether a pellet's or nail's way a..b (b where it struck, on `xEnd`; or `xBody` along a..b its holder's body first,
// -1 none) is stopped by a held weapon (vr_agrunt_shotdisarm_block): its first hit along the way the weapon itself at the
// true volumes (the hand first: the body's), crossing the weapon's line at least half a unit before the body (a stock
// against its chest: the chest's). `xOnly`: that weapon alone (world: every held one). vr_ags_bp: the weapon (world: not
// stopped), vr_ags_bs: where along a..b. VR_AGS_Meet's results kept.
entity vr_ags_bp;
float vr_ags_bs;
float(entity xOnly, vector a, vector b, entity xEnd, float xBody) VR_AGS_Blocks =
{
    vr_ags_bp = world;
    if(!cvar_hget(cvarh_vr_agrunt_shotdisarm_block))
    {
        return FALSE;
    }
    entity kp = vr_ags_mp;
    float kpart = vr_ags_mpart;
    float kf = vr_ags_mf;
    float ks = vr_ags_ms;
    vr_ags_exact = TRUE;
    if(xOnly)
    {
        vr_ags_mp = world;
        vr_ags_ms = 2;
        VR_AGS_MeetOne(xOnly.owner, xOnly, a, b);
    }
    else
    {
        VR_AGS_Meet(a, b);
    }
    vr_ags_exact = FALSE;
    float len = vlen(b - a);
    float ok = vr_ags_mp && vr_ags_mpart == 1 && vr_ags_mf >= 1;
    if(ok && xEnd && xEnd == vr_ags_mp.owner && (1 - vr_ags_ms) * len < 0.5)
    {
        ok = FALSE; // (its body where the weapon is: the body's)
    }
    if(ok && xBody >= 0 && vr_ags_ms * len + 0.5 > xBody * len)
    {
        ok = FALSE;
    }
    if(ok)
    {
        vr_ags_bp = vr_ags_mp;
        vr_ags_bs = vr_ags_ms;
    }
    vr_ags_mp = kp;
    vr_ags_mpart = kpart;
    vr_ags_mf = kf;
    vr_ags_ms = ks;
    return ok;
};

// A shot stopped on the held weapon `p` at `at`: sparks, a tink (one a blast's pellets), no blood.
void(entity p, vector at, float kind) VR_AGS_Stopped =
{
    vr_ags_absorbed = vr_ags_absorbed + 1;
    particle2(at, '0 0 40', QVR_PARTICLE_PRESET_SPARKS, 4);
    if(time - p.vr_ags_tink_at >= 0.1)
    {
        p.vr_ags_tink_at = time;
        sound(p, CHAN_AUTO, kind == QVR_AGS_NAIL ? "weapons/tink1.wav" : sprintf("weapons/ric%g.wav", 1 + min(2,
            floor(random() * 3))), 1, ATTN_NORM);
    }
    VR_AGS_Log(sprintf("%s stopped on its weapon (%s): no damage to it", VR_AGS_KindName(kind), p.netname));
};

float(entity xFrom, vector xA, vector xB, vector xDir, float xDamage, float xKind) VR_AGS_Shot =
{
    float counts = cvar_hget(cvarh_vr_agrunt_shotdisarm);
    float blocks = xKind == QVR_AGS_PELLET && cvar_hget(cvarh_vr_agrunt_shotdisarm_block);
    if(!(counts || blocks) || !(xFrom.flags & FL_CLIENT))
    {
        return FALSE;
    }
    // (the caller's trace and vectors as they were: its next pellets' spread, its hit)
    vector fw = v_forward;
    vector rt = v_right;
    vector up = v_up;
    entity te = trace_ent;
    vector tp = trace_endpos;
    float tf = trace_fraction;
    vector tn = trace_plane_normal;
    float stopped = FALSE;
    if(blocks)
    {
        stopped = VR_AGS_Blocks(world, xA, xB, tf < 1 ? te : world, -1);
    }
    if(counts)
    {
        VR_AGS_Meet(xA, xB);
    }
    else
    {
        vr_ags_mp = world;
    }
    if(vr_ags_mp)
    {
        vector at = xA + (xB - xA) * vr_ags_ms;
        float w = vr_ags_mpart == 1 ? cvar_hget(cvarh_vr_agrunt_shotdisarm_weapon_w) : cvar_hget(cvarh_vr_agrunt_shotdisarm_hand_w);
        VR_AGS_Add(vr_ags_mp, xDamage * VR_AGS_KindWeight(xKind) * w * vr_ags_mf, xDir, vr_ags_mpart, at, xKind);
    }
    else if(counts && te.vr_ag && te.health > 0)
    {
        VR_AGS_Log(sprintf("%s on its body", VR_AGS_KindName(xKind)));
    }
    if(stopped)
    {
        VR_AGS_Stopped(vr_ags_bp, xA + (xB - xA) * vr_ags_bs, xKind);
    }
    v_forward = fw;
    v_right = rt;
    v_up = up;
    trace_ent = te;
    trace_endpos = tp;
    trace_fraction = tf;
    trace_plane_normal = tn;
    return stopped;
};

float(entity targ, entity inflictor, entity attacker, float take) VR_AGS_Hit =
{
    if(!cvar_hget(cvarh_vr_agrunt_shotdisarm) || !targ.vr_ag || !(attacker.flags & FL_CLIENT))
    {
        return FALSE;
    }
    if(vr_hitkind == QVR_HITKIND_SPLASH)
    {
        float w = cvar_hget(cvarh_vr_agrunt_shotdisarm_w_splash) * cvar_hget(cvarh_vr_agrunt_shotdisarm_weapon_w);
        if(w > 0 && targ.health > 0 && !targ.vr_ag_tug_pl && cvar_hget(cvarh_vr_agrunt_shotdisarm_parts) != 1)
        {
            vr_ags_mf = 1;
            if(targ.vr_ag_prop)
            {
                VR_AGS_Add(targ.vr_ag_prop, take * w, targ.vr_ag_prop.origin - inflictor.origin, 1, targ.vr_ag_prop.origin,
                    QVR_AGS_SPLASH);
            }
            if(targ.vr_ag_prop2)
            {
                VR_AGS_Add(targ.vr_ag_prop2, take * w, targ.vr_ag_prop2.origin - inflictor.origin, 1,
                    targ.vr_ag_prop2.origin, QVR_AGS_SPLASH);
            }
        }
        return FALSE;
    }
    return vr_hitkind == QVR_HITKIND_SHOT;
};

// A player's projectile's kind (-1: not one that counts).
float(entity e) VR_AGS_ProjKind =
{
    if(e.touch == spike_touch || e.touch == superspike_touch)
    {
        return QVR_AGS_NAIL;
    }
    if(e.touch == T_MissileTouch || e.touch == GrenadeTouch)
    {
        return QVR_AGS_BLAST;
    }
    return -1;
};

float(entity e, float kind) VR_AGS_ProjDamage =
{
    if(e.touch == superspike_touch)
    {
        return VR_WpnDmg(e.owner, cvarh_vr_dmg_super_nail, 18);
    }
    if(kind == QVR_AGS_NAIL)
    {
        return VR_WpnDmg(e.owner, cvarh_vr_dmg_nail, 9);
    }
    return 110 * VR_WpnDmgScale(e.owner, cvarh_vr_dmg_rocket, 100);
};

// The players' nails, rockets and grenades flying through `p` or its hand this frame (a frame back, a frame ahead).
void(entity g, entity p) VR_AGS_Projectiles =
{
    VR_AG_WeaponLine(p);
    entity e = findradius((vr_ag_wa + vr_ag_wb) * 0.5, 128);
    while(e)
    {
        entity next = e.chain;
        float kind = ((e.owner.flags & FL_CLIENT) && e.vr_ags_by != p && e.velocity != '0 0 0') ? VR_AGS_ProjKind(e) : -1;
        if(kind >= 0)
        {
            vector a = e.origin - e.velocity * frametime;
            vector b = e.origin + e.velocity * frametime;
            float len = vlen(b - a);
            float body = -1;
            if(cvar_hget(cvarh_vr_agrunt_shotdisarm))
            {
                vr_ags_mp = world;
                vr_ags_ms = 2;
                VR_AGS_MeetOne(g, p, a, b);
                body = vr_ags_mp ? hitmodel_segment(g, a, b, 0, HITCLASS_GUNS) : -1;
                if(vr_ags_mp && (body < 0 || body * len + 4 >= vr_ags_ms * len)) // (its body first: that it hits)
                {
                    e.vr_ags_by = p;
                    vector at = a + (b - a) * vr_ags_ms;
                    float w = vr_ags_mpart == 1 ? cvar_hget(cvarh_vr_agrunt_shotdisarm_weapon_w) :
                        cvar_hget(cvarh_vr_agrunt_shotdisarm_hand_w);
                    VR_AGS_Add(p, VR_AGS_ProjDamage(e, kind) * VR_AGS_KindWeight(kind) * w * vr_ags_mf, e.velocity,
                        vr_ags_mpart, at, kind);
                }
            }
            // A nail whose first hit is the weapon: stopped there (vr_agrunt_shotdisarm_block; its body first: the body's).
            if(kind == QVR_AGS_NAIL && VR_AGS_Blocks(p, a, b, world, -1))
            {
                body = hitmodel_segment(g, a, b, 0, HITCLASS_GUNS);
                if(body < 0 || vr_ags_bs * len + 0.5 <= body * len)
                {
                    VR_AGS_Stopped(p, a + (b - a) * vr_ags_bs, kind);
                    remove(e);
                }
            }
        }
        e = next;
    }
};

// Debug 2: its volumes as sparks (the weapon's line, the hand to the forearm's end), every 0.15 s.
void(entity g, entity p) VR_AGS_Show =
{
    if(time - p.vr_ags_shown < 0.15)
    {
        return;
    }
    p.vr_ags_shown = time;
    float k;
    VR_AG_WeaponLine(p);
    for(k = 0; k <= 6; k++)
    {
        particle2(vr_ag_wa + (vr_ag_wb - vr_ag_wa) * (k / 6), '0 0 0', QVR_PARTICLE_PRESET_CONTACTSPARKS, 1);
    }
    VR_AGS_HandLine(g, p);
    for(k = 0; k <= 3; k++)
    {
        particle2(vr_ags_ha + (vr_ags_hb - vr_ags_ha) * (k / 3), '0 0 0', QVR_PARTICLE_PRESET_SPARKS, 1);
    }
};

// VR_AG_WeaponStruck (its prop's think, every frame; `p` in its first slot): the flights through it, then its roll if one
// is due. TRUE: shot out of its hand.
float(entity g, entity p) VR_AGS_Frame =
{
    if(!(cvar_hget(cvarh_vr_agrunt_shotdisarm) || cvar_hget(cvarh_vr_agrunt_shotdisarm_block)) || g.vr_ag_tug_pl ||
       g.vr_ag_prop != p)
    {
        p.vr_ags_pend = FALSE;
        return FALSE;
    }
    if(cvar_hget(cvarh_vr_agrunt_shotdisarm_debug) >= 2)
    {
        VR_AGS_Show(g, p);
    }
    VR_AGS_Projectiles(g, p); // (the nails it stops even with shot disarms off: vr_agrunt_shotdisarm_block)
    if(!cvar_hget(cvarh_vr_agrunt_shotdisarm))
    {
        p.vr_ags_pend = FALSE;
        return FALSE;
    }
    if(!p.vr_ags_pend)
    {
        return FALSE;
    }
    p.vr_ags_pend = FALSE;
    float need = max(0.01, cvar_hget(cvarh_vr_agrunt_shotdisarm_damage));
    float acc = p.vr_ags_acc;
    float chance = cvar_hget(cvarh_vr_agrunt_shotdisarm_chance) +
        cvar_hget(cvarh_vr_agrunt_shotdisarm_chance_scale) * (acc / need - 1);
    chance = min(VR_AGS_MAX_CHANCE, max(0, chance));
    float r = random();
    vector dir = p.vr_ags_dir;
    p.vr_ags_acc = 0;
    vr_ags_rolls = vr_ags_rolls + 1;
    vr_ags_last_acc = acc;
    if(r >= chance)
    {
        p.vr_ags_cool = time + max(0, cvar_hget(cvarh_vr_agrunt_shotdisarm_cooldown));
        VR_AGS_Log(sprintf("roll on %s: %.1f damage, chance %.2f, rolled %.2f: holds on", p.netname, acc, chance, r));
        return FALSE;
    }
    VR_AGS_Log(sprintf("roll on %s: %.1f damage, chance %.2f, rolled %.2f: shot out of its hand", p.netname, acc, chance, r));
    VR_AG_WeaponLine(p);
    particle2((vr_ag_wa + vr_ag_wb) * 0.5, '0 0 60', QVR_PARTICLE_PRESET_SPARKS, 12);
    sound(g, CHAN_AUTO, sprintf("weapons/ric%g.wav", 1 + min(2, floor(random() * 3))), 1, ATTN_NORM);
    vr_ags_disarms = vr_ags_disarms + 1;
    VR_AG_Disarm(g, dir, 150 + min(150, acc * 3), sprintf("shot out of its hand, %.1f damage", acc));
    return TRUE;
};
