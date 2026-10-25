#ifdef QVR_TESTS
// vr_limbs_test.qc -- limb gore's tests (vr_limbs.qc; docs/vr-port/ROUND21.md, "Limb gore").
//
// `vr_limb_test <n>` (Debug > Gore Tests > Limb Gore), on the monster (or corpse) nearest you. 1 every limb of its
// model hung in a row before you (Spawn Its Limbs: the limb models made from its own); 2 a corpse cut apart limb by
// limb, the ends first, down to the torso, then its head; 3 the same, whole limbs first; 4 a sword's killing slash at
// a limb (health 1); 5 a fist's (a pop by chance: vr_decap_pop_roll); 6 a shotgun blast at a limb that kills; 7 a
// lightning bolt's; 8 the chance multipliers' rates (2000 rolls at chance 0.5, the head's and a limb's); 9 an explosion
// beside a limb that kills (health 1); 10 a body gibbed (its limbs thrown: vr_gib_limbs); 11 twice Most Limbs thrown
// (the cap); 12 a sword's slash at a limb at full health (a zombie dies of it); 13 where each limb's middle maps
// (ragdolllimb); 14 the main hand put on the last limb thrown (then grip it: vr_mock_button main grip 1); 15 whether a
// hand holds it; 16 its body's speed; 17 the limbs lying about (where, how fast: none fallen out); 18 the cut pieces'
// own fountains (their spouts, the drops so far); 19 what each limb and the head of every kind of monster here weighs
// cut off (limbpiece: its share of the ragdoll's mass); 20 an enemy enforcer's laser bolt into the nearest living
// monster's head that kills (health 1; 21 at a limb); 22 the nearest living monster held still in frame 17 (a grunt's
// last death frame: lying, its head far from where it stands); 23 where its head is for the head and limb tests: a
// point on its head as animated against its standing head zone's middle (VR_Decap_OnMeleeHead, VR_Decap_OnHead,
// ragdolllimb); 24-28 blunt corpse impacts (PASS/FAIL), 29 a corpse limb's stored worn damage; 30-38 each weapon's hits
// wearing a corpse's limb off (vr_limbs_wear_test.qc); 39 a close look into the nearest corpse's stump, 40 into the
// last limb thrown's cap (VR_Limb_TestLookAtCut). `limbtest:` lines.

void(float xTest) VR_Limb_WearTest; // (vr_limbs_wear_test.qc)

float() VR_Limb_TestCount =
{
    float n = 0;
    for(entity e = find(world, classname, "vr_limb"); e; e = find(e, classname, "vr_limb"))
    {
        n++;
    }
    return n;
};

float() VR_Limb_TestSmallGibs =
{
    float n = 0;
    for(entity e = find(world, classname, "vr_smallgib"); e; e = find(e, classname, "vr_smallgib"))
    {
        n++;
    }
    return n;
};

float vr_limb_test_sgibs; // small gibs before the test's blow

float() VR_Limb_TestFountains =
{
    float n = 0;
    for(entity e = find(world, classname, "vr_decap_fountain"); e; e = find(e, classname, "vr_decap_fountain"))
    {
        n = n + (e.vr_limb_bone >= 0 ? 1 : 0);
    }
    return n;
};

// The nearest monster with limbs (alive: `xLive` 1; dead 0; either -1) within 400 units.
entity(float xLive) VR_Limb_TestTarget =
{
    entity best = world;
    // (A crowd's test: its monster in turn; vr_ragdoll_act's: its target, however far.)
    float bestDist = (vr_gore_test_only != world || vr_test_only != world) ? 999999 : 400;
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(!(e.flags & FL_MONSTER) || e.vr_gib || (xLive == 1 && e.health <= 0) || (xLive == 0 && e.health > 0) ||
           (vr_gore_test_only != world && e != vr_gore_test_only) || VR_TestSkips(e) || ragdolllimbs(e, 0) == 0)
        {
            continue;
        }
        float d = vlen(e.origin - self.origin);
        if(d < bestDist)
        {
            best = e;
            bestDist = d;
        }
    }
    return best;
};

// The limb a test strikes: an arm's lower part if it has one (the elbow), else its first limb joint.
float(entity m) VR_Limb_TestBone =
{
    float top = VR_Limb_TopJoints(m);
    float all = ragdolllimbs(m, 0);
    for(float b = 1; b < 16; b++)
    {
        if(top & VR_Limb_Bit(b))
        {
            for(float c = b + 1; c < 16; c++)
            {
                if((all & VR_Limb_Bit(c)) && ragdolllimbs(m, 5, c) == b)
                {
                    return c;
                }
            }
            return b;
        }
    }
    return -1;
};

// Test aid (vr_limb_test 39, 40; Debug > Gore Tests > Limb Gore): a close look at a cut's flesh, where you look. 39:
// you float (noclip) 14 units from the nearest corpse's first stump, turned to face it from the side it faces (look
// straight ahead, pitched down, first: your head's pitch is kept); 40: the last limb thrown hung in the air 10 units
// before your eyes, its cut end turned to you. Prints `limbtest: 39|40 look ...`.
void(float xTest) VR_Limb_TestLookAtCut =
{
    makevectors(self.headpos != '0 0 0' ? self.v_viewangle : self.v_angle); // (where you look: your head, not your aim)
    vector fwd = v_forward;
    vector eye = self.headpos != '0 0 0' ? self.headpos : self.origin + self.view_ofs;
    if(xTest == 39)
    {
        entity m = VR_Limb_TestTarget(0);
        float stumps = (m != world && isragdoll(m)) ? ragdolllimbs(m, 2) : 0;
        float b = -1;
        for(float k = 1; k < 16 && b < 0; k++)
        {
            b = (stumps & VR_Limb_Bit(k)) ? k : -1;
        }
        if(b < 0)
        {
            VR_Gore_Print("limbtest: 39 look: no corpse with a stump near\n");
            return;
        }
        vector at = ragdollcut(m, 4, b);
        vector out = ragdollcut(m, 5, b);
        // (Turned to face it from the side it faces, your head's pitch kept: the turn is the play space's, so look
        // straight ahead first, pitched down.)
        vector face = '0 0 0';
        face_x = (self.headpos != '0 0 0') ? self.v_viewangle_x : self.v_angle_x;
        face_y = (fabs(out_x) + fabs(out_y) > 0.05) ? vectoyaw(out * -1) : self.v_angle_y;
        self.angles = '0 1 0' * face_y;
        self.fixangle = TRUE;
        makevectors(face);
        fwd = v_forward;
        self.movetype = MOVETYPE_NOCLIP;
        self.velocity = '0 0 0';
        setorigin(self, at - fwd * 14 - (eye - self.origin));
        VR_Gore_Print(sprintf("limbtest: 39 look at %s's stump (joint %g) at %v, facing %v (%.2f to the view)\n", m.classname,
            b, at, ragdollcut(m, 5, b), ragdollcut(m, 5, b) * fwd * -1));
        return;
    }
    entity h = vr_limb_last;
    if(h == world || h.classname != "vr_limb" || h.model == "" || h.vr_limb_end == '0 0 0')
    {
        VR_Gore_Print("limbtest: 40 look: no limb thrown\n");
        return;
    }
    // (Its cut end turned to you: the turn whose end points nearest back along the view.)
    vector best = '0 0 0';
    float bestDot = -2;
    vector ang = '0 0 0';
    for(ang_x = -90; ang_x <= 90; ang_x = ang_x + 15)
    {
        for(ang_y = 0; ang_y < 360; ang_y = ang_y + 15)
        {
            float d = normalize(modelpoint(h, ang, h.vr_limb_end)) * fwd * -1;
            if(d > bestDot)
            {
                bestDot = d;
                best = ang;
            }
        }
    }
    h.movetype = MOVETYPE_NONE;
    h.velocity = '0 0 0';
    h.avelocity = '0 0 0';
    h.vr_rigid = 0;
    h.vr_spin = '0 0 0';
    h.nextthink = time + 120;
    h.angles = best;
    setorigin(h, eye + fwd * 10 - modelpoint(h, best, h.vr_limb_end));
    VR_Gore_Print(sprintf("limbtest: 40 look at %s's cap at %v (%.2f to the view)\n", h.model, eye + fwd * 10, bestDot));
};

void(entity m, string xWhat, float xCuts, float xPops, float xLimbs, float xFountains) VR_Limb_TestReport =
{
    vector parts = isragdoll(m) ? ragdollcut(m, 7) : '0 0 0';
    VR_Gore_Print(sprintf("limbtest: %s on %s: cut %g popped %g, limbs %g -> %g, fountains +%g", xWhat, m.classname,
        vr_limb_cuts - xCuts, vr_limb_pops - xPops, xLimbs, VR_Limb_TestCount(), VR_Limb_TestFountains() - xFountains));
    VR_Gore_Print(sprintf(", health %g ragdoll %g parts %g cut %g gib %g small gibs +%g\n", m.health, isragdoll(m), parts_z, parts_x,
        m.vr_gib, VR_Limb_TestSmallGibs() - vr_limb_test_sgibs));
    if(vr_limb_cuts > xCuts && vr_limb_last != world)
    {
        entity h = vr_limb_last;
        VR_Gore_Print(sprintf("limbtest: limb %s at %v, %.0f u/s, spin %.1f, gib %g rigid %g solid %g\n", h.model, h.origin,
            vlen(h.velocity), vlen(h.vr_spin), h.vr_gib, h.vr_rigid, h.solid));
    }
};

void(float xTest) VR_Limb_Test =
{
    float cuts = vr_limb_cuts;
    float pops = vr_limb_pops;
    float limbs = VR_Limb_TestCount();
    float fountains = VR_Limb_TestFountains();
    vr_limb_test_sgibs = VR_Limb_TestSmallGibs();
    float b;
    float all;
    vector parts;
    entity m;
    makevectors('0 1 0' * self.v_angle_y);
    if(xTest == 8)
    {
        float oldLimb = vr_decap_limb;
        vr_decap_by = world;
        for(float k = 0; k < 2; k++)
        {
            float won = 0;
            for(float i = 0; i < 2000; i++)
            {
                vr_decap_limb = k ? 3 : -1;
                vr_decap_chance = 0.5;
                vr_decap_why = "test";
                won = won + VR_Decap_Roll(self);
            }
            VR_Gore_Print(sprintf("limbtest: 8 %s at chance 0.5 x scale %g: %g of 2000 (%.3f)\n", k ? "limb" : "head",
                cvar_hget(k ? cvarh_vr_limbs_chance_scale : cvarh_vr_decap_chance_scale), won, won / 2000));
        }
        vr_decap_limb = oldLimb;
        return;
    }
    if(xTest == 18)
    {
        // (The pieces' own fountains: each one's age, how far its spout is from its piece's middle, and was from the
        // stump's joint as it was cut; the drops thrown so far.)
        float nf = 0;
        for(entity f = find(world, classname, "vr_limb_fountain"); f; f = find(f, classname, "vr_limb_fountain"))
        {
            entity pc = f.owner;
            vector sp = pc.origin + modelpoint(pc, pc.angles, f.vr_limb_end);
            bprint(sprintf("limbtest: 18 fountain of %s (joint %g) age %.2f of %.2f: spout %v, %.1f from its middle", pc.model,
                f.vr_limb_bone, time - f.ltime, f.wait, sp, vlen(sp - pc.origin)));
            bprint(sprintf(", born %.2f from the stump's joint; piece at %v %.0f u/s, %.2f kg\n", f.dmg, pc.origin,
                vlen(pc.velocity), pc.vr_prop_mass));
            nf++;
        }
        bprint(sprintf("limbtest: 18 %g fountains, %g drops thrown in all\n", nf, vr_limb_endspurts));
        return;
    }
    if(xTest == 39 || xTest == 40)
    {
        VR_Limb_TestLookAtCut(xTest);
        return;
    }
    if(xTest == 19)
    {
        // (Every kind of monster here once: each limb joint's piece (what a cut there takes) and the head's, kg and share.)
        for(entity e = nextent(world); e; e = nextent(e))
        {
            if(!(e.flags & FL_MONSTER) || e.vr_gib || ragdolllimbs(e, 0) == 0)
            {
                continue;
            }
            float dup = FALSE;
            for(entity o = nextent(world); o != e && !dup; o = nextent(o))
            {
                dup = ((o.flags & FL_MONSTER) && !o.vr_gib && (o.classname == e.classname) && (ragdolllimbs(o, 0) != 0));
            }
            if(dup)
            {
                continue;
            }
            vector hp = limbpiece(e, -1, 0);
            float top = VR_Limb_TopJoints(e);
            float topShare = 0;
            string line = sprintf("limbtest: 19 %s (ragdoll %.0f kg, x%g): head %.1f kg %.1f%%", e.classname, hp_y,
                cvar_hget(cvarh_vr_limbs_mass_scale), hp_x, hp_z * 100);
            all = ragdolllimbs(e, 0);
            for(b = 1; b < 16; b++)
            {
                if(all & VR_Limb_Bit(b))
                {
                    vector lp = limbpiece(e, b, 0);
                    line = strcat(line, sprintf(", %g%s %.1f kg %.1f%%", b, (top & VR_Limb_Bit(b)) ? "*" : "", lp_x, lp_z * 100));
                    topShare = topShare + ((top & VR_Limb_Bit(b)) ? lp_z : 0);
                }
            }
            bprint(sprintf("%s; whole limbs (*) and head %.1f%% of it\n", line, (topShare + hp_z) * 100));
        }
        return;
    }
    if(xTest >= 30 && xTest <= 38)
    {
        VR_Limb_WearTest(xTest);
        return;
    }
    if(xTest == 29)
    {
        // The per-joint blunt damage stored on the nearest corpse (after test 28 and a save/load: it persists).
        m = VR_Limb_TestTarget(0);
        if(m == world)
        {
            bprint("limbtest: 29: no corpse with limbs near\n");
            return;
        }
        b = VR_Limb_TestBone(m);
        bprint(sprintf("limbtest: 29 %s joint %g stored worn damage %.1f (joint 0 %.1f) %s\n", m.classname, b,
            m.vr_limb_corpse_dmg[b], m.vr_limb_corpse_dmg[0], m.vr_limb_corpse_dmg[b] > 0 ? "PASS" : "FAIL"));
        return;
    }
    if(xTest >= 24 && xTest <= 28)
    {
        m = VR_Limb_TestTarget(0);
        if(m == world)
        {
            bprint("limbtest: corpse impact: no corpse with limbs near\n");
            return;
        }
        b = VR_Limb_TestBone(m);
        vector at = limbplace(m, b, 3);
        float hpBefore = m.vr_corpse_hp;
        float gibBefore = m.vr_gib;
        if(xTest == 27)
        {
            float oldWeapon = VRGetEntWeapon(self, cVR_MainHand);
            VRSetEntWeapon(self, cVR_MainHand, WID_AXE);
            VR_Decap_TestBlow(at, VR_MTHING_AXE, VR_MKIND_SLASH, 0.85, 0, 8);
            float cutOk = VR_Limb_CorpseBlow(m, cVR_MainHand, at);
            VRSetEntWeapon(self, cVR_MainHand, oldWeapon);
            VR_Gore_Print(sprintf("limbtest: 27 axe corpse slash %g\n", cutOk));
        }
        else
        {
            entity impact = self;
            if(xTest == 26)
            {
                impact = spawn();
                impact.classname = "vr_limbtest_prop";
                impact.vr_gib = TRUE; // a physical thing: QVR_CDMG_PROPS, as a thrown brick
                setorigin(impact, at);
                impact.velocity = v_forward * 300;
            }
            float kind = xTest == 26 ? QVR_CDMG_PROPS : QVR_CDMG_BLUNT;
            float mult = VR_Corpse_DamageKindMult(kind);
            VR_Corpse_Parts(m);
            float hp = VR_Corpse_Health();
            if(xTest == 25)
            {
                at = limbplace(m, 0, 3); // pelvis surface: a torso hit, even with ten times the body's health
            }
            for(float hit = 0; hit < ((xTest == 25 || xTest == 28) ? 1 : 2); hit++)
            {
                VR_SetHitKind(xTest == 26 ? QVR_HITKIND_NONE : QVR_HITKIND_MELEE, -1, at, v_forward);
                float damage = hp * (xTest == 25 ? 10 : 0.6) * VR_Corpse_PositionalFactor(m) / max(0.001, mult);
                T_Damage(m, impact, self, damage);
                VR_ClearHitKind();
            }
            if(impact != self)
            {
                remove(impact);
            }
        }
        VR_Gore_Print(sprintf("limbtest: %g corpse hp %.1f -> %.1f, joint %g\n", xTest, hpBefore, m.vr_corpse_hp, b));
        VR_Limb_TestReport(m, "corpse impact", cuts, pops, limbs, fountains);
        // 24/26: the struck limb pops, the body's health untouched; 25: a torso blow of ten times the corpse's health
        // gibs it whole; 27: the axe cuts the limb; 28: one blow, nothing pops (its damage is kept: 29).
        float cutN = vr_limb_cuts - cuts;
        float popN = vr_limb_pops - pops;
        float bodyKept = (m.vr_corpse_hp == hpBefore && m.vr_gib == gibBefore);
        float ok;
        if(xTest == 25)
        {
            ok = m.vr_corpse_hp <= 0;
        }
        else if(xTest == 27)
        {
            ok = (cutN == 1 && popN == 0 && bodyKept);
        }
        else if(xTest == 28)
        {
            ok = (cutN == 0 && popN == 0 && bodyKept && m.vr_limb_corpse_dmg[b] > 0);
        }
        else
        {
            ok = (popN == 1 && cutN == 0 && bodyKept);
        }
        bprint(sprintf("limbtest: %g corpse impact %s (cut %g popped %g, hp %.1f -> %.1f)\n", xTest, ok ? "PASS" : "FAIL",
            cutN, popN, hpBefore, m.vr_corpse_hp));
        return;
    }
    if(xTest == 22 || xTest == 23)
    {
        m = VR_Limb_TestTarget(1);
        if(m == world)
        {
            bprint(sprintf("limbtest: %g: no living monster with limbs near\n", xTest));
            return;
        }
        if(xTest == 22)
        {
            m.frame = 17;
            m.think = SUB_Null;
            m.nextthink = 0;
            m.velocity = '0 0 0';
            bprint(sprintf("limbtest: 22 %s held in frame %g\n", m.classname, m.frame));
            return;
        }
        float hb = ragdolllimbs(m, 3);
        vector animated = limbplace(m, hb, 3); // (a point on its head bone's surface, as its frame has it)
        if(!VR_Decap_HeadZone(m))
        {
            bprint(sprintf("limbtest: 23 %s has no head zone\n", m.classname));
            return;
        }
        float hyaw = m.angles_y * (3.14159265 / 180);
        vector standing = m.origin + '1 0 0' * (cos(hyaw) * xHeadForward) + '0 1 0' * (sin(hyaw) * xHeadForward) +
            VR_HeadSide('1 0 0' * cos(hyaw) + '0 1 0' * sin(hyaw)) + '0 0 1' * xHeadUp;
        bprint(sprintf("limbtest: 23 %s frame %g: its head as animated %v, its standing head zone's middle %v (%.1f apart)\n",
            m.classname, m.frame, animated, standing, vlen(animated - standing)));
        bprint(sprintf("limbtest: 23 melee beheading zone: animated %g, standing %g; head zone: animated %g, standing %g",
            VR_Decap_OnMeleeHead(m, animated, '0 0 0'), VR_Decap_OnMeleeHead(m, standing, '0 0 0'),
            VR_Decap_OnHead(m, animated, '0 0 0'), VR_Decap_OnHead(m, standing, '0 0 0')));
        bprint(sprintf("; limb joint: animated %g, standing %g (head bone %g)\n", ragdolllimb(m, animated),
            ragdolllimb(m, standing), hb));
        return;
    }
    if(xTest == 20 || xTest == 21)
    {
        // (As an enemy enforcer's bolt strikes: Laser_Touch, from a shooter of its own 64 units off: of ten ways in (level
        // all round, from above and below), the first whose line meets the head (or the limb) first.)
        m = VR_Limb_TestTarget(1);
        if(m == world)
        {
            bprint(sprintf("limbtest: %g: no living monster with limbs near\n", xTest));
            return;
        }
        float lb = (xTest == 20) ? ragdolllimbs(m, 3) : VR_Limb_TestBone(m);
        vector la = (xTest == 20) ? limbplace(m, lb, 3) : limbplace(m, lb, 0);
        float lpops = vr_decap_pops;
        float lrolls = vr_decap_rolls;
        m.health = 1;
        entity shooter = spawn();
        shooter.classname = "monster_enforcer";
        setorigin(shooter, la);
        entity oself = self;
        self = shooter;
        entity bolt = LaunchLaser(shooter.origin, '1 0 0');
        self = bolt;
        vector bd = '1 0 0';
        float clear = FALSE;
        for(float w = 0; w < 10 && !clear; w++)
        {
            float wa = w * 0.785398;
            bd_x = (w < 8) ? cos(wa) : 0;
            bd_y = (w < 8) ? sin(wa) : 0;
            bd_z = (w < 8) ? 0 : ((w == 8) ? -1 : 1);
            VR_TouchTrace(la - bd * 64, la + bd * 16);
            clear = ((trace_ent == m) && ((xTest == 20) ? VR_Decap_OnHead(m, trace_endpos, bd) : (VR_Limb_At(m, trace_endpos) == lb && !VR_Decap_OnHead(m, trace_endpos, bd))));
        }
        setorigin(shooter, la - bd * 64);
        setorigin(bolt, trace_endpos - bd * 2);
        bolt.velocity = bd * 600;
        other = m;
        dprint(sprintf("limbtest: %g bolt going %v (%s): its line meets %s at %v (limb %g)\n", xTest, bd, clear ? "clear" : "no clear line",
            trace_ent.classname, trace_endpos, ragdolllimb(m, trace_endpos)));
        Laser_Touch();
        self = oself;
        remove(shooter);
        bprint(sprintf("limbtest: %g laser at %s's %s (joint %g, maps to %g): health %g", xTest, m.classname,
            (xTest == 20) ? "head" : "limb", lb, ragdolllimb(m, la), m.health));
        bprint(sprintf(", head popped %g, limbs popped %g, rolls %g\n", vr_decap_pops - lpops, vr_limb_pops - pops,
            vr_decap_rolls - lrolls));
        return;
    }
    if(xTest == 17)
    {
        // (The limbs lying about: each one's model, where it is, how fast: none fallen out of the world.)
        for(entity e = find(world, classname, "vr_limb"); e; e = find(e, classname, "vr_limb"))
        {
            string inSolid = "";
            if(pointcontents(e.origin) == CONTENT_SOLID)
            {
                inSolid = " (in the world's solid)";
            }
            VR_Gore_Print(sprintf("limbtest: 17 %s at %v, %.0f u/s%s\n", e.model, e.origin, vlen(e.velocity), inSolid));
        }
        return;
    }
    if(xTest == 16)
    {
        // (The nearest dead monster's pelvis: how fast its body goes a few frames after the blow: vr_limbs_body_speed.)
        m = VR_Limb_TestTarget(0);
        if(m == world)
        {
            m = VR_Decap_TestTarget(FALSE);
        }
        VR_Gore_Print(sprintf("limbtest: 16 %s's body at %.0f u/s (ragdoll %g)\n", m.classname, vlen(limbplace(m, 0, 2)), isragdoll(m)));
        return;
    }
    if(xTest == 14 || xTest == 15)
    {
        entity h = vr_limb_last;
        if(h == world || h.classname != "vr_limb")
        {
            VR_Gore_Print("limbtest: no limb thrown\n");
            return;
        }
        if(xTest == 14)
        {
            // (It brought before you, within the mock hand's reach, lying still; the hand on it.)
            // (Hung before you at chest height, as Spawn Its Limbs hangs them: the mock hand doesn't reach the floor.)
            setorigin(h, self.origin + v_forward * 16 + '0 0 8');
            h.velocity = '0 0 0';
            h.vr_spin = '0 0 0';
            h.movetype = MOVETYPE_NONE;
            h.vr_rigid = 0;
            VR_Gore_Print(sprintf("limbtest: 14 %s brought before you at %v (you at %v)\n", h.model, h.origin, self.origin));
        }
        else
        {
            VR_Gore_Print(sprintf("limbtest: 15 %s held %g (by %s), at %v, a hand %.1f units from it\n", h.model,
                h.carry_player != world, h.carry_player.classname, h.origin, min(vlen(VRGetHandPos(cVR_MainHand) - h.origin),
                vlen(VRGetHandPos(cVR_OffHand) - h.origin))));
        }
        return;
    }
    if(xTest == 11)
    {
        float most = cvar_hget(cvarh_vr_limbs_max);
        m = VR_Limb_TestTarget(-1);
        if(m == world)
        {
            VR_Gore_Print("limbtest: 11: no monster near\n");
            return;
        }
        b = VR_Limb_TestBone(m);
        for(float j = 0; j < most * 2; j++)
        {
            VR_Limb_Throw(m, limbmodel(m, b), b, self.origin + v_forward * 64 + '0 0 40' + v_right * (j * 4 - most * 4), '0 0 0',
                '0 0 50', '0 0 0');
        }
        VR_Gore_Print(sprintf("limbtest: 11 threw %g limbs, Most Limbs %g: %g lie about\n", most * 2, most, VR_Limb_TestCount()));
        return;
    }
    m = VR_Limb_TestTarget((xTest == 2 || xTest == 3) ? 0 : ((xTest == 1 || xTest == 13) ? -1 : 1));
    if(m == world)
    {
        VR_Gore_Print(sprintf("limbtest: %g: no monster with limbs near\n", xTest));
        return;
    }
    if(xTest == 1)
    {
        all = ragdolllimbs(m, 0);
        float n = 0;
        for(b = 1; b < 16; b++)
        {
            if(!(all & VR_Limb_Bit(b)))
            {
                continue;
            }
            string lm = limbmodel(m, b);
            VR_Gore_Print(sprintf("limbtest: 1 %s limb %g: %s\n", m.classname, b, lm));
            if(lm != "")
            {
                entity t = VR_Limb_Throw(m, lm, b, self.origin + self.view_ofs + v_forward * 48 + v_right * (n * 12 - 50) - '0 0 6', '0 0 0',
                    '0 0 0', '0 0 0');
                t.movetype = MOVETYPE_NONE; // (they hang there to be looked at)
                t.vr_rigid = 0;
                n++;
            }
        }
        return;
    }
    if(xTest == 13)
    {
        all = ragdolllimbs(m, 0);
        for(b = 1; b < 16; b++)
        {
            if(all & VR_Limb_Bit(b))
            {
                vector p = limbplace(m, b, 3);
                VR_Gore_Print(sprintf("limbtest: 13 %s limb %g middle %v maps to %g\n", m.classname, b, p, ragdolllimb(m, p)));
            }
        }
        return;
    }
    vector vel = v_forward * 300;
    if(xTest == 2 || xTest == 3)
    {
        // Down to the torso: the ends first (the highest joint left), or whole limbs (the top joints).
        for(float step = 0; step < 16; step++)
        {
            all = ragdolllimbs(m, 0);
            if(!all)
            {
                break;
            }
            float pick = -1;
            float pool = (xTest == 3) ? VR_Limb_TopJoints(m) : all;
            for(b = 1; b < 16; b++)
            {
                if(pool & VR_Limb_Bit(b))
                {
                    pick = b;
                    if(xTest == 3)
                    {
                        break;
                    }
                }
            }
            float ok = VR_Limb_Cut(m, self, vel, QVR_DECAP_SWING, pick, FALSE);
            parts = ragdollcut(m, 7);
            VR_Gore_Print(sprintf("limbtest: %g cut limb %g: %g, parts left %g, cut bones %g, model %s\n", xTest, pick, ok, parts_z,
                parts_x, vr_limb_last.model));
            if(!ok)
            {
                break;
            }
        }
        float headOk = VR_Decap_Allowed(m) ? VR_Decap_Cut(m, self, vel, QVR_DECAP_SWING) : 0;
        parts = ragdollcut(m, 7);
        VR_Gore_Print(sprintf("limbtest: %g done: head %g, parts left %g (the torso), limbs left %g\n", xTest, headOk, parts_z,
            ragdolllimbs(m, 0)));
        VR_Limb_TestReport(m, "dismembered", cuts, pops, limbs, fountains);
        return;
    }
    b = VR_Limb_TestBone(m);
    vector at = limbplace(m, b, 3); // (a point on its surface)
    float mapped = ragdolllimb(m, at);
    string what = "";
    if(xTest == 10)
    {
        what = "gibbed";
        T_Damage(m, self, self, 1000);
    }
    else if(xTest == 9)
    {
        what = "an explosion by a limb";
        m.health = 1;
        entity blast = spawn();
        setorigin(blast, at + normalize(at - (m.origin + '0 0 8')) * 12);
        T_RadiusDamage(blast, self, 60, world);
        remove(blast);
    }
    else if(xTest == 6 || xTest == 7)
    {
        what = (xTest == 6) ? "a shotgun blast at a limb" : "a lightning bolt at a limb";
        m.health = 1;
        if(VR_Limb_IsZombie(m))
        {
            m.health = 60;
        }
        VR_Decap_TestShot(xTest == 6 ? QVR_DECAP_SHOTGUN : QVR_DECAP_LIGHTNING, at);
    }
    else if(xTest == 5)
    {
        what = "a fist at a limb";
        m.health = 1;
        VR_Decap_TestBlow(at, VR_MTHING_FIST, VR_MKIND_POMMEL, 0.7, 0, 8);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        T_Damage_VRMelee(m, self, self, 30);
        VR_ClearHitKind();
    }
    else
    {
        what = (xTest == 12) ? "a slash at a limb at full health" : "a sword's slash at a limb";
        if(xTest != 12)
        {
            m.health = 1;
        }
        VR_Decap_TestBlow(at, VR_MTHING_SWORD, VR_MKIND_SLASH, 0.7, 0, 8);
        VR_SetHitKind(QVR_HITKIND_MELEE, cVR_MainHand, at, normalize(at - self.origin));
        T_Damage_VRMelee(m, self, self, (xTest == 12) ? 10 : 30);
        VR_ClearHitKind();
    }
    VR_Gore_Print(sprintf("limbtest: %g at limb %g (maps to %g)\n", xTest, b, mapped));
    VR_Limb_TestReport(m, what, cuts, pops, limbs, fountains);
};

// Test aid `vr_test_cut <n>` (console; Debug > World and Physics; Misc/quakevr/ragdoll/dismember_test.sh; ROUND22.md,
// "Active ragdolls: dismemberment"): a sword's cut at the nearest monster's limb, found by its bones' names (ragdolllimbs
// 6): n 1 its left arm (the shoulder), 2 its right arm, 3 its left leg (the hip), 4 its right leg, 5 both legs, 6 its left
// forearm (the elbow), 7 its left shin (the knee), 8 its head. The live one nearest you killed by it (its last point of
// health: Limb gore's killing blow, armed as a slash's), or with n + 10 the dead one nearest you cut so (a body still
// dying: cut off as it falls). Prints `test cut:`.
void(float want) VR_TestCut;
void() VR_TestCut_Frame =
{
    float want = cvar("vr_test_cut");
    if(want < 1)
    {
        return;
    }
    cvar_set("vr_test_cut", "0");
    entity pl = find(world, classname, "player");
    if(pl == world)
    {
        return;
    }
    entity oself = self;
    self = pl;
    VR_TestCut(want);
    self = oself;
};

void(float want) VR_TestCut =
{
    float dead = (want > 10);
    float n = dead ? (want - 10) : want;
    entity m = VR_Limb_TestTarget(dead ? 0 : 1);
    if(m == world)
    {
        bprint(sprintf("test cut: no %s monster with limbs near\n", dead ? "dead" : "live"));
        return;
    }
    float b = -1;
    float b2 = -1;
    if(n == 8)
    {
        b = ragdolllimbs(m, 3);
    }
    else if(n == 5)
    {
        b = ragdolllimbs(m, 6, 2);
        b2 = ragdolllimbs(m, 6, 3);
    }
    else
    {
        b = ragdolllimbs(m, 6, (n <= 4) ? (n - 1) : ((n == 6) ? 4 : 6));
    }
    if(b <= 0)
    {
        bprint(sprintf("test cut: %s has no such limb (%g)\n", m.classname, n));
        return;
    }
    makevectors('0 1 0' * self.v_angle_y);
    vector vel = v_forward * 300;
    float ok;
    if(dead)
    {
        ok = (n == 8) ? VR_Decap_Cut(m, self, vel, QVR_DECAP_SWING) : VR_Limb_Cut(m, self, vel, QVR_DECAP_SWING, b, FALSE);
    }
    else
    {
        vector at = limbplace(m, b, 3);
        m.health = 1;
        if(n == 8)
        {
            ok = VR_Decap_Arm(m, self, at, vel, QVR_DECAP_SWING);
        }
        else
        {
            ok = VR_Limb_Arm(m, self, at, vel, QVR_DECAP_SWING, b);
        }
        T_Damage(m, self, self, 20);
        ok = (ok && (m.health <= 0) && isragdoll(m));
    }
    if(ok && b2 > 0)
    {
        ok = VR_Limb_Cut(m, self, vel, QVR_DECAP_SWING, b2, FALSE);
    }
    bprint(sprintf("test cut: %s (%s) at %v: %g joint %g%s: %s\n", m.classname, dead ? "dead" : "live",
        m.origin, n, b, (b2 > 0) ? sprintf(" and %g", b2) : "", ok ? "cut" : "NOT cut"));
};

// Every server frame (StartFrame): a limb test asked for (vr_limb_test).
void() VR_Limb_TestFrame =
{
    VR_TestCut_Frame(); // (the active ragdolls' dismemberment tests: vr_test_cut)
    float ask = cvar_hget(cvarh_vr_limb_test);
    if(!ask)
    {
        return;
    }
    cvar_set("vr_limb_test", "0");
    entity p = find(world, classname, "player");
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    if(cvar("vr_gore_test_crowd") > 0)
    {
        VR_Gore_TestCrowd(ask, FALSE);
        self = oself;
        return;
    }
    VR_Limb_Test(ask);
    self = oself;
};

// vr_limb_test or vr_decap_test (`xDecap`) `xTest` on every monster within vr_gore_test_crowd units of you (`self`),
// each in turn as if it were the nearest, in one frame, quietly: one `goretest: crowd` line. (A blow's new entities
// are not monsters; one killed or gibbed by a blow before its turn is passed over by the test's own check.)
void(float xTest, float xDecap) VR_Gore_TestCrowd =
{
    float radius = cvar("vr_gore_test_crowd");
    float cuts = vr_limb_cuts;
    float pops = vr_limb_pops;
    float heads = vr_decap_count;
    float headPops = vr_decap_pops;
    float n = 0;
    vr_gore_test_quiet = TRUE;
    for(entity m = nextent(world); m; m = nextent(m))
    {
        if(!(m.flags & FL_MONSTER) || vlen(m.origin - self.origin) > radius)
        {
            continue;
        }
        vr_gore_test_only = m;
        if(xDecap)
        {
            VR_Decap_Test(xTest);
        }
        else
        {
            VR_Limb_Test(xTest);
        }
        n++;
    }
    vr_gore_test_only = world;
    vr_gore_test_quiet = FALSE;
    bprint(sprintf("goretest: crowd %s %g on %g monsters within %g: limbs cut %g popped %g, heads cut %g",
        xDecap ? "vr_decap_test" : "vr_limb_test", xTest, n, radius, vr_limb_cuts - cuts, vr_limb_pops - pops,
        vr_decap_count - heads));
    bprint(sprintf(" popped %g; limbs lying %g\n", vr_decap_pops - headPops, VR_Limb_TestCount()));
};
#endif
