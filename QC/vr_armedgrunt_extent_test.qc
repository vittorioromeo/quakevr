#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_extent_test.qc -- `vr_agrunt_test 214`: a held weapon's drawn extent (agweaponextent, held::drawnExtent:
// the weapon line VR_AG_WeaponLine lays for shots, parries and strikes) follows its model's transform tuned mid-map
// (vr_gunmodelscale here, as the Weapon Studio or a vr_wofs change would). A fresh sword grunt 60 units ahead; its
// sword's front end read, vr_gunmodelscale times 1.5, read again (its front 1.5 times as far), put back, read again (as
// at first). Each check `agtest: PASS|FAIL extent ...`; ends on `agtest: test 214 over: P passed, F failed`.
// ----------------------------------------------------------------------------

float vr_agxt_keep;  // vr_gunmodelscale before
float vr_agxt_front; // the front end's distance ahead at first
float vr_agxt_until; // the set-up's give-up time (a safety net: FAIL "timed out")

// Its weapon's front end, along the weapon (agweaponextent's vr_ag_wx_front); 0: no drawn surface.
float(entity w) VR_AGXT_Front =
{
    vector size = agweaponextent(w);
    return size_y > 1 ? vr_ag_wx_front_x : 0;
};

void() VR_AGXT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_Clear();
        vr_agxt_keep = cvar_hget(cvarh_vr_gunmodelscale);
        vr_agt_grunt = VR_AGT_Spawn(WID_SWORD, 60);
        vr_agxt_until = time + 5;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(!(g.takedamage && g.vr_ag_prop && g.vr_ag_prop.vr_ag_was_n >= 1 && weapondrawnpose(g.vr_ag_prop, 0, 31) != '0 0 0'))
        {
            if(time > vr_agxt_until)
            {
                VR_AGT_Check("extent: timed out (no sword drawn)", FALSE);
                vr_agt_step = 4;
            }
            return;
        }
        vr_agxt_front = VR_AGXT_Front(g.vr_ag_prop);
        VR_AGT_Check(sprintf("extent: the sword's front end %g units along it", vr_agxt_front), vr_agxt_front > 4);
        cvar_set("vr_gunmodelscale", ftos(vr_agxt_keep * 1.5));
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        float f = VR_AGXT_Front(g.vr_ag_prop);
        VR_AGT_Check(sprintf("extent: vr_gunmodelscale x1.5 mid-map, its front end %g (was %g; expected about %g)", f,
            vr_agxt_front, vr_agxt_front * 1.5), fabs(f - vr_agxt_front * 1.5) < vr_agxt_front * 0.1);
        cvar_set("vr_gunmodelscale", ftos(vr_agxt_keep));
        vr_agt_step = 3;
        return;
    }
    if(vr_agt_step == 3)
    {
        float b = VR_AGXT_Front(g.vr_ag_prop);
        VR_AGT_Check(sprintf("extent: vr_gunmodelscale put back, its front end %g (expected %g)", b, vr_agxt_front),
            fabs(b - vr_agxt_front) < 0.01);
        vr_agt_step = 4;
        return;
    }
    // (step 4) the end
    cvar_set("vr_gunmodelscale", ftos(vr_agxt_keep));
    if(g)
    {
        VR_AGT_Remove(g);
    }
    vr_agt_grunt = world;
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};
#endif
