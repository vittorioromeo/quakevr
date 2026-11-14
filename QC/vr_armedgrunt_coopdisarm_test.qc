#ifdef QVR_TESTS
// ----------------------------------------------------------------------------
// vr_armedgrunt_coopdisarm_test.qc -- `vr_agrunt_test 215`: a grunt disarmed by shooting its gun can still be killed
// (ROUND25.md, "coopdisarm": in a coop listen game it could not). A shotgun grunt 200 units ahead hunting the player; the
// player's shotgun blasts (FireBulletsImpl, as his) at its gun until it is knocked out; a while later (scratch4 seconds,
// at least 1: its fists, its seek for a weapon), blasts at its body until it dies. scratch3 1: the map's own nearest armed
// grunt (e1m1's: a replacement) brought ahead instead of a spawned one. Checks `agtest: PASS|FAIL coopdisarm ...`; ends on
// `agtest: test 215 over: P passed, F failed`. Run it in single player and in coop (`coop 1; maxplayers 4`, a bot added:
// `impulse 100`; a bot near may kill it first: that passes too).
// ----------------------------------------------------------------------------

float vr_agcd_until;  // the step's give-up time (a safety net: FAIL "timed out")
float vr_agcd_next;   // the next blast's time
float vr_agcd_shots;  // blasts so far in the step
float vr_agcd_h0;     // its health before the kill's first blast

// The clients in the game (the player and any bots).
float() VR_AGCD_Clients =
{
    float n = 0;
    entity c = VR_Player();
    while(c)
    {
        n = n + 1;
        c = VR_NextPlayer(c);
    }
    return n;
};

// The map's nearest living armed humanoid holding a gun, moved 200 units ahead of the player, facing him.
entity() VR_AGCD_MapGrunt =
{
    entity p = vr_agt_player;
    entity best = world;
    float bestDist = 100000;
    entity e = nextent(world);
    while(e)
    {
        if(e.vr_ag && e.health > 0 && e.vr_ag_prop && vlen(e.origin - p.origin) < bestDist)
        {
            best = e;
            bestDist = vlen(e.origin - p.origin);
        }
        e = nextent(e);
    }
    if(!best)
    {
        return world;
    }
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    traceline(p.origin, p.origin + fw * 200, TRUE, p);
    vector at = trace_endpos - fw * 24;
    setorigin(best, at);
    best.angles_y = vectoyaw(p.origin - at);
    return best;
};

// The time between blasts.
float() VR_AGCD_Gap = { return 0.25; };

// One shotgun blast of the player's at `at`, from 96 units off along his eye's line.
void(vector at, vector spread) VR_AGCD_Blast =
{
    entity pl = vr_agt_player;
    vector eye = pl.origin + pl.view_ofs;
    vector dir = normalize(at - eye);
    vector from = at - dir * 96;
    entity oldself = self;
    self = pl;
    vr_pellet_damage = cvar_hget(cvarh_vr_dmg_shotgun);
    VR_Hit_Begin(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
    vr_tracer_hand = cVR_MainHand;
    vr_tracer_from = from;
    FireBulletsImpl(6, dir, spread, from, 2048, FALSE);
    VR_Hit_End();
    vr_pellet_damage = 0;
    self = oldself;
};

void() VR_AGCD_Frame =
{
    entity p = vr_agt_player;
    entity g = vr_agt_grunt;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        if(cvar_hget(cvarh_scratch3) & 1) // (a map's own: its nearest armed grunt, a replacement, brought ahead)
        {
            vr_agt_grunt = VR_AGCD_MapGrunt();
            VR_AGT_Print(sprintf("coopdisarm: the map's %s (replacement %g) with %s", vr_agt_grunt.classname,
                vr_agt_grunt.vr_agr, vr_agt_grunt.vr_ag_prop.netname));
        }
        else
        {
            vr_agt_grunt = VR_AGT_Spawn(WID_SHOTGUN, 200);
        }
        vr_agcd_until = time + 0.6;
        vr_agt_step = 1;
        if(!vr_agt_grunt)
        {
            VR_AGT_Check("coopdisarm: an armed grunt to try", FALSE);
            vr_agt_step = 5;
        }
        return;
    }
    if(vr_agt_step == 1) // (its spawn's last step: walkmonster_start_go)
    {
        if(time < vr_agcd_until) { return; }
        VR_AGT_Hunt(g);
        vr_agcd_h0 = g.health;
        g.health = 1000; // (pellets past its gun into its body don't kill it before its disarm: its health back after)
        vr_agcd_until = time + 15;
        vr_agcd_next = time;
        vr_agcd_shots = 0;
        vr_agt_step = 2;
        return;
    }
    if(vr_agt_step == 2) // its gun shot out
    {
        if(!g.vr_ag_prop || g.health <= 0 || time > vr_agcd_until)
        {
            if(g.health > 0)
            {
                g.health = vr_agcd_h0;
            }
            VR_AGT_Check(sprintf("coopdisarm: its gun shot out (%g blasts, health %g, coop %g, %g clients)", vr_agcd_shots,
                g.health, coop, VR_AGCD_Clients()),!g.vr_ag_prop && g.health > 0);
            vr_agcd_until = time + max(1, cvar_hget(cvarh_scratch4)); // (disarmed a while: seeking a weapon, its fists)
            vr_agt_step = 3;
            vr_agcd_h0 = g.health;
            if(g.vr_ag_prop || g.health <= 0)
            {
                vr_agt_step = 5;
            }
            return;
        }
        if(time >= vr_agcd_next)
        {
            VR_AG_WeaponLine(g.vr_ag_prop);
            VR_AGCD_Blast((vr_ag_wa + vr_ag_wb) * 0.5, '0 0 0'); // (all on its gun: its body spared)
            vr_agcd_shots = vr_agcd_shots + 1;
            vr_agcd_next = time + VR_AGCD_Gap();
        }
        return;
    }
    if(vr_agt_step == 3) // (a while disarmed; scratch4 seconds: its fists, its seek)
    {
        if(g.health != vr_agcd_h0) // (who hurt it meanwhile: a bot's shots, its fall)
        {
            VR_AGT_Print(sprintf("coopdisarm: hurt while disarmed: health %g -> %g by %s (%s)", vr_agcd_h0, g.health,
                damage_attacker.classname, damage_attacker.netname));
            vr_agcd_h0 = g.health;
        }
        if(g.health <= 0)
        {
            vr_agt_step = 4; // (killed meanwhile: it could die)
            return;
        }
        if(time < vr_agcd_until) { return; }
        vr_agcd_h0 = g.health;
        vr_agcd_shots = 0;
        vr_agcd_next = time;
        vr_agcd_until = time + 10;
        vr_agt_step = 4;
        return;
    }
    if(vr_agt_step == 4) // killed by blasts on its body
    {
        if(g.health <= 0 || time > vr_agcd_until)
        {
            VR_AGT_Check(sprintf("coopdisarm: killed after its disarm (%g blasts, health %g -> %g, takedamage %g)",
                vr_agcd_shots, vr_agcd_h0, g.health, g.takedamage), g.health <= 0);
            vr_agt_step = 5;
            return;
        }
        if(time >= vr_agcd_next)
        {
            float h = g.health;
            VR_AGCD_Blast(g.origin + '0 0 8', '0.01 0.01 0');
            vr_agcd_shots = vr_agcd_shots + 1;
            if(vr_agcd_shots <= 3)
            {
                VR_AGT_Print(sprintf("coopdisarm: blast %g: health %g -> %g", vr_agcd_shots, h, g.health));
            }
            vr_agcd_next = time + VR_AGCD_Gap();
        }
        return;
    }
    // (step 5) the end
    vr_agt_grunt = world;
    cvar_set("vr_agrunt_test", "0");
};
#endif
