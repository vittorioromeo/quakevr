//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_parry.qc -- armed humanoids parry melee blows (vr_agrunt_parry; Combat > Armed Grunts > Parrying;
// ROUND22.md, "Armed grunts parry"). An armed grunt or enforcer holding a weapon defends against a melee blow as you do:
// - It sees the blow coming (VR_AGPar_Frame, each frame its weapon thinks): your hand's weapon (or fist) closing on it
//   fast, or a monster's melee attack on it (an ogre's chainsaw, another armed grunt's swing). After a reaction time
//   (vr_agrunt_parry_react) and by a chance (vr_agrunt_parry_chance; both by skill) it raises and angles its weapon into
//   the blow's way (VR_AGPar_Pose: its pose's targets, the arms' IK reaching them as drawn). Never from behind, unaware
//   of the attacker, staggered, down, in pain, swinging itself or in a tug over its weapon.
// - The hit test is honest: only a blow that meets its weapon (the held weapon's line, as VR_AG_WeaponLine lays it, the
//   same the weapon blows and throws use) before its body is parried: your blow's swept weapon (its last pose to this
//   one: VR_Blow's points) passing within VR_AGPAR_R of it, before the point where it met the body; a monster's blow,
//   its way from its weapon (or its front, its arm's height) to the struck one's chest crossing it. A blow to the head,
//   the legs, a side the weapon doesn't cover: its full damage.
// - A parried blow: vr_agrunt_parry_damage of its damage (no zone multiplier, no beheading, no pain), the parry's ring
//   and sparks, your hand knocked back (haptics) and both pushed apart; a blow from the same attacker within
//   vr_parry_cooldown of its last parry is a quiet one (its damage cut, nothing else).
// - Stamina: each parry spends vr_agrunt_parry_cost of its vr_agrunt_parry_count; the parry that empties it knocks the
//   weapon out of its hand (VR_AG_Disarm: its clatter and grunt). It recovers vr_agrunt_parry_recover a second.
// ----------------------------------------------------------------------------

float VR_AGPAR_R = 5;            // units: your weapon's sweep this near its weapon's line meets it
float VR_AGPAR_RMON = 5;         // ... a monster's blow's way (its line to the chest's middle: its weapon held ready
                                 // beside that misses it, raised into it meets it)
float VR_AGPAR_FOV = 70;         // degrees either side of its facing: an attacker further round is behind it
float VR_AGPAR_MARK = 0.3;       // s: a parried motion's later blows on it are that parry's
vector VR_AGPAR_CHEST = '2 0 14'; // its chest (body space)

float vr_agpar_n;      // full parries (the tests)
float vr_agpar_quiet;  // quiet ones
float vr_agpar_dis;    // weapons knocked away by a parry
vector vr_agpar_x;     // where the blow met its weapon
string vr_agpar_why;   // why the last blow was not parried (the tests, developer)

// Its chance and reaction by skill (easy: slow, often caught out; nightmare: quick, sure).
float() VR_AGPar_SkillChance = { return skill < 1 ? 0.6 : skill < 2 ? 1 : skill < 3 ? 1.15 : 1.3; };
float() VR_AGPar_SkillReact = { return skill < 1 ? 1.4 : skill < 2 ? 1 : skill < 3 ? 0.85 : 0.7; };

// An armed one holding a weapon, parrying on (VR_Guard_Hand: it parries instead of throwing its arms up, whose drawn
// guard would move its weapon where the game's line is not).
float(entity g) VR_AGPar_Guards =
{
    return g.vr_ag && g.vr_ag_prop && cvar_hget(cvarh_vr_agrunt_parry);
};

// Whether `g` can parry a blow by `by` now (else vr_agpar_why).
float(entity g, entity by) VR_AGPar_Able =
{
    vr_agpar_why = "";
    if(!VR_AGPar_Guards(g) || g.health <= 0 || g.vr_ag_tug_pl || g.vr_ag_took_at > 0)
    {
        vr_agpar_why = "no weapon in hand";
        return FALSE;
    }
    if(g.vr_knockdown || g.vr_kd_stagger || isragdoll(g) || g.vr_parry_stagger_until > time)
    {
        vr_agpar_why = "staggered or down";
        return FALSE;
    }
    if(g.pain_finished > time)
    {
        vr_agpar_why = "in pain";
        return FALSE;
    }
    if(VR_AG_Striking(g))
    {
        vr_agpar_why = "swinging itself";
        return FALSE;
    }
    if(g.enemy != by && by.enemy != g)
    {
        vr_agpar_why = "unaware of it";
        return FALSE;
    }
    vector d = by.origin - g.origin;
    d_z = 0;
    if(vlen(d) > 1)
    {
        makevectors([0, g.angles_y, 0]);
        if(normalize(d) * v_forward < cos(VR_AGPAR_FOV * 0.017453292))
        {
            vr_agpar_why = "from behind";
            return FALSE;
        }
    }
    return TRUE;
};

// The sweep from the line a0..a1 (its last pose) to b0..b1 (now) meets its weapon line wa..wb within `r`: the sweep's
// share then (0 .. 1, vr_agpar_x where), -1 never.
float(vector a0, vector a1, vector b0, vector b1, vector wa, vector wb, float r) VR_AGPar_SweepMeets =
{
    float k;
    for(k = 0; k <= 4; k++)
    {
        float t = k * 0.25;
        vector p0 = a0 + (b0 - a0) * t;
        vector p1 = a1 + (b1 - a1) * t;
        if(VR_AG_SegDist(p0, p1, wa, wb) <= r)
        {
            // (Where: its weapon's point nearest the sweep's line's middle.)
            vector w = wb - wa;
            vector m = (p0 + p1) * 0.5;
            float f = (w * w) > 0.01 ? min(1, max(0, ((m - wa) * w) / (w * w))) : 0;
            vr_agpar_x = wa + w * f;
            return t;
        }
    }
    return -1;
};

// ... against each weapon `g` holds (two held: either).
float(entity g, vector a0, vector a1, vector b0, vector b1, float r) VR_AGPar_SweepHeld =
{
    float t = -1;
    if(g.vr_ag_prop)
    {
        VR_AG_WeaponLine(g.vr_ag_prop);
        t = VR_AGPar_SweepMeets(a0, a1, b0, b1, vr_ag_wa, vr_ag_wb, r);
    }
    if(g.vr_ag_prop2)
    {
        VR_AG_WeaponLine(g.vr_ag_prop2);
        float t2 = VR_AGPar_SweepMeets(a0, a1, b0, b1, vr_ag_wa, vr_ag_wb, r);
        if(t2 >= 0 && (t < 0 || t2 < t))
        {
            t = t2;
        }
    }
    return t;
};

// The sweep's share when it passed nearest `c` (where the blow met the body).
float(vector a0, vector a1, vector b0, vector b1, vector c) VR_AGPar_SweepAt =
{
    float best = 0;
    float bestD = 100000;
    float k;
    for(k = 0; k <= 4; k++)
    {
        float t = k * 0.25;
        float d = VR_AG_SegDist(a0 + (b0 - a0) * t, a1 + (b1 - a1) * t, c, c);
        if(d < bestD)
        {
            bestD = d;
            best = t;
        }
    }
    return best;
};

// Player `pl`'s hand `h`'s weapon this frame (its blow's points: the last pose and now) meets `g`'s weapon (-1 not; its
// share of the sweep). `bodyAt` the point where it met the body ('0 0 0': none): the weapon met after it is no parry.
float(entity g, entity pl, float h, vector bodyAt) VR_AGPar_PlayerMeets =
{
    entity was = self;
    self = pl;
    entity b = VR_Blow(h);
    self = was;
    float n = b.mh_n;
    if(n < 1)
    {
        return -1;
    }
    vector a0 = b.mh_pp[0];
    vector a1 = b.mh_pp[n - 1];
    vector b0 = b.mh_cp[0];
    vector b1 = b.mh_cp[n - 1];
    float t = VR_AGPar_SweepHeld(g, a0, a1, b0, b1, VR_AGPAR_R);
    if(t < 0 || bodyAt == '0 0 0')
    {
        return t;
    }
    float tb = VR_AGPar_SweepAt(a0, a1, b0, b1, bodyAt);
    if(t > tb + 0.25)
    {
        vr_agpar_why = "its body met first";
        return -1;
    }
    return t;
};

// A monster `m`'s blow on `g`: its way from its weapon (an armed one's held weapon's far end; else its front at its
// arm's height) to `g`'s chest meets `g`'s weapon.
float(entity g, entity m) VR_AGPar_MonsterMeets =
{
    VR_AGP_Body(g);
    vector chest = g.origin + VR_AGP_ToWorld(VR_AGPAR_CHEST);
    vector from;
    vector to = chest - m.origin;
    to_z = 0;
    to = vlen(to) > 1 ? normalize(to) : '1 0 0';
    if(m.vr_ag && m.vr_ag_prop)
    {
        VR_AG_WeaponLine(m.vr_ag_prop);
        from = vr_ag_wb;
        // (Its own weapon's line too: blade on blade.)
        vector wa = vr_ag_wa;
        vector wb = vr_ag_wb;
        if(VR_AGPar_SweepHeld(g, wa, wb, wa, wb, VR_AGPAR_R) >= 0)
        {
            return 1;
        }
    }
    else
    {
        from = m.origin + to * max(8, m.maxs_x) + '0 0 1' * (m.mins_z + (m.maxs_z - m.mins_z) * 0.6);
    }
    return VR_AGPar_SweepHeld(g, from, chest, from, chest, VR_AGPAR_RMON) >= 0;
};

// `g` parries `by`'s blow (`h` its hand, -1 a monster's), met at vr_agpar_x: the ring, sparks, both pushed apart, its
// stamina spent (the last knocks its weapon away). The blow's damage `damage` cut (vr_agpar_now set).
float(entity g, entity by, float h, float damage) VR_AGPar_Parried =
{
    vr_agpar_now = TRUE;
    vector away = by.origin - g.origin; // (from it to the attacker)
    away_z = 0;
    away = vlen(away) > 1 ? normalize(away) : '1 0 0';
    float kept = min(1, max(0, cvar_hget(cvarh_vr_agrunt_parry_damage)));
    float quiet = g.vr_agpar_by == by && time - g.vr_agpar_last < max(0, cvar_hget(cvarh_vr_parry_cooldown));
    g.vr_agpar_by = by;
    g.vr_agpar_last = time;
    g.vr_agpar_hand = h + 1;
    if(by.flags & FL_CLIENT)
    {
        // (Its weapon's later blows this motion are this parry's; its hand's knock on the weapon too: VR_AG_HandStrike.)
        if(h == cVR_OffHand) { by.vr_ag_knock_off = time; } else { by.vr_ag_knock_at = time; }
    }
    if(quiet)
    {
        vr_agpar_quiet = vr_agpar_quiet + 1;
        VR_AG_Log(sprintf("parry: %s's blow again (cooldown): %g of %g", by.classname, damage * kept, damage));
        return damage * kept;
    }
    vr_agpar_n = vr_agpar_n + 1;
    sound(g, CHAN_AUTO, "vr/parry.wav", 1, ATTN_NORM);
    VR_SoundVaried(g, CHAN_AUTO, "player/axhit2.wav", 0.35, ATTN_NORM);
    particle2(vr_agpar_x + away * 2, (away + '0 0 0.5') * 80, QVR_PARTICLE_PRESET_SPARKS, 24);
    if((by.flags & FL_CLIENT) && h >= 0)
    {
        entity was = self;
        self = by;
        haptic(h, 0.0, 0.3, 90, 1.0);
        handimpact(h, 5, away);
        if(VRGet2HAiming(self))
        {
            haptic(VRGetOtherHand(h), 0.0, 0.25, 90, 0.8);
            handimpact(VRGetOtherHand(h), 3, away);
        }
        self = was;
        VR_Push(by, away, 160 * cvar_hget(cvarh_vr_parry_push_player), 30 * cvar_hget(cvarh_vr_parry_push_player));
    }
    else
    {
        VR_Push(by, away, 260 * cvar_hget(cvarh_vr_parry_push_enemy), 80 * cvar_hget(cvarh_vr_parry_push_enemy));
    }
    VR_Push(g, away * -1, 140, 30);
    // Its stamina: what it spent recovers over time; the parry that empties it costs it its weapon.
    float count = max(0.1, cvar_hget(cvarh_vr_agrunt_parry_count));
    float load = max(0, g.vr_agpar_load - (time - g.vr_agpar_load_at) * max(0, cvar_hget(cvarh_vr_agrunt_parry_recover)));
    load = load + max(0, cvar_hget(cvarh_vr_agrunt_parry_cost));
    g.vr_agpar_load_at = time;
    g.vr_agpar_load = load;
    VR_AG_Log(sprintf("parry: %s's blow on its weapon at %v: %g of %g damage, stamina spent %.2f of %g", by.classname,
        vr_agpar_x, damage * kept, damage, load, count));
    // (Within half a parry of empty: two parries in a row, at the defaults, are those within about 1.4 s.)
    if(load >= count - 0.5 * max(0, cvar_hget(cvarh_vr_agrunt_parry_cost)) - 0.001)
    {
        g.vr_agpar_load = 0;
        vr_agpar_dis = vr_agpar_dis + 1;
        if(g.vr_ag_prop2 && !g.vr_ag_tug_pl && vlen(vr_agpar_x - g.vr_ag_prop2.origin) < vlen(vr_agpar_x - g.vr_ag_prop.origin))
        {
            VR_AG_Swap(g); // (the weapon that took the blow)
        }
        VR_AG_Disarm(g, away * -1, 140, "parried until its strength gave out");
    }
    return damage * kept;
};

// T_Damage_VRMelee and T_Damage: a blow of `attacker`'s on armed `targ` (its damage `damage`): parried (cut, vr_agpar_now
// set) if it met the weapon first and it could parry.
float(entity targ, entity inflictor, entity attacker, float damage) VR_AGPar_Blow =
{
    vr_agpar_now = FALSE;
    vr_agpar_why = "";
    if(!targ.vr_ag || !targ.vr_ag_prop || targ == attacker || damage <= 0 || !cvar_hget(cvarh_vr_agrunt_parry) ||
       targ.health <= 0)
    {
        return damage;
    }
    float h = -1;
    if(attacker.flags & FL_CLIENT)
    {
        if(vr_hitkind != QVR_HITKIND_MELEE || vr_hithand < 0 || inflictor != attacker)
        {
            return damage;
        }
        h = vr_hithand;
        // (This motion already parried on it: its blows that get through are that parry's.)
        if(targ.vr_agpar_by == attacker && targ.vr_agpar_hand == h + 1 && time - targ.vr_agpar_last < VR_AGPAR_MARK)
        {
            vr_agpar_x = vr_hitpos;
            return VR_AGPar_Parried(targ, attacker, h, damage);
        }
        if(VR_AGPar_PlayerMeets(targ, attacker, h, vr_hitpos) < 0)
        {
            if(vr_agpar_why == "")
            {
                vr_agpar_why = "missed its weapon";
            }
            VR_AG_Log(sprintf("parry: none: your blow %s", vr_agpar_why));
            return damage;
        }
    }
    else if((attacker.flags & FL_MONSTER) && inflictor == attacker && VR_Parry_MeleeMonster(attacker))
    {
        vector d = attacker.origin - targ.origin;
        d_z = 0;
        if(vlen(d) > 160)
        {
            return damage;
        }
        if(!VR_AGPar_MonsterMeets(targ, attacker))
        {
            vr_agpar_why = "missed its weapon";
            VR_AG_Log(sprintf("parry: none: %s's blow missed its weapon", attacker.classname));
            return damage;
        }
    }
    else
    {
        return damage;
    }
    if(!VR_AGPar_Able(targ, attacker))
    {
        VR_AG_Log(sprintf("parry: none: %s's blow on its weapon, but %s", attacker.classname, vr_agpar_why));
        return damage;
    }
    // (Its weapon raised for this one, or met where it was held: either way it meets the blow.)
    return VR_AGPar_Parried(targ, attacker, h, damage);
};

// VR_AG_WeaponStruck (its weapon's think, each frame): your blow on its weapon that hasn't met its body (the weapon held
// out): parried now. TRUE: its weapon knocked away.
float(entity g) VR_AGPar_WeaponFrame =
{
    if(!cvar_hget(cvarh_vr_agrunt_parry) || !g.vr_ag_prop)
    {
        return FALSE;
    }
    entity pl = find(world, classname, "player");
    while(pl)
    {
        if(pl.health > 0 && vlen(pl.origin - g.origin) < 160)
        {
            float h;
            for(h = 0; h < 2; h++)
            {
                if(g.vr_agpar_by == pl && g.vr_agpar_hand == h + 1 && time - g.vr_agpar_last < VR_AGPAR_MARK)
                {
                    continue;
                }
                entity was = self;
                self = pl;
                entity b = VR_Blow(h);
                self = was;
                float lenient = b.mh_lenient > 0 ? b.mh_lenient : 1;
                if(!b.mh_fresh || b.mh_dt <= 0 || b.mh_thing == VR_MTHING_NONE || b.mh_fast < VR_Melee_MinSpeed() * lenient)
                {
                    continue;
                }
                if(VR_AGPar_PlayerMeets(g, pl, h, '0 0 0') >= 0 && VR_AGPar_Able(g, pl))
                {
                    VR_AGPar_Parried(g, pl, h, 0);
                    vr_agpar_now = FALSE;
                    if(!g.vr_ag_prop)
                    {
                        return TRUE;
                    }
                }
            }
        }
        pl = find(pl, classname, "player");
    }
    return FALSE;
};

// ----------------------------------------------------------------------------
// Seeing the blow coming; the parry's pose
// ----------------------------------------------------------------------------

// Its weapon's think, each frame: a blow coming at it (your hand's weapon or fist closing on it fast, a monster's melee
// attack on it) decided once (the chance), the parry raised after the reaction time and held while it comes; its point
// (body space, eased) the pose's target.
void(entity g) VR_AGPar_Frame =
{
    if(!VR_AGPar_Guards(g))
    {
        g.vr_agpar_w = 0;
        g.vr_agpar_since = 0;
        return;
    }
    float mu = VRMetersToUnits();
    VR_AGP_Body(g);
    vector chest = g.origin + VR_AGP_ToWorld(VR_AGPAR_CHEST);
    entity by = world;
    vector at = '0 0 0';
    float bestD = 100000;
    entity pl = find(world, classname, "player");
    while(pl)
    {
        if(pl.health > 0 && vlen(pl.origin - g.origin) < 160)
        {
            float h;
            for(h = 0; h < 2; h++)
            {
                if(VR_Melee_Thing(pl, h) == VR_MTHING_NONE)
                {
                    continue;
                }
                vector hp = VRGetEntHandPos(pl, h);
                vector tip = h == cVR_MainHand ? pl.muzzlepos : pl.offmuzzlepos;
                if(VRIsHandEmpty(pl, h) || vlen(tip - hp) > 48)
                {
                    tip = hp;
                }
                vector hv = VRGetEntHandVel(pl, h) - pl.velocity;
                float sp = vlen(hv) / mu;
                vector ahead = tip + hv * 0.25;
                float dNow = vlen(tip - chest);
                float dWay = VR_AG_SegDist(tip, ahead, chest, chest);
                if(dNow < 30 || (sp >= 1.2 && dWay < 40 && (chest - tip) * hv > 0))
                {
                    if(dNow < bestD)
                    {
                        bestD = dNow;
                        by = pl;
                        at = (hp + tip) * 0.5;
                    }
                }
            }
        }
        pl = find(pl, classname, "player");
    }
    entity m = findradius(g.origin, 170);
    while(m)
    {
        if((m.flags & FL_MONSTER) && m != g && m.health > 0 && m.enemy == g && m.vr_melee_begun > 0 &&
           time - m.vr_melee_begun < 1.5 && VR_Parry_MeleeMonster(m))
        {
            float dm = vlen(m.origin - g.origin);
            if(dm < bestD)
            {
                bestD = dm;
                by = m;
                if(m.vr_ag && m.vr_ag_prop)
                {
                    VR_AG_WeaponLine(m.vr_ag_prop);
                    at = (vr_ag_wa + vr_ag_wb) * 0.5;
                }
                else
                {
                    vector tm = chest - m.origin;
                    tm_z = 0;
                    at = m.origin + normalize(tm) * max(8, m.maxs_x) + '0 0 1' * (m.mins_z + (m.maxs_z - m.mins_z) * 0.6);
                }
            }
        }
        m = m.chain;
    }
    if(by && VR_AGPar_Able(g, by))
    {
        if(!g.vr_agpar_since || time - g.vr_agpar_seen > 0.35)
        {
            // A new blow coming: will it see it in time?
            g.vr_agpar_since = time;
            float chance = min(1, max(0, cvar_hget(cvarh_vr_agrunt_parry_chance)) * VR_AGPar_SkillChance());
            float react = max(0, cvar_hget(cvarh_vr_agrunt_parry_react)) * VR_AGPar_SkillReact();
            g.vr_agpar_ready = random() < chance ? time + react : -1;
            VR_AG_Log(sprintf("parry: %s's blow coming: %s", by.classname,
                g.vr_agpar_ready > 0 ? sprintf("raised in %.2f s", react) : "caught out"));
        }
        g.vr_agpar_seen = time;
        g.vr_agpar_tby = by;
        vector tl = VR_AGP_ToLocal(at - g.origin);
        g.vr_agpar_t = g.vr_agpar_w > 0.01 ? g.vr_agpar_t + (tl - g.vr_agpar_t) * min(1, frametime * 14) : tl;
    }
    float up = g.vr_agpar_ready > 0 && time >= g.vr_agpar_ready && time - g.vr_agpar_seen < 0.35 &&
               VR_AGPar_Able(g, g.vr_agpar_tby);
    if(!up && time - g.vr_agpar_seen >= 0.35)
    {
        g.vr_agpar_since = 0;
    }
    g.vr_agpar_w = VR_AGP_Ease(g.vr_agpar_w, up ? 1 : 0, up ? 9 : 3);
};

// The parry's pose (VR_AGP_Pose, after its weapon's own: before a blade's tilt), its share vr_agpar_w: the held weapon's
// middle put between its chest and the blow (17 units out from its chest towards it), its line across the blow's way:
// upright against a blow at its middle, level over its head against one from above, its edge to the attacker.
void(entity g, entity p) VR_AGPar_Pose =
{
    float w = g.vr_agpar_w;
    if(w <= 0 || !p || p != g.vr_ag_prop)
    {
        return;
    }
    vector d = g.vr_agpar_t - VR_AGPAR_CHEST;
    if(vlen(d) < 1)
    {
        d = '1 0 0';
    }
    d = normalize(d);
    d_x = max(d_x, 0.35); // (in front of it)
    d = normalize(d);
    vector mid = VR_AGPAR_CHEST + d * 17;
    mid_x = max(mid_x, 8);
    mid_z = min(30, max(-2, mid_z));
    vector up = '0 0 1' - d * d_z;
    up = vlen(up) > 0.01 ? normalize(up) : '0 0 1';
    vector across = VR_AGP_Cross('0 0 1', d); // (to its left)
    across = vlen(across) > 0.01 ? normalize(across) : '0 1 0';
    float high = VR_AGP_Smooth((d_z - 0.15) / 0.45);
    vector a = normalize(up * (1 - high) + across * (high + 0.3 * (1 - high)));
    float len = min(40, max(8, vlen(weaponmuzzle(p) - p.origin)));
    vector grip = mid - a * (len * 0.5 - 4);
    vector edge = d - a * (d * a);
    edge = vlen(edge) > 0.01 ? normalize(edge) : '1 0 0';
    VR_AGP_Body(g);
    vr_agp_v = vr_agp_v * (1 - w) + VR_AM_Fit(g, grip) * w;
    vr_agp_wr = vr_agp_wr * (1 - w);
    vr_agp_fw = VR_AGP_Nlerp(vr_agp_fw, VR_AGP_ToWorld(a), w);
    vr_agp_up = VR_AGP_Nlerp(vr_agp_up, VR_AGP_ToWorld(edge), w);
    vr_agp_up = vr_agp_up - vr_agp_fw * (vr_agp_up * vr_agp_fw);
    vr_agp_up = vlen(vr_agp_up) > 0.01 ? normalize(vr_agp_up) : '0 0 1';
    vr_agp_wo = vr_agp_wo * (1 - w);
};
