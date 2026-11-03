// vr_armedgrunt_impact_test.qc -- vr_agrunt_test 202: armed humanoids' impact rule (VR_AG_FallImpact: sustained speed, a
// real stop, real contact; his words 2026-11-02, ROUND23.md "Armed humanoids' impact rule"). On a map with a flat wall in
// sight of its start (vrfiringrange; vrtesthall has none: VR_AGIT_FindWall). A fresh shotgun humanoid each case, you
// unseen, its get-up held off:
//   keeps it: a small push lying; you walking into it lying (5 and 10 m/s); a gentle topple; held and swung, the hand
//   reversing in mid-air. Drops it: a fall from height; a throw into the floor; a throw into a wall; held and banged into
//   a wall. PASS: each as said, and a drop only after an impact over vr_agrunt_fall_keep_impact (never without one). A case
//   ends as soon as it drops or lies still (a give-up at 5 s fails it). scratch1 n > 0: case n - 1 alone. scratch4 n: the
//   whole list n times (the physics varies run to run). scratch2 1: held by the head; scratch3: the hand's m/s.
//   vr_agrunt_debug 2, developer 1: its body's speeds each frame ("fall: speed ...").
#ifdef QVR_TESTS
float vr_agit_case;
float vr_agit_t0;      // the case's motion began
float vr_agit_still;   // frames it has lain still
float vr_agit_drop_at; // when its weapon left its hand (0: kept)
float vr_agit_swing;   // the hand's motion: its phase
vector vr_agit_home;   // where you started
vector vr_agit_fw;     // towards the wall you face (level)
vector vr_agit_wall;   // the wall's point ahead of you
float vr_agit_held;    // a hand had it during the case
float vr_agit_keep_yank;
float vr_agit_round;   // the list's rounds done (scratch4)
float VR_AGIT_PUSH = 0;
float VR_AGIT_WALK = 1;
float VR_AGIT_RUN = 2;
float VR_AGIT_TOPPLE = 3;
float VR_AGIT_SWING = 4;
float VR_AGIT_FALL = 5;
float VR_AGIT_FLOOR = 6;
float VR_AGIT_WALL = 7;
float VR_AGIT_BANG = 8;
float VR_AGIT_PRESS = 9;       // a gentle press with a real grip: 9 torso, 10 head side, 11 a limb, on the floor; 12 torso, on a wall
float VR_AGIT_PRESS_WALL = 12;
float VR_AGIT_CASES = 13;
float vr_agit_health;  // the press: its health as your hand arrived
float vr_agit_dealt;   // ... the impacts dealt till then (vr_rimp_dealt)
float vr_agit_phase;   // ... how far along
float VR_AGIT_ARM = 32;   // units: the reach you hold a body at, and the swing's length each way
float VR_AGIT_HAND = 8;   // m/s: the hand's swing (a fast swing's; the body it drives is slower)
float VR_AGIT_GIVEUP = 5; // s

string(float c) VR_AGIT_Name =
{
    return c == VR_AGIT_PUSH ? "small push, lying" : c == VR_AGIT_WALK ? "you walking into it at 5 m/s, lying" :
           c == VR_AGIT_RUN ? "you walking into it at 10 m/s, lying" : c == VR_AGIT_TOPPLE ? "gentle topple" :
           c == VR_AGIT_SWING ? "held, the hand reversed in mid-air" : c == VR_AGIT_FALL ? "fall from height" :
           c == VR_AGIT_FLOOR ? "thrown into the floor" : c == VR_AGIT_WALL ? "thrown into a wall" :
           c == VR_AGIT_BANG ? "held, banged into a wall" : c == VR_AGIT_PRESS ? "gentle press, its torso on the floor" :
           c == VR_AGIT_PRESS + 1 ? "gentle press, part 1 on the floor" : c == VR_AGIT_PRESS + 2 ? "gentle press, part 8 on the floor" :
           "gentle press, its torso on a wall";
};

// To your right, the wall ahead.
vector() VR_AGIT_Side = { return '1 0 0' * vr_agit_fw_y - '0 1 0' * vr_agit_fw_x; };

// Your hand to a world point over `xFor` s (0: at once).
void(vector xAt, float xFor) VR_AGIT_HandTo =
{
    localcmd(sprintf("vr_mock_hand_glide %g\nvr_mock_hand_to main %s\n", xFor, VR_AGTP_Xyz(xAt)));
};

// The held cases: you take it by its torso, draw your hand back and swing it (towards the wall ahead, `bang`, or across
// the open hall and back). Done (TRUE): the swing over.
float(entity p, entity g, float t, float bang) VR_AGIT_Swing =
{
    // (At your side: the body clear of you; ahead through the wall when banging.)
    vector mid = p.origin + VR_AGIT_Side() * VR_AGIT_ARM + '0 0 24';
    vector back = mid - vr_agit_fw * 32; // (a good swing's way: your grip gets the body up to speed before your hand arrives)
    vector far = mid + vr_agit_fw * 72;
    if(vr_agit_swing >= 4 && cvar_hget(cvarh_vr_agrunt_debug) >= 2)
    {
        traceline(p.handpos, p.handpos + vr_agit_fw * 128, TRUE, g); // (the rule's: VR_AG_FallImpact)
        VR_AGT_Print(sprintf("hand %.0f off the wall, its body %.0f; ahead %.1f units (solid %g) normal %v, reach %g share %g", (vr_agit_wall - p.handpos) * vr_agit_fw,
            (vr_agit_wall - g.origin) * vr_agit_fw, trace_fraction * 128, trace_startsolid, trace_plane_normal,
            cvar_hget(cvarh_vr_agrunt_fall_impact_reach), cvar_hget(cvarh_vr_agrunt_fall_impact_held)));
    }
    float speed = cvar_hget(cvarh_scratch3) > 0 ? cvar_hget(cvarh_scratch3) : VR_AGIT_HAND; // (scratch3: the hand's m/s)
    float each = vlen(far - back) / (speed * VRMetersToUnits()); // s a swing's way
    if(vr_agit_swing == 0)
    {
        localcmd(cvar_hget(cvarh_scratch2) > 0 ? "vr_mock_hand_glide 0\nvr_mock_hand_to main ragdoll head 0\n" :
            "vr_mock_hand_glide 0\nvr_mock_hand_to main ragdoll 0\n"); // (scratch2 1: by its head)
        vr_agit_swing = 1;
    }
    else if(vr_agit_swing == 1 && t >= 0.1)
    {
        localcmd("+grabmain\n");
        vr_agit_swing = 2;
    }
    else if(vr_agit_swing == 2 && t >= 0.25)
    {
        vr_agit_held = VR_AG_HeldByHand(g);
        VR_AGT_Print(sprintf("%s: taken %g, up %.0f units", VR_AGIT_Name(vr_agit_case), vr_agit_held, g.origin_z - p.origin_z));
        VR_AGIT_HandTo(back, 0.5);
        vr_agit_swing = 3;
    }
    else if(vr_agit_swing == 3 && t >= 1)
    {
        VR_AGT_Print(sprintf("%s: swung from %.0f units off the wall, up %.0f, at %g m/s", VR_AGIT_Name(vr_agit_case),
            (vr_agit_wall - g.origin) * vr_agit_fw, g.origin_z - p.origin_z, speed));
        VR_AGIT_HandTo(far, each);
        vr_agit_swing = 4;
    }
    else if(vr_agit_swing == 4 && t >= 1 + each)
    {
        VR_AGT_Print(sprintf("%s: swung to %.0f units off the wall", VR_AGIT_Name(vr_agit_case), (vr_agit_wall - g.origin) * vr_agit_fw));
        if(!bang)
        {
            VR_AGIT_HandTo(back, each); // (reversed in mid-air, at once)
        }
        vr_agit_swing = 5;
    }
    else if(vr_agit_swing == 5 && t >= 1 + each * 2 + 0.3)
    {
        localcmd("-grabmain\n");
        return TRUE;
    }
    return vr_agit_swing > 5;
};

// The press (his note vrragtriggers_2026-11-03_02-18-13: the hand coming down at 2-3 m/s, the grip closing on the way, and
// on down a little: health 25 to 0, 23 to -1 at once): a real grip on a lying body: your hand above it comes down at 2 m/s,
// the grip closes just before it arrives, and it goes on the same way 16 units (in the floor or the wall), stays a moment
// and lets go. Done: TRUE.
float(entity p, entity g, float t, float c) VR_AGIT_Press =
{
    float part = c == VR_AGIT_PRESS + 1 ? 1 : c == VR_AGIT_PRESS + 2 ? 8 : 0;
    part = cvar_hget(cvarh_scratch2) > 0 ? cvar_hget(cvarh_scratch2) - 1 : part; // (scratch2 n: part n - 1)
    part = cvar_hget(cvarh_scratch2) < 0 ? vr_agit_round : part; // (scratch2 -1, with scratch4: each round the next part)
    float u = VRMetersToUnits();
    vector way = c == VR_AGIT_PRESS_WALL ? vr_agit_fw : '0 0 -1';
    if(vr_agit_phase == 0)
    {
        vector to = c == VR_AGIT_PRESS_WALL ? vr_agit_fw : g.origin - p.origin;
        to_z = 0;
        vector at = g.origin - normalize(to) * 28; // (you step up to it)
        at_z = p.origin_z;
        setorigin(p, at);
        p.velocity = '0 0 0';
        VR_AGIT_HandTo(p.origin + '0 0 8', 0);
        vr_agit_health = g.health;
        vr_agit_dealt = vr_rimp_dealt;
        vr_agit_phase = 1;
    }
    else if(vr_agit_phase == 1 && t >= 0.1)
    {
        localcmd(sprintf("vr_mock_hand_glide 0.4\nvr_mock_hand_to main ragdoll %g\n", part));
        vr_agit_phase = 2;
    }
    else if(vr_agit_phase == 2 && t >= 0.55)
    {
        localcmd("+grabmain\n");
        vr_agit_phase = 3;
    }
    else if(vr_agit_phase == 3 && t >= 0.6)
    {
        float press = cvar_hget(cvarh_scratch3) > 0 ? cvar_hget(cvarh_scratch3) : 2; // (scratch3: the press's m/s)
        VR_AGIT_HandTo(p.handpos + way * 16, 16 / (press * u));
        vr_agit_phase = 4;
    }
    else if(vr_agit_phase == 4 && t >= 0.75)
    {
        vr_agit_held = VR_AG_HeldByHand(g);
        VR_AGT_Print(sprintf("%s: part %g taken %g, health %g, your hand %.0f up from your feet", VR_AGIT_Name(c), part, vr_agit_held, g.health,
            p.handpos_z - p.absmin_z));
        vr_agit_phase = 5;
    }
    else if(vr_agit_phase == 5 && t >= 1.5)
    {
        localcmd("-grabmain\n");
        return TRUE;
    }
    return FALSE;
};

// His note's real cause (ROUND23.md): bending over it. VR_Headbutt_Target from his own measure, 31 units from its middle, the
// head coming down at it: no target; with the head in its drawn body: it.
void(entity p, entity g) VR_AGIT_HeadCheck =
{
    entity oldself = self;
    self = p;
    vector over = g.origin + '13 5 28';
    entity far = VR_Headbutt_Target(over, normalize(g.origin - over));
    vector in = g.origin + '0 0 12';
    entity near = VR_Headbutt_Target(in, '0 0 -1');
    self = oldself;
    VR_AGT_Check(sprintf("impact bending over it, lying: a head 31 units off its middle hits %s, one in its body hits %s:", far.classname,
        near.classname), far != g && near == g);
};

// A real wall to bang things on (not a crate, a pillar or a low wall): the first of 8 ways from you, 100 units off or more,
// whose surface is flat across a body's width and up to above your hand: vr_agit_fw, vr_agit_wall.
void(entity p) VR_AGIT_FindWall =
{
    for(float k = 0; k < 8; k++)
    {
        makevectors('0 1 0' * (p.angles_y + k * 45));
        vector fw = v_forward;
        traceline(p.origin, p.origin + fw * 3000, TRUE, p);
        vector at = trace_endpos;
        float d = trace_fraction * 3000;
        vr_agit_fw = fw;
        vr_agit_wall = at;
        float flat = (d >= 100 && d < 2900);
        for(float i = 0; i < 9 && flat; i++)
        {
            vector o = p.origin + VR_AGIT_Side() * (floor(i / 3) - 1) * 48 + '0 0 1' * ((i - floor(i / 3) * 3) * 32 - 16);
            traceline(o, o + fw * 3000, TRUE, p);
            flat = (fabs(trace_fraction * 3000 - d) < 8);
        }
        if(flat)
        {
            VR_AGT_Print(sprintf("the wall: %.0f units off, way %g of 8", d, k));
            return;
        }
    }
    VR_AGT_Print("no flat wall found");
};

// On vrclimb (its ledges): shoved off a real ledge, your real shove (his note vrragtriggers_2026-11-03_02-18-13: "they get
// damaged correctly, but they never drop their weapon"): over the rung tower's edge (a 280-unit drop), the long ledge's (240,
// into the trench), the block's (96); the tower's with Over a Ledge off (so a stagger goes over it), and with knockdowns off
// (it slides off on its feet); the block's on its feet (96: under VR_SHOVE_SAFEFALL, unhurt: it keeps it). PASS: its weapon
// dropped, on landing, not before; the last: kept, and it did fall. The block's knocked down: its ragdoll usually topples on
// the block's edge and stays up (fell 24-36 units, as measured): kept then, dropped if it does go over.
float vr_agit_lc;       // the case
float vr_agit_lz;       // its height at the shove
float vr_agit_lowest;   // ... the lowest it got
float vr_agit_lkeep1, vr_agit_lkeep2;
vector vr_agit_llast;
float VR_AGIT_LEDGES = 6;

string(float c) VR_AGIT_LedgeName =
{
    return c == 0 ? "off the tower (280)" : c == 1 ? "off the long ledge (240)" : c == 2 ? "off the block (96)" :
           c == 3 ? "off the tower, staggering (280)" : c == 4 ? "off the tower, on its feet (280)" : "off the block, on its feet (96)";
};

void() VR_AGIT_Ledge =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float u = VRMetersToUnits();
    float c = vr_agit_lc;
    vector at = c == 1 ? '112 400 72' : (c == 2 || c == 5) ? '16 -310 120' : '112 -160 304';
    if(vr_agt_step == 0)
    {
        if(c == 0)
        {
            vr_agit_lkeep1 = cvar_hget(cvarh_vr_knockdown_ledge);
            vr_agit_lkeep2 = cvar_hget(cvarh_vr_knockdown_chance);
            if(cvar_hget(cvarh_scratch1) > 0)
            {
                c = vr_agit_lc = cvar_hget(cvarh_scratch1) - 1;
                at = c == 1 ? '112 400 72' : (c == 2 || c == 5) ? '16 -310 120' : '112 -160 304';
            }
        }
        cvar_set("vr_knockdown_ledge", c == 3 ? "0" : ftos(vr_agit_lkeep1));
        cvar_set("vr_knockdown_chance", c >= 4 ? "0" : "100");
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        VR_AGPT_Clear();
        setorigin(p, at + '40 0 0');
        p.velocity = '0 0 0';
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 40);
        setorigin(g, at);
        g.angles_y = 180;
        vr_agit_t0 = time;
        vr_agit_drop_at = 0;
        vr_agit_still = 0;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_agit_t0 + 0.4)
        {
            return; // (started)
        }
        if(g.vr_wp_pack)
        {
            remove(g.vr_wp_pack); // (as VR_AGIT_Frame)
            g.vr_wp_pack = world;
        }
        g.health = 2000; // (the fall's damage never kills it: a death drops it anyway)
        traceline(g.origin - '40 0 0', g.origin - '40 0 2000', TRUE, g);
        VR_AGT_Print(sprintf("%s: at %v, on the floor %g, the ground past its edge %.0f units below it", VR_AGIT_LedgeName(c), g.origin,
            (g.flags & FL_ONGROUND) != 0, g.absmin_z - trace_endpos_z));
        vector oldpos = g.origin;
        entity oldself = self;
        self = p;
        vr_bash_exhaust_dmg = 0;
        vr_bash_exhaust_push = 0;
        VR_Bash_Hit(g, g.origin, '-1 0 0', QVR_HITKIND_SHOVE, -1, 1, 1);
        self = oldself;
        VR_AGT_Print(sprintf("%s: shoved: down %g, stagger %g, velocity %v", VR_AGIT_LedgeName(c), g.vr_knockdown, g.vr_kd_stagger, g.velocity));
        vr_agit_lz = oldpos_z;
        vr_agit_lowest = oldpos_z;
        vr_agit_llast = g.origin;
        vr_agit_t0 = time;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2)
    {
        float t = time - vr_agit_t0;
        vr_agit_lowest = min(vr_agit_lowest, g.origin_z);
        if(!g.vr_ag_prop && vr_agit_drop_at == 0)
        {
            vr_agit_drop_at = time;
        }
        vr_agit_still = (vlen(g.origin - vr_agit_llast) < 1) ? vr_agit_still + 1 : 0;
        vr_agit_llast = g.origin;
        if(cvar_hget(cvarh_vr_agrunt_debug) >= 2)
        {
            VR_AGT_Print(sprintf("t %.2f z %.0f health %g down %g stagger %g", t, g.origin_z, g.health, g.vr_knockdown, g.vr_kd_stagger));
        }
        float fell = vr_agit_lz - vr_agit_lowest;
        if(vr_agit_drop_at == 0 && t < 7 && vr_agit_still < 40)
        {
            return;
        }
        VR_AGT_Check(sprintf("ledge %s: %s; fell %.0f units, health 2000 to %g; hardest impact %.1f m/s, fell at %.1f m/s, down %g:",
            VR_AGIT_LedgeName(c), vr_agit_drop_at > 0 ? sprintf("dropped at %.2f s", vr_agit_drop_at - vr_agit_t0) : "kept", fell, g.health,
            g.vr_ag_fall_hit / u, g.vr_ag_fall_down / u, g.vr_knockdown),
            c == 2 ? (vr_agit_drop_at > 0) == (fell >= 64) : ((c == 5 ? vr_agit_drop_at == 0 : vr_agit_drop_at > 0) && fell >= 64));
        vr_agit_lc = vr_agit_lc + 1;
        vr_agt_step = 0;
        if(vr_agit_lc >= VR_AGIT_LEDGES || cvar_hget(cvarh_scratch1) > 0)
        {
            vr_agit_lc = 0;
            VR_AGPT_Clear();
            cvar_set("vr_knockdown_ledge", ftos(vr_agit_lkeep1));
            cvar_set("vr_knockdown_chance", ftos(vr_agit_lkeep2));
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            cvar_set("vr_agrunt_test", "0");
        }
    }
};

void() VR_AGIT_Frame =
{
    if(mapname == "vrclimb")
    {
        VR_AGIT_Ledge();
        return;
    }
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    float u = VRMetersToUnits();
    float c = vr_agit_case;
    if(vr_agt_step == 0)
    {
        if(c == 0 && vr_agit_round == 0)
        {
            vr_agit_home = p.origin;
            VR_AGIT_FindWall(p);
            vr_agit_keep_yank = cvar_hget(cvarh_vr_agrunt_disarm_yank);
            cvar_set("vr_agrunt_disarm_yank", "0"); // (a hand on its body here, never on its weapon)
            if(cvar_hget(cvarh_scratch1) > 0)
            {
                c = vr_agit_case = cvar_hget(cvarh_scratch1) - 1;
            }
        }
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        p.weapon2 = WID_FIST;
        p.mainhand_lastgrabtime = time - 2;
        p.mainhand_lastreleasetime = time - 1;
        localcmd("-grabmain\n");
        VR_AGPT_Clear();
        // (The last cases' dropped weapons: a grip near one takes it, not the body.)
        for(entity w = find(world, classname, "thrown_weapon"); w != world; w = find(world, classname, "thrown_weapon"))
        {
            remove(w);
        }
        for(entity k = find(world, classname, "vr_wornpack"); k != world; k = find(world, classname, "vr_wornpack"))
        {
            remove(k);
        }
        // You stand well off the wall, but near it for its own cases; it ahead of you.
        float wallcase = c == VR_AGIT_WALL || c == VR_AGIT_BANG;
        float off = c == VR_AGIT_BANG ? 40 : (vlen(vr_agit_wall - vr_agit_home) > 900 ? 900 : vlen(vr_agit_wall - vr_agit_home) - 16);
        off = wallcase ? min(off, 240) : off;
        vector stand = vr_agit_wall - vr_agit_fw * off;
        stand_z = vr_agit_home_z;
        if(c == VR_AGIT_BANG)
        {
            stand = stand - VR_AGIT_Side() * VR_AGIT_ARM; // (your hand and the body on the line the wall was measured on)
        }
        if(c == VR_AGIT_SWING)
        {
            // (Midway between the side walls: nothing in the swing's way.)
            vector side = VR_AGIT_Side();
            traceline(stand, stand + side * 2000, TRUE, p);
            float right = trace_fraction * 2000;
            traceline(stand, stand - side * 2000, TRUE, p);
            stand = stand + side * (right - trace_fraction * 2000) * 0.5;
            VR_AGT_Print(sprintf("%s: the side walls %.0f units off", VR_AGIT_Name(c), (right + trace_fraction * 2000) * 0.5));
        }
        setorigin(p, stand);
        p.velocity = '0 0 0';
        VR_AGIT_HandTo(p.origin + '0 0 8', 0); // (out of any wall the last case left it in)
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, (c == VR_AGIT_BANG || c == VR_AGIT_SWING) ? 40 : 110);
        if(c == VR_AGIT_BANG || c == VR_AGIT_SWING)
        {
            setorigin(g, p.origin + VR_AGIT_Side() * VR_AGIT_ARM);
        }
        if(c == VR_AGIT_WALL)
        {
            setorigin(g, vr_agit_wall - vr_agit_fw * 72); // (it reaches the wall before it touches the floor)
        }
        if(c == VR_AGIT_PRESS_WALL)
        {
            // (It falls against the wall, you beside it.)
            setorigin(p, vr_agit_wall - vr_agit_fw * 48 - VR_AGIT_Side() * VR_AGIT_ARM);
            setorigin(g, vr_agit_wall - vr_agit_fw * 24);
        }
        vr_agit_phase = 0;
        if(c < VR_AGIT_PRESS)
        {
            g.health = 400; // (an impact's damage never kills it: a death drops it anyway; a press: its own, it must lose none)
        }
        VR_AGT_Print(sprintf("%s: you %.0f units off the wall, it %.0f", VR_AGIT_Name(c), vlen(p.origin - vr_agit_wall),
            vlen(g.origin - vr_agit_wall)));
        vr_agit_t0 = time;
        vr_agit_still = 0;
        vr_agit_drop_at = 0;
        vr_agit_swing = 0;
        vr_agit_held = 0;
        vr_agt_step = 1;
        return;
    }
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_agit_t0 + 0.3)
        {
            return; // (started)
        }
        if(g.vr_wp_pack)
        {
            remove(g.vr_wp_pack); // (a worn pack torn off by the landing can hit its weapon: another rule)
            g.vr_wp_pack = world;
        }
        makevectors('0 1 0' * g.angles_y);
        VR_Knockdown_Start(g);
        g.vr_kd_until = time + 30; // (it lies till the case ends)
        if(c == VR_AGIT_TOPPLE || c == VR_AGIT_PUSH || c == VR_AGIT_WALK || c == VR_AGIT_RUN || c == VR_AGIT_SWING || c == VR_AGIT_BANG || c >= VR_AGIT_PRESS)
        {
            ragdollshovetopple(g, v_forward * -1, 1, FALSE, 40);
        }
        else if(c == VR_AGIT_FALL)
        {
            ragdollfling(g, '0 0 8' * u); // (up about 1.3 m, and down)
        }
        else if(c == VR_AGIT_FLOOR)
        {
            ragdollfling(g, '0 0 6' * u);
        }
        else if(c == VR_AGIT_WALL)
        {
            ragdollfling(g, (vr_agit_fw * 9 + '0 0 3') * u);
        }
        vr_agit_t0 = time;
        vr_agt_step = 2;
        return;
    }
    float t = time - vr_agit_t0;
    if(vr_agit_drop_at == 0 && !g.vr_ag_prop)
    {
        vr_agit_drop_at = time;
    }
    float over = FALSE;
    if(vr_agit_drop_at > 0)
    {
        over = TRUE;
    }
    else if(c == VR_AGIT_PUSH || c == VR_AGIT_WALK || c == VR_AGIT_RUN || c >= VR_AGIT_PRESS)
    {
        // Down and still first (it lies), then the push: a shove's one kick, you walking through it, or your hand's press.
        if(vr_agt_step == 2)
        {
            vr_agit_still = (vlen(g.vr_ag_fall_v) < 40 && t > 0.5) ? vr_agit_still + 1 : 0;
            if(vr_agit_still >= 15 || t > 2.5)
            {
                vr_agit_still = 0;
                vr_agit_t0 = time;
                vr_agt_step = 3;
                if(c == VR_AGIT_PUSH)
                {
                    ragdollfling(g, vr_agit_fw * 3 * u);
                }
                else
                {
                    VR_AGT_Print(sprintf("%s: lying, you %.0f units off", VR_AGIT_Name(c), vlen(g.origin - p.origin)));
                }
            }
            return;
        }
        if(c >= VR_AGIT_PRESS)
        {
            if(vr_agit_phase == 0 && c == VR_AGIT_PRESS)
            {
                VR_AGIT_HeadCheck(p, g);
            }
            over = VR_AGIT_Press(p, g, t, c);
        }
        else if(c == VR_AGIT_PUSH)
        {
            over = t >= 1;
        }
        else
        {
            vector to = g.origin - p.origin;
            to_z = 0;
            if(p.flags & FL_ONGROUND)
            {
                to = normalize(to) * (c == VR_AGIT_RUN ? 10 : 5) * u;
                p.velocity_x = to_x;
                p.velocity_y = to_y;
            }
            over = t >= 1.2;
        }
    }
    else if(c == VR_AGIT_SWING || c == VR_AGIT_BANG)
    {
        over = VR_AGIT_Swing(p, g, t, c == VR_AGIT_BANG);
    }
    else
    {
        if(c == VR_AGIT_FLOOR && t >= 0.2 && t - frametime < 0.2)
        {
            ragdollfling(g, (vr_agit_fw * 2 + '0 0 -8') * u); // (thrown down from above your head)
        }
        vr_agit_still = (vlen(g.vr_ag_fall_v) < 40 && t > 0.5) ? vr_agit_still + 1 : 0;
        over = vr_agit_still >= 15;
    }
    if(!over && t < VR_AGIT_GIVEUP)
    {
        return;
    }
    if(g.health <= 0 && c >= VR_AGIT_PRESS && vr_agit_phase < 4)
    {
        VR_AGT_Print("press: it died before the press");
    }
    float most = cvar_hget(cvarh_vr_agrunt_fall_keep_impact) * u;
    float wants = (c >= VR_AGIT_FALL && c < VR_AGIT_PRESS);
    float dropped = vr_agit_drop_at > 0;
    float hard = g.vr_ag_fall_hit >= most;
    localcmd("-grabmain\n");
    if(c == VR_AGIT_PRESS_WALL && !vr_agit_held)
    {
        // (The mock hand doesn't get a grip on a body slumped at a wall: not a result either way.)
        VR_AGT_Print(sprintf("impact %s: NOT RUN, no grip (health %g to %g)", VR_AGIT_Name(c), vr_agit_health, g.health));
    }
    else if(c >= VR_AGIT_PRESS)
    {
        VR_AGT_Check(sprintf("impact %s: %s; hardest impact %.1f m/s, health %g to %g, %g impacts dealt, held %g:", VR_AGIT_Name(c),
            dropped ? "dropped" : "kept", g.vr_ag_fall_hit / u, vr_agit_health, g.health, vr_rimp_dealt - vr_agit_dealt, vr_agit_held),
            t < VR_AGIT_GIVEUP + 10 && !dropped && !hard && vr_agit_held && g.health == vr_agit_health && vr_rimp_dealt == vr_agit_dealt);
    }
    else
    VR_AGT_Check(strcat(sprintf("impact %s: %s; hardest impact %.1f m/s (lets go at %g), flung %.1f, fell %.1f m/s, held %g:",
        VR_AGIT_Name(c), dropped ? sprintf("dropped at %.2f s", vr_agit_drop_at - vr_agit_t0) : "kept", g.vr_ag_fall_hit / u,
        most / u, g.vr_ag_fall_fling / u, g.vr_ag_fall_down / u, vr_agit_held), t >= VR_AGIT_GIVEUP ? " GAVE UP:" : ""),
        t < VR_AGIT_GIVEUP && dropped == wants && hard == wants && ((c != VR_AGIT_SWING && c != VR_AGIT_BANG) || vr_agit_held));
    vr_agit_case = vr_agit_case + 1;
    vr_agt_step = 0;
    float alone = cvar_hget(cvarh_scratch1) > 0;
    if((vr_agit_case >= VR_AGIT_CASES || alone) && vr_agit_round + 1 < cvar_hget(cvarh_scratch4))
    {
        vr_agit_round = vr_agit_round + 1;
        vr_agit_case = alone ? c : 0;
        return;
    }
    if(vr_agit_case >= VR_AGIT_CASES || alone)
    {
        vr_agit_case = 0;
        vr_agit_round = 0;
        VR_AGPT_Clear();
        VR_AGPT_EmptyHand(p);
        cvar_set("vr_agrunt_disarm_yank", ftos(vr_agit_keep_yank));
        cvar_set("vr_mock_hand_glide", "0");
        setorigin(p, vr_agit_home);
        p.velocity = '0 0 0';
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        cvar_set("vr_agrunt_test", "0");
    }
};
#endif
