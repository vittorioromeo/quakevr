#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_drawnpose_test.qc: test 99 (vr_agrunt_test 99), the drawn pose's one query (ROUND23.md, "Drawn pose: worn
// backpacks and disarm volumes"): a nail pistol, a shotgun (wearing a worn backpack) and a rocket launcher grunt in turn,
// 60 ahead of you, standing ("standing"), toppled by a shove while falling ("falling": an active ragdoll) and lying
// ("lying"). Each state: the disarm volumes (VR_AG_WeaponLine, VR_AGS_HandLine) on the weapon as drawn (its grip within
// 2 units of the drawn weapon's origin, its line within 3 degrees of the drawn forward), the line the weapon model's own
// length (within 10%); on a ragdoll the limb its weapon rides the hand holding it as drawn (not the off hand); the worn
// pack on the chest bone as drawn (its source "live" standing, "ragdoll" after) and its entity within 3 units of it. The
// nail pistol standing: a pellet across its line 3 units past its front end does not count, one across its middle does.
// Ends on `agtest: test 99 over: ...` (run.sh -QuitOn on it); each wait is the state reached (or its give-up, a FAIL).
// ----------------------------------------------------------------------------

float vr_dpt_t0;
float vr_dpt_k;     // the weapon under test (0..2)
float vr_dpt_packs; // vr_worn_backpacks as it was

float(float k) VR_DPT_Wid =
{
    if(k == 0) { return WID_NAILPISTOL; }
    if(k == 1) { return WID_SHOTGUN; }
    return WID_ROCKET_LAUNCHER;
};

// The volumes of `g`'s weapon against the weapon as drawn, in state `what`.
void(entity g, string what) VR_DPT_Measure =
{
    entity p = g.vr_ag_prop;
    string wn = sprintf("drawnpose %s %s", VR_DPT_Wid(vr_dpt_k) == WID_NAILPISTOL ? "nail pistol" :
        VR_DPT_Wid(vr_dpt_k) == WID_SHOTGUN ? "shotgun" : "rocket launcher", what);
    if(!p)
    {
        VR_AGT_Check(sprintf("%s: it holds its weapon", wn), FALSE);
        return;
    }
    vector dfw = weapondrawnpose(p, 0, 31);
    vector dat = weapondrawnpose(p, 0, 30);
    VR_AGT_Check(sprintf("%s: its weapon drawn lately", wn), dfw != '0 0 0');
    VR_AG_WeaponLine(p);
    vector ax = normalize(vr_ag_wb - vr_ag_wa);
    float ang = vlen(ax - normalize(dfw)) * 57.29578; // (degrees: the chord, the angle's to a hundredth below 10)
    vector size = weapondrawnpose(p, 0, 35);
    float len = vlen(vr_ag_wb - vr_ag_wa);
    VR_AGS_HandLine(g, p);
    float grip = vlen(vr_ags_ha - dat);
    VR_AGT_Check(sprintf("%s: volume grip %.2f units off the drawn weapon, axis %.2f deg off it, server's weapon %.2f off",
        wn, grip, ang, vlen(p.origin - dat)), grip < 2 && ang < 3);
    VR_AGT_Check(sprintf("%s: volume %.1f long x %.1f radius, the model %.1f long", wn, len, vr_ag_wr, size_y),
        size_y > 1 && fabs(len - size_y) <= 0.1 * size_y);
    if(isragdoll(g))
    {
        vector hv = weapondrawnpose(g, p.vr_ag_left ? 1 : 0, 36);
        VR_AGT_Check(sprintf("%s: its weapon rides limb %g, the hand holding it %g", wn, p.vr_ag_rb - 1, hv_x),
            hv_x >= 0 && p.vr_ag_rb - 1 == hv_x);
    }
    entity pk = g.vr_wp_pack;
    if(pk)
    {
        vector src = limbpiece(g, 0, 26);
        float want = isragdoll(g) ? 1 : 2;
        vector at = limbpiece(g, 0, 20);
        VR_AGT_Check(sprintf("%s: pack on chest bone %g as drawn (%g, want %g), its entity %.2f units off, %.1f units from the frame's place",
            wn, src_y, src_x, want, vlen(pk.origin - at), vlen(limbpiece(g, 0, 22) - limbpiece(g, 0, 25))),
            src_x == want && vlen(pk.origin - at) < 3);
    }
};

// A pellet across the nail pistol's line at `along` (0 its back end, 1 its front end; past 1: beyond it): counted?
float(entity g, float along) VR_DPT_Pellet =
{
    entity p = g.vr_ag_prop;
    VR_AG_WeaponLine(p);
    vector ax = normalize(vr_ag_wb - vr_ag_wa);
    vector q = vr_ag_wa + (vr_ag_wb - vr_ag_wa) * along;
    if(along > 1)
    {
        q = vr_ag_wb + ax * (along - 1);
    }
    vector side = normalize([ax_y, -ax_x, 0]);
    if(side == '0 0 0')
    {
        side = '0 1 0';
    }
    float n = vr_ags_hits_w + vr_ags_hits_h;
    VR_AGS_Shot(vr_agt_player, q - side * 40, q + side * 40, side, 1, QVR_AGS_PELLET);
    return vr_ags_hits_w + vr_ags_hits_h > n;
};

void() VR_DPT_Frame =
{
    entity pl = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = time - vr_dpt_t0;
    if(vr_agt_step == 0)
    {
        vr_dpt_packs = cvar("vr_worn_backpacks");
        pl.flags = pl.flags | FL_NOTARGET;
        vr_dpt_k = 0;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        cvar_set("vr_worn_backpacks", vr_dpt_k == 1 ? "1" : "0");
        vr_agt_grunt = g = VR_AGT_Spawn(VR_DPT_Wid(vr_dpt_k), 60);
        g.health = 400;
        vr_dpt_t0 = time;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        if(t < 0.4)
        {
            return; // (its arms posed and drawn)
        }
        VR_DPT_Measure(g, "standing");
        if(VR_DPT_Wid(vr_dpt_k) == WID_NAILPISTOL && g.vr_ag_prop)
        {
            VR_AGT_Check("drawnpose nail pistol: a pellet 3 units past its front end not counted", !VR_DPT_Pellet(g, 1 + 3 / max(1, vlen(vr_ag_wb - vr_ag_wa))));
            VR_AGT_Check("drawnpose nail pistol: a pellet across its middle counted", VR_DPT_Pellet(g, 0.5));
        }
        makevectors('0 1 0' * g.angles_y);
        VR_Knockdown_Start(g);
        ragdollshovetopple(g, v_forward * -1, 1, FALSE, 40);
        vr_dpt_t0 = time;
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        if(!isragdoll(g) && t < 1)
        {
            return;
        }
        if(t < 0.15)
        {
            return; // (a few frames into its fall)
        }
        VR_AGT_Check("drawnpose: toppled (a ragdoll)", isragdoll(g));
        VR_DPT_Measure(g, "falling");
        vr_dpt_t0 = time;
        vr_agt_step = 4;
        return;
    }
    if(vr_agt_step == 4)
    {
        if(t < 1.2 && vlen(g.velocity) > 5)
        {
            return; // (lying: down and still, or its give-up)
        }
        if(t < 0.6)
        {
            return;
        }
        VR_DPT_Measure(g, "lying");
        VR_AGT_Remove(g);
        vr_agt_grunt = world;
        vr_dpt_k = vr_dpt_k + 1;
        vr_agt_step = vr_dpt_k < 3 ? 1 : 5;
        return;
    }
    cvar_set("vr_worn_backpacks", ftos(vr_dpt_packs));
    pl.flags = pl.flags - (pl.flags & FL_NOTARGET);
    vr_agt_step = 0;
    cvar_set("vr_agrunt_test", "0");
};
#endif
