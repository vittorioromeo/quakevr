#ifdef QVR_TESTS
// vr_rag1108_test.qc -- the round of 2026-11-08's ragdoll and armed-humanoid fixes (ROUND24.md, "Ragdolls and armed
// humanoids, 11-08"). `vr_rag1108_test <n>` (any map with room ahead: e1m1's start; god, notarget):
// 1: removed outright, an armed one's worn pack, weapon and back tank go with it (the toolgun's remover, the scene's
//    cleanup on a dead wearer, a raw remove): none left, none torn off later as their edicts are made again. Ends on
//    `rag1108rm: <p> passed, <f> failed`.
// 2: the back tanks beside the worn packs: a flamethrower's on a grunt, a knight and a death knight, an enforcer's, a
//    poison sprayer's on a grunt, all facing away; each tank off its pack's box, to the wearer's right; a screenshot.
//    Ends on `rag1108tank: ...`.
// 3: the gib rule: small hits never fully gib a corpse (its pool worn out), a solid one then does, one big hit gibs a
//    fresh one, explosions by their pool. Ends on `rag1108gib: ...`.
// 4: shoves on ones getting up: a rising body back down, a weaker shove late in the rise (a balance step or down), one
//    rising the old way (its get-up's frames) down again. Ends on `rag1108shove: ...`.

float vr_r18_state;
float vr_r18_frames;
float vr_r18_rips;
float vr_r18_n;
float vr_r18_old;
float vr_r18_late; // frames into its second rise its one-hand shove lands (test 5: later, nearly up)
entity vr_r18_a, vr_r18_b, vr_r18_c, vr_r18_d, vr_r18_e;

entity(entity p, float kind, float wid, float side, float ahead, float away) VR_WPT_SpawnKind;

// What `g` still has about it: its worn pack (2), weapon (4), back tank (1), added up.
float(entity g) VR_R18_Left =
{
    float n = 0;
    for(entity k = find(world, classname, "vr_wornpack"); k; k = find(k, classname, "vr_wornpack"))
    {
        if(k.vr_wp_wearer == g) { n = n | 2; }
    }
    for(entity w = find(world, classname, "vr_ag_weapon"); w; w = find(w, classname, "vr_ag_weapon"))
    {
        if(w.owner == g) { n = n | 4; }
    }
    for(entity t = find(world, classname, "vr_fueltank_worn"); t; t = find(t, classname, "vr_fueltank_worn"))
    {
        if(t.owner == g) { n = n | 1; }
    }
    return n;
};

entity(entity g) VR_R18_Tank =
{
    for(entity t = find(world, classname, "vr_fueltank_worn"); t; t = find(t, classname, "vr_fueltank_worn"))
    {
        if(t.owner == g) { return t; }
    }
    return world;
};

void(entity p) VR_R18_RemoveBegin =
{
    VR_Test_Begin("rag1108rm");
    vr_r18_a = VR_WPT_SpawnKind(p, VR_AM_GRUNT, WID_ADPYRO, -50, 140, TRUE);
    vr_r18_b = VR_WPT_SpawnKind(p, VR_AM_GRUNT, WID_SHOTGUN, 50, 140, TRUE);
    vr_r18_c = VR_WPT_SpawnKind(p, VR_AM_GRUNT, WID_ADPYRO, 0, 220, TRUE);
    vr_r18_state = 10;
    vr_r18_frames = 0;
};

void(entity p) VR_R18_RemoveFrame =
{
    vr_r18_frames++;
    if(vr_r18_state == 10)
    {
        if(VR_R18_Left(vr_r18_a) != 7 || VR_R18_Left(vr_r18_c) != 7 || !(VR_R18_Left(vr_r18_b) & 2))
        {
            if(vr_r18_frames > 144)
            {
                VR_Test_Check(FALSE, sprintf("timed out waiting for the packs and tanks (a %g, b %g, c %g)",
                    VR_R18_Left(vr_r18_a), VR_R18_Left(vr_r18_b), VR_R18_Left(vr_r18_c)));
                VR_Test_Verdict();
                vr_r18_state = 0;
            }
            return;
        }
        vr_r18_rips = vr_wp_rips;
        VR_Toolgun_Remove(vr_r18_a);
        VR_Test_Check(VR_R18_Left(vr_r18_a) == 0, sprintf("the toolgun's remover takes the pack, weapon and tank with it (left %g)",
            VR_R18_Left(vr_r18_a)));
        vr_r18_b.health = 10;
        T_Damage(vr_r18_b, p, p, 11); // (dies whole: its pack takeable on its back)
        remove(vr_r18_c); // (a raw remove: the pack's and the weapon's own think take them)
        vr_r18_state = 11;
        vr_r18_frames = 0;
        return;
    }
    if(vr_r18_state == 11)
    {
        if(!vr_r18_b.vr_wp_pack || vr_r18_b.vr_wp_pack.vr_wp_state != VR_WP_DEAD)
        {
            if(vr_r18_frames > 144)
            {
                VR_Test_Check(FALSE, "timed out waiting for the dead one's pack");
                vr_r18_state = 13;
            }
            return;
        }
        VR_Test_Check(VR_R18_Left(vr_r18_c) == 0, sprintf("a raw remove: its pack, weapon and tank gone by themselves (left %g)",
            VR_R18_Left(vr_r18_c)));
        entity body = vr_r18_b;
        VR_Scene_Remove(body); // (vr_scene_clean corpses)
        VR_Test_Check(VR_R18_Left(body) == 0, "the scene's cleanup takes a dead wearer's pack with it");
        vr_r18_state = 12;
        vr_r18_frames = 0;
        return;
    }
    if(vr_r18_state == 12)
    {
        // The freed edicts made again (ED_Alloc reuses one half a second after it was freed): spawns enough to fill them.
        if(vr_r18_frames == 72)
        {
            for(float i = 0; i < 24; i++)
            {
                entity f = spawn();
                f.classname = "vr_r18_filler";
            }
        }
        if(vr_r18_frames < 108)
        {
            return;
        }
        VR_Test_Check(vr_wp_rips == vr_r18_rips, sprintf("no pack torn off as the edicts were made again (rips %g -> %g)",
            vr_r18_rips, vr_wp_rips));
        float items = 0;
        for(entity k = find(world, classname, "vr_wornpack"); k; k = find(k, classname, "vr_wornpack"))
        {
            if(!(k.vr_wp_wearer.flags & FL_MONSTER)) // (the map's own armed ones wear theirs)
            {
                items++;
            }
        }
        VR_Test_Check(items == 0, sprintf("no worn pack left without a wearer (%g)", items));
        for(entity f2 = find(world, classname, "vr_r18_filler"); f2; f2 = find(f2, classname, "vr_r18_filler"))
        {
            f2.classname = "vr_r18_gone";
            remove(f2);
        }
        vr_r18_state = 13;
    }
    if(vr_r18_state == 13)
    {
        VR_Test_Verdict();
        vr_r18_state = 0;
    }
};

void(entity p) VR_R18_TankBegin =
{
    VR_Test_Begin("rag1108tank");
    vr_r18_a = VR_WPT_SpawnKind(p, VR_AM_GRUNT, WID_ADPYRO, -100, 150, TRUE);
    vr_r18_b = VR_WPT_SpawnKind(p, VR_AM_KNIGHT, WID_ADPYRO, -50, 150, TRUE);
    vr_r18_c = VR_WPT_SpawnKind(p, VR_AM_DKNIGHT, WID_ADPYRO, 0, 150, TRUE);
    vr_r18_d = VR_WPT_SpawnKind(p, VR_AM_ENFORCER, WID_ADPYRO, 50, 150, TRUE);
    vr_r18_e = VR_WPT_SpawnKind(p, VR_AM_GRUNT, WID_ADFUMIGATOR, 100, 150, TRUE);
    vr_r18_state = 20;
    vr_r18_frames = 0;
};

// One wearer's tank: off its pack's box (none: an enforcer's on its model), to its right of its back's middle.
void(entity g, string what) VR_R18_TankCheck =
{
    entity t = VR_R18_Tank(g);
    if(!t)
    {
        VR_Test_Check(FALSE, sprintf("%s wears a tank", what));
        return;
    }
    makevectors([0, g.angles_y, 0]);
    float right = (t.origin - g.origin) * v_right;
    VR_Test_Check(right > 3, sprintf("%s's tank is to its right (%.1f units)", what, right));
    if(g.vr_wp_pack && !g.vr_wp_pack.vr_wp_baked)
    {
        float off = ragdollbone(g, t.origin, QVR_WORNPACK_AT);
        VR_Test_Check(off >= 1, sprintf("%s's tank's middle is off its pack's box (%.1f units)", what, off));
    }
};

void(entity p) VR_R18_TankFrame =
{
    vr_r18_frames++;
    if(vr_r18_state == 20)
    {
        float all = (VR_R18_Tank(vr_r18_a) && VR_R18_Tank(vr_r18_b) && VR_R18_Tank(vr_r18_c) && VR_R18_Tank(vr_r18_d) &&
                     VR_R18_Tank(vr_r18_e));
        if(!all && vr_r18_frames <= 144)
        {
            return;
        }
        if(vr_r18_frames < 36)
        {
            return; // (their packs placed on their drawn backs)
        }
        VR_R18_TankCheck(vr_r18_a, "a grunt's flamethrower");
        VR_R18_TankCheck(vr_r18_b, "a knight's flamethrower");
        VR_R18_TankCheck(vr_r18_c, "a death knight's flamethrower");
        VR_R18_TankCheck(vr_r18_d, "an enforcer's flamethrower");
        VR_R18_TankCheck(vr_r18_e, "a grunt's poison sprayer");
        localcmd("screenshot\n");
        vr_r18_state = 21;
        vr_r18_frames = 0;
        return;
    }
    if(vr_r18_state == 21 && vr_r18_frames >= 4) // (the screenshot taken)
    {
        VR_Test_Verdict();
        vr_r18_state = 0;
    }
};

// A corpse made: a grunt ahead killed whole.
entity(entity p, float side) VR_R18_Corpse =
{
    return VR_WPT_SpawnKind(p, VR_AM_GRUNT, -1, side, 120, FALSE); // (killed whole once it has started: VR_R18_GibFrame)
};

// One hit of `d` on corpse `g` as a shot (or a blast's), its line.
void(entity p, entity g, float d, float blast) VR_R18_Hit =
{
    VR_Hit_BeginAs(blast ? QVR_HITKIND_SPLASH : QVR_HITKIND_SHOT, FALSE);
    T_Damage(g, p, p, d);
    VR_Hit_End();
};

void(entity p) VR_R18_GibBegin =
{
    VR_Test_Begin("rag1108gib");
    cvar_set("vr_corpse_gib_overkill", "50");
    cvar_set("vr_corpse_gib_hit_min", "35");
    vr_r18_a = VR_R18_Corpse(p, -60);
    vr_r18_b = VR_R18_Corpse(p, 0);
    vr_r18_c = VR_R18_Corpse(p, 60);
    vr_r18_state = 30;
    vr_r18_frames = 0;
};

void(entity p) VR_R18_GibFrame =
{
    vr_r18_frames++;
    if(vr_r18_frames == 5)
    {
        for(float k = 0; k < 3; k++)
        {
            entity g = k == 0 ? vr_r18_a : (k == 1 ? vr_r18_b : vr_r18_c);
            g.health = 10;
            T_Damage(g, p, p, 11); // (dies whole)
        }
    }
    float ready = ((vr_r18_a.vr_corpse == 2 || vr_r18_a.vr_corpse_dying) && (vr_r18_b.vr_corpse == 2 || vr_r18_b.vr_corpse_dying) &&
                   (vr_r18_c.vr_corpse == 2 || vr_r18_c.vr_corpse_dying));
    if(!ready)
    {
        if(vr_r18_frames > 720)
        {
            VR_Test_Check(FALSE, sprintf("timed out waiting for the corpses (%s health %g corpse %g dying %g ragdoll %g)",
                vr_r18_a.classname, vr_r18_a.health, vr_r18_a.vr_corpse, vr_r18_a.vr_corpse_dying, isragdoll(vr_r18_a)));
            VR_Test_Verdict();
            vr_r18_state = 0;
        }
        return;
    }
    // A: many small hits wear it out, never gib it; a lesser one after still doesn't; a solid one then does.
    float gibbed = FALSE;
    for(float i = 0; i < 24 && !gibbed; i++)
    {
        VR_R18_Hit(p, vr_r18_a, 10, FALSE);
        gibbed = vr_corpse_last_gibbed;
    }
    VR_Test_Check(!gibbed && vr_r18_a.vr_corpse_hp <= 0, sprintf("24 small hits (%.1f each) wear its pool out (%.1f) but never gib it",
        vr_corpse_last_hit, vr_r18_a.vr_corpse_hp));
    VR_R18_Hit(p, vr_r18_a, 30, FALSE);
    VR_Test_Check(!vr_corpse_last_gibbed, sprintf("a hit of %.1f (under the per-hit least) on it worn out: whole", vr_corpse_last_hit));
    VR_R18_Hit(p, vr_r18_a, 42, FALSE);
    VR_Test_Check(vr_corpse_last_gibbed, sprintf("a solid hit of %.1f on it worn out: gibbed", vr_corpse_last_hit));
    // B: fresh, one big hit (a point-blank super shotgun's) gibs it.
    VR_R18_Hit(p, vr_r18_b, 45, FALSE);
    VR_Test_Check(!vr_corpse_last_gibbed, sprintf("a fresh one, a hit of %.1f: whole", vr_corpse_last_hit));
    VR_R18_Hit(p, vr_r18_b, 60, FALSE);
    VR_Test_Check(vr_corpse_last_gibbed, sprintf("a hit of %.1f (past the overkill): gibbed", vr_corpse_last_hit));
    // C: explosions by its pool, as before.
    float blasts = 0;
    gibbed = FALSE;
    while(!gibbed && blasts < 12)
    {
        VR_R18_Hit(p, vr_r18_c, 30, TRUE);
        blasts++;
        gibbed = vr_corpse_last_gibbed;
    }
    VR_Test_Check(gibbed && blasts > 1, sprintf("blasts of %.1f gib it as its pool runs out (%g blasts)", vr_corpse_last_hit, blasts));
    VR_Test_Verdict();
    vr_r18_state = 0;
};

// A shove of `push` on `g` from the player (two hands: -1).
void(entity p, entity g, float hand, float push) VR_R18_Shove =
{
    makevectors([0, p.angles_y, 0]);
    entity oldSelf = self;
    self = p;
    VR_Bash_Hit(g, g.origin + '0 0 16', v_forward, QVR_HITKIND_SHOVE, hand, 0, push);
    self = oldSelf;
};

void(entity p) VR_R18_ShoveBegin =
{
    VR_Test_Begin("rag1108shove");
    vr_r18_old = cvar_hget(cvarh_vr_ragdoll_active_getup);
    cvar_set("vr_ragdoll_active_getup", "1");
    vr_r18_a = VR_WPT_SpawnKind(p, VR_AM_GRUNT, WID_SHOTGUN, 0, 110, FALSE);
    vr_r18_state = 40;
    vr_r18_frames = 0;
    vr_r18_n = vr_kd_getup_shoves;
};

void(entity p) VR_R18_ShoveFrame =
{
    vr_r18_frames++;
    entity g = vr_r18_a;
    if(vr_r18_frames > 1440)
    {
        entity tb = vr_r18_state >= 45 ? vr_r18_b : vr_r18_a;
        VR_Test_Check(FALSE, sprintf("timed out in state %g (knockdown %g, rise %g, ragdoll %g, health %g, until %.1f now %.1f)",
            vr_r18_state, tb.vr_knockdown, tb.vr_kd_rise, isragdoll(tb), tb.health, tb.vr_kd_until, time));
        vr_r18_state = 49;
    }
    if(vr_r18_state == 40 && vr_r18_frames >= 3)
    {
        VR_Test_Check(VR_Knockdown_Start(g), "knocked down");
        g.vr_kd_until = time + 0.5;
        vr_r18_state = 41;
        vr_r18_frames = 0;
        return;
    }
    if(vr_r18_state == 41 || vr_r18_state == 43)
    {
        // Rising (its get-up due, still a ragdoll): a third of a second in, shoved.
        if(g.vr_knockdown != 1 || !g.vr_kd_rise)
        {
            vr_r18_n = 0;
            if(g.vr_knockdown == 0 && g.vr_kd_rise == 0 && vr_r18_frames > 360)
            {
                VR_Test_Check(FALSE, "it got up before the shove");
                vr_r18_state = 49;
            }
            return;
        }
        vr_r18_n++;
        if(vr_r18_n < (vr_r18_state == 41 ? 24 : vr_r18_late))
        {
            return;
        }
        float before = vr_kd_getup_shoves;
        VR_R18_Shove(p, g, vr_r18_state == 41 ? -1 : 0, vr_r18_state == 41 ? 1 : 0.7);
        if(vr_r18_state == 41)
        {
            VR_Test_Check(vr_kd_getup_shoves == before + 1 && vr_kd_getup_shove_how == 1 && g.vr_knockdown == 1 && !g.vr_kd_rise,
                sprintf("a two-hand shove early in its rise: back down (how %g, knockdown %g, rise %g)", vr_kd_getup_shove_how,
                g.vr_knockdown, g.vr_kd_rise));
            g.vr_kd_until = time + 0.5;
            vr_r18_state = 43;
        }
        else
        {
            VR_Test_Check(vr_kd_getup_shoves == before + 1 && (vr_kd_getup_shove_how == 1 || vr_kd_getup_shove_how == 2),
                sprintf("a one-hand shove late in its rise lands (%s)", vr_kd_getup_shove_how == 2 ? "a balance step" : "back down"));
            vr_r18_state = 44;
        }
        vr_r18_frames = 0;
        return;
    }
    if(vr_r18_state == 44)
    {
        // The old way: its get-up's frames (vr_ragdoll_active_getup 0) on a new one.
        cvar_set("vr_ragdoll_active_getup", "0");
        vr_r18_b = VR_WPT_SpawnKind(p, VR_AM_GRUNT, WID_SHOTGUN, 60, 110, FALSE);
        vr_r18_state = 45;
        vr_r18_frames = 0;
        return;
    }
    if(vr_r18_state == 45 && vr_r18_frames >= 3)
    {
        VR_Knockdown_Start(vr_r18_b);
        vr_r18_b.vr_kd_until = time + 0.5;
        vr_r18_state = 46;
        return;
    }
    if(vr_r18_state == 46)
    {
        if(vr_r18_b.vr_knockdown != 2)
        {
            return;
        }
        VR_R18_Shove(p, vr_r18_b, -1, 1);
        VR_Test_Check(vr_kd_getup_shove_how == 3 && vr_r18_b.vr_knockdown == 1 && isragdoll(vr_r18_b),
            sprintf("a shove on one rising through its get-up's frames: down again (how %g, knockdown %g)", vr_kd_getup_shove_how,
            vr_r18_b.vr_knockdown));
        vr_r18_state = 49;
    }
    if(vr_r18_state == 49)
    {
        cvar_set("vr_ragdoll_active_getup", ftos(vr_r18_old));
        VR_Test_Verdict();
        vr_r18_state = 0;
    }
};

void() VR_Rag1108_TestFrame =
{
    entity p = VR_Player();
    if(!p)
    {
        return;
    }
    if(vr_r18_state == 0)
    {
        float request = cvar_hget(cvarh_vr_rag1108_test);
        if(!request)
        {
            return;
        }
        cvar_set("vr_rag1108_test", "0");
        if(request == 1)
        {
            VR_R18_RemoveBegin(p);
        }
        if(request == 2)
        {
            VR_R18_TankBegin(p);
        }
        if(request == 3)
        {
            VR_R18_GibBegin(p);
        }
        if(request == 4)
        {
            vr_r18_late = 60;
            VR_R18_ShoveBegin(p);
        }
        if(request == 5)
        {
            vr_r18_late = 130; // (nearly up: its balance step)
            VR_R18_ShoveBegin(p);
        }
        return;
    }
    if(vr_r18_state >= 10 && vr_r18_state < 20)
    {
        VR_R18_RemoveFrame(p);
    }
    else if(vr_r18_state >= 20 && vr_r18_state < 30)
    {
        VR_R18_TankFrame(p);
    }
    else if(vr_r18_state >= 30 && vr_r18_state < 40)
    {
        VR_R18_GibFrame(p);
    }
    else if(vr_r18_state >= 40 && vr_r18_state < 50)
    {
        VR_R18_ShoveFrame(p);
    }
};
#endif
