 /***********************************************
*                                              *
*             FrikBot Waypoints                *
*         "The better than roaming AI"         *
*                                              *
***********************************************/

/*

This program is in the Public Domain. My crack legal
team would like to add:

RYAN "FRIKAC" SMITH IS PROVIDING THIS SOFTWARE "AS IS"
AND MAKES NO WARRANTY, EXPRESS OR IMPLIED, AS TO THE
ACCURACY, CAPABILITY, EFFICIENCY, MERCHANTABILITY, OR
FUNCTIONING OF THIS SOFTWARE AND/OR DOCUMENTATION. IN
NO EVENT WILL RYAN "FRIKAC" SMITH BE LIABLE FOR ANY
GENERAL, CONSEQUENTIAL, INDIRECT, INCIDENTAL,
EXEMPLARY, OR SPECIAL DAMAGES, EVEN IF RYAN "FRIKAC"
SMITH HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES, IRRESPECTIVE OF THE CAUSE OF SUCH DAMAGES.

You accept this software on the condition that you
indemnify and hold harmless Ryan "FrikaC" Smith from
any and all liability or damages to third parties,
including attorney fees, court costs, and other
related costs and expenses, arising out of your use
of this software irrespective of the cause of said
liability.

The export from the United States or the subsequent
reexport of this software is subject to compliance
with United States export control and munitions
control restrictions. You agree that in the event you
seek to export this software, you assume full
responsibility for obtaining all necessary export
licenses and approvals and for assuring compliance
with applicable reexport restrictions.

Any reproduction of this software must contain
this notice in its entirety.

*/



/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Waypoint Linking code

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/


float (entity e1, entity e2) CheckLinked =
{
    if((e1 == e2) || (e2 == world) || (e1 == world))
        return FALSE;
    else if(e1.target1 == e2)
    {
        if(e1.b_aiflags & AI_TELELINK_1)
            return 2;
        else return TRUE;
    }
    else if(e1.target2 == e2)
    {
        if(e1.b_aiflags & AI_TELELINK_2)
            return 2;
        else return TRUE;
    }
    else if(e1.target3 == e2)
    {
        if(e1.b_aiflags & AI_TELELINK_3)
            return 2;
        else return TRUE;
    }
    else if(e1.target4 == e2)
    {
        if(e1.b_aiflags & AI_TELELINK_4)
            return 2;
        else return TRUE;
    }

    else return FALSE;
};


float (entity e1, entity e2) LinkWays =
{
    if((e1 == e2) || (e2 == world) || (e1 == world))
    {
        return FALSE;
    }
    else if(CheckLinked(e1, e2))
    {
        return FALSE; // already linked!!!
    }

    if(e1.target1 == world)
    {
        e1.target1 = e2;
        return TRUE;
    }
    else if(e1.target2 == world)
    {
        e1.target2 = e2;
        return TRUE;
    }
    else if(e1.target3 == world)
    {
        e1.target3 = e2;
        return TRUE;
    }
    else if(e1.target4 == world)
    {
        e1.target4 = e2;
        return TRUE;
    }
    else
    {
        return FALSE;
    }

};
// Link Ways part 2, used only for teleporters

float (entity e1, entity e2) TeleLinkWays =
{
    if((e1 == e2) || (e2 == world) || (e1 == world))
        return FALSE;
    else if(CheckLinked(e1, e2))
        return FALSE; // already linked!!!

    if(e1.target1 == world)
    {
        e1.target1 = e2;
        e1.b_aiflags = e1.b_aiflags | AI_TELELINK_1;
        return TRUE;
    }
    else if(e1.target2 == world)
    {
        e1.target2 = e2;
        e1.b_aiflags = e1.b_aiflags | AI_TELELINK_2;
        return TRUE;
    }
    else if(e1.target3 == world)
    {
        e1.target3 = e2;
        e1.b_aiflags = e1.b_aiflags | AI_TELELINK_3;
            return TRUE;
    }
    else if(e1.target4 == world)
    {
        e1.target4 = e2;
        e1.b_aiflags = e1.b_aiflags | AI_TELELINK_4;
        return TRUE;
    }
    else
        return FALSE;

};

void (entity e1, entity e2) UnlinkWays =
{
    if((e1 == e2) || (e2 == world) || (e1 == world))
        return;

    if(!CheckLinked(e1, e2))
        return;

    if(e1.target1 == e2)
    {
        e1.b_aiflags = e1.b_aiflags - (e1.b_aiflags & AI_TELELINK_1);
        e1.target1 = world;
    }
    if(e1.target2 == e2)
    {
        e1.b_aiflags = e1.b_aiflags - (e1.b_aiflags & AI_TELELINK_2);
        e1.target2 = world;
    }
    if(e1.target3 == e2)
    {
        e1.b_aiflags = e1.b_aiflags - (e1.b_aiflags & AI_TELELINK_3);
        e1.target3 = world;
    }
    if(e1.target4 == e2)
    {
        e1.b_aiflags = e1.b_aiflags - (e1.b_aiflags & AI_TELELINK_4);
        e1.target4 = world;
    }

};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

FindWaypoint

This is used quite a bit, by many different
functions big lag causer

Finds the closest, fisible, waypoint to e

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

entity(entity start) FindWayPoint =
{
    entity t;
    vector org;
    float dst, tdst;
    entity best;

    org = realorigin(self);

    t = way_head;
    if(start != world)
    {
        dst = vlen(start.origin - org);
        best = start;
    }
    else
    {
        dst = 100000;
        best = world;
    }
    while(t)
    {
        // real players cut through ignore types
        if(dst < 20)
            return best;
        if(!(t.b_aiflags & AI_IGNORE_TYPES) || self.ishuman)
        {
            tdst = vlen(t.origin - org);
            if(tdst < dst)
            {
                if(sisible(t))
                {
                    dst = tdst;
                    best = t;
                }
            }
        }
        t = t._next;
    }
    return best;
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Waypoint Spawning Code

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

entity way_foot; // Ugh. Do I need a foot for this or not?

entity(vector org) make_waypoint =
{
    entity point;
    point = spawn();
    point.classname = "waypoint";
    point.solid = SOLID_TRIGGER;
    point.movetype = MOVETYPE_NONE;
    point.items = -1;
    setorigin(point, org);

    setsize(point, VEC_HULL_MIN, VEC_HULL_MAX);
    waypoints = waypoints + 1;
    if(!way_head)
    {
        way_head = point;
        way_foot = point;
    }
    else
    {
        way_foot._next = point;
        point._last = way_foot;
        way_foot = point;
    }

    point.count = waypoints;
    if(waypoint_mode > WM_LOADED) // editor modes
        setmodel(point, "progs/s_bubble.spr");
    return point;
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Dynamic Waypoint spawning and linking. Not
very good all things considered.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() DynamicWaypoint =
{
    entity t;
    float dist = 0, dynlink = 0, dynpoint = 0, editor = 0;

    if(self.teleport_time > self.portal_time)
    {
        if(!(self.flags & FL_WATERJUMP))
        {
            self.dyn_flags = 2;
            if(!self.ishuman)
            {
                bot_lost(self.target1, TRUE);
                self.enemy = world;
            }
        }
        self.portal_time = self.teleport_time;
    }
// stacking everything on waypoint_mode might've been good for the editor,
// but it sucks to beat hell for this code.


// convert waypoint_mode to something more usable..
    if(waypoint_mode > WM_LOADED)
    {
        if(self.ishuman)
        {
            if(waypoint_mode == WM_EDITOR_DYNLINK)
                dynlink = 1;
            else if(waypoint_mode == WM_EDITOR_DYNAMIC)
                dynlink = dynpoint = 1;
            editor = 1;
        }
    }
    else if(waypoint_mode == WM_DYNAMIC)
        dynlink = dynpoint = 1;

// if there's nothing for dynamic to do..
    if(!dynpoint)
    {
        if(!editor)
            return;
    }
// for speed sake, I won't have bots dynamic waypoint in coop
    if(!self.ishuman)
        if(coop)
            return;

// don't waypoint in single player
    if(max_clients < 2)
        return;
// if you're dead
    else if(self.health <= 0)
    {
        if(dynpoint)
        {
            if(self.current_way)
            {
                if(pointcontents(self.origin) < -4)
                {
                    if(self.current_way.b_aiflags & AI_BLIND)
                        self.current_way.b_aiflags = self.current_way.b_aiflags | AI_PRECISION;
                    else
                        self.current_way.b_aiflags = self.current_way.b_aiflags | AI_BLIND;
                }
            }
        }
        self.dyn_dest = '0 0 0';
        self.current_way = world;
        self.dyn_flags = 0;
        return;
    }

// you shouldn't be making waypoints mid air
    if(dynpoint)
    {
        if(!((self.flags & FL_ONGROUND) || self.waterlevel == 3))
        {
            if(self.dyn_flags != 2)
            {
                self.dyn_flags = 1;
            }
            return;
        }
    }
// keep from doing the rest of this every frame
    if(self.dyn_time > time)
        return;
    self.dyn_time = time + 0.2;

// display the links for editor mode
    if(editor)
    {
        if(self.current_way)
        {
            if(self.current_way.target1)
                DeveloperLightning(self.current_way, self.current_way.target1, self.current_way.b_aiflags & AI_TELELINK_1);
            if(self.current_way.target2)
                DeveloperLightning(self.current_way, self.current_way.target2, self.current_way.b_aiflags & AI_TELELINK_2);
            if(self.current_way.target3)
                DeveloperLightning(self.current_way, self.current_way.target3, self.current_way.b_aiflags & AI_TELELINK_3);
            if(self.current_way.target4)
                DeveloperLightning(self.current_way, self.current_way.target4, self.current_way.b_aiflags & AI_TELELINK_4);
        }
        if(self.b_aiflags & AI_HOLD_SELECT)
            return;
    }

    t = FindWayPoint(self.current_way);
    if(t)
    {
        dist = vlen(self.origin - t.origin);
        if(dist < 192)
        {
            if(dist < 64)
            {

                if(t != self.current_way)
                {
                    if(dynlink)
                    {
                        if(!self.dyn_flags)
                        {
                            if(wisible(t, self.current_way))
                                LinkWays(t, self.current_way);
                        }
                        if(self.dyn_flags == 2)
                            TeleLinkWays(self.current_way, t);
                        else if(wisible(t, self.current_way))
                            LinkWays(self.current_way, t);
                    }
                    if(editor)
                    {
                        setmodel(t, "progs/s_light.spr");
                        if(self.current_way)
                            setmodel(self.current_way, "progs/s_bubble.spr");
                    }
                }
                self.current_way = t;
                self.dyn_flags = 0;
            }
            self.dyn_dest = self.origin + self.view_ofs;
            return;
        }
    }

    if(frik_recognize_plat(FALSE))
    {
        if(vlen(trace_ent.velocity) > 0)
        {
            if(self.dyn_plat)
                return;
            self.dyn_plat = TRUE;
            if(!self.dyn_flags)
                self.dyn_flags = 1;
            //bprint("on a plat!!!!!\n");
        }
        else
            self.dyn_plat = FALSE;
    }
    else
        self.dyn_plat = FALSE;

    if(self.dyn_flags == 2)
        self.dyn_dest = self.origin + self.view_ofs;
    else if(self.dyn_dest == '0 0 0')
        self.dyn_dest = self.origin + self.view_ofs;
    if(!dynpoint)
        return;
    t = make_waypoint(self.dyn_dest);

    if(!self.dyn_flags)
    {
        if(wisible(t, self.current_way))
            LinkWays(t, self.current_way);
    }
    if(self.dyn_flags == 2)
        TeleLinkWays(self.current_way, t);
    else if(wisible(t, self.current_way))
        LinkWays(self.current_way, t);

    if(editor)
    {
        setmodel(t, "progs/s_light.spr");
        if(self.current_way)
            setmodel(self.current_way, "progs/s_bubble.spr");
    }
    self.current_way = t;
    self.dyn_flags = 0;

    self.dyn_dest = self.origin + self.view_ofs;

    if(frik_recognize_plat(FALSE))
    {
        if(trace_ent.classname == "door")
            t.b_aiflags = t.b_aiflags | AI_DOORFLAG;
    }
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Waypoint Loading from file

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() ClearAllWays =
{

    entity t, n;
    t = way_head;
    while(t)
    {
        n = t._next;
        remove(t);
        t = n;
    }
    way_head = world;
    way_foot = world;
    waypoints = 0;
};

entity(float num) WaypointForNum =
{
    entity t;
    if(!num)
        return world;

    t = way_head;
    while (t)
    {
        if(t.count == num)
            return t;
        t = t._next;
    }
    return world;
};

void() FixThisWaypoint =
{
    self.enemy.target1 = WaypointForNum(self.enemy.b_pants);
    self.enemy.target2 = WaypointForNum(self.enemy.b_skill);
    self.enemy.target3 = WaypointForNum(self.enemy.b_shirt);
    self.enemy.target4 = WaypointForNum(self.enemy.b_frags);
    self.enemy = self.enemy._next;
    self.nextthink = time;
    if(self.enemy == world)
    {
        remove(self);
        fixer = world;
    }
};

void() FixWaypoints =
{
    if(!fixer)
        fixer = spawn();
    fixer.nextthink = time;
    fixer.think = FixThisWaypoint;
    fixer.enemy = way_head;
};



void(entity what) delete_waypoint =
{
    entity t;

    if(way_head == what)
        way_head = what._next;
    if(way_foot == what)
        way_foot = what._last;
    if(what._last)
        what._last._next = what._next;
    if(what._next)
        what._next._last = what._last;
    waypoints = 0;
    t = way_head;
    while(t)
    {
        t.count = waypoints = waypoints + 1;
        if(CheckLinked(t, what))
            UnlinkWays(t, what);
        t = t._next;
    }
    if(self.current_way == what)
        self.current_way = world;
    remove(what);
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

FindRoute & FindThing used by the pathing code
in bot_ai.qc

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/


entity(string s) FindThing =
{
    entity t;
    float tdst, dst;
    entity best;
    dst = 100000;
    best = world;
    t = find (world, classname, s);
    while (t != world)
    {
        tdst = vlen(((t.absmin + t.absmax) * 0.5) - self.origin);
            if(tdst < dst)
        {
            dst = tdst;
            best = t;
        }
        t = find(t, classname, s);
    }
    return best;
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

FindRoute, this is a key function in the
pathing. The name is a bit misleading, this
code finds the closest waypoint that is part
of a route calculated by the begin_route and
end_route routines This is a definite path to
an object.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

entity(entity lastone) FindRoute =
{
    // kinda like FindWaypoint, only of this bots route though
    entity t, best;
    float dst, tdst, flag;
    flag = ClientBitFlag(self.b_clientno);

    if(lastone)
    {
        if(lastone.target1.b_sound & flag)
            return lastone.target1;
        else if(lastone.target2.b_sound & flag)
            return lastone.target2;
        else if(lastone.target3.b_sound & flag)
            return lastone.target3;
        else if(lastone.target4.b_sound & flag)
            return lastone.target4;
    }

    t = way_head;
    dst = 100000;
    best = world;
    while(t)
    {
        tdst = vlen(t.origin - self.origin);
        if((tdst < dst) && (t.b_sound & flag))
        {
            dst = tdst;
            best = t;
        }
        t = t._next;
    }
    return best;
};
/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Route & path table management

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() ClearRouteTable =
{
    // cleans up route table

    entity t;
    t = way_head;
    while (t)
    {
        t. keys = FALSE;
        t.enemy = world;
        t.items = -1; // not in table
        t = t._next;
    }
};

void() ClearMyRoute =
{
    float flag;
    entity t;

    flag = ClientBitFlag(self.b_clientno);

    t = way_head;
    while (t)
    {
        t.b_sound = t.b_sound - (t.b_sound & flag);
        t = t._next;
    }
};


/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Mark_path

After the route has been found, mark it with
bitflags  so the table can be used for a
different bot.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/


void(entity this) mark_path =
{
    entity t, oself;
    float flag;

    ClearMyRoute();

    oself = self;
    self = this;
    t = FindWayPoint(this.current_way);
    self = oself;
    // FIXME
    // ugh, better way to find players please!!!
    if(this.classname != "player")
        this.current_way = t;

    if(t.enemy == world)
    {
        bot_lost(this, FALSE);
        self.route_failed = TRUE;
        return;
    }

    flag = ClientBitFlag(self.b_clientno);

    while(t)
    {
        if(t.b_sound & flag)
            return;
        if(t == self.last_way)
            return;
        t.b_sound = t.b_sound | flag;
        t = t.enemy;
    }
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

WaypointThink

Calculates the routes. We use thinks to avoid
tripping the runaway loop counter

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void(entity e2, float b_bit) FollowLink =
{
    float dist;

    if(self.b_aiflags & b_bit)
        dist = self.items;
    else
        dist = vlen(self.origin - e2.origin) + self.items;

    // check if this is an RJ link
    if(e2.b_aiflags & AI_SUPER_JUMP)
    {
        if(!bot_can_rj(route_table))
            return;
    }
    if(e2.b_aiflags & AI_DIFFICULT)
        dist = dist + 1000;

    if((dist < e2.items) || (e2.items == -1))
    {
        if(!e2.keys)
            busy_waypoints = busy_waypoints + 1;
        e2.keys = TRUE;
        e2.items = dist;
        e2.think = WaypointThink;
        e2.nextthink = time;
        e2.enemy = self;
    }
};

void() WaypointThink =
{
    entity oself;

    if(self.items == -1)
        return;
    // can you say ugly?
    if(self.b_aiflags & AI_TRACE_TEST)
    {
        if(self.target1)
        {
            traceline(self.origin, self.target1.origin, TRUE, self);
            if(trace_fraction == 1)
                FollowLink(self.target1, AI_TELELINK_1);
        }
        if(self.target2)
        {
            traceline(self.origin, self.target2.origin, TRUE, self);
            if(trace_fraction == 1)
                FollowLink(self.target2, AI_TELELINK_2);
        }
        if(self.target3)
        {
            traceline(self.origin, self.target3.origin, TRUE, self);
            if(trace_fraction == 1)
            FollowLink(self.target3, AI_TELELINK_3);
        }
        if(self.target4)
        {
            traceline(self.origin, self.target4.origin, TRUE, self);
            if(trace_fraction == 1)
                FollowLink(self.target4, AI_TELELINK_4);
        }
     }
    else
    {
        if(self.target1)
            FollowLink(self.target1, AI_TELELINK_1);
        if(self.target2)
            FollowLink(self.target2, AI_TELELINK_2);
        if(self.target3)
            FollowLink(self.target3, AI_TELELINK_3);
        if(self.target4)
            FollowLink(self.target4, AI_TELELINK_4);
    }

    busy_waypoints = busy_waypoints - 1;
    self.keys = FALSE;

    if(busy_waypoints <= 0)
    {
        if(direct_route)
        {
            oself = self;
            self = route_table;
            bot_get_path(self.target1, FALSE);
            self = oself;
            direct_route = FALSE;
        }
    }
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

begin_route and bot_get_path

PLEASE NOTE: bot_get_path replaces the old
calls to begin_route.

Routing isn't done all at once now, but in two
stages, the bot will calc a route *THEN*
choose a target, *THEN* mark a path.

Boy it's confusing.

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

float() begin_route =
{
    if(busy_waypoints > 0)
        return FALSE;

    if(route_table != world)
    {
        if(!route_table.ishuman)
        {
            if(route_table.b_clientno != -1)
                return FALSE;
        }
    }

    route_table = self;
    ClearRouteTable();
    self.last_way = FindWayPoint(self.current_way);

    if(self.last_way != world)
    {
        self.last_way.items = vlen(self.last_way.origin - self.origin);
        self.last_way.nextthink = time;
        self.last_way.think = WaypointThink;
        self.last_way.keys = TRUE;
        busy_waypoints = 1;
        return TRUE;
    }
    else
    {
        route_table = world;
        busy_waypoints = 0;
        return FALSE;
    }
};

void(entity this, float direct) bot_get_path =
{
    if(this == world)
        return;

    if(route_table == self)
    {
        if(busy_waypoints <= 0)
        {
            route_table = world;
            mark_path(this);
        }
        return;
    }
    if(direct)
    {
        if(begin_route())
            direct_route = TRUE;
        else
            bot_lost(this, FALSE);
        return;
    }
};

/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

BSP/QC Waypoint loading

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void() waypoint =
{
    self.solid = SOLID_TRIGGER;
    self.movetype = MOVETYPE_NONE;
    setorigin(self, self.origin);

    setsize(self, VEC_HULL_MIN, VEC_HULL_MAX);
    waypoints = waypoints + 1;
    if(!way_head)
    {
        way_head = self;
        way_foot = self;
    }
    else
    {
        way_foot._next = self;
        self._last = way_foot;
        way_foot = self;
    }

    self.count = waypoints;
    waypoint_mode = WM_LOADED;
    if(self.count == 1)
    {
        localcmd("echo BSP waypoints detected\n");
        self.think = FixWaypoints; // wait until all bsp loaded points are spawned
        self.nextthink = time;
    }
};

void(vector org, vector bit1, float bit4, float flargs) make_way =
{
    entity y;
    waypoint_mode = WM_LOADED;
    y = make_waypoint(org);
    y.b_aiflags = flargs;
    y.b_pants = bit1_x;
    y.b_skill = bit1_y;
    y.b_shirt = bit1_z;
    y.b_frags = bit4;
    if(y.count == 1)
    {
        localcmd("echo QuakeC waypoints detected\n");
        y.think = FixWaypoints; // wait until all qc loaded points are spawned
        y.nextthink = time;
    }
};


/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

Temporary Marker code

-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

void(vector org) SpawnTempWaypoint =
{
    entity tep;

    if(!self.temp_way)
        self.temp_way = tep = spawn();
    else
        tep = self.temp_way;

    tep.classname = "temp_waypoint";
    tep.solid = SOLID_TRIGGER;
    tep.movetype = MOVETYPE_NOCLIP;
    setorigin(tep, org);
    target_add(tep);
    setsize(tep, VEC_HULL_MIN, VEC_HULL_MAX); // FIXME: convert these to numerical
};
