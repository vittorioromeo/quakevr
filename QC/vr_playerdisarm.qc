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

.entity vr_pd_main; // the main hand's holder of the adding up (spawned at its first hit)
.entity vr_pd_off;  // ... the off hand's
.float vr_pd_projwon; // a holder: a projectile's own roll won (VR_PD_Frame knocks it out)

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

// Whether `pl` can be disarmed now.
float(entity pl) VR_PD_On =
{
    return cvar_hget(cvarh_vr_player_disarm) && (pl.flags & FL_CLIENT) && pl.health > 0 &&
        cvar_hget(cvarh_vr_player_disarm_chance) > 0;
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
    if(vr_pd_part == 1)
    {
        particle2(at, '0 0 20', QVR_PARTICLE_PRESET_SPARKS, 2); // (metal struck)
    }
    VR_PD_Log(sprintf("%s: %.1f, %.1f of %g", what, amount, st.vr_ags_acc,
        max(0.01, cvar_hget(cvarh_vr_agrunt_shotdisarm_damage))));
};

// `pl`'s hand `h`'s weapon knocked out (going `dir`, as hard as `speed`): it drops as the grip lost drops it.
void(entity pl, float h, vector dir, float speed, string why) VR_PD_Disarm =
{
    if(VRIsHandWeaponless(pl, h))
    {
        return;
    }
    entity oldSelf = self;
    self = pl; // (the hand's getters and the drop read self)
    vector at = VRGetEntHandPos(pl, h);
    particle2(at, '0 0 60', QVR_PARTICLE_PRESET_SPARKS, 12);
    sound(pl, CHAN_AUTO, sprintf("weapons/ric%g.wav", 1 + min(2, floor(random() * 3))), 1, ATTN_NORM);
    haptic(h, 0.0, 0.35, 120, 1.0);
    stuffcmd(pl, "bf\n"); // (a flash, as a pickup's)
    vr_wpn_dropped = world;
    DropWeaponInHandScaled(pl, h, VRGetEntWeapon(pl, h), VRGetEntWeaponFlags(pl, h),
        max(0, cvar_hget(cvarh_vr_weight_drop_velocity)));
    if(vr_wpn_dropped != world && dir != '0 0 0')
    {
        vr_wpn_dropped.velocity = vr_wpn_dropped.velocity + normalize(dir) * speed * 0.5 + '0 0 60';
    }
    sprint(pl, "Your weapon is knocked out of your hand!\n");
    vr_pd_disarms = vr_pd_disarms + 1;
    vr_pd_last_hand = h;
    VR_PD_Log(sprintf("hand %g: %s: knocked out", h, why));
    self = oldSelf;
};

void(entity xFrom, vector xA, vector xB, vector xDir, float xDamage, float xKind) VR_PD_Shot =
{
    if(!cvar_hget(cvarh_vr_player_disarm))
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
        if(pl != xFrom && VR_PD_On(pl) && VR_AGS_Closest(xA, xB, pl.origin, pl.origin) < 128)
        {
            // Stopped on its box or body: the hands just behind that point still on its way.
            vector b = (tf < 1 && te == pl) ? xB + normalize(xDir) * 24 : xB;
            VR_PD_Meet(pl, xA, b, near);
            if(vr_pd_hand >= 0)
            {
                float w = vr_pd_part == 1 ? cvar_hget(cvarh_vr_agrunt_shotdisarm_weapon_w) :
                    cvar_hget(cvarh_vr_agrunt_shotdisarm_hand_w);
                VR_PD_Add(pl, vr_pd_hand, xDamage * VR_AGS_KindWeight(xKind) * w * (vr_pd_gap <= 0 ? 1 : 0.5), xDir, xFrom,
                    xA + (b - xA) * vr_pd_s, sprintf("%s on your %s (hand %g)", VR_AGS_KindName(xKind),
                    vr_pd_part == 1 ? "weapon" : "hand", vr_pd_hand));
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
        if(kind >= 0)
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
                if(rolls)
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
    if(!VR_PD_On(pl))
    {
        if(pl.vr_pd_main) { pl.vr_pd_main.vr_ags_pend = FALSE; pl.vr_pd_main.vr_pd_projwon = FALSE; }
        if(pl.vr_pd_off) { pl.vr_pd_off.vr_ags_pend = FALSE; pl.vr_pd_off.vr_pd_projwon = FALSE; }
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

void(entity pl, entity by, float h, float damage, float gap) VR_PD_Blow =
{
    if(!VR_PD_On(pl) || h < 0 || VRIsHandWeaponless(pl, h))
    {
        return;
    }
    float reach = max(0.5, cvar_hget(cvarh_vr_dirparry_contact)) + max(0, cvar_hget(cvarh_vr_agrunt_vol_blow));
    if(gap > reach || damage < cvar_hget(cvarh_vr_agrunt_disarm_damage))
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
