#ifdef QVR_TESTS
// vr_dm_test 22 (vr_spawnloop_test.qc): a multiplayer game's spawns on a map with no deathmatch start (mpbugs3, his note
// 2026-11-21: Dwell started from Select Campaign in a multiplayer game: "runaway loop error" in SelectSpawnPoint,
// Host_Error). Set before `vr_campaign_native dwell`: on Dwell's start it waits for its player, connects a bot if he is
// alone (a second player's spawn), then (as the second is in): two players spawned and alive (deathmatch: coop's starts,
// no telefrag), SelectSpawnPoint returns. Verdict "spawnloop: N passed, M failed"; FAIL "timed out" after VR_SLT_GIVEUP
// frames on the map.

float VR_SLT_GIVEUP = 900;
float vr_slt_n;    // frames waited on the map
float vr_slt_bot;  // a bot connected by the test

void() VR_SpawnLoopTest_Frame =
{
    float ask = cvar_hget(cvarh_vr_dm_test);
    if(ask == 22)
    {
        if(mapname != "start" || VR_Player() == world)
        {
            return; // (the campaign's start not loaded yet, or its player not in)
        }
        if(vr_slt_n == 0)
        {
            VR_Test_BeginFlags("spawnloop", VR_TEST_LOUD);
        }
        vr_slt_n++;
        float alive = 0;
        float spawned = 0; // (a deathmatch bot may kill the player at once: spawned, not alive)
        entity p = VR_Player();
        while(p)
        {
            if(p.health > 0 && p.deadflag == DEAD_NO) { alive++; }
            spawned++;
            p = VR_NextPlayer(p);
        }
        if(spawned < 2 && vr_slt_n < VR_SLT_GIVEUP)
        {
            if(!vr_slt_bot && vr_slt_n >= 30 && ClientNextAvailable() != -1)
            {
                vr_slt_bot = 1;
                entity oself = self;
                self = VR_Player();
                BotConnect(0, 0, 1); // (a second player spawned here: SelectSpawnPoint again)
                self = oself;
                dprint("spawnloop: a bot connected\n");
            }
            return;
        }
        cvar_set("vr_dm_test", "0");
        for(p = VR_Player(); p; p = VR_NextPlayer(p))
        {
            bprint(sprintf("spawnloop: %s health %g deadflag %g at %v\n", p.netname, p.health, p.deadflag, p.origin));
        }
        VR_Test_Check(vr_slt_n < VR_SLT_GIVEUP && alive >= 2, sprintf("two players spawned and alive on %s (%g of %g; the bot "
            "connected by the test %g; timed out if not)", mapname, alive, spawned, vr_slt_bot));
        VR_Test_Check(deathmatch || coop, sprintf("a multiplayer game (deathmatch %g, coop %g)", deathmatch, coop));
        entity oself2 = self;
        self = VR_Player();
        entity s = SelectSpawnPoint(); // (looped for ever with no deathmatch start)
        self = oself2;
        float dmStarts = find(world, classname, "info_player_deathmatch") != world;
        VR_Test_Check(s != world && s.classname != "", // (deathmatch without its starts: coop's, VR_CoopSpawnPoint)
            sprintf("SelectSpawnPoint returns (%s; deathmatch starts on the map: %g)", s.classname, dmStarts));
        VR_Test_Verdict();
    }
};
#endif
