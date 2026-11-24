#ifdef QVR_TESTS
// vr_armedgrunt_spear_test.qc -- an armed grunt with the spear (VR_AG_CODE_SPEAR 106, the toolgun's Armed Grunt: Spear):
// held as a spear, not a sword (spearfix; vr_armedgrunt_pose.qc VR_AGP_Pole: any pole weapon by its slot's data).
// `vr_agrunt_test 230` (vrtesthall, god):
//   0 posed: 130 units ahead, ready, its AI held, its enemy a stand-in 80 units to its right as you see it (so a
//     screenshot shows the hold side on): the spear in its hand, held two-handed, its head at its enemy (within VR_AGSP_AIM); the
//     engine's vr_agholds_report prints `agpole <ent>: ... both on the shaft` (each hand's grip within 3.5 units of the
//     shaft: the script checks it); an eyeshot (vr_eyeshot 1);
//   1 hunting you: its thrust (an eyeshot at the blow) strikes and hurts you.
// Each `agtest: PASS|FAIL agspear ...`; ends on `agtest: test 230 over: P passed, F failed`.

float VR_AGSP_AIM = 25;  // degrees its tip's way may be off the way to you, ready
float vr_agsp_case;
float vr_agsp_s0;        // its strikes at case 1's start
float vr_agsp_d0;        // the damage armed humanoids dealt you then
float vr_agsp_shot;      // case 1: the blow's screenshot taken
entity vr_agsp_tgt;      // case 0: its enemy, a stand-in on its right as you see it

void() VR_AGSP_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        vr_agt_grunt = g = VR_AGT_Spawn(VR_AG_CODE_SPEAR, 130);
        // (its enemy for the pose a stand-in on its right as you see it: the hold side on in your view)
        if(!vr_agsp_tgt)
        {
            vr_agsp_tgt = spawn();
        }
        makevectors('0 1 0' * p.angles_y);
        setorigin(vr_agsp_tgt, g.origin + v_right * 80);
        setsize(vr_agsp_tgt, '-8 -8 -24', '8 8 32');
        vr_agsp_case = 0;
        vr_agsp_shot = 0;
        vr_agt_until = time + 8;
        vr_agt_step = 1;
        return;
    }
    if(!g || g.health <= 0)
    {
        VR_AGT_Check("agspear: the grunt is gone", FALSE);
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    entity w = g.vr_ag_prop;
    if(vr_agsp_case == 0)
    {
        // (its AI held, the stand-in its enemy: its pose readies, facing it)
        g.enemy = vr_agsp_tgt;
        g.nextthink = time + 1;
        g.angles_y = vectoyaw(vr_agsp_tgt.origin - g.origin);
        if(w && g.vr_agp_rdy >= 0.99 && !g.vr_ag_act && vr_agt_step == 1)
        {
            vector tip = normalize(weaponmuzzle(w) - w.origin);
            vector to = normalize(vr_agsp_tgt.origin + '0 0 4' - w.origin);
            float off = atan2(vlen(VR_Cross(tip, to)), tip * to) * (180 / 3.14159265);
            VR_AGT_Check(sprintf("agspear: it holds the spear (weapon %g, kind %g, a pole weapon %g)", w.weapon,
                VR_ADMelee_Kind(w.weaponflags), VR_AGP_Pole(w)), w.weapon == WID_SWORD &&
                VR_ADMelee_Kind(w.weaponflags) == VR_SWORDKIND_SPEAR && VR_AGP_Pole(w));
            VR_AGT_Check(sprintf("agspear: held in both hands (two-handed %g)", VR_AGP_TwoHanded(w)), VR_AGP_TwoHanded(w));
            VR_AGT_Check(sprintf("agspear: ready, its head at its enemy (%.1f deg off, at most %g)", off, VR_AGSP_AIM), off <= VR_AGSP_AIM);
            localcmd("vr_agholds_report\nvr_eyeshot 1\n"); // (the eyes' view: <gamedir>/eyeshots)
            vr_agt_step = 2;
            vr_agt_until = time + 0.5; // (the report and the screenshot's frames: the pose held)
            return;
        }
        if(vr_agt_step == 2 && time >= vr_agt_until)
        {
            vr_agsp_case = 1;
            vr_agt_step = 1;
            g.enemy = p;
            vr_agsp_s0 = g.vr_ag_strikes;
            vr_agsp_d0 = vr_ag_player_dmg;
            vr_agt_until = time + 10;
            VR_AGT_Hunt(g);
            return;
        }
        if(time >= vr_agt_until)
        {
            VR_AGT_Check(sprintf("agspear: timed out readying (prop %g, ready %.2f, act %g)", w != world, g.vr_agp_rdy, g.vr_ag_act), FALSE);
            cvar_set("vr_agrunt_test", "0");
        }
        return;
    }
    if(!vr_agsp_shot && g.vr_ag_act == VR_AGP_SWING && g.vr_ag_act_hit)
    {
        vr_agsp_shot = 1;
        localcmd("vr_eyeshot 1\n");
    }
    if(g.vr_ag_strikes > vr_agsp_s0)
    {
        float dmg = vr_ag_player_dmg - vr_agsp_d0;
        VR_AGT_Check(sprintf("agspear: its thrust hurts you (%.0f damage, %g strikes)", dmg, g.vr_ag_strikes - vr_agsp_s0), dmg > 0);
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    if(time >= vr_agt_until)
    {
        VR_AGT_Check("agspear: timed out: no blow struck", FALSE);
        cvar_set("vr_agrunt_test", "0");
    }
};
#endif
