#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_ragfire_test.qc (docs/vr-port/RAGFIRE.md; vr_agrunt_ragfire, a prototype):
//
// Test 237: armed humanoids firing as ragdolls, their gun hand's wrist turning the gun onto you (ragfire2). Each case
// spawns one ahead of you (you unseen while it is set up), knocks it down (a topple: down vr_knockdown_time_min..max,
// set to 6 s here; its landings never cost it its weapon) and then lets it see you, until it gets up.
// - The firing cases: a grunt's revolver, an enforcer's and a grunt's nail pistol, each toppled onto its back, its
//   front, its left and its right side, in two rounds (the second turned 25 degrees and farther off), then each
//   staggering. Each case's line gives its shots, its due shots within the cone, those held back as blocked, and its
//   drawn gun's angle off you over its time down (median / 90th %); a case FAILs only if it never got down, dropped its
//   gun, timed out, or (on its back, the pose it lands in best) never fired.
// - The never cases: a grunt's rocket launcher (a big, explosive gun) and a grunt's revolver with vr_agrunt_ragfire 0.
// - The summary checks: over every firing case's time down, the drawn gun's angle off you (median, 90th %, by pose);
//   the shots' own (median within VR_AGRF_SHOT_MEDIAN degrees: shots leave along the drawn gun) and shots per 10 s down.
//   `ragfire summary: ...` lines carry the numbers (RAGFIRE.md's table).
// The test's last line is agtest's `agtest: test 237 over: N passed, M failed` (run.sh -QuitOn on it).
// ----------------------------------------------------------------------------

float vr_agrf_case;
float vr_agrf_t0;
float vr_agrf_down;
float vr_agrf_on0;      // vr_agrunt_ragfire as it was
float vr_agrf_keep0;    // vr_agrunt_fall_keep_impact as it was
float vr_agrf_tmin0;    // vr_knockdown_time_min, _max as they were
float vr_agrf_tmax0;
float vr_agrf_asked0;   // the global counts at the case's start
float vr_agrf_blocked0;
float vr_agrf_shots0;   // vr_ag_rf_fired at the case's start
float vr_agrf_downtime; // the firing cases' time down, all told (s)
float vr_agrf_shots;    // ... their shots
vector vr_agrf_home;
float VR_AGRF_GUNS = 3;   // the firing guns (VR_AGRF_Wid 0..2)
float VR_AGRF_POSES = 5;  // back, front, left side, right side, staggering
float VR_AGRF_ROUNDS = 2; // the toppled poses twice (the second turned and farther)
float VR_AGRF_SHOT_MEDIAN = 15; // degrees: the shots' drawn gun angle off you, median, at most
float VR_AGRF_BIN = 3;    // degrees a histogram bin
float VR_AGRF_BINS = 60;  // ... bins (0..180)
// The drawn gun's angle off you, histograms: a case's (0..59), by pose (60 + pose * 60), the shots' (360 + 0..59).
float vr_agrf_hist[420];

float() VR_AGRF_Firing = { return VR_AGRF_GUNS * (VR_AGRF_POSES - 1) * VR_AGRF_ROUNDS + VR_AGRF_GUNS; };
float() VR_AGRF_Count = { return VR_AGRF_Firing() + 2; };
// The case's gun (0 grunt revolver, 1 enforcer nail pistol, 2 grunt nail pistol, 3 grunt rocket launcher, 4 grunt
// revolver with the setting off), pose (0 back, 1 front, 2 left side, 3 right side, 4 staggering) and round.
float(float c) VR_AGRF_Gun =
{
    float n = VR_AGRF_Firing();
    if(c >= n)
    {
        return 3 + (c - n);
    }
    return c - VR_AGRF_GUNS * floor(c / VR_AGRF_GUNS);
};
float(float c) VR_AGRF_Pose =
{
    float topples = VR_AGRF_GUNS * (VR_AGRF_POSES - 1) * VR_AGRF_ROUNDS;
    if(c >= VR_AGRF_Firing())
    {
        return 0;
    }
    if(c >= topples)
    {
        return 4;
    }
    float k = floor(c / VR_AGRF_GUNS);
    return k - (VR_AGRF_POSES - 1) * floor(k / (VR_AGRF_POSES - 1));
};
float(float c) VR_AGRF_Round =
{
    return c < VR_AGRF_GUNS * (VR_AGRF_POSES - 1) * VR_AGRF_ROUNDS ? floor(c / (VR_AGRF_GUNS * (VR_AGRF_POSES - 1))) : 0;
};
float(float c) VR_AGRF_Wid =
{
    float gun = VR_AGRF_Gun(c);
    if(gun == 1 || gun == 2) { return WID_NAILPISTOL; } // (a long gun lying muzzle down is held back: RAGFIRE.md, "Results")
    if(gun == 3) { return WID_ROCKET_LAUNCHER; }
    return WID_REVOLVER;
};
float(float c) VR_AGRF_Kind = { return VR_AGRF_Gun(c) == 1 ? 1 : 0; };
float(float c) VR_AGRF_Fires = { return VR_AGRF_Gun(c) <= 2; }; // (3: an excluded gun; 4: the setting off)
string(float pose) VR_AGRF_PoseName =
{
    if(pose == 0) { return "back"; }
    if(pose == 1) { return "front"; }
    if(pose == 2) { return "left side"; }
    if(pose == 3) { return "right side"; }
    return "staggering";
};
string(float c) VR_AGRF_Name =
{
    float gun = VR_AGRF_Gun(c);
    if(gun == 3) { return "grunt rocket launcher (never)"; }
    if(gun == 4) { return "grunt revolver, setting off (never)"; }
    string g = gun == 0 ? "grunt revolver" : gun == 1 ? "enforcer nail pistol" : "grunt nail pistol";
    return sprintf("%s, %s%s", g, VR_AGRF_PoseName(VR_AGRF_Pose(c)), VR_AGRF_Round(c) ? ", round 2" : "");
};

void(float base) VR_AGRF_Clear =
{
    float i = 0;
    while(i < VR_AGRF_BINS)
    {
        vr_agrf_hist[base + i] = 0;
        i = i + 1;
    }
};
void(float base, float ang) VR_AGRF_Add =
{
    float b = min(VR_AGRF_BINS - 1, floor(max(0, ang) / VR_AGRF_BIN));
    vr_agrf_hist[base + b] = vr_agrf_hist[base + b] + 1;
};
float(float base) VR_AGRF_Total =
{
    float i = 0;
    float n = 0;
    while(i < VR_AGRF_BINS)
    {
        n = n + vr_agrf_hist[base + i];
        i = i + 1;
    }
    return n;
};
// The q-th quantile (0..1) of a histogram, degrees (its bin's middle); -1 empty.
float(float base, float q) VR_AGRF_Quantile =
{
    float n = VR_AGRF_Total(base);
    if(n <= 0)
    {
        return -1;
    }
    float i = 0;
    float sum = 0;
    while(i < VR_AGRF_BINS)
    {
        sum = sum + vr_agrf_hist[base + i];
        if(sum >= q * n)
        {
            return (i + 0.5) * VR_AGRF_BIN;
        }
        i = i + 1;
    }
    return (VR_AGRF_BINS - 0.5) * VR_AGRF_BIN;
};

void() VR_AGRF_Summary =
{
    float pose = 0;
    float all = 0;
    while(pose < VR_AGRF_POSES)
    {
        float base = VR_AGRF_BINS * (1 + pose);
        all = all + VR_AGRF_Total(base);
        VR_Test_Print(sprintf("ragfire summary: %s: gun angle off its target median %.0f, p90 %.0f deg (%g frames)\n",
            VR_AGRF_PoseName(pose), VR_AGRF_Quantile(base, 0.5), VR_AGRF_Quantile(base, 0.9), VR_AGRF_Total(base)));
        pose = pose + 1;
    }
    // (All poses together: their histograms summed into the case's, free now.)
    float i = 0;
    while(i < VR_AGRF_BINS)
    {
        float s = 0;
        pose = 0;
        while(pose < VR_AGRF_POSES)
        {
            s = s + vr_agrf_hist[VR_AGRF_BINS * (1 + pose) + i];
            pose = pose + 1;
        }
        vr_agrf_hist[i] = s;
        i = i + 1;
    }
    float shotBase = VR_AGRF_BINS * 6;
    float per10 = vr_agrf_downtime > 0 ? 10 * vr_agrf_shots / vr_agrf_downtime : 0;
    VR_Test_Print(sprintf("ragfire summary: all: gun angle off its target median %.0f, p90 %.0f deg (%g frames); %g shots in %.1f s down (%.2f per 10 s)\n",
        VR_AGRF_Quantile(0, 0.5), VR_AGRF_Quantile(0, 0.9), all, vr_agrf_shots, vr_agrf_downtime, per10));
    float sm = VR_AGRF_Quantile(shotBase, 0.5);
    float s90 = VR_AGRF_Quantile(shotBase, 0.9);
    float cone = cvar_hget(cvarh_vr_agrunt_ragfire_cone);
    VR_AGT_Check(sprintf("ragfire summary: shots: drawn gun angle off its target median %.0f, p90 %.0f deg (%g shots; median within %g, cone %g)",
        sm, s90, VR_AGRF_Total(shotBase), VR_AGRF_SHOT_MEDIAN, cone), sm >= 0 && sm <= VR_AGRF_SHOT_MEDIAN);
};

void() VR_AGRF_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    p.flags = p.flags | FL_GODMODE;
    if(vr_agt_step == 0)
    {
        if(vr_agrf_case == 0)
        {
            vr_agrf_home = p.origin;
            vr_agrf_on0 = cvar_hget(cvarh_vr_agrunt_ragfire);
            vr_agrf_keep0 = cvar_hget(cvarh_vr_agrunt_fall_keep_impact);
            vr_agrf_tmin0 = cvar_hget(cvarh_vr_knockdown_time_min);
            vr_agrf_tmax0 = cvar_hget(cvarh_vr_knockdown_time_max);
            cvar_set("vr_agrunt_fall_keep_impact", "100");
            cvar_set("vr_knockdown_time_min", "6");
            cvar_set("vr_knockdown_time_max", "6");
            float b = 0;
            while(b < 7)
            {
                VR_AGRF_Clear(VR_AGRF_BINS * b);
                b = b + 1;
            }
            vr_agrf_downtime = 0;
            vr_agrf_shots = 0;
        }
        if(vr_agrf_case >= VR_AGRF_Count())
        {
            VR_AGRF_Summary();
            cvar_set("vr_agrunt_ragfire", ftos(vr_agrf_on0));
            cvar_set("vr_agrunt_fall_keep_impact", ftos(vr_agrf_keep0));
            cvar_set("vr_knockdown_time_min", ftos(vr_agrf_tmin0));
            cvar_set("vr_knockdown_time_max", ftos(vr_agrf_tmax0));
            cvar_set("vr_agrunt_test_kind", "0");
            p.flags = p.flags - (p.flags & (FL_NOTARGET | FL_GODMODE));
            vr_agrf_case = 0;
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        cvar_set("vr_agrunt_ragfire", VR_AGRF_Gun(vr_agrf_case) == 4 ? "0" : "1");
        cvar_set("vr_agrunt_test_kind", ftos(VR_AGRF_Kind(vr_agrf_case)));
        p.flags = p.flags | FL_NOTARGET;
        setorigin(p, vr_agrf_home);
        p.velocity = '0 0 0';
        vr_agt_grunt = g = VR_AGT_Spawn(VR_AGRF_Wid(vr_agrf_case), VR_AGRF_Round(vr_agrf_case) ? 120 : 80);
        g.health = 400; // (a landing's impact damage never kills it)
        VR_AGRF_Clear(0);
        vr_agrf_t0 = time;
        vr_agrf_down = 0;
        vr_agt_step = 1;
        return;
    }
    string name = VR_AGRF_Name(vr_agrf_case);
    float pose = VR_AGRF_Pose(vr_agrf_case);
    float fires = VR_AGRF_Fires(vr_agrf_case);
    if(vr_agt_step == 1)
    {
        if(!g.takedamage || time < vr_agrf_t0 + 0.3)
        {
            return; // (its arms posed and sent first)
        }
        // Its fall's way: back (away from you), front, its left, its right; the second round's turned 25 degrees.
        float yaw = g.angles_y + (pose == 0 ? 180 : pose == 2 ? 90 : pose == 3 ? -90 : 0);
        yaw = yaw + (VR_AGRF_Round(vr_agrf_case) ? 25 : 0);
        makevectors('0 1 0' * yaw);
        if(pose == 4)
        {
            makevectors('0 1 0' * (g.angles_y + 180));
            if(!VR_Knockdown_Stagger(g, v_forward, 1, 0.3))
            {
                VR_Knockdown_Start(g);
                ragdollshovetopple(g, v_forward, 1, FALSE, 40);
            }
        }
        else
        {
            VR_Knockdown_Start(g);
            ragdollshovetopple(g, v_forward, 1, FALSE, 40);
        }
        g.enemy = p; // (it knows where you are: its AI asleep while it lies, its gun not)
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        g.vr_ag_rf_shots = 0;
        vr_agrf_asked0 = vr_ag_rf_asked;
        vr_agrf_blocked0 = vr_ag_rf_blocked;
        vr_agrf_shots0 = vr_ag_rf_fired;
        vr_agrf_t0 = time;
        vr_agt_step = 2;
        return;
    }
    float t = time - vr_agrf_t0;
    float lying = (g.vr_knockdown == 1 && isragdoll(g) && !g.vr_kd_rise);
    if(lying)
    {
        vr_agrf_down = 1;
        if(fires)
        {
            vr_agrf_downtime = vr_agrf_downtime + frametime;
            // (Its drawn gun's angle off you, this frame's, once its arm's reach has eased in.)
            if(t > 0.5 && vr_ag_rf_ang_at > time - 0.05)
            {
                VR_AGRF_Add(0, vr_ag_rf_ang);
                VR_AGRF_Add(VR_AGRF_BINS * (1 + pose), vr_ag_rf_ang);
            }
        }
    }
    float shots = g.vr_ag_rf_shots;
    if(fires && vr_ag_rf_fired > vr_agrf_shots0)
    {
        VR_AGRF_Add(VR_AGRF_BINS * 6, vr_ag_rf_shot_ang); // (one shot a frame at most: its interval)
        vr_agrf_shots = vr_agrf_shots + (vr_ag_rf_fired - vr_agrf_shots0);
        vr_agrf_shots0 = vr_ag_rf_fired;
    }
    string why = "";
    if(!g.vr_ag_prop)
    {
        why = "it dropped its weapon";
    }
    else if(!vr_agrf_down && t > 1)
    {
        why = "never became a ragdoll";
    }
    if(why == "")
    {
        if(vr_agrf_down && !lying)
        {
            why = "ok"; // (getting up, or its stagger over: its time down over)
        }
        else if(t > 9)
        {
            why = "timed out (still down 9 s on)";
        }
        else
        {
            return;
        }
    }
    // (On its back it must fire (it lands well); the other poses as they land; the never cases never.)
    float ok = (why == "ok" && (!fires ? shots == 0 : pose != 0 || shots >= 1));
    string angs = sprintf("gun angle median %.0f, p90 %.0f deg", VR_AGRF_Quantile(0, 0.5), VR_AGRF_Quantile(0, 0.9));
    VR_AGT_Check(sprintf("ragfire %s: %g shots as a ragdoll in %.2f s (%g due within the cone, %g blocked), %s%s",
        name, shots, t, vr_ag_rf_asked - vr_agrf_asked0, vr_ag_rf_blocked - vr_agrf_blocked0, angs,
        why == "ok" ? "" : strcat(": ", why)), ok);
    VR_AGT_Remove(g);
    vr_agt_grunt = world;
    vr_agrf_case = vr_agrf_case + 1;
    vr_agt_step = 0;
};
#endif
