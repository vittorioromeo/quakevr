// vr_ai_props.qc -- monsters round the physics props in their way (vr_ai_props; docs/vr-port/STEALTH_PLAN.md, "Props
// in the way"). Quake's movetogoal steps straight at its goal and, blocked, tries a few other ways at random: a crate or
// a barrel (a solid prop: SOLID_BBOX, Box3D's) between a monster and the player held it there, stepping in place. Now a
// monster whose step is blocked by a solid prop pushes it (by its own weight against the prop's: Box3D moves a light
// one, not a heavy one: physicspush) and walks round it: to a point beside the prop, on the side nearer its goal with
// room for it, then on to its goal. (Looked for before each step: blocked, movetogoal's random tries still move it, so
// "it didn't move" doesn't tell.) Under the umbrella (vr_ai_enhanced): off, Quake's movetogoal.

.vector vr_detour_at;     // the point beside the prop it walks to
.float vr_detour_until;   // until when (given up after that)
.entity vr_detour_prop;   // and the prop

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

// movetogoal(dist) for ai_run, ai_walk and the stealth AI's walks: round the props in the way.
void(float dist) VR_AI_MoveToGoal =
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
