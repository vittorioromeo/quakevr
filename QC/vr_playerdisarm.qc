//
//
//
// ----------------------------------------------------------------------------
// The player disarmed (ROUND25.md, "The player disarmed"; vr_player_disarm, vr_player_disarm_chance)
// ----------------------------------------------------------------------------
//
// His words (2026-11-15): players get disarmed by melee hits and bullet shots, just as armed humanoids are by a precise
// shot or melee hit on their weapon or hand, "using the same exact rules as what happens for enemies today"; in single
// player as an option, on by default (multiplayer too).
//
// - One rule (vr_armedgrunt_shot.qc VR_Disarm_Add, VR_Disarm_Roll, VR_Disarm_ProjChance): the armed humanoids' own
//   cvars (vr_agrunt_shotdisarm_*), each hand of each player its own holder of the adding up (a "vr_pd_hand" entity),
//   the chance times vr_player_disarm_chance (at most certain).
// - What a shot meets: anyone else's (a monster's, another player's) pellet or bolt's traced path (FireBulletsImpl,
//   LightningDamage: VR_PD_Shot) or projectile's flight (a frame back, a frame ahead: VR_PD_Frame) on a held weapon's
//   line as the melee and the parry lay it out (combat.qc VR_Parry_WeaponLine; VR_AGS_W_RADIUS round it, grown by
//   vr_agrunt_vol_shot_weapon) or on the hand holding it (VR_AGS_H_RADIUS, grown by vr_agrunt_vol_shot_hand); within
//   vr_agrunt_shotdisarm_near beyond them at half. vr_agrunt_shotdisarm_parts as for them. An empty hand or a fist: never.
//   The shot itself goes on as ever (it is not stopped on your weapon: your body takes it as before).
// - Blows: a blow with a line (a knight's sword, an ogre's chainsaw, an armed humanoid's swing: vr_dirparry.qc) that lands
//   meeting your weapon (not across it: no parry; within the parry's contact distance plus vr_agrunt_vol_blow) with at
//   least vr_agrunt_disarm_damage: knocked out (the armed humanoids' blow rule), at vr_player_disarm_chance. Never through
//   a parry or the guard: those block the blow before this (combat.qc VR_Parry).
// - Knocked out: the hand's weapon drops as it does when the grip is lost (vr_wpnholster.qc DropWeaponInHandScaled at
//   vr_weight_drop_velocity: a world weapon with its record: its clip, its ammo), pushed along the shots' way; sparks, a
//   ricochet, the hand's buzz, a flash, a line.
//
// PvE's own rule (disarmfix, BACKLOG 6, 2026-11-22: "very inconsistent: hard to predict"): the above stays for PvP (another
// player's or a bot's shots and blows, vr_player_disarm_pvp: as it was). A monster's (and the world's) hits on your weapon
// or the hand holding it give jolts, no dice; vr_player_disarm_jolts (2) jolts on one hand within vr_player_disarm_window
// (3 s, from the last) knock its weapon out; each one short is the warning (the hand knocked aside, a buzz, a rattle, a
// line). What gives a jolt, by blow type and strength (the hit tests as above):
//   - a blow with a line (a sword, an axe, a chainsaw, an armed humanoid's swing) landing on your weapon or its hand, not
//     parried: 1, or 2 when heavy (vr_player_disarm_heavy damage: at once). Any damage (the old 10-damage floor left a
//     knight's sword, 9 at most, never disarming).
//   - a blow landing on your body, not parried: an armed humanoid's fist, butt or shove 1; any other blow 1 when heavy;
//     a lighter one (a bite, a claw, a blade on your body) none. On the held weapon's hand nearest the attacker.
//   - shots and projectiles on your weapon or its hand (pellets, nails, lasers, bolts, grenades, a vore's ball): their
//     damage (times vr_agrunt_shotdisarm_w_* and _weapon_w / _hand_w; a projectile in full within _proj_near of them, a
//     hitscan shot at half within _near) adds up over vr_agrunt_shotdisarm_window; each
//     vr_player_disarm_shot of it is a jolt (a full grunt's blast on your gun 2: out; an enforcer's laser 1; a nail none,
//     two 1; an ogre's grenade: out).
// vr_player_disarm_chance: the knock-out's chance once the jolts are reached (1: certain; a lost roll leaves it one short).
// Blows are read in the parry's resolution (combat.qc VR_Parry): with vr_parry 0 only shots disarm.

.entity vr_pd_main; // the main hand's holder of the adding up (spawned at its first hit)
.entity vr_pd_off;  // ... the off hand's
.float vr_pd_projwon; // a holder: a projectile's own roll won (VR_PD_Frame knocks it out)
.float vr_pd_jolts;   // a holder (PvE): jolts on the hand now,
.float vr_pd_jolt_t;  // ... when the last came (vr_player_disarm_window from it: gone),
.float vr_pd_jnew;    // ... jolts since its last frame (VR_PD_Frame: the warning or the knock-out),
.vector vr_pd_jdir;   // ... their way,
.string vr_pd_jwhy;   // ... the last one's cause,
.float vr_pd_sum;     // ... shots' damage adding up,
.float vr_pd_sum_t;   // ... from when (vr_agrunt_shotdisarm_window)
float vr_pd_jolted;   // jolts given (the tests),
float vr_pd_warns;    // ... warnings (a jolt short of the knock-out)

float vr_pd_hits;      // hits counted on your weapons and hands (the tests),
float vr_pd_rolls;     // ... rolls,
float vr_pd_disarms;   // ... weapons knocked out,
float vr_pd_last_hand; // ... the last one's hand
float vr_pd_hand;      // VR_PD_Meet's: the hand met (-1: none),
float vr_pd_part;      // ... 1 its weapon, 2 the hand,
float vr_pd_s;         // ... where along the path (0..1),
float vr_pd_gap;       // ... the gap (units; 0 or less: on it)

void(string s) VR_PD_Log =
{
    if(cvar_hget(cvarh_vr_agrunt_shotdisarm_debug))
    {
        bprint(sprintf("playerdisarm: %s\n", s));
    }
};

// Whether what `by` does may disarm a player (his call, 2026-11-16): another player's (a bot's too) shots, blows and grabs
// by vr_player_disarm_pvp (PvP, on by default), anything else's (a monster's, the world's) by vr_player_disarm (PvE, off by
// default until it is polished).
float(entity by) VR_PD_ByOn =
{
    return (by.flags & FL_CLIENT) ? cvar_hget(cvarh_vr_player_disarm_pvp) : cvar_hget(cvarh_vr_player_disarm);
};

// Whether `pl` can be disarmed now by anyone (alive and in the game: a leaver's body is DEAD_DEAD, its health kept).
float(entity pl) VR_PD_Can =
{
    // (PvP only where there are other players: single player skips the projectile scan, mpreview D1)
    return (cvar_hget(cvarh_vr_player_disarm) || ((coop || deathmatch) && cvar_hget(cvarh_vr_player_disarm_pvp))) &&
        (pl.flags & FL_CLIENT) &&
        pl.health > 0 && pl.deadflag == DEAD_NO && cvar_hget(cvarh_vr_player_disarm_chance) > 0;
};

// Whether `pl` can be disarmed now by `by`.
float(entity pl, entity by) VR_PD_On =
{
    return VR_PD_ByOn(by) && VR_PD_Can(pl);
};

// `pl`'s hand `h`'s holder of the adding up.
entity(entity pl, float h) VR_PD_Holder =
{
    entity e = h == cVR_MainHand ? pl.vr_pd_main : pl.vr_pd_off;
    if(e == world)
    {
        e = spawn();
        e.classname = "vr_pd_hand";
        e.owner = pl;
        e.cnt = h;
        if(h == cVR_MainHand) { pl.vr_pd_main = e; } else { pl.vr_pd_off = e; }
    }
    return e;
};

// The path a..b against `pl`'s held weapons and the hands holding them: the first met within `near` units beyond them
// (vr_pd_hand, -1 none; vr_pd_part, vr_pd_s, vr_pd_gap).
void(entity pl, vector a, vector b, float near) VR_PD_Meet =
{
    vr_pd_hand = -1;
    vr_pd_s = 2;
    vr_pd_gap = 999;
    float parts = cvar_hget(cvarh_vr_agrunt_shotdisarm_parts);
    float d;
    for(float h = 0; h <= 1; h++)
    {
        if(!VR_Parry_WeaponLine(pl, h)) // (an empty hand, a fist: nothing to knock out)
        {
            continue;
        }
        if(parts != 1)
        {
            d = VR_AGS_Closest(a, b, vr_parry_butt, vr_parry_end) - VR_AGS_W_RADIUS -
                cvar_hget(cvarh_vr_agrunt_vol_shot_weapon);
            if(d <= near && vr_ags_s < vr_pd_s)
            {
                vr_pd_hand = h;
                vr_pd_part = 1;
                vr_pd_s = vr_ags_s;
                vr_pd_gap = d;
            }
        }
        if(parts != 0)
        {
            vector hp = VRGetEntHandPos(pl, h);
            d = VR_AGS_Closest(a, b, hp, hp) - VR_AGS_H_RADIUS - cvar_hget(cvarh_vr_agrunt_vol_shot_hand);
            if(d <= near && vr_ags_s < vr_pd_s)
            {
                vr_pd_hand = h;
                vr_pd_part = 2;
                vr_pd_s = vr_ags_s;
                vr_pd_gap = d;
            }
        }
    }
};

// A hit met at `at`: on the weapon (vr_pd_part 1), a few sparks (metal struck).
void(vector at) VR_PD_Struck =
{
    if(vr_pd_part == 1)
    {
        particle2(at, '0 0 20', QVR_PARTICLE_PRESET_SPARKS, 2);
    }
};

// `amount` more on `pl`'s hand `h` (met at `at`, going `dir`, by `by`).
void(entity pl, float h, float amount, vector dir, entity by, vector at, string what) VR_PD_Add =
{
    if(amount <= 0)
    {
        return;
    }
    entity st = VR_PD_Holder(pl, h);
    st.vr_ags_shooter = by;
    vr_pd_hits = vr_pd_hits + 1;
    if(!VR_Disarm_Add(st, amount, dir))
    {
        VR_PD_Log(sprintf("%s: %.1f, cooling down (%.2f s)", what, amount, st.vr_ags_cool - time));
        return;
    }
    VR_PD_Struck(at);
    VR_PD_Log(sprintf("%s: %.1f, %.1f of %g", what, amount, st.vr_ags_acc,
        max(0.01, cvar_hget(cvarh_vr_agrunt_shotdisarm_damage))));
};

// A weapon of yours knocked out or taken (VR_Player_Disarm: shots, blows, another player's grab): its sounds at
// vr_player_disarm_sound. A metal clang at the weapon (the one dropped, else the hand) for everyone near, and a short
// alert sting to `pl` alone, not placed (the client's playvol: a local sound). Sounds: make_disarm_sounds.py (CC0).
float vr_pd_sounds; // the disarm sounds started (the tests)
void(entity pl, float h) VR_PD_DisarmSound =
{
    float vol = min(1, max(0, cvar_hget(cvarh_vr_player_disarm_sound)));
    if(vol <= 0)
    {
        return;
    }
    entity at = vr_wpn_dropped != world ? vr_wpn_dropped : pl;
    sound(at, at == pl ? CHAN_AUTO : CHAN_BODY, "vr/disarm_clang.wav", vol, ATTN_NORM);
    if(pl.flags & FL_CLIENT)
    {
        stuffcmd(pl, sprintf("playvol vr/disarm_sting.wav %g\n", vol));
    }
    vr_pd_sounds = vr_pd_sounds + 1;
    dprint(sprintf("playerdisarm: sounds: the clang at %s for everyone near, the sting to %s (hand %g, volume %g)\n",
        at == pl ? "your hand" : "the weapon", pl.netname, h, vol));
};

// The one way a weapon leaves a player's hand against his will, any player's (single player's, a listen host's own, a
// remote one, a bot), whatever did it: player `v`'s weapon in hand `vh` knocked out (`taker` world: it drops as the grip
// lost drops it, a world weapon with its record, pushed along `dir` as hard as `speed`) or taken (into `taker`'s hand
// `th`: another player's grab, vr_playerdisarm_grab.qc). Shots and blows: VR_PD_Disarm. TRUE: done.
float(entity v, float vh, entity taker, float th, vector dir, float speed, string why) VR_Player_Disarm =
{
    if(!(v.flags & FL_CLIENT) || VRIsHandWeaponless(v, vh))
    {
        return FALSE;
    }
    entity oldSelf = self;
    self = v; // (the hand's getters, the buzz and the drop read self)
    haptic(vh, 0.0, 0.35, 120, 1.0);
    vr_wpn_dropped = world;
    DropWeaponInHandScaled(v, vh, VRGetEntWeapon(v, vh), VRGetEntWeaponFlags(v, vh),
        taker ? 0 : max(0, cvar_hget(cvarh_vr_weight_drop_velocity)));
    self = oldSelf;
    entity w = vr_wpn_dropped;
    VR_PD_DisarmSound(v, vh); // (the clang where it left the hand, before a taker takes it)
    if(w && taker)
    {
        setorigin(w, VRGetEntHandPos(taker, th));
        w.velocity = '0 0 0';
        w.vr_spin = '0 0 0';
        w.handtouch_ent = taker;
        w.handtouch_hand = th;
        float catching = forcegrabCatching;
        forcegrabCatching = TRUE; // (taken now, however long ago the grab began)
        wpnthrow_handtouch_ent(w);
        forcegrabCatching = catching;
    }
    else if(w && dir != '0 0 0')
    {
        w.velocity = w.velocity + normalize(dir) * speed * 0.5 + '0 0 60';
    }
    if(taker)
    {
        sprint(v, sprintf("%s takes your weapon!\n", taker.netname));
    }
    else
    {
        sprint(v, "Your weapon is knocked out of your hand!\n");
    }
    VR_PD_Log(sprintf("%s's hand %g: %s: %s", v.netname, vh, why, taker ? "taken" : "knocked out"));
    return TRUE;
};

// `pl`'s hand `h`'s weapon knocked out by a shot, a projectile or a blow (going `dir`, as hard as `speed`).
void(entity pl, float h, vector dir, float speed, string why) VR_PD_Disarm =
{
    if(VRIsHandWeaponless(pl, h))
    {
        return;
    }
    particle2(VRGetEntHandPos(pl, h), '0 0 60', QVR_PARTICLE_PRESET_SPARKS, 12);
    sound(pl, CHAN_AUTO, sprintf("weapons/ric%g.wav", 1 + min(2, floor(random() * 3))), 1, ATTN_NORM);
    stuffcmd(pl, "bf\n"); // (a flash, as a pickup's)
    if(VR_Player_Disarm(pl, h, world, 0, dir, speed, why))
    {
        vr_pd_disarms = vr_pd_disarms + 1;
        vr_pd_last_hand = h;
    }
};

// Whether what `by` does to a player goes by the PvP rule (the armed humanoids' adding up and rolls: as it was) rather than
// PvE's jolts.
float(entity by) VR_PD_PvP =
{
    return (by.flags & FL_CLIENT) != 0;
};

// PvE: `n` jolts on `pl`'s hand `h` (going `dir`; `why`): VR_PD_Frame warns or knocks it out.
void(entity pl, float h, float n, vector dir, string why) VR_PD_Jolt =
{
    if(n <= 0 || h < 0 || VRIsHandWeaponless(pl, h))
    {
        return;
    }
    entity st = VR_PD_Holder(pl, h);
    if(st.vr_pd_jolts > 0 && time - st.vr_pd_jolt_t > max(0, cvar_hget(cvarh_vr_player_disarm_window)))
    {
        st.vr_pd_jolts = 0; // (the grip recovered)
    }
    st.vr_pd_jolts = st.vr_pd_jolts + n;
    st.vr_pd_jolt_t = time;
    st.vr_pd_jnew = st.vr_pd_jnew + n;
    st.vr_pd_jdir = dir;
    st.vr_pd_jwhy = why;
    vr_pd_jolted = vr_pd_jolted + n;
    VR_PD_Log(sprintf("%s: %g jolt%s on hand %g (%g of %g)", why, n, n == 1 ? "" : "s", h, st.vr_pd_jolts,
        max(1, cvar_hget(cvarh_vr_player_disarm_jolts))));
};

// PvE: a shot's or projectile's `amount` (weighted damage) on `pl`'s hand `h`: a jolt for each vr_player_disarm_shot it
// adds up to over vr_agrunt_shotdisarm_window.
void(entity pl, float h, float amount, vector dir, vector at, string what) VR_PD_ShotAdd =
{
    if(amount <= 0)
    {
        return;
    }
    entity st = VR_PD_Holder(pl, h);
    vr_pd_hits = vr_pd_hits + 1;
    if(st.vr_pd_sum <= 0 || time - st.vr_pd_sum_t > cvar_hget(cvarh_vr_agrunt_shotdisarm_window))
    {
        st.vr_pd_sum = 0;
        st.vr_pd_sum_t = time;
    }
    float need = max(0.5, cvar_hget(cvarh_vr_player_disarm_shot));
    float before = floor(st.vr_pd_sum / need);
    st.vr_pd_sum = st.vr_pd_sum + amount;
    float n = floor(st.vr_pd_sum / need) - before;
    VR_PD_Struck(at);
    VR_PD_Log(sprintf("%s: %.1f, %.1f of %g a jolt", what, amount, st.vr_pd_sum, need));
    VR_PD_Jolt(pl, h, min(n, max(1, cvar_hget(cvarh_vr_player_disarm_jolts))), dir, what);
};

// PvE: the warning, a jolt short of the knock-out: the hand knocked aside, a buzz, a rattle at it, a line.
void(entity pl, float h, vector dir) VR_PD_Warn =
{
    entity oldSelf = self;
    self = pl; // (the hand's knock and buzz read self)
    handimpact(h, 4, normalize(dir));
    haptic(h, 0.0, 0.2, 90, 0.8);
    self = oldSelf;
    float vol = min(1, max(0, cvar_hget(cvarh_vr_player_disarm_sound)));
    if(vol > 0)
    {
        sound(pl, CHAN_AUTO, "weapons/tink1.wav", vol, ATTN_NORM);
    }
    if(pl.flags & FL_CLIENT)
    {
        sprint(pl, "Your grip on your weapon loosens!\n");
    }
    vr_pd_warns = vr_pd_warns + 1;
};

// The hand of `pl`'s holding a weapon (not a fist) nearest `from` (-1: none).
float(entity pl, vector from) VR_PD_NearHand =
{
    float best = -1;
    float bestD = 0;
    for(float h = 0; h <= 1; h++)
    {
        if(!VR_Parry_WeaponLine(pl, h))
        {
            continue;
        }
        float d = vlen(VRGetEntHandPos(pl, h) - from);
        if(best < 0 || d < bestD)
        {
            best = h;
            bestD = d;
        }
    }
    return best;
};

void(entity xFrom, vector xA, vector xB, vector xDir, float xDamage, float xKind) VR_PD_Shot =
{
    if(!VR_PD_ByOn(xFrom))
    {
        return;
    }
    // (the caller's trace and vectors as they were)
    vector fw = v_forward;
    vector rt = v_right;
    vector up = v_up;
    entity te = trace_ent;
    vector tp = trace_endpos;
    float tf = trace_fraction;
    vector tn = trace_plane_normal;
    float near = max(0, cvar_hget(cvarh_vr_agrunt_shotdisarm_near));
    for(entity pl = VR_Player(); pl; pl = VR_NextPlayer(pl))
    {
        if(pl != xFrom && VR_PD_On(pl, xFrom) && VR_AGS_Closest(xA, xB, pl.origin, pl.origin) < 128)
        {
            // Stopped on its box or body: the hands just behind that point still on its way.
            vector b = (tf < 1 && te == pl) ? xB + normalize(xDir) * 24 : xB;
            VR_PD_Meet(pl, xA, b, near);
            if(vr_pd_hand >= 0)
            {
                float w = vr_pd_part == 1 ? cvar_hget(cvarh_vr_agrunt_shotdisarm_weapon_w) :
                    cvar_hget(cvarh_vr_agrunt_shotdisarm_hand_w);
                float amt = xDamage * VR_AGS_KindWeight(xKind) * w * (vr_pd_gap <= 0 ? 1 : 0.5);
                string what = sprintf("%s on your %s (hand %g)", VR_AGS_KindName(xKind), vr_pd_part == 1 ? "weapon" : "hand",
                    vr_pd_hand);
                if(VR_PD_PvP(xFrom))
                {
                    VR_PD_Add(pl, vr_pd_hand, amt, xDir, xFrom, xA + (b - xA) * vr_pd_s, what);
                }
                else
                {
                    VR_PD_ShotAdd(pl, vr_pd_hand, amt, xDir, xA + (b - xA) * vr_pd_s, what);
                }
            }
        }
    }
    v_forward = fw;
    v_right = rt;
    v_up = up;
    trace_ent = te;
    trace_endpos = tp;
    trace_fraction = tf;
    trace_plane_normal = tn;
};

// Someone else's projectile's kind for `pl` (-1: not one, or counted already).
float(entity e, entity pl) VR_PD_ProjKind =
{
    if(e.owner == pl || e.owner == world || !e.owner.takedamage || e.velocity == '0 0 0' ||
       (e.flags & (FL_MONSTER | FL_CLIENT)) || (e.vr_ags_by.classname == "vr_pd_hand" && e.vr_ags_by.owner == pl))
    {
        return -1;
    }
    float k = VR_AGS_ProjKind(e); // (nails, rockets, grenades: a monster's or a player's)
    if(k >= 0)
    {
        return k;
    }
    if(e.touch == Laser_Touch || e.touch == ZombieGrenadeTouch)
    {
        return QVR_AGS_NAIL;
    }
    if(e.touch == OgreGrenadeTouch || e.touch == ShalMissileTouch)
    {
        return QVR_AGS_BLAST;
    }
    return -1;
};

float(entity e, float kind) VR_PD_ProjDamage =
{
    if(e.owner.flags & FL_CLIENT)
    {
        return VR_AGS_ProjDamage(e, kind);
    }
    if(e.touch == Laser_Touch) { return 15; }
    if(e.touch == superspike_touch) { return 18; }
    if(e.touch == ZombieGrenadeTouch) { return 10; }
    if(kind == QVR_AGS_NAIL) { return 9; }
    return 40; // (an ogre's grenade, a vore's ball: their blast's)
};

// The projectiles flying through `pl`'s held weapons or hands this frame (a frame back, a frame ahead).
void(entity pl) VR_PD_Projectiles =
{
    float rolls = cvar_hget(cvarh_vr_agrunt_shotdisarm_proj);
    float near = max(0, rolls ? cvar_hget(cvarh_vr_agrunt_shotdisarm_proj_near) : cvar_hget(cvarh_vr_agrunt_shotdisarm_near));
    entity e = findradius(pl.origin + '0 0 16', 112);
    while(e)
    {
        entity next = e.chain;
        float kind = VR_PD_ProjKind(e, pl);
        if(kind >= 0 && VR_PD_ByOn(e.owner)) // (a player's projectile: PvP; a monster's: PvE)
        {
            vector a = e.origin - e.velocity * frametime;
            vector b = e.origin + e.velocity * frametime;
            VR_PD_Meet(pl, a, b, near);
            if(vr_pd_hand >= 0)
            {
                entity st = VR_PD_Holder(pl, vr_pd_hand);
                e.vr_ags_by = st; // (counted once)
                float dmg = VR_PD_ProjDamage(e, kind);
                float w = vr_pd_part == 1 ? cvar_hget(cvarh_vr_agrunt_shotdisarm_weapon_w) :
                    cvar_hget(cvarh_vr_agrunt_shotdisarm_hand_w);
                string what = sprintf("%s on your %s (hand %g)", VR_AGS_KindName(kind), vr_pd_part == 1 ? "weapon" : "hand",
                    vr_pd_hand);
                if(!VR_PD_PvP(e.owner))
                {
                    // (in full within vr_agrunt_shotdisarm_proj_near: a bolt or a nail has a size of its own)
                    VR_PD_ShotAdd(pl, vr_pd_hand, dmg * VR_AGS_KindWeight(kind) * w, e.velocity, a + (b - a) * vr_pd_s, what);
                }
                else if(rolls)
                {
                    float f = vr_pd_gap <= 0 ? 1 : (near > 0 ? max(0, 1 - vr_pd_gap / near) : 0);
                    float chance = VR_Disarm_ProjChance(dmg, kind, f * w, cvar_hget(cvarh_vr_player_disarm_chance));
                    float r = random();
                    vr_pd_hits = vr_pd_hits + 1;
                    vr_pd_rolls = vr_pd_rolls + 1;
                    VR_PD_Log(sprintf("%s: %.0f damage, chance %.2f, rolled %.2f", what, dmg, chance, r));
                    if(r < chance)
                    {
                        st.vr_pd_projwon = TRUE;
                        st.vr_ags_dir = e.velocity;
                        st.vr_ags_acc = 0;
                    }
                }
                else
                {
                    VR_PD_Add(pl, vr_pd_hand, dmg * VR_AGS_KindWeight(kind) * w * (vr_pd_gap <= 0 ? 1 : 0.5), e.velocity,
                        e.owner, a + (b - a) * vr_pd_s, what);
                }
            }
        }
        e = next;
    }
};

void(entity pl) VR_PD_Frame =
{
    entity st;
    if(!VR_PD_Can(pl))
    {
        for(float c = 0; c <= 1; c++)
        {
            st = c == cVR_MainHand ? pl.vr_pd_main : pl.vr_pd_off;
            if(st)
            {
                st.vr_ags_pend = FALSE;
                st.vr_pd_projwon = FALSE;
                st.vr_pd_jnew = 0;
                st.vr_pd_jolts = 0;
            }
        }
        return;
    }
    VR_PD_Projectiles(pl);
    for(float h = 0; h <= 1; h++)
    {
        st = h == cVR_MainHand ? pl.vr_pd_main : pl.vr_pd_off;
        if(st == world)
        {
            continue;
        }
        // PvE's jolts (VR_PD_Jolt): enough, knocked out; one short, the warning.
        if(st.vr_pd_jnew > 0)
        {
            st.vr_pd_jnew = 0;
            float need = max(1, cvar_hget(cvarh_vr_player_disarm_jolts));
            if(VRIsHandWeaponless(pl, h))
            {
                st.vr_pd_jolts = 0;
            }
            else if(st.vr_pd_jolts >= need)
            {
                float jc = min(1, max(0, cvar_hget(cvarh_vr_player_disarm_chance)));
                float jr = random();
                vr_pd_rolls = vr_pd_rolls + 1;
                if(jc >= 1 || jr < jc)
                {
                    st.vr_pd_jolts = 0;
                    st.vr_pd_sum = 0;
                    VR_PD_Disarm(pl, h, st.vr_pd_jdir, 150 + 75 * need, st.vr_pd_jwhy);
                    continue;
                }
                VR_PD_Log(sprintf("hand %g: %g jolts, chance %.2f, rolled %.2f: holds on", h, st.vr_pd_jolts, jc, jr));
                st.vr_pd_jolts = need - 1;
                VR_PD_Warn(pl, h, st.vr_pd_jdir);
            }
            else
            {
                VR_PD_Warn(pl, h, st.vr_pd_jdir);
            }
        }
        if(st.vr_pd_projwon)
        {
            st.vr_pd_projwon = FALSE;
            st.vr_ags_pend = FALSE;
            VR_PD_Disarm(pl, h, st.vr_ags_dir, 300, "a projectile");
            continue;
        }
        if(!st.vr_ags_pend)
        {
            continue;
        }
        if(VRIsHandWeaponless(pl, h))
        {
            st.vr_ags_pend = FALSE;
            st.vr_ags_acc = 0;
            continue;
        }
        vr_pd_rolls = vr_pd_rolls + 1;
        float won = VR_Disarm_Roll(st, cvar_hget(cvarh_vr_player_disarm_chance));
        VR_PD_Log(sprintf("roll on hand %g: %.1f damage, chance %.2f, rolled %.2f%s", h, vr_disarm_acc, vr_disarm_chance,
            vr_disarm_r, won ? "" : ": holds on"));
        if(won)
        {
            VR_PD_Disarm(pl, h, vr_disarm_dir, 150 + min(150, vr_disarm_acc * 3), sprintf("shot, %.1f damage", vr_disarm_acc));
        }
    }
};

// A blow of `by`'s that landed on `pl` (not parried, not on crossed arms: combat.qc VR_Parry), `damage` as it lands;
// `line`: a blow with a line (the directional parry's), and `h` the hand whose weapon it met at `gap` units (-1: none).
float vr_pd_blows; // landed blows read (the tests)
void(entity pl, entity by, float h, float damage, float gap, float line) VR_PD_Blow =
{
    if(!VR_PD_On(pl, by) || damage <= 0)
    {
        return;
    }
    vr_pd_blows = vr_pd_blows + 1;
    float reach = max(0.5, cvar_hget(cvarh_vr_dirparry_contact)) + max(0, cvar_hget(cvarh_vr_agrunt_vol_blow));
    if(!VR_PD_PvP(by))
    {
        // PvE: jolts by the blow's type and strength.
        float heavy = damage >= max(1, cvar_hget(cvarh_vr_player_disarm_heavy));
        if(h >= 0 && !VRIsHandWeaponless(pl, h) && gap <= reach)
        {
            VR_PD_Jolt(pl, h, heavy ? 2 : 1, VRGetEntHandPos(pl, h) - by.origin, sprintf("%s's %sblow of %g on your weapon",
                by.classname, heavy ? "heavy " : "", damage));
            return;
        }
        float fist = !line && (by.vr_ag || by.vr_eshove_striking); // (an armed humanoid's punch, butt or shove)
        if(!fist && !heavy)
        {
            VR_PD_Log(sprintf("%s's blow of %g on your body: too light to jolt", by.classname, damage));
            return;
        }
        float nh = VR_PD_NearHand(pl, by.origin);
        if(nh < 0)
        {
            return; // (no weapon held: nothing to knock out)
        }
        VR_PD_Jolt(pl, nh, 1, VRGetEntHandPos(pl, nh) - by.origin, sprintf("%s's %s of %g on your body", by.classname,
            fist ? "fist" : "heavy blow", damage));
        return;
    }
    // PvP: as it was (a blow landing on your weapon, at least vr_agrunt_disarm_damage, rolls vr_player_disarm_chance).
    if(h < 0 || VRIsHandWeaponless(pl, h) || gap > reach || damage < cvar_hget(cvarh_vr_agrunt_disarm_damage))
    {
        return;
    }
    float chance = min(1, max(0, cvar_hget(cvarh_vr_player_disarm_chance)));
    float r = random();
    vr_pd_rolls = vr_pd_rolls + 1;
    VR_PD_Log(sprintf("%s's blow of %g on your weapon (hand %g, %.1f units): chance %.2f, rolled %.2f", by.classname, damage,
        h, gap, chance, r));
    if(r < chance)
    {
        VR_PD_Disarm(pl, h, VRGetEntHandPos(pl, h) - by.origin, 150, sprintf("a blow of %g", damage));
    }
};
