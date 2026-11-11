// vr_ragthud_test.qc -- vr_agrunt_test 225, on vrclimb: a living monster down as a ragdoll knocks on what it hits as a
// corpse does (the physics sounds' flesh thuds: vr_physsound.cpp, vr_box3d_step.cpp soundHits; vr_physsound_living;
// ROUND24.md, "Active ragdolls' thuds"). A grunt on the long ledge (its lip at x 96, its top at z 48, a 240-unit drop
// into the trench) shoved off it by two hands from 24 units (toppled over at once: it falls as an active ragdoll), then
// one knocked down on the flat top far from the lip, each watched until it lies still or is back up (8 s: the safety
// net); then both again with vr_physsound_living 0 (its switch: no thuds).
//   sv_random_seed 1;map vrclimb;wait30;god;vr_agrunt_test 225     -QuitOn "test 225 over"
// PASS: the fall knocked as it landed (once its middle was 64 units below the top) and the flat knockdown knocked; no
// trial knocked more than RTH_MOST times in any second (a body tumbling: no rattle); the switch off: none.
// vr_debug_physsound 1 shows each knock ("physsound: ... impact flesh body ...").

#ifdef QVR_TESTS

float RTH_LIP = 96;
float RTH_TOP = 48;
float RTH_MOST = 5;  // knocks in any one second, at most (a body knocks at most every two vr_physsound_interval: 4.2 a s)
float RTH_TRIALS = 6; // the ledge, the flat, the ledge killed by its landing; the ledge and the flat with the switch off;
                      // the ledge while its smash sounds (.vr_rsmash_next held ahead: the smash is its sound, no knock)

float rth_trial, rth_step, rth_t0, rth_z0, rth_k0, rth_land, rth_still, rth_n, rth_keep_chance, rth_keep_living;
float rth_vol;               // the trial's loudest knock
float rth_times[8];          // its last knocks' times (a ring)
float rth_burst;             // its most knocks within a second
vector rth_last;

void() VR_RTH_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(mapname != "vrclimb")
    {
        VR_AGT_Print(sprintf("ragdoll thuds: NOT RUN on %s: map vrclimb", mapname));
        cvar_set("vr_agrunt_test", "0");
        return;
    }
    float ledge = rth_trial != 1 && rth_trial != 4;
    float off = rth_trial == 3 || rth_trial == 4;
    float dies = rth_trial == 2;
    float smashing = rth_trial == 5;
    if(rth_step == 0)
    {
        if(rth_trial >= RTH_TRIALS)
        {
            VR_AGPT_Clear();
            p.flags = p.flags - (p.flags & FL_NOTARGET);
            cvar_set("vr_knockdown_chance", ftos(rth_keep_chance));
            localcmd(sprintf("tune vr_physsound_living %g\n", rth_keep_living)); // (a tuning constant: tune)
            rth_trial = 0; // (another run starts over)
            cvar_set("vr_agrunt_test", "0");
            return;
        }
        if(rth_trial == 0)
        {
            rth_keep_chance = cvar_hget(cvarh_vr_knockdown_chance);
            rth_keep_living = cvar_hget(cvarh_vr_physsound_living);
            cvar_set("vr_knockdown_chance", "100");
        }
        localcmd(sprintf("tune vr_physsound_living %g\n", off ? 0 : rth_keep_living)); // (before the shove: the spawn's 0.4 s)
        p.flags = p.flags | FL_NOTARGET;
        VR_AGPT_EmptyHand(p);
        VR_AGPT_Clear();
        vector at = '0 0 0';
        at_x = RTH_LIP + (ledge ? 24 : 128);
        at_y = 240 + 60 * rth_trial; // (along the ledge)
        at_z = RTH_TOP + 24;
        setorigin(p, at + '48 0 0');
        p.velocity = '0 0 0';
        vr_agt_grunt = g = VR_AGT_Spawn(WID_SHOTGUN, 40);
        setorigin(g, at);
        g.angles_y = 0; // (facing the shover)
        rth_t0 = time;
        rth_step = 1;
        return;
    }
    if(rth_step == 1)
    {
        if(!g.takedamage || time < rth_t0 + 0.4)
        {
            return; // (started)
        }
        if(g.vr_wp_pack)
        {
            remove(g.vr_wp_pack); // (as vr_ledgeshove_test.qc)
            g.vr_wp_pack = world;
        }
        g.health = 2000; // (the fall never kills it: alive all the way down)
        rth_z0 = g.origin_z;
        rth_k0 = physsoundinfo(g, 0);
        rth_land = -1;
        rth_n = 0;
        rth_vol = 0;
        rth_burst = 0;
        rth_still = 0;
        rth_last = g.origin;
        entity oldself = self;
        self = p;
        vr_bash_exhaust_dmg = 0;
        vr_bash_exhaust_push = 0;
        if(ledge)
        {
            VR_Bash_Hit(g, g.origin, '-1 0 0', QVR_HITKIND_SHOVE, -1, 1, VR_Shove_HandsPush(TRUE));
        }
        else if(!VR_Knockdown_Try(g, '1 0 0', QVR_HITKIND_SHOVE, -1, 1))
        {
            VR_Knockdown_Start(g);
        }
        self = oldself;
        if(dies)
        {
            g.health = 1; // (its landing kills it: its death scream as it lands)
        }
        if(smashing)
        {
            g.vr_rsmash_next = time + 20; // (as if a strike of yours smashed it all the way: VR_RagSmash's cooldown)
        }
        rth_t0 = time;
        rth_step = 2;
        return;
    }
    // Watching it: each new knock.
    float k = physsoundinfo(g, 0) - rth_k0;
    if(k > rth_n)
    {
        rth_n = k;
        rth_vol = max(rth_vol, physsoundinfo(g, 2));
        float slot = rth_n - 8 * floor(rth_n / 8);
        rth_times[slot] = time;
        float inSecond = 0;
        float i = 0;
        while(i < 8)
        {
            inSecond = inSecond + ((rth_times[i] > 0 && time - rth_times[i] < 1) ? 1 : 0);
            i = i + 1;
        }
        rth_burst = max(rth_burst, inSecond);
        if(rth_land < 0 && g.origin_z < rth_z0 - 64)
        {
            rth_land = rth_n;
        }
    }
    rth_still = vlen(g.origin - rth_last) < 0.5 ? rth_still + 1 : 0;
    rth_last = g.origin;
    float back = g.health > 0 && time > rth_t0 + 0.5 && g.vr_knockdown != 1;
    float lying = !g.vr_kd_stagger && rth_still >= 72;
    float timedout = time > rth_t0 + 8;
    if(!back && !lying && !timedout)
    {
        return;
    }
    float fell = g.origin_z < rth_z0 - 150;
    string name = ledge ? "off the ledge" : "the flat knockdown";
    name = dies ? strcat(name, ", killed by its landing") : off ? strcat(name, ", Living Bodies 0") :
           smashing ? strcat(name, ", its smash sounding") : name;
    string ending = back ? "back up" : lying ? "lying still" : "timed out";
    VR_AGT_Print(sprintf("%s: %g knocks (the loudest %.2f), %g in its busiest second, %s, %s", name, rth_n, rth_vol,
        rth_burst, ending, g.health > 0 ? "alive" : "dead"));
    if(off)
    {
        VR_AGT_Check(sprintf("%s: Living Bodies 0, no knocks (%g)", ledge ? "the ledge" : "the flat", rth_n), rth_n == 0);
    }
    else if(smashing)
    {
        VR_AGT_Check(sprintf("the ledge, its smash sounding: no knock on top (%g)", rth_n), rth_n == 0 && fell);
    }
    else if(ledge)
    {
        VR_AGT_Check("off the ledge: it fell into the trench", fell);
        if(dies)
        {
            VR_AGT_Check("off the ledge: its landing killed it", g.health <= 0);
        }
        VR_AGT_Check(sprintf("off the ledge: it knocked as it landed (%g of %g knocks below the top)",
            rth_land < 0 ? 0 : rth_n - rth_land + 1, rth_n), rth_land > 0);
        VR_AGT_Check(sprintf("off the ledge: no rattle (%g in a second, at most %g)", rth_burst, RTH_MOST), rth_burst <= RTH_MOST);
    }
    else
    {
        VR_AGT_Check(sprintf("the flat knockdown: it knocked (%g)", rth_n), rth_n > 0);
        VR_AGT_Check(sprintf("the flat knockdown: no rattle (%g in a second, at most %g)", rth_burst, RTH_MOST), rth_burst <= RTH_MOST);
    }
    float i2 = 0;
    while(i2 < 8)
    {
        rth_times[i2] = 0;
        i2 = i2 + 1;
    }
    VR_AGPT_Clear();
    rth_trial = rth_trial + 1;
    rth_step = 0;
};

#endif
