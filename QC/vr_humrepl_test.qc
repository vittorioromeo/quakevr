#ifdef QVR_TESTS
// vr_humrepl_test.qc -- AD's and Quoth's humanoid variants as armed humanoids, the recolours by weapon (vr_humrepl.qc;
// ARMED_MONSTERS.md, "AD's and Quoth's variants"):
// - vr_ad_test 219 (in AD's rules: an AD map, or e1m1 in the AD campaign; humrepl_test.sh): each variant spawned by its
//   AD function is its row holding its gun, with its health and look, counted once; one's death fires its target and
//   counts; with vr_agrunt_repl_variants 0 it stays AD's; a toolgun-style armed grunt with a rocket launcher and one with
//   a nailgun get their looks; then the client is waited for (its looks known, vr_humlook_known: a few frames, a 3 s
//   give-up) and vr_humlook_info prints the nearest. Ends with "humrepl: N passed, M failed".
// - vr_ad_test 220 (any map; humrepl_census.sh): the map's census, one line: "humcensus: <map> monsters N armed A
//   variants V left L", L the variants' classnames still AD's (0 with the setting on), then "humcensus: done".

float vr_humt_used;
void() VR_HUMT_Used = { vr_humt_used = vr_humt_used + 1; };

// Variant `v` made by its AD spawn function at `at`.
entity(float v, vector at) VR_HUMT_Spawn =
{
    entity os = self;
    entity e = spawn();
    e.classname = VR_HUM_ClassName(v);
    setorigin(e, at);
    self = e;
    if(v == VR_HUM_V_ROCKET)          { ad_monster_army_rocket(); }
    else if(v == VR_HUM_V_GRENADE)    { ad_monster_army_grenade(); }
    else if(v == VR_HUM_V_PLASMA)     { ad_monster_army_plasma(); }
    else if(v == VR_HUM_V_DEFENDER)   { ad_monster_defender(); }
    else if(v == VR_HUM_V_ELIMINATOR) { ad_monster_eliminator(); }
    else if(v == VR_HUM_V_PYRO)       { ad_monster_pyro(); }
    else if(v == VR_HUM_V_FUMIGATOR)  { ad_monster_fumigator(); }
    else                              { ad_monster_dcrossbow(); }
    self = os;
    return e;
};

// The category its gun gives (VR_HUM_Category's, as the table names them).
float(float v) VR_HUMT_Cat =
{
    if(v == VR_HUM_V_ROCKET || v == VR_HUM_V_GRENADE) { return VR_HUM_CAT_EXPLOSIVE; }
    if(v == VR_HUM_V_PLASMA || v == VR_HUM_V_ELIMINATOR) { return VR_HUM_CAT_ENERGY; }
    if(v == VR_HUM_V_PYRO) { return VR_HUM_CAT_FLAME; }
    if(v == VR_HUM_V_FUMIGATOR) { return VR_HUM_CAT_POISON; }
    return VR_HUM_CAT_SHELLS; // (the defender's shotgun, the crossbow)
};

// The armed ones with a look now.
float() VR_HUMT_Looks =
{
    float n = 0;
    entity e = nextent(world);
    while(e)
    {
        n = n + ((e.vr_ag && e.vr_hum_look > 0) ? 1 : 0);
        e = nextent(e);
    }
    return n;
};

void() VR_HUMT_ClientThink =
{
    float timedOut = time > self.ltime;
    entity r = self.enemy;
    if(r)
    {
        // A death fires its target and counts (once it is up: its monster's start makes it damageable a moment later).
        if(r.takedamage == DAMAGE_NO && !timedOut)
        {
            self.nextthink = time;
            return;
        }
        vr_humt_used = 0;
        float killed = killed_monsters;
        T_Damage(r, self.owner, self.owner, 5000);
        VR_Test_Check(vr_humt_used == 1 && killed_monsters == killed + 1, sprintf("a rocket soldier's death fired its target (%g) and counted (%g -> %g)%s",
            vr_humt_used, killed, killed_monsters, timedOut ? ": timed out waiting for it to be up" : ""));
        self.enemy = world;
    }
    float known = cvar_hget(cvarh_vr_humlook_known);
    if(known >= self.cnt || timedOut)
    {
        VR_Test_Check(known >= self.cnt, sprintf("the client knows %g looks (the armed ones with a look: %g)%s", known,
            self.cnt, timedOut ? ": timed out" : ""));
        localcmd("vr_humlook_info\n");
        VR_Test_Verdict();
        remove(self);
        return;
    }
    self.nextthink = time; // (the next frame: the reliable messages come with it)
};

void(entity p) VR_HUM_Test =
{
    VR_Test_BeginFlags("humrepl", VR_TEST_LOUD);
    if(!AD_Rules())
    {
        VR_Test_Check(FALSE, "AD's rules (run in an AD map, or e1m1 in the AD campaign)");
        VR_Test_Verdict();
        return;
    }
    // (the settings it needs on for its spawns, put back as they were after them: a checklist scene runs it in his game)
    string keep = strzone(sprintf("vr_agrunt_repl %g;vr_agrunt_repl_ad %g;vr_agrunt_repl_variants %g;vr_agknight %g\n",
        cvar_hget(cvarh_vr_agrunt_repl), cvar_hget(cvarh_vr_agrunt_repl_ad), cvar_hget(cvarh_vr_agrunt_repl_variants),
        cvar_hget(cvarh_vr_agknight)));
    cvar_set("vr_agrunt_repl", "1");
    cvar_set("vr_agrunt_repl_ad", "1");
    cvar_set("vr_agrunt_repl_variants", "1");
    cvar_set("vr_agknight", "1");
    makevectors([0, p.v_angle_y, 0]);
    vector at = p.origin + v_forward * 200;
    float v;
    for(v = 1; v < VR_HUM_V_COUNT; v++)
    {
        float before = total_monsters;
        entity e = VR_HUMT_Spawn(v, at + v_right * (v - 4.5) * 56);
        float kind = VR_HUM_Row(v);
        VR_AG_Decode(VR_HUM_Weapon(v));
        float wid = vr_ag_dec_wid;
        VR_Test_Check(e.vr_ag && e.vr_ag_kind == kind && e.vr_hum_variant == v, sprintf("%s: an armed row %g (want %g, armed %g)",
            VR_HUM_ClassName(v), e.vr_ag_kind, kind, e.vr_ag));
        VR_Test_Check(e.vr_ag_wid == wid && e.vr_ag_prop != world, sprintf("%s: holds %g (want %g)", VR_HUM_ClassName(v), e.vr_ag_wid, wid));
        VR_Test_Check(e.health == VR_HUM_Health(v), sprintf("%s: health %g (want %g)", VR_HUM_ClassName(v), e.health, VR_HUM_Health(v)));
        VR_Test_Check(e.vr_hum_look == kind * 16 + VR_HUMT_Cat(v), sprintf("%s: look %g (want %g)", VR_HUM_ClassName(v),
            e.vr_hum_look, kind * 16 + VR_HUMT_Cat(v)));
        VR_Test_Check(total_monsters == before + 1, sprintf("%s: counted once (%g -> %g)", VR_HUM_ClassName(v), before, total_monsters));
    }
    // A death fires its target and counts (killed in VR_HUMT_ClientThink once it is up).
    entity relay = spawn();
    relay.targetname = "vr_humt_relay";
    relay.use = VR_HUMT_Used;
    entity r = VR_HUMT_Spawn(VR_HUM_V_ROCKET, at + v_forward * 96);
    r.target = "vr_humt_relay";
    // Off: AD's own.
    cvar_set("vr_agrunt_repl_variants", "0");
    entity off = VR_HUMT_Spawn(VR_HUM_V_PLASMA, at + v_forward * 160);
    cvar_set("vr_agrunt_repl_variants", "1");
    VR_Test_Check(!off.vr_ag && off.classname == "monster_army_plasma", sprintf("off: the plasma soldier AD's (%s, armed %g)",
        off.classname, off.vr_ag));
    // Looks by weapon, however armed (a toolgun spawn: "weapon").
    entity os = self;
    entity g1 = spawn();
    setorigin(g1, at - v_forward * 60);
    g1.weapon = WID_ROCKET_LAUNCHER;
    self = g1;
    monster_vr_grunt();
    entity g2 = spawn();
    setorigin(g2, at - v_forward * 60 + v_right * 60);
    g2.weapon = WID_NAILGUN;
    self = g2;
    monster_vr_enforcer();
    self = os;
    VR_Test_Check(g1.vr_hum_look == VR_AM_GRUNT * 16 + VR_HUM_CAT_EXPLOSIVE, sprintf("a grunt with a rocket launcher: look %g", g1.vr_hum_look));
    VR_Test_Check(g2.vr_hum_look == VR_AM_ENFORCER * 16 + VR_HUM_CAT_NAILS, sprintf("an enforcer with a nailgun: look %g", g2.vr_hum_look));
    VR_Test_Check(g1.vr_wp_pack != world, "the rocket grunt wears a pack (its look the engine's from its wearer)");
    localcmd(keep); // (the settings as they were)
    // The client's word.
    entity w = spawn();
    w.classname = "vr_humt_wait";
    w.cnt = VR_HUMT_Looks() - 1; // (less the rocket soldier killed first: its corpse keeps its look, but it may be gibbed)
    w.enemy = r;
    w.owner = p;
    w.ltime = time + 3;
    w.think = VR_HUMT_ClientThink;
    w.nextthink = time;
};

// vr_ad_test 221 (humrepl_shots.sh): a row of armed humanoids of row scratch1 before you, one per category: a shotgun (as
// it is), a rocket launcher, the thunderbolt, AD's flamethrower and poison sprayer (AD's data mounted), the last row's
// gone first. Prints "humlineup: row K: N made".
.float vr_humt_row; // one of 221's row
void(entity p) VR_HUM_Lineup =
{
    entity e = findflags(world, vr_humt_row, 1);
    while(e)
    {
        entity next = findflags(e, vr_humt_row, 1);
        VR_AGR_Unmake(e.vr_ag_prop);
        VR_AGR_Unmake(e.vr_ag_prop2);
        if(e.vr_wp_pack)
        {
            remove(e.vr_wp_pack);
        }
        remove(e);
        e = next;
    }
    float kind = rint(cvar_hget(cvarh_scratch1));
    float w[5];
    w[0] = WID_SHOTGUN;
    w[1] = WID_ROCKET_LAUNCHER;
    w[2] = WID_LIGHTNING;
    w[3] = VR_ADGun_Available() ? WID_ADPYRO : WID_FLAREGUN; // (without AD's data: the flare gun, flame too)
    w[4] = VR_ADGun_Available() ? WID_ADFUMIGATOR : WID_SUPER_NAILGUN; // (... nails: as it is)
    makevectors([0, p.angles_y, 0]); // (its body's yaw: the view's in the tests' mock headset)
    entity os = self;
    float made = 0;
    float i;
    for(i = 0; i < 5; i++)
    {
        entity g = spawn();
        setorigin(g, p.origin + v_forward * 150 + v_right * (i - 2) * 44);
        g.angles_y = vectoyaw(p.origin - g.origin);
        g.weapon = w[i];
        self = g;
        VR_AM_Spawn(kind);
        self = os;
        g.vr_humt_row = 1;
        made = made + (g.vr_ag ? 1 : 0);
    }
    bprint(sprintf("humlineup: row %g: %g made\n", kind, made));
};

void() VR_HUM_CensusNow =
{
    float armed = 0;
    float left = 0;
    entity e = nextent(world);
    while(e)
    {
        armed = armed + ((e.vr_ag && (e.flags & FL_MONSTER)) ? 1 : 0);
        float v;
        for(v = 1; v < VR_HUM_V_COUNT; v++)
        {
            left = left + (e.classname == VR_HUM_ClassName(v) ? 1 : 0);
        }
        e = nextent(e);
    }
    bprint(sprintf("humcensus: %s monsters %g armed %g variants %g left %g\n", mapname, total_monsters, armed, vr_hum_made, left));
    bprint("humcensus: done\n");
};

// The census once a player is in the game (a script sets vr_ad_test 220 as it loads the map: its first frame comes
// before the client, whose console would miss the lines); a 10 s give-up prints it anyway.
void() VR_HUM_CensusThink =
{
    if(!VR_Player() && time < self.ltime)
    {
        self.nextthink = time;
        return;
    }
    if(self.cnt < 3) // (three frames more: its client's sign-on done, the lines reach its console)
    {
        self.cnt = self.cnt + 1;
        self.nextthink = time;
        return;
    }
    VR_HUM_CensusNow();
    remove(self);
};

void() VR_HUM_Census =
{
    entity w = spawn();
    w.classname = "vr_humt_census";
    w.ltime = time + 10;
    w.think = VR_HUM_CensusThink;
    w.nextthink = time;
};
#endif
