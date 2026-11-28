#ifdef QVR_TESTS
// vr_attrs_test.qc -- enemy attributes (vr_attrs.qc; ROUND26.md, "elite"):
// `vr_agrunt_test 233` (vrtesthall, god): for each armed kind (grunt, enforcer, knight, death knight) an ordinary one
//   and an elite one side by side, the same weapon: the elite's knobs by the Tunables' factors (fire rate, reaction,
//   accuracy, aggressiveness, melee speed: its swing as much shorter), its steps exactly 1.25x, its health and turning,
//   its look's elite bit over the same weapon colour, its pains shrugged at the tunable's share (the ordinary never);
//   then infected too (both bits, the zombie's row, the elite's factors as they were), then no longer elite (its
//   stats and health back, still infected). A map's "vr_attrs" key taken up at spawn (with its gains); Hard and
//   Nightmare's roll (only with the option, only on Hard or Nightmare, never for the tools' spawns); the arena's wave
//   rule (none before its wave, the same elites for the same wave, about its chance). One frame; ends on
//   `agtest: test 233 over: P passed, F failed`.
// `vr_agrunt_test 234` (his review): an ordinary and an elite grunt (rocket launcher), enforcer (lightning gun), knight
//   and death knight (sword) in a row ahead, unarmoured, held still (the script's vr_agrunt_freeze 1); after VR_ATL_SHOT
//   frames a screenshot and `vr_humlook_dyetest 4` (each elite's clothes as drawn, warmer than its stock skin: its own
//   verdict line `humlook dyetest: P passed, F failed`) and `vr_humlook_glowtest 4 0` (the elite marker: each elite's
//   instance lit, the others' dark); then all armoured (red, every piece): a second screenshot and the glow test with
//   each elite's plates lit; then the hall's light dimmed (lightstyle 0 "b"): a third screenshot (elite2: plain,
//   armoured, dim, for his review); ends the same way (test 234 over).
// VR_Attr_TestOne: the toolgun's elite test's look at each spawn (vr_toolgun.cpp vr_toolgun_armed_elite_test).

float VR_ATL_SHOT = 40; // frames before test 234's screenshot (their spawns' starts, their idle poses)
float VR_ATL_COUNT = 8; // the lineup: an ordinary and an elite of each armed kind
float VR_ATL_ARMOUR = 30; // frames from a phase's change (armoured, dim) to its screenshot (the plates made and drawn, the light)
float vr_atl_frames;
float vr_att_dist; // VR_ATT_Spawn's distance ahead (0: 160) (the lineup's further: all eight in view)
entity vr_atl_ents[8];

// Close: |a - b| within a thousandth of b.
float(float a, float b) VR_ATT_Near = { return fabs(a - b) <= 0.001 * max(1, fabs(b)); };

// A knob's value with `g` as self.
float(entity g, float which) VR_ATT_Knob =
{
    entity oldself = self;
    self = g;
    float v = 0;
    if(which == 0) { v = VR_AGAI_FireRate(); }
    if(which == 1) { v = VR_AGAI_Reaction(); }
    if(which == 2) { v = VR_AGAI_Accuracy(); }
    if(which == 3) { v = VR_AGAI_Aggression(); }
    if(which == 4) { v = VR_AG_Speed(); }
    self = oldself;
    return v;
};

string(float which) VR_ATT_KnobName =
{
    return which == 0 ? "fire rate" : which == 1 ? "reaction" : which == 2 ? "accuracy" : which == 3 ? "aggressiveness" : "steps";
};
// ... and its elite factor (the Tunables').
float(float which) VR_ATT_Tunable =
{
    if(which == 0) { return cvar_hget(cvarh_vr_elite_firerate); }
    if(which == 1) { return cvar_hget(cvarh_vr_elite_reaction); }
    if(which == 2) { return cvar_hget(cvarh_vr_elite_accuracy); }
    if(which == 3) { return cvar_hget(cvarh_vr_elite_aggression); }
    return cvar_hget(cvarh_vr_elite_speed);
};

// Its swing's length (VR_AGP_Length) as its swing would start (its speed as VR_AGP_Begin sets it).
float(entity g) VR_ATT_SwingLength =
{
    g.vr_ag_act = VR_AGP_SWING;
    g.vr_ag_act_speed = min(1, max(0.1, cvar_hget(cvarh_vr_agrunt_melee_speed))) * VR_Attr_Mult(g, VR_ATTR_ST_MELEE);
    float len = VR_AGP_Length(g);
    g.vr_ag_act = 0;
    g.vr_ag_act_speed = 1;
    return len;
};

// An armed humanoid of kind `kind` with `wid`, `side` units to the right of the line ahead, its "vr_attrs" key `keyed`.
entity(float kind, float wid, float side, float keyed) VR_ATT_Spawn =
{
    entity p = vr_agt_player;
    makevectors('0 1 0' * p.angles_y);
    vector at = p.origin + v_forward * (vr_att_dist ? vr_att_dist : 160) + v_right * side;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.weapon = wid;
    self.vr_attrs = keyed;
    VR_AM_Spawn(kind);
    entity g = self;
    self = oldself;
    return g;
};

void(float kind) VR_ATT_Kind =
{
    string kn = kind == 0 ? "grunt" : kind == 1 ? "enforcer" : kind == 2 ? "knight" : "death knight";
    float wid = kind < 2 ? WID_ROCKET_LAUNCHER : WID_SWORD;
    entity n = VR_ATT_Spawn(kind, wid, -50, 0);
    entity e = VR_ATT_Spawn(kind, wid, 50, 0);
    float h0 = e.health;
    float y0 = e.yaw_speed ? e.yaw_speed : 20;
    VR_Attr_Set(e, VR_ATTR_ELITE, TRUE);
    float k;
    float elite0 = VR_ATT_Knob(e, 0);
    for(k = 0; k < 5; k++)
    {
        float a = VR_ATT_Knob(n, k);
        float b = VR_ATT_Knob(e, k);
        VR_AGT_Check(sprintf("elite: %s: %s %.3f, ordinary %.3f: x%.3f (the tunable's %g)", kn, VR_ATT_KnobName(k), b, a,
            a ? b / a : 0, VR_ATT_Tunable(k)), a > 0 && VR_ATT_Near(b, a * VR_ATT_Tunable(k)));
    }
    VR_AGT_Check(sprintf("elite: %s: its steps exactly 1.25x (%.4f)", kn, VR_ATT_Knob(e, 4) / VR_ATT_Knob(n, 4)),
        VR_ATT_Near(VR_ATT_Knob(e, 4), VR_ATT_Knob(n, 4) * 1.25));
    float hk = cvar_hget(cvarh_vr_elite_health);
    VR_AGT_Check(sprintf("elite: %s: health %g from %g (x%g), turning %g from %g", kn, e.health, h0, hk, e.yaw_speed, y0),
        e.health == ceil(h0 * hk) && VR_ATT_Near(e.yaw_speed, y0 * cvar_hget(cvarh_vr_elite_turn)));
    float body = n.vr_hum_look ? n.vr_hum_look : kind * 16;
    VR_AGT_Check(sprintf("elite: %s: its look %g: the elite bit over the same weapon's colour (ordinary %g)", kn,
        e.vr_hum_look, n.vr_hum_look), (e.vr_hum_look & VR_HUM_LOOK_ELITE) && (e.vr_hum_look & 4095) == body &&
        !(n.vr_hum_look & VR_HUM_LOOK_ELITE));
    float ls = VR_ATT_SwingLength(e);
    float ln = VR_ATT_SwingLength(n);
    VR_AGT_Check(sprintf("elite: %s: its swing %.3f s, ordinary %.3f s (x%g quicker)", kn, ls, ln, cvar_hget(cvarh_vr_elite_melee)),
        ln > 0 && VR_ATT_Near(ls * cvar_hget(cvarh_vr_elite_melee), ln));
    float i;
    float shrugs = 0;
    float nshrugs = 0;
    for(i = 0; i < 400; i++)
    {
        shrugs = shrugs + VR_Attr_ShrugsPain(e);
        nshrugs = nshrugs + VR_Attr_ShrugsPain(n);
    }
    float want = 1 - cvar_hget(cvarh_vr_elite_pain);
    VR_AGT_Check(sprintf("elite: %s: pains shrugged %.2f (want %.2f), the ordinary %g", kn, shrugs / 400, want, nshrugs),
        fabs(shrugs / 400 - want) < 0.08 && nshrugs == 0);

    // Infected too: both bits, the zombie's row, the elite's factors as they were.
    VR_Attr_Set(e, VR_ATTR_INFECTED, TRUE);
    VR_AGT_Check(sprintf("elite: %s: infected too: attributes %g, row %g, fire rate %.3f, look %g", kn, VR_Attrs(e),
        e.vr_mg3_infected, VR_ATT_Knob(e, 0), e.vr_hum_look), VR_Attrs(e) == (VR_ATTR_INFECTED | VR_ATTR_ELITE) &&
        e.vr_mg3_infected == MG3_INFECTED_ZOMBIE && VR_ATT_Near(VR_ATT_Knob(e, 0), elite0) &&
        (e.vr_hum_look & VR_HUM_LOOK_ELITE));
    // No longer elite: back as it was, still infected.
    VR_Attr_Set(e, VR_ATTR_ELITE, FALSE);
    VR_AGT_Check(sprintf("elite: %s: no longer elite: health %g (was %g), fire rate %.3f, steps %.3f, look %g, row %g", kn,
        e.health, h0, VR_ATT_Knob(e, 0), VR_ATT_Knob(e, 4), e.vr_hum_look, e.vr_mg3_infected),
        fabs(e.health - h0) <= 1 && VR_ATT_Near(VR_ATT_Knob(e, 0), VR_ATT_Knob(n, 0)) &&
        VR_ATT_Near(VR_ATT_Knob(e, 4), VR_ATT_Knob(n, 4)) && e.vr_hum_look == n.vr_hum_look &&
        e.vr_mg3_infected == MG3_INFECTED_ZOMBIE && VR_Attrs(e) == VR_ATTR_INFECTED);
    VR_AGT_Remove(n);
    VR_AGT_Remove(e);
};

void() VR_ATT_Frame =
{
    float skill0 = skill;
    float spawn0 = cvar_hget(cvarh_vr_elite_spawn);
    skill = 1;
    float kind;
    for(kind = 0; kind < 4; kind++)
    {
        VR_ATT_Kind(kind);
    }

    // A map's key: taken up at spawn, with its gains (an elite grunt keyed: its health, its look).
    entity n = VR_ATT_Spawn(0, WID_SHOTGUN, -50, 0);
    entity e = VR_ATT_Spawn(0, WID_SHOTGUN, 50, VR_ATTR_ELITE);
    VR_AGT_Check(sprintf("elite: a map's vr_attrs 2: attributes %g, health %g (ordinary %g), look %g", VR_Attrs(e), e.health,
        n.health, e.vr_hum_look), VR_Attrs(e) == VR_ATTR_ELITE && e.health == ceil(n.health * cvar_hget(cvarh_vr_elite_health)) &&
        (e.vr_hum_look & VR_HUM_LOOK_ELITE));
    VR_AGT_Check(sprintf("elite: the ordinary one spawned with vr_elite_spawn %g: attributes %g", spawn0, VR_Attrs(n)),
        VR_Attrs(n) == 0);

    // Hard and Nightmare's roll (a sure chance): only with the option, only on Hard or Nightmare, never a tool's spawn.
    cvar_set("vr_elite_spawn", "1");
    skill = 2;
    float hard = VR_Elite_Rolls(n, 1);
    skill = 3;
    float nm = VR_Elite_Rolls(n, 1);
    skill = 1;
    float normal = VR_Elite_Rolls(n, 1);
    skill = 2;
    vr_agr_hold = vr_agr_hold + 1;
    float held = VR_Elite_Rolls(n, 1);
    vr_agr_hold = vr_agr_hold - 1;
    float none = VR_Elite_Rolls(n, 0);
    cvar_set("vr_elite_spawn", "0");
    float off = VR_Elite_Rolls(n, 1);
    VR_AGT_Check(sprintf("elite: the skill's roll: hard %g nightmare %g normal %g, a tool's %g, chance 0 %g, option off %g",
        hard, nm, normal, held, none, off), hard && nm && !normal && !held && !none && !off);
    cvar_set("vr_elite_spawn", ftos(spawn0));
    skill = skill0;
    VR_AGT_Remove(n);
    VR_AGT_Remove(e);

    // The arena's wave rule.
    float i;
    float early = 0, sure = 0, never = 0, share = 0, same = 1;
    for(i = 0; i < 500; i++)
    {
        early = early + VR_ARENA_EliteAt(3, i, 1, 4);
        sure = sure + VR_ARENA_EliteAt(4, i, 1, 4);
        never = never + VR_ARENA_EliteAt(9, i, 0.2, 0) + VR_ARENA_EliteAt(9, i, 0, 4);
        float a = VR_ARENA_EliteAt(6, i, 0.2, 4);
        share = share + a;
        if(a != VR_ARENA_EliteAt(6, i, 0.2, 4)) { same = 0; }
    }
    VR_AGT_Check(sprintf("elite: the arena's rule: before its wave %g, from it at chance 1 %g/500, off %g, at 0.2 %.3f, the same each time %g",
        early, sure, never, share / 500, same), early == 0 && sure == 500 && never == 0 && fabs(share / 500 - 0.2) < 0.06 && same);
    cvar_set("vr_agrunt_test", "0");
};

// Test 234: the lineup (his review).
void() VR_ATL_Frame =
{
    float i;
    if(vr_atl_frames == 0)
    {
        // (ROUND26 "elitedye": no armour, so its plates hide no dye: the skill's roll gave the elite enforcer a green
        // cuirass, helmet and pauldrons, read as "stock green"; the first one clear of the hall's left wall)
        vr_att_dist = 260;
        for(i = 0; i < VR_ATL_COUNT; i++)
        {
            float kind = floor(i / 2);
            float wid = kind == 0 ? WID_ROCKET_LAUNCHER : kind == 1 ? WID_LIGHTNING : WID_SWORD;
            vr_atl_ents[i] = VR_ATT_Spawn(kind, wid, -55 + i * 36, (i & 1) ? VR_ATTR_ELITE : 0);
            VR_AgArmor_Give(vr_atl_ents[i], 0, 0);
        }
        vr_att_dist = 0;
        VR_AGT_Print("elite lineup: ordinary and elite grunt (rockets), enforcer (lightning), knight and death knight (sword), left to right");
    }
    vr_atl_frames = vr_atl_frames + 1;
    float glowFails;
    if(vr_atl_frames == VR_ATL_SHOT)
    {
        localcmd("screenshot\n");
        cvar_set("vr_test_humlook_dye", "-1");
        cvar_set("vr_test_humlook_glow", "-1");
        localcmd(sprintf("vr_humlook_dyetest %g\n", VR_ATL_COUNT / 2)); // (each elite's clothes as drawn: its failures into vr_test_humlook_dye)
        localcmd(sprintf("vr_humlook_glowtest %g 0\n", VR_ATL_COUNT / 2)); // (the elite marker: each elite lit, the rest dark)
    }
    if(vr_atl_frames == VR_ATL_SHOT + 4)
    {
        float made = 0;
        float bare = 0;
        for(i = 0; i < VR_ATL_COUNT; i++)
        {
            made = made + (vr_atl_ents[i] != world && ((vr_atl_ents[i].vr_hum_look & VR_HUM_LOOK_ELITE) != 0) == (i & 1));
            bare = bare + (vr_atl_ents[i] != world && vr_atl_ents[i].vr_ar_mask == 0);
        }
        VR_AGT_Check(sprintf("elite lineup: %g of %g as they should be (every second one an elite), %g unarmoured, the screenshot taken",
            made, VR_ATL_COUNT, bare), made == VR_ATL_COUNT && bare == VR_ATL_COUNT);
        float dyeFails = cvar_hget(cvarh_vr_test_humlook_dye);
        VR_AGT_Check(sprintf("elite lineup: every elite's clothes drawn gold (vr_humlook_dyetest: %g failed; -1 not run)", dyeFails),
            dyeFails == 0);
        glowFails = cvar_hget(cvarh_vr_test_humlook_glow);
        VR_AGT_Check(sprintf("elite lineup: the marker on every elite, none on the others (vr_humlook_glowtest: %g failed; -1 not run)",
            glowFails), glowFails == 0);
        // (elite2: now armoured all over, each pair its tier: green, yellow, red, green; the elites' plates with the gold trim,
        // their faces the tier's colour, the marker on them too)
        for(i = 0; i < VR_ATL_COUNT; i++)
        {
            VR_AgArmor_Give(vr_atl_ents[i], 1 + (floor(i / 2) - 3 * floor(floor(i / 2) / 3)), 1023);
        }
    }
    if(vr_atl_frames == VR_ATL_SHOT + VR_ATL_ARMOUR)
    {
        localcmd("screenshot\n");
        cvar_set("vr_test_humlook_glow", "-1");
        localcmd(sprintf("vr_humlook_glowtest %g %g\n", VR_ATL_COUNT / 2, VR_ATL_COUNT / 2)); // (each elite's plates lit too)
    }
    if(vr_atl_frames == VR_ATL_SHOT + VR_ATL_ARMOUR + 4)
    {
        glowFails = cvar_hget(cvarh_vr_test_humlook_glow);
        VR_AGT_Check(sprintf("elite lineup, armoured: the marker on every elite's body and plates, none on the others' (%g failed)",
            glowFails), glowFails == 0);
        lightstyle(0, "b"); // (dim light: the hall's lights nearly out)
    }
    if(vr_atl_frames == VR_ATL_SHOT + VR_ATL_ARMOUR * 2)
    {
        localcmd("screenshot\n");
    }
    if(vr_atl_frames >= VR_ATL_SHOT + VR_ATL_ARMOUR * 2 + 2)
    {
        lightstyle(0, "m");
        for(i = 0; i < VR_ATL_COUNT; i++)
        {
            VR_AGT_Remove(vr_atl_ents[i]);
            vr_atl_ents[i] = world;
        }
        vr_atl_frames = 0;
        cvar_set("vr_agrunt_test", "0");
    }
};

// The toolgun's elite test (vr_toolgun.cpp): the spawn `e` elite as `xElite`, infected with row `xRow`; removed after.
float(entity e, float xElite, float xRow) VR_Attr_TestOne =
{
    float speed = VR_Attr_Mult(e, VR_ATTR_ST_SPEED);
    float dyed = (e.vr_hum_look & VR_HUM_LOOK_ELITE) != 0;
    float ok = ((VR_Attrs(e) & VR_ATTR_ELITE) != 0) == (xElite != 0) && e.vr_mg3_infected == xRow &&
               dyed == (xElite && e.vr_ag_kind < VR_AM_DGUARD) &&
               VR_ATT_Near(speed, xElite ? cvar_hget(cvarh_vr_elite_speed) : 1);
    if(!ok)
    {
        dprint(sprintf("elite test: %s: attributes %g, row %g, look %g, speed x%g\n", e.classname, VR_Attrs(e), e.vr_mg3_infected,
            e.vr_hum_look, speed));
    }
    VR_AGT_Remove(e);
    return ok;
};
#endif
