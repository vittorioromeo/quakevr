#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_gunlag_test.qc: test 205 (vr_agrunt_test 205), a held gun stays with its body as it walks (the note
// vrragtriggers_2026-11-03_02-23-27: knights' and death knights' guns lagged while they sidestepped, backpedalled and came
// on; grunts' and enforcers' not). Each kind in turn holds a nail pistol, in vrtesthall, plays its own run frames and is
// moved at VR_AGL_SPEED by this test, along its facing and to each side (never turned, its aim straight ahead): on the
// spot, forward, right, backwards, left. Each phase's numbers are its drawn hand from its drawn head, model space
// (weapondrawnpose(e, side, 29)): mean, and off the phase on the spot's mean (`off`: forward, left, up; `way`: along the way
// it goes, below 0 behind it: a lag). Ends on `agtest: test 205 over: N passed, M failed`.
// ----------------------------------------------------------------------------

float VR_AGL_SPEED = 120;
float VR_AGL_PHASE_SECS = 1.6;
float VR_AGL_SETTLE = 0.5;
float VR_AGL_PHASES = 5;
float VR_AGL_MOST = 3; // units: its hand off where it runs on the spot (the frames' own sway: under 1.5; a lag: 4 to 9)
float vr_agl_kind;
float vr_agl_phase;
float vr_agl_t0;
vector vr_agl_sum;
float vr_agl_n;
vector vr_agl_base;
vector vr_agl_last;
float vr_agl_jump;
vector vr_agl_fw;
vector vr_agl_right;
vector vr_agl_home;
vector vr_agl_view; // (your view's way: across it)
entity vr_agl_prey;
float vr_agl_i; // its run frame next
float vr_agl_shot; // this phase's screenshot taken

string(float xPhase) VR_AGL_PhaseName =
{
    if(xPhase == 0) { return "on the spot"; }
    if(xPhase == 1) { return "forward"; }
    if(xPhase == 2) { return "right"; }
    if(xPhase == 3) { return "backwards"; }
    return "left";
};

vector(float xPhase) VR_AGL_Way =
{
    if(xPhase == 1) { return vr_agl_fw; }
    if(xPhase == 2) { return vr_agl_right; }
    if(xPhase == 3) { return -vr_agl_fw; }
    if(xPhase == 4) { return -vr_agl_right; }
    return '0 0 0';
};

// Its run's frames, played as its AI plays them (no AI: this test moves it, never turned).
void() VR_AGL_Step =
{
    VR_AM_Anim(VR_AM_A_RUN, vr_agl_i, VR_AGL_Step);
    vr_agl_i = (vr_agl_i + 1) % 8;
    setorigin(self, self.origin + VR_AGL_Way(vr_agl_phase) * (VR_AGL_SPEED * 0.1));
    self.flags = self.flags | FL_ONGROUND;
    self.velocity = '0 0 0';
};

void() VR_AGL_Spawn =
{
    entity p = vr_agt_player;
    makevectors('0 1 0' * p.angles_y);
    vr_agl_view = v_forward;
    vr_agl_fw = v_right; // (its facing across your view: you see its side)
    vr_agl_right = -v_forward;
    entity oldself = self;
    self = spawn();
    if(vr_agl_kind == 0)
    {
        vr_agl_home = p.origin + vr_agl_view * 160;
    }
    setorigin(self, vr_agl_home);
    self.angles_y = vectoyaw(vr_agl_fw);
    self.weapon = WID_NAILPISTOL;
    VR_AM_Spawn(vr_agl_kind);
    vr_agt_grunt = self;
    self = oldself;
    vr_agt_step = 1;
    vr_agl_t0 = time;
};

void() VR_AGL_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGPT_Clear();
        p.flags = p.flags | FL_NOTARGET;
        vr_agl_kind = 0;
        vr_agl_prey = spawn();
        vr_agl_prey.classname = "vr_agl_prey";
        VR_AGL_Spawn();
        return;
    }
    if(vr_agt_step == 1)
    {
        if(time - vr_agl_t0 < 1)
        {
            return; // (its spawn's drop, its weapon in its hand)
        }
        g.enemy = vr_agl_prey;
        g.angles_y = vectoyaw(vr_agl_fw); // (its AI off from here: it may have turned to you meanwhile)
        g.attack_finished = time + 1000;
        vr_agl_i = 0;
        g.think = VR_AGL_Step;
        g.nextthink = time + 0.1;
        vr_agl_phase = 0;
        vr_agl_shot = FALSE;
        vr_agl_t0 = time;
        vr_agl_n = 0;
        vr_agl_sum = '0 0 0';
        vr_agl_jump = 0;
        vr_agt_step = 2;
        return;
    }
    setorigin(vr_agl_prey, g.origin + vr_agl_fw * 600);
    vector camera = vr_agl_home - vr_agl_view * 80;
    camera_z = p.origin_z;
    setorigin(p, camera);
    p.velocity = '0 0 0';
    float t = time - vr_agl_t0;
    if(cvar_hget(cvarh_scratch1) == 1 && (t >= VR_AGL_PHASE_SECS * 0.5) && !vr_agl_shot)
    {
        vr_agl_shot = TRUE; // (its side at its place, as it crosses it)
        localcmd("screenshot\n");
    }
    if(t >= VR_AGL_SETTLE)
    {
        vector rel = weapondrawnpose(g, 0, 29);
        if(rel != '0 0 0')
        {
            vr_agl_sum = vr_agl_sum + rel;
            vr_agl_n = vr_agl_n + 1;
            if(vr_agl_n > 1)
            {
                vr_agl_jump = max(vr_agl_jump, vlen(rel - vr_agl_last));
            }
            vr_agl_last = rel;
        }
    }
    if(t < VR_AGL_PHASE_SECS)
    {
        return;
    }
    vector mean = vr_agl_n ? vr_agl_sum * (1 / vr_agl_n) : '0 0 0';
    if(vr_agl_phase == 0)
    {
        vr_agl_base = mean;
    }
    vector off = mean - vr_agl_base;
    // The way it goes in its own frame (x forward, y left): 1 forward, 2 right, 3 back, 4 left.
    vector wayM = '0 0 0';
    if(vr_agl_phase == 1) { wayM = '1 0 0'; }
    else if(vr_agl_phase == 2) { wayM = '0 -1 0'; }
    else if(vr_agl_phase == 3) { wayM = '-1 0 0'; }
    else if(vr_agl_phase == 4) { wayM = '0 1 0'; }
    float along = off * wayM;
    string numbers = sprintf("hand from head %.1f %.1f %.1f, off %.1f %.1f %.1f, way %.1f", mean_x, mean_y, mean_z, off_x, off_y,
        off_z, along);
    VR_AGT_Print(sprintf("gunlag: %s %s: %s, biggest frame step %.2f (%g frames)", VR_AM_UiKindName(vr_agl_kind),
        VR_AGL_PhaseName(vr_agl_phase), numbers, vr_agl_jump, vr_agl_n));
    if(vr_agl_phase == 0)
    {
        VR_AGT_Check(sprintf("gunlag: %s: its hand drawn on the spot", VR_AM_UiKindName(vr_agl_kind)), vr_agl_n > 40);
    }
    else
    {
        VR_AGT_Check(sprintf("gunlag: %s %s: its gun stays with it (%.1f off, most %g)", VR_AM_UiKindName(vr_agl_kind),
            VR_AGL_PhaseName(vr_agl_phase), vlen(off), VR_AGL_MOST), (vr_agl_n > 40) && (vlen(off) <= VR_AGL_MOST));
    }
    vr_agl_phase = vr_agl_phase + 1;
    vr_agl_shot = FALSE;
    vr_agl_t0 = time;
    vr_agl_n = 0;
    vr_agl_sum = '0 0 0';
    vr_agl_jump = 0;
    if(vr_agl_phase < VR_AGL_PHASES)
    {
        // (Across its place: as far before it as it will go after.)
        setorigin(g, vr_agl_home - VR_AGL_Way(vr_agl_phase) * (VR_AGL_SPEED * VR_AGL_PHASE_SECS * 0.5));
        return;
    }
    VR_AGT_Remove(g);
    vr_agl_kind = vr_agl_kind + 1;
    if(vr_agl_kind < VR_AM_COUNT)
    {
        VR_AGL_Spawn();
        return;
    }
    remove(vr_agl_prey);
    cvar_set("vr_agrunt_test", "0");
};
#endif
