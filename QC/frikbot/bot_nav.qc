// bot_nav.qc -- QVR: FrikBot's waypoints and the navmesh together (vr_bot_navmesh, off by default; docs/vr-port/NAVMESH.md
// 23, his design of 2026-11-21). FrikBot keeps its goal choice (items, powerups, weapons, health, players), its waypoint
// roaming and routes, its combat; the navmesh (the follower, builtin navbot: vr_navfollow.cpp) walks a bot to a goal when
// (a) a player it saw is within vr_bot_navmesh_range (seen within vr_bot_navmesh_memory s), unless its goal is an item it
// needs more (health or armour under 60 health, a powerup); (b) its goal has no waypoint route (mark_path, bot_path, or no
// waypoint near: BotNav_Look); (c) the waypoint route is over vr_bot_navmesh_detour times the mesh's path and 128 units
// more. Back to the waypoints when the player is lost or far, the goal reached, the mesh has no path, and always in water
// or on a waypoint's special way (a train, a rocket jump, surfacing). With vr_bot_navmesh 0 nothing here runs.

// The goal under the waypoints on its target stack (world: none, or only waypoints).
entity() BotNav_StackGoal =
{
    if(self.target1 && self.target1.classname != "waypoint" && self.target1.classname != "temp_waypoint") { return self.target1; }
    if(self.target2 && self.target2.classname != "waypoint" && self.target2.classname != "temp_waypoint") { return self.target2; }
    if(self.target3 && self.target3.classname != "waypoint" && self.target3.classname != "temp_waypoint") { return self.target3; }
    if(self.target4 && self.target4.classname != "waypoint" && self.target4.classname != "temp_waypoint") { return self.target4; }
    return world;
};

string(float why) BotNav_Why =
{
    if(why == 1) { return "seen"; }
    if(why == 2) { return "no route"; }
    return "shorter";
};

// The player it sees now: its foe (bot_fight_style drops one unseen), or a player it went for (on its stack) in sight.
void() BotNav_NoteSeen =
{
    entity p = world;
    if(self.enemy.classname == "player")
    {
        p = self.enemy;
    }
    else
    {
        entity g = BotNav_StackGoal();
        if(g.classname == "player" && fisible(g))
        {
            p = g;
        }
    }
    if(p && p.health > 0)
    {
        self.vr_botnav_foe = p;
        self.vr_botnav_seen = time;
    }
};

// Each AI frame (BotAI, routing or not): the player in sight noted (vr_bot_navmesh).
void() BotNav_Note =
{
    if(cvar_hget(cvarh_vr_bot_navmesh))
    {
        BotNav_NoteSeen();
    }
};

// A player it saw that it may walk to: alive, seen lately, near enough.
float(entity p) BotNav_FoeOk =
{
    if(!p || p == self || p.classname != "player" || p.health <= 0 || !p.modelindex)
    {
        return FALSE;
    }
    if(time - self.vr_botnav_seen > cvar_hget(cvarh_vr_bot_navmesh_memory))
    {
        return FALSE;
    }
    return (vlen(p.origin - self.origin) <= cvar_hget(cvarh_vr_bot_navmesh_range));
};

// Its goal an item it needs more than the player: a powerup; health or armour while under 60 health.
float(entity g) BotNav_Needs =
{
    if(!(g.flags & FL_ITEM))
    {
        return FALSE;
    }
    if(substring(g.classname, 0, 14) == "item_artifact_")
    {
        return TRUE;
    }
    return (self.health < 60 && (substring(g.classname, 0, 11) == "item_health" || substring(g.classname, 0, 10) == "item_armor"));
};

// Back to the waypoints: its plan dropped, its waypoint route found again from here.
void(string why) BotNav_Stop =
{
    if(!self.vr_botnav)
    {
        return;
    }
    dprint(sprintf("botnav: %s waypoints (%s)\n", self.netname, why));
    navbot(self, world, 2);
    self.vr_botnav = world;
    self.vr_botnav_why = 0;
    ClearMyRoute();
    self.last_way = FindWayPoint(self.current_way);
};

// (b), (c): goal `g` walked by the navmesh when it has a whole path there (and, why 3, the waypoint route of `route` units
// is much longer) -> TRUE: taken (on its target stack).
float(entity g, float why, float route) BotNav_Take =
{
    if(!cvar_hget(cvarh_vr_bot_navmesh) || !g || self.waterlevel >= 2)
    {
        return FALSE;
    }
    vector pr = navbot(self, g, 3);
    if(pr_x != 1)
    {
        return FALSE;
    }
    if(why == 3 && route <= pr_y * cvar_hget(cvarh_vr_bot_navmesh_detour) + 128)
    {
        return FALSE;
    }
    if(!target_onstack(g))
    {
        target_add(g);
    }
    if(self.vr_botnav != g)
    {
        dprint(sprintf("botnav: %s navmesh to %s (%s: path %.0f, route %.0f)\n", self.netname, g.classname,
            BotNav_Why(why), pr_y, route));
    }
    navbot(self, world, 2);
    self.vr_botnav = g;
    self.vr_botnav_why = why;
    self.vr_botnav_left = 100000;
    return TRUE;
};

// (b) with no waypoint near (begin_route failed): the best goal within vr_bot_navmesh_range, seen or not (a player only in
// sight), taken when the mesh reaches it; one it doesn't is left alone 5 s (FrikBot's search time) -> TRUE: taken.
float() BotNav_Look =
{
    if(!cvar_hget(cvarh_vr_bot_navmesh) || self.waterlevel >= 2)
    {
        return FALSE;
    }
    entity best = world;
    float bestp = 1;
    for(entity e = findradius(self.origin, cvar_hget(cvarh_vr_bot_navmesh_range)); e; e = e.chain)
    {
        if(e.classname == "player" && !fisible(e))
        {
            continue;
        }
        float p = priority_for_thing(e);
        if(p > bestp)
        {
            bestp = p;
            best = e;
        }
    }
    if(!best)
    {
        return FALSE;
    }
    if(BotNav_Take(best, 2, 0))
    {
        return TRUE;
    }
    if(best.classname != "player")
    {
        best.search_time = time + 5;
    }
    return FALSE;
};

// Each AI frame (BotAI, before FrikBot's walk): the goal it walks by the navmesh now, its keys toward the next corner ->
// TRUE: walked (FrikBot's frik_movetogoal, bot_path and route search skipped this frame).
float() BotNav_Step =
{
    if(!cvar_hget(cvarh_vr_bot_navmesh))
    {
        if(self.vr_botnav)
        {
            navbot(self, world, 2);
            self.vr_botnav = world;
        }
        return FALSE;
    }
    // (the player in sight noted by BotNav_Note, each AI frame, routing or not)
    entity g = BotNav_StackGoal();
    entity goal = world;
    float why = 0;
    if(BotNav_FoeOk(self.vr_botnav_foe) && !BotNav_Needs(g))
    {
        if(self.vr_botnav_seen != time)
        {
            goal = self.vr_botnav_foe;
            why = 1;
        }
        else if(self.vr_botnav_why == 1)
        {
            // In sight again: FrikBot's walk and fight (its goal, its strafing, its range).
            BotNav_Stop("the player in sight");
            return FALSE;
        }
    }
    if(!goal && self.vr_botnav && self.vr_botnav == g && self.vr_botnav_why != 1)
    {
        goal = g;
        why = self.vr_botnav_why;
    }
    if(goal && (self.waterlevel >= 2 || (self.b_aiflags & (AI_RIDE_TRAIN | AI_SUPER_JUMP | AI_SURFACE))))
    {
        BotNav_Stop("water or a waypoint's way");
        return FALSE;
    }
    if(!goal)
    {
        BotNav_Stop(self.vr_botnav_why == 1 ? "the player lost or far" : "its goal reached or dropped");
        return FALSE;
    }
    if(goal != self.vr_botnav)
    {
        dprint(sprintf("botnav: %s navmesh to %s (%s)\n", self.netname, goal.classname == "player" ? goal.netname :
            goal.classname, BotNav_Why(why)));
        navbot(self, world, 2);
        self.vr_botnav = goal;
        self.vr_botnav_why = why;
        self.vr_botnav_left = 100000;
    }
    vector c = navbot(self, goal, 0);
    vector st = navbot(self, goal, 1);
    if(st_x == 2)
    {
        return FALSE; // (the navmesh for its size building: FrikBot's walk meanwhile)
    }
    if(st_x != 1)
    {
        BotNav_Stop("no navmesh path");
        if(why == 1)
        {
            self.vr_botnav_foe = world;
        }
        else
        {
            bot_lost(goal, FALSE);
        }
        return FALSE;
    }
    self.b_aiflags = self.b_aiflags & AI_DANGER; // (no waypoint's flags: AI_WAIT would hold it still)
    if(st_y & 4)
    {
        self.keys = self.keys & 960;
    }
    else
    {
        vector way = c - self.origin;
        way_z = 0;
        self.keys = (self.keys & 960) + frik_KeysForDir(way);
    }
    if((st_y & 2) && (self.flags & FL_ONGROUND))
    {
        bot_jump();
    }
    if(st_z < self.vr_botnav_left - 8)
    {
        self.vr_botnav_left = st_z;
        self.search_time = time + 5; // (a long walk that gets on is not given up at 5 s)
    }
    if(why != 1)
    {
        bot_check_lost(goal); // (an item reached or taken; FrikBot's other rules)
    }
    return TRUE;
};

// QVR: lift shafts kept clear (vr_shaft, NAVMESH.md 24). A bot keeps out of a raised lift's shaft as the monsters do (its
// moves are a player's, never SV_movestep's soft wall): one in it with no reason there (its enemy in it, or its target1
// when that is a thing: an item, a player it follows) walks out, also when its reason ends; one whose move would take it
// in waits at the edge till the lift comes down, then goes on (boards it). FrikBot's search time gives a goal up after
// 5 s of that. Last in BotAI: its keys as its AI left them (by the waypoints or the navmesh, fighting, its ai flags).
void() BotShaft =
{
    if(!cvar_hget(cvarh_vr_shaft) || self.waterlevel >= 3)
    {
        return; // (swimming; wading counts, off the ground in it too: e1m1's lift comes down into water)
    }
    entity g = self.target1;
    if(g.classname == "waypoint" || g.classname == "temp_waypoint")
    {
        g = world; // (a point on its route, never a reason to stand under a lift)
    }
    makevectors('0 1 0' * self.v_angle_y);
    vector wish = '0 0 0';
    if(self.keys & KEY_MOVEFORWARD) { wish = wish + v_forward; }
    if(self.keys & KEY_MOVEBACK) { wish = wish - v_forward; }
    if(self.keys & KEY_MOVERIGHT) { wish = wish + v_right; }
    if(self.keys & KEY_MOVELEFT) { wish = wish - v_right; }
    float r = navshaftbot(self, g, wish, time < self.vr_shaft_flee);
    if(r == -2)
    {
        self.keys = self.keys & 960; // (all but the view keys: it waits at the edge)
    }
    else if(r >= 0)
    {
        makevectors('0 1 0' * r);
        self.keys = (self.keys & 960) + frik_KeysForDir(v_forward);
    }
};
