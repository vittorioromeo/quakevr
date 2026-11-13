// vr_ai_props.qc -- monsters round the physics props in their way (vr_ai_props; docs/vr-port/STEALTH.md, "Props
// in the way"). Quake's movetogoal steps straight at its goal and, blocked, tries a few other ways at random: a crate or
// a barrel (a solid prop: SOLID_BBOX, Box3D's) between a monster and the player held it there, stepping in place. Now a
// monster whose step is blocked by a solid prop pushes it (by its own weight against the prop's: Box3D moves a light
// one, not a heavy one: physicspush) and walks round it: to a point beside the prop, on the side nearer its goal with
// room for it, then on to its goal. (Looked for before each step: blocked, movetogoal's random tries still move it, so
// "it didn't move" doesn't tell.) Under the umbrella (vr_ai_enhanced): off, Quake's movetogoal.

.vector vr_detour_at;     // the point beside the prop it walks to
.float vr_detour_until;   // until when (given up after that)
.entity vr_detour_prop;   // and the prop
.float vr_nav_melee;      // 1: a melee walker (VR_NAV_Melee), for the navmesh's approach slots (vr_nav.cpp approach)

void() dog_leap1;

float(entity e) VR_AI_IsSolidProp =
{
    return e != world && e.solid == SOLID_BBOX && (e.vr_rigid >= 2 || e.carry_rigid == 2) && !(e.flags & FL_MONSTER);
};

// On its way round: TRUE while it steps towards the point beside the prop; FALSE there (or blocked, or out of time):
// on to its goal.
float(float dist) VR_AI_Detour =
{
    vector d = self.vr_detour_at - self.origin;
    d_z = 0;
    if(vlen(d) < 12 || time > self.vr_detour_until)
    {
        self.vr_detour_until = 0;
        return FALSE;
    }
    self.ideal_yaw = vectoyaw(d);
    ChangeYaw();
    if(walkmove(self.ideal_yaw, min(dist, vlen(d))))
    {
        return TRUE;
    }
    self.vr_detour_until = 0;
    return FALSE;
};

// A point beside `p` on side `s` (+1 left of `dir`, -1 right) for the monster to step to, with room for it; '0 0 0' if
// none.
vector(entity p, vector dir, float s) VR_AI_Beside =
{
    vector c = (p.absmin + p.absmax) * 0.5;
    vector half = (p.absmax - p.absmin) * 0.5;
    half_z = 0;
    float r = vlen(half) + max(self.maxs_x, self.maxs_y) * 1.42 + 12;
    vector side = '-1 0 0' * dir_y + '0 1 0' * dir_x;
    vector at = c + side * (s * r) + dir * 8;
    at_z = self.origin_z;
    tracebox(self.origin, self.mins, self.maxs, at, FALSE, self);
    if(trace_fraction < 1 && vlen(trace_endpos - self.origin) < 0.5 * vlen(at - self.origin))
    {
        return '0 0 0';
    }
    return trace_endpos;
};

// A solid prop in its way to its goal (its box swept a step and a bit straight at it): pushed, and a way round it. TRUE:
// it is on its way round.
float(float dist) VR_AI_PropBlocked =
{
    vector goal = self.goalentity.origin;
    vector dir = goal - self.origin;
    dir_z = 0;
    if(vlen(dir) < 1)
    {
        return FALSE;
    }
    dir = normalize(dir);
    tracebox(self.origin, self.mins, self.maxs, self.origin + dir * min(dist + 32, vlen(goal - self.origin)), FALSE, self);
    entity p = trace_ent;
    if(trace_fraction >= 1 || !VR_AI_IsSolidProp(p))
    {
        return FALSE;
    }
    if(cvar_hget(cvarh_vr_ai_prop_push))
    {
        // (its step's speed, at the prop's side it meets, by its weight: a dog barely moves a crate, an ogre shoves it)
        physicspush(p, trace_endpos + dir * max(self.maxs_x, 8), dir * dist * 10, VR_Grapple_MonsterMass(self));
    }
    vector a = VR_AI_Beside(p, dir, 1);
    vector b = VR_AI_Beside(p, dir, -1);
    vector at = a;
    if(a == '0 0 0' || (b != '0 0 0' && vlen(b - self.origin) + vlen(goal - b) < vlen(a - self.origin) + vlen(goal - a)))
    {
        at = b;
    }
    if(at == '0 0 0' || vlen(at - self.origin) < 4)
    {
        return FALSE;
    }
    self.vr_detour_at = at;
    self.vr_detour_prop = p;
    self.vr_detour_until = time + 1 + vlen(at - self.origin) / 50;
    if(cvar_hget(cvarh_vr_stealth_debug))
    {
        dprint(sprintf("ai_props: %s blocked by %s at %v: round it by %v\n", self.classname, p.classname, p.origin, at));
    }
    return VR_AI_Detour(dist);
};

// movetogoal(dist), round the props in the way.
void(float dist) VR_AI_MoveRound =
{
    if(!cvar_hget(cvarh_vr_ai_props) || !VR_Stealth_On() || !(self.flags & FL_MONSTER) || (self.flags & (FL_FLY | FL_SWIM)) ||
       self.goalentity == world)
    {
        movetogoal(dist);
        return;
    }
    if(self.vr_detour_until > time && VR_AI_Detour(dist))
    {
        return;
    }
    if(dist >= 1 && VR_AI_PropBlocked(dist))
    {
        return;
    }
    movetogoal(dist);
};

// movetogoal(dist) for ai_run, ai_walk and the stealth AI's walks: by way of the map's stairs when the foe is on another
// level (vr_arena_nav.qc), round the props in the way.
// The chase on the map's navmesh (vr_nav, docs/vr-port/NAVMESH.md): a walker after its foe turns toward its path's next
// corner and steps there with Quake's walkmove. FALSE: Quake's chase as before (no path, the foe straight ahead on the
// mesh, or the step blocked: a prop, another monster).
// A walker that fights up close: no missile attack, a dog (its leap is a lunge at arm's length), or an armed humanoid
// holding no ranged weapon (a knight's sword).
float() VR_NAV_Melee =
{
    if(self.vr_ag) { return !VR_WP_CarriesRanged(self); }
    return !self.th_missile || self.th_missile == dog_leap1;
};

// No melee at a foe on another level (feet more than vr_nav_melee_level apart) that a whole navmesh path leads to.
// Quake's melee range is 120 in a straight line, so a dog or a knight on a tower's stair below him on the top was "in
// range": it bit or swung at air, its attack's charge (ai_charge: movetogoal) ran it into the tower's wall, and it stood
// there for good (vr_arena_test 41; NAVMESH.md 15). It walks the path up instead; on his level it fights as before. No
// path there (a ledge it can't climb): as before.
float() VR_NAV_MeleeOff =
{
    if(!cvar_hget(cvarh_vr_nav) || !(self.flags & FL_MONSTER) || (self.flags & (FL_FLY | FL_SWIM)) || self.enemy == world)
    {
        return FALSE;
    }
    float lev = cvar_hget(cvarh_vr_nav_melee_level);
    if(lev <= 0 || fabs((self.enemy.origin_z + self.enemy.mins_z) - (self.origin_z + self.mins_z)) <= lev)
    {
        return FALSE;
    }
    return navreach(self, self.enemy) > 0;
};

float vr_nav_trail_steps; // the navmesh steps taken along a trail's markers (vr_stealth_test 125 counts them)

// The navmesh's step to its enemy, or along his trail: through a teleporter (the stealth AI's marker where he came out,
// vr_stealth.qc VR_Stealth_Trail: its path through the teleporter's link), and under vr_nav_trail to the trail's other
// markers (where it saw him last, where he was heading, the spots it searches at the trail's end: VR_Stealth_TrailNav),
// never to where he is now while it can't see him (NAVMESH.md 20).
float(float dist) VR_NAV_Step =
{
    entity g = self.goalentity;
    float toGate = g != world && g == self.stl_marker && self.stl_trail_gate != world;
    float via = 0;
    if(g != world && g == self.stl_marker && cvar_hget(cvarh_vr_nav_gate_marker)) { via = VR_Stealth_MarkerVia(self); }
    float trail = g != world && g == self.stl_marker && !toGate && !via && VR_Stealth_TrailNav(self);
    if(!cvar_hget(cvarh_vr_nav) || dist <= 0 || g == world || (g != self.enemy && !toGate && !via && !trail) ||
       !(self.flags & FL_MONSTER) || (self.flags & (FL_FLY | FL_SWIM)))
    {
        return FALSE;
    }
    // (no slot on a ring round a trail marker: straight to the spot, VR_Stealth_Trail's "there" is 48 from it)
    self.vr_nav_melee = trail ? FALSE : VR_NAV_Melee();
    float yaw;
    if(via)
    {
        // (a marker just past a gate's face: the path to it carried through the gate, i.e. through the gate's
        // teleporter link, lined up with its opening; the marker itself is carried as the monster crosses)
        vector keep = g.origin;
        setorigin(g, portal_ai_gate(via, 3, keep));
        yaw = navsteer(self, g);
        setorigin(g, keep);
    }
    else
    {
        yaw = navsteer(self, g);
    }
    if(yaw < 0) { return FALSE; }
    if(trail) { vr_nav_trail_steps = vr_nav_trail_steps + 1; }
    float act = navlink(self);
    self.ideal_yaw = yaw;
    ChangeYaw();
    if(act == 2) { return TRUE; } // (waiting for a lift, riding one, or falling off a ledge: standing)
    if(walkmove(yaw, dist)) { return TRUE; }
    if(act == 1)
    {
        // A drop link: at the ledge, Quake's own fall (the floor pulled out from under it: FL_PARTIALGROUND lets the step
        // hang over the edge), pushed on as it goes over so it lands clear of the wall below.
        self.flags = self.flags | FL_PARTIALGROUND;
        if(!walkmove(yaw, dist)) { return FALSE; }
        if(!(self.flags & FL_ONGROUND))
        {
            makevectors('0 1 0' * yaw);
            self.velocity = v_forward * cvar_hget(cvarh_vr_nav_drop_push);
        }
        return TRUE;
    }
    // (Blocked: a little to either side first, as Quake's chase tries the diagonals; which side first at random.)
    float side = cvar_hget(cvarh_vr_nav_sidestep);
    if(side <= 0) { return FALSE; }
    if(random() < 0.5) { side = 0 - side; }
    if(walkmove(yaw + side, dist)) { return TRUE; }
    return walkmove(yaw - side, dist);
};

void(float dist) VR_AI_MoveToGoal =
{
    if(VR_NAV_Step(dist)) { return; }
    if(self.stl_trail_gate != world && self.goalentity == self.stl_marker && self.stl_marker != world)
    {
        // (his trail through a teleporter with no navmesh path there: straight into its trigger, round the props)
        vector keep = self.stl_marker.origin;
        setorigin(self.stl_marker, VR_Stealth_GateMid(self.stl_trail_gate));
        VR_AI_MoveRound(dist);
        setorigin(self.stl_marker, keep);
        return;
    }
    entity nav = VR_ARENA_NavStep(dist);
    if(nav == world)
    {
        VR_AI_MoveRound(dist);
        return;
    }
    entity goal = self.goalentity;
    self.goalentity = nav;
    self.ideal_yaw = vectoyaw(nav.origin - self.origin);
    VR_AI_MoveRound(dist);
    self.goalentity = goal;
};
