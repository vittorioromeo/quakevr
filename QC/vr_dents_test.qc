#ifdef QVR_TESTS
// vr_dents_test.qc -- every hit dents (ROUND22.md, "Every hit dents"): `vr_dents_test <n>` (Debug > Gore Tests) strikes
// the monster or corpse nearest you (within 512 units) as one damage path does, where your eyes look at its middle, and
// prints the wound event it sent (vr_wounds.qc: its kind; "blunt" for a blunt blow's round dent). The engine then dents it
// (vr_gore_dents_info; vr_wounds_debug 1 prints each dent).
//   1  a brick thrown into it (forcegrabbable_touch, as a thrown prop's touch)
//   2  a hand's blow of 25 (the weapon you hold: a blade cuts, a fist or a gun is blunt; on a corpse, its strike's T_Damage)
//   3  the same as a pommel strike (a sword's hilt or pommel: blunt)
//   4  a sword held by its blade swung pommel first into it (a corpse: VR_Corpse_StrikeAlong + VR_Corpse_Struck, the
//      ragdoll strike's own path; the line laid from its pommel inside the body out towards you)
//   5  the same sword held by its handle, its blade into it (a cut, not blunt)
//   6  five shotgun pellets round that point (VR_Wound_Pellet: the wound events alone, no damage): their dents, and their
//      blood streams (vr_gore_streams_chance)

entity vr_dtest_target;
vector vr_dtest_at;
vector vr_dtest_dir;

// The monster or corpse nearest `p` and where its eyes' line to its middle meets its model; FALSE for none.
float(entity p) VR_DentsTest_Find =
{
    entity best = world;
    float bestD = 512;
    for(entity e = findflags(world, flags, FL_MONSTER); e; e = findflags(e, flags, FL_MONSTER))
    {
        float d = vlen((e.absmin + e.absmax) * 0.5 - p.origin);
        if(e.takedamage && !e.vr_gib && d < bestD)
        {
            best = e;
            bestD = d;
        }
    }
    if(best == world)
    {
        return FALSE;
    }
    vector eye = p.origin + p.view_ofs;
    vector mid = (best.absmin + best.absmax) * 0.5;
    vr_dtest_dir = normalize(mid - eye);
    traceline(eye, mid + vr_dtest_dir * 64, MOVE_HITMODEL_MELEE, p);
    vr_dtest_at = trace_ent == best ? trace_endpos : mid;
    vr_dtest_target = best;
    return TRUE;
};

// A sword's melee line (vr_mline_*) for test 4 and 5: 40 units long, `xByBlade` held at 0.9 of the way by its blade with its
// pommel 2 units inside the body at vr_dtest_at, else by its handle with its tip 4 units inside it; out towards `xFrom`.
void(vector xFrom, float xByBlade) VR_DentsTest_SwordLine =
{
    vector u = normalize(xFrom - vr_dtest_at);
    // Its surface on that line (the eyes' trace may have missed its model: then its middle).
    traceline(vr_dtest_at + u * 80, vr_dtest_at, MOVE_HITGIBS | MOVE_HITMODEL_MELEE, self);
    if(trace_ent == vr_dtest_target)
    {
        vr_dtest_at = trace_endpos;
    }
    float len = 40;
    float back = VR_Melee_NearBack(VR_MTHING_SWORD);
    if(xByBlade)
    {
        vector e1 = vr_dtest_at - u * 2;
        vector h = e1 + u * len * back;
        vector t = h + u * len;
        vr_mline_free = TRUE;
        vr_mline_back = back;
        vr_mline_hs = 0.9;
        vr_mline_tip = FALSE;
        vr_mline_grip = e1 + (t - e1) * 0.9;
        vr_mline_far = e1;
        vr_mline_near = t;
    }
    else
    {
        vr_mline_free = FALSE;
        vr_mline_far = vr_dtest_at - u * 4;
        vr_mline_grip = vr_mline_far + u * len;
        vr_mline_near = vr_mline_grip + u * len * back;
    }
};

void() VR_DentsTest_Frame =
{
    float n = cvar("vr_dents_test");
    if(!n)
    {
        return;
    }
    cvar_set("vr_dents_test", "0");
    entity p = VR_Player();
    if(p == world || !VR_DentsTest_Find(p))
    {
        bprint("dentstest: no monster near\n");
        return;
    }
    entity m = vr_dtest_target;
    float corpse = m.vr_corpse == 2 || m.vr_corpse_dying;
    float sent = vr_wound_sent;
    float hp = corpse ? m.vr_corpse_hp : m.health;
    vr_wound_lastkind = 0;
    string what = "";
    entity oldSelf = self;
    entity oldOther = other;
    self = p;
    if(n == 1)
    {
        what = "a thrown brick";
        entity b = spawn();
        setmodel(b, "progs/vr_brick1.mdl");
        setsize(b, '-3 -3 -2', '3 3 2');
        setorigin(b, vr_dtest_at - vr_dtest_dir * 4);
        VR_Debris_Setup(b, 2);
        VR_Carry_Throw(b, vr_dtest_dir * 700);
        self = b;
        other = m;
        forcegrabbable_touch();
    }
    else if(n == 2 || n == 3)
    {
        what = n == 2 ? "a hand's blow" : "a pommel strike";
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, vr_dtest_at, vr_dtest_dir);
        vr_melee_blunt = n == 3;
        if(corpse)
        {
            T_Damage(m, p, p, 25); // (VR_Corpse_Struck's)
        }
        else
        {
            T_Damage_VRMelee(m, p, p, 25);
        }
        vr_melee_blunt = FALSE;
        VR_ClearHitKind();
    }
    else if(n == 6)
    {
        // Five pellets round the point (the wound events alone: no damage), a blast's spread up close.
        what = "five shotgun pellets";
        vector side = '0 0 0';
        side_x = vr_dtest_dir_y;
        side_y = -vr_dtest_dir_x;
        side = normalize(side);
        for(float i = 0; i < 5; i++)
        {
            vector off = side * ((i - 2) * 3) + '0 0 1' * (i * 1.5 - 3);
            VR_Wound_Pellet(m, vr_dtest_at + off, vr_dtest_dir, 4);
        }
    }
    else if(n == 4 || n == 5)
    {
        what = n == 4 ? "a sword by its blade, pommel first" : "a sword by its handle, blade first";
        VR_DentsTest_SwordLine(p.origin + p.view_ofs, n == 4);
        entity c = VR_Corpse_StrikeAlong(p, VR_MTHING_SWORD, vr_dtest_dir);
        if(c == world)
        {
            bprint(sprintf("dentstest: %d (%s) on %s: the line struck no corpse%s\n", n, what, m.classname,
                corpse ? "" : " (it lives: the living's melee is vr_melee.qc's)"));
        }
        else
        {
            m.vr_corpse_struck = 0;
            VR_Corpse_Struck(cVR_MainHand, c, vr_cstrike_at, vr_dtest_dir, 400, vr_cstrike_blunt);
            what = sprintf("%s (struck%s, %.2f of the way from its handle to its tip)", what, vr_cstrike_blunt ? " blunt" : "",
                vr_cstrike_fh);
        }
    }
    self = oldSelf;
    other = oldOther;
    float k = vr_wound_lastkind;
    string kn = k == 0 ? "none" : (floor(k) == QVR_WOUND_MELEE ? (k != floor(k) ? "melee, blunt" : "melee, a cut") :
                (floor(k) == QVR_WOUND_NAIL ? "nail" : ftos(k)));
    // (sprintf: eight arguments at most)
    bprint(sprintf("dentstest: %d %s on %s (%s; main hand weapon %d): ", n, what, m.classname, corpse ? "corpse" : "alive",
        VRGetEntWeapon(p, cVR_MainHand)));
    bprint(sprintf("wound events +%d (%s), %s %.0f -> %.0f\n", vr_wound_sent - sent, kn, corpse ? "corpse hp" : "health",
        hp, corpse ? m.vr_corpse_hp : m.health));
};
#endif
