// vr_smallgibs_test.qc -- the small gibs' tests (docs/vr-port/ROUND21.md, "Small gibs"): `vr_smallgibs_test <n>` runs
// test n on the monster or corpse nearest the player (impulse 241 puts one ahead) and prints `sgibtest:` lines
// (Debug > Gore Tests). N of each: vr_smallgibs_test_n (default 200); a blow's damage: vr_smallgibs_test_dmg (35).
//   1 shotgun blasts (6 pellets, its damage and spread)    2 super shotgun blasts (14)
//   3 nails and super nails                                 4 an axe's blade, its pommel, a punch
//   5 nails under Quad Damage                               6 the chainsaw's chain for a second (a corpse: alive too)
//   7 a large gib and a head burst                          8 Most Small Gibs 10, 15 made: the oldest go
//   9 one in the off hand at Small Gibs Last 1 (the script holds the grip 3 s: vr_mock_button off grip 1, then 0):
//     there after 3 s, gone 1 s after it lands (and the fade)
//  10 the grace: one from just behind the monster through it, with the grace and without
//  11 the ones lying about: how many, their size, rigid, asleep, stuck, held
//  12 thrown into the nearest wall: a gib and a small gib at 220 u/s stick (Thrown Gibs Stick 1), a gib at 400 bursts
//  13 flight by situation (Flight by Situation): for each in turn (melee, chainsaw, guns, explosions, thrown, a
//     monster's blow, gibbed by a blow, gibbed by a rocket (a corpse: a corpse's gibbing), a large gib burst) some are
//     torn out (to the side: not at you), their launch speeds out and up, and 2.5 s later how far they lie from where
//     they were made. The gibbings also print the old flight (the gibs' at 0.6, no Speed or Up)
//     (vr_smallgibs_test_hand 1: the melee and chainsaw ones at the main hand, put in the body's middle: the mock)
//  14 a crowd blown up among explosive boxes (a stress of the unreliable broadcast's room: ROUND21.md, "A full broadcast
//     drops whole messages"): vr_smallgibs_test_crowd grunts (24) and half as many boxes in rings round a point 160
//     units ahead, a rocket's blast there 1 s on (the boxes go off in a chain), what is left 3 s on (vr_debug_net 1
//     prints each frame the broadcast was full); vr_smallgibs_test_blasts: that many blasts in that frame (round the point)
//  15 not a test: to the wall ahead, vr_smallgibs_test_dist units from it (64), to throw gibs at it
//  16 the monster or corpse put under your feet and gibbed by a blow; 2 s on, the health you and the monsters within
//     160 units lost, and the gibs' flung hits and those spared as just made (vr_gib_spawn_harmless; god off)
//  17 the same, gibbed by a shot (a super shotgun blast's damage at your feet)
//  18 a gib made at your feet, thrown by you (from a step ahead) at the monster nearest 1 s later (vr_test_throw_up_speed, 10 m/s; Fresh
//     Gibs Harmless For is over): 2.5 s on, the health it lost
//  19 as 16, but killed by the blow, not gibbed (a live monster: its death, its gun and backpack dropped under you)
//  20 a monster's backpack dropped 2 x vr_smallgibs_test_dist units over you: past Monster Drops Harmless For it falls on
//     you and hurts (Hurt Only Falling); 3 s on, the health you lost
//  (16, 17 and 19 print the monster drops' flung hits on you too, and those spared: Monster Drops Harmless For, Monster
//  Drops Hurt Only Falling)
// With vr_smallgibs_test_dummy 1 their target is the nearest training dummy instead (vr_dummy_gore: it bleeds as a grunt);
// 1, 2, 3, 4 and 6 print the gore each sent too (gore hits, wound events, blood particles), to compare with a grunt's.

float vr_sgib_test;       // the test running, its step, when the step began
float vr_sgib_test_step;
float vr_sgib_test_t;
entity vr_sgib_test_m;    // its monster or corpse
entity vr_sgib_test_g;    // its small (or large) gib
entity vr_sgib_test_e;    // a third (test 12)
float vr_sgib_test_id;
float vr_sgib_test_a;     // what it counts
float vr_sgib_test_b;

entity(entity p) VR_SmallGib_TestTarget =
{
    entity best = world;
    float bestd = 0;
    entity e = findradius(p.origin, 800);
    while(e)
    {
        float ok = cvar("vr_smallgibs_test_dummy") ? e.classname == "vr_dummy" :
                   (e.classname != "vr_dummy" && ((e.flags & FL_MONSTER) || e.vr_corpse == 2));
        if(e != p && ok && !e.vr_gib)
        {
            float d = vlen(e.origin - p.origin);
            if(best == world || d < bestd)
            {
                best = e;
                bestd = d;
            }
        }
        e = e.chain;
    }
    return best;
};

// The gore sent since VR_SmallGib_TestGoreMark (the dummy's to compare with a grunt's: vr_smallgibs_test_dummy).
float vr_sgib_test_gore;
float vr_sgib_test_wound;
float vr_sgib_test_blood;

void() VR_SmallGib_TestGoreMark =
{
    vr_sgib_test_gore = vr_gore_sent;
    vr_sgib_test_wound = vr_wound_sent;
    vr_sgib_test_blood = vr_blood_sent;
};

void(string xName) VR_SmallGib_TestGorePrint =
{
    bprint(sprintf("sgibtest: %s: gore hits %d, wound events %d, blood particles %d\n", xName, vr_gore_sent - vr_sgib_test_gore,
        vr_wound_sent - vr_sgib_test_wound, vr_blood_sent - vr_sgib_test_blood));
};

// `n` blasts of `xPellets` at `m`, from the player's eyes.
void(entity m, float xPellets, float xDmg, vector xSpread, float n, string xName) VR_SmallGib_TestBlasts =
{
    float withGibs = 0;
    float made = vr_sgib_made;
    vector src = self.origin + '0 0 22';
    m.health = 100000;
    vr_sgib_rolls = 0;
    vr_sgib_rolldmg = 0;
    VR_SmallGib_TestGoreMark();
    for(float i = 0; i < n; i++)
    {
        vector dir = normalize((m.absmin + m.absmax) * 0.5 - src);
        float before = vr_sgib_made;
        VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
        vr_pellet_damage = xDmg;
        FireBulletsImpl(xPellets, dir, xSpread, src, 2048, TRUE);
        vr_pellet_damage = 0;
        VR_ClearHitKind();
        if(vr_sgib_made > before)
        {
            withGibs++;
        }
    }
    bprint(sprintf("sgibtest: %s: %d blasts at %s %.0f units away: %d hits weighed, %.1f damage each\n", xName, n, m.classname,
        vlen(m.origin - self.origin), vr_sgib_rolls, vr_sgib_rolldmg / max(1, vr_sgib_rolls)));
    bprint(sprintf("sgibtest: %s: %d tore small gibs out (%.0f%%), %d small gibs\n", xName, withGibs,
        100 * withGibs / max(1, n), vr_sgib_made - made));
    VR_SmallGib_TestGorePrint(xName);
};

// `n` blows of `xDmg` on `m` by `inflictor` (the player's hand `xKind` with weapon `xWeapon`, `xBlunt` its pommel), the
// rate that tore any out.
void(entity m, entity inflictor, float xKind, float xWeapon, float xBlunt, float xDmg, float n, string xName) VR_SmallGib_TestBlows =
{
    float withGibs = 0;
    float made = vr_sgib_made;
    float oldWeapon = self.weapon;
    vector mid = (m.absmin + m.absmax) * 0.5;
    m.health = 100000;
    if(m.vr_corpse == 2)
    {
        m.vr_corpse_hp = 100000;
    }
    VR_SmallGib_TestGoreMark();
    for(float i = 0; i < n; i++)
    {
        float before = vr_sgib_made;
        self.weapon = xWeapon;
        VR_SetHitKind(xKind, xKind == QVR_HITKIND_MELEE ? cVR_MainHand : -1, mid, normalize(mid - self.origin));
        vr_melee_blunt = xBlunt;
        T_Damage(m, inflictor, self, xDmg);
        vr_melee_blunt = FALSE;
        VR_ClearHitKind();
        self.weapon = oldWeapon;
        if(vr_sgib_made > before)
        {
            withGibs++;
        }
    }
    bprint(sprintf("sgibtest: %s: %d of %.0f on %s, %d tore small gibs out (%.0f%%), %d small gibs\n", xName, n, xDmg,
        m.classname, withGibs, 100 * withGibs / max(1, n), vr_sgib_made - made));
    VR_SmallGib_TestGorePrint(xName);
};

float(entity g) VR_SmallGib_TestInBox =
{
    entity m = vr_sgib_test_m;
    vector o = (g.absmin + g.absmax) * 0.5;
    return o_x > m.absmin_x && o_x < m.absmax_x && o_y > m.absmin_y && o_y < m.absmax_y && o_z > m.absmin_z && o_z < m.absmax_z;
};

// Test 13: what step `xStep` (1..9) tears out (QVR_SGIB_SIT_*).
float(entity m, float xStep) VR_SmallGib_TestSpeedSit =
{
    if(xStep == 1) { return QVR_SGIB_SIT_MELEE; }
    if(xStep == 2) { return QVR_SGIB_SIT_SAW; }
    if(xStep == 3) { return QVR_SGIB_SIT_GUNS; }
    if(xStep == 4) { return QVR_SGIB_SIT_EXPLOSIONS; }
    if(xStep == 5) { return QVR_SGIB_SIT_THROWN; }
    if(xStep == 6) { return QVR_SGIB_SIT_OTHER; }
    if(xStep == 9) { return QVR_SGIB_SIT_BURST; }
    return m.vr_corpse == 2 ? QVR_SGIB_SIT_CORPSE : QVR_SGIB_SIT_GIBBED;
};

string(float xStep) VR_SmallGib_TestSpeedName =
{
    if(xStep == 1) { return "melee (a sword's blade, 35)"; }
    if(xStep == 2) { return "chainsaw"; }
    if(xStep == 3) { return "guns (a super shotgun blast, 42)"; }
    if(xStep == 4) { return "explosions (a rocket's blast, 60)"; }
    if(xStep == 5) { return "thrown (an axe, 40)"; }
    if(xStep == 6) { return "other (a monster's blow, 20)"; }
    if(xStep == 7) { return "gibbed by a blow"; }
    if(xStep == 8) { return "gibbed by a rocket"; }
    return "a large gib burst";
};

// Test 13's step `xStep`: some torn out of `m` (self is the player), to its side.
void(entity m, float xStep) VR_SmallGib_TestSpeedLaunch =
{
    float sit = VR_SmallGib_TestSpeedSit(m, xStep);
    vr_sgib_st_n[sit] = 0;
    vr_sgib_st_h[sit] = 0;
    vr_sgib_st_v[sit] = 0;
    vector mid = (m.absmin + m.absmax) * 0.5;
    vector fwd = normalize(mid - self.origin);
    fwd_z = 0;
    fwd = normalize(fwd);
    vector side = '1 0 0' * fwd_y - '0 1 0' * fwd_x; // to the side: a blow from there flings them back
    m.health = 100000;
    if(m.vr_corpse == 2)
    {
        m.vr_corpse_hp = 100000;
    }
    float oldWeapon = self.weapon;
    float oldQuad = self.super_damage_finished;
    self.super_damage_finished = time + 30; // every hit tears them out
    if(xStep == 1 && cvar("vr_smallgibs_test_hand"))
    {
        bprint(sprintf("sgibtest: speeds: the main hand %.0f units from its middle\n", vlen(VRGetHandPos(cVR_MainHand) - mid)));
    }
    entity e = world;
    if(xStep == 4 || xStep == 5 || xStep == 6 || xStep == 8)
    {
        e = spawn();
        e.classname = (xStep == 4 || xStep == 8) ? "missile" : (xStep == 5 ? "thrown_weapon" : "monster_test");
        e.weapon = WID_AXE;
        e.super_damage_finished = time + 30;
        setorigin(e, mid + side * (m.maxs_x + 8));
        e.velocity = side * -1000;
    }
    if(xStep == 7 || xStep == 8)
    {
        // A gibbing (its gibs' push: from the player, or a rocket flying at it), and the old flight sampled.
        float dm = xStep == 7 ? -40 : -100;
        float oldGibSpeed = vr_gib_speed;
        vr_gib_speed = cvar_hget(xStep == 7 ? cvarh_vr_gib_speed_melee : cvarh_vr_gib_speed_heavy);
        damage_inflictor = xStep == 7 ? self : e;
        float oh = 0, ov = 0;
        entity p = self;
        self = m;
        for(float j = 0; j < 100; j++)
        {
            vector ovel = VR_Gore_Fling(VelocityForDamage(dm)) * 0.6;
            ov = ov + ovel_z;
            ovel_z = 0;
            oh = oh + vlen(ovel);
        }
        vr_sgib_corpsegib = m.vr_corpse == 2;
        for(float k = 0; k < 3; k++)
        {
            VR_SmallGib_Gibbed(dm);
        }
        vr_sgib_corpsegib = FALSE;
        self = p;
        vr_gib_speed = oldGibSpeed;
        bprint(sprintf("sgibtest: speeds: %s: old: out %.0f u/s, up %.0f u/s\n",
            VR_SmallGib_TestSpeedName(xStep), oh / 100, ov / 100));
    }
    else if(xStep == 9)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        for(float b = 0; b < 4; b++)
        {
            VR_SmallGib_Burst(self.origin + v_forward * 48 + side * (b * 8 - 12) - '0 0 16', side, 1);
        }
    }
    else
    {
        for(float i = 0; i < 8; i++)
        {
            m.vr_sgib_sawtime = -100;
            VR_ClearHitKind();
            if(xStep == 1 || xStep == 2)
            {
                self.weapon = xStep == 1 ? WID_SWORD : WID_CHAINSAW;
                VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, mid, -side);
                if(cvar("vr_smallgibs_test_hand"))
                {
                    // Torn out at the main hand (vr_smallgibs_test_hand 1: put in the body), in its reach body, as at a
                    // blade's or the chain's contact.
                    VR_SmallGib_Tear(m, VRGetHandPos(cVR_MainHand), -side, 1, VR_SmallGib_TestSpeedSit(m, xStep));
                }
                else
                {
                    VR_SmallGib_Hit(m, self, self, 35);
                }
            }
            else if(xStep == 3)
            {
                VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, mid, -side);
                VR_SmallGib_Hit(m, self, self, 42);
            }
            else if(xStep == 4)
            {
                VR_SetHitKind(QVR_HITKIND_SPLASH, -1, '0 0 0', -side);
                VR_SmallGib_Hit(m, e, self, 60);
            }
            else if(xStep == 5)
            {
                VR_SmallGib_Hit(m, e, self, 40);
            }
            else
            {
                VR_SmallGib_Hit(m, e, e, 20);
            }
            VR_ClearHitKind();
        }
    }
    if(e != world)
    {
        remove(e);
    }
    self.weapon = oldWeapon;
    self.super_damage_finished = oldQuad;
};

// Test 13's step `xStep` 2.5 s on: its launches, and how far they lie from where they were made (then gone).
void(entity m, float xStep) VR_SmallGib_TestSpeedLanded =
{
    float sit = VR_SmallGib_TestSpeedSit(m, xStep);
    float m2u = VRMetersToUnits();
    float n = max(1, vr_sgib_st_n[sit]);
    float h = vr_sgib_st_h[sit] / n;
    float v = vr_sgib_st_v[sit] / n;
    float cnt = 0, dist = 0, most = 0, pk = 0, pkt = 0, pkmost = 0;
    entity g = find(world, classname, "vr_smallgib");
    while(g)
    {
        entity next = find(g, classname, "vr_smallgib");
        if(g.vr_sgib_sit == sit && g.vr_sgib_id)
        {
            vector d = g.origin - g.vr_sgib_org;
            d_z = 0;
            cnt++;
            dist = dist + vlen(d);
            most = max(most, vlen(d));
            pk = pk + g.vr_sgib_peak;
            pkt = pkt + g.vr_sgib_peakt;
            pkmost = max(pkmost, g.vr_sgib_peak);
            VR_SmallGib_Remove(g);
        }
        g = next;
    }
    bprint(sprintf("sgibtest: speeds: %s [%s %.2fx, up %.2fx]\n", VR_SmallGib_TestSpeedName(xStep),
        VR_SmallGib_SitName(sit), VR_SmallGib_SitMult(sit, FALSE), VR_SmallGib_SitMult(sit, TRUE)));
    bprint(sprintf("sgibtest: speeds:   %d: out %.0f u/s %.1f m/s, up %.0f u/s %.1f m/s; lie %.2f m, max %.2f m\n",
        vr_sgib_st_n[sit], h, h / m2u, v, v / m2u, dist / max(1, cnt) / m2u, most / m2u));
    bprint(sprintf("sgibtest: speeds:   flew across at %.0f u/s (most %.0f), fastest %.2f s after it was made\n",
        pk / max(1, cnt), pkmost, pkt / max(1, cnt)));
};

// Tests 16, 17: the health of the live monsters within 160 units of `p` but `m`, summed.
float(entity p, entity m) VR_SmallGib_TestNearHealth =
{
    float sum = 0;
    entity e = findradius(p.origin, 160);
    while(e)
    {
        if(e != m && e != p && (e.flags & FL_MONSTER) && e.health > 0)
        {
            sum = sum + e.health;
        }
        e = e.chain;
    }
    return sum;
};

void(float xCase) VR_SmallGib_TestStart =
{
    vr_sgib_test = xCase;
    vr_sgib_test_step = 0;
    vr_sgib_test_t = time;
    vr_sgib_test_m = VR_SmallGib_TestTarget(self);
    float n = cvar("vr_smallgibs_test_n");
    n = n > 0 ? n : 200;
    float dmg = cvar("vr_smallgibs_test_dmg");
    dmg = dmg > 0 ? dmg : 35;
    entity m = vr_sgib_test_m;
    if(xCase == 15)
    {
        // To the wall ahead (the view's yaw), vr_smallgibs_test_dist units from it (default 64): gibs thrown from there
        // strike it (sticking tests: Misc/quakevr/gib_stick_test.sh).
        float wd = cvar("vr_smallgibs_test_dist");
        wd = wd > 0 ? wd : 64;
        makevectors('0 1 0' * self.v_viewangle_y);
        vector weye = self.origin + '0 0 16';
        traceline(weye, weye + v_forward * 2048, TRUE, self);
        float wfrac = trace_fraction;
        vector wto = self.origin + v_forward * max(0, 2048 * trace_fraction - wd - 16);
        tracebox(self.origin + '0 0 2', self.mins, self.maxs, wto + '0 0 2', TRUE, self); // (off the floor)
        if(wfrac >= 1 || trace_startsolid)
        {
            bprint(sprintf("sgibtest: no wall ahead within 2048 units (%.2f %d, yaw %.0f)\n", wfrac, trace_startsolid, self.v_viewangle_y));
        }
        else
        {
            setorigin(self, trace_endpos);
            self.velocity = '0 0 0';
            traceline(self.origin + '0 0 16', self.origin + '0 0 16' + v_forward * 2048, TRUE, self);
            bprint(sprintf("sgibtest: at %v, the wall ahead %.0f units away (normal %v)\n", self.origin,
                2048 * trace_fraction, trace_plane_normal));
        }
        vr_sgib_test = 0;
        return;
    }
    if(xCase == 20)
    {
        // A monster's backpack (a stand-in with FL_MONSTER) dropped from over your head: its toss, then its fall on you.
        entity mon = spawn();
        setorigin(mon, self.origin + '0 0 1' * cvar("vr_smallgibs_test_dist") * 2);
        mon.flags = FL_MONSTER;
        mon.ammo_shells = 5;
        entity oself20 = self;
        self = mon;
        DropBackpack();
        self = oself20;
        remove(mon);
        vr_sgib_test_a = self.health;
        vr_fling_drophits = 0;
        vr_fling_dropspared = 0;
        bprint(sprintf("sgibtest: a monster's backpack dropped %.0f units over you\n", cvar("vr_smallgibs_test_dist") * 2));
        return;
    }
    if(xCase == 18 && m == world)
    {
        bprint("sgibtest: no monster near\n");
        vr_sgib_test = 0;
        return;
    }
    if(xCase == 18)
    {
        entity spot = spawn();
        setorigin(spot, self.origin);
        entity oself18 = self;
        self = spot;
        ThrowGib("progs/gib1.mdl", -10);
        self = oself18;
        vr_sgib_test_g = world;
        for(entity lg = nextent(world); lg != world; lg = nextent(lg))
        {
            if(lg.vr_gib && lg.vr_gib_made == time && lg.carry_player == world)
            {
                vr_sgib_test_g = lg;
            }
        }
        remove(spot);
        vr_sgib_test_a = m.health;
        bprint(sprintf("sgibtest: a gib made at your feet: %d\n", vr_sgib_test_g != world));
        return;
    }
    if(m == world && xCase != 8 && xCase != 9 && xCase != 11 && xCase != 12 && xCase != 14)
    {
        bprint("sgibtest: no monster or corpse near\n");
        vr_sgib_test = 0;
        return;
    }
    bprint(sprintf("sgibtest: test %d on %s (corpse %d)\n", xCase, m.classname, m.vr_corpse));
    if(xCase == 16 || xCase == 17 || xCase == 19)
    {
        // Under your feet, on the floor; gibbed by a blow, or by super shotgun blasts at your feet until it is.
        traceline(self.origin, self.origin - '0 0 256', TRUE, self);
        setorigin(m, trace_endpos - '0 0 1' * m.mins_z + '0 0 1');
        m.velocity = '0 0 0';
        vr_sgib_test_a = self.health;
        vr_sgib_test_b = VR_SmallGib_TestNearHealth(self, m);
        vr_fling_gibhits = 0;
        vr_fling_gibspared = 0;
        vr_fling_drophits = 0;
        vr_fling_dropspared = 0;
        vector feet = (m.absmin + m.absmax) * 0.5;
        vector from = self.origin + '0 0 22';
        if(xCase == 16 || xCase == 19)
        {
            VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, feet, normalize(feet - from));
            T_Damage(m, self, self, xCase == 19 ? max(1, m.health) : max(m.health, m.vr_corpse_hp) + 100);
            VR_ClearHitKind();
        }
        else
        {
            // (A super shotgun's blast's damage, as a shot from you: a lying corpse's thin model under your feet is
            // missed by straight-down pellets, so the hit is dealt as the blast's.)
            VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, feet, normalize(feet - from));
            T_Damage(m, self, self, max(m.health, m.vr_corpse_hp) + 100);
            VR_ClearHitKind();
        }
        bprint(sprintf("sgibtest: underfoot: gibbed %d, %d small gibs\n", m.vr_gib != 0, vr_sgib_made));
        return;
    }
    if(m.classname == "vr_dummy")
    {
        m.frame = 0; // (as a grunt's below; its think, which goes on, stands it again)
    }
    else if(m != world && m.vr_corpse != 2 && (m.flags & FL_MONSTER))
    {
        // Still, standing (its first frame): the same pose every run (a pose with the arms ahead takes the pellets
        // as limbs', 0.6 of their damage).
        m.think = SUB_Null;
        m.nextthink = -1;
        m.frame = 0;
        m.velocity = '0 0 0';
    }
    if(xCase == 1)
    {
        VR_SmallGib_TestBlasts(m, 6, cvar_hget(cvarh_vr_dmg_shotgun), '0.01 0.01 0', n, "shotgun");
    }
    else if(xCase == 2)
    {
        VR_SmallGib_TestBlasts(m, 14, cvar_hget(cvarh_vr_dmg_super_shotgun), '0.035 0.025 0', n, "super shotgun");
    }
    else if(xCase == 3 || xCase == 5)
    {
        if(xCase == 5)
        {
            n = ceil(n / 4); // (each makes 1 to 4: one frame's work)
        }
        entity spk = spawn();
        spk.classname = "spike";
        setorigin(spk, (m.absmin + m.absmax) * 0.5 + normalize(self.origin - m.origin) * 16);
        if(xCase == 5)
        {
            self.super_damage_finished = time + 30;
        }
        VR_SmallGib_TestBlows(m, spk, QVR_HITKIND_NONE, self.weapon, FALSE, cvar_hget(cvarh_vr_dmg_nail), n, xCase == 5 ? "nails, quad" : "nails");
        VR_SmallGib_TestBlows(m, spk, QVR_HITKIND_NONE, self.weapon, FALSE, cvar_hget(cvarh_vr_dmg_super_nail), n,
            xCase == 5 ? "super nails, quad" : "super nails");
        if(xCase == 5)
        {
            VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_FIST, FALSE, 8, n, "a light punch, quad");
            self.super_damage_finished = 0;
        }
        remove(spk);
    }
    else if(xCase == 4)
    {
        VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_AXE, FALSE, dmg, n, "axe blade");
        VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_AXE, TRUE, dmg, n, "axe pommel");
        VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_SWORD, FALSE, dmg, n, "sword blade");
        VR_SmallGib_TestBlows(m, self, QVR_HITKIND_MELEE, WID_FIST, FALSE, 25, n, "a strong punch");
    }
    else if(xCase == 6)
    {
        m.health = max(m.health, 100000);
        vr_sgib_test_a = vr_sgib_made;
        vr_sgib_test_b = 0;
        VR_SmallGib_TestGoreMark();
        if(m.vr_corpse == 2)
        {
            m.vr_corpse_hp = 100000;
        }
    }
    else if(xCase == 7)
    {
        makevectors('0 1 0' * self.v_viewangle_y);
        vector at = self.origin + v_forward * 48;
        vr_sgib_test_a = vr_sgib_made;
        entity oself = self;
        self = spawn();
        setorigin(self, at);
        ThrowGib("progs/gib2.mdl", -10);
        ThrowHead("progs/h_ogre.mdl", -10); // (the spawned thing itself made a head)
        vr_sgib_test_g = self;
        self = oself;
    }
    else if(xCase == 8)
    {
        cvar_set("vr_smallgibs_max", "10");
        vr_sgib_test_a = vr_sgib_serial + 1; // the first one's serial
        makevectors('0 1 0' * self.v_viewangle_y);
        for(float k = 0; k < 15; k++)
        {
            VR_SmallGib_Spawn(self.origin + v_forward * 64 + v_right * (k * 4 - 28) + '0 0 8', '0 0 0', world);
        }
        float lowest = 0;
        float firstFive = 0;
        for(float s = 0; s < 256; s++)
        {
            if(VR_SmallGib_SlotLive(s))
            {
                if(!lowest || vr_sgib_ringid[s] < lowest)
                {
                    lowest = vr_sgib_ringid[s];
                }
                if(vr_sgib_ringid[s] < vr_sgib_test_a + 5)
                {
                    firstFive++;
                }
            }
        }
        bprint(sprintf("sgibtest: cap: made 15 at most 10: %d lie about, the oldest #%d (the first was #%d), %d of the first five left\n",
            VR_SmallGib_Count(), lowest, vr_sgib_test_a, firstFive));
        cvar_set("vr_smallgibs_max", "40");
        vr_sgib_test = 0;
    }
    else if(xCase == 9)
    {
        cvar_set("vr_smallgibs_time", "1");
        entity g = VR_SmallGib_Spawn(VRGetHandPos(cVR_OffHand), '0 0 0', world);
        vr_sgib_test_g = g;
        vr_sgib_test_id = g.vr_sgib_id;
        if(g != world && VRIsHandEmpty(self, cVR_OffHand))
        {
            VR_Carry_Start(self, cVR_OffHand, g);
        }
        bprint(sprintf("sgibtest: held: #%d in the off hand %d\n", g.vr_sgib_id, g.carry_player == self));
    }
    else if(xCase == 12)
    {
        // Thrown into the nearest wall (of four ways round): a gib at 220 u/s and a small gib at 220 (stick: Thrown Gibs
        // Stick 1 for the test), a gib at 400 (faster than Gib Splat Speed: it bursts).
        float bestFrac = 2;
        vector wallDir = '0 0 0';
        vector eye = self.origin + '0 0 16';
        for(float yaw = 0; yaw < 360; yaw = yaw + 90)
        {
            makevectors('0 1 0' * yaw);
            traceline(eye, eye + v_forward * 400, TRUE, self);
            if(trace_fraction < bestFrac && !trace_startsolid)
            {
                bestFrac = trace_fraction;
                wallDir = v_forward;
            }
        }
        if(bestFrac >= 1)
        {
            bprint("sgibtest: no wall within 400 units\n");
            vr_sgib_test = 0;
            return;
        }
        cvar_set("vr_gore_stick_thrown", "1");
        vector wall = eye + wallDir * (400 * bestFrac);
        makevectors(vectoangles(wallDir));
        for(float w = 0; w < 3; w++)
        {
            vector from = wall - wallDir * 48 + v_right * (w * 24 - 24);
            entity tg;
            if(w == 1)
            {
                cvar_set("vr_smallgibs_grace", "0");
                tg = VR_SmallGib_Spawn(from, wallDir * 220, world);
                cvar_set("vr_smallgibs_grace", "0.15");
            }
            else
            {
                tg = spawn();
                setmodel(tg, "progs/gib1.mdl");
                setorigin(tg, from);
                tg.think = SUB_Remove;
                tg.nextthink = time + 30;
                VR_MakeGibGrabbable(tg);
                VR_Gib_MakeDestroyable(tg, 1);
                tg.vr_gib_armtime = time;
                tg.vr_rigid = 1;
                tg.velocity = wallDir * (w == 0 ? 220 : 400);
            }
            tg.movetype = MOVETYPE_TOSS;
            VR_Gib_Released(tg, vlen(tg.velocity), cVR_MainHand);
            if(w == 0) { vr_sgib_test_g = tg; }
            else if(w == 1) { vr_sgib_test_m = tg; }
            else { vr_sgib_test_e = tg; }
        }
        vr_sgib_test_t = time;
    }
    else if(xCase == 11)
    {
        // The ones lying about: how many, their size, mass, how many rigid bodies, asleep, stuck, held.
        float cnt = 0, rigid = 0, asleep = 0, stuck = 0, held = 0;
        float sz = 0, szMin = 1000, szMax = 0, last = 0;
        for(float r = 0; r < 256; r++)
        {
            if(VR_SmallGib_SlotLive(r))
            {
                entity q = vr_sgib_ring[r];
                vector qd = q.absmax - q.absmin;
                float qs = max(qd_x, max(qd_y, qd_z));
                cnt++;
                last = r;
                sz = sz + qs;
                szMin = min(szMin, qs);
                szMax = max(szMax, qs);
                rigid = rigid + (q.vr_rigid != 0);
                asleep = asleep + ((q.flags & FL_ONGROUND) != 0);
                stuck = stuck + (q.vr_gib_stuck != 0);
                held = held + (q.carry_player != world);
            }
        }
        bprint(sprintf("sgibtest: %d small gibs (%d made): size %.1f..%.1f (mean %.1f) units, a rock %.1f\n",
            cnt, vr_sgib_made, szMin, szMax, sz / max(1, cnt), VR_SmallGib_RockSize()));
        bprint(sprintf("sgibtest: %d rigid, %d asleep, %d stuck, %d held; mass %.2f kg\n", rigid, asleep, stuck, held,
            cnt > 0 ? propvalue(vr_sgib_ring[last], "mass") : 0));
        vr_sgib_test = 0;
    }
    else if(xCase == 13)
    {
        vr_sgib_test_a = 0;
        if(cvar("vr_smallgibs_max") < 128)
        {
            cvar_set("vr_smallgibs_max", "128"); // (each step's lie about together)
        }
        if(cvar("vr_smallgibs_test_hand"))
        {
            // The main hand into the body's middle (the mock headset), half a second to get there.
            localcmd(sprintf("vr_mock_hand_to main %v\n", (m.absmin + m.absmax) * 0.5));
            vr_sgib_test_t = time + 0.5;
        }
    }
    else if(xCase == 10)
    {
        m.think = SUB_Null; // (still: its AI stopped)
        m.nextthink = -1;
        m.velocity = '0 0 0';
    }
    if(xCase == 14)
    {
        float crowd = cvar("vr_smallgibs_test_crowd");
        crowd = crowd > 0 ? crowd : 24;
        entity p = self;
        makevectors('0 1 0' * p.v_viewangle_y);
        vector c = p.origin + v_forward * 160;
        vr_sgib_test_e = spawn();
        setorigin(vr_sgib_test_e, c);
        vr_sgib_test_a = vr_sgib_made;
        for(float i = 0; i < crowd * 1.5; i++)
        {
            // Rings of 12 (every third a box), 36 units apart.
            float ring = floor(i / 12);
            float ang = (i - ring * 12) * 30 + ring * 15;
            makevectors('0 1 0' * ang);
            vector at = c + v_forward * (40 + ring * 36);
            self = spawn();
            if(i - floor(i / 3) * 3 == 2)
            {
                setorigin(self, at - '16 16 0'); // (a box's origin is its corner)
                misc_explobox();
            }
            else
            {
                entity d = self;
                d.weapon = 0;
                setorigin(d, at);
                activator = p;
                func_enemy_dispenser_use();
                if(self != d)
                {
                    self.angles_y = ang + 180;
                }
                remove(d);
            }
            self = p;
        }
    }
    if(xCase <= 5)
    {
        vr_sgib_test = 0;
    }
};

// vr_smallgibs_trace: sample `n`'s age (s), 0 past the last.
float(float n) VR_SmallGib_TraceAge =
{
    if(n < 1) { return 0.05; }
    if(n < 2) { return 0.1; }
    if(n < 3) { return 0.2; }
    if(n < 4) { return 0.3; }
    if(n < 5) { return 0.5; }
    if(n < 6) { return 1; }
    if(n < 7) { return 2.5; }
    return 0;
};

// vr_smallgibs_trace (ROUND21.md, "Small gibs batted by the blade"): each small gib's speed at its samples' ages, each
// jump in it (faster by 25 u/s or more in a frame), its fastest, and at 2.5 s how far it lay from where it was made.
void() VR_SmallGib_TraceFrame =
{
    if(!cvar_hget(cvarh_vr_smallgibs_trace))
    {
        return;
    }
    for(float i = 0; i < 256; i++)
    {
        if(!VR_SmallGib_SlotLive(i))
        {
            continue;
        }
        entity g = vr_sgib_ring[i];
        if(g.vr_sgib_tdone)
        {
            continue;
        }
        float age = time - g.vr_sgib;
        float sp = vlen(g.velocity);
        if(sp > g.vr_sgib_tpeak)
        {
            g.vr_sgib_tpeak = sp;
            g.vr_sgib_tpeakt = age;
        }
        if(age > 0 && sp > g.vr_sgib_tlast + 25)
        {
            bprint(sprintf("sgibtrace: #%g %.3f s: speed jump %.0f to %.0f u/s (%v), rigid %g, held %g\n", g.vr_sgib_id, age,
                g.vr_sgib_tlast, sp, g.velocity, g.vr_rigid, g.carry_player != world));
        }
        g.vr_sgib_tlast = sp;
        float at = VR_SmallGib_TraceAge(g.vr_sgib_tstep);
        if(at > 0 && age >= at)
        {
            g.vr_sgib_tstep = g.vr_sgib_tstep + 1;
            vector d = g.origin - g.vr_sgib_org;
            if(at >= 2.5)
            {
                g.vr_sgib_tdone = TRUE;
                bprint(sprintf("sgibtrace: #%g done: lay %.1f units from where made (%.1f across), fastest %.0f u/s at %.3f s, situation %g\n",
                    g.vr_sgib_id, vlen(d), vlen(d - '0 0 1' * d_z), g.vr_sgib_tpeak, g.vr_sgib_tpeakt, g.vr_sgib_sit));
            }
            else
            {
                bprint(sprintf("sgibtrace: #%g %.2f s: %.0f u/s, %.1f units from where made, rigid %g\n", g.vr_sgib_id, age, sp,
                    vlen(d), g.vr_rigid));
            }
        }
    }
};

// Every server frame (StartFrame): a test asked for (vr_smallgibs_test), or the next step of one running.
void() VR_SmallGib_TestFrame =
{
    VR_SmallGib_TraceFrame();
    float ask = cvar("vr_smallgibs_test");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    if(ask)
    {
        cvar_set("vr_smallgibs_test", "0");
        VR_SmallGib_TestStart(ask);
    }
    else if(vr_sgib_test)
    {
        float t = time - vr_sgib_test_t;
        entity m = vr_sgib_test_m;
        entity g = vr_sgib_test_g;
        if(vr_sgib_test == 6)
        {
            // The chain running through it a second (VR_Saw's cut: a tick's damage each frame).
            if(t < 1)
            {
                float oldWeapon = self.weapon;
                self.weapon = WID_CHAINSAW;
                vector mid = (m.absmin + m.absmax) * 0.5;
                VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, mid, normalize(mid - self.origin));
                T_DamageImpl(m, self, self, cvar_hget(cvarh_vr_chainsaw_damage) * frametime, 1);
                VR_ClearHitKind();
                self.weapon = oldWeapon;
                vr_sgib_test_b++;
            }
            else
            {
                bprint(sprintf("sgibtest: chainsaw on %s (corpse %d): %d ticks in %.2f s, %d small gibs (one each %.2f s at most)\n",
                    m.classname, m.vr_corpse, vr_sgib_test_b, t, vr_sgib_made - vr_sgib_test_a,
                    cvar_hget(cvarh_vr_smallgibs_saw_interval)));
                VR_SmallGib_TestGorePrint("chainsaw");
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 14)
        {
            if(vr_sgib_test_step == 0 && t >= 1)
            {
                // A rocket's blast in the middle (vr_smallgibs_test_blasts: that many in the frame, the others round it).
                vector c = vr_sgib_test_e.origin;
                float blasts = max(1, cvar("vr_smallgibs_test_blasts"));
                for(float i = 0; i < blasts; i++)
                {
                    makevectors('0 1 0' * (i * 360 / blasts));
                    setorigin(vr_sgib_test_e, c + v_forward * (i > 0 ? 64 : 0));
                    T_RadiusDamage(vr_sgib_test_e, world, 250, world);
                    WriteByte(MSG_BROADCAST, SVC_TEMPENTITY);
                    WriteByte(MSG_BROADCAST, TE_EXPLOSION);
                    WriteVec3(MSG_BROADCAST, vr_sgib_test_e.origin);
                }
                setorigin(vr_sgib_test_e, c);
                vr_sgib_test_step = 1;
            }
            else if(vr_sgib_test_step == 1 && t >= 4)
            {
                float standing = 0;
                float boxes = 0;
                for(entity e = nextent(world); e; e = nextent(e))
                {
                    if(e.classname == "monster_army" && e.health > 0)
                    {
                        standing++;
                    }
                    if(e.classname == "explo_box")
                    {
                        boxes++;
                    }
                }
                bprint(sprintf("sgibtest: crowd: %d grunts standing, %d boxes left, %d small gibs\n", standing, boxes,
                    vr_sgib_made - vr_sgib_test_a));
                remove(vr_sgib_test_e);
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 7)
        {
            if(vr_sgib_test_step == 0 && t >= 0.4)
            {
                // Both armed: shot to bursting.
                vr_sgib_test_a = vr_sgib_made;
                entity e = findradius(g.origin, 400);
                float burst = 0;
                while(e)
                {
                    if(e.vr_gib && !e.vr_sgib && e.th_die == VR_Gib_Die)
                    {
                        T_Damage(e, self, self, 100);
                        burst++;
                    }
                    e = e.chain;
                }
                bprint(sprintf("sgibtest: burst %d large gibs and heads: %d small gibs (Burst Into %.0f, a head half as many again)\n",
                    burst, vr_sgib_made - vr_sgib_test_a, cvar_hget(cvarh_vr_smallgibs_burst)));
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 9)
        {
            // Held by the off hand's grip (the script holds it: vr_mock_button off grip 1), then let go of (grip 0).
            float live = g != world && g.vr_sgib_id == vr_sgib_test_id;
            if(vr_sgib_test_step == 0 && t >= 2.9)
            {
                bprint(sprintf("sgibtest: held: after %.1f s (Small Gibs Last 1 s) there %d, held %d, alpha %.2f\n", t, live,
                    live && g.carry_player != world, g.alpha));
                vr_sgib_test_step = 1;
                vr_sgib_test_a = 0;
                vr_sgib_test_b = 0;
            }
            else if(vr_sgib_test_step == 1)
            {
                if(!vr_sgib_test_a && live && g.carry_player == world)
                {
                    vr_sgib_test_a = t; // let go
                }
                if(vr_sgib_test_a && !vr_sgib_test_b && live && (g.flags & FL_ONGROUND))
                {
                    vr_sgib_test_b = t; // landed
                }
                if(!live || t > 20)
                {
                    bprint(sprintf("sgibtest: held: let go at %.2f s, landed at %.2f s, gone at %.2f s (%.2f s after landing: Last 1 s and the fade)\n",
                        vr_sgib_test_a, vr_sgib_test_b, t, t - vr_sgib_test_b));
                    cvar_set("vr_smallgibs_time", "20");
                    vr_sgib_test = 0;
                }
            }
        }
        else if(vr_sgib_test == 18)
        {
            entity tg = vr_sgib_test_g;
            if(vr_sgib_test_step == 0 && t >= 1 && tg != world && tg.vr_gib)
            {
                // Thrown at it as yours (let go of by you, from your eyes).
                vr_sgib_test_step = 1;
                tg.throwhit = QVR_THROWHIT_HIT;
                tg.owner = world;
                tg.vr_fling_hit = 0;
                tg.flags = tg.flags - (tg.flags & FL_ONGROUND);
                tg.vr_rigid = 1;
                tg.carry_letgo = self;
                tg.carry_letgo_time = time;
                vector at = (vr_sgib_test_m.absmin + vr_sgib_test_m.absmax) * 0.5;
                vector eye = self.origin + '0 0 22';
                eye = eye + normalize(at - eye) * 32; // (ahead of you, out of your box)
                setorigin(tg, eye - 0.5 * (tg.mins + tg.maxs));
                tg.vr_spin = '0 0 0';
                tg.avelocity = '0 0 0';
                VR_Carry_Throw(tg, normalize(at - eye) * cvar("vr_test_throw_up_speed") * VRMetersToUnits());
            }
            if(t >= 3.5)
            {
                bprint(sprintf("sgibtest: a gib thrown at %s %.1f s after it was made: it lost %.0f health\n",
                    vr_sgib_test_m.classname, 1, vr_sgib_test_a - vr_sgib_test_m.health));
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 20)
        {
            if(t >= 3)
            {
                bprint(sprintf("sgibtest: drop over you: you lost %.0f health; its flung hits on you %d, spared %d\n",
                    vr_sgib_test_a - self.health, vr_fling_drophits, vr_fling_dropspared));
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 16 || vr_sgib_test == 17 || vr_sgib_test == 19)
        {
            if(t >= (vr_sgib_test == 19 ? 4 : 2)) // (19: its drops come with its death frames)
            {
                bprint(sprintf("sgibtest: underfoot (%s): you lost %.0f health, the monsters near %.0f; gibs' flung hits %d, spared as just made %d (Fresh Gibs Harmless For %.2f s)\n",
                    vr_sgib_test == 16 ? "a blow" : vr_sgib_test == 17 ? "shotgun blasts" : "killed, not gibbed", vr_sgib_test_a - self.health,
                    vr_sgib_test_b - VR_SmallGib_TestNearHealth(self, world), vr_fling_gibhits, vr_fling_gibspared,
                    cvar_hget(cvarh_vr_gib_spawn_harmless)));
                bprint(sprintf("sgibtest: underfoot drops: flung hits on you %d, spared %d (Monster Drops Harmless For %.2f s, Hurt Only Falling %d, Pass Through Bodies %d)\n",
                    vr_fling_drophits, vr_fling_dropspared, cvar_hget(cvarh_vr_prop_drop_grace),
                    cvar_hget(cvarh_vr_prop_drop_falls_only), cvar("vr_prop_drop_pass_inside")));
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 12)
        {
            if(t >= 1)
            {
                entity g1 = vr_sgib_test_g;
                entity g2 = vr_sgib_test_m;
                entity g3 = vr_sgib_test_e;
                bprint(sprintf("sgibtest: thrown into a wall: a gib at 220 stuck %d burst %d; a small gib at 220 stuck %d\n",
                    g1.vr_gib_stuck != 0, !g1.vr_gib, g2.vr_gib_stuck != 0));
                bprint(sprintf("sgibtest: thrown into a wall: a gib at 400 (Gib Splat Speed %.0f) stuck %d burst %d\n",
                    cvar_hget(cvarh_vr_gib_splat_speed), g3.vr_gib_stuck != 0, !g3.vr_gib));
                cvar_set("vr_gore_stick_thrown", "0.5");
                vr_sgib_test = 0;
            }
        }
        else if(vr_sgib_test == 13)
        {
            // Each situation in turn: torn out, then 2.5 s later where they lie. Each frame, their fastest across in
            // their first second (what they really fly at: a push out of a body or a hand shows here, not at launch).
            entity pg = find(world, classname, "vr_smallgib");
            while(pg)
            {
                vector pv = pg.velocity;
                pv_z = 0;
                if(pg.vr_sgib_id && time - pg.vr_sgib_born < 1 && pg.carry_player == world && vlen(pv) > pg.vr_sgib_peak)
                {
                    pg.vr_sgib_peak = vlen(pv);
                    pg.vr_sgib_peakt = time - pg.vr_sgib_born;
                }
                pg = find(pg, classname, "vr_smallgib");
            }
            if(t < 0)
            {
                // (the hand on its way: vr_smallgibs_test_hand)
            }
            else if(!vr_sgib_test_a)
            {
                VR_SmallGib_TestSpeedLaunch(m, vr_sgib_test_step + 1);
                vr_sgib_test_a = 1;
                vr_sgib_test_t = time;
            }
            else if(t >= 2.5)
            {
                VR_SmallGib_TestSpeedLanded(m, vr_sgib_test_step + 1);
                vr_sgib_test_a = 0;
                vr_sgib_test_step++;
                if(vr_sgib_test_step >= 9)
                {
                    vr_sgib_test = 0;
                }
            }
        }
        else if(vr_sgib_test == 10)
        {
            if(vr_sgib_test_step == 0 || vr_sgib_test_step == 2)
            {
                if(vr_sgib_test_step == 2)
                {
                    cvar_set("vr_smallgibs_grace", "0");
                }
                // Just behind it, going through it towards you (300 units/s: across it in the grace).
                vector mid2 = (m.absmin + m.absmax) * 0.5;
                vector out = normalize(self.origin - m.origin);
                out_z = 0;
                g = VR_SmallGib_Spawn(mid2 - out * (m.maxs_x + 6), out * 300 + '0 0 60', m);
                vr_sgib_test_g = g;
                vr_sgib_test_id = g.vr_sgib_id;
                vr_sgib_test_t = time;
                vr_sgib_test_a = 0;
                vr_sgib_test_b = 0;
                vr_sgib_test_step++;
            }
            else if(t >= 0.05 && t < 0.07)
            {
                vr_sgib_test_b = g.owner == m;
            }
            else if(t >= 0.3)
            {
                vector o = (g.absmin + g.absmax) * 0.5 - (m.absmin + m.absmax) * 0.5;
                vector way = normalize(self.origin - m.origin);
                way_z = 0;
                bprint(sprintf("sgibtest: grace %.2f: from behind it at 300 u/s: passes through at 0.05 s %d; at 0.3 s %.0f units %s it (going %.0f u/s %s)\n",
                    cvar_hget(cvarh_vr_smallgibs_grace), vr_sgib_test_b, fabs(o * way), o * way > 0 ? "in front of" : "behind",
                    fabs(g.velocity * way), g.velocity * way > 0 ? "on" : "back"));
                vr_sgib_test_step++;
                if(vr_sgib_test_step >= 4)
                {
                    cvar_set("vr_smallgibs_grace", "0.15");
                    vr_sgib_test = 0;
                }
            }
        }
    }
    self = oself;
};
