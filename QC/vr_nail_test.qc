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
// vr_nail_test 2: three nails hung still 40 units before your eyes for 30 s (the nailgun's, the super nailgun's, a lava
// nail where Dissolution of Eternity is there), to see their size in the headset.

float vr_nlt_step;     // 0 idle
float vr_nlt_frames;   // frames into the step
float vr_nlt_pass, vr_nlt_fail;
float vr_nlt_case;     // step 1: 0 nailgun, 1 super nailgun, 2 nail pistol, 3 an armed grunt's nailgun
float vr_nlt_precise;  // step 2: the setting before
float vr_nlt_mode;     // step 2: 0 precise, 1 Quake's boxes
entity vr_nlt_player;
entity vr_nlt_nail;    // step 1: the nail measured
entity vr_nlt_g;       // the grunt
vector vr_nlt_o1;      // step 1: its origin a frame after it left
float vr_nlt_t1;
float vr_nlt_hitlo, vr_nlt_hithi; // step 2: the lowest and highest side offsets that met the grunt
float vr_nlt_hit0;     // ... the middle one met
float vr_nlt_k;        // ... the next nail's side offset
.float vr_nlt_off;     // a step 2 nail's side offset
.float vr_nlt_mark;    // a nail that was there before the shot (step 1: the new one is the one without)

void(float ok, string what) VR_NLT_Check =
{
    if(ok) vr_nlt_pass++; else vr_nlt_fail++;
    dprint(sprintf("nailtest: %s %s\n", ok ? "PASS" : "FAIL", what));
};

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
        VR_NLT_Check(n.mins == '0 0 0' && n.maxs == '0 0 0', sprintf("a nail's hit box: a point (mins %v, maxs %v)",
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
    vr_nlt_player.flags = vr_nlt_player.flags - (vr_nlt_player.flags & FL_NOTARGET);
    dprint(sprintf("nailtest: %g passed, %g failed\n", vr_nlt_pass, vr_nlt_fail));
    vr_nlt_step = 0;
};

void() VR_NailTest_Frame =
{
    float req = cvar("vr_nail_test");
    if(req)
    {
        cvar_set("vr_nail_test", "0");
        vr_nlt_player = find(world, classname, "player");
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
        vr_nlt_pass = vr_nlt_fail = 0;
        vr_nlt_precise = cvar("vr_hit_precise");
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
        VR_NLT_Check(FALSE, sprintf("timed out (step %g)", s));
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
                VR_NLT_Check(FALSE, sprintf("%s: no nail", VR_NLT_CaseName(vr_nlt_case)));
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
            VR_NLT_Check(fabs(flown - want) <= want * 0.01 && r >= 1.15 && r <= 1.2,
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
            VR_NLT_Check(vr_nlt_hit0 && vr_nlt_hitlo >= -lim && vr_nlt_hithi <= lim,
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
            VR_NLT_End();
        }
    }
};
#endif
