// vr_kdtrig.qc -- ragdoll triggers (vr_kdtrig; Combat > Ragdoll Triggers; ROUND22.md, "The flinch
// removed; ragdoll triggers"): predefined situations in normal combat in which a hit knocks a living humanoid down as an active ragdoll,
// besides the shove, the leg sweep, the leg pull and the lift. It goes down through the shove's knockdown
// (VR_Knockdown_Start, ragdollshovetopple), so it lies, is held and gets up as any knockdown.
//
// - The table (VR_KdTrig_Init): each situation a name, its chance's cvar (0: off), its least damage's cvar and how it
//   falls (pushed whole over an edge, or toppled where it stands). VR_KdTrig_Match says whether it holds for a hit and
//   its weight (how much likelier than its chance: the hit's strength, its pellets, how wounded). To add one: a line in
//   VR_KdTrig_Init, a case in VR_KdTrig_Match, its cvars (vr_cvars.inc, vr_cvars.qc) and menu rows.
// - In table order each situation that holds rolls its chance (times its weight; big monsters resist as Hits Unbalance
//   reckons it, vr_hit_stagger_resist); the first that wins knocks it down. The hit's way: vr_hitreact.qc's.
//   (In the table's order, the numbers here.)
//   1 stairs: a step down behind it (two or more within vr_kdtrig_look units past its hull, the first right behind
//     its feet) -> pushed whole down them.
//   2 ledge: a drop of vr_kdtrig_ledge_drop or more behind it (room for its hull to drop) -> pushed off.
//   3 slope: on ground steeper than vr_kdtrig_slope_angle, the hit pushing it down the slope -> toppled down it.
//   4 shotgun: a shotgun's or super shotgun's blast within vr_kdtrig_shotgun_range, at least half of
//     vr_kdtrig_shotgun_pellets landing (the weight: pellets over that, less with distance) -> toppled back.
//   5 wounded: under vr_kdtrig_wounded_health of its full health (the weight from half at it to one at none).
//   6 blow: a strong melee blow or a flung prop (the weight: its damage over vr_kdtrig_blow_min, up to twice; a
//     blade's times vr_kdtrig_blow_blade: fists, a gun's butt, a hammer or a mace, a pommel, the crowbar, props are
//     blunt) -> toppled back.
// - Monster groups (Combat > Monster Groups): its group's switch (vr_mgrp_<group>_kdtrig: the humanoids' on), its
//   resistance (vr_kdtrig_resist_<group>) and each trigger's share (vr_mgrp_<group>_ledge ... _blast: its chance times it).
// - Never: vr_knockdown or vr_kdtrig off, its group's triggers off, no get-up of its own (vr_kd_chance_h), one down,
//   staggering, parried, flying or swimming, a boss (VR_HST_BOSS), a dummy that stays put; for vr_kdtrig_cooldown s
//   after it gets up. Room for its ragdoll: a dead body may go for it, never a living knockdown.
// - vr_kdtrig_debug 1: a line for each that fires (`kdtrig: <name> ...`); 2: also each that held and lost its roll.

float VR_KDT_COUNT = 7;
string vr_kdt_name[7];
float vr_kdt_on_h[7];     // its switch's cvar
float vr_kdt_chance_h[7]; // its chance's cvar
float vr_kdt_min_h[7];    // its least damage's cvar (0: none)
float vr_kdt_push_h[7];   // its push's cvar
float vr_kdt_edge[7];     // TRUE: pushed whole over an edge (ragdollshovetopple's ledge); FALSE: toppled where it stands
float vr_kdt_throw[7];    // toppled: units it is carried per push (a blast's throw farther)
float vr_ad_splash;       // AD's T_Damage dealing radius damage (VR_AD_Splash; AD's own explosions)
float vr_kdt_ready;

.float vr_kdt_next;       // when a situational knockdown may take it again
.float vr_kdt_fired;      // the last that took it (its index + 1; the tests')
.float vr_kdt_fired_time; // when

// VR_KdTrig_Behind's (the ground behind it along the hit), VR_KdTrig_Match's (its way to fall, how far).
float vr_kdt_steps;   // steps down (6 units or more) found
float vr_kdt_first;   // units along to the first of them
float vr_kdt_fall;    // units from its feet down to the lowest ground found
float vr_kdt_ledge;   // a ledge's drop found
float vr_kdt_past;    // units along to past the first edge (its half width on)
vector vr_kdt_way;    // level: the way it falls
float vr_kdt_weight;  // the chance times this
float vr_kdt_resist;  // the monster's resistance (VR_KdTrig_Resist): the chance over it, the least damage times it
float vr_kdt_test_damage; // a test's (VR_KdTrig_TestHit): the hit's strength reckoned from this damage (0: the damage dealt)
float vr_kdt_test_dist;   // a test's shotgun distance (0: the attacker's)
float vr_kdt_test_weapon; // a test's weapon, plus 1 (WID_* + 1; 0: the attacker's)
float vr_kdt_test_strength; // vr_kdtrig_test_damage: a test hit's damage instead of its own (0: its own)

void(float i, string xName, float xOnH, float xChanceH, float xMinH, float xPushH, float xEdge, float xThrow) VR_KdTrig_Def =
{
    vr_kdt_name[i] = xName;
    vr_kdt_on_h[i] = xOnH;
    vr_kdt_chance_h[i] = xChanceH;
    vr_kdt_min_h[i] = xMinH;
    vr_kdt_push_h[i] = xPushH;
    vr_kdt_edge[i] = xEdge;
    vr_kdt_throw[i] = xThrow;
};

// AD's T_Damage (ad_import.py's patch round VR_AD_Hit): its radius damage, TRUE as it deals it.
void(float xSplash) VR_AD_Splash =
{
    vr_ad_splash = xSplash;
};

// The table, in the order they roll (a fall's situations first: they set how it falls).
void() VR_KdTrig_Init =
{
    VR_KdTrig_Def(0, "ledge", cvarh_vr_kdtrig_ledge_on, cvarh_vr_kdtrig_ledge, cvarh_vr_kdtrig_ledge_min,
        cvarh_vr_kdtrig_ledge_push, TRUE, 16);
    VR_KdTrig_Def(1, "stairs", cvarh_vr_kdtrig_stairs_on, cvarh_vr_kdtrig_stairs, cvarh_vr_kdtrig_stairs_min,
        cvarh_vr_kdtrig_stairs_push, TRUE, 16);
    VR_KdTrig_Def(2, "slope", cvarh_vr_kdtrig_slope_on, cvarh_vr_kdtrig_slope, cvarh_vr_kdtrig_slope_min,
        cvarh_vr_kdtrig_slope_push, FALSE, 16);
    VR_KdTrig_Def(3, "shotgun", cvarh_vr_kdtrig_shotgun_on, cvarh_vr_kdtrig_shotgun, 0, cvarh_vr_kdtrig_shotgun_push,
        FALSE, 16);
    VR_KdTrig_Def(4, "wounded", cvarh_vr_kdtrig_wounded_on, cvarh_vr_kdtrig_wounded, cvarh_vr_kdtrig_wounded_min,
        cvarh_vr_kdtrig_wounded_push, FALSE, 16);
    VR_KdTrig_Def(5, "blow", cvarh_vr_kdtrig_blow_on, cvarh_vr_kdtrig_blow, cvarh_vr_kdtrig_blow_min,
        cvarh_vr_kdtrig_blow_push, FALSE, 16);
    VR_KdTrig_Def(6, "blast", cvarh_vr_kdtrig_blast_on, cvarh_vr_kdtrig_blast, cvarh_vr_kdtrig_blast_min,
        cvarh_vr_kdtrig_blast_push, FALSE, 48);
    vr_kdt_ready = TRUE;
};

// The ground behind `e` along level `dir`, from its feet to vr_kdtrig_look units past its hull (a sample every 8 units,
// up to a wall or a step up): vr_kdt_steps, _first, _fall, _ledge, _past.
void(entity e, vector dir) VR_KdTrig_Behind =
{
    vr_kdt_steps = 0;
    vr_kdt_first = 0;
    vr_kdt_fall = 0;
    vr_kdt_ledge = FALSE;
    vr_kdt_past = 0;
    float half = max(e.maxs_x, e.maxs_y);
    float reach = half + max(0, cvar_hget(cvarh_vr_kdtrig_look));
    float height = max(24, cvar_hget(cvarh_vr_kdtrig_ledge_drop));
    vector start = e.origin + '0 0 1';
    tracebox(start, e.mins, e.maxs, start + dir * reach, TRUE, e);
    if(trace_startsolid)
    {
        return;
    }
    // (Its hull clear along the way; past a step down it would go lower, so the samples go on to `reach` whatever the
    // hull's trace found, unless a wall stands right behind it.)
    float clear = trace_fraction * reach;
    if(clear < half)
    {
        return;
    }
    float feet = e.origin_z + e.mins_z;
    float ground = feet;
    float lowest = feet;
    float d = 0;
    while(d < reach)
    {
        d = min(reach, d + 8);
        vector p = e.origin + dir * d;
        p_z = ground + 18; // (a step's height over the last ground: no step up past it)
        traceline(p, p - '0 0 1' * (18 + height), TRUE, e);
        if(trace_startsolid)
        {
            return; // (a wall or a step up)
        }
        if(trace_fraction == 1)
        {
            // An edge: room for its hull past it, as far down?
            vector q = p + dir * half;
            q_z = ground - e.mins_z + 1;
            tracebox(q, e.mins, e.maxs, q - '0 0 1' * height, TRUE, e);
            if(!trace_startsolid && trace_fraction == 1)
            {
                vr_kdt_ledge = TRUE;
                vr_kdt_past = d + half;
                vr_kdt_fall = feet - (ground - height);
            }
            return;
        }
        float step = ground - trace_endpos_z;
        if(step >= 6)
        {
            if(!vr_kdt_steps)
            {
                vr_kdt_first = d;
                vr_kdt_past = d + half;
            }
            vr_kdt_steps = vr_kdt_steps + 1;
        }
        ground = trace_endpos_z;
        lowest = min(lowest, ground);
        vr_kdt_fall = feet - lowest;
    }
};

// Whether a melee blow (its kind QVR_HITKIND_MELEE) or a flung prop (`inflictor`) by `attacker` is blunt: all but the
// blades' edges (the axes, the swords, the chainsaw; a sword's pommel and AD's hammer and mace are blunt: vr_melee_blunt).
float(entity inflictor, entity attacker) VR_KdTrig_Blunt =
{
    if(inflictor != attacker || vr_melee_blunt)
    {
        return TRUE;
    }
    float hand = vr_hithand >= 0 ? vr_hithand : cVR_MainHand;
    float w = vr_kdt_test_weapon > 0 ? vr_kdt_test_weapon - 1 : VRGetEntWeapon(attacker, hand);
    return w != WID_AXE && w != WID_SUPERAXE && w != WID_SWORD && w != WID_CHAINSAW;
};

// Whether situation `i` holds for a hit on `e` of strength `xDamage` along level `dir` (vr_kdt_way, _weight set).
float(float i, entity e, entity inflictor, entity attacker, float xDamage, vector dir) VR_KdTrig_Match =
{
    vr_kdt_way = dir;
    float least = vr_kdt_min_h[i] ? max(1, cvar_hget(vr_kdt_min_h[i]) * vr_kdt_resist) : 1;
    vr_kdt_weight = min(1.5, max(1, xDamage / least)); // (stronger hits, up to half again)
    if(vr_kdt_min_h[i] && xDamage < least)
    {
        return FALSE;
    }
    float half = max(e.maxs_x, e.maxs_y);
    if(i == 0) // ledge
    {
        return vr_kdt_ledge;
    }
    if(i == 1) // stairs
    {
        return !vr_kdt_ledge && vr_kdt_steps >= 2 && vr_kdt_fall >= 24 && vr_kdt_first <= half + 16;
    }
    if(i == 2) // slope
    {
        traceline(e.origin, e.origin + '0 0 1' * (e.mins_z - 16), TRUE, e);
        if(trace_fraction == 1 || trace_plane_normal_z >= cos(max(1, cvar_hget(cvarh_vr_kdtrig_slope_angle)) * 0.0174533))
        {
            return FALSE;
        }
        vector down = trace_plane_normal;
        down_z = 0;
        down = normalize(down);
        if(down * dir < 0.5)
        {
            return FALSE;
        }
        vr_kdt_way = normalize(dir + down);
        return TRUE;
    }
    if(i == 3) // shotgun
    {
        if(!(inflictor == attacker && (attacker.flags & FL_CLIENT) && vr_hitpellets > 0))
        {
            return FALSE;
        }
        float hand = vr_hithand >= 0 ? vr_hithand : cVR_MainHand;
        float w = vr_kdt_test_weapon > 0 ? vr_kdt_test_weapon - 1 : VRGetEntWeapon(attacker, hand);
        if(w != WID_SHOTGUN && w != WID_GRUNTGUN && w != WID_SUPER_SHOTGUN)
        {
            return FALSE;
        }
        float range = max(1, cvar_hget(cvarh_vr_kdtrig_shotgun_range));
        float dist = vr_kdt_test_dist > 0 ? vr_kdt_test_dist : vlen(vr_hr_at - (attacker.origin + attacker.view_ofs));
        float ref = max(1, cvar_hget(cvarh_vr_kdtrig_shotgun_pellets));
        if(dist > range || vr_hitpellets * 2 < ref)
        {
            return FALSE;
        }
        vr_kdt_weight = (vr_hitpellets / ref) * (1 - 0.5 * dist / range);
        return TRUE;
    }
    if(i == 4) // wounded
    {
        float full = e.max_health > 0 ? e.max_health : e.health;
        float below = max(0, cvar_hget(cvarh_vr_kdtrig_wounded_health));
        float share = full > 0 ? e.health / full : 1;
        if(share >= below)
        {
            return FALSE;
        }
        vr_kdt_weight = 0.5 + 0.5 * (1 - share / below);
        return TRUE;
    }
    if(i == 5) // blow
    {
        if(!(attacker.flags & FL_CLIENT) ||
           !(vr_hitkind == QVR_HITKIND_MELEE || (inflictor != attacker && inflictor.vr_fling_hit == time)))
        {
            return FALSE;
        }
        vr_kdt_weight = min(2, xDamage / least) *
                        (VR_KdTrig_Blunt(inflictor, attacker) ? 1 : max(0, cvar_hget(cvarh_vr_kdtrig_blow_blade)));
        return TRUE;
    }
    if(i == 6) // blast
    {
        if(vr_hitkind != QVR_HITKIND_SPLASH && !vr_ad_splash)
        {
            return FALSE;
        }
        vector centre = inflictor.solid == SOLID_BSP ? (inflictor.absmin + inflictor.absmax) * 0.5 : inflictor.origin;
        vector away = (e.absmin + e.absmax) * 0.5 - centre;
        float dist = vlen(away);
        float range = max(1, cvar_hget(cvarh_vr_kdtrig_blast_range));
        if(dist > range)
        {
            return FALSE;
        }
        away_z = 0;
        if(vlen(away) > 1)
        {
            vr_kdt_way = normalize(away);
        }
        vr_kdt_weight = min(2, xDamage / least) * (1 - 0.5 * dist / range);
        return TRUE;
    }
    return FALSE;
};

// `e`'s resistance to the ragdoll triggers (Monster Resistance): its monster group's own (vr_kdtrig_resist_<group>, 0:
// none; Combat > Monster Groups), else its hull's volume over a grunt's (32 x 32 x 64) to vr_kdtrig_resist_size (an
// ogre's, 5.5 times: 2.3).
float(entity e) VR_KdTrig_Resist =
{
    float own = monstergroupget(e, MGP_RESIST);
    if(own > 0)
    {
        return own;
    }
    float volume = max(1, e.size_x * e.size_y * e.size_z) / (32 * 32 * 64);
    return max(0.25, pow(max(1, volume), max(0, cvar_hget(cvarh_vr_kdtrig_resist_size))));
};

// Whether a situational knockdown may take `e` now at all.
float(entity e) VR_KdTrig_Can =
{
    if(!cvar_hget(cvarh_vr_kdtrig) || !cvar_hget(cvarh_vr_knockdown) || !(e.flags & FL_MONSTER) || e.health <= 0 ||
       e.vr_knockdown || e.vr_corpse || e.vr_corpse_dying || !e.vr_kd_chance_h || (e.flags & (FL_FLY | FL_SWIM)) ||
       e.solid == SOLID_BSP || e.vr_parry_stagger_until > time || time < e.vr_kdt_next ||
       (VR_IsDummy(e) && cvar_hget(cvarh_vr_dummy_anchored)))
    {
        return FALSE;
    }
    // (Its monster group's switch: the humanoids' groups on, the rest off; Combat > Monster Groups.)
    float full = e.max_health > 0 ? e.max_health : e.health;
    return full < VR_HST_BOSS && monstergroupget(e, MGP_KDTRIG) && canragdoll(e);
};

// Knocks `e` down for situation `i`, falling along vr_kdt_way. TRUE if it went down.
float(entity e, float i, float xPush) VR_KdTrig_Down =
{
    vr_kd_room_dead_only = TRUE;
    float room = VR_Knockdown_MakeRoom(e);
    vr_kd_room_dead_only = FALSE;
    if(!room || !VR_Knockdown_Start(e))
    {
        return FALSE;
    }
    float edge = vr_kdt_edge[i] && vr_kdt_past > 0;
    float reach = edge ? vr_kdt_past : 8 + vr_kdt_throw[i] * xPush;
    ragdollshovetopple(e, vr_kdt_way, xPush, edge, reach);
    e.vr_kdt_next = e.vr_kd_until + max(0, cvar_hget(cvarh_vr_kdtrig_cooldown));
    e.vr_kdt_fired = i + 1;
    e.vr_kdt_fired_time = time;
    return TRUE;
};

// A parry's stagger as an active ragdoll (vr_parry_ragdoll; ROUND22.md, "Parried enemies stagger as ragdolls"; the
// author: the parried pain pose's stagger looked unnatural). `m`, parried by `p` (VR_Parry_Interrupt, combat.qc), a
// humanoid whose monster group takes the ragdoll triggers (Combat > Monster Groups) staggers back as a shove's stagger
// (ragdollstagger) for xHold s (vr_parry_stagger: the same recovery as the pose's), then is on its feet, fighting. On
// level ground it never goes over (sure-footed: a small push, a step back); with a step down, stairs or a ledge behind
// it, or on a slope down away from you (the ragdoll triggers' looks: VR_KdTrig_Behind, VR_KdTrig_Match) it is pushed that
// way, its balance by chance (VR_PARRY_RAG_BALANCE ..), and may go down (a knockdown, getting up as any). TRUE: it
// staggers so (else the pose's stagger).
float VR_PARRY_RAG_PUSH = 0.35;      // the push (a two-hand shove's 1) on level ground,
float VR_PARRY_RAG_REACH = 12;       // and its carry (units): a step back
float VR_PARRY_RAG_SLOPE_PUSH = 0.7; // over a drop or a slope
float VR_PARRY_RAG_SLOPE_REACH = 28;
float VR_PARRY_RAG_BALANCE = 0.2;    // there its balance this .. this + VR_PARRY_RAG_SWAY (under 0.5: it gives way)
float VR_PARRY_RAG_SWAY = 0.5;
float(entity m, entity p, float xHold) VR_Parry_Ragdoll =
{
    if(!cvar_hget(cvarh_vr_parry_ragdoll) || !cvar_hget(cvarh_vr_knockdown) || !(m.flags & FL_MONSTER) ||
       m.health <= 0 || m.vr_knockdown || m.vr_corpse || !m.vr_kd_chance_h || (m.flags & (FL_FLY | FL_SWIM)) ||
       !(m.flags & FL_ONGROUND) || VR_IsDummy(m) || !monstergroupget(m, MGP_KDTRIG) || !canragdoll(m) ||
       !ragdollcanstagger(m))
    {
        return FALSE;
    }
    float full = m.max_health > 0 ? m.max_health : m.health;
    if(full >= VR_HST_BOSS)
    {
        return FALSE;
    }
    if(!vr_kdt_ready)
    {
        VR_KdTrig_Init();
    }
    vector dir = m.origin - p.origin;
    dir_z = 0;
    dir = normalize(dir);
    if(dir == '0 0 0')
    {
        makevectors(m.angles);
        dir = v_forward * -1;
    }
    // The ground behind it: a drop (a ledge, stairs) or a slope down that way.
    VR_KdTrig_Behind(m, dir);
    vr_kdt_resist = 1;
    string what = "";
    if(VR_KdTrig_Match(0, m, p, p, 1000, dir))
    {
        what = "a ledge";
    }
    else if(VR_KdTrig_Match(1, m, p, p, 1000, dir))
    {
        what = "stairs";
    }
    else if(VR_KdTrig_Match(2, m, p, p, 1000, dir))
    {
        what = "a slope";
    }
    float level = what == "";
    vector way = level ? dir : vr_kdt_way;
    vr_kd_room_dead_only = TRUE;
    float room = VR_Knockdown_MakeRoom(m);
    vr_kd_room_dead_only = FALSE;
    if(!room || !VR_Knockdown_Start(m))
    {
        return FALSE;
    }
    float push = level ? VR_PARRY_RAG_PUSH : VR_PARRY_RAG_SLOPE_PUSH;
    float balance = level ? 1 : VR_PARRY_RAG_BALANCE + random() * VR_PARRY_RAG_SWAY;
    float reach = level ? VR_PARRY_RAG_REACH : VR_PARRY_RAG_SLOPE_REACH;
    if(!ragdollstagger(m, way, push, balance, reach, level ? xHold : 0, level))
    {
        ragdollshovetopple(m, way, push, FALSE, reach); // (its ragdoll made, no stagger: down, as a shove's)
        return TRUE;
    }
    m.vr_kd_stagger = 1;
    m.vr_kd_balance = balance;
    m.vr_kd_dir = way;
    m.vr_kd_parry = 1;
    m.nextthink = time + 0.05;
    dprint(sprintf("parry ragdoll: %s staggers back as a ragdoll (%s), push %.2f, balance %.2f%s\n", m.classname,
        level ? "level ground: it stays up" : what, push, balance, level ? sprintf(", up again in %.2f s", xHold) : ""));
    return TRUE;
};

// T_DamageDeal's and AD's (vr_ad_bridge.qc), the hit's damage dealt, before its pain: TRUE if it went down.
float(entity targ, entity inflictor, entity attacker, float take) VR_KdTrig_Hit =
{
    if(take <= 0 || inflictor == world || attacker == world || vr_hitkind == QVR_HITKIND_BLEED ||
       inflictor.classname == "vr_burn" || !VR_KdTrig_Can(targ))
    {
        return FALSE;
    }
    if(!vr_kdt_ready)
    {
        VR_KdTrig_Init();
    }
    float dmg = vr_kdt_test_strength > 0 ? vr_kdt_test_strength : vr_kdt_test_damage > 0 ? vr_kdt_test_damage : take;
    VR_HitReact_Where(targ, inflictor, attacker);
    vector dir = vr_hr_dir;
    dir_z = 0;
    dir = normalize(dir);
    if(dir == '0 0 0')
    {
        dir = targ.origin - attacker.origin;
        dir_z = 0;
        dir = normalize(dir);
    }
    float loud = cvar_hget(cvarh_vr_kdtrig_debug);
    vr_kdt_resist = VR_KdTrig_Resist(targ);
    float looked = FALSE;
    float i;
    for(i = 0; i < VR_KDT_COUNT; i = i + 1)
    {
        float base = cvar_hget(vr_kdt_on_h[i]) ? max(0, cvar_hget(vr_kdt_chance_h[i])) : 0;
        base = base * max(0, monstergroupget(targ, MGP_LEDGE + i)); // (its monster group's share of this trigger)
        if(base <= 0)
        {
            continue;
        }
        if(vr_kdt_edge[i] && !looked)
        {
            VR_KdTrig_Behind(targ, dir);
            looked = TRUE;
        }
        if(!VR_KdTrig_Match(i, targ, inflictor, attacker, dmg, dir))
        {
            continue;
        }
        float chance = min(1, base * vr_kdt_weight / vr_kdt_resist);
        float roll = random();
        if(roll >= chance)
        {
            if(loud >= 2)
            {
                dprint(sprintf("kdtrig: %s on %s (%.0f damage): chance %.2f, roll %.2f: stands\n", vr_kdt_name[i],
                    targ.classname, dmg, chance, roll));
            }
            continue;
        }
        float push = min(1.25, max(0.6, 0.6 * vr_kdt_weight)) * max(0, cvar_hget(vr_kdt_push_h[i]));
        if(!VR_KdTrig_Down(targ, i, push))
        {
            if(loud)
            {
                dprint(sprintf("kdtrig: %s on %s: no ragdoll (no room)\n", vr_kdt_name[i], targ.classname));
            }
            return FALSE;
        }
        if(loud)
        {
            dprint(sprintf("kdtrig: %s on %s (%.0f damage, %g pellets): chance %.2f, roll %.2f: down", vr_kdt_name[i],
                targ.classname, dmg, vr_hitpellets, chance, roll));
            dprint(sprintf(" (resistance %.2f, steps %g, fall %.0f, past %.0f, push %.2f)\n", vr_kdt_resist, vr_kdt_steps,
                vr_kdt_fall, vr_kdt_past, push));
        }
        return TRUE;
    }
    return FALSE;
};

// ----------------------------------------------------------------------------
// Tests (vr_kdtrig_test <n>; Debug > Tests > Ragdoll Triggers), hits through T_Damage as the weapons deal them (1
// damage: never a kill; the strength reckoned from the weapon's), by the first player, on the live monster nearest him:
//   1 a shotgun blast (6 pellets), 2 a super shotgun's (14), 5 a blunt melee blow (30: a fist's), 6 a blade's (an
//   axe's), 7 a blast 48 units before it (60); vr_kdtrig_test_damage: their damage instead (0: their own);
//   vr_kdtrig_test_yaw: the hits' way (deg; 0: from the player); vr_kdtrig_test_dist: the shotguns' distance;
//   20 a grunt at vr_kdtrig_test_x, _y, _z (on the floor under it), facing the way the hits come from;
//   30 + k: vr_kdtrig_test_n trials of test k on a fresh grunt there (vr_kdtrig_test_health: its health, 0..1 a share
//   of its full health), each `kdtrig trial` line, then `kdtrig test: <fired> of <n>` and how many each situation took.
// ----------------------------------------------------------------------------

.float vr_kdt_trial_mode; // the trials' controller: the test, the trials left, its phase, its grunt, how many went down
.float vr_kdt_trial_left;
.float vr_kdt_trial_phase;
.entity vr_kdt_trial_mon;
.float vr_kdt_trial_fired;
float vr_kdt_took[7]; // the trials' (one run at a time): how many each situation took

entity(entity pl) VR_KdTrig_TestTarget =
{
    entity best = world;
    float bestD = 100000;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if((e.flags & FL_MONSTER) && e.health > 0 && !VR_IsDummy(e) && vlen(e.origin - pl.origin) < bestD)
        {
            bestD = vlen(e.origin - pl.origin);
            best = e;
        }
    }
    return best;
};

// A grunt at `at` (on the floor under it) facing the way the test hits come from.
entity(entity pl, vector at) VR_KdTrig_TestSpawn =
{
    tracebox(at + '0 0 64', '-16 -16 -24', '16 16 40', at - '0 0 512', TRUE, world); // (a grunt's hull: on stairs, the
    at = trace_endpos + '0 0 1';                                                         // highest step under it)
    entity oldSelf = self;
    entity m = spawn();
    self = m;
    setorigin(m, at);
    m.angles = '0 1 0' * vectoyaw(pl.origin - at);
    if(cvar("vr_kdtrig_test_yaw") != 0)
    {
        m.angles = '0 1 0' * (cvar("vr_kdtrig_test_yaw") + 180);
    }
    float k = cvar("vr_kdtrig_test_class"); // (0 a grunt, 3 a death knight, 4 an ogre)
    if(k == 3)
    {
        m.classname = "monster_hell_knight";
        monster_hell_knight();
    }
    else if(k == 4)
    {
        m.classname = "monster_ogre";
        monster_ogre();
    }
    else
    {
        m.classname = "monster_army";
        monster_army();
    }
    float h = cvar("vr_kdtrig_test_health");
    if(h > 1)
    {
        m.health = h;
        m.max_health = h;
    }
    else if(h > 0)
    {
        m.max_health = m.max_health > 0 ? m.max_health : m.health;
        m.health = max(1, ceil(m.max_health * h));
    }
    self = oldSelf;
    return m;
};

// Hit `best` by player `pl` with test `xMode`.
void(entity pl, entity best, float xMode) VR_KdTrig_TestHit =
{
    vector eye = pl.origin + pl.view_ofs;
    vector chest = (best.absmin + best.absmax) * 0.5 + '0 0 6';
    vector dir = chest - eye;
    dir_z = 0;
    dir = normalize(dir);
    float yaw = cvar("vr_kdtrig_test_yaw");
    if(yaw != 0)
    {
        dir = '1 0 0' * cos(yaw * 0.0174533) + '0 1 0' * sin(yaw * 0.0174533);
    }
    vr_kdt_test_dist = cvar("vr_kdtrig_test_dist");
    vr_kdt_test_strength = cvar("vr_kdtrig_test_damage");
    entity oldSelf = self;
    self = pl;
    if(xMode == 1 || xMode == 2)
    {
        vr_kdt_test_weapon = 1 + (xMode == 2 ? WID_SUPER_SHOTGUN : WID_SHOTGUN);
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, chest, dir);
        vr_hitpellets = xMode == 2 ? 14 : 6;
        vr_kdt_test_damage = vr_hitpellets * 4;
        T_Damage(best, pl, pl, best.health > vr_kdt_test_damage ? vr_kdt_test_damage : 1);
        vr_hitpellets = 0;
        vr_kdt_test_weapon = 0;
    }
    else if(xMode == 5 || xMode == 6)
    {
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, chest, dir);
        vr_kdt_test_damage = 30;
        vr_melee_blunt = xMode == 5;
        vr_kdt_test_weapon = 1 + (xMode == 5 ? WID_FIST : WID_AXE);
        T_Damage_VRMelee(best, pl, pl, best.health > vr_kdt_test_damage ? vr_kdt_test_damage : 1);
        vr_melee_blunt = FALSE;
        vr_kdt_test_weapon = 0;
    }
    else if(xMode == 7)
    {
        entity blast = spawn();
        blast.classname = "vr_kdtrig_testblast";
        setorigin(blast, best.origin - dir * 48);
        VR_SetHitKind(QVR_HITKIND_SPLASH, -1, '0 0 0', '0 0 0');
        vr_kdt_test_damage = 60;
        T_Damage(best, blast, pl, best.health > vr_kdt_test_damage ? vr_kdt_test_damage : 1);
        remove(blast);
    }
    VR_ClearHitKind();
    vr_kdt_test_damage = 0;
    vr_kdt_test_strength = 0;
    vr_kdt_test_dist = 0;
    self = oldSelf;
};

// Every monster gone (vr_knockdown_test 20's: a clean slate between trials).
void() VR_KdTrig_TestClear =
{
    entity next = world;
    for(entity mon = nextent(world); mon; mon = next)
    {
        next = nextent(mon);
        if(mon.flags & FL_MONSTER)
        {
            if(mon.vr_shove != world && mon.vr_shove.classname == "vr_shove" && mon.vr_shove.owner == mon)
            {
                remove(mon.vr_shove);
            }
            VR_Knockdown_RemoveBody(mon);
        }
    }
};

// The trials' controller: phase 0 spawn, 1 hit (its AI begun), 2 judge and clear.
void() VR_KdTrig_TrialThink =
{
    entity pl = find(world, classname, "player");
    if(!pl)
    {
        remove(self);
        return;
    }
    vector at = '1 0 0' * cvar("vr_kdtrig_test_x") + '0 1 0' * cvar("vr_kdtrig_test_y") + '0 0 1' * cvar("vr_kdtrig_test_z");
    if(self.vr_kdt_trial_phase == 0)
    {
        self.vr_kdt_trial_mon = VR_KdTrig_TestSpawn(pl, at);
        self.vr_kdt_trial_phase = 1;
        self.nextthink = time + 0.6;
        return;
    }
    entity m = self.vr_kdt_trial_mon;
    if(self.vr_kdt_trial_phase == 1)
    {
        m.vr_kdt_fired = 0;
        VR_KdTrig_TestHit(pl, m, self.vr_kdt_trial_mode);
        self.vr_kdt_trial_phase = 2;
        self.nextthink = time + 0.2;
        return;
    }
    float k = m.vr_kdt_fired;
    dprint(sprintf("kdtrig trial: %s at %v health %g: %s\n", m.classname, m.origin, m.health,
        k ? vr_kdt_name[k - 1] : "stands"));
    if(k)
    {
        self.vr_kdt_trial_fired = self.vr_kdt_trial_fired + 1;
        vr_kdt_took[k - 1] = vr_kdt_took[k - 1] + 1;
    }
    VR_KdTrig_TestClear();
    self.vr_kdt_trial_left = self.vr_kdt_trial_left - 1;
    if(self.vr_kdt_trial_left > 0)
    {
        self.vr_kdt_trial_phase = 0;
        self.nextthink = time + 0.1;
        return;
    }
    float n = max(1, cvar("vr_kdtrig_test_n"));
    dprint(sprintf("kdtrig test: %g of %g (ledge %g, stairs %g, slope %g", self.vr_kdt_trial_fired, n, vr_kdt_took[0],
        vr_kdt_took[1], vr_kdt_took[2]));
    dprint(sprintf(", shotgun %g, wounded %g, blow %g, blast %g)\n", vr_kdt_took[3], vr_kdt_took[4], vr_kdt_took[5],
        vr_kdt_took[6]));
    remove(self);
};

// StartFrame: vr_kdtrig_test.
void() VR_KdTrig_Frame =
{
    float want = cvar("vr_kdtrig_test");
    if(want < 1)
    {
        return;
    }
    cvar_set("vr_kdtrig_test", "0");
    if(!vr_kdt_ready)
    {
        VR_KdTrig_Init();
    }
    entity pl = find(world, classname, "player");
    if(!pl)
    {
        return;
    }
    vector at = '1 0 0' * cvar("vr_kdtrig_test_x") + '0 1 0' * cvar("vr_kdtrig_test_y") + '0 0 1' * cvar("vr_kdtrig_test_z");
    if(want == 20)
    {
        VR_KdTrig_TestSpawn(pl, at);
        return;
    }
    if(want > 30)
    {
        VR_KdTrig_TestClear();
        float j;
        for(j = 0; j < VR_KDT_COUNT; j = j + 1)
        {
            vr_kdt_took[j] = 0;
        }
        entity c = spawn();
        c.classname = "vr_kdtrig_trials";
        c.vr_kdt_trial_mode = want - 30;
        c.vr_kdt_trial_left = max(1, cvar("vr_kdtrig_test_n"));
        c.think = VR_KdTrig_TrialThink;
        c.nextthink = time + 0.05;
        return;
    }
    entity best = VR_KdTrig_TestTarget(pl);
    if(!best)
    {
        dprint("kdtrig test: no live monster\n");
        return;
    }
    dprint(sprintf("kdtrig test %g: %s %.0f units off\n", want, best.classname, vlen(best.origin - pl.origin)));
    VR_KdTrig_TestHit(pl, best, want);
};
