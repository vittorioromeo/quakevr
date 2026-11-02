#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_footstep_test.qc (ROUND23.md, "Enemy footsteps"; vr_footstep_test 1, vr_footsteps.cpp's vr_footstep_report):
//
// Each biped in turn, made by its own spawn function ahead of you, walks towards a far place on its own (ai_walk, you
// unseen: FL_NOTARGET) for its phase, then runs there (ai_run after a hidden enemy under the floor: no sight of it, no
// shots) back across, then is removed. Then the engine's report: per model its steps heard walking and running, how
// loud, and how far each was from the foot's contact as the client drew it, and the verdict line,
// `footstep_test: N passed, M failed` (-QuitOn). The phases are counted game time for two strides; nothing waits on a guess.
// `vr_footstep_test 2`: one crowd of 32 grunts hunting you, for the budget: the same report (see the "dropped" count).
// `vr_footstep_test <100 or more>`: a lone grunt, then ogre, fiend and shambler, the same phases, you kept that many units
// behind it: the report's steps and how loud they reach your head at that distance.
// ----------------------------------------------------------------------------

float vr_fst_case;   // the class in turn (1..)
float vr_fst_phase;  // 0 none yet, 1 walking, 2 running, 3 over
float vr_fst_until;
entity vr_fst_mon;
entity vr_fst_goal;
entity vr_fst_prey;
vector vr_fst_way;
vector vr_fst_home;

// The longest way along the floor from the player: the test's track.
vector() VR_FST_Way =
{
    entity p = VR_Player();
    vector best = '1 0 0';
    float most = 0;
    for(float a = 0; a < 360; a = a + 45)
    {
        vector d = '0 0 0';
        d_x = cos(a);
        d_y = sin(a);
        traceline(p.origin, p.origin + d * 4000, TRUE, p);
        float len = vlen(trace_endpos - p.origin);
        if(len > most)
        {
            most = len;
            best = d;
        }
    }
    return best;
};

// Its spawn function by case (0: past the last).
float() VR_FST_Spawn =
{
    if(vr_fst_case == 1) { monster_army(); }
    else if(vr_fst_case == 2) { monster_enforcer(); }
    else if(vr_fst_case == 3) { monster_knight(); }
    else if(vr_fst_case == 4) { monster_hell_knight(); }
    else if(vr_fst_case == 5) { monster_zombie(); }
    else if(vr_fst_case == 6) { monster_ogre(); }
    else if(vr_fst_case == 7) { monster_demon1(); }
    else if(vr_fst_case == 8) { monster_shambler(); }
    else { return FALSE; }
    return TRUE;
};

string() VR_FST_Class =
{
    if(vr_fst_case == 1) { return "monster_army"; }
    if(vr_fst_case == 2) { return "monster_enforcer"; }
    if(vr_fst_case == 3) { return "monster_knight"; }
    if(vr_fst_case == 4) { return "monster_hell_knight"; }
    if(vr_fst_case == 5) { return "monster_zombie"; }
    if(vr_fst_case == 6) { return "monster_ogre"; }
    if(vr_fst_case == 7) { return "monster_demon1"; }
    return "monster_shambler";
};

// A place at `dist` along the track, down on the floor.
vector(float dist) VR_FST_At =
{
    traceline(vr_fst_home, vr_fst_home + vr_fst_way * dist, TRUE, world);
    return trace_endpos - vr_fst_way * 32;
};

void() VR_FST_Next =
{
    vr_fst_case = vr_fst_case + 1;
    if(cvar_hget(cvarh_vr_footstep_test) >= 100 && vr_fst_case == 2)
    {
        vr_fst_case = 6; // (the lone runs: the grunt to compare with, then the heavy ones)
    }
    vr_fst_phase = 0;
    entity oldself = self;
    self = spawn();
    self.classname = VR_FST_Class();
    setorigin(self, VR_FST_At(120));
    self.angles_y = vectoyaw(vr_fst_way);
    if(!VR_FST_Spawn())
    {
        vr_fst_mon = world;
        self = oldself;
        return;
    }
    vr_fst_mon = self;
    self = oldself;
    vr_fst_phase = 0;
    vr_fst_until = time + 0.3; // (its spawn's walkmonster_start takes a frame or two)
};

void(entity g, vector at) VR_FST_GoalAt =
{
    setorigin(g, at);
};

void() VR_Footstep_TestFrame =
{
    float req = rint(cvar_hget(cvarh_vr_footstep_test));
    if(req <= 0)
    {
        return;
    }
    entity p = VR_Player();
    if(!p)
    {
        return;
    }
    if(!vr_fst_goal)
    {
        p.flags = p.flags | FL_NOTARGET | FL_GODMODE;
        vr_fst_home = p.origin;
        vr_fst_way = VR_FST_Way();
        vr_fst_goal = spawn();
        vr_fst_goal.classname = "vr_fst_goal";
        vr_fst_prey = spawn();
        vr_fst_prey.classname = "vr_fst_prey";
        vr_fst_prey.health = 100;
        vr_fst_prey.takedamage = DAMAGE_NO;
        vr_fst_prey.flags = FL_NOTARGET;
        vr_fst_case = 0;
        if(req == 2)
        {
            // The crowd: 32 grunts in a block ahead, hunting you.
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            for(float i = 0; i < 32; i = i + 1)
            {
                entity oself = self;
                self = spawn();
                self.classname = "monster_army";
                setorigin(self, VR_FST_At(200 + floor(i / 6) * 48) + '0 1 0' * ((i - floor(i / 6) * 6) * 40 - 100));
                monster_army();
                self = oself;
            }
            vr_fst_case = 999;
            vr_fst_until = time + 0.5; // (their spawns' walkmonster_start)
            return;
        }
        VR_FST_Next();
        return;
    }
    if(vr_fst_case == 999)
    {
        if(time >= vr_fst_until)
        {
            VR_MonstersTest_Run(2); // (each hunts you)
            vr_fst_case = 1000;
            vr_fst_until = time + 8;
        }
        return;
    }
    if(vr_fst_case >= 1000)
    {
        if(time >= vr_fst_until && vr_fst_case == 1000)
        {
            vr_fst_case = 1001;
            localcmd("vr_footstep_info\nvr_footstep_report\n");
        }
        return;
    }
    if(req >= 100 && vr_fst_mon)
    {
        p.movetype = MOVETYPE_NOCLIP; // (held there through whatever is in the way)
        p.velocity = '0 0 0';
        setorigin(p, vr_fst_mon.origin - vr_fst_way * req);
    }
    if(!vr_fst_mon)
    {
        if(vr_fst_case > 0)
        {
            vr_fst_case = 1000;
            vr_fst_until = time;
        }
        return;
    }
    if(time < vr_fst_until)
    {
        return;
    }
    entity oldself = self;
    if(vr_fst_phase == 0)
    {
        // Walking: towards a far place, you unseen.
        setorigin(vr_fst_goal, VR_FST_At(2000));
        vr_fst_goal.solid = SOLID_NOT;
        self = vr_fst_mon;
        self.goalentity = vr_fst_goal;
        self.movetarget = vr_fst_goal;
        self.ideal_yaw = vectoyaw(vr_fst_way);
        self.pausetime = 0;
        self.th_walk();
        self = oldself;
        vr_fst_phase = 1;
        vr_fst_until = time + 3.2;
    }
    else if(vr_fst_phase == 1)
    {
        // Running: after a hidden "enemy" under the floor, far along the way back (it never sees it: no shots).
        setorigin(vr_fst_prey, vr_fst_home - vr_fst_way * 3000 - '0 0 4000');
        self = vr_fst_mon;
        self.enemy = vr_fst_prey;
        self.goalentity = vr_fst_prey;
        self.show_hostile = 0;
        self.th_run();
        self = oldself;
        vr_fst_phase = 2;
        vr_fst_until = time + 3.2;
    }
    else
    {
        remove(vr_fst_mon);
        VR_FST_Next();
    }
};
#else
void() VR_Footstep_TestFrame = {};
#endif
