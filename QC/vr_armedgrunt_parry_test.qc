//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_parry_test.qc -- armed humanoids parrying (vr_armedgrunt_parry.qc). `vr_agrunt_test 60` (Debug > Tests >
// Armed Grunts: They Parry Your Blows), in an open map (vrtesthall), you unseen: sword grunts (the test's kind:
// vr_agrunt_test_kind) 70 units ahead, aware of you, held still, take your blows as the melee deals them (a blade 32
// units long swept 28 units across this frame, T_Damage_VRMelee, 20 damage; the hit test their own):
// - one swept through its weapon: parried (`weapon`: 20 x vr_agrunt_parry_damage, the ring), and a second one 0.6 s later:
//   parried and disarmed (`second`: its weapon lying, its clatter);
// - a chop on its head beside its weapon, a cut across its legs: not parried, their full damage (`head`, `legs`);
// - one through its weapon from behind it: not parried (`behind`);
// - an ogre's chainsaw on one facing the ogre (its parry raised into the blow's way first): parried (`ogre`).
// Each `agparry: <case>: ... PASS|FAIL`, then `agparry: <p> passed, <f> failed`.
// ----------------------------------------------------------------------------

float vr_agpt2_chance; // vr_agrunt_parry_chance before (the test's 1)
float vr_agpt2_parry;
entity vr_agpt2_ogre;

void(string s) VR_AGPT2_Print = { bprint(sprintf("agparry: %s\n", s)); };
void(string what, float ok) VR_AGPT2_Check =
{
    VR_AGPT2_Print(sprintf("%s %s", what, ok ? "PASS" : "FAIL"));
    if(ok) { vr_agt_passed = vr_agt_passed + 1; } else { vr_agt_failed = vr_agt_failed + 1; }
};

// Held still (its AI off), aware of you, healthy, not in pain.
void(entity g) VR_AGPT2_Hold =
{
    g.think = SUB_Null;
    g.nextthink = -1;
    g.enemy = vr_agt_player;
    g.health = 1000;
    g.pain_finished = 0;
    g.vr_ag_act = 0;
};

// The least distance between the blade swept from `c0` to `c1` (its line along `along`, 26 back, 6 ahead) and `g`'s
// weapon.
float(entity g, vector c0, vector c1, vector along) VR_AGPT2_Miss =
{
    VR_AG_WeaponLine(g.vr_ag_prop);
    float best = 100000;
    float k;
    for(k = 0; k <= 8; k++)
    {
        vector c = c0 + (c1 - c0) * (k / 8);
        best = min(best, VR_AG_SegDist(c - along * 26, c + along * 6, vr_ag_wa, vr_ag_wb));
    }
    return best;
};

// Your main hand's blade swept from `c0` to `c1` this frame (its line along `along`) meeting `g`'s body at `at`: the
// melee's blow of 20 (T_Damage_VRMelee). Its damage dealt; vr_agpt2_expect what it would be unparried.
float vr_agpt2_expect;
float(entity g, vector c0, vector c1, vector along, vector at) VR_AGPT2_Blow =
{
    entity p = vr_agt_player;
    entity oldself = self;
    self = p;
    p.weapon = WID_SWORD;
    entity b = VR_Blow(cVR_MainHand);
    b.mh_n = 2;
    b.mh_pp[0] = c0 - along * 26;
    b.mh_pp[1] = c0 + along * 6;
    b.mh_cp[0] = c1 - along * 26;
    b.mh_cp[1] = c1 + along * 6;
    vector dir = normalize(c1 - c0);
    float mult = VR_Melee_Positional(g, at, dir);
    vr_agpt2_expect = VR_Melee_CapDamage(20 * mult, mult);
    float h0 = g.health;
    VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, dir);
    T_Damage_VRMelee(g, p, p, 20);
    VR_ClearHitKind();
    b.mh_n = 0;
    self = oldself;
    return h0 - g.health;
};

// Its body's point `l` (body space: x ahead, y its left, z up).
vector(entity g, vector l) VR_AGPT2_At = { VR_AGP_Body(g); return g.origin + VR_AGP_ToWorld(l); };

// A blade swept across its weapon's middle, pointing at it from you (`side` the way across).
float(entity g) VR_AGPT2_WeaponBlow =
{
    VR_AG_WeaponLine(g.vr_ag_prop);
    vector mid = (vr_ag_wa + vr_ag_wb) * 0.5;
    vector along = g.origin - vr_agt_player.origin;
    along_z = 0;
    along = normalize(along);
    vector side = VR_AGP_Cross(along, '0 0 1');
    float miss = VR_AGPT2_Miss(g, mid - side * 14, mid + side * 14, along);
    float d = VR_AGPT2_Blow(g, mid - side * 14, mid + side * 14, along, VR_AGPT2_At(g, VR_AGPAR_CHEST));
    VR_AGPT2_Print(sprintf("(a blade through its weapon: %.1f units from it, %g damage, unparried %g; why '%s')", miss, d,
        vr_agpt2_expect, vr_agpar_why));
    return d;
};

void() VR_AGPT2_End =
{
    entity p = vr_agt_player;
    cvar_set("vr_agrunt_parry_chance", ftos(vr_agpt2_chance));
    cvar_set("vr_agrunt_parry", ftos(vr_agpt2_parry));
    if(vr_agpt2_ogre)
    {
        remove(vr_agpt2_ogre);
        vr_agpt2_ogre = world;
    }
    VR_AGPT2_Print(sprintf("%g passed, %g failed", vr_agt_passed, vr_agt_failed));
    p.flags = p.flags - (p.flags & FL_NOTARGET);
    cvar_set("vr_agrunt_test", "0");
};

float vr_agpt2_n0;
float vr_agpt2_fb0;
void() VR_AGPar_Test =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        p.flags = p.flags | FL_NOTARGET;
        vr_agpt2_chance = cvar("vr_agrunt_parry_chance");
        vr_agpt2_parry = cvar("vr_agrunt_parry");
        cvar_set("vr_agrunt_parry_chance", "1");
        cvar_set("vr_agrunt_parry", "1");
        vr_agt_grunt = VR_AGT_Spawn(WID_SWORD, 70);
        vr_agt_step = 1;
        vr_agt_until = time + 0.5;
        return;
    }
    if(time < vr_agt_until)
    {
        if(vr_agpt2_ogre)
        {
            vr_agpt2_ogre.vr_melee_begun = time; // (its chainsaw's attack under way: the grunt sees it coming)
        }
        return;
    }
    if(vr_agt_step == 1)
    {
        // The first parry.
        VR_AGPT2_Hold(g);
        vr_agpt2_n0 = vr_agpar_n;
        float d1 = VR_AGPT2_WeaponBlow(g);
        float kept = 20 * cvar_hget(cvarh_vr_agrunt_parry_damage);
        VR_AGPT2_Check(sprintf("weapon: parried %g, %g damage (kept %g of 20), still armed %g:", vr_agpar_n - vr_agpt2_n0, d1,
            kept, g.vr_ag_prop != world), vr_agpar_n == vr_agpt2_n0 + 1 && fabs(d1 - kept) < 0.01 && g.vr_ag_prop != world);
        VR_AGPT2_Hold(g);
        vr_agt_step = 2;
        vr_agt_until = time + 0.6;
        return;
    }
    if(vr_agt_step == 2)
    {
        // The second, in a row: its strength spent, the weapon knocked away.
        VR_AGPT2_Hold(g);
        vr_agpt2_fb0 = vr_ag_feedbacks;
        float d2 = VR_AGPT2_WeaponBlow(g);
        entity lying = VR_AGT_LyingNear(g.origin);
        VR_AGPT2_Check(sprintf("second: parried %g in all, %g damage, disarmed %g, lying %s, %g clatter:",
            vr_agpar_n - vr_agpt2_n0, d2, g.vr_ag_prop == world, lying.netname, vr_ag_feedbacks - vr_agpt2_fb0),
            vr_agpar_n == vr_agpt2_n0 + 2 && g.vr_ag_prop == world && lying != world && vr_ag_feedbacks == vr_agpt2_fb0 + 1);
        VR_AGT_Remove(g);
        VR_AGT_ClearWeapons();
        vr_agt_grunt = VR_AGT_Spawn(WID_SWORD, 70);
        vr_agt_step = 3;
        vr_agt_until = time + 0.5;
        return;
    }
    if(vr_agt_step == 3 || vr_agt_step == 4)
    {
        // A chop on its head beside its weapon (on its other side), or a cut across its legs: full damage.
        VR_AGPT2_Hold(g);
        vr_agpt2_n0 = vr_agpar_n;
        VR_AG_WeaponLine(g.vr_ag_prop);
        VR_AGP_Body(g);
        vector along = g.origin - p.origin;
        along_z = 0;
        along = normalize(along);
        vector c0;
        vector c1;
        vector at;
        string what;
        if(vr_agt_step == 3)
        {
            vector wl = VR_AGP_ToLocal((vr_ag_wa + vr_ag_wb) * 0.5 - g.origin);
            float y = wl_y < 0 ? 6 : -6; // (beside its weapon, the other side of its head)
            float hz = g.view_ofs_z > 0 ? g.view_ofs_z : 22;
            c0 = VR_AGPT2_At(g, [2, y, hz + 16]);
            c1 = VR_AGPT2_At(g, [2, y, hz - 4]);
            at = VR_AGPT2_At(g, [2, y * 0.5, hz]);
            what = "head";
        }
        else
        {
            c0 = VR_AGPT2_At(g, [6, -14, -16]);
            c1 = VR_AGPT2_At(g, [6, 14, -16]);
            at = VR_AGPT2_At(g, [2, 0, -16]);
            what = "legs";
        }
        // (The blade along `along` ends 6 units past its centre's way: put the way's centre on its body's front.)
        float miss = VR_AGPT2_Miss(g, c0 - along * 4, c1 - along * 4, along);
        float d = VR_AGPT2_Blow(g, c0 - along * 4, c1 - along * 4, along, at);
        VR_AGPT2_Check(sprintf("%s: %.1f units from its weapon, parried %g, %g damage (unparried %g), why '%s':", what, miss,
            vr_agpar_n - vr_agpt2_n0, d, vr_agpt2_expect, vr_agpar_why),
            vr_agpar_n == vr_agpt2_n0 && fabs(d - vr_agpt2_expect) < 0.01 && d > 0 && vr_agpar_why == "missed its weapon");
        VR_AGT_Remove(g);
        vr_agt_grunt = VR_AGT_Spawn(WID_SWORD, 70);
        if(vr_agt_step == 4)
        {
            vr_agt_grunt.angles_y = vr_agt_grunt.angles_y + 180; // (its back to you)
        }
        vr_agt_step = vr_agt_step + 1;
        vr_agt_until = time + 0.5;
        return;
    }
    if(vr_agt_step == 5)
    {
        // Through its weapon from behind it.
        VR_AGPT2_Hold(g);
        vr_agpt2_n0 = vr_agpar_n;
        float d5 = VR_AGPT2_WeaponBlow(g);
        VR_AGPT2_Check(sprintf("behind: parried %g, %g damage (unparried %g), why '%s':", vr_agpar_n - vr_agpt2_n0, d5,
            vr_agpt2_expect, vr_agpar_why),
            vr_agpar_n == vr_agpt2_n0 && fabs(d5 - vr_agpt2_expect) < 0.01 && vr_agpar_why == "from behind");
        VR_AGT_Remove(g);
        // An ogre between you and a fresh one, facing it, its chainsaw's attack on it.
        vr_agt_grunt = VR_AGT_Spawn(WID_SWORD, 110);
        g = vr_agt_grunt;
        vector to = p.origin - g.origin;
        to_z = 0;
        entity oldself = self;
        self = spawn();
        setorigin(self, g.origin + normalize(to) * 56);
        self.classname = "monster_ogre";
        monster_ogre();
        self.angles_y = vectoyaw(g.origin - self.origin);
        vr_agpt2_ogre = self;
        self = oldself;
        vr_agt_step = 6;
        vr_agt_until = time + 1; // (its start: walkmonster_start_go, up to 0.6 s on)
        return;
    }
    if(vr_agt_step == 6)
    {
        // Both held still, the ogre at it: its parry rises into the chainsaw's way.
        VR_AGPT2_Hold(g);
        g.enemy = vr_agpt2_ogre;
        vr_agpt2_ogre.think = SUB_Null;
        vr_agpt2_ogre.nextthink = -1;
        vr_agpt2_ogre.enemy = g;
        vr_agpt2_ogre.vr_melee_begun = time;
        VR_AGPT2_Print(sprintf("(the chainsaw's way meets its weapon held ready: %g, its parry up %.2f)",
            VR_AGPar_MonsterMeets(g, vr_agpt2_ogre), g.vr_agpar_w));
        vr_agt_step = 7;
        vr_agt_until = time + 0.6;
        return;
    }
    if(vr_agt_step == 7)
    {
        entity o = vr_agpt2_ogre;
        vr_agpt2_n0 = vr_agpar_n;
        float h0 = g.health;
        entity oldself2 = self;
        self = o;
        T_Damage(g, o, o, 10);
        self = oldself2;
        float d7 = h0 - g.health;
        VR_AG_WeaponLine(g.vr_ag_prop);
        VR_AGP_Body(g);
        VR_AGPT2_Print(sprintf("(its parry's pose: its weapon's middle %v, its line %v, body space)",
            VR_AGP_ToLocal((vr_ag_wa + vr_ag_wb) * 0.5 - g.origin), VR_AGP_ToLocal(normalize(vr_ag_wb - vr_ag_wa))));
        float kept7 = 10 * cvar_hget(cvarh_vr_agrunt_parry_damage);
        VR_AGPT2_Check(sprintf("ogre: its parry up %.2f, parried %g, %g damage (kept %g of 10), why '%s':", g.vr_agpar_w,
            vr_agpar_n - vr_agpt2_n0, d7, kept7, vr_agpar_why),
            vr_agpar_n == vr_agpt2_n0 + 1 && fabs(d7 - ceil(kept7)) < 0.01); // (T_Damage rounds up)
        VR_AGT_Remove(g);
        VR_AGPT2_End();
    }
};
