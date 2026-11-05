// vr_arena_nav.qc -- the map's stairs, for walkers whose foe stands on another level (vr_arena_nav).
//
// Quake's movetogoal only heads at the goal and steps at random when blocked, and a walker never steps off a ledge: on
// a raised platform or under one it wanders along the edge until chance finds the stairs (measured in vrarena, 2026-11:
// walking, hardly ever stuck, and rarely down inside 25 s: vr_arena_test 36). A map says where its flights are with
// pairs of info_arena_nav points; a walker whose foe's feet are on another level walks to the best flight's near end, on
// to its far end, then goes on as ever. A map without them behaves as it always did: nothing here runs.

.entity ar_nav_other;   // an info_arena_nav: the other end of its flight
.entity ar_nav_next;    // ... the next point of the map's
.entity ar_nav_at;      // a walker: the point it is heading for
.float ar_nav_far;      // ... 1 when that is the flight's far end: it finishes the flight
.float ar_nav_until;    // ... when it gives that up and looks again

float AR_NAV_LEVEL = 40;    // feet this close to the foe's are on its level: no stairs needed (a 48-high cover is not)
float AR_NAV_NEAR = 56;     // a flight's end this close to a walker's feet in height is on the walker's level
float AR_NAV_AWAY = 120;    // this far off the foe's level, a flight leading away from it will do (a level between)
float AR_NAV_GIVEUP = 12;   // seconds on one leg (knocked about, or blocked) before it looks again

entity ar_nav_first;
float ar_nav_scanned;
float ar_nav_pick_far;  // VR_ARENA_NavPick's answer is the flight's far end: the walker is already on it

/*QUAKED info_arena_nav (.8 .5 0) (-8 -8 0) (8 8 16)
One end of a flight of stairs or a ramp a walking monster can use, on the step or the floor itself. Pairs: the lower end
has "target", the upper end "targetname". A monster whose enemy's feet are on another level walks through the pair that
serves it best. Maps with none: Quake's own chase.
*/
void() info_arena_nav =
{
}

void() VR_ARENA_NavScan =
{
    ar_nav_scanned = 1;
    entity e = find(world, classname, "info_arena_nav");
    while(e)
    {
        if(e.target != "")
        {
            entity top = find(world, targetname, e.target);
            if(top)
            {
                e.ar_nav_other = top;
                top.ar_nav_other = e;
            }
        }
        e.ar_nav_next = ar_nav_first;
        ar_nav_first = e;
        e = find(e, classname, "info_arena_nav");
    }
}

// The near end of the flight that makes the shortest way to the foe; world if no flight serves.
entity(float xFeet, float xGoalFeet, vector xGoal) VR_ARENA_NavPick =
{
    entity best = world;
    float bestCost = 0;
    float off = fabs(xFeet - xGoalFeet);
    entity n = ar_nav_first;
    while(n)
    {
        entity e = n.ar_nav_other;
        if(e && fabs(n.origin_z - xFeet) <= AR_NAV_NEAR && (fabs(e.origin_z - xGoalFeet) < off || off > AR_NAV_AWAY))
        {
            // (Partway up a flight, its feet between the two ends: straight on to the end that serves, not back to the
            // other first. A blow or a chase can leave it on the steps.)
            float partway = xFeet > min(n.origin_z, e.origin_z) + 8 && xFeet < max(n.origin_z, e.origin_z) - 8;
            vector d = (partway ? e.origin : n.origin) - self.origin;
            d_z = 0;
            float cost = vlen(d) + vlen(e.origin - xGoal) + (partway ? 0 : vlen(n.origin - e.origin));
            if(best == world || cost < bestCost)
            {
                best = partway ? e : n;
                bestCost = cost;
                ar_nav_pick_far = partway;
            }
        }
        n = n.ar_nav_next;
    }
    return best;
}

// This map's flights are marked, and in use.
float() VR_ARENA_NavOn =
{
    if(!ar_nav_scanned) { VR_ARENA_NavScan(); }
    return ar_nav_first != world && cvar_hget(cvarh_vr_arena_nav);
}

// The point this walker should head for instead of its foe, this step; world: its foe, as ever.
entity(float xDist) VR_ARENA_NavStep =
{
    if(!VR_ARENA_NavOn()) { return world; }
    entity g = self.goalentity;
    if(g == world || g != self.enemy || !(self.flags & FL_MONSTER) || (self.flags & (FL_FLY | FL_SWIM))) { return world; }

    float feet = self.origin_z + self.mins_z;
    float goalFeet = g.origin_z + g.mins_z;
    entity at = self.ar_nav_at;
    if(at && time > self.ar_nav_until) { at = world; }
    // (On the foe's level: done, unless it is on a flight: halfway down it may already count as level, and heading at
    // the foe from there only walks it along the flight's edge. It finishes the flight.)
    if(fabs(feet - goalFeet) <= AR_NAV_LEVEL && (at == world || !self.ar_nav_far))
    {
        self.ar_nav_at = world;
        return world;
    }
    if(at)
    {
        vector d = at.origin - self.origin;
        d_z = 0;
        if(vlen(d) < 24 + xDist && fabs(feet - at.origin_z) <= 24)
        {
            at = self.ar_nav_far ? world : at.ar_nav_other;
            self.ar_nav_far = 1;
            self.ar_nav_until = time + AR_NAV_GIVEUP;
        }
    }
    if(at == world)
    {
        // (A foe in the air keeps its level until it lands: no new walk for a jump.)
        if(!(g.flags & FL_ONGROUND) || fabs(feet - goalFeet) <= AR_NAV_LEVEL)
        {
            self.ar_nav_at = world;
            return world;
        }
        at = VR_ARENA_NavPick(feet, goalFeet, [g.origin_x, g.origin_y, goalFeet]);
        self.ar_nav_far = ar_nav_pick_far;
        self.ar_nav_until = time + AR_NAV_GIVEUP;
    }
    self.ar_nav_at = at;
    return at;
}
