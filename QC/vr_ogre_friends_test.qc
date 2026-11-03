// vr_ogre_friends_test.qc -- vr_agrunt_test 204: ogres mind their friends (vr_ogre_friends.qc). Three ogres 420 ahead of you,
// 140 apart, hunting you (you can't die), each with a friend (an ogre held still behind it, its health kept up) who walks into
// its shot's way, 110 ahead of it, during its wind-up (an attack begins only with a clear sight line: CheckAttack), half its
// attacks. Each kind (id's ogre; MG3's rocket ogre when its data is there) three runs of VR_OGFT_TIME s:
//   careless (vr_ogre_friendly_care 0): the friendly hits (the scene's own: the shots do go into them);
//   careful: at most half the friendly hits per wind-up its friend walked into (those left: its give-up, a blast's edge); it
//   keeps attacking: its tries at least VR_OGFT_TRIES of the careless attacks (half of them held here, by the scene: shots at
//   least VR_OGFT_RATE);
//   it strafed while held (the units printed);
//   always in the way (every attack's wind-up): held, it fires anyway after vr_ogre_friendly_giveup: never idle.
// `agtest: ogre friends <kind> <run>: <h> friendly hits (<d> damage), <s> shots (<r> a minute), <n> held`, the checks, and
// `agtest: test 204 over: <p> passed, <f> failed`.
#ifdef QVR_TESTS
float VR_OGFT_TIME = 30;     // s a run (each ogre attacks every 2 to 4 s)
float VR_OGFT_TRIES = 0.7;   // careful: its tries (attacks that fired, a rocket ogre's volley of 2, + holds) at least this share of the careless attacks
float VR_OGFT_RATE = 0.25;   // ... its shots (half its wind-ups have a friend walk in) at least this share
float vr_ogft_kind;          // 0 id's ogre, 1 the rocket ogre
float vr_ogft_run;           // 0 careless, 1 careful, 2 always in the way
float vr_ogft_until;
float vr_ogft_hits;
float vr_ogft_dmg;
float vr_ogft_shots0;
float vr_ogft_held0;
float vr_ogft_slid;          // units strafed while held
float vr_ogft_crossed;       // wind-ups its friend walked into
float vr_ogft_careless_share; // ... the careless run's hits a time
float vr_ogft_careless_hits;
float vr_ogft_careless_shots;
float vr_ogft_care;          // the setting as it was
entity vr_ogft_ogre[3];
entity vr_ogft_friend[3];
float vr_ogft_winding[3];    // its wind-up under way
float vr_ogft_cross[3];      // ... its friend walks in this time
vector vr_ogft_home[3];      // where it began
vector vr_ogft_was[3];       // ... last frame
vector vr_ogft_at;           // where you stand throughout

string() VR_OGFT_KindName = { return vr_ogft_kind ? "rocket ogre" : "ogre"; };

// An ogre at `at`, facing you.
entity(vector at, float rocket) VR_OGFT_Spawn =
{
    entity was = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(vr_agt_player.origin - at);
    if(rocket)
    {
        monster_ogre_rocket();
    }
    else
    {
        monster_ogre();
    }
    entity o = self;
    self = was;
    return o;
};

void() VR_OGFT_Remove =
{
    float k;
    for(k = 0; k < 3; k++)
    {
        if(vr_ogft_ogre[k]) { remove(vr_ogft_ogre[k]); }
        if(vr_ogft_friend[k]) { remove(vr_ogft_friend[k]); }
        vr_ogft_ogre[k] = world;
        vr_ogft_friend[k] = world;
    }
    entity e = nextent(world);
    while(e)
    {
        entity n = nextent(e);
        if(e.classname == "ogre_grenade" || e.classname == "ogre_missile")
        {
            remove(e);
        }
        e = n;
    }
};

void() VR_OGFT_End =
{
    localcmd(sprintf("tune vr_ogre_friendly_care %g\n", vr_ogft_care));
    entity p = vr_agt_player;
    p.flags = p.flags - (p.flags & FL_GODMODE);
    VR_Test_VerdictPart("test 204 over:");
    cvar_set("vr_agrunt_test", "0");
};

void() VR_OGFT_Frame =
{
    entity p = vr_agt_player;
    float k;
    p.flags = p.flags | FL_GODMODE;
    if(vr_agt_step > 0 || vr_ogft_run > 0 || vr_ogft_kind > 0)
    {
        // (a blast's push never takes you, and so the next run's ogres, somewhere else)
        setorigin(p, vr_ogft_at);
        p.velocity = '0 0 0';
    }
    if(vr_agt_step == 0)
    {
        if(vr_ogft_run == 0 && vr_ogft_kind == 0)
        {
            vr_ogft_care = cvar_hget(cvarh_vr_ogre_friendly_care);
            vr_ogft_at = p.origin;
        }
        if(vr_ogft_kind == 1 && !MG3_RocketOgre_Available())
        {
            VR_AGT_Print("ogre friends: rocket ogre: SKIP (no Dawn of the Machine data)");
            VR_OGFT_End();
            return;
        }
        localcmd(sprintf("tune vr_ogre_friendly_care %g\n", vr_ogft_run == 0 ? 0 : 1));
        makevectors('0 1 0' * p.angles_y);
        vector fw = v_forward;
        vector side = v_right;
        for(k = 0; k < 3; k++)
        {
            vector o = p.origin + fw * 420 + side * ((k - 1) * 140);
            vr_ogft_ogre[k] = VR_OGFT_Spawn(o, vr_ogft_kind);
            vr_ogft_friend[k] = VR_OGFT_Spawn(o + fw * 200, 0);
            vr_ogft_home[k] = o;
            vr_ogft_winding[k] = 0;
            vr_ogft_cross[k] = 0;
        }
        vr_agt_step = 1;
        vr_ogft_until = time + 1; // (they start, on the floor)
        return;
    }
    // The friends held still, always in the way, their health kept up: each drop a hit.
    for(k = 0; k < 3; k++)
    {
        entity f = vr_ogft_friend[k];
        f.nextthink = time + 0.5;
        f.enemy = world;
        f.velocity = '0 0 0';
        if(vr_agt_step == 2 && f.health < 1000)
        {
            vr_ogft_hits = vr_ogft_hits + 1;
            vr_ogft_dmg = vr_ogft_dmg + 1000 - f.health;
        }
        f.health = 1000;
        // Its wind-up (ogre_nail1 to its shot's frame): its friend in the way, or behind it.
        entity og = vr_ogft_ogre[k];
        if(vr_agt_step == 2)
        {
            // (after you alone, alive: a neighbour's stray blast starts no fight among them)
            og.enemy = p;
            og.oldenemy = world;
            og.health = max(og.health, 200);
        }
        // (and for its shot's flight: to ogre_nail7's. A hold sends it to its run: out of the way at once.)
        float winding = ((vr_agt_step == 2) && ((og.think == ogre_nail2) || (og.think == ogre_nail3) || (og.think == ogre_nail4) ||
            (og.think == ogre_nail5) || (og.think == ogre_nail6) || (og.think == ogre_nail7)));
        if(winding && !vr_ogft_winding[k])
        {
            vr_ogft_cross[k] = ((vr_ogft_run == 2) || (random() < 0.5));
            vr_ogft_crossed = vr_ogft_crossed + vr_ogft_cross[k];
        }
        vr_ogft_winding[k] = winding;
        if(og.attack_state == AS_SLIDING && vr_agt_step == 2)
        {
            vr_ogft_slid = vr_ogft_slid + vlen(og.origin - vr_ogft_was[k]);
        }
        vr_ogft_was[k] = og.origin;
        if(vlen(p.origin - og.origin) < 300)
        {
            setorigin(og, vr_ogft_home[k]); // (kept far off: up close it would swing, not shoot)
        }
        vector b = p.origin - og.origin;
        b_z = 0;
        b = normalize(b);
        setorigin(f, og.origin + b * ((winding && vr_ogft_cross[k]) ? 110 : -200));
    }
    if(time < vr_ogft_until)
    {
        return;
    }
    if(vr_agt_step == 1)
    {
        for(k = 0; k < 3; k++)
        {
            entity was = self;
            self = vr_ogft_ogre[k];
            self.enemy = p;
            FoundTarget();
            self = was;
        }
        vr_ogft_hits = 0;
        vr_ogft_dmg = 0;
        vr_ogft_slid = 0;
        vr_ogft_crossed = 0;
        vr_ogft_shots0 = vr_ogre_ff_shots;
        vr_ogft_held0 = vr_ogre_ff_held;
        vr_ogft_until = time + VR_OGFT_TIME;
        vr_agt_step = 2;
        return;
    }
    float shots = vr_ogre_ff_shots - vr_ogft_shots0;
    float held = vr_ogre_ff_held - vr_ogft_held0;
    float share = vr_ogft_hits / max(1, vr_ogft_crossed);
    VR_AGT_Print(sprintf("ogre friends %s %s: %g friendly hits (%g damage; %.2f per the %g wind-ups it walked into)", VR_OGFT_KindName(),
        vr_ogft_run == 0 ? "careless" : vr_ogft_run == 1 ? "careful" : "always in the way", vr_ogft_hits, vr_ogft_dmg, share, vr_ogft_crossed));
    VR_AGT_Print(sprintf("ogre friends %s: %g shots (%.1f a minute an ogre), %g held", VR_OGFT_KindName(), shots, shots * 60 / (VR_OGFT_TIME * 3), held));
    if(vr_ogft_run == 0)
    {
        vr_ogft_careless_hits = vr_ogft_hits;
        vr_ogft_careless_share = share;
        vr_ogft_careless_shots = shots;
        VR_AGT_Check(sprintf("ogre friends %s careless: its shots go into them (%g hits, %g held)", VR_OGFT_KindName(), vr_ogft_hits, held),
            vr_ogft_hits > 0 && held == 0);
    }
    else if(vr_ogft_run == 1)
    {
        VR_AGT_Check(sprintf("ogre friends %s careful: %g hits against %g, %g shots + %g held against %g shots, strafed %.0f units", VR_OGFT_KindName(),
            vr_ogft_hits, vr_ogft_careless_hits, shots, held, vr_ogft_careless_shots, vr_ogft_slid),
            share * 2 <= vr_ogft_careless_share && shots >= vr_ogft_careless_shots * VR_OGFT_RATE &&
            shots / (vr_ogft_kind ? 2 : 1) + held >= (vr_ogft_careless_shots / (vr_ogft_kind ? 2 : 1)) * VR_OGFT_TRIES && held > 0 && vr_ogft_slid > 0);
    }
    else
    {
        VR_AGT_Check(sprintf("ogre friends %s always in the way: still fires (%g shots against %g careless), %g held", VR_OGFT_KindName(),
            shots, vr_ogft_careless_shots, held), shots >= 3 && held > 0);
    }
    VR_OGFT_Remove();
    vr_agt_step = 0;
    vr_ogft_run = vr_ogft_run + 1;
    if(vr_ogft_run > 2)
    {
        vr_ogft_run = 0;
        vr_ogft_kind = vr_ogft_kind + 1;
        if(vr_ogft_kind > 1)
        {
            vr_ogft_kind = 0;
            VR_OGFT_End();
        }
    }
};
#endif
