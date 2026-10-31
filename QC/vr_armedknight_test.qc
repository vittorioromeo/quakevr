#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedknight_test.qc (ROUND23.md, "Knights as armed humanoids"):
//
// Test 95: the knights as armed humanoids (vr_agknight). In vrtesthall, a knight 90 ahead of you, made by its own spawn
// function (monster_knight) as a map's would be:
//   off      vr_agknight 0: a plain knight (its own model, not armed): nothing changes with the setting off;
//   spawn    vr_agknight 1: an armed knight (kind 2, progs/vrknight.mdl) holding a sword, the sword at its hand;
//   swing    it hunts you: its sword (the armed humanoids' melee) hurts you;
//   disarm   its sword knocked out (VR_AG_Disarm): lying by it; it takes it up again (you unseen);
//   death    killed (not gibbed): its sword falls, once (its own sword drop, VR_DropKnightSword, not as well).
// `vr_agrunt_test_kind 3`: the same for the death knight (vr_agdknight, monster_hell_knight, progs/vrhknight.mdl), with
//   spell    after its swing, moved out of reach (you in sight): it casts its fireball volley (its knightspikes) with
//            its sword in its hand, and still holds it after.
// Each prints `agtest: PASS|FAIL <what>`; the run ends on `agtest: test 95 over: N passed, M failed` (-QuitOn).
// A case that never gets there FAILs as "timed out" (a safety net).
// ----------------------------------------------------------------------------

float vr_aknt_case;  // 0 off, 1 spawn, 2 swing, 3 disarm, 4 death, 5 done
float vr_aknt_t0;
float vr_aknt_flag0; // vr_agknight as it was
float vr_aknt_hp;    // the player's health as the swing case began
vector vr_aknt_home;

// The knight kind under test (vr_agrunt_test_kind 3: the death knight; else the knight) and its names.
float() VR_AKNT_Dk = { return rint(cvar("vr_agrunt_test_kind")) == VR_AM_DKNIGHT; };
string() VR_AKNT_Flag = { return VR_AKNT_Dk() ? "vr_agdknight" : "vr_agknight"; };
string() VR_AKNT_Name = { return VR_AKNT_Dk() ? "death knight" : "knight"; };

// Its fireballs in flight (knightspikes it cast).
float(entity g) VR_AKNT_Spells =
{
    float n = 0;
    entity e = find(world, classname, "knightspike");
    while(e)
    {
        n = n + (e.owner == g);
        e = find(e, classname, "knightspike");
    }
    return n;
};

// A knight `dist` ahead of the player, made by monster_knight.
entity(float dist) VR_AKNT_Spawn =
{
    entity p = vr_agt_player;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    traceline(p.origin, p.origin + fw * dist, TRUE, p);
    vector at = trace_endpos - fw * 24;
    entity oldself = self;
    self = spawn();
    self.classname = VR_AKNT_Dk() ? "monster_hell_knight" : "monster_knight";
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    if(VR_AKNT_Dk())
    {
        monster_hell_knight();
    }
    else
    {
        monster_knight();
    }
    entity g = self;
    self = oldself;
    return g;
};

// Swords lying within 96 of `at`.
float(vector at) VR_AKNT_Lying =
{
    float n = 0;
    entity e = find(world, classname, "thrown_weapon");
    while(e)
    {
        if(vlen(e.origin - at) < 200) // (a dying one throws it 150 away)
        {
            n = n + 1;
        }
        e = find(e, classname, "thrown_weapon");
    }
    return n;
};

// Its sword knocked out (you unseen): lying by it; the re-arm case next.
void(entity g) VR_AKNT_Disarm =
{
    entity p = vr_agt_player;
    p.health = 100;
    p.flags = p.flags | FL_NOTARGET;
    g.enemy = world;
    makevectors(g.angles);
    VR_AG_Disarm(g, v_right, 120, "test 95");
    VR_AGT_Check(sprintf("knight disarm: empty-handed (%g), its sword lying by it (%g)", g.vr_ag_wid,
        VR_AKNT_Lying(g.origin)), !g.vr_ag_prop && VR_AKNT_Lying(g.origin) == 1);
    vr_aknt_case = 3;
    vr_aknt_t0 = time;
};

void() VR_AKNT_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float t = time - vr_aknt_t0;
    if(vr_agt_step == 0)
    {
        if(vr_aknt_case == 0)
        {
            vr_aknt_home = p.origin;
            vr_aknt_flag0 = cvar(VR_AKNT_Flag());
            VR_AGT_ClearWeapons();
            cvar_set(VR_AKNT_Flag(), "0");
            g = VR_AKNT_Spawn(90);
            VR_AGT_Check(sprintf("%s off: a plain one (armed %g, model %s)", VR_AKNT_Name(), g.vr_ag, g.model),
                !g.vr_ag && g.model == (VR_AKNT_Dk() ? "progs/hknight.mdl" : "progs/knight.mdl"));
            remove(g);
            cvar_set(VR_AKNT_Flag(), "1");
            p.flags = p.flags | FL_NOTARGET;
            vr_agt_grunt = g = VR_AKNT_Spawn(90);
            VR_AGT_Check(sprintf("%s on: armed (%g), kind %g, model %s, a sword (%g)", VR_AKNT_Name(), g.vr_ag,
                g.vr_ag_kind, g.model, g.vr_ag_wid), g.vr_ag &&
                g.vr_ag_kind == (VR_AKNT_Dk() ? VR_AM_DKNIGHT : VR_AM_KNIGHT) &&
                g.model == (VR_AKNT_Dk() ? "progs/vrhknight.mdl" : "progs/vrknight.mdl") &&
                g.vr_ag_wid == WID_SWORD && g.vr_ag_prop != world);
            vr_aknt_case = 1;
            vr_aknt_t0 = time;
            vr_agt_step = 1;
        }
        return;
    }
    if(vr_aknt_case == 1)
    {
        if(t < 0.5)
        {
            return; // (its pose sent, its sword placed)
        }
        float d = g.vr_ag_prop ? vlen(g.vr_ag_prop.origin - VR_AM_Hand(g)) : 999;
        VR_AGT_Check(sprintf("knight spawn: its sword %.1f from its hand", d), d < 16);
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        p.health = 1000;
        vr_aknt_hp = p.health;
        VR_AGT_Hunt(g);
        vr_aknt_case = 2;
        vr_aknt_t0 = time;
        return;
    }
    if(vr_aknt_case == 2)
    {
        if(p.health < vr_aknt_hp || t > 15)
        {
            VR_AGT_Check(sprintf("knight swing: hurt you %g in %.1f s%s, holding %g", vr_aknt_hp - p.health, t,
                t > 15 ? " (timed out)" : "", g.vr_ag_wid), p.health < vr_aknt_hp && g.vr_ag_wid == WID_SWORD);
            if(VR_AKNT_Dk())
            {
                // Out of its reach, you in sight: its spell (vr_am_spell).
                vector dir = normalize(g.origin - p.origin);
                dir_z = 0;
                traceline(g.origin, g.origin + dir * 360, TRUE, g);
                setorigin(g, trace_endpos - dir * 32);
                g.angles_y = vectoyaw(p.origin - g.origin);
                p.health = 1000;
                vr_aknt_case = 6;
                vr_aknt_t0 = time;
                return;
            }
            VR_AKNT_Disarm(g);
        }
        return;
    }
    if(vr_aknt_case == 6)
    {
        if(VR_AKNT_Spells(g) > 0 || t > 15)
        {
            VR_AGT_Check(sprintf("death knight spell: cast %g fireballs at %.0f in %.1f s%s, holding %g",
                VR_AKNT_Spells(g), vlen(g.origin - p.origin), t, t > 15 ? " (timed out)" : "", g.vr_ag_wid),
                VR_AKNT_Spells(g) > 0 && g.vr_ag_wid == WID_SWORD && g.vr_ag_prop != world);
            VR_AKNT_Disarm(g);
        }
        return;
    }
    if(vr_aknt_case == 3)
    {
        if(g.vr_ag_prop || t > 15)
        {
            VR_AGT_Check(sprintf("knight re-arm: took up %g in %.1f s%s", g.vr_ag_wid, t, t > 15 ? " (timed out)" : ""),
                g.vr_ag_wid == WID_SWORD);
            T_Damage(g, p, p, g.health + 10); // (dies, not gibbed)
            vr_aknt_case = 4;
            vr_aknt_t0 = time;
        }
        return;
    }
    if(vr_aknt_case == 4)
    {
        if(t < 0.3)
        {
            return;
        }
        VR_AGT_Check(sprintf("knight death: its sword fell once (%g lying), dead (%g)", VR_AKNT_Lying(g.origin),
            g.health), VR_AKNT_Lying(g.origin) == 1 && g.health <= 0);
        if(VR_AKNT_Lying(g.origin) != 1)
        {
            entity e = findradius(g.origin, 300);
            while(e)
            {
                VR_AGT_Print(sprintf("  near: %s %s %.0f", e.classname, e.model, vlen(e.origin - g.origin)));
                e = e.chain;
            }
        }
        VR_AGT_ClearWeapons();
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        setorigin(p, vr_aknt_home);
        cvar_set(VR_AKNT_Flag(), ftos(vr_aknt_flag0));
        vr_aknt_case = 0;
        vr_agt_step = 0;
        vr_agt_grunt = world;
        cvar_set("vr_agrunt_test", "0");
    }
};
#endif
