#ifdef QVR_TESTS
// vr_decap's test harness (moved out of vr_decap.qc; ROUND22's QC cleanup: a pure move). Its globals and the
// cvars it reads stay with the feature.

// The nearest monster (live: xLive 1; a dead one: 0) to `self` within 400 units (a crowd's test: its monster in turn).
entity(float xLive) VR_Decap_TestTarget =
{
    entity best = world;
    float bestDist = vr_gore_test_only != world ? 999999 : 400;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.vr_gib || e.vr_headless || (xLive ? e.health <= 0 : e.health > 0) ||
           (vr_gore_test_only != world && e != vr_gore_test_only))
        {
            continue;
        }
        float d = vlen(e.origin - self.origin);
        if(d < bestDist)
        {
            best = e;
            bestDist = d;
        }
    }
    return best;
};

// Hand `cVR_MainHand`'s blow made up: `xThing` held across the line from the player to `xAt` (its grip `xF` of its
// length short of it), moving `xAlong` (1: along its line) at `xSpeed` m/s, as kind `xKind`.
void(vector xAt, float xThing, float xKind, float xF, float xAlong, float xSpeed) VR_Decap_TestBlow =
{
    entity b = VR_Blow(cVR_MainHand);
    vector toward = xAt - self.origin;
    toward_z = 0;
    toward = normalize(toward);
    vector axis;
    axis_x = -toward_y;
    axis_y = toward_x;
    axis_z = 0;
    float len = 30;
    b.mh_thing = xThing;
    b.mh_wid = WID_SWORD;
    if(xThing == VR_MTHING_AXE)
    {
        b.mh_wid = WID_AXE;
    }
    else if(xThing == VR_MTHING_SAW)
    {
        b.mh_wid = WID_CHAINSAW;
    }
    b.mh_kind = xKind;
    b.mh_speed = xSpeed;
    b.mh_free = 0;
    b.mh_grip = xAt - axis * (len * xF);
    b.mh_far = b.mh_grip + axis * len;
    b.mh_near = b.mh_grip - axis * 6;
    vector eye = self.origin + self.view_ofs;
    b.mh_rgrip = VR_Blow_ToPlaySpace(b.mh_grip - eye) * (1 / VRMetersToUnits());
    b.mh_rfar = VR_Blow_ToPlaySpace(b.mh_far - eye) * (1 / VRMetersToUnits());
    b.mh_rnear = VR_Blow_ToPlaySpace(b.mh_near - eye) * (1 / VRMetersToUnits());
    vector way = normalize(toward * (1 - xAlong) + axis * xAlong);
    vector v = VR_Blow_ToPlaySpace(way) * xSpeed;
    b.mh_vgrip = v;
    b.mh_vfar = v;
    b.mh_vnear = v;
    b.mh_c_pos = xAt;
    b.mh_c_dir = way;
    b.mh_c_ent = world;
};

// The made-up blow (VR_Decap_TestBlow) swept through `xAt` (its points from 10 units short of their line's place to 10
// past, along its way) as the hand's blows find what they strike (VR_Melee_Sweep's targets, VR_Melee_BoxTarget: a
// knocked-down one's ragdoll too; its model as drawn, hitmodel_segment): whether it meets `m`, where (b.mh_c_pos). (Not
// VR_Melee_Sweep itself: its blow is judged by the hand's own motion, which a made-up blow has none of.)
float(entity m, vector xAt) VR_Decap_TestSweep =
{
    entity b = VR_Blow(cVR_MainHand);
    VR_Melee_LayoutOf(b);
    vector way = b.mh_c_dir;
    if(!VR_Blow_Strikable(m) || !VR_Melee_BoxTarget(m))
    {
        return FALSE;
    }
    float best = 2;
    for(float k = 0; k < vr_mpt_n; k++)
    {
        vector p = b.mh_grip + (b.mh_far - b.mh_grip) * vr_mpt_f[k];
        float t = hitmodel_target(m) ? hitmodel_segment(m, p - way * 10, p + way * 10, VR_MELEE_RADIUS, HITCLASS_MELEE) : 0;
        if(t >= 0 && t < best && VR_Melee_Strikes(vr_mpt_kind[k]))
        {
            best = t;
            b.mh_c_pos = p - way * 10 + way * (20 * t);
        }
    }
    return best <= 1;
};

// A head pop's test shot from your eye at `xAt`: a blast (`xWeapon` QVR_DECAP_SHOTGUN or _SSG: its pellets, no spread)
// or a bolt (_LIGHTNING), as the weapon's (vr_decap_weapon, its Weapon Damage).
void(float xWeapon, vector xAt) VR_Decap_TestShot =
{
    vector src = self.origin + self.view_ofs;
    if(vr_gore_test_only != world)
    {
        src = xAt + normalize(src - xAt) * 24; // (a crowd's: from just before it, not through the others)
    }
    vector dir = normalize(xAt - src);
    VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
    vr_decap_weapon = xWeapon;
    if(xWeapon == QVR_DECAP_LIGHTNING)
    {
        self.damage_weapon = IT_LIGHTNING;
        LightningDamage(src, xAt + dir * 40, self, cvar_hget(cvarh_vr_dmg_lightning));
    }
    else
    {
        self.damage_weapon = IT_SHOTGUN;
        vr_pellet_damage = cvar_hget(xWeapon == QVR_DECAP_SSG ? cvarh_vr_dmg_super_shotgun : cvarh_vr_dmg_shotgun);
        vr_tracer_hand = cVR_MainHand;
        vr_tracer_from = src;
        vector spread = '0 0 0';
        if(vr_poptest_spread)
        {
            spread = (xWeapon == QVR_DECAP_SSG) ? '0.035 0.025 0' : '0.01 0.01 0'; // (W_FireSuperShotgun's, W_FireShotgun's)
        }
        FireBulletsImpl(xWeapon == QVR_DECAP_SSG ? 14 : 6, dir, spread, src, 4096, FALSE);
        vr_pellet_damage = 0;
    }
    vr_decap_weapon = 0;
    VR_ClearHitKind();
};

// A test blast's damage, all at the head (`xWeapon` as VR_Decap_TestShot's).
float(float xWeapon) VR_Decap_TestBlastDamage =
{
    float head = cvar_hget(cvarh_vr_positional_damage) ? PositionalDamageFactor(1) : 1;
    if(xWeapon == QVR_DECAP_SSG)
    {
        return 14 * cvar_hget(cvarh_vr_dmg_super_shotgun) * head;
    }
    return 6 * cvar_hget(cvarh_vr_dmg_shotgun) * head;
};

// Heads (the head gib models) in the level.
float() VR_Decap_TestHeads =
{
    float n = 0;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(e.vr_gib && e.gorging) // (a head: ThrowHead's, VR_Decap_ThrowHead's)
        {
            n++;
        }
    }
    return n;
};

void(float xTest) VR_Decap_PopTest =
{
    if(xTest == 40)
    {
        float lengths[12] = {1, 2, 3, 3.5, 5, 7, 9, 11, 13, 14.5, 15, 25};
        for(float w = 0; w < 3; ++w)
        {
            float how = (w == 0) ? QVR_DECAP_SHOTGUN : (w == 1) ? QVR_DECAP_SSG : QVR_DECAP_LIGHTNING;
            for(float sh = 0; sh < 3; ++sh)
            {
                float share = (sh == 0) ? 1 : (sh == 1) ? 0.5 : 0.2;
                string line = sprintf("poptest: table %s share %.1f:", VR_Decap_HowName(how), share);
                for(float i = 0; i < 12; ++i)
                {
                    line = strcat(line, sprintf(" %g=%.2f", lengths[i],
                        VR_Decap_PopChance(how, lengths[i] * VR_DECAP_PLAYER_LENGTH, share)));
                }
                bprint(strcat(line, "\n"));
            }
        }
        return;
    }
    entity m = VR_Decap_TestTarget(TRUE);
    if(m == world)
    {
        bprint(sprintf("poptest: %g: no live monster near\n", xTest));
        return;
    }
    VR_Decap_HeadZone(m);
    makevectors('0 1 0' * m.angles_y);
    vector at = m.origin + v_forward * xHeadForward + VR_HeadSide(v_forward) + '0 0 1' * xHeadUp;
    vector aim = at;
    if(xTest == 44)
    {
        aim = m.origin + '0 0 4';
    }
    // Where to shoot from: level with its head, `dist` off, the way from it to you first.
    float dist = cvar("vr_decap_poptest_dist") * VR_DECAP_PLAYER_LENGTH;
    vector away = self.origin - at;
    away_z = 0;
    float yaw0 = vectoyaw(away);
    vector eye = '0 0 0';
    float clear = FALSE;
    for(float k = 0; k < 16; ++k)
    {
        makevectors('0 1 0' * (yaw0 + k * 22.5));
        eye = aim + v_forward * dist;
        traceline(eye, aim, FALSE, self); // (nothing in the way: no wall, no other monster, no training dummy)
        if(!trace_startsolid && (trace_ent == m || trace_fraction == 1))
        {
            clear = TRUE;
            break;
        }
    }
    if(!clear)
    {
        bprint(sprintf("poptest: %g: no clear line %.0f units from it\n", xTest, dist));
        return;
    }
    vector old = self.origin;
    setorigin(self, eye - self.view_ofs);
    float range = vlen(at - eye);
    if(xTest == 47 || xTest == 48)
    {
        // A thing thrown at its head (or its body: vr_decap_poptest_body) at 16 m/s, met just before it: its real hit
        // (forcegrabbable_touch for a weapon, VR_SolidProp_Impact for the box).
        vector to = at;
        if(cvar("vr_decap_poptest_body"))
        {
            to = m.origin + '0 0 4';
        }
        vector dir = normalize(to - eye);
        vector vel = dir * 16 * VRMetersToUnits();
        entity thing = world;
        m.health = 1;
        if(xTest == 47)
        {
            float wid = cvar("vr_decap_poptest_wid");
            thing = CreateThrownWeapon(self, wid, 0, WeaponInst_New(0), to - dir * 2, vectoangles(dir), vel, '0 0 0',
                FALSE, TRUE, WeaponIdToThrowDamage(wid));
        }
        else
        {
            entity oldSelf = self;
            self = spawn();
            setorigin(self, to - dir * 2);
            misc_explobox();
            thing = self;
            self = oldSelf;
            thing.takedamage = DAMAGE_NO; // (never blows up)
            setorigin(thing, to - dir * 2);
            thing.velocity = vel;
            thing.owner = self;
            thing.throwhit = QVR_THROWHIT_NEVER_HIT;
            thing.carry_thrown = time;
            thing.throwdmgspeed = 700 * VRMetersToUnits() / 120; // (as VR_Carry_Release throws it)
            SetThrownWeaponBaseDamage(thing, cvar_hget(cvarh_vr_carry_throw_damage));
        }
        float pops0 = vr_decap_pops;
        entity me = self;
        self = thing;
        other = m;
        if(xTest == 47)
        {
            forcegrabbable_touch();
        }
        else
        {
            VR_SolidProp_Impact(16);
        }
        self = me;
        bprint(sprintf("poptest: %g %s (%s, %.1f kg) thrown at the %s of %s: ", xTest, thing.classname, thing.netname,
            VR_Thrown_Mass(thing), cvar("vr_decap_poptest_body") ? "body" : "head", m.classname));
        bprint(sprintf("%s, headless %g, gibbed %g, health %g\n", (vr_decap_pops > pops0) ? "popped" : "not popped",
            m.vr_headless, m.vr_gib, m.health));
        remove(thing);
    }
    else if(xTest >= 41 && xTest <= 44)
    {
        float weapon = (xTest == 41) ? QVR_DECAP_SHOTGUN : (xTest == 43) ? QVR_DECAP_LIGHTNING : QVR_DECAP_SSG;
        m.health = 1;
        float pops = vr_decap_pops;
        float rolls = vr_decap_rolls;
        VR_Decap_TestShot(weapon, aim);
        // (sprintf takes at most 7 values: QC builtins take 8 parameters)
        bprint(sprintf("poptest: %g %s at the %s of %s from %.0f units (%.1f lengths): ", xTest, VR_Decap_HowName(weapon),
            (xTest == 44) ? "body" : "head", m.classname, range, range / VR_DECAP_PLAYER_LENGTH));
        bprint(sprintf("%s, rolled %g, headless %g, gibbed %g, health %g\n", (vr_decap_pops > pops) ? "popped" : "not popped",
            vr_decap_rolls - rolls, m.vr_headless, m.vr_gib, m.health));
    }
    else if(xTest == 45 || xTest == 46)
    {
        float wpn = (xTest == 46) ? QVR_DECAP_SSG : QVR_DECAP_SHOTGUN;
        float n = max(1, cvar("vr_decap_poptest_n"));
        for(float b = 0; b < 15; ++b)
        {
            vr_poptest_bn[b] = 0;
            vr_poptest_bpop[b] = 0;
            vr_poptest_bsum[b] = 0;
        }
        float armed = 0, popped = 0, sum = 0;
        vr_poptest_spread = TRUE;
        vector morg = m.origin, mang = m.angles;
        float mframe = m.frame;
        for(float t = 0; t < n; ++t)
        {
            // (Each blast at it as it stood: the blasts before knocked it back, turned it, played its pain.)
            setorigin(m, morg);
            m.angles = mang;
            m.frame = mframe;
            m.velocity = '0 0 0';
            m.health = 100000;
            vr_decap_ent = world;
            vr_decap_time = -1;
            VR_Decap_TestShot(wpn, at);
            float hp = (m.vr_decap_bserial == vr_sgib_blastserial) ? m.vr_decap_bn : 0;
            if(vr_decap_time != time)
            {
                continue; // (not armed: no headshot, under the Head Share)
            }
            vr_decap_ent = world; // (not killed: VR_Decap_After let it go; the chance it would have rolled kept)
            float c = vr_decap_chance;
            float won = VR_Decap_Roll(m); // (its roll: random, vr_decap_pop_roll; Quad Damage pops all)
            armed++;
            sum += c;
            popped += won;
            hp = min(14, hp);
            vr_poptest_bn[hp] += 1;
            vr_poptest_bpop[hp] += won;
            vr_poptest_bsum[hp] += c;
        }
        vr_poptest_spread = FALSE;
        m.health = 100;
        bprint(sprintf("poptest: %g %s at %s's head from %.0f units (%.1f lengths): ", xTest, VR_Decap_HowName(wpn),
            m.classname, range, range / VR_DECAP_PLAYER_LENGTH));
        bprint(sprintf("%g blasts, %g headshots, mean chance %.3f, popped %g (rate %.3f)\n", n, armed, sum / max(1, armed),
            popped, popped / max(1, armed)));
        string byp = "poptest: by pellets at the head (n chance rate):";
        for(float q = 0; q < 15; ++q)
        {
            if(vr_poptest_bn[q] > 0)
            {
                byp = strcat(byp, sprintf(" %g:%g %.2f %.2f", q, vr_poptest_bn[q], vr_poptest_bsum[q] / vr_poptest_bn[q],
                    vr_poptest_bpop[q] / vr_poptest_bn[q]));
            }
        }
        bprint(strcat(byp, "\n"));
    }
    setorigin(self, old);
};

// The made-up blow of blunt class `xClass` at level `xLevel` (0 soft, 1 medium, 2 hard) at `xAt`: its damage (before
// the headshot multiplier).
float(vector xAt, float xClass, float xLevel) VR_Decap_TestBluntBlow =
{
    float thing = VR_MTHING_FIST, kind = VR_MKIND_PUNCH, f = 0.7, base = cvar_hget(cvarh_vr_dmg_fist);
    if(xClass == VR_DECAP_BCLASS_GUN) { thing = VR_MTHING_GUN; kind = VR_MKIND_SLASH; base = cvar_hget(cvarh_vr_dmg_gun_bash); }
    else if(xClass == VR_DECAP_BCLASS_CROWBAR) { thing = VR_MTHING_CROWBAR; kind = VR_MKIND_SLASH; base = cvar_hget(cvarh_vr_crowbar_damage); }
    else if(xClass == VR_DECAP_BCLASS_POMMEL) { thing = VR_MTHING_SWORD; kind = VR_MKIND_POMMEL; f = -0.15; base = 20; }
    else if(xClass == VR_DECAP_BCLASS_CLUB) { thing = VR_MTHING_CLUB; kind = VR_MKIND_SLASH; base = cvar_hget(cvarh_vr_walltorch_damage); }
    else if(xClass == VR_DECAP_BCLASS_MJOLNIR) { thing = VR_MTHING_AXE; kind = VR_MKIND_SLASH; base = cvar_hget(cvarh_vr_dmg_mjolnir); }
    VR_Decap_TestBlow(xAt, thing, kind, f, 0, VR_DECAP_TEST_SPEED[xLevel]);
    if(xClass == VR_DECAP_BCLASS_MJOLNIR)
    {
        VR_Blow(cVR_MainHand).mh_wid = WID_MJOLNIR;
    }
    return base * VR_DECAP_TEST_STRENGTH[xLevel];
};

void(float xTest) VR_Decap_BluntTest =
{
    entity m = VR_Decap_TestTarget(TRUE);
    if(m == world)
    {
        bprint(sprintf("poptest: %g: no live monster near\n", xTest));
        return;
    }
    VR_Decap_HeadZone(m);
    makevectors('0 1 0' * m.angles_y);
    vector at = m.origin + v_forward * xHeadForward + VR_HeadSide(v_forward) + '0 0 1' * xHeadUp;
    vector dir = normalize(at - (self.origin + self.view_ofs));
    float quad = VR_Decap_QuadPops(self);
    m.frame = 0; // (standing: the head zone's middle is its head's, not where a pain or run frame put it)
    if(xTest == 49)
    {
        float n = max(1, cvar("vr_decap_poptest_n"));
        float oldHealth = m.health;
        float hm = max(1, cvar_hget(cvarh_vr_headshot_mult));
        for(float cls = 0; cls < 6; ++cls)
        {
            string line = sprintf("poptest: 49 %s on %s (quad %g):", VR_Decap_BluntName(cls), m.classname, quad);
            for(float lvl = 0; lvl < 3; ++lvl)
            {
                // (The blow armed once, its chance rolled n times: VR_Decap_Roll's roll.)
                float armed = 0, popped = 0, c = 0;
                float dmg = VR_Decap_TestBluntBlow(at, cls, lvl) * hm;
                m.health = 100000;
                vr_decap_ent = world;
                vr_decap_time = -1;
                VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
                float yes = VR_Decap_Blow(m, self, dmg);
                VR_ClearHitKind();
                vr_decap_ent = world;
                c = vr_decap_chance;
                for(float t = 0; yes && t < n; ++t)
                {
                    vr_decap_chance = c;
                    armed++;
                    popped += VR_Decap_Roll(m);
                }
                line = strcat(line, sprintf(" %s %.0fm/s %.0fdmg chance %.4f armed %g popped %g rate %.4f;",
                    (lvl == 0) ? "soft" : (lvl == 1) ? "medium" : "hard", VR_DECAP_TEST_SPEED[lvl], dmg, c, armed, popped,
                    popped / max(1, armed)));
            }
            bprint(strcat(line, "\n"));
        }
        m.health = oldHealth;
        return;
    }
    float player = self.takedamage;
    float pops = vr_decap_pops;
    float rolls = vr_decap_rolls;
    string what;
    m.health = (m.classname == "monster_zombie") ? 60 : 1;
    if(xTest >= 50 && xTest <= 55)
    {
        float k = xTest - 50;
        float d = VR_Decap_TestBluntBlow(at, k, 2);
        what = sprintf("a hard %s blow (%.0f damage, chance %.4f)", VR_Decap_BluntName(k), d,
            VR_Decap_BluntChance(k, VR_DECAP_TEST_SPEED[2], d * max(1, cvar_hget(cvarh_vr_headshot_mult))));
        if(m.classname == "monster_zombie") { d = max(d, 30); }
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
        T_Damage_VRMelee(m, self, self, d);
        VR_ClearHitKind();
    }
    else
    {
        entity p = spawn();
        p.owner = self;
        p.movetype = MOVETYPE_FLYMISSILE;
        p.solid = SOLID_BBOX;
        p.angles = vectoangles(dir);
        p.velocity = dir * ((xTest == 58) ? 600 : 1000);
        setorigin(p, at - dir * 6);
        entity old = self;
        self.takedamage = DAMAGE_NO; // (the blast spares you)
        self = p;
        other = m;
        if(xTest == 56) { what = "a nail"; p.classname = "spike"; spike_touch(); }
        else if(xTest == 57) { what = "a rocket"; p.classname = "missile"; T_MissileTouch(); }
        else { what = "a grenade"; p.classname = "grenade"; GrenadeTouch(); }
        self = old;
        self.takedamage = player;
    }
    bprint(sprintf("poptest: %g %s at the head of %s (quad %g): %s, rolled %g", xTest, what, m.classname, quad,
        (vr_decap_pops > pops) ? "popped" : "not popped", vr_decap_rolls - rolls));
    bprint(sprintf(", headless %g, gibbed %g, health %g\n", m.vr_headless, m.vr_gib, m.health));
};

void(float xTest) VR_Decap_Test =
{
    if(xTest >= 49 && xTest <= 58)
    {
        VR_Decap_BluntTest(xTest);
        return;
    }
    if(xTest >= 40 && xTest <= 48)
    {
        VR_Decap_PopTest(xTest);
        return;
    }
    if(xTest == 32)
    {
        VR_Gore_Print(sprintf("gremlingrabtest: held=%g\n", ragdollheld(self, cVR_MainHand)));
        return;
    }
    if(xTest == 30)
    {
        entity small = spawn(); small.vr_rigid = 1; small.classname = "vr_smallgib"; small.vr_sgib = time;
        float rejected = !VR_Button_Thrown(small);
        small.classname = "gib"; small.vr_sgib = 0;
        VR_Gore_Print(sprintf("buttongibtest: small_rejected=%g large_allowed=%g\n", rejected, VR_Button_Thrown(small)));
        remove(small);
        return;
    }
    if(xTest == 31)
    {
        entity g = find(world, classname, "monster_gremlin");
        if(g != world && g.health <= 0)
        {
            entity player = self; self = g;
            self.solid = SOLID_NOT;
            gremlin_flip8();
            VR_Corpse_Watch(g);
            self = player;
        }
        return;
    }
    float heads = VR_Decap_TestHeads();
    float before = vr_decap_count;
    float pops = vr_decap_pops;
    entity m = VR_Decap_TestTarget((xTest != 5 && xTest != 8 && xTest != 10 && (xTest < 20 || xTest >= 24)));
    if(xTest == 33 && m != world)
    {
        LightningDamage(self.origin + self.view_ofs, m.origin + 0.5 * (m.mins + m.maxs), self, 1);
        VR_Gore_Print(sprintf("bodyshocktest: target=%s health=%g corpse=%g\n", m.classname, m.health, m.vr_corpse));
        return;
    }
    if(xTest == 8)
    {
        // (A headless corpse first: gibbed, it throws no head.)
        for(entity hl = nextent(world); hl; hl = nextent(hl))
        {
            if(hl.vr_headless && !hl.vr_gib && hl.health <= 0 && vlen(hl.origin - self.origin) < 400)
            {
                m = hl;
            }
        }
    }
    if(m == world)
    {
        VR_Gore_Print(sprintf("decaptest: %g: no %s monster near\n", xTest, (xTest == 5 || xTest == 8 || xTest == 10) ? "dead" : "live"));
        return;
    }
    // Its head: the head zone's middle (a ragdoll: its head part's).
    vector at;
    if(isragdoll(m))
    {
        at = ragdollcut(m, 6);
    }
    else
    {
        VR_Decap_HeadZone(m);
        makevectors('0 1 0' * m.angles_y);
        at = m.origin + v_forward * xHeadForward + VR_HeadSide(v_forward) + '0 0 1' * xHeadUp;
    }
    if(xTest == 19)
    {
        VR_Decap_Sweep(m);
        return;
    }
    if(xTest == 11)
    {
        m.health = 1; // (no blow: the next one kills it, a thrown axe's)
        VR_Gore_Print(sprintf("decaptest: 11 %s at health 1\n", m.classname));
        return;
    }
    float dmg = 30;
    string what = "";
    if(xTest == 8)
    {
        what = "gibbed";
        T_Damage(m, self, self, 1000);
    }
    else if(xTest == 5)
    {
        what = "a slash at a corpse's head";
        VR_Decap_TestBlow(at, VR_MTHING_SWORD, VR_MKIND_SLASH, 0.7, 0, 8);
        if(!VR_Decap_CorpseBlow(m, cVR_MainHand, at))
        {
            VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
            T_Damage(m, self, self, dmg);
            VR_ClearHitKind();
        }
    }
    else if(xTest == 6 || xTest == 10)
    {
        what = (xTest == 6) ? "the chainsaw" : "the chainsaw at a corpse's head";
        if(xTest == 6)
        {
            m.health = 1;
        }
        VR_Decap_TestBlow(at, VR_MTHING_SAW, VR_MKIND_SLASH, 0.8, 0, 0.5);
        float saw = VR_Decap_Saw(m, cVR_MainHand, at);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        VR_DamageCredit_SetWeapon(self, WID_CHAINSAW);
        if(saw != 2)
        {
            T_DamageImpl(m, self, self, 8, 1);
        }
        VR_ClearHitKind();
        if(saw == 1)
        {
            VR_Decap_After(m);
        }
    }
    else if(xTest >= 20 && xTest <= 22)
    {
        what = "a headshot on a corpse";
        // Shoot from the exposed side of its head, rather than through the
        // body or an arm lying between the player's eyes and the head.
        vector playerOrigin = self.origin;
        vector away = at - m.origin;
        away_z = 0;
        setorigin(self, at + normalize(away) * 64 + '0 0 24' - self.view_ofs);
        VR_Decap_TestShot(xTest == 22 ? QVR_DECAP_LIGHTNING : xTest == 21 ? QVR_DECAP_SSG : QVR_DECAP_SHOTGUN, at);
        setorigin(self, playerOrigin);
    }
    else if(xTest >= 23 && xTest <= 27)
    {
        what = "a blunt head blow";
        if(xTest == 24 || xTest == 26 || xTest == 27) { m.health = 1; }
        if(xTest == 25) { m.health = 500; }
        if(xTest == 26) { at = m.origin; what = "a blunt body blow"; }
        VR_Decap_TestBlow(at, VR_MTHING_FIST, VR_MKIND_POMMEL, 0.7, 0, 8);
        if(xTest == 27)
        {
            entity b = VR_Blow(cVR_MainHand);
            b.mh_thing = VR_MTHING_AXE;
            b.mh_kind = VR_MKIND_SLASH;
            b.mh_wid = WID_MJOLNIR;
            what = "Mjolnir's head blow";
        }
        if(xTest == 23)
        {
            if(!VR_Decap_CorpseBlow(m, cVR_MainHand, at)) { T_Damage(m, self, self, dmg); }
        }
        else
        {
            VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
            T_Damage_VRMelee(m, self, self, dmg);
            VR_ClearHitKind();
        }
    }
    else if(xTest >= 12 && xTest <= 18)
    {
        // Head pops: a shot at its head (or its body) from your eye.
        float weapon = QVR_DECAP_SHOTGUN;
        vector aim = at;
        if(xTest == 12) { what = "a shotgun headshot"; m.health = VR_Decap_TestBlastDamage(weapon) - 1; }
        else if(xTest == 13) { weapon = QVR_DECAP_SSG; what = "a super shotgun headshot"; m.health = VR_Decap_TestBlastDamage(weapon) - 1; }
        else if(xTest == 14) { weapon = QVR_DECAP_SSG; what = "a super shotgun headshot, overkill"; m.health = 1; }
        else if(xTest == 15) { weapon = QVR_DECAP_LIGHTNING; what = "a lightning bolt at the head"; m.health = 1; }
        else if(xTest == 16) { what = "a shotgun body shot"; m.health = 1; aim = m.origin + '0 0 4'; }
        else if(xTest == 17) { what = "a shotgun headshot not killing"; m.health = max(m.health, 500); }
        else { weapon = QVR_DECAP_LIGHTNING; what = "a lightning bolt at the body"; m.health = 1; aim = m.origin + '0 0 4'; }
        if(m.classname == "monster_zombie")
        {
            m.health = 60; // (a zombie: its own health, as it always is)
        }
        VR_Gore_Print(sprintf("decaptest: %g at health %g\n", xTest, m.health));
        VR_Decap_TestShot(weapon, aim);
    }
    else
    {
        float thing = (xTest == 9 || xTest == 61) ? VR_MTHING_AXE : VR_MTHING_SWORD;
        float kind = VR_MKIND_SLASH;
        float f = 0.7;
        float along = 0;
        float speed = 8;
        if(xTest == 2) { what = "a stab"; along = 1; }
        else if(xTest == 3) { what = "a pommel strike"; f = -0.15; kind = VR_MKIND_POMMEL; }
        else if(xTest == 4) { what = "a slow slash"; speed = max(0.2, cvar_hget(cvarh_vr_decap_speed) * 0.5); }
        else if(xTest == 7) { what = "a slash at full health"; dmg = 10; }
        else if(xTest == 61) { what = "an axe's slash swept through the neck"; f = 0.85; }
        else { what = (xTest == 9) ? "an axe's slash" : "a sword's slash"; f = (xTest == 9) ? 0.85 : 0.7; }
        if(xTest != 7)
        {
            m.health = 1;
        }
        VR_Decap_TestBlow(at, thing, kind, f, along, speed);
        if(xTest == 61)
        {
            // (As a hand's blow finds what it strikes: VR_Melee_Sweep. Its contact is the blow's.)
            float struck = VR_Decap_TestSweep(m, at);
            VR_Gore_Print(sprintf("decaptest: 61 the sweep %s %s%s at %v\n", struck ? "strikes" : "misses", m.classname,
                m.vr_knockdown == 1 ? " (knocked down)" : "", VR_Blow(cVR_MainHand).mh_c_pos));
            if(!struck)
            {
                m = world;
            }
            else
            {
                at = VR_Blow(cVR_MainHand).mh_c_pos;
            }
        }
        if(m != world)
        {
            VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
            T_Damage_VRMelee(m, self, self, dmg);
            VR_ClearHitKind();
        }
    }
    float headsNow = VR_Decap_TestHeads(); // (not in the call's arguments: a call there overwrites the ones before it)
    float rag = isragdoll(m);
    // (sprintf takes at most 7 values: QC builtins take 8 parameters)
    VR_Gore_Print(sprintf("decaptest: %g %s on %s: %s, health %g, headless %g", xTest, what, m.classname,
        (vr_decap_count > before) ? "beheaded" : "not beheaded", m.health, m.vr_headless));
    VR_Gore_Print(sprintf(", ragdoll %g, heads %g -> %g, popped %g, gibbed %g\n", rag, heads, headsNow, vr_decap_pops - pops,
        m.vr_gib));
    if(vr_decap_count > before && vr_decap_lasthead != world)
    {
        entity h = vr_decap_lasthead;
        vector neck = ragdollcut(m, 4);
        VR_Gore_Print(sprintf("decaptest: head %s at %v (the neck %v), %.0f u/s %v", h.model, h.origin, neck, vlen(h.velocity),
            h.velocity));
        VR_Gore_Print(sprintf(", spin %.1f rad/s %v (gib %g, rigid %g)\n", vlen(h.vr_spin), h.vr_spin, h.vr_gib, h.vr_rigid));
    }
};

// Every server frame (StartFrame): Show Hit Zones (VR_Decap_DebugFrame), a test asked for (vr_decap_test).
void() VR_Decap_TestFrame =
{
    VR_Decap_DebugFrame();
    float ask = cvar("vr_decap_test");
    if(!ask)
    {
        return;
    }
    cvar_set("vr_decap_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    if(cvar("vr_gore_test_crowd") > 0)
    {
        VR_Gore_TestCrowd(ask, TRUE);
        self = oself;
        return;
    }
    VR_Decap_Test(ask);
    self = oself;
};
#endif
