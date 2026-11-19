#ifdef QVR_TESTS
// vr_skewerlist_test.qc -- `vr_embed_test 49` (skewerlist): the skewered gibs' list the shots walk (vr_carry.qc
// VR_Skewer_ListFirst/Next, VR_Gib_TraceHeld) instead of every edict. Gibs go on the sword in the main hand by a stab
// (as vr_embed_test 28's case 5); a shot across one is VR_Gib_TraceHeld's, as FireBulletsImpl's after its trace.
// - none skewered: a shot's walk visits nothing;
// - two on the blade, the later (the list's head) taken by the toolgun's remover: a shot across the other hits it, the
//   list not made again (the removed one's chain still leads on);
// - two on the blade, one burst: a shot across the other hits it;
// - two on the blade, the earlier removed with a plain remove() and its edict used again for something else: the walk
//   catches it (made again once) and the shot hits the other; again with the reused edict a new skewered gib (its old
//   predecessor still points at it: a loop): made again, both found;
// - the blade gone (the holder's weapon taken: what a disconnect or a death does to them, VR_Skewer_Find): they fall
//   off, the walk drops them and visits nothing; then their own fade removes them, and it still visits nothing.
// `vr_embed_test 50` puts two on the blade, prints "skewerlist: ready", saves (vr_skewerlist_test.sav) and loads, and
// once loaded runs `vr_embed_test 51`'s check: the list made at the load holds both, still on the blade, and a shot
// across each hits it. (51 alone: the check, after a load made by hand.)
// Ends `skewerlist: N passed, M failed`.

entity vr_skl_g1, vr_skl_g2, vr_skl_old;
// vr_skl_reb (rebuilds counted), vr_skl_phase (1: saved and loading: the save holds it, the check once OnLoadGame made
// the list again): declared in vr_embed_test.qc, which reads them.

void() VR_SKL_Begin =
{
    VR_Test_BeginFlags("skewerlist", VR_TEST_LOUD);
    vr_eh_home = VR_Player().origin;
    localcmd("vr_mock_hand_to_absolute 1\n");
    vr_et_on = 49;
    vr_et_case = 0;
    VR_ET_To(0);
};

// `g` (world: a new one) a gib (VR_ETS_Gib's) on the sword in the main hand by a stab.
entity(entity p, entity g) VR_SKL_StabInto =
{
    if(g == world)
    {
        g = VR_ETS_Gib(p, VR_ETS_OnLine(0));
    }
    else
    {
        vector at = VR_ETS_OnLine(0);
        setmodel(g, "progs/gib1.mdl");
        setsize(g, '0 0 0', '0 0 0');
        g.movetype = MOVETYPE_NONE;
        g.solid = SOLID_NOT;
        g.think = SUB_Remove;
        g.ltime = time;
        g.nextthink = time + 10;
        VR_MakeGibGrabbable(g);
        VR_Gib_MakeDestroyable(g, 1);
        setorigin(g, at - (g.mins + g.maxs) * 0.5);
    }
    VR_ETS_PutOn(g, 0);
    VR_ETS_Blow(p, g, VR_MKIND_STAB, 6);
    return g;
};

entity(entity p) VR_SKL_Stab = { return VR_SKL_StabInto(p, world); };

// A shot across `g` (world: through the air above the player): what VR_Gib_TraceHeld gives it.
entity(entity p, entity g) VR_SKL_Shot =
{
    makevectors(p.angles);
    vector at = g != world ? g.vr_skewer_c : p.origin + '0 0 200';
    trace_fraction = 1;
    trace_ent = world;
    hit_hand = -1;
    entity os = self;
    self = p;
    VR_Gib_TraceHeld(at + v_right * 60, at - v_right * 60);
    self = os;
    return trace_ent;
};

void(entity g) VR_SKL_Gone =
{
    if(g != world && g.modelindex)
    {
        remove(g);
    }
};

void() VR_SKL_End =
{
    localcmd("-grabmain\n");
    VR_SKL_Gone(vr_skl_g1);
    VR_SKL_Gone(vr_skl_g2);
    VRSetHandEmpty(VR_Player(), cVR_MainHand);
    vr_et_on = 0;
    if(vr_skl_save)
    {
        vr_skl_save = 0;
        bprint("skewerlist: ready\n");
        return;
    }
    VR_Test_Verdict();
};

void() VR_SKL_Step =
{
    entity p = self;
    float t = time - vr_et_t0;
    float s = vr_et_s;
    if(s == 0)
    {
        setorigin(p, vr_eh_home);
        p.velocity = '0 0 0';
        localcmd("+grabmain\n");
        VR_ET_To(1);
    }
    else if(s == 1 && VR_HandGrabUtil_IsHandGrabbing(p, cVR_MainHand) && t > 0.1)
    {
        VR_EH_Give(WID_SWORD, 0);
        VR_ET_To(2);
    }
    else if(s == 2 && t > 0.3)
    {
        if(!vr_skl_save)
        {
            entity none = VR_SKL_Shot(p, world);
            VR_Test_Check(none == world && vr_skewer_walked == 0 && vr_skewer_head == world,
                sprintf("none skewered: a shot's walk visits nothing (%g visited)", vr_skewer_walked));
        }
        vr_skl_g1 = VR_SKL_Stab(p);
        VR_ET_To(3);
    }
    else if(s == 3 && t > 0.2)
    {
        vr_skl_g2 = VR_SKL_Stab(p);
        VR_ET_To(4);
    }
    else if(s == 4 && t > 0.2)
    {
        VR_Test_Check(vr_skl_g1.vr_skewer_p == p && vr_skl_g2.vr_skewer_p == p, "two gibs on the blade");
        if(vr_skl_save)
        {
            VR_ET_To(60);
            return;
        }
        // The later one (the list's head) taken by the toolgun's remover.
        vr_skl_reb = vr_skewer_rebuilds;
        VR_Toolgun_Remove(vr_skl_g2);
        vr_skl_g2 = world;
        entity hit = VR_SKL_Shot(p, vr_skl_g1);
        VR_Test_Check(hit == vr_skl_g1 && vr_skewer_rebuilds == vr_skl_reb,
            sprintf("the head removed by the toolgun: a shot across the other hits it (%s), not made again (%g)", hit.classname,
            vr_skewer_rebuilds - vr_skl_reb));
        VR_SKL_Gone(vr_skl_g1);
        vr_skl_g1 = VR_SKL_Stab(p);
        VR_ET_To(5);
    }
    else if(s == 5 && t > 0.2)
    {
        vr_skl_g2 = VR_SKL_Stab(p);
        VR_ET_To(6);
    }
    else if(s == 6 && t > 0.2)
    {
        // One burst (a blow, a blast: VR_Gib_Burst).
        VR_Gib_Burst(vr_skl_g2, '0 0 1');
        vr_skl_g2 = world;
        entity hit2 = VR_SKL_Shot(p, vr_skl_g1);
        VR_Test_Check(hit2 == vr_skl_g1, sprintf("one burst: a shot across the other hits it (%s)", hit2.classname));
        VR_SKL_Gone(vr_skl_g1);
        vr_skl_g1 = VR_SKL_Stab(p);
        VR_ET_To(7);
    }
    else if(s == 7 && t > 0.2)
    {
        vr_skl_g2 = VR_SKL_Stab(p);
        VR_ET_To(8);
    }
    else if(s == 8 && t > 0.2)
    {
        // The earlier one (behind the head) removed with a plain remove(), nothing told: its edict is used again
        // (ED_Alloc: freed over 2 s ago) for something else.
        vr_skl_old = vr_skl_g1;
        remove(vr_skl_g1);
        vr_skl_g1 = world;
        VR_ET_To(9);
    }
    else if(s == 9 && t > 2.5)
    {
        entity filler = world;
        for(float k = 0; k < 400 && filler == world; k++)
        {
            entity e = spawn();
            e.classname = "vr_skl_filler";
            if(e == vr_skl_old) { filler = e; } else { e.think = SUB_Remove; e.nextthink = time + 0.1; }
        }
        VR_Test_Check(filler != world, "the removed gib's edict used again");
        vr_skl_reb = vr_skewer_rebuilds;
        entity hit3 = VR_SKL_Shot(p, vr_skl_g2);
        VR_Test_Check(hit3 == vr_skl_g2 && vr_skewer_rebuilds == vr_skl_reb + 1,
            sprintf("its edict reused: the walk made the list again (%g) and a shot across the other hits it (%s)",
            vr_skewer_rebuilds - vr_skl_reb, hit3.classname));
        if(filler != world) { remove(filler); }
        VR_SKL_Gone(vr_skl_g2);
        vr_skl_g1 = VR_SKL_Stab(p);
        VR_ET_To(10);
    }
    else if(s == 10 && t > 0.2)
    {
        vr_skl_g2 = VR_SKL_Stab(p);
        VR_ET_To(11);
    }
    else if(s == 11 && t > 0.2)
    {
        vr_skl_old = vr_skl_g1; // (behind the head)
        remove(vr_skl_g1);
        vr_skl_g1 = world;
        VR_ET_To(12);
    }
    else if(s == 12 && t > 2.5)
    {
        // Its edict a new gib on the blade: linked at the head, its old predecessor still pointing at it.
        entity again = world;
        for(float k2 = 0; k2 < 400 && again == world; k2++)
        {
            entity e2 = spawn();
            if(e2 == vr_skl_old) { again = e2; } else { e2.think = SUB_Remove; e2.nextthink = time + 0.1; }
        }
        VR_Test_Check(again != world, "the removed gib's edict used again, for a new gib");
        if(again != world)
        {
            vr_skl_g1 = VR_SKL_StabInto(p, again);
        }
        VR_ET_To(13);
    }
    else if(s == 13 && t > 0.2)
    {
        vr_skl_reb = vr_skewer_rebuilds;
        VR_SKL_Shot(p, world); // (a walk: the shot through the air above)
        VR_Test_Check(vr_skl_g1 != world && vr_skewer_rebuilds == vr_skl_reb + 1 && vr_skewer_walked == 2,
            sprintf("a loop (the reused edict skewered again): the walk made the list again (%g) and visits both (%g)",
            vr_skewer_rebuilds - vr_skl_reb, vr_skewer_walked));
        // (The two sit close on the blade: a shot across one may meet the other first, the nearer: either is hit.)
        entity hit4 = VR_SKL_Shot(p, vr_skl_g2);
        VR_Test_Check(hit4 != world && (hit4 == vr_skl_g1 || hit4 == vr_skl_g2), "a shot across them hits one");
        entity rest = hit4 == vr_skl_g1 ? vr_skl_g2 : vr_skl_g1;
        entity hit5 = VR_SKL_Shot(p, rest);
        VR_Test_Check(hit5 == rest && vr_skewer_rebuilds == vr_skl_reb + 1,
            sprintf("... a second across the other hits it, the list not made again (%g)", vr_skewer_rebuilds - vr_skl_reb));
        VR_SKL_Gone(vr_skl_g1);
        VR_SKL_Gone(vr_skl_g2);
        vr_skl_g1 = VR_SKL_Stab(p);
        VR_ET_To(14);
    }
    else if(s == 14 && t > 0.2)
    {
        vr_skl_g2 = VR_SKL_Stab(p);
        VR_ET_To(15);
    }
    else if(s == 15 && t > 0.2)
    {
        VR_Test_Check(vr_skl_g1.vr_skewer_p == p && vr_skl_g2.vr_skewer_p == p, "two gibs on the blade again");
        VRSetHandEmpty(p, cVR_MainHand); // (the blade gone: as a disconnect's or a death's, VR_Skewer_Find finds none)
        VR_ET_To(16);
    }
    else if(s == 16 && t > 0.3)
    {
        VR_SKL_Shot(p, world);
        VR_Test_Check(vr_skl_g1.vr_skewer_p == world && vr_skl_g2.vr_skewer_p == world && vr_skewer_walked == 0 &&
            vr_skewer_head == world, sprintf("the blade gone: both fell off, the walk visits nothing (%g) and the list is empty",
            vr_skewer_walked));
        vr_skl_g1.nextthink = time + 0.05; // (their own fade: SUB_Remove)
        vr_skl_g2.nextthink = time + 0.05;
        VR_ET_To(17);
    }
    else if(s == 17 && t > 0.3)
    {
        VR_SKL_Shot(p, world);
        VR_Test_Check(!vr_skl_g1.modelindex && !vr_skl_g2.modelindex && vr_skewer_walked == 0,
            sprintf("removed by their own fade: the walk still visits nothing (%g)", vr_skewer_walked));
        vr_skl_g1 = world;
        vr_skl_g2 = world;
        VR_SKL_End();
    }
    else if(s == 60 && t > 0.2)
    {
        bprint("skewerlist: ready\n");
        vr_et_on = 0; // (the gibs stay on the blade, the hand gripping: saved so)
        vr_skl_save = 0;
        vr_skl_reb = vr_skewer_rebuilds;
        vr_skl_phase = 1;
        localcmd("save vr_skewerlist_test\nload vr_skewerlist_test\n");
    }
};

// vr_embed_test 51, after the save and the load: the list made again at the load holds both, still on the blade, and a
// shot across one hits it.
void() VR_SKL_AfterLoad =
{
    vr_skl_phase = 0;
    VR_Test_BeginFlags("skewerlist", VR_TEST_LOUD);
    entity p = VR_Player();
    // (On the blade in the hand, or on the sword let go of as the load began, the mock's grip not yet held again: a gib
    // stays on a thrown blade too, vr_skewer_w.)
    VR_Test_Check(vr_skl_g1.vr_skewer_p == p && vr_skl_g2.vr_skewer_p == p, "after the load, both gibs on the blade");
    VR_SKL_Shot(p, world);
    VR_Test_Check(vr_skewer_walked == 2, sprintf("the list made at the load holds both (%g visited)", vr_skewer_walked));
    entity hit = VR_SKL_Shot(p, vr_skl_g2);
    VR_Test_Check(hit == vr_skl_g1 || hit == vr_skl_g2, "a shot across them hits one");
    entity rest = hit == vr_skl_g1 ? vr_skl_g2 : vr_skl_g1;
    entity hit2 = VR_SKL_Shot(p, rest);
    VR_Test_Check(hit2 == rest, "a second across the other hits it");
    localcmd("-grabmain\n");
    VR_SKL_Gone(vr_skl_g1);
    VR_SKL_Gone(vr_skl_g2);
    VR_Test_Verdict();
};
#endif
