// Dead monsters stay dead (ROUND22.md, "A beheaded zombie knight ran again"): a watch for any dead monster that animates
// again (vr_debug_deadanim; Debug > Logging, "Dead Monsters Animating"), and its test (vr_deadanim_test; developer 1).
// After AD's files: it knows AD's monsters' animation callbacks.
//
// A monster is dead once its health is down to 0 or its deadflag is set (a deliberate rising, an intact AD zombie's, is
// never a death: its health is whole). Each server frame the watch looks at every dead monster still drawn:
// - its think one of its AI's animations (its stand, walk, run, missile or melee callbacks; AD's, and AD's wake-up):
//   "deadanim: ... runs its <which>";
// - its frame changing VR_DA_SETTLE s after it died (its death animation long over), twice within VR_DA_AGAIN s (once:
//   the engine putting a corpse stuck mid-death at its death's last frame): "deadanim: ... animates".
// Each is printed (dprint: developer 1) a few times at most per monster; vr_deadanim_count counts them for the tests.

.float vr_da_dead;   // when it was first seen dead (0: alive, or not watched)
.float vr_da_frame;  // its frame when last seen
.float vr_da_model;  // and its model (a head left of it, a gibbed one: watched no more)
.float vr_da_moved;  // when its frame last changed after its death was over
.float vr_da_warned; // its warnings printed
float vr_deadanim_count; // warnings this map (vr_deadanim_test 1)
float VR_DA_SETTLE = 3;  // s after its death: its death animation is over
float VR_DA_AGAIN = 0.5; // s: its frame changing twice within this, it animates
float VR_DA_WARN_MOST = 3;

float(entity e) VR_DeadAnim_IsDead =
{
    return (e.flags & FL_MONSTER) && (e.health <= 0 || e.deadflag != DEAD_NO);
};

// Which of its AI's animations its think is (0: none): 1 stand, 2 walk, 3 run, 4 missile, 5 melee, 6 AD's wake-up.
float(entity e) VR_DeadAnim_AiThink =
{
    void() f = e.think;
    if(!f || f == SUB_Null)
    {
        return 0;
    }
    if(f == e.th_stand || f == e.ad_th_stand) { return 1; }
    if(f == e.th_walk || f == e.ad_th_walk) { return 2; }
    if(f == e.th_run || f == e.ad_th_run) { return 3; }
    if(f == e.th_missile || f == e.ad_th_missile) { return 4; }
    if(f == e.th_melee || f == e.ad_th_melee) { return 5; }
    if(f == e.ad_th_wakeup) { return 6; }
    return 0;
};

string(float xWhich) VR_DeadAnim_Name =
{
    if(xWhich == 1) { return "stand"; }
    if(xWhich == 2) { return "walk"; }
    if(xWhich == 3) { return "run"; }
    if(xWhich == 4) { return "missile attack"; }
    if(xWhich == 5) { return "melee attack"; }
    return "wake-up";
};

void(entity e, string xWhat) VR_DeadAnim_Warn =
{
    vr_deadanim_count = vr_deadanim_count + 1;
    e.vr_da_warned = e.vr_da_warned + 1;
    if(e.vr_da_warned > VR_DA_WARN_MOST)
    {
        return;
    }
    dprint(sprintf("deadanim: dead %s (health %g, deadflag %g, headless %g, ragdoll %g) %s, %.1f s after its death",
        e.classname, e.health, e.deadflag, e.vr_headless, isragdoll(e), xWhat, time - e.vr_da_dead));
    dprint(sprintf(" (frame %g, solid %g, movetype %g, at %v)\n", e.frame, e.solid, e.movetype, e.origin));
};

// StartFrame: the watch (vr_debug_deadanim).
void() VR_DeadAnim_Frame =
{
    if(!cvar_hget(cvarh_vr_debug_deadanim))
    {
        return;
    }
    for(entity e = nextent(world); e; e = nextent(e))
    {
        if(!VR_DeadAnim_IsDead(e) || !e.modelindex)
        {
            e.vr_da_dead = 0;
            continue;
        }
        if(!e.vr_da_dead || e.modelindex != e.vr_da_model)
        {
            e.vr_da_dead = time;
            e.vr_da_frame = e.frame;
            e.vr_da_model = e.modelindex;
            e.vr_da_warned = 0;
            e.vr_da_moved = 0;
        }
        float ai = VR_DeadAnim_AiThink(e);
        if(ai && e.nextthink > 0)
        {
            VR_DeadAnim_Warn(e, sprintf("runs its %s", VR_DeadAnim_Name(ai)));
        }
        else if(e.frame != e.vr_da_frame && time > e.vr_da_dead + VR_DA_SETTLE)
        {
            // (Once is a corpse put at its death's last frame, the engine's: it animates if it changes again soon.)
            if(time - e.vr_da_moved < VR_DA_AGAIN)
            {
                VR_DeadAnim_Warn(e, sprintf("animates (frame %g to %g)", e.vr_da_frame, e.frame));
            }
            e.vr_da_moved = time;
        }
        e.vr_da_frame = e.frame;
    }
};

// ---------------------------------------------------------------------------------------------------------------------
// The test (vr_deadanim_test N; self the player; Misc/quakevr/deadanim_test.sh): on the nearest live monster,
//   1 the watch's report: the dead monsters watched, each one's state, the warnings so far ("deadanim: report")
//   2 a stagger (its ragdoll's, steady: it recovers), 3 a parry's stagger (VR_Parry_Ragdoll), 4 an AD zombie's or
//   Quake's zombie's own fall (no ragdoll: AD's ad_zombiek_knockdown, ad_zombie_paine1, Quake's zombie_paine1), 5 its use fired (a
//   trigger reaching it: the nearest dead one's), 6 killed by a blow of its health (a zombie: gibbed), 7 the nearest
//   dead one's think and its frame (its death's state), 8 an AD zombie laid on the floor as a map's dormant one (AD's
//   onground ZOMBIEFLR: up when its trigger uses it), 9 the nearest live one's state (knocked down 1, getting up 2;
//   staggering, a parry's)

entity(float xLive) VR_DeadAnim_Nearest =
{
    entity best = world;
    float bestDist = 100000;
    for(entity m = nextent(world); m; m = nextent(m))
    {
        if((m.flags & FL_MONSTER) && m.modelindex && (xLive ? !VR_DeadAnim_IsDead(m) : VR_DeadAnim_IsDead(m)) &&
           vlen(m.origin - self.origin) < bestDist)
        {
            best = m;
            bestDist = vlen(m.origin - self.origin);
        }
    }
    return best;
};

void(float xMode) VR_DeadAnim_Test =
{
    if(xMode == 1)
    {
        float n = 0;
        for(entity e = nextent(world); e; e = nextent(e))
        {
            if(VR_DeadAnim_IsDead(e) && e.modelindex)
            {
                n = n + 1;
                dprint(sprintf("deadanim: %s dead %.1f s, frame %g, think AI %g, headless %g, ragdoll %g, warned %g\n",
                    e.classname, e.vr_da_dead ? time - e.vr_da_dead : -1, e.frame, VR_DeadAnim_AiThink(e), e.vr_headless,
                    isragdoll(e), e.vr_da_warned));
            }
        }
        dprint(sprintf("deadanim: report: %g dead watched, %g warnings, time %.1f\n", n, vr_deadanim_count, time));
        return;
    }
    entity m = VR_DeadAnim_Nearest(xMode != 5 && xMode != 7);
    if(m == world)
    {
        dprint(sprintf("deadanim: test %g: no monster\n", xMode));
        return;
    }
    entity oself = self;
    float r = 0;
    if(xMode == 2 || xMode == 3)
    {
        vector dir = m.origin - self.origin;
        dir_z = 0;
        if(xMode == 2)
        {
            if(ragdollcanstagger(m))
            {
                r = VR_Knockdown_Stagger(m, normalize(dir), 0.4, 1);
            }
        }
        else
        {
            r = VR_Parry_Ragdoll(m, self, 1);
        }
    }
    else if(xMode == 4)
    {
        self = m;
        if(m.ad_classtype == ad_CT_MONZOMBIEK)
        {
            ad_zombiek_knockdown();
            r = 1;
        }
        else if(m.ad_classtype == ad_CT_MONZOMBIE || m.ad_classtype == ad_CT_MONMUMMY)
        {
            ad_zombie_paine1();
            r = 1;
        }
        else if(m.classname == "monster_zombie")
        {
            zombie_paine1();
            r = 1;
        }
        self = oself;
    }
    else if(xMode == 5)
    {
        if(m.use)
        {
            activator = self;
            ad_activator = self;
            self = m;
            self.use();
            self = oself;
            r = 1;
        }
    }
    else if(xMode == 6)
    {
        T_Damage(m, self, self, max(1, m.health));
        r = 1;
    }
    else if(xMode == 8)
    {
        self = m;
        if(m.ad_classtype == ad_CT_MONZOMBIEK)
        {
            ad_zombiek_onground(ad_MONAI_ZOMBIEFLR);
            r = 1;
        }
        else if(m.ad_classtype == ad_CT_MONZOMBIE || m.ad_classtype == ad_CT_MONMUMMY)
        {
            ad_zombie_onground(ad_MONAI_ZOMBIEFLR);
            r = 1;
        }
        self = oself;
    }
    else if(xMode == 9)
    {
        dprint(sprintf("deadanim: test 9: %s before the blow: knockdown %g stagger %g parry %g frame %g ragdoll %g\n",
            m.classname, m.vr_knockdown, m.vr_kd_stagger, m.vr_kd_parry, m.frame, isragdoll(m)));
        return;
    }
    else if(xMode == 7)
    {
        r = VR_DeadAnim_AiThink(m);
        dprint(sprintf("deadanim: test 7: %s think AI %g nextthink in %.2f frame %g\n", m.classname, r,
            m.nextthink - time, m.frame));
    }
    dprint(sprintf("deadanim: test %g on %s: %g (health %g, knockdown %g, ragdoll %g)\n", xMode, m.classname, r, m.health,
        m.vr_knockdown, isragdoll(m)));
};

// StartFrame: a test asked for (vr_deadanim_test).
void() VR_DeadAnim_TestFrame =
{
    float ask = cvar_hget(cvarh_vr_deadanim_test);
    if(!ask)
    {
        return;
    }
    cvar_set("vr_deadanim_test", "0");
    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    VR_DeadAnim_Test(ask);
    self = oself;
};
