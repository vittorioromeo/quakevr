#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_dazzle_test.qc -- `vr_agrunt_test 232`: armed humanoids blinded by the flashlight (vr_armedgrunt_dazzle.qc;
// ROUND26.md "flashblind"). The player's own lamp (on and clipped to his head by the run's script: the real beam as his
// client lit it), a shotgun grunt held 120 units down it, its face to the lens:
//   lit:      its dazzle builds, its shielding hand comes up before its eyes, its shots' error and spread grow;
//   lamp off: the dazzle fades over the fade time (not at once), then nothing: its aim as before, exactly;
//   off-axis (25 degrees), behind a solid box, its back to the lens: nothing;
//   each other kind (the enforcer, the knight, the death knight; AD's two with AD): its hand comes up before its eyes.
// Each check `agtest: PASS|FAIL dazzle ...`; ends on `agtest: test 232 over: P passed, F failed`.
// ----------------------------------------------------------------------------

float vr_agdz_t0;      // the step's start
float vr_agdz_until;   // a wait's give-up time (a safety net: FAIL "timed out")
float vr_agdz_base;    // its accuracy undazzled (VR_AGAI_Accuracy)
float vr_agdz_kind;    // the kinds' step: the row now
float vr_agdz_keepkind; // vr_agrunt_test_kind before
float vr_agdz_mid;     // the fade's dazzle 0.4 s in
entity vr_agdz_box;    // the blocker

// Its accuracy now (the layer's shots divide their error by it).
float(entity g) VR_AGDZ_Acc =
{
    entity oldself = self;
    self = g;
    float a = VR_AGAI_Accuracy();
    self = oldself;
    return a;
};

// Held `dist` down the beam, `ang` degrees off its axis (about the upright), its face to the lens (or its back, `away`);
// its own AI held (its think put off, no enemy).
void(entity g, float ang, float dist, float away) VR_AGDZ_Hold =
{
    vector lens = flashlightbeam(vr_agt_player, 0);
    vector dir = flashlightbeam(vr_agt_player, 1);
    if(dir == '0 0 0')
    {
        return;
    }
    vector a = vectoangles(dir);
    a_x = -a_x;
    a_y = a_y + ang;
    makevectors(a);
    vector eye = lens + v_forward * dist;
    setorigin(g, eye - VR_AG_DazzleEye(g) + g.origin);
    g.angles_y = vectoyaw(lens - eye) + (away ? 180 : 0);
    g.velocity = '0 0 0';
    g.enemy = world;
    g.nextthink = time + 0.5;
};

// The player's lamp is on.
float() VR_AGDZ_LampOn = { vector b = flashlightbeam(vr_agt_player, 2); return b_x >= 1; };

// Its weapon drawn (its spawn finished) and the lamp on.
float(entity g) VR_AGDZ_Ready =
{
    return g.takedamage && g.vr_ag_prop && g.vr_ag_prop.vr_ag_was_n >= 1 && weapondrawnpose(g.vr_ag_prop, 0, 31) != '0 0 0' &&
           VR_AGDZ_LampOn();
};

// The shielding hand before its eyes: within `most` units of them, on the lens's side.
float(entity g, float most) VR_AGDZ_HandUp =
{
    vector eye = VR_AG_DazzleEye(g);
    vector toLens = normalize(flashlightbeam(vr_agt_player, 0) - eye);
    return vlen(g.vr_ag_shield_at - eye) < most && (g.vr_ag_shield_at - eye) * toLens > 1;
};

void(float step) VR_AGDZ_Step = { vr_agt_step = step; vr_agdz_t0 = time; vr_agdz_until = time + 6; };

// The kinds' weapons: a gun for those that carry one, the sword for the knights.
float(float k) VR_AGDZ_KindWid =
{
    if(k == VR_AM_ENFORCER) { return WID_NAILGUN; }
    if(k == VR_AM_DCROSSBOW) { return WID_ADCROSSBOW; }
    if(k == VR_AM_GRUNT) { return WID_SHOTGUN; }
    return WID_SWORD;
};

void() VR_AGDZ_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = time - vr_agdz_t0;
    if(vr_agt_step == 0)
    {
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_Clear();
        vr_agdz_keepkind = cvar_hget(cvarh_vr_agrunt_test_kind);
        cvar_set("vr_agrunt_test_kind", "0");
        vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 120);
        if(!VR_AGDZ_LampOn())
        {
            // (its lamp taken, clipped on his head and switched on, as the run scripts do: step 1 waits for it lit)
            localcmd("vr_flashlight 1;wait20;vr_flashlight_clip_head right;wait10;vr_flashlight_toggle\n");
        }
        VR_AGDZ_Step(1);
        return;
    }
    if(vr_agt_step == 1)
    {
        // (its spawn finished, the lamp on: held off the beam until undazzled (its spawn spot may be in the beam), its
        // aim undazzled read)
        if(g.takedamage)
        {
            VR_AGDZ_Hold(g, 60, 120, FALSE);
        }
        if(!VR_AGDZ_Ready(g) || g.vr_ag_dazzle > 0 || g.vr_ag_shieldw > 0)
        {
            if(time > vr_agdz_until)
            {
                VR_AGT_Check(sprintf("dazzle: timed out (weapon drawn %g, lamp %g, dazzle %g)", g.vr_ag_prop != world,
                    VR_AGDZ_LampOn(), g.vr_ag_dazzle), FALSE);
                VR_AGDZ_Step(9);
            }
            return;
        }
        vr_agdz_base = VR_AGDZ_Acc(g);
        VR_AGT_Check(sprintf("dazzle: undazzled off the beam (two-handed gun %g, accuracy %g)",
            VR_AG_GunTwoHands(g, g.vr_ag_prop), vr_agdz_base), vr_agdz_base > 0);
        VR_AGDZ_Step(2);
        return;
    }
    if(vr_agt_step == 2)
    {
        // Lit on its face: dazzled, its hand up, its aim worse (and its two-handed gun in one hand: worse still).
        VR_AGDZ_Hold(g, 0, 120, FALSE);
        if(t < 1.2)
        {
            return;
        }
        float acc = VR_AGDZ_Acc(g);
        float spread = cvar_hget(cvarh_vr_agrunt_dazzle_spread);
        float want = 1 + spread * 0.95 + (VR_AG_GunTwoHands(g, g.vr_ag_prop) ? 0.9 : 0);
        VR_AGT_Check(sprintf("dazzle: lit on its face: dazzle %.2f (lamp %.2f), hand %.2f", g.vr_ag_dazzle, g.vr_ag_dazzle_lit,
            g.vr_ag_shieldw), g.vr_ag_dazzle > 0.95 && g.vr_ag_shieldw > 0.95);
        VR_AGT_Check(sprintf("dazzle: its hand before its eyes (%.1f from them)", vlen(g.vr_ag_shield_at - VR_AG_DazzleEye(g))),
            VR_AGDZ_HandUp(g, 10));
        VR_AGT_Check(sprintf("dazzle: its aim error and spread x%.2f (at least x%.2f)", vr_agdz_base / acc, want),
            vr_agdz_base / acc >= want);
        if(cvar_hget(cvarh_scratch1) == 232)
        {
            localcmd("screenshot\n"); // (scratch1 232: his picture of a shielding grunt, the test ends here)
            VR_AGDZ_Step(9);
            return;
        }
        localcmd("vr_flashlight_toggle\n");
        VR_AGDZ_Step(3);
        return;
    }
    if(vr_agt_step == 3)
    {
        // The lamp off: it fades over the fade time, then nothing.
        VR_AGDZ_Hold(g, 0, 120, FALSE);
        if(VR_AGDZ_LampOn())
        {
            vr_agdz_t0 = time;
            if(time > vr_agdz_until)
            {
                VR_AGT_Check("dazzle: timed out (the lamp stayed on)", FALSE);
                VR_AGDZ_Step(9);
            }
            return;
        }
        if(t >= 0.4 && !vr_agdz_mid)
        {
            vr_agdz_mid = g.vr_ag_dazzle + 0.001;
        }
        if(t < cvar_hget(cvarh_vr_agrunt_dazzle_fade) + 0.5)
        {
            return;
        }
        VR_AGT_Check(sprintf("dazzle: lamp off: fading (%.2f 0.4 s on), then gone (dazzle %g, hand %g)", vr_agdz_mid - 0.001,
            g.vr_ag_dazzle, g.vr_ag_shieldw), vr_agdz_mid > 0.3 && vr_agdz_mid < 0.9 && g.vr_ag_dazzle == 0 && g.vr_ag_shieldw == 0);
        VR_AGT_Check(sprintf("dazzle: its aim as before (accuracy %g, was %g)", VR_AGDZ_Acc(g), vr_agdz_base),
            VR_AGDZ_Acc(g) == vr_agdz_base);
        localcmd("vr_flashlight_toggle\n");
        VR_AGDZ_Step(4);
        return;
    }
    if(vr_agt_step == 4 || vr_agt_step == 5 || vr_agt_step == 6)
    {
        // The lamp on again: 25 degrees off its axis; then behind a solid box; then its back to the lens: nothing.
        float off = vr_agt_step == 4 ? 25 : 0;
        VR_AGDZ_Hold(g, off, 120, vr_agt_step == 6);
        if(!VR_AGDZ_LampOn())
        {
            vr_agdz_t0 = time;
            if(time > vr_agdz_until)
            {
                VR_AGT_Check("dazzle: timed out (the lamp stayed off)", FALSE);
                VR_AGDZ_Step(9);
            }
            return;
        }
        if(vr_agt_step == 5 && !vr_agdz_box)
        {
            vr_agdz_box = spawn();
            vr_agdz_box.classname = "vr_agdz_box";
            vr_agdz_box.solid = SOLID_BBOX;
            vr_agdz_box.movetype = MOVETYPE_NONE;
            setorigin(vr_agdz_box, flashlightbeam(p, 0) + flashlightbeam(p, 1) * 60);
            setsize(vr_agdz_box, '-12 -12 -12', '12 12 12');
        }
        if(t < 1)
        {
            return;
        }
        string what = vr_agt_step == 4 ? "25 degrees off the beam" : vr_agt_step == 5 ? "behind a solid box" : "its back to the lens";
        VR_AGT_Check(sprintf("dazzle: %s: nothing (dazzle %g, lamp %g, hand %g)", what, g.vr_ag_dazzle, g.vr_ag_dazzle_lit,
            g.vr_ag_shieldw), g.vr_ag_dazzle == 0 && g.vr_ag_dazzle_lit == 0 && g.vr_ag_shieldw == 0);
        if(vr_agdz_box)
        {
            remove(vr_agdz_box);
            vr_agdz_box = world;
        }
        VR_AGDZ_Step(vr_agt_step + 1);
        if(vr_agt_step == 7)
        {
            vr_agdz_kind = VR_AM_ENFORCER;
            VR_AGT_Remove(g);
            cvar_set("vr_agrunt_test_kind", ftos(vr_agdz_kind));
            vr_agt_grunt = VR_AGT_Spawn(VR_AGDZ_KindWid(vr_agdz_kind), 120);
        }
        return;
    }
    if(vr_agt_step == 7)
    {
        // Each kind lit: its hand comes up before its eyes (the grunt's place fitted to its body).
        if(!VR_AGDZ_Ready(g))
        {
            vr_agdz_t0 = time;
            if(time > vr_agdz_until)
            {
                VR_AGT_Check(sprintf("dazzle: kind %g: timed out (weapon drawn %g)", vr_agdz_kind, g.vr_ag_prop != world), FALSE);
                VR_AGDZ_Step(9);
            }
            return;
        }
        VR_AGDZ_Hold(g, 0, 120, FALSE);
        if(t < 1.2)
        {
            return;
        }
        VR_AM_Row(vr_agdz_kind);
        VR_AGT_Check(sprintf("dazzle: %s lit: dazzle %.2f, hand %.2f, %.1f from its eyes", vr_am_spawnname, g.vr_ag_dazzle,
            g.vr_ag_shieldw, vlen(g.vr_ag_shield_at - VR_AG_DazzleEye(g))), g.vr_ag_shieldw > 0.95 && VR_AGDZ_HandUp(g, 14));
        VR_AGT_Remove(g);
        vr_agt_grunt = world;
        vr_agdz_kind = vr_agdz_kind + 1;
        float last = cvar_hget(cvarh_vr_ad_mounted) ? VR_AM_ROWS : VR_AM_COUNT;
        if(vr_agdz_kind >= last)
        {
            VR_AGDZ_Step(9);
            return;
        }
        cvar_set("vr_agrunt_test_kind", ftos(vr_agdz_kind));
        vr_agt_grunt = VR_AGT_Spawn(VR_AGDZ_KindWid(vr_agdz_kind), 120);
        VR_AGDZ_Step(7);
        return;
    }
    // (step 9) the end
    if(vr_agdz_box)
    {
        remove(vr_agdz_box);
        vr_agdz_box = world;
    }
    if(g)
    {
        VR_AGT_Remove(g);
    }
    vr_agt_grunt = world;
    cvar_set("vr_agrunt_test_kind", ftos(vr_agdz_keepkind));
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};
#endif
