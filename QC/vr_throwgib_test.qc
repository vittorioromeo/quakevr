#ifdef QVR_TESTS
// vr_throwgib_test.qc -- a blade thrown blade first kills without gibbing, deeply in the body (his 2026-11-12 notes: a running
// sword throw point first at a grunt gibbed it; ROUND25.md, "Thrown blades don't gib"). By the embedded blades' test
// machinery (vr_embed_test.qc: VR_ET_ThrowSword). `vr_embed_test 41` (map vrtesthall, god, notarget, vr_embed 1), a grunt
// with its 30 health ahead, the blade thrown at its chest at VR_TG_SPEED (a running throw):
//   0 the sword point first: it dies whole (no gibs), the sword stays in the body, deep (vr_embed_throw_kill_depth);
//   1 the dagger point first: the same;
//   2 the sword point first under Quad: gibbed, as before;
//   3 the sword handle first: no blade in, its blow (if any) never counted blade first (its overkill unchanged);
//   4 vr_embed 0 (no blade sticks: its box's blow alone, VR_Throw_PointFirst; the touch called at the chest at the throw's
//     speed), the sword point first: it dies whole.
// Ends `throwgib: N passed, M failed`.

float VR_TG_SPEED = 20; // m/s: a hard throw while running (VR_ET_SPEED's 14 plus a run)
float VR_TG_CASES = 5;
float vr_tg_hp;         // the grunt's health before the throw
float vr_tg_dead;       // whether the throw killed it
float vr_tg_count0;     // vr_throw_bladein_count before the throw
float vr_tg_pf;         // VR_Throw_PointFirst as it was thrown
vector vr_tg_at;        // case 4: where its box meets the body (at the chest)
string vr_tg_model;     // the grunt's model (gibbed: its head's, or none)

float() VR_TG_Flags = { return (vr_et_case == 1) ? VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) : 0; };

void() VR_TG_Begin =
{
    VR_Test_BeginFlags("throwgib", VR_TEST_LOUD);
    cvar_set("vr_knockdown", "0");
    cvar_set("vr_hit_stagger", "0");
    vr_eh_home = VR_Player().origin;
    vr_et_on = 41;
    vr_et_case = 0;
    VR_ET_To(0);
};

// Whether the body `m` was gibbed: its model no longer a grunt's (its head now, or freed).
float(entity m) VR_TG_Gibbed =
{
    return m.model != vr_tg_model || m.vr_gib;
};

void() VR_TG_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    if(t > VR_ET_GIVEUP)
    {
        VR_Test_Check(FALSE, sprintf("timed out: case %g state %g", vr_et_case, vr_et_s));
        p.super_damage_finished = 0;
        cvar_set("vr_embed", "1");
        VR_ET_NextCase();
        return;
    }
    if(vr_et_case >= VR_TG_CASES)
    {
        VR_Test_Verdict();
        vr_et_on = 0;
        return;
    }
    string name = (vr_et_case == 1) ? "dagger" : "sword";
    if(vr_et_case == 2) { name = "sword under Quad"; }
    if(vr_et_case == 3) { name = "sword handle first"; }
    if(vr_et_case == 4) { name = "sword, vr_embed 0"; }
    if(vr_et_s == 0)
    {
        VR_EH_ClearGibs(); // (the last case's gibs would take the throw)
        for(entity sg = find(world, classname, "vr_smallgib"); sg; )
        {
            entity sgn = find(sg, classname, "vr_smallgib");
            remove(sg);
            sg = sgn;
        }
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        p.super_damage_finished = (vr_et_case == 2) ? (time + 30) : 0;
        cvar_set("vr_embed", (vr_et_case == 4) ? "0" : "1");
        vr_tg_count0 = vr_throw_bladein_count;
        vr_et_m = VR_ET_Grunt(110);
        vr_et_m.health = 30; // (a grunt's own)
        vr_tg_model = vr_et_m.model;
        vr_tg_hp = vr_et_m.health;
        vr_et_flags = VR_TG_Flags();
        vector at = VR_ET_Chest(vr_et_m);
        vector go = at - (p.origin + '0 0 22');
        go_z = 0;
        go = normalize(go);
        vr_et_w = VR_ET_ThrowSword(WID_SWORD, at - go * 80, go, (vr_et_case == 3) ? (go * -1) : go, TRUE);
        vr_et_flags = 0;
        vr_et_w.velocity = go * (VR_TG_SPEED * VRMetersToUnits());
        vr_tg_pf = VR_Throw_PointFirst(vr_et_w, vr_et_m);
        vr_tg_at = at - go * 8;
        VR_ET_To(1);
        return;
    }
    if(vr_et_s == 1)
    {
        if(vr_et_case == 4 && vr_et_w.throwhit == QVR_THROWHIT_NEVER_HIT && vr_et_m.takedamage)
        {
            // Its box meeting the body's (Box3D bounces a sword that doesn't stick off the body before Quake's touch: here
            // the touch itself, at the chest, at the throw's speed), once the body takes damage (its start's next frame).
            setorigin(vr_et_w, vr_tg_at);
            entity oldself = self;
            entity oldother = other;
            self = vr_et_w;
            other = vr_et_m;
            forcegrabbable_touch();
            self = oldself;
            other = oldother;
        }
        // The throw over: stuck, or it struck and fell (handle first, gibbed), or its time out (1.2 s).
        if(VR_ET_Resolved(vr_et_w) || (vr_et_w.throwhit == QVR_THROWHIT_HIT && t > 0.3))
        {
            vr_tg_dead = ((vr_et_m.health <= 0) || VR_TG_Gibbed(vr_et_m));
            VR_ET_To(2);
        }
        return;
    }
    // ---- 2: the body down (its death's fall, its ragdoll: half a second of its frames)
    if(vr_et_s == 2 && t >= 0.5)
    {
        entity m = vr_et_m;
        entity w = vr_et_w;
        float in = w.vr_stick_depth + w.vr_stick_slide;
        float blade = max(1, w.vr_stick_bladelen);
        float counted = vr_throw_bladein_count - vr_tg_count0;
        if(vr_et_case != 3)
        {
            VR_Test_Check(vr_tg_dead, sprintf("%s at %g m/s: the throw killed it (health %g of %g)", name, VR_TG_SPEED, m.health, vr_tg_hp));
        }
        if(vr_et_case <= 1)
        {
            VR_Test_Check(!VR_TG_Gibbed(m), sprintf("%s: it dies whole, no gibs (model %s, health %g)", name, m.model, m.health));
            VR_Test_Check(VR_AxeStick_InMonster(w) && w.vr_stick_embed == AXESTICK_POINT,
                sprintf("%s: the blade still in the body (kind %g, how %g)", name, w.vr_stick_kind, w.vr_stick_embed));
            VR_Test_Check(counted >= 1 && vr_tg_pf, sprintf("%s: its blow counted blade first (%g), point first by its box too (%g)", name,
                counted, vr_tg_pf));
            float want = blade * max(0, cvar_hget(cvarh_vr_embed_throw_kill_depth)) * 0.01;
            VR_Test_Check(in >= want - 1, sprintf("%s: deep in it: %.1f units of the blade's %.1f (%.0f%%; a kill's %g%%)", name, in,
                blade, in / blade * 100, cvar_hget(cvarh_vr_embed_throw_kill_depth)));
        }
        else if(vr_et_case == 2)
        {
            VR_Test_Check(VR_TG_Gibbed(m), sprintf("%s: gibbed, as before (model %s, health %g)", name, m.model, m.health));
        }
        else if(vr_et_case == 3)
        {
            VR_Test_Check(!VR_AxeStick_IsStuck(w), sprintf("%s: no blade in it (kind %g)", name, w.vr_stick_kind));
            VR_Test_Check(counted == 0, sprintf("%s: its blow not counted blade first (%g; health %g of %g)", name, counted, m.health,
                vr_tg_hp));
            VR_Test_Check(!vr_tg_pf, sprintf("%s: not point first by its box (VR_Throw_PointFirst %g)", name, vr_tg_pf));
        }
        else
        {
            VR_Test_Check(!VR_AxeStick_IsStuck(w), sprintf("%s: no blade sticks (kind %g)", name, w.vr_stick_kind));
            VR_Test_Check(counted >= 1, sprintf("%s: its box's blow counted point first (%g)", name, counted));
            VR_Test_Check(!VR_TG_Gibbed(m), sprintf("%s: it dies whole, no gibs (model %s, health %g)", name, m.model, m.health));
            cvar_set("vr_embed", "1");
        }
        p.super_damage_finished = 0;
        VR_ET_NextCase();
    }
};
#endif
