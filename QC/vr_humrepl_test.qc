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
    else if(v == VR_HUM_V_DGUARD)     { ad_monster_dguard(); }
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
    string keep = strzone(sprintf("vr_agrunt_repl %g;vr_agrunt_repl_ad %g;vr_agrunt_repl_variants %g;vr_agknight %g;vr_agrunt_repl_crossbow %g\n",
        cvar_hget(cvarh_vr_agrunt_repl), cvar_hget(cvarh_vr_agrunt_repl_ad), cvar_hget(cvarh_vr_agrunt_repl_variants),
        cvar_hget(cvarh_vr_agknight), cvar_hget(cvarh_vr_agrunt_repl_crossbow)));
    cvar_set("vr_agrunt_repl", "1");
    cvar_set("vr_agrunt_repl_ad", "1");
    cvar_set("vr_agrunt_repl_variants", "1");
    cvar_set("vr_agknight", "1");
    cvar_set("vr_agrunt_repl_crossbow", "1"); // (its own switch, on by default since adknights; on for the rows below)
    cvar_set("vr_agrunt_repl_dguard", "1");
    makevectors([0, p.v_angle_y, 0]);
    vector hf = v_forward; // (kept: each spawn makes its own vectors)
    vector hr = v_right;
    vector at = p.origin + hf * 200;
    float v;
    for(v = 1; v < VR_HUM_V_COUNT; v++)
    {
        float before = total_monsters;
        entity e = VR_HUMT_Spawn(v, at + hr * (v - 4.5) * 56);
        float kind = VR_HUM_Row(v);
        VR_AG_Decode(VR_HUM_Weapon(v));
        float wid = vr_ag_dec_wid;
        VR_Test_Check(e.vr_ag && e.vr_ag_kind == kind && e.vr_hum_variant == v, sprintf("%s: an armed row %g (want %g, armed %g)",
            VR_HUM_ClassName(v), e.vr_ag_kind, kind, e.vr_ag));
        VR_Test_Check(e.vr_ag_wid == wid && e.vr_ag_prop != world, sprintf("%s: holds %g (want %g)", VR_HUM_ClassName(v), e.vr_ag_wid, wid));
        VR_Test_Check(e.health == VR_HUM_Health(v), sprintf("%s: health %g (want %g)", VR_HUM_ClassName(v), e.health, VR_HUM_Health(v)));
        float look = kind < VR_AM_DGUARD ? kind * 16 + VR_HUMT_Cat(v) : 0; // (AD's armed knights: AD's skins)
        VR_Test_Check(e.vr_hum_look == look, sprintf("%s: look %g (want %g)", VR_HUM_ClassName(v), e.vr_hum_look, look));
        VR_AM_Row(kind);
        VR_Test_Check(e.model == vr_am_model, sprintf("%s: model %s (want %s)", VR_HUM_ClassName(v), e.model, vr_am_model));
        if(v == VR_HUM_V_DGUARD)
        {
            // (adknights: its own sword kept, the Death Guard's)
            VR_Test_Check(VR_ADMelee_Kind(e.vr_ag_wflags) == VR_ADMELEE_DGUARD, sprintf("monster_dguard: its sword's kind %g (want %g)",
                VR_ADMelee_Kind(e.vr_ag_wflags), VR_ADMELEE_DGUARD));
        }
        VR_Test_Check(total_monsters == before + 1, sprintf("%s: counted once (%g -> %g)", VR_HUM_ClassName(v), before, total_monsters));
    }
    // A death fires its target and counts (killed in VR_HUMT_ClientThink once it is up).
    entity relay = spawn();
    relay.targetname = "vr_humt_relay";
    relay.use = VR_HUMT_Used;
    entity r = VR_HUMT_Spawn(VR_HUM_V_ROCKET, at + hf * 96);
    r.target = "vr_humt_relay";
    // Off: AD's own.
    cvar_set("vr_agrunt_repl_variants", "0");
    entity off = VR_HUMT_Spawn(VR_HUM_V_PLASMA, at + hf * 160);
    cvar_set("vr_agrunt_repl_variants", "1");
    VR_Test_Check(!off.vr_ag && off.classname == "monster_army_plasma", sprintf("off: the plasma soldier AD's (%s, armed %g)",
        off.classname, off.vr_ag));
    // The death guard's own switch off: AD's own (adknights).
    cvar_set("vr_agrunt_repl_dguard", "0");
    entity dg = VR_HUMT_Spawn(VR_HUM_V_DGUARD, at + hf * 160 - hr * 64);
    cvar_set("vr_agrunt_repl_dguard", "1");
    VR_Test_Check(!dg.vr_ag && dg.classname == "monster_dguard", sprintf("dguard off: the death guard AD's (%s, armed %g)",
        dg.classname, dg.vr_ag));
    // (vrmisc, his note 2026-11-26) The crossbow knight's own switch off: AD's own.
    cvar_set("vr_agrunt_repl_crossbow", "0");
    entity xb = VR_HUMT_Spawn(VR_HUM_V_CROSSBOW, at + hf * 160 + hr * 64);
    cvar_set("vr_agrunt_repl_crossbow", "1");
    VR_Test_Check(!xb.vr_ag && xb.classname == "monster_dcrossbow", sprintf("crossbow off: the crossbow knight AD's (%s, armed %g)",
        xb.classname, xb.vr_ag));
    // Looks by weapon, however armed (a toolgun spawn: "weapon").
    entity os = self;
    entity g1 = spawn();
    setorigin(g1, at - hf * 60);
    g1.weapon = WID_ROCKET_LAUNCHER;
    self = g1;
    monster_vr_grunt();
    entity g2 = spawn();
    setorigin(g2, at - hf * 60 + hr * 60);
    g2.weapon = WID_NAILGUN;
    self = g2;
    monster_vr_enforcer();
    self = os;
    VR_Test_Check(g1.vr_hum_look == VR_AM_GRUNT * 16 + VR_HUM_CAT_EXPLOSIVE, sprintf("a grunt with a rocket launcher: look %g", g1.vr_hum_look));
    VR_Test_Check(g2.vr_hum_look == VR_AM_ENFORCER * 16 + VR_HUM_CAT_NAILS, sprintf("an enforcer with a nailgun: look %g", g2.vr_hum_look));
    VR_Test_Check(g1.vr_wp_pack != world, "the rocket grunt wears a pack (its look the engine's from its wearer)");
    // (fbmisc1123) The enforcer's own laser rifle uses cells: blue as any energy weapon (it was its own gun's: undyed).
    entity g4 = spawn();
    setorigin(g4, at - hf * 120);
    g4.weapon = WID_ENFRIFLE;
    self = g4;
    monster_vr_enforcer();
    self = os;
    VR_Test_Check(g4.vr_hum_look == VR_AM_ENFORCER * 16 + VR_HUM_CAT_ENERGY, sprintf("an enforcer with its laser rifle: look %g (want %g)",
        g4.vr_hum_look, VR_AM_ENFORCER * 16 + VR_HUM_CAT_ENERGY));
    // His note kellbase1 2026-11-22: an energy weapon's enforcer drops a blue pack (DropBackpack: its body's look + pack).
    entity g3 = spawn();
    setorigin(g3, at - hf * 60 - hr * 60);
    g3.weapon = WID_LIGHTNING;
    self = g3;
    monster_vr_enforcer();
    self.ammo_cells = 10;
    DropBackpack();
    self = os;
    entity pk = world;
    entity it = nextent(world);
    while(it)
    {
        if(it.model == "progs/backpack.mdl" && it.vr_monster_drop == time && vlen(it.origin - g3.origin) < 40)
        {
            pk = it;
        }
        it = nextent(it);
    }
    float wantPack = VR_AM_ENFORCER * 16 + VR_HUM_CAT_ENERGY + 4096;
    VR_Test_Check(g3.vr_hum_look == VR_AM_ENFORCER * 16 + VR_HUM_CAT_ENERGY && pk != world && pk.vr_hum_look == wantPack,
        sprintf("a thunderbolt enforcer's dropped pack: look %g (want %g; its body %g)", pk.vr_hum_look, wantPack, g3.vr_hum_look));
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
// gone first. Prints "humlineup: row K: N made". The Texture Studio's Line Up (vr_humstudio.cpp) calls VR_HUM_LineupKind
// with its options (`opts`, bits): 1 no backpacks, 2 no armour, 4 no weapons drawn, 8 a backpack on a stand of its own at
// the row's end (its look the row's explosive one's pack), 16 the row turned `turn` degrees from facing you.
.float vr_humt_row; // one of 221's row
.float vr_humt_yaw; // ... its yaw facing you (VR_HUM_LineupTurn turns it from there)
void(entity p, float kind, float opts, float turn) VR_HUM_LineupTurn =
{
    entity e = findflags(world, vr_humt_row, 1);
    while(e)
    {
        e.angles_y = anglemod(e.vr_humt_yaw + turn);
        e.ideal_yaw = e.angles_y;
        e = findflags(e, vr_humt_row, 1);
    }
};

void(entity p, float kind, float opts, float turn) VR_HUM_LineupKind =
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
    float w[5];
    w[0] = WID_SHOTGUN;
    w[1] = WID_ROCKET_LAUNCHER;
    w[2] = WID_LIGHTNING;
    w[3] = VR_ADGun_Available() ? WID_ADPYRO : WID_FLAREGUN; // (without AD's data: the flare gun, flame too)
    w[4] = VR_ADGun_Available() ? WID_ADFUMIGATOR : WID_SUPER_NAILGUN; // (... nails: as it is)
    makevectors([0, p.angles_y, 0]); // (its body's yaw: the view's in the tests' mock headset)
    vector fw = v_forward; // (kept: each spawn makes its own vectors)
    vector rt = v_right;
    entity os = self;
    float made = 0;
    float i;
    for(i = 0; i < 5; i++)
    {
        entity g = spawn();
        setorigin(g, p.origin + fw * 170 + rt * (i - 2) * 64); // (64 apart: one's held-out gun arm clear of the next)
        g.angles_y = vectoyaw(p.origin - g.origin);
        g.weapon = w[i];
        self = g;
        VR_AM_Spawn(kind);
        self = os;
        g.vr_humt_row = 1;
        g.vr_humt_yaw = g.angles_y;
        if((opts & 1) && g.vr_wp_pack)
        {
            remove(g.vr_wp_pack);
            g.vr_wp_pack = world;
            g.vr_wp_worn = 0;
        }
        if(opts & 2)
        {
            VR_AgArmor_Give(g, 0, 0);
        }
        if(opts & 4)
        {
            if(g.vr_ag_prop) { g.vr_ag_prop.alpha = 0.001; } // (drawn not at all: ENTALPHA_ZERO; held as before)
            if(g.vr_ag_prop2) { g.vr_ag_prop2.alpha = 0.001; }
        }
        made = made + (g.vr_ag ? 1 : 0);
        bprint(sprintf("humlineup: %g at %v (you %v), armed %g, look %g\n", i, g.origin, p.origin, g.vr_ag, g.vr_hum_look));
    }
    if(opts & 8)
    {
        precache_model("progs/backpack.mdl");
        entity pk = spawn();
        pk.classname = "vr_humt_pack";
        setmodel(pk, "progs/backpack.mdl");
        pk.vr_ag_kind = kind;
        VR_WP_Size(pk);
        pk.solid = SOLID_NOT;
        pk.movetype = MOVETYPE_NONE;
        setorigin(pk, p.origin + fw * 110 + rt * 96 - '0 0 8'); // (before the row's right half: in a flat view's field too)
        pk.angles_y = vectoyaw(p.origin - pk.origin) + 180; // (its back to you: the side a wearer shows)
        pk.vr_humt_yaw = pk.angles_y;
        pk.vr_hum_look = kind * 16 + VR_HUM_CAT_EXPLOSIVE + 4096; // (its own look: a worn pack's, its wearer's + 4096)
        pk.vr_humt_row = 1;
        bprint(sprintf("humlineup: a pack on its stand at %v\n", pk.origin));
    }
    if(opts & 16)
    {
        VR_HUM_LineupTurn(p, kind, opts, turn);
    }
    bprint(sprintf("humlineup: row %g: %g made\n", kind, made));
};

void(entity p) VR_HUM_Lineup =
{
    VR_HUM_LineupKind(p, rint(cvar_hget(cvarh_scratch1)), 0, 0);
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

// vr_ad_test 222 (Misc/quakevr/adhumaudit_shots.sh; ARMED_MONSTERS.md "Audit 2"): one of AD's remaining humanoid models
// (scratch1 the pair, 0..VR_HUMO_COUNT-1) beside the armed row's model nearest its build, both in their first stand
// frame, as still props (no monster, no AI): ours, the two overlaid (AD's translucent), AD's, and the overlay turned
// side-on. AD's data mounted (its models read in place). Prints "humoverlay: pair K: <ours> <AD's>".
float VR_HUMO_COUNT = 12;
.float vr_humo_prop; // one of 222's props (gone with the next pair)

string(float k) VR_HUMO_Ours =
{
    if(k == 1 || k == 7) { return "progs/vrknight.mdl"; }
    if(k == 9) { return "progs/vrenforcer.mdl"; }
    if(k == 11) { return "progs/vrgrunt.mdl"; }
    return "progs/vrhknight.mdl";
};

string(float k) VR_HUMO_Theirs =
{
    if(k == 0) { return "progs/mon_dknight.mdl"; }
    if(k == 1) { return "progs/mon_dguard.mdl"; }
    if(k == 2) { return "progs/mon_dguardq.mdl"; }
    if(k == 3) { return "progs/mon_dsergeant.mdl"; }
    if(k == 4) { return "progs/mon_dfury.mdl"; }
    if(k == 5) { return "progs/mon_dlord.mdl"; }
    if(k == 6) { return "progs/mon_hknight.mdl"; }
    if(k == 7) { return "progs/mon_zombiek.mdl"; }
    if(k == 8) { return "progs/mon_justice.mdl"; }
    if(k == 9) { return "progs/mon_centurion.mdl"; }
    if(k == 10) { return "progs/mon_judge.mdl"; }
    return "progs/mon_skullwiz.mdl";
};

entity(string m, vector at, float yaw, float a) VR_HUMO_Prop =
{
    entity e = spawn();
    e.classname = "vr_humo_prop";
    e.vr_humo_prop = 1;
    precache_model(m);
    setmodel(e, m);
    setorigin(e, at);
    e.solid = SOLID_NOT;
    e.movetype = MOVETYPE_NONE;
    e.frame = 0;
    e.angles_y = yaw;
    e.alpha = a;
    return e;
};

void(entity p) VR_HUM_Overlay =
{
    entity e = findflags(world, vr_humo_prop, 1);
    while(e)
    {
        entity next = findflags(e, vr_humo_prop, 1);
        remove(e);
        e = next;
    }
    float k = rint(cvar_hget(cvarh_scratch1));
    if(k < 0 || k >= VR_HUMO_COUNT)
    {
        bprint(sprintf("humoverlay: no pair %g (0..%g)\n", k, VR_HUMO_COUNT - 1));
        return;
    }
    string ours = VR_HUMO_Ours(k);
    string theirs = VR_HUMO_Theirs(k);
    makevectors([0, p.angles_y, 0]);
    vector fw = v_forward;
    vector rt = v_right;
    float far = k == 10 ? 210 : 140; // (the judicator twice our height)
    float gap = k == 10 ? 80 : 50;
    vector mid = p.origin + fw * far;
    float face = vectoyaw(p.origin - mid);
    VR_HUMO_Prop(ours, mid - rt * gap * 1.5, face, 0);
    VR_HUMO_Prop(ours, mid - rt * gap * 0.5, face, 0);
    VR_HUMO_Prop(theirs, mid - rt * gap * 0.5, face, 0.45);
    VR_HUMO_Prop(theirs, mid + rt * gap * 0.5, face, 0);
    VR_HUMO_Prop(ours, mid + rt * gap * 1.5, face + 90, 0);
    VR_HUMO_Prop(theirs, mid + rt * gap * 1.5, face + 90, 0.45);
    bprint(sprintf("humoverlay: pair %g: %s %s\n", k, ours, theirs));
};
#endif
