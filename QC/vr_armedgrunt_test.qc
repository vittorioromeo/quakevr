// vr_armedgrunt_test.qc -- the armed grunts' tests (vr_armedgrunt.qc; ROUND22.md, "Armed grunts"). `vr_agrunt_test N`
// (Debug > Tests > Armed Grunts), in an open map (vrtesthall), the player kept alive (his health topped up):
// - 1: every weapon in turn: an armed grunt with it ahead (a melee one at arm's length, an explosive one far), the
//   player its enemy, for a few seconds: `agtest: <weapon>: <shots> shots, <dmg> damage to you, clip <c>, reserve <r>,
//   <n> reloads: PASS|FAIL` (PASS: it hurt the player), then `agtest: every weapon: <p> passed, <f> failed`.
// - 2: a shotgun grunt with half a reserve: it fires, reloads (`reload PASS`), is struck on its gun hand (`disarm PASS`:
//   its gun lies about, its clip with it), goes back for it (`rearm PASS`), and is killed (`drop PASS`: the gun lies
//   where it died, its rounds in it).
// - 3: the yank: a shotgun grunt standing ahead (the player unseen), the player's main hand emptied, put on its gun
//   (vr_mock_hand_to) and closed (+grabmain): `yank PASS` when the shotgun is in that hand and the grunt holds nothing.
// - 4: armed grunts' state printed (`agtest: state: ...`: run it again after a save and a load).

float vr_agt_step;      // the test's step
float vr_agt_index;     // test 1: the weapon being tried
float vr_agt_until;     // the step's end
float vr_agt_passed;
float vr_agt_failed;
float vr_agt_dmg0;      // vr_ag_player_dmg at the try's start
float vr_agt_clip;      // test 2: its clip before something
entity vr_agt_grunt;
entity vr_agt_player;
float VR_AGT_COUNT = 17;

float(float i) VR_AGT_Wid =
{
    if(i == 0) { return -1; }
    if(i == 1) { return WID_AXE; }
    if(i == 2) { return WID_SWORD; }
    if(i == 3) { return WID_CROWBAR; }
    if(i == 4) { return WID_CHAINSAW; }
    if(i == 5) { return WID_MJOLNIR; }
    if(i == 6) { return WID_SHOTGUN; }
    if(i == 7) { return WID_SUPER_SHOTGUN; }
    if(i == 8) { return WID_NAILGUN; }
    if(i == 9) { return WID_SUPER_NAILGUN; }
    if(i == 10) { return WID_GRENADE_LAUNCHER; }
    if(i == 11) { return WID_ROCKET_LAUNCHER; }
    if(i == 12) { return WID_LIGHTNING; }
    if(i == 13) { return WID_LASER_CANNON; }
    if(i == 14) { return WID_PROXIMITY_GUN; }
    if(i == 15) { return WID_GRUNTGUN; }
    return WID_ENFRIFLE;
};

string(float wid) VR_AGT_Name = { return wid < 0 ? "fists" : WeaponNameOf(wid, 0); };

// An armed grunt with `wid` (-1 its fists), `dist` ahead of the player on the floor, facing him.
entity(float wid, float dist) VR_AGT_Spawn =
{
    entity p = vr_agt_player;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    traceline(p.origin, p.origin + fw * dist, TRUE, p);
    vector at = trace_endpos - fw * 24;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.weapon = wid;
    monster_vr_grunt();
    entity g = self;
    self = oldself;
    return g;
};

// It sees the player (FoundTarget).
void(entity g) VR_AGT_Hunt =
{
    entity oldself = self;
    self = g;
    self.enemy = vr_agt_player;
    FoundTarget();
    self = oldself;
};

void(entity g) VR_AGT_Remove =
{
    if(g.vr_ag_prop)
    {
        g.vr_ag_prop.weaponinst = world;
        remove(g.vr_ag_prop);
    }
    g.vr_ag_prop = world;
    remove(g);
};

// Every weapon lying about goes (between test 1's tries).
void() VR_AGT_ClearWeapons =
{
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        entity n = find(e, classname, "thrown_weapon");
        remove(e);
        e = n;
    }
};

void(string s) VR_AGT_Print = { bprint(sprintf("agtest: %s\n", s)); };

void(string what, float ok) VR_AGT_Check =
{
    VR_AGT_Print(sprintf("%s %s", what, ok ? "PASS" : "FAIL"));
    if(ok) { vr_agt_passed = vr_agt_passed + 1; } else { vr_agt_failed = vr_agt_failed + 1; }
};

// The weapon lying nearest `at` (world: none within 96).
entity(vector at) VR_AGT_LyingNear =
{
    entity best = world;
    float bestDist = 96;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(vlen(e.origin - at) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - at);
        }
        e = find(e, classname, "thrown_weapon");
    }
    return best;
};

void() VR_AGTest_Weapons =
{
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        if(vr_agt_index >= VR_AGT_COUNT)
        {
            VR_AGT_Print(sprintf("every weapon: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        VR_AGT_ClearWeapons();
        float wid = VR_AGT_Wid(vr_agt_index);
        float c = VR_AG_Class(wid);
        float dist = (c == VR_AG_CLASS_MELEE || c == VR_AG_CLASS_FISTS) ? 70 : c == VR_AG_CLASS_EXPLOSIVE ? 360 : 240;
        vr_agt_grunt = VR_AGT_Spawn(wid, dist);
        vr_agt_step = 1;
        vr_agt_until = time + 0.6;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until) { return; }
        VR_AGT_Hunt(g);
        vr_agt_dmg0 = vr_ag_player_dmg;
        vr_agt_step = 2;
        vr_agt_until = time + 5;
        return;
    }
    if(time < vr_agt_until && g.health > 0)
    {
        return;
    }
    float dmg = vr_ag_player_dmg - vr_agt_dmg0;
    VR_AGT_Check(sprintf("%s: %g shots, %g blows, %g damage to you, clip %g, reserve %g, %g reloads, %g disarms:",
        VR_AGT_Name(g.vr_ag_lastwid), g.vr_ag_total_shots, g.vr_ag_strikes, dmg, g.vr_ag_prop.weaponinst.wi_clip,
        g.vr_ag_reserve, g.vr_ag_reloads, g.vr_ag_disarms), dmg > 0);
    VR_AGT_Remove(g);
    vr_agt_index = vr_agt_index + 1;
    vr_agt_step = 0;
    if(cvar_hget(cvarh_vr_agrunt_test) >= 100)
    {
        cvar_set("vr_agrunt_test", "0"); // (one weapon's)
    }
};

void() VR_AGTest_Sequence =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        cvar_set("vr_agrunt_reserve", "1");
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 200);
        vr_agt_step = 1;
        vr_agt_until = time + 0.6;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until) { return; }
        VR_AGT_Hunt(g);
        vr_agt_step = 2;
        vr_agt_until = time + 25;
        return;
    }
    if(vr_agt_step == 2) // fires its clip out, reloads
    {
        if(g.vr_ag_reloads < 1 && time < vr_agt_until) { return; }
        VR_AGT_Check(sprintf("reload: %g shots, %g reloads, clip %g, reserve %g:", vr_ag_shots_fired, g.vr_ag_reloads,
            g.vr_ag_prop.weaponinst.wi_clip, g.vr_ag_reserve), g.vr_ag_reloads >= 1 && g.vr_ag_prop.weaponinst.wi_clip > 0);
        // Struck on its gun hand.
        vr_agt_clip = g.vr_ag_prop.weaponinst.wi_clip;
        vr_hitpos = g.vr_ag_prop.origin;
        T_Damage(g, p, p, 12);
        vr_hitpos = '0 0 0';
        entity w = VR_AGT_LyingNear(g.origin);
        VR_AGT_Check(sprintf("disarm: held %s, lying %s with %g:", g.vr_ag_prop ? "something" : "nothing",
            w ? w.netname : "nothing", w.weaponinst.wi_clip), !g.vr_ag_prop && w && w.weaponinst.wi_clip == vr_agt_clip);
        vr_agt_step = 3;
        vr_agt_until = time + 12;
        return;
    }
    if(vr_agt_step == 3) // goes back for it
    {
        if(!g.vr_ag_prop && time < vr_agt_until) { return; }
        VR_AGT_Check(sprintf("rearm: %g pickups, holds %s:", g.vr_ag_pickups, g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"),
            g.vr_ag_pickups >= 1 && g.vr_ag_prop != world);
        vr_agt_clip = g.vr_ag_prop.weaponinst.wi_clip;
        vector at = g.origin;
        T_Damage(g, p, p, 200);
        entity w2 = VR_AGT_LyingNear(at);
        VR_AGT_Check(sprintf("drop: lying %s with %g (held %g):", w2 ? w2.netname : "nothing", w2.weaponinst.wi_clip, vr_agt_clip),
            w2 && w2.weapon == WID_SHOTGUN && w2.weaponinst.wi_clip == vr_agt_clip);
        VR_AGT_Print(sprintf("sequence: %g passed, %g failed", vr_agt_passed, vr_agt_failed));
        cvar_set("vr_agrunt_reserve", "2");
        cvar_set("vr_agrunt_test", "0");
    }
};

void() VR_AGTest_Yank =
{
    entity g = vr_agt_grunt;
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        p.flags = p.flags | FL_NOTARGET;
        p.weapon = WID_FIST; // (the main hand empty)
        p.weaponinst = world;
        p.weaponflags = 0;
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 60);
        vr_agt_step = 1;
        vr_agt_until = time + 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time < vr_agt_until) { return; }
        // The grunt moved so that its gun is at your main hand (the script holds it out: vr_mock_hand), then the script
        // closes it (+grabmain) within 3 s.
        vector off = p.handpos - g.vr_ag_prop.origin;
        off_z = 0;
        setorigin(g, g.origin + off);
        VR_AG_PlaceProp(g, g.vr_ag_prop);
        VR_AGT_Print(sprintf("yank: the hand at %v, the gun at %v", p.handpos, g.vr_ag_prop.origin));
        vr_agt_step = 2;
        vr_agt_until = time + 3;
        return;
    }
    if(time < vr_agt_until && p.weapon != WID_SHOTGUN)
    {
        return;
    }
    VR_AGT_Check(sprintf("yank: your main hand %s, the grunt holds %s:", VR_AGT_Name(p.weapon),
        g.vr_ag_prop ? g.vr_ag_prop.netname : "nothing"), p.weapon == WID_SHOTGUN && !g.vr_ag_prop);
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};

void() VR_AGTest_State =
{
    float n = 0;
    entity e = find(world, classname, "monster_army");
    while(e)
    {
        if(e.vr_ag)
        {
            n = n + 1;
            entity q = e.vr_ag_prop;
            VR_AGT_Print(sprintf("state: %s health %g, holds %s (clip %g, reserve %g), its weapon %.1f from its hand",
                VR_AGT_Name(e.vr_ag_wid), e.health, q ? q.netname : "nothing", q.weaponinst.wi_clip,
                e.vr_ag_reserve, q ? vlen(q.origin - modelvertex(e, VR_AG_HAND_V1)) : -1));
            if(q)
            {
                VR_AGT_Print(sprintf("state:   %s at %v (the grunt's origin + %v), angles %v, muzzle %v", q.model, q.origin,
                    q.origin - e.origin, q.angles, weaponmuzzle(q)));
            }
        }
        e = find(e, classname, "monster_army");
    }
    VR_AGT_Print(sprintf("state: %g armed grunts", n));
    cvar_set("vr_agrunt_test", "0");
};

void() VR_AGTest_Frame =
{
    float t = cvar_hget(cvarh_vr_agrunt_test);
    if(!t)
    {
        vr_agt_player = world;
        return;
    }
    if(!vr_agt_player)
    {
        vr_agt_player = find(world, classname, "player");
        vr_agt_step = 0;
        vr_agt_index = 0;
        vr_agt_passed = 0;
        vr_agt_failed = 0;
        if(!vr_agt_player)
        {
            return;
        }
    }
    vr_agt_player.health = max(vr_agt_player.health, 500); // (kept alive)
    if(t == 1 || t >= 100)
    {
        if(t >= 100 && vr_agt_step == 0 && vr_agt_index == 0)
        {
            vr_agt_index = t - 100; // (`100+i`: weapon i alone)
        }
        VR_AGTest_Weapons();
    }
    else if(t == 2) { VR_AGTest_Sequence(); }
    else if(t == 3) { VR_AGTest_Yank(); }
    else if(t == 4) { VR_AGTest_State(); }
    else if(t >= 6 && t <= 9) // a row of armed grunts ahead, 30 units apart (eyeshots): 6 guns, 7 melee, 8 more guns, 9 the rest
    {
        float base = (t - 6) * 4;
        float k;
        for(k = 0; k < 4; k++)
        {
            float i = base + k + 1;
            if(i <= VR_AGT_COUNT)
            {
                makevectors(vr_agt_player.angles);
                vector side = v_right;
                side_z = 0;
                vector keep = vr_agt_player.origin;
                setorigin(vr_agt_player, keep + side * ((k - 1.5) * 34));
                entity r = VR_AGT_Spawn(VR_AGT_Wid(i < VR_AGT_COUNT ? i : 0), cvar("scratch1") > 0 ? cvar("scratch1") : 110);
                r.angles_y = vectoyaw(keep - r.origin);
                setorigin(vr_agt_player, keep);
            }
        }
        cvar_set("vr_agrunt_test", "0");
    }
    else if(t == 5) // a few armed grunts ahead, hunting you (eyeshots, save and load)
    {
        VR_AGT_Spawn(WID_SHOTGUN, 160);
        makevectors(vr_agt_player.angles);
        vr_agt_player.angles_y = vr_agt_player.angles_y + 25;
        VR_AGT_Spawn(WID_ROCKET_LAUNCHER, 220);
        vr_agt_player.angles_y = vr_agt_player.angles_y - 50;
        VR_AGT_Spawn(WID_AXE, 130);
        vr_agt_player.angles_y = vr_agt_player.angles_y + 25;
        cvar_set("vr_agrunt_test", "0");
    }
};
