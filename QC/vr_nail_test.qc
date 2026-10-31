#ifdef QVR_TESTS
//
//
//
// ----------------------------------------------------------------------------
// The nails' speed, size and hit box (vr_nail_test 1; Debug > Tests > Nail Pistol > Nail Speed and Size Test; round 23,
// "Nails smaller and faster"). Headless, fast mode, a definite end: each check prints "nailtest: PASS|FAIL <what>", the
// run ends on "nailtest: N passed, M failed". A state machine on the game's frames, each step on its own condition:
//   1 speeds: the player's nailgun, super nailgun and nail pistol (their real fire functions, the main hand), and an
//     armed grunt's nailgun (VR_AG_ShootProp, as its attack fires it): each nail's speed measured from its flight (two
//     frames' way over their time), against vr_nail_speed / vr_nailpistol_speed and Quake VR's speeds before round 23
//     (1750, 2600): 15-20% faster.
//   2 the hit box: an unarmed grunt 256 ahead; nails from 200 units in front of it along the player's line, one at a time
//     at 2-unit steps across it (-30..30) at its chest, with precise hits (its model) and then Quake's boxes
//     (vr_hit_precise 0): which met it. Each a point (its mins and maxs 0), the middle met (no nail through it), none
//     met beyond the monster's box (16) and the tolerance (2).
//   3 the drawn size: a nail of each model hung still ahead, `vr_physics_list spike` prints each one's "drawn x" (the
//     client's matrix: vr_nail_scale, 1 before round 23).
//   4 a dropped backpack lying loose: nails across and over it meet it only at its drawn box as it lies (VR_NLT_BagStep;
//     round 23, "Backpacks' hit box"). vr_nail_test 3 runs this step alone (Debug > Tests > Nail Pistol > Nails at a
//     Dropped Backpack).
// vr_nail_test 2: three nails hung still 40 units before your eyes for 30 s (the nailgun's, the super nailgun's, a lava
// nail where Dissolution of Eternity is there), to see their size in the headset.

float vr_nlt_step;     // 0 idle
float vr_nlt_frames;   // frames into the step
float vr_nlt_case;     // step 1: 0 nailgun, 1 super nailgun, 2 nail pistol, 3 an armed grunt's nailgun
float vr_nlt_precise;  // step 2: the setting before
float vr_nlt_mode;     // step 2: 0 precise, 1 Quake's boxes
entity vr_nlt_player;
entity vr_nlt_nail;    // step 1: the nail measured
entity vr_nlt_g;       // the grunt
entity vr_nlt_t;       // vr_nail_test 4: the grunt the nails strike
float vr_nlt_wsent;    // ... the wound events, blood particles before the case's nail
float vr_nlt_bsent;
string vr_nlt_nclass;  // ... its nail's class as it last flew
vector vr_nlt_o1;      // step 1: its origin a frame after it left
float vr_nlt_t1;
float vr_nlt_hitlo, vr_nlt_hithi; // step 2: the lowest and highest side offsets that met the grunt
float vr_nlt_hit0;     // ... the middle one met
float vr_nlt_k;        // ... the next nail's side offset
.float vr_nlt_off;     // a step 2 nail's side offset
.float vr_nlt_mark;    // a nail that was there before the shot (step 1: the new one is the one without)


void() VR_NLT_Next = { vr_nlt_step++; vr_nlt_frames = 0; };

void() VR_NLT_MarkSpikes =
{
    entity e = find(world, classname, "spike");
    while(e) { e.vr_nlt_mark = TRUE; e = find(e, classname, "spike"); }
};

entity() VR_NLT_NewSpike =
{
    entity e = find(world, classname, "spike");
    while(e)
    {
        if(!e.vr_nlt_mark) { return e; }
        e = find(e, classname, "spike");
    }
    return world;
};

// Step 1's shot `xCase` (the fire function as the game calls it), its nail found.
void(float xCase) VR_NLT_Fire =
{
    entity p = vr_nlt_player;
    entity os = self;
    VR_NLT_MarkSpikes();
    if(xCase == 3)
    {
        entity a = vr_nlt_g;
        self = a;
        a.enemy = p;
        if(a.vr_ag_prop) { a.vr_ag_prop.weaponinst.wi_clip = 50; VR_AG_ShootProp(a.vr_ag_prop); }
    }
    else
    {
        self = p;
        float wid = xCase == 0 ? WID_NAILGUN : xCase == 1 ? WID_SUPER_NAILGUN : WID_NAILPISTOL;
        VR_WeaponUtil_EntAddWeapon(self, wid);
        VRGiveEntWeapon(self, cVR_MainHand, wid, 0, xCase == 2 ? VR_NailPistol_MagSize() : 50);
        W_SetCurrentAmmoFor(cVR_MainHand);
        self.ammo_nails = max(self.ammo_nails, 100);
        if(xCase == 2) { W_FireNailPistol(cVR_MainHand); } else { W_FireSpikes(cVR_MainHand, 0); }
    }
    self = os;
    vr_nlt_nail = VR_NLT_NewSpike();
};

string(float xCase) VR_NLT_CaseName =
{
    if(xCase == 0) return "the player's nailgun";
    if(xCase == 1) return "the player's super nailgun";
    if(xCase == 2) return "the player's nail pistol";
    return "an armed grunt's nailgun";
};

void() VR_NLT_NailTouch =
{
    if(cvar("developer") > 1)
    {
        dprint(sprintf("nailtest: debug sweep %g met %s at %v\n", self.vr_nlt_off, other.classname, self.origin));
    }
    if(other == vr_nlt_g)
    {
        if(self.vr_nlt_off == 0) { vr_nlt_hit0 = TRUE; }
        vr_nlt_hitlo = min(vr_nlt_hitlo, self.vr_nlt_off);
        vr_nlt_hithi = max(vr_nlt_hithi, self.vr_nlt_off);
    }
    remove(self);
};

float() VR_NLT_SweepLeft =
{
    float c = 0;
    entity e = find(world, classname, "spike");
    while(e) { if(e.touch == VR_NLT_NailTouch) { c++; } e = find(e, classname, "spike"); }
    return c;
};

// Step 2: a nail across the grunt at side offset `xOff` (no damage: VR_NLT_NailTouch). One at a time: nails spawned side
// by side in one frame meet each other as they leave.
void(float xOff) VR_NLT_SweepNail =
{
    entity g = vr_nlt_g;
    vector dir = g.origin - vr_nlt_player.origin;
    dir_z = 0;
    dir = normalize(dir);
    vector side = '0 0 0';
    side_x = -dir_y;
    side_y = dir_x;
    entity os = self;
    self = vr_nlt_player;
    entity n = launch_spike(g.origin + '0 0 8' - dir * 200 + side * xOff, dir);
    self = os;
    n.touch = VR_NLT_NailTouch;
    n.vr_nlt_off = xOff;
    if(xOff == 0 && vr_nlt_mode == 0)
    {
        VR_Test_Check(n.mins == '0 0 0' && n.maxs == '0 0 0', sprintf("a nail's hit box: a point (mins %v, maxs %v)",
            n.mins, n.maxs));
    }
};

// A nail of model `xModel` hung still at `xAt`, facing `xDir`, for `xLife` s.
entity(string xModel, vector xAt, vector xDir, float xLife) VR_NLT_Hang =
{
    entity os = self;
    self = vr_nlt_player;
    entity n = launch_spike(xAt, xDir);
    self = os;
    setmodel(n, xModel);
    setsize(n, VEC_ORIGIN, VEC_ORIGIN);
    n.movetype = MOVETYPE_NONE;
    n.solid = SOLID_NOT;
    n.velocity = '0 0 0';
    n.touch = SUB_Null;
    n.nextthink = time + xLife;
    return n;
};

void() VR_NLT_End =
{
    cvar_set("vr_hit_precise", ftos(vr_nlt_precise));
    if(vr_nlt_g) { VR_AGT_Remove(vr_nlt_g); vr_nlt_g = world; }
    if(vr_nlt_t) { VR_AGT_Remove(vr_nlt_t); vr_nlt_t = world; }
    vr_nlt_player.flags = vr_nlt_player.flags - (vr_nlt_player.flags & FL_NOTARGET);
    VR_Test_Verdict();
    vr_nlt_step = 0;
};

// Step 4 (vr_nail_test 3 alone): a backpack lying loose (DropBackpack, as a monster drops it), settled, and nails at it one
// at a time (no damage, no push: VR_NLT_BagTouch): across it at its middle's height at 2-unit steps (-30..30 off its
// middle), then over it (0..24 above its middle). Its drawn box as it lies (drawnbounds, turned by its angles as the
// engine turns a prop's: held::axesFromAngles) is where they may meet it: the middle met, none beyond the box's reach
// (+1), each met at the box (within 1.5), none flown through it (it never moved). Before the fix FL_ITEM's box (15 units
// wider each way, for pickups) stopped nails up to 22 units off the bag.
entity vr_nlt_bag;
float vr_nlt_settled;  // frames it has lain still
float vr_nlt_axis;     // 0 the sweep across, 1 the one over it
vector vr_nlt_bx, vr_nlt_by, vr_nlt_bz, vr_nlt_bmid, vr_nlt_bhalf; // its drawn box: axes, middle, half sizes
vector vr_nlt_bat;     // where it lay when the sweeps began
vector vr_nlt_dir, vr_nlt_side;
float vr_nlt_reach, vr_nlt_reachup; // the box's reach from its middle across the nails' line, and up
float vr_nlt_bagwide;  // nails that met it beyond its reach
float vr_nlt_bagoff;   // ... met it away from its box
float vr_nlt_baghits;
float vr_nlt_bagthru;  // nails that went on past it having moved it
float vr_nlt_uphi;     // the sweep over it: the highest that met it
.float vr_nlt_up;      // a nail's height over the middle
.float vr_nlt_wide;    // a nail aimed beyond the box's reach

void() VR_NLT_BagShape =
{
    entity b = vr_nlt_bag;
    vector a = b.angles;
    a_x = -a_x; // (an alias model's pitch, as held::axesFromAngles)
    makevectors(a);
    vr_nlt_bx = v_forward;
    vr_nlt_by = '0 0 0' - v_right;
    vr_nlt_bz = v_up;
    vector lo = drawnbounds(b, 0);
    vector hi = drawnbounds(b, 1);
    vector mid = (lo + hi) * 0.5;
    vr_nlt_bhalf = (hi - lo) * 0.5;
    vr_nlt_bhalf_x = max(vr_nlt_bhalf_x, 0.5);
    vr_nlt_bhalf_y = max(vr_nlt_bhalf_y, 0.5);
    vr_nlt_bhalf_z = max(vr_nlt_bhalf_z, 0.5);
    vr_nlt_bmid = b.origin + vr_nlt_bx * mid_x + vr_nlt_by * mid_y + vr_nlt_bz * mid_z;
};

// The box's reach from its middle along unit `s`.
float(vector s) VR_NLT_BagReach =
{
    return vr_nlt_bhalf_x * fabs(vr_nlt_bx * s) + vr_nlt_bhalf_y * fabs(vr_nlt_by * s) + vr_nlt_bhalf_z * fabs(vr_nlt_bz * s);
};

// Whether `p` is in the box grown by `xGrow`.
float(vector p, float xGrow) VR_NLT_InBag =
{
    vector d = p - vr_nlt_bmid;
    return fabs(d * vr_nlt_bx) <= vr_nlt_bhalf_x + xGrow && fabs(d * vr_nlt_by) <= vr_nlt_bhalf_y + xGrow &&
           fabs(d * vr_nlt_bz) <= vr_nlt_bhalf_z + xGrow;
};

void() VR_NLT_BagTouch =
{
    if(cvar("developer") > 1)
    {
        dprint(sprintf("nailtest: debug bag nail %g across, %g up met %s at %v\n", self.vr_nlt_off, self.vr_nlt_up,
            other == vr_nlt_bag ? "the bag" : other.classname, self.origin));
    }
    if(other == vr_nlt_bag)
    {
        vr_nlt_baghits++;
        if(self.vr_nlt_wide) { vr_nlt_bagwide++; }
        if(!VR_NLT_InBag(self.origin, 1.5)) { vr_nlt_bagoff++; }
        if(self.vr_nlt_off == 0 && self.vr_nlt_up == 0) { vr_nlt_hit0 = TRUE; }
        if(vr_nlt_axis == 0)
        {
            vr_nlt_hitlo = min(vr_nlt_hitlo, self.vr_nlt_off);
            vr_nlt_hithi = max(vr_nlt_hithi, self.vr_nlt_off);
        }
        else
        {
            vr_nlt_uphi = max(vr_nlt_uphi, self.vr_nlt_up);
        }
    }
    else if(vlen(vr_nlt_bag.origin - vr_nlt_bat) > 0.1)
    {
        vr_nlt_bagthru++; // (it went on, and the bag moved: its shape was met, not its box)
        vr_nlt_bat = vr_nlt_bag.origin;
    }
    remove(self);
};

// A nail along the line to the bag's middle, `xOff` across it and `xUp` over its middle, from 100 units before it.
void(float xOff, float xUp) VR_NLT_BagNail =
{
    entity os = self;
    self = vr_nlt_player;
    entity n = launch_spike(vr_nlt_bmid + '0 0 1' * xUp - vr_nlt_dir * 100 + vr_nlt_side * xOff, vr_nlt_dir);
    self = os;
    n.touch = VR_NLT_BagTouch;
    n.vr_nlt_off = xOff;
    n.vr_nlt_up = xUp;
    n.vr_nlt_wide = ((fabs(xOff) > vr_nlt_reach + 1) || (xUp > vr_nlt_reachup + 1));
};

float() VR_NLT_BagLeft =
{
    float c = 0;
    entity e = find(world, classname, "spike");
    while(e) { if(e.touch == VR_NLT_BagTouch) { c++; } e = find(e, classname, "spike"); }
    return c;
};

void() VR_NLT_BagStep =
{
    if(vr_nlt_frames == 1)
    {
        // Dropped as a monster drops it, a little ahead.
        makevectors('0 1 0' * vr_nlt_player.angles_y);
        traceline(vr_nlt_player.origin, vr_nlt_player.origin + v_forward * 160, TRUE, vr_nlt_player);
        entity os = self;
        self = spawn();
        setorigin(self, trace_endpos - v_forward * 24 + '0 0 24');
        self.flags = FL_MONSTER;
        self.ammo_shells = 5;
        DropBackpack();
        remove(self);
        self = os;
        vr_nlt_bag = world;
        entity nb = find(world, model, "progs/backpack.mdl");
        while(nb) { vr_nlt_bag = nb; nb = find(nb, model, "progs/backpack.mdl"); }
        vr_nlt_settled = 0;
        if(!vr_nlt_bag)
        {
            VR_Test_Check(FALSE, "a lying backpack: none dropped");
            VR_NLT_End();
        }
        return;
    }
    entity b = vr_nlt_bag;
    if(vr_nlt_settled < 30)
    {
        // Settled: on the floor and still 30 frames.
        if((b.flags & FL_ONGROUND) && vlen(b.velocity) < 1) { vr_nlt_settled++; } else { vr_nlt_settled = 0; }
        if(vr_nlt_settled == 30)
        {
            VR_NLT_BagShape();
            vr_nlt_dir = vr_nlt_bmid - vr_nlt_player.origin;
            vr_nlt_dir_z = 0;
            vr_nlt_dir = normalize(vr_nlt_dir);
            vr_nlt_side = '0 0 0';
            vr_nlt_side_x = -vr_nlt_dir_y;
            vr_nlt_side_y = vr_nlt_dir_x;
            vr_nlt_reach = VR_NLT_BagReach(vr_nlt_side);
            vr_nlt_reachup = VR_NLT_BagReach('0 0 1');
            vr_nlt_bat = b.origin;
            vr_nlt_axis = 0;
            vr_nlt_k = -30;
            vr_nlt_hitlo = 1000;
            vr_nlt_hithi = -1000;
            vr_nlt_uphi = -1000;
            vr_nlt_hit0 = FALSE;
            vr_nlt_baghits = vr_nlt_bagwide = vr_nlt_bagoff = vr_nlt_bagthru = 0;
            dprint(sprintf("nailtest: the backpack lies at %v, angles %v; its drawn box's middle %v, half sizes %v\n",
                b.origin, b.angles, vr_nlt_bmid, vr_nlt_bhalf));
        }
        return;
    }
    if(VR_NLT_BagLeft())
    {
        return; // (its nail still flying)
    }
    if(vr_nlt_axis == 0 && vr_nlt_k <= 30)
    {
        VR_NLT_BagNail(vr_nlt_k, 0);
        vr_nlt_k += 2;
        return;
    }
    if(vr_nlt_axis == 0)
    {
        vr_nlt_axis = 1;
        vr_nlt_k = 0;
    }
    if(vr_nlt_k <= 24)
    {
        VR_NLT_BagNail(0, vr_nlt_k);
        vr_nlt_k += 2;
        return;
    }
    VR_Test_Check(vr_nlt_hit0 && vr_nlt_bagwide == 0 && vr_nlt_hitlo >= -vr_nlt_reach - 1 && vr_nlt_hithi <= vr_nlt_reach + 1,
        sprintf("a lying backpack: the nails across it met it from %g to %g units off its middle (the middle %s; its drawn box reaches %.1f; none beyond: %g did)",
            vr_nlt_hitlo, vr_nlt_hithi, vr_nlt_hit0 ? "met" : "MISSED", vr_nlt_reach, vr_nlt_bagwide));
    VR_Test_Check(vr_nlt_uphi >= 0 && vr_nlt_uphi <= vr_nlt_reachup + 1,
        sprintf("a lying backpack: the nails over it met it up to %g over its middle (its drawn box reaches %.1f up)", vr_nlt_uphi,
            vr_nlt_reachup));
    VR_Test_Check(vr_nlt_baghits > 0 && vr_nlt_bagoff == 0,
        sprintf("a lying backpack: each nail met it at its drawn box (%g of %g away from it)", vr_nlt_bagoff, vr_nlt_baghits));
    VR_Test_Check(vr_nlt_bagthru == 0 && vlen(b.origin - vr_nlt_bat) <= 0.1,
        sprintf("a lying backpack: no nail flew on through it having pushed it (%g did)", vr_nlt_bagthru));
    remove(b);
    vr_nlt_bag = world;
    VR_NLT_End();
};

// vr_nail_test 4 (Debug > Tests > Nail Pistol > Nail Wounds Test; round 23, "Nail hits bleed and dent"): each nail weapon's
// real nail (the nailgun's, the super nailgun's, the nail pistol's, an armed grunt's nailgun's: an enemy's) set flying
// at an unarmed grunt's chest from 48 units: its wound a nail's (a dent and blood: QVR_WOUND_NAIL), blood spawned, not set
// on fire; then a lava nail (the nailgun's nail turned, VR_Burn_NailToLava; Dissolution of Eternity there): its wound a
// burn. Before the fix every projectile's hit was tagged a hitscan shot (VR_Proj_Begin) and painted as lightning's (a
// burn mark, no dent, no blood stream). Each case: fired at frame 2, then waits for its wound event (60 frames: FAIL).
void() VR_NLT_WoundStep =
{
    float c = vr_nlt_case;
    entity t = vr_nlt_t;
    if(vr_nlt_frames == 2)
    {
        if(c == 4 && !vr_have_lava)
        {
            dprint("nailtest: no lava nails here (Dissolution of Eternity): the lava nail's case skipped\n");
            VR_NLT_End();
            return;
        }
        t.health = 1000; // (it lives through every nail)
        if(t.burn_fire) { VR_Burn_Out(t.burn_fire, "the nail test's next case"); }
        vr_nlt_wsent = vr_wound_sent;
        vr_nlt_bsent = vr_blood_sent;
        vr_wound_lastkind = 0;
        VR_NLT_Fire(c == 4 ? 0 : c);
        entity n = vr_nlt_nail;
        if(!n)
        {
            VR_Test_Check(FALSE, sprintf("wounds: %s: no nail", c == 4 ? "a lava nail" : VR_NLT_CaseName(c)));
            vr_nlt_frames = 100;
        }
        else
        {
            vector chest = (t.absmin + t.absmax) * 0.5 + '0 0 6';
            vector d = chest - vr_nlt_player.origin;
            d_z = 0;
            d = normalize(d);
            setorigin(n, chest - d * 48);
            n.velocity = d * vlen(n.velocity);
            n.angles = vectoangles(n.velocity);
            if(c == 4) { VR_Burn_NailToLava(n, n.origin, n); }
            vr_nlt_nclass = n.classname;
        }
        return;
    }
    if(vr_nlt_frames < 3)
    {
        return;
    }
    if(vr_nlt_nail.classname == "spike" || vr_nlt_nail.classname == "lava_spike") // (still flying: freed, it has none)
    {
        vr_nlt_nclass = vr_nlt_nail.classname;
    }
    if(vr_wound_sent == vr_nlt_wsent && vr_nlt_frames < 60)
    {
        return; // (in flight)
    }
    string nm = c == 4 ? "a lava nail" : VR_NLT_CaseName(c);
    float k = vr_wound_lastkind;
    float blood = vr_blood_sent - vr_nlt_bsent;
    if(vr_nlt_frames < 100)
    {
        dprint(sprintf("nailtest: wounds: %s (%s, owner %s): wound kind %g, blood +%g, on fire %g, health %g\n", nm,
            vr_nlt_nclass, c == 3 ? "the armed grunt" : "the player", k, blood, t.burn_fire != world, t.health));
        if(c == 4)
        {
            VR_Test_Check(k == QVR_WOUND_BURN, sprintf("wounds: %s burns (wound kind %g, want %g)", nm, k, QVR_WOUND_BURN));
        }
        else
        {
            VR_Test_Check(vr_wound_sent > vr_nlt_wsent, sprintf("wounds: %s struck (a wound event)", nm));
            VR_Test_Check(k == QVR_WOUND_NAIL, sprintf("wounds: %s dents and bleeds (wound kind %g, want %g)", nm, k,
                QVR_WOUND_NAIL));
            VR_Test_Check(blood > 0, sprintf("wounds: %s spawned blood (+%g)", nm, blood));
            VR_Test_Check(vr_nlt_nclass == "spike" && t.burn_fire == world,
                sprintf("wounds: %s not burnt (nail %s, on fire %g)", nm, vr_nlt_nclass, t.burn_fire != world));
        }
    }
    vr_nlt_nail = world;
    vr_nlt_case++;
    vr_nlt_frames = 0;
    if(vr_nlt_case > 4)
    {
        VR_NLT_End();
    }
};

void() VR_NailTest_Frame =
{
    float req = cvar("vr_nail_test");
    if(req)
    {
        cvar_set("vr_nail_test", "0");
        vr_nlt_player = VR_Player();
        if(!vr_nlt_player) { dprint("nailtest: no player\n"); return; }
        if(req == 2)
        {
            makevectors(vr_nlt_player.v_angle);
            vector eye = vr_nlt_player.origin + vr_nlt_player.view_ofs + v_forward * 40;
            VR_NLT_Hang("progs/spike.mdl", eye - v_right * 8, v_right, 30);
            VR_NLT_Hang("progs/s_spike.mdl", eye, v_right, 30);
            if(vr_have_lava) { VR_NLT_Hang("progs/lspike.mdl", eye + v_right * 8, v_right, 30); }
            dprint(sprintf("nailtest: nails hung before your eyes (vr_nail_scale %g)\n", cvar("vr_nail_scale")));
            return;
        }
        VR_Test_Begin("nailtest");
        vr_nlt_precise = cvar("vr_hit_precise");
        if(req == 3)
        {
            vr_nlt_step = 4;
            vr_nlt_frames = 0;
            return;
        }
        if(req == 4)
        {
            vr_nlt_player.flags = vr_nlt_player.flags | FL_NOTARGET;
            vr_npt_player = vr_nlt_player;
            vr_nlt_g = VR_NPT_Grunt(400, WID_NAILGUN, 0); // (the armed grunt whose nail is the enemy's)
            vr_nlt_t = VR_NPT_Grunt(200, -1, 0);          // (the one they strike)
            vr_nlt_case = 0;
            vr_nlt_step = 10;
            vr_nlt_frames = -10;
            return;
        }
        vr_nlt_player.flags = vr_nlt_player.flags | FL_NOTARGET;
        vr_npt_player = vr_nlt_player; // (VR_NPT_Grunt's: vr_nailpistol_test.qc)
        vr_nlt_g = VR_NPT_Grunt(400, WID_NAILGUN, 0);
        vr_nlt_case = 0;
        vr_nlt_step = 1;
        vr_nlt_frames = -10; // (the main hand's pose settled first)
        localcmd("vr_mock_hand main 0.03 1.69 -0.20 70 0 0\n"); // (level, at the eye: the nail pistol's aim test's sight picture)
        dprint(sprintf("nailtest: vr_nail_speed %g, vr_nailpistol_speed %g, vr_nail_scale %g\n", cvar("vr_nail_speed"),
            cvar("vr_nailpistol_speed"), cvar("vr_nail_scale")));
        return;
    }
    if(!vr_nlt_step)
    {
        return;
    }
    vr_nlt_frames++;
    float s = vr_nlt_step;
    if(vr_nlt_frames > 1200)
    {
        VR_Test_Check(FALSE, sprintf("timed out (step %g)", s));
        VR_NLT_End();
        return;
    }
    if(s == 1)
    {
        // Each case: fired at frame 2, its origin a frame later, measured two frames after that.
        if(vr_nlt_frames == 2)
        {
            VR_NLT_Fire(vr_nlt_case);
            if(!vr_nlt_nail)
            {
                VR_Test_Check(FALSE, sprintf("%s: no nail", VR_NLT_CaseName(vr_nlt_case)));
                vr_nlt_frames = 6;
            }
        }
        else if(vr_nlt_frames == 3 && vr_nlt_nail)
        {
            vr_nlt_o1 = vr_nlt_nail.origin;
            vr_nlt_t1 = time;
        }
        else if(vr_nlt_frames == 5 && vr_nlt_nail)
        {
            float flown = vlen(vr_nlt_nail.origin - vr_nlt_o1) / max(0.001, time - vr_nlt_t1);
            float want = vr_nlt_case == 2 ? cvar("vr_nailpistol_speed") : cvar("vr_nail_speed");
            float before = vr_nlt_case == 2 ? 2600 : 1750;
            float r = flown / before;
            VR_Test_Check(fabs(flown - want) <= want * 0.01 && r >= 1.15 && r <= 1.2,
                sprintf("%s: %.0f u/s flown (setting %g; before %g: x%.3f, want x1.15..1.20)", VR_NLT_CaseName(vr_nlt_case),
                    flown, want, before, r));
            remove(vr_nlt_nail);
            vr_nlt_nail = world;
        }
        if(vr_nlt_frames >= 6)
        {
            vr_nlt_case++;
            vr_nlt_frames = 0;
            if(vr_nlt_case > 3)
            {
                VR_AGT_Remove(vr_nlt_g);
                vr_nlt_g = VR_NPT_Grunt(256, -1, 0);
                vr_nlt_mode = 0;
                VR_NLT_Next();
            }
        }
    }
    else if(s == 2)
    {
        // A sweep each mode: a nail at each offset once the one before is gone (met something, or the wall beyond); its
        // result once the last is gone.
        if(vr_nlt_frames == 1)
        {
            cvar_set("vr_hit_precise", vr_nlt_mode == 0 ? "1" : "0");
            vr_nlt_hitlo = 1000;
            vr_nlt_hithi = -1000;
            vr_nlt_hit0 = FALSE;
            vr_nlt_k = -30;
        }
        else if(VR_NLT_SweepLeft())
        {
            // (its nail still flying)
        }
        else if(vr_nlt_k <= 30)
        {
            VR_NLT_SweepNail(vr_nlt_k);
            vr_nlt_k += 2;
        }
        else
        {
            string what = vr_nlt_mode == 0 ? "precise hits (its model)" : "Quake's boxes";
            float lim = 16 + (vr_nlt_mode == 0 ? cvar("vr_hit_tolerance_guns") : max(0, cvar("vr_gameplayfix_missilesize")));
            VR_Test_Check(vr_nlt_hit0 && vr_nlt_hitlo >= -lim && vr_nlt_hithi <= lim,
                sprintf("%s: the nails that met the grunt from %g to %g units off its middle (the middle %s; at most %g: its box 16 and the tolerance; Quake's 15-unit missile box would meet to 31)",
                    what, vr_nlt_hitlo, vr_nlt_hithi, vr_nlt_hit0 ? "met" : "MISSED", lim));
            vr_nlt_mode++;
            vr_nlt_frames = 0;
            if(vr_nlt_mode > 1)
            {
                cvar_set("vr_hit_precise", ftos(vr_nlt_precise));
                VR_AGT_Remove(vr_nlt_g);
                vr_nlt_g = world;
                VR_NLT_Next();
            }
        }
    }
    else if(s == 3)
    {
        if(vr_nlt_frames == 1)
        {
            makevectors('0 1 0' * vr_nlt_player.angles_y);
            vector at = vr_nlt_player.origin + v_forward * 64 + '0 0 16';
            VR_NLT_Hang("progs/spike.mdl", at - v_right * 8, v_right, 2);
            VR_NLT_Hang("progs/s_spike.mdl", at + v_right * 8, v_right, 2);
        }
        else if(vr_nlt_frames == 4)
        {
            dprint(sprintf("nailtest: the nails' drawn size (vr_nail_scale %g; 1 before):\n", cvar("vr_nail_scale")));
            localcmd(sprintf("vr_debug_box3d 1; vr_physics_list spike; vr_debug_box3d %g\n", cvar("vr_debug_box3d")));
        }
        else if(vr_nlt_frames == 7)
        {
            entity e = find(world, classname, "spike");
            while(e) { entity nx = find(e, classname, "spike"); if(e.movetype == MOVETYPE_NONE) { remove(e); } e = nx; }
            VR_NLT_Next();
        }
    }
    else if(s == 4)
    {
        VR_NLT_BagStep();
    }
    else if(s == 10)
    {
        VR_NLT_WoundStep();
    }
};
#endif
