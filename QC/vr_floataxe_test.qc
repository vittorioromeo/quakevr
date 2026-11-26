#ifdef QVR_TESTS
// vr_floataxe_test.qc -- `vr_embed_test 83` (floataxe, his words 2026-11-26, flat: "I killed a zombie ... by throwing an axe to his
// head, which popped it. The axe remained floating in mid air after the zombie was dead"). A thrown blade's own blow that cut off the
// head it went into let go of it (VR_AxeStick_CutDone: it fell), and then the stick's set-up held it still where the head was
// (vr_axestick.qc VR_AxeStick_Stuck). Map vrtesthall, god, notarget. An axe thrown edge first at VR_ET_SPEED, then VR_FA_WATCH s:
//   0 at a zombie's head (health 1, as decap_test.sh's `thrown`): its head off, the axe falls;
//   1 at a grunt's head (health 1): beheaded, the axe falls;
//   2 into a grunt's chest (no damage): stays in; the grunt gibbed whole: the axe falls;
//   3 into a grunt's chest: stays in; the grunt removed (a corpse gone): the axe falls;
//   4 into a zombie's chest: stays in; the zombie gibbed (its head gib the same entity, another model): the axe falls.
// Floating: held still (MOVETYPE_NONE) while stuck in nothing, or stuck in a body gone or another model. Ends
// `floataxe: N passed, M failed`.

float VR_FA_FROM = 56;   // units from the target the throw starts
float VR_FA_WATCH = 1;   // s watched after the head is hit or the body goes
float VR_FA_CASES = 5;
float VR_FA_GIVEUP = 3;  // s a state may wait (the safety net: a FAIL "timed out")
float vr_fa_case;
float vr_fa_z0;          // the blade's height as its host let go of it (or as it struck)
float vr_fa_model;       // the host's modelindex as the blade stuck
float vr_fa_floatcount;  // frames the blade floated
float vr_fa_keepcut;     // vr_decap_thrown as it was

float(float c) VR_FA_Kind = { return (c == 0 || c == 4) ? 2 : 0; }; // zombie, grunt
float(float c) VR_FA_AtHead = { return c <= 1; };

string(float c) VR_FA_Name =
{
    if(c == 0) { return "an axe at a zombie's head"; }
    if(c == 1) { return "an axe at a grunt's head"; }
    if(c == 2) { return "an axe in a grunt's chest, the grunt gibbed"; }
    if(c == 3) { return "an axe in a grunt's chest, the grunt removed"; }
    return "an axe in a zombie's chest, the zombie gibbed";
};

void() VR_FA_Begin =
{
    VR_Test_BeginFlags("floataxe", VR_TEST_LOUD);
    vr_eh_home = VR_Player().origin;
    vr_fa_case = 0;
    cvar_set("vr_knockdown", "0");
    vr_et_on = 83;
    vr_et_case = 0;
    VR_ET_To(0);
};

// Whether the blade `w` floats now: held still and stuck in nothing, or stuck in what is gone or another model now.
float(entity w) VR_FA_Floats =
{
    if(w.movetype != MOVETYPE_NONE)
    {
        return FALSE;
    }
    if(!VR_AxeStick_IsStuck(w))
    {
        return TRUE;
    }
    entity h = w.vr_stick_ent;
    return VR_AxeStick_InMonster(w) && (h == world || h.model == "" || h.modelindex != vr_fa_model);
};

// The head's point of `m` (its head bone's surface; no rig: near its box's top).
vector(entity m) VR_FA_Head =
{
    float hb = ragdolllimbs(m, 3);
    if(hb >= 0)
    {
        return limbplace(m, hb, 3) - '0 0 3'; // (its surface's point is its top: a zombie's blade would pass over, its handle striking)
    }
    return m.origin + '0 0 1' * (m.maxs_z - 8);
};

void() VR_FA_Frame =
{
    entity p = self;
    float t = time - vr_et_t0;
    entity m = vr_et_m;
    entity w = vr_et_w;
    if(vr_et_s == 0)
    {
        if(vr_fa_case >= VR_FA_CASES)
        {
            cvar_set("vr_knockdown", "1");
            VR_ET_Clean();
            VR_EH_ClearGibs();
            VR_Test_Verdict();
            vr_et_on = 0;
            return;
        }
        VR_ET_Clean();
        VR_EH_ClearGibs();
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        makevectors([0, p.angles_y, 0]);
        vr_et_m = VR_Test_Spawn(p, VR_FA_Kind(vr_fa_case), p.origin + v_forward * 160, p.angles_y + 180);
        VR_ET_To(1);
        return;
    }
    if(vr_et_s == 1)
    {
        if(t < 0.3)
        {
            return; // (on the floor, its pose taken)
        }
        if(m == world)
        {
            VR_Test_Check(FALSE, sprintf("%s: no monster spawned", VR_FA_Name(vr_fa_case)));
            vr_fa_case = vr_fa_case + 1;
            VR_ET_To(0);
            return;
        }
        float head = VR_FA_AtHead(vr_fa_case);
        vector at = head ? VR_FA_Head(m) : (limbplace(m, 0, 3) + '0 0 6');
        VR_Test_Print(sprintf("%s: %s: aimed at %v from its origin (box top %g)\n", vr_test_tag, VR_FA_Name(vr_fa_case), at - m.origin, m.maxs_z));
        vector go = at - (p.origin + '0 0 22');
        go_z = 0;
        go = normalize(go);
        // (Raised by gravity's drop on the way: it meets the point it was aimed at.)
        float fly = VR_FA_FROM / (VR_ET_SPEED * VRMetersToUnits());
        float drop = 0.5 * cvar_hget(cvarh_sv_gravity) * fly * fly;
        m.health = head ? 1 : max(m.health, 500); // (the head: its blow kills, as decap_test.sh's `thrown`)
        if(!head)
        {
            // (Only stuck: a blade in a zombie's body cuts its limb off and kills it, vr_limbs.qc VR_Limb_Thrown, and a dead zombie is
            // never gibbed. Put back as it stuck.)
            vr_fa_keepcut = cvar_hget(cvarh_vr_decap_thrown);
            cvar_set("vr_decap_thrown", "0");
        }
        vr_fa_model = m.modelindex;
        vr_et_w = VR_ET_ThrowAxe(at - go * VR_FA_FROM + '0 0 1' * drop, go, head);
        VR_ET_To(2);
        return;
    }
    if(vr_et_s == 2)
    {
        if(w.throwhit == QVR_THROWHIT_HIT || w.vr_stick_kind != 0 || t > VR_FA_GIVEUP)
        {
            VR_Test_Print(sprintf("%s: %s: struck at %.2f s, kind %g, movetype %g, its host's health %g, headless %g\n", vr_test_tag,
                VR_FA_Name(vr_fa_case), t, w.vr_stick_kind, w.movetype, m.health, m.vr_headless));
            if(!VR_FA_AtHead(vr_fa_case))
            {
                cvar_set("vr_decap_thrown", ftos(vr_fa_keepcut));
                float inIt = VR_AxeStick_InMonster(w) && w.vr_stick_ent == m;
                VR_Test_Check(inIt, sprintf("%s: the axe stays in the body (kind %g)", VR_FA_Name(vr_fa_case), w.vr_stick_kind));
                if(!inIt)
                {
                    vr_fa_case = vr_fa_case + 1;
                    VR_ET_To(0);
                    return;
                }
                VR_ET_To(3);
                return;
            }
            VR_Test_Check(m.vr_headless, sprintf("%s: its head off (headless %g, health %g)", VR_FA_Name(vr_fa_case), m.vr_headless, m.health));
            vr_fa_z0 = w.origin_z;
            vr_fa_floatcount = 0;
            VR_ET_To(4);
        }
        return;
    }
    if(vr_et_s == 3)
    {
        if(t < 0.2)
        {
            return; // (its stick settled)
        }
        vr_fa_z0 = w.origin_z;
        vr_fa_floatcount = 0;
        if(vr_fa_case == 3)
        {
            remove(m);
            vr_et_m = world;
        }
        else
        {
            T_Damage(m, p, p, 2000); // (gibbed whole)
        }
        VR_ET_To(4);
        return;
    }
    if(vr_et_s == 4)
    {
        vr_fa_floatcount = vr_fa_floatcount + VR_FA_Floats(w);
        if(t < VR_FA_WATCH)
        {
            return;
        }
        float fell = vr_fa_z0 - w.origin_z;
        string name = VR_FA_Name(vr_fa_case);
        VR_Test_Print(sprintf("%s: %s: after %g s the axe is %.1f units lower, kind %g, movetype %g, floated %g frames\n", vr_test_tag, name,
            VR_FA_WATCH, fell, w.vr_stick_kind, w.movetype, vr_fa_floatcount));
        // (A frame's grace: the stuck think's 0.02 s, VR_AxeStick_Think, finds its host gone or changed.)
        VR_Test_Check(vr_fa_floatcount <= 3, sprintf("%s: never floating (%g frames held still in nothing)", name, vr_fa_floatcount));
        VR_Test_Check(!VR_AxeStick_IsStuck(w) && w.movetype != MOVETYPE_NONE && fell >= 8,
            sprintf("%s: the axe falls (%.1f units in %g s, 8 at least; kind %g, movetype %g)", name, fell, VR_FA_WATCH, w.vr_stick_kind, w.movetype));
        vr_fa_case = vr_fa_case + 1;
        VR_ET_To(0);
    }
};
#endif
