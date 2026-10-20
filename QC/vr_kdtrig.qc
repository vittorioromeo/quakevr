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
//   8 thrown, 9 tackle: no hit; a thrown body striking it (the engine's callBodyHits -> VR_KdTrig_BodyHit), you running
//     into it with a shoulder leading (the shoulder charge: VR_KdTrig_Tackle) -> by weight pushed back and toppled the
//     way it was struck, a stagger back or nothing (VR_KdTrig_Bump; "Thrown bodies and tackles" below).
// - Monster groups (Combat > Monster Groups): its group's switch (vr_mgrp_<group>_kdtrig: the humanoids' on), its
//   resistance (vr_kdtrig_resist_<group>) and each trigger's share (vr_mgrp_<group>_ledge ... _blast, _thrown, _tackle:
//   its chance times it).
// - Never: vr_knockdown or vr_kdtrig off, its group's triggers off, no get-up of its own (vr_kd_chance_h), one down,
//   staggering, parried, flying or swimming, a boss (VR_HST_BOSS), a dummy that stays put; for vr_kdtrig_cooldown s
//   after it gets up. Room for its ragdoll: a dead body may go for it, never a living knockdown.
// - vr_kdtrig_debug 1: a line for each that fires (`kdtrig: <name> ...`); 2: also each that held and lost its roll.

float VR_KDT_COUNT = 7;  // the hits' (VR_KdTrig_Hit's loop); the thrown body and the tackle after them, their own entries
float VR_KDT_THROWN = 7;
float VR_KDT_TACKLE = 8;
string vr_kdt_name[9];  // (the hits' seven, then the thrown body and the tackle: VR_KDT_THROWN, VR_KDT_TACKLE)
float vr_kdt_on_h[9];     // its switch's cvar
float vr_kdt_chance_h[9]; // its chance's cvar
float vr_kdt_min_h[9];    // its least damage's cvar (0: none)
float vr_kdt_push_h[9];   // its push's cvar
float vr_kdt_edge[9];     // TRUE: pushed whole over an edge (ragdollshovetopple's ledge); FALSE: toppled where it stands
float vr_kdt_throw[9];    // toppled: units it is carried per push (a blast's throw farther)
float vr_kdt_reach_set;   // (VR_KdTrig_Down: units it is carried instead, when more than 0: a shoulder charge's knock)
float vr_kdt_launch;      // (VR_KdTrig_Down: its ragdoll launched along vr_kdt_way this fast, u/s: a thrown body's, a charge's)
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
    VR_KdTrig_Def(VR_KDT_THROWN, "thrown", cvarh_vr_kdtrig_thrown_on, cvarh_vr_kdtrig_thrown, 0, cvarh_vr_kdtrig_thrown_push,
        FALSE, 32);
    VR_KdTrig_Def(VR_KDT_TACKLE, "tackle", cvarh_vr_kdtrig_tackle_on, cvarh_vr_kdtrig_tackle, 0, cvarh_vr_kdtrig_tackle_push,
        FALSE, 32);
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
    if(!room)
    {
        return FALSE;
    }
    // A thrown body's or a charge's: its ragdoll launched along the way (the shove's topple carries it its reach only
    // with a launch that way: without one it falls over where it stood).
    vector wasVel = e.velocity;
    if(vr_kdt_launch > 0)
    {
        e.velocity = vr_kdt_way * vr_kdt_launch + '0 0 1' * max(e.velocity_z, 60);
    }
    if(!VR_Knockdown_Start(e))
    {
        e.velocity = wasVel;
        return FALSE;
    }
    float edge = vr_kdt_edge[i] && vr_kdt_past > 0;
    float reach = edge ? vr_kdt_past : vr_kdt_reach_set > 0 ? vr_kdt_reach_set : 8 + vr_kdt_throw[i] * xPush;
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
vector vr_parry_way; // (the ragdoll triggers' short stagger, VR_KdTrig_Stagger: its way instead of from `p`; level, unit)
float vr_parry_rag_push_set;  // (and its push and reach when more than the parry's: VR_KdTrig_Bump's, by the strike)
float vr_parry_rag_reach_set;
float(entity m, entity p, float xHold) VR_Parry_Ragdoll =
{
    if(!cvar_hget(cvarh_vr_parry_ragdoll) || !cvar_hget(cvarh_vr_knockdown) || !(m.flags & FL_MONSTER) ||
       m.health <= 0 || m.vr_knockdown || m.vr_corpse || !m.vr_kd_chance_h || (m.flags & (FL_FLY | FL_SWIM)) ||
       VR_IsDummy(m) || !monstergroupget(m, MGP_KDTRIG) || !canragdoll(m) || !ragdollcanstagger(m))
    {
        return FALSE;
    }
    // On its feet: the parry's own push (VR_Push, vr_parry_push_enemy) has just lifted it off the ground (FL_ONGROUND
    // cleared, an upward speed) before this is asked, so the ground is looked for under its feet instead (the author's
    // games never staggered as ragdolls: the headless check had the push off). Its push is the stagger's from here.
    if(!(m.flags & FL_ONGROUND))
    {
        tracebox(m.origin, m.mins, m.maxs, m.origin - '0 0 6', FALSE, m);
        if(trace_fraction >= 1 || trace_startsolid || trace_plane_normal_z < 0.7)
        {
            return FALSE;
        }
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
    if(vr_parry_way != '0 0 0')
    {
        dir = vr_parry_way;
    }
    dir_z = 0;
    dir = normalize(dir);
    if(dir == '0 0 0')
    {
        makevectors(m.angles);
        dir = v_forward * -1;
    }
    // The ground behind it: a drop (a ledge, stairs) or a slope down that way. (Parry balance: only a stagger over 2 may
    // send it down them, vr_parry_rag_nofall; a lesser one is as on level ground.)
    vr_kdt_resist = 1;
    string what = "";
    if(!vr_parry_rag_nofall)
    {
        VR_KdTrig_Behind(m, dir);
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
    }
    float level = what == "";
    vector way = level ? dir : vr_kdt_way;
    vr_kd_room_dead_only = TRUE;
    float room = VR_Knockdown_MakeRoom(m);
    vr_kd_room_dead_only = FALSE;
    if(!room)
    {
        return FALSE;
    }
    // (The parry's push and lift undone: the stagger carries it, from its feet.)
    vector wasVel = m.velocity;
    float wasFlags = m.flags;
    m.velocity = '0 0 0';
    m.flags = m.flags | FL_ONGROUND;
    if(!VR_Knockdown_Start(m))
    {
        m.velocity = wasVel;
        m.flags = wasFlags;
        return FALSE;
    }
    float push = level ? VR_PARRY_RAG_PUSH : VR_PARRY_RAG_SLOPE_PUSH;
    float balance = level ? 1 : VR_PARRY_RAG_BALANCE + random() * VR_PARRY_RAG_SWAY;
    float reach = level ? VR_PARRY_RAG_REACH : VR_PARRY_RAG_SLOPE_REACH;
    if(vr_parry_rag_push_set > 0) // (a thrown body's or a charge's stagger: harder and farther, VR_KdTrig_Bump)
    {
        push = max(push, vr_parry_rag_push_set);
        reach = max(reach, vr_parry_rag_reach_set);
    }
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
    // A melee hit (a blow, a headbutt, a bash, a shove, a flung prop) on a heavy group (Parry Poise over 1): every
    // trigger's chance but the shotgun's and the blast's times its poise factor (combat.qc VR_Poise_Factor): weapons and
    // explosives stay the answer to it.
    float poiseF = 1;
    float flung = inflictor != attacker && inflictor.vr_fling_hit == time;
    if(vr_hitkind == QVR_HITKIND_MELEE || vr_hitkind == QVR_HITKIND_HEADBUTT || vr_hitkind == QVR_HITKIND_BASH ||
       vr_hitkind == QVR_HITKIND_SHOVE || flung)
    {
        poiseF = VR_Poise_Factor(targ);
    }
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
        float chance = min(1, base * vr_kdt_weight / vr_kdt_resist * ((i == 3 || i == 6) ? 1 : poiseF));
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
// Thrown bodies and tackles (ROUND22.md, "Thrown bodies and tackles"): no damage dealt, so not VR_KdTrig_Hit's loop;
// their own entries, the table's last two (on, chance, push; its monster group's share at MGP_LEDGE + its index). By
// weight: the monster's (vr_kdtrig_weight times its resistance) against the thrower's (the thrown body's mass, the
// player's vr_kdtrig_tackle_weight): up to vr_kdtrig_weight_topple times it toppled the way it was struck, up to
// vr_kdtrig_weight_stagger times only a short stagger (VR_KdTrig_Stagger), heavier nothing; shamblers, fiends and the
// bosses never (their groups take no triggers: VR_KdTrig_Can).

.float vr_kdt_said;        // a monster's: its last `kdtrig:` line for a strike that did nothing (one each half second)
.float vr_kdt_bump;        // a monster's: the last thrown body's or charge's outcome on it (2 down, 1 staggered; the tests')
float vr_kdt_tweight;      // VR_KdTrig_Class's: the monster's weight (kg)

// `e`'s weight class against a thrower of `xMass` kg: 2 it can topple, 1 it only staggers, 0 it stands.
float(entity e, float xMass) VR_KdTrig_Class =
{
    vr_kdt_tweight = max(1, cvar_hget(cvarh_vr_kdtrig_weight)) * VR_KdTrig_Resist(e);
    float ratio = vr_kdt_tweight / max(1, xMass);
    if(ratio <= max(0, cvar_hget(cvarh_vr_kdtrig_weight_topple)))
    {
        return 2;
    }
    if(ratio <= max(0, cvar_hget(cvarh_vr_kdtrig_weight_stagger)))
    {
        return 1;
    }
    return 0;
};

// A heavier one's short stagger by `by` along level `dir` (vr_kdtrig_bump_stagger s: a parry's, as an active ragdoll where
// it can, else its pain pose's: combat.qc VR_Parry_InterruptFor).
void(entity m, entity by, vector dir) VR_KdTrig_Stagger =
{
    float hold = min(3, max(0.1, cvar_hget(cvarh_vr_kdtrig_bump_stagger)));
    vr_parry_way = dir;
    VR_Parry_InterruptFor(m, by, hold);
    vr_parry_way = '0 0 0';
    m.vr_kdt_next = time + hold + 0.5;
};

// A strike of trigger `i` on `m` that did nothing, said (vr_kdtrig_debug; once each half second a monster: a thrown body
// in it is met every frame).
void(float i, entity m, string xWhy) VR_KdTrig_Stands =
{
    if(cvar_hget(cvarh_vr_kdtrig_debug) && time >= m.vr_kdt_said)
    {
        m.vr_kdt_said = time + 0.5;
        dprint(sprintf("kdtrig: %s on %s: stands (%s)\n", vr_kdt_name[i], m.classname, xWhy));
    }
};

// Why `m` can't be taken by a ragdoll trigger now (VR_KdTrig_Can false).
string(entity m) VR_KdTrig_WhyNot =
{
    if(!monstergroupget(m, MGP_KDTRIG))
    {
        return "its monster group takes no triggers";
    }
    float full = m.max_health > 0 ? m.max_health : m.health;
    if(full >= VR_HST_BOSS)
    {
        return "a boss";
    }
    return "down, staggering or cooling down";
};

// Trigger `i` (the thrown body's, the tackle's) on `m` by a thrower of `xMass` kg along level `dir`, struck `xStrike`
// strong (1: enough to topple one of its own weight), by `by`: toppled, staggered or nothing. 2 down, 1 staggered, 0
// nothing.
float(float i, entity m, entity by, float xMass, vector dir, float xStrike, string xHow) VR_KdTrig_Bump =
{
    if(!vr_kdt_ready)
    {
        VR_KdTrig_Init();
    }
    if(!cvar_hget(vr_kdt_on_h[i]))
    {
        return 0;
    }
    if(!VR_KdTrig_Can(m))
    {
        VR_KdTrig_Stands(i, m, VR_KdTrig_WhyNot(m));
        return 0;
    }
    float base = max(0, cvar_hget(vr_kdt_chance_h[i])) * max(0, monstergroupget(m, MGP_LEDGE + i));
    if(base <= 0)
    {
        VR_KdTrig_Stands(i, m, "its group's share 0");
        return 0;
    }
    float cls = VR_KdTrig_Class(m, xMass);
    float loud = cvar_hget(cvarh_vr_kdtrig_debug);
    string about = sprintf("%s, %.0f kg against its %.0f kg, strike %.2f", xHow, xMass, vr_kdt_tweight, xStrike);
    if(cls == 0)
    {
        VR_KdTrig_Stands(i, m, sprintf("too heavy: %s", about));
        return 0;
    }
    if(xStrike < max(0, cvar_hget(cvarh_vr_kdtrig_thrown_least)) && i == VR_KDT_THROWN)
    {
        VR_KdTrig_Stands(i, m, sprintf("too weak: %s", about));
        return 0;
    }
    // A toppleable class: its chance in full from a strike of 1, less below it down to none at the least (a strike near
    // a toppling one topples often, not never: the author's throws toppled too rarely). Pushed with the strike: a harder
    // one carries it farther (VR_KdTrig_Down: 8 + its throw's units times the push).
    // A shoulder charge knocks it back by weight instead: vr_kdtrig_tackle_knock units one of his weight (up to 1.5 times
    // that a lighter one), toppled at the end; staggered back 0.6 of it.
    float least = i == VR_KDT_THROWN ? max(0, cvar_hget(cvarh_vr_kdtrig_thrown_least)) : 0;
    float pushK = max(0, cvar_hget(vr_kdt_push_h[i]));
    float knock = 0;
    if(i == VR_KDT_TACKLE)
    {
        knock = max(0, cvar_hget(cvarh_vr_kdtrig_tackle_knock)) * min(1.5, xMass / vr_kdt_tweight) * pushK;
    }
    if(cls == 2)
    {
        float chance = min(1, base) * min(1, max(0, (xStrike - least) / max(0.01, 1 - least)));
        float roll = random();
        if(roll < chance)
        {
            float push = min(1.5, max(0.6, 0.6 * xStrike)) * pushK;
            vr_kdt_way = dir;
            vr_kdt_past = 0;
            vr_kdt_reach_set = knock;
            vr_kdt_launch = min(450, 150 + 100 * xStrike * max(0.5, pushK)); // (the push-back: with the strike)
            float down = VR_KdTrig_Down(m, i, push);
            if(down && loud)
            {
                dprint(sprintf("kdtrig: %s on %s: down (%s; push %.2f, carried %.0f units, launched %.0f u/s, yaw %.0f)\n",
                    vr_kdt_name[i], m.classname, about, push, knock > 0 ? knock : 8 + vr_kdt_throw[i] * push,
                    vr_kdt_launch, vectoyaw(dir)));
            }
            vr_kdt_reach_set = 0;
            vr_kdt_launch = 0;
            if(down)
            {
                m.vr_kdt_bump = 2;
                return 2;
            }
            if(loud)
            {
                dprint(sprintf("kdtrig: %s on %s: no ragdoll (no room)\n", vr_kdt_name[i], m.classname));
            }
        }
        else if(loud >= 2)
        {
            dprint(sprintf("kdtrig: %s on %s: chance %.2f, roll %.2f: staggers instead\n", vr_kdt_name[i], m.classname,
                chance, roll));
        }
    }
    // Staggered back with the strike: farther and harder the harder it was struck and the lighter it is (its push over
    // the parry's own small step back), on its feet.
    float light = cls == 2 ? 1 : 0.6;
    vr_parry_rag_push_set = min(1, (0.25 + 0.3 * xStrike) * light * max(0.5, pushK));
    vr_parry_rag_reach_set = knock > 0 ? min(128, 12 + 0.6 * knock) : min(96, 12 + 20 * xStrike * light * pushK);
    float back = vr_parry_rag_reach_set;
    VR_KdTrig_Stagger(m, by, dir);
    vr_parry_rag_push_set = 0;
    vr_parry_rag_reach_set = 0;
    if(loud)
    {
        dprint(sprintf("kdtrig: %s on %s: staggers (%s%s; back %.0f units)\n", vr_kdt_name[i], m.classname, about,
            cls == 1 ? ": a heavier class" : "", back));
    }
    m.vr_kdt_bump = 1;
    return 1;
};

// The engine's (vr_box3d_step.cpp callBodyHits): a thrown ragdoll `body` (a corpse or a knocked-down one; its mass xMass
// kg, its middle going at xVel units/s) meeting living monster `m`'s box. Its strike: its momentum over that of a body of
// m's weight at vr_kdtrig_thrown_speed.
void(entity m, entity body, float xMass, vector xVel) VR_KdTrig_BodyHit =
{
    if(m == body || m.health <= 0 || m.vr_knockdown || !(m.flags & FL_MONSTER))
    {
        return;
    }
    if(!vr_kdt_ready)
    {
        VR_KdTrig_Init();
    }
    vector dir = xVel;
    dir_z = 0;
    dir = normalize(dir);
    if(dir == '0 0 0')
    {
        dir = m.origin - body.origin;
        dir_z = 0;
        dir = normalize(dir);
    }
    float speed = vlen(xVel);
    VR_KdTrig_Class(m, xMass); // (its weight)
    float strike = xMass * speed / (vr_kdt_tweight * max(1, cvar_hget(cvarh_vr_kdtrig_thrown_speed)));
    VR_KdTrig_Bump(VR_KDT_THROWN, m, body, xMass, dir, strike, sprintf("a %s at %.0f u/s", body.classname, speed));
};

// The shoulder charge (the tackle's entry; ROUND22.md, "Shoulder charge"): running at full speed into a monster with a
// shoulder leading, side-on, both hands low and close to the body (the leading arm tucked, not reaching out). In the
// player's level frame along the run (his head looks the way he runs: vr_movement_mode 1 runs where the head looks, so
// the head can't show the torso's turn; the hands can): the hands' middle, from the head, lies round to the side of
// the run by vr_kdtrig_tackle_turn degrees or more (the torso turned side-on: the hands in front of it are beside the
// run, the leading shoulder the other side); both vr_kdtrig_tackle_low units or more below the head; each within
// vr_kdtrig_tackle_reach of the head's line (level), none reaching ahead past VR_KDT_CHARGE_AHEAD of it; no shove or
// bash going, no gun aimed with both hands; held vr_kdtrig_tackle_hold s. A running guard (hands before the chest), a
// running shove, a gun carried or aimed, walking into one (too slow), a sidestep (not running the way he looks) don't.
float VR_KDT_CHARGE_AHEAD = 0.6; // a hand reaching ahead past this share of the reach: not tucked (a shove's, a guard's)
.float vr_kdt_tackle_next;  // a player's: when he may charge again
.float vr_kdt_charge_since; // a player's: since when he holds the charge's pose (0: not)
.float vr_kdt_charge_said;  // (vr_kdtrig_debug 2: the pose's why-not, each half second while running fast)
float vr_kdt_charge_side;   // VR_KdTrig_ChargePose's: the leading shoulder (1 the left, -1 the right),
vector vr_kdt_charge_m;     // its measures (x the hands' middle's turn from the run, deg; y the higher hand's drop below
                            // the head, units; z the farther hand's level distance from the head's line, units),
string vr_kdt_charge_why;   // and why it doesn't hold ("" it does)

// Whether `self` (a player) running along level unit `run` is in the shoulder charge's pose (above).
float(vector run) VR_KdTrig_ChargePose =
{
    vr_kdt_charge_why = "";
    vr_kdt_charge_m = '0 0 0';
    if(!VRHandsTracked(self))
    {
        vr_kdt_charge_why = "hands not tracked";
        return FALSE;
    }
    if(VRGet2HAiming(self))
    {
        vr_kdt_charge_why = "a gun aimed with both hands";
        return FALSE;
    }
    if(VR_Blow(cVR_MainHand).mh_pushing || VR_Blow(cVR_OffHand).mh_pushing)
    {
        vr_kdt_charge_why = "a shove or a bash going";
        return FALSE;
    }
    vector right;
    right_x = run_y;
    right_y = -run_x;
    right_z = 0;
    vector head = VRGetHeadPos(self);
    vector m = VRGetEntHandPos(self, cVR_MainHand) - head;
    vector o = VRGetEntHandPos(self, cVR_OffHand) - head;
    vector mid = (m + o) * 0.5;
    float ahead = mid * run;
    float side = mid * right;
    float yaw = vectoyaw('1 0 0' * ahead + '0 1 0' * side);
    float turn = yaw > 180 ? 360 - yaw : yaw;
    float drop = min(-m_z, -o_z);
    vector ml = m;
    ml_z = 0;
    vector ol = o;
    ol_z = 0;
    float far = max(vlen(ml), vlen(ol));
    float out = max(m * run, o * run);
    vr_kdt_charge_m = '1 0 0' * turn + '0 1 0' * drop + '0 0 1' * far;
    vr_kdt_charge_side = side >= 0 ? 1 : -1; // (the hands to the right of the run: the left shoulder leads)
    float reach = max(1, cvar_hget(cvarh_vr_kdtrig_tackle_reach));
    if(drop < cvar_hget(cvarh_vr_kdtrig_tackle_low))
    {
        vr_kdt_charge_why = sprintf("a hand high (%.0f units below the head)", drop);
        return FALSE;
    }
    if(far > reach)
    {
        vr_kdt_charge_why = sprintf("a hand out from the body (%.0f units)", far);
        return FALSE;
    }
    if(out > reach * VR_KDT_CHARGE_AHEAD)
    {
        vr_kdt_charge_why = sprintf("a hand reaching ahead (%.0f units)", out);
        return FALSE;
    }
    if(fabs(side) < 2 || turn < cvar_hget(cvarh_vr_kdtrig_tackle_turn))
    {
        vr_kdt_charge_why = sprintf("not side-on (the hands %.0f deg round from the run)", turn);
        return FALSE;
    }
    return TRUE;
};

// VR_Motion_Sample's (a take recorded or played; self the player): the charge's pose along the run (the way he looks
// when still), into the take's qc column:
//  charge     x the hands' middle's turn from the run (deg), y the higher hand's drop below the head, z the farther hand's
//             level distance from the head's line (units)
//  charge_ok  x the pose holds (1 the left shoulder leading, -1 the right, 0 not), y the ground speed (u/s), z how long
//             it has been held running (s)
void() VR_KdTrig_ChargeSample =
{
    vector run = self.velocity;
    run_z = 0;
    float speed = vlen(run);
    if(speed < 1)
    {
        run = VRForwardOf('0 1 0' * self.v_viewangle_y);
        run_z = 0;
    }
    float ok = VR_KdTrig_ChargePose(normalize(run));
    motionvalue("charge", vr_kdt_charge_m);
    motionvalue("charge_ok", '1 0 0' * (ok * vr_kdt_charge_side) + '0 1 0' * speed +
        '0 0 1' * (self.vr_kdt_charge_since ? time - self.vr_kdt_charge_since : 0));
};

// PlayerVRMelee's, after the bash (client.qc; self the player): on the ground, running at vr_kdtrig_tackle_speed of
// sv_maxspeed or faster the way he looks, in the charge's pose held vr_kdtrig_tackle_hold s, his box about to meet a
// living monster's: a shoulder charge (a "shouldercharge" event for the motion takes). It knocks the monster back by
// weight (VR_KdTrig_Bump: his vr_kdtrig_tackle_weight; vr_kdtrig_tackle_knock units for one of his weight, more for a
// lighter one, then toppled; a heavier one staggers back less; the heaviest stand) and he bounces back
// (vr_kdtrig_tackle_bump). Cooldown vr_kdtrig_tackle_cooldown. The training dummy (the takes' target) is only judged,
// with the Tackles off too: the event and the bounce, it stands.
void() VR_KdTrig_Tackle =
{
    if(self.health <= 0 || time < self.vr_kdt_tackle_next || !(self.flags & FL_ONGROUND))
    {
        self.vr_kdt_charge_since = 0;
        return;
    }
    vector run = self.velocity;
    run_z = 0;
    float speed = vlen(run);
    float least = max(1, cvar("sv_maxspeed")) * max(0, cvar_hget(cvarh_vr_kdtrig_tackle_speed));
    float loud = cvar_hget(cvarh_vr_kdtrig_debug);
    vector fwd = VRForwardOf('0 1 0' * self.v_viewangle_y);
    fwd_z = 0;
    fwd = normalize(fwd);
    // (Standing or walking: the pose along the way he looks, so that one taken before the run-up counts as held.)
    run = speed < least * 0.5 ? fwd : run * (1 / speed);
    float pose = VR_KdTrig_ChargePose(run);
    if(speed < least * 0.5)
    {
        if(!pose)
        {
            self.vr_kdt_charge_since = 0;
        }
        else if(!self.vr_kdt_charge_since)
        {
            self.vr_kdt_charge_since = time;
        }
        return;
    }
    if(pose && fwd * run < 0.5)
    {
        pose = FALSE;
        vr_kdt_charge_why = "not running the way you look (a sidestep)";
    }
    if(!pose)
    {
        self.vr_kdt_charge_since = 0;
        if(loud >= 2 && speed >= least && time >= self.vr_kdt_charge_said)
        {
            self.vr_kdt_charge_said = time + 0.5;
            dprint(sprintf("kdtrig: charge pose: no (%s; turn %.0f deg, drop %.0f, reach %.0f; %.0f u/s)\n",
                vr_kdt_charge_why, vr_kdt_charge_m_x, vr_kdt_charge_m_y, vr_kdt_charge_m_z, speed));
        }
        return;
    }
    if(!self.vr_kdt_charge_since)
    {
        self.vr_kdt_charge_since = time;
    }
    // About to meet it: his box carried this frame's run and a bit on (blocked by it, his speed is gone next frame).
    float reach = 8 + speed * max(frametime, 0.01) * 2;
    vector lo = self.absmin + run * reach;
    vector hi = self.absmax + run * reach;
    entity best = world;
    float bestD = 100000;
    for(entity e = findradius(self.origin + run * reach, reach + 96); e; e = e.chain)
    {
        if(e == self || !(e.flags & FL_MONSTER) || e.health <= 0 || e.solid == SOLID_NOT || e.vr_knockdown ||
           (e.origin - self.origin) * run <= 0)
        {
            continue;
        }
        if(lo_x > e.absmax_x || hi_x < e.absmin_x || lo_y > e.absmax_y || hi_y < e.absmin_y || lo_z > e.absmax_z ||
           hi_z < e.absmin_z)
        {
            continue;
        }
        float d = vlen(e.origin - self.origin);
        if(d < bestD)
        {
            bestD = d;
            best = e;
        }
    }
    if(!best)
    {
        return;
    }
    float dummy = VR_IsDummy(best);
    if(!dummy && (!cvar_hget(cvarh_vr_kdtrig_tackle_on) || !cvar_hget(cvarh_vr_kdtrig)))
    {
        return;
    }
    self.vr_kdt_tackle_next = time + 0.3; // (a strike that did nothing: not every frame)
    float held = time - self.vr_kdt_charge_since;
    if(speed < least || held < max(0, cvar_hget(cvarh_vr_kdtrig_tackle_hold)))
    {
        if(loud)
        {
            dprint(sprintf("kdtrig: charge on %s: none (%.0f u/s, the pose held %.2f s: %s)\n", best.classname, speed, held,
                speed < least ? "too slow" : "not long enough"));
        }
        return;
    }
    string side = vr_kdt_charge_side > 0 ? "left" : "right";
    string how = sprintf("running %.0f u/s, %s shoulder, turn %.0f deg, hands %.0f below the head, %.0f out", speed, side,
        vr_kdt_charge_m_x, vr_kdt_charge_m_y, vr_kdt_charge_m_z);
    VR_Motion_Event("shouldercharge", side, -1, speed, (best.absmin + best.absmax) * 0.5, best, how);
    self.vr_kdt_tackle_next = time + max(0, cvar_hget(cvarh_vr_kdtrig_tackle_cooldown));
    self.vr_kdt_charge_since = 0;
    // He bounces back off it (the hit's feel), whatever it does.
    self.velocity = run * (-max(0, cvar_hget(cvarh_vr_kdtrig_tackle_bump))) + '0 0 1' * self.velocity_z;
    if(dummy)
    {
        dprint(sprintf("kdtrig: shoulder charge on the training dummy (it stands): %s\n", how));
        return;
    }
    float weight = max(1, cvar_hget(cvarh_vr_kdtrig_tackle_weight));
    VR_KdTrig_Bump(VR_KDT_TACKLE, best, self, weight, run, speed / least, how);
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
float vr_kdt_took[9]; // the trials' (one run at a time): how many each situation took
float vr_kdt_spawn_grunt;   // VR_KdTrig_TestSpawn: a grunt whatever vr_kdtrig_test_class (the thrown trials' corpse)
float vr_kdt_trial_push;    // the thrown trials': the pushes' sum, the downs, the staggers
float vr_kdt_trial_downs;
float vr_kdt_trial_staggers;
.entity vr_kdt_trial_body;
.vector vr_kdt_trial_from;

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
    if(at == '0 0 0') // (vr_kdtrig_test_x, _y, _z all 0: 64 units ahead of the player, along the view's yaw)
    {
        at = pl.origin + ('1 0 0' * cos(pl.v_viewangle_y * 0.0174533) + '0 1 0' * sin(pl.v_viewangle_y * 0.0174533)) * 64;
    }
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
    if(cvar("vr_kdtrig_test_face") == 1) // (facing away: hit from behind)
    {
        m.angles_y = m.angles_y + 180;
    }
    // (0 a grunt, 3 a death knight, 4 an ogre, 5 a grunt named monster_boglord: a boss, 6 a knight, 7 an enforcer, 8 a
    // shambler)
    float k = vr_kdt_spawn_grunt ? 0 : cvar("vr_kdtrig_test_class");
    if(k == 6)
    {
        m.classname = "monster_knight";
        monster_knight();
    }
    else if(k == 7)
    {
        m.classname = "monster_enforcer";
        monster_enforcer();
    }
    else if(k == 3)
    {
        m.classname = "monster_hell_knight";
        monster_hell_knight();
    }
    else if(k == 5)
    {
        m.classname = "monster_army"; // (a grunt named for a boss: the group is by classname)
        monster_army();
        m.classname = "monster_boglord";
    }
    else if(k == 4)
    {
        m.classname = "monster_ogre";
        monster_ogre();
    }
    else if(k == 8)
    {
        m.classname = "monster_shambler";
        monster_shambler();
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
    else if(xMode >= 8)
    {
        // 8 a two-hand open-palm shove (VR_Bash_Hit, a real one's), 9 it strikes you (a melee monster's blow: parried
        // as your pose is, VR_Parry), 10 knocked down at once (vr_knockdown_test 0's); aware of you or not
        // (vr_kdtrig_test_aware), facing as vr_kdtrig_test_face put it.
        best.enemy = cvar("vr_kdtrig_test_aware") ? pl : world;
        if(xMode == 8)
        {
            vr_bash_exhaust_dmg = 0;
            vr_bash_exhaust_push = 0;
            VR_Bash_Hit(best, chest, dir, QVR_HITKIND_SHOVE, -1, 1, VR_Shove_HandsPush(TRUE));
        }
        else if(xMode == 9)
        {
            vr_parry_test_force = cvar("vr_kdtrig_test_parry"); // (a forced parry: combat.qc VR_Parry)
            float before = pl.health;
            T_Damage(pl, best, best, 10);
            vr_parry_test_force = 0;
            dprint(sprintf("kdtrig test: its blow of 10 on you: health %g -> %g\n", before, pl.health));
        }
        else if(xMode == 11) // (11: its shove on you, vr_enemyshove.qc's: parried as your pose is)
        {
            entity was = self;
            self = best;
            self.vr_eshove_target = pl;
            vr_parry_test_force = cvar("vr_kdtrig_test_parry");
            VR_EnemyShove_Hit();
            vr_parry_test_force = 0;
            self = was;
        }
        else if(canragdoll(best) && VR_Knockdown_MakeRoom(best))
        {
            best.velocity = dir * 220 + '0 0 120';
            VR_Knockdown_Start(best);
        }
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

// Ragdoll `body` flung at `aim`'s middle at vr_kdtrig_test_speed (a throw's arc: up by the gravity over its flight).
float(entity body, entity aim, float xTest) VR_KdTrig_TestFling =
{
    float spd = max(10, cvar("vr_kdtrig_test_speed"));
    vector to = (aim.absmin + aim.absmax) * 0.5 - body.origin;
    float flight = vlen(to) / spd;
    vector vel = normalize(to) * spd + '0 0 1' * (0.5 * cvar("sv_gravity") * flight);
    float ok = ragdollfling(body, vel);
    dprint(sprintf("kdtrig test %g: %s flung at %s %.0f units off at %.0f u/s: %g\n", xTest, body.classname, aim.classname,
        vlen(to), spd, ok));
    return ok;
};

// The thrown-body trials (vr_kdtrig_test 44: trial mode 14): a fresh target (vr_kdtrig_test_class) at the test place, a
// grunt 128 units before it (towards you) killed (a corpse), flung at it at vr_kdtrig_test_speed, judged after
// vr_kdtrig_test_wait s (0: 1.2): down, staggered or stood, and how far it was pushed (level units from where it stood).
float VR_KDT_TRIAL_THROWN = 14;

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
    entity tm = self.vr_kdt_trial_mon;
    if(self.vr_kdt_trial_mode == VR_KDT_TRIAL_THROWN && self.vr_kdt_trial_phase == 0)
    {
        // (The default place, 64 units ahead, put the corpse 64 units behind you: in a wall or off the ledge you stand
        // at, it fell where the trace found a floor and was flung from 500 units off, landing after the judging. The
        // target 192 units ahead: the corpse between you, 64 ahead, as the trials were meant.)
        if(at == '0 0 0')
        {
            at = pl.origin + ('1 0 0' * cos(pl.v_viewangle_y * 0.0174533) + '0 1 0' * sin(pl.v_viewangle_y * 0.0174533)) * 192;
        }
        tm = VR_KdTrig_TestSpawn(pl, at);
        self.vr_kdt_trial_mon = tm;
        vector back = pl.origin - tm.origin;
        back_z = 0;
        back = normalize(back);
        vr_kdt_spawn_grunt = TRUE;
        entity c = VR_KdTrig_TestSpawn(pl, tm.origin + back * 128);
        vr_kdt_spawn_grunt = FALSE;
        self.vr_kdt_trial_body = c;
        pl.flags = pl.flags | FL_NOTARGET; // (the target stands where it is: its push is the strike's alone)
        self.vr_kdt_trial_phase = 10;
        self.nextthink = time + 0.5;
        return;
    }
    if(self.vr_kdt_trial_mode == VR_KDT_TRIAL_THROWN && self.vr_kdt_trial_phase == 10) // (its AI begun: killed)
    {
        T_Damage(self.vr_kdt_trial_body, pl, pl, self.vr_kdt_trial_body.health + 1);
        self.vr_kdt_trial_from_x = time + 4; // (until then the corpse may still be dying)
        self.vr_kdt_trial_phase = 1;
        self.nextthink = time + 0.5;
        return;
    }
    if(self.vr_kdt_trial_mode == VR_KDT_TRIAL_THROWN && self.vr_kdt_trial_phase == 1)
    {
        if(!isragdoll(self.vr_kdt_trial_body) && time < self.vr_kdt_trial_from_x) // (its death's frames first)
        {
            self.nextthink = time + 0.1;
            return;
        }
        tm.vr_kdt_fired = 0;
        tm.vr_kdt_bump = 0;
        self.vr_kdt_trial_from = tm.origin;
        if(!isragdoll(self.vr_kdt_trial_body) || !VR_KdTrig_TestFling(self.vr_kdt_trial_body, tm, 44))
        {
            dprint("kdtrig trial: thrown: no corpse ragdoll to fling\n");
        }
        self.vr_kdt_trial_phase = 2;
        self.nextthink = time + (cvar("vr_kdtrig_test_wait") > 0 ? cvar("vr_kdtrig_test_wait") : 1.2);
        return;
    }
    if(self.vr_kdt_trial_mode == VR_KDT_TRIAL_THROWN)
    {
        vector now = isragdoll(tm) ? ragdollpoint(tm, 0, '0 0 0', TRUE) : tm.origin; // (down: its pelvis)
        vector moved = now - self.vr_kdt_trial_from;
        moved_z = 0;
        string what = tm.vr_kdt_bump == 2 ? "down" : tm.vr_kdt_bump == 1 ? "staggers" : "stands";
        dprint(sprintf("kdtrig trial: thrown at %.0f u/s on %s: %s, pushed %.0f units\n", cvar("vr_kdtrig_test_speed"),
            tm.classname, what, vlen(moved)));
        vr_kdt_trial_push = vr_kdt_trial_push + vlen(moved);
        vr_kdt_trial_downs = vr_kdt_trial_downs + (tm.vr_kdt_bump == 2);
        vr_kdt_trial_staggers = vr_kdt_trial_staggers + (tm.vr_kdt_bump == 1);
        VR_KdTrig_TestClear();
        self.vr_kdt_trial_left = self.vr_kdt_trial_left - 1;
        if(self.vr_kdt_trial_left > 0)
        {
            self.vr_kdt_trial_phase = 0;
            self.nextthink = time + 0.1;
            return;
        }
        float tn = max(1, cvar("vr_kdtrig_test_n"));
        dprint(sprintf("kdtrig test thrown: %.0f u/s: down %g, staggered %g, stood %g of %g; mean push %.0f units\n",
            cvar("vr_kdtrig_test_speed"), vr_kdt_trial_downs, vr_kdt_trial_staggers,
            tn - vr_kdt_trial_downs - vr_kdt_trial_staggers, tn, vr_kdt_trial_push / tn));
        pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
        remove(self);
        return;
    }
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
        self.nextthink = time + (cvar("vr_kdtrig_test_wait") > 0 ? cvar("vr_kdtrig_test_wait") : 0.2);
        return;
    }
    float k = m.vr_kdt_fired;
    dprint(sprintf("kdtrig trial: %s at %v health %g: %s%s\n", m.classname, m.origin, m.health,
        k ? vr_kdt_name[k - 1] : "stands", m.vr_knockdown == 1 ? (m.vr_kd_stagger ? ", staggering" : ", down") :
        m.vr_knockdown == 2 ? ", rising" : ""));
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
    if(want == 22) // (every monster gone: a clean slate between cases)
    {
        VR_KdTrig_TestClear();
        return;
    }
    if(want == 21)
    {
        vector ahead = VRForwardOf('0 1 0' * pl.v_viewangle_y);
        ahead_z = 0;
        VR_KdTrig_TestSpawn(pl, pl.origin + normalize(ahead) * 160);
        return;
    }
    if(want == 12)
    {
        // The ragdoll nearest him flung at the live monster (on its feet) nearest it.
        entity body = world;
        float bd = 100000;
        for(entity r = nextent(world); r; r = nextent(r))
        {
            if(isragdoll(r) && vlen(r.origin - pl.origin) < bd)
            {
                bd = vlen(r.origin - pl.origin);
                body = r;
            }
        }
        entity aim = world;
        float ad = 100000;
        for(entity t = nextent(world); t; t = nextent(t))
        {
            if(t != body && (t.flags & FL_MONSTER) && t.health > 0 && !t.vr_knockdown && vlen(t.origin - body.origin) < ad)
            {
                ad = vlen(t.origin - body.origin);
                aim = t;
            }
        }
        if(!body || !aim)
        {
            dprint("kdtrig test: no ragdoll, or no live monster to fling it at\n");
            return;
        }
        VR_KdTrig_TestFling(body, aim, 12);
        return;
    }
    if(want == 13)
    {
        entity dead = VR_KdTrig_TestTarget(pl);
        if(dead)
        {
            dprint(sprintf("kdtrig test 13: %s killed\n", dead.classname));
            T_Damage(dead, pl, pl, dead.health + 1);
        }
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
        vr_kdt_trial_push = 0;
        vr_kdt_trial_downs = 0;
        vr_kdt_trial_staggers = 0;
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
