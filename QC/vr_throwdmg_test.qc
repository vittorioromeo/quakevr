#ifdef QVR_TESTS
// vr_throwdmg_test.qc -- `vr_embed_test 47`'s second part (throwfix2; his note 2026-11-15 17:58): a thrown sword or dagger hits
// hard but never gibs (combat.qc VR_Gib_Overkill: a thrown pointed blade's overkill is vr_gib_overkill_blade's, whichever way it
// struck); point first it stays in the body (embedded after its damage, never a blunt blow), and a body it kills keeps it: the
// blade rides the corpse (or falls), never left floating where the body stood (vr_axestick.qc VR_AxeStick_Think).
// Each of a grunt, a knight and an ogre (their own health) is thrown at by the sword and the dagger at VR_TD_SPEED (the most a
// throw leaves the hand at, vr_throw_max_speed), at its chest from VR_TD_FROM units: point first, flat (the blade across its
// flight) and handle first. Then VR_TD_WATCH s of its fall: the blade's tip against the body as drawn (modelanchor).
// Called by vr_throwpoint_test.qc after its throws (VR_TD_Start); ends `gibthrow: N passed, M failed` with them.

float VR_TD_FROM = 80;     // units from the chest the throw starts
float VR_TD_WATCH = 2;     // s the body and the blade are watched after the hit
float VR_TD_DRIFT = 24;    // units: a stuck tip further than this from the body as drawn, a few frames: floating
float VR_TD_CASES = 18;    // 3 monsters x 2 blades x 3 ways
float vr_td_case;
float vr_td_speed;         // m/s (vr_throw_max_speed)
float vr_td_hp;            // the body's health before the throw
string vr_td_model;        // its model (gibbed: another, or none)
float vr_td_stuck;         // stuck in the body at the hit
float vr_td_dead;          // the throw killed it
float vr_td_d0;            // the tip's distance from the body's surface as it stuck
float vr_td_drift;         // the most it was off the body since (units)
float vr_td_floatcount;    // frames the blade stayed stuck off the body (VR_TD_DRIFT), or in a body gone or gibbed
float vr_td_gibs;          // gibbed bodies (any case)
float vr_td_killed;        // bodies killed
float vr_td_ptstuck;       // point first throws stuck in the body
float vr_td_ptn;           // point first throws
float vr_td_otherstuck;    // flat or handle first throws stuck (should be none)
float vr_td_blunt;         // flat or handle first throws that dealt damage
float vr_td_floated;       // throws whose blade floated off the body
float vr_td_maxdmg;        // the most damage one throw dealt

float(float c) VR_TD_Kind = { float m = floor(c / 6); return (m == 0) ? 0 : ((m == 1) ? 5 : 1); }; // grunt, knight, ogre
float(float c) VR_TD_Dagger = { return floor(c / 3) & 1; };
float(float c) VR_TD_Way = { return c - floor(c / 3) * 3; }; // 0 point first, 1 flat, 2 handle first

string(float c) VR_TD_Name =
{
    float k = VR_TD_Kind(c);
    float w = VR_TD_Way(c);
    return sprintf("%s, %s %s", (k == 0) ? "grunt" : ((k == 5) ? "knight" : "ogre"), VR_TD_Dagger(c) ? "dagger" : "sword",
        (w == 0) ? "point first" : ((w == 1) ? "flat" : "handle first"));
};

void() VR_TD_Start =
{
    vr_td_case = 0;
    vr_td_speed = cvar_hget(cvarh_vr_throw_max_speed);
    vr_td_gibs = 0;
    vr_td_killed = 0;
    vr_td_ptstuck = 0;
    vr_td_ptn = 0;
    vr_td_otherstuck = 0;
    vr_td_blunt = 0;
    vr_td_floated = 0;
    vr_td_maxdmg = 0;
    cvar_set("vr_knockdown", "0");
    localcmd("tune vr_throw_point_align 0\n"); // (each blade arrives as thrown: point first, flat, handle first)
    VR_ET_To(10);
};

float(entity m) VR_TD_Gibbed =
{
    return (m.model != vr_td_model) || m.vr_gib;
};

// How far the blade's tip is from the body's surface as drawn (a ragdoll's as its parts carry it; -1 no model).
float(entity w, entity m) VR_TD_Off =
{
    vector an = modelanchor(m, weaponmuzzle(w));
    return (an_x < 0) ? -1 : fabs(trace_plane_dist);
};

void() VR_TD_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    if(vr_et_s == 10)
    {
        if(vr_td_case >= VR_TD_CASES)
        {
            VR_Test_Print(sprintf("%s: thrown blades at %g m/s: %g of %g bodies killed, %g gibbed;", vr_test_tag, vr_td_speed, vr_td_killed,
                VR_TD_CASES, vr_td_gibs));
            VR_Test_Print(sprintf(" point first %g of %g in the body, flat or handle first %g stuck, %g blunt blows;", vr_td_ptstuck, vr_td_ptn,
                vr_td_otherstuck, vr_td_blunt));
            VR_Test_Print(sprintf(" %g floated; most damage %.0f\n", vr_td_floated, vr_td_maxdmg));
            VR_Test_Check(vr_td_gibs == 0, sprintf("no thrown sword or dagger gibbed its body (%g gibbed)", vr_td_gibs));
            VR_Test_Check(vr_td_ptstuck == vr_td_ptn, sprintf("every point first throw stayed in the body (%g of %g)", vr_td_ptstuck, vr_td_ptn));
            VR_Test_Check((vr_td_otherstuck == 0) && (vr_td_blunt == VR_TD_CASES - vr_td_ptn),
                sprintf("flat and handle first throws struck as blunt blows (%g of %g; %g stuck)", vr_td_blunt, VR_TD_CASES - vr_td_ptn, vr_td_otherstuck));
            VR_Test_Check(vr_td_killed >= 8, sprintf("hard throws kill (%g of %g bodies)", vr_td_killed, VR_TD_CASES));
            VR_Test_Check(vr_td_floated == 0, sprintf("no blade left floating off the body it killed (%g)", vr_td_floated));
            cvar_set("vr_knockdown", "1");
            localcmd(sprintf("tune vr_throw_point_align %g\n", vr_tp_keep));
            VR_Test_Verdict();
            vr_et_on = 0;
            return;
        }
        VR_ET_Clean();
        VR_EH_ClearGibs();
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        makevectors([0, vr_tp_yaw, 0]);
        vr_et_m = VR_Test_Spawn(p, VR_TD_Kind(vr_td_case), p.origin + v_forward * 160, vr_tp_yaw + 180);
        VR_ET_To(11);
        return;
    }
    entity m = vr_et_m;
    if((vr_et_s == 11) && (t > 0.3)) // (on the floor, its pose taken)
    {
        vr_td_model = m.model;
        vr_td_hp = m.health;
        vector at = limbplace(m, 0, 3) + '0 0 6';
        vector go = at - (p.origin + '0 0 22');
        go_z = 0;
        go = normalize(go);
        vector line = go;
        float way = VR_TD_Way(vr_td_case);
        if(way == 1)
        {
            makevectors([0, vectoyaw(go), 0]);
            line = v_right;
        }
        else if(way == 2)
        {
            line = go * -1;
        }
        vr_et_flags = VR_TD_Dagger(vr_td_case) ? VR_ADMelee_Flags(VR_SWORDKIND_DAGGER) : 0;
        vr_et_w = VR_ET_ThrowSword(WID_SWORD, at - go * VR_TD_FROM, go, line, TRUE);
        vr_et_flags = 0;
        vr_et_w.velocity = go * (vr_td_speed * VRMetersToUnits());
        VR_ET_To(12);
        return;
    }
    entity w = vr_et_w;
    if(vr_et_s == 12)
    {
        if(VR_ET_Resolved(w) || ((w.throwhit == QVR_THROWHIT_HIT) && (t > 0.2)))
        {
            vr_td_stuck = (VR_AxeStick_InMonster(w) && (w.vr_stick_embed == AXESTICK_POINT));
            vr_td_d0 = vr_td_stuck ? VR_TD_Off(w, m) : 0;
            vr_td_drift = 0;
            vr_td_floatcount = 0;
            VR_ET_To(13);
        }
        return;
    }
    if(vr_et_s == 13)
    {
        float gibbed = VR_TD_Gibbed(m);
        if(VR_AxeStick_InMonster(w) && (w.movetype == MOVETYPE_NONE))
        {
            if(gibbed || (m.model == ""))
            {
                vr_td_floatcount = vr_td_floatcount + 1;
            }
            else
            {
                float off = VR_TD_Off(w, m);
                vr_td_drift = max(vr_td_drift, off);
                vr_td_floatcount = vr_td_floatcount + (off > VR_TD_DRIFT);
            }
        }
        if(cvar_hget(cvarh_developer) && (floor(t * 4) != floor((t - frametime) * 4)))
        {
            dprint(sprintf("throwdmg: %.2f s: body %v (model at %v, torso %v), health %g, flags %g;", t, m.origin, limbplace(m, 0, 0),
                limbplace(m, 0, 3), m.health, m.flags & FL_MONSTER));
            dprint(sprintf(" blade %v, kind %g, movetype %g\n", w.origin, w.vr_stick_kind, w.movetype));
        }
        if(t < VR_TD_WATCH)
        {
            return;
        }
        string name = VR_TD_Name(vr_td_case);
        float dealt = vr_td_hp - max(m.health, -100);
        float way = VR_TD_Way(vr_td_case);
        float floated = (vr_td_floatcount > 3);
        vr_td_dead = (m.health <= 0);
        vr_td_maxdmg = max(vr_td_maxdmg, vr_td_hp - m.health);
        vr_td_gibs = vr_td_gibs + gibbed;
        vr_td_killed = vr_td_killed + vr_td_dead;
        vr_td_floated = vr_td_floated + floated;
        if(way == 0)
        {
            vr_td_ptn = vr_td_ptn + 1;
            vr_td_ptstuck = vr_td_ptstuck + vr_td_stuck;
        }
        else
        {
            vr_td_otherstuck = vr_td_otherstuck + VR_AxeStick_IsStuck(w);
            vr_td_blunt = vr_td_blunt + ((m.health < vr_td_hp) && !VR_AxeStick_IsStuck(w));
        }
        VR_Test_Print(sprintf("%s: %s: health %g -> %g (%.0f dealt)%s%s,", vr_test_tag, name, vr_td_hp, m.health, dealt,
            vr_td_dead ? ", killed" : "", gibbed ? ", GIBBED" : ""));
        VR_Test_Print(sprintf(" %s; tip off its torso %.1f, moved %.1f since%s\n",
            vr_td_stuck ? "in the body" : (VR_AxeStick_IsStuck(w) ? "stuck" : "not stuck"), vr_td_d0, vr_td_drift,
            floated ? sprintf(", FLOATING (%g frames stuck in a gibbed or gone body)", vr_td_floatcount) : ""));
        vr_td_case = vr_td_case + 1;
        VR_ET_To(10);
    }
};
#endif
