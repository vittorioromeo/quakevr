// vr_mount.qc -- wall mounts (DYNAMIC_SPAWNS.md, "Prerequisites available"): a thing attached to a wall or a surface
// (a wall torch today; shields, weapons on racks, lamps later) and its one detach rule. A hand's grab takes it off by the
// thing's own handtouch; a shot or a blast by the thing's own damage; an impact by this rule, whatever made the impact:
// a physics object of any origin (thrown by a hand, dropped, flung by a blast, a ragdoll) heavy and fast enough. Never a
// player or a living monster walking past it.
//
// A mounted thing: .vr_mount 1 (the engine reads it: vr_box3d_step.cpp's body hits bring ragdolls' strikes here too),
// .vr_mount_detach its detach (the torch's knock-off), VR_Mount_Touch from its touch.

.float vr_mount;                                // 1 attached to a wall or a surface (VR_Mount_Attach); 0 not
.void(entity xBy, vector xDir) vr_mount_detach; // its detach: self the mounted thing, xBy what struck it, along xDir

float(entity e) VR_Thrown_HitSpeed;               // (vr_carry.qc)
float(entity e) VR_Thrown_Mass;                   // (vr_carry.qc)
float(entity e, entity xTo, vector xVel) VR_Prop_Approach; // (vr_carry.qc)

// `e` attached, `xDetach` its detach.
void(entity e, void(entity xBy, vector xDir) xDetach) VR_Mount_Attach =
{
    e.vr_mount = 1;
    e.vr_mount_detach = xDetach;
};

entity vr_mount_last_by; // what last struck a mounted thing off (the tests read it: vr_systems_test 15)

// `e` off its mount by `xBy` along `xDir` (its detach called once).
void(entity e, entity xBy, vector xDir) VR_Mount_Detach =
{
    if(!e.vr_mount)
    {
        return;
    }
    e.vr_mount = 0;
    vr_mount_last_by = xBy;
    if(cvar_hget(cvarh_developer)) dprint(sprintf("mount: %s (%s) off by %s (%s)\n", e.classname, vtos(e.origin), xBy.classname, vtos(xBy.origin)));
    entity oself = self;
    self = e;
    self.vr_mount_detach(xBy, xDir);
    self = oself;
};

// The impact rule: `xMass` kg meeting it at `xSpeed` units/s towards it detaches it when the speed is a throw's of that
// mass (the curve every throw has: throwweight, as VR_Thrown_HitSpeed) and the mass is vr_prop_impact_min_mass or more.
// `xNeed` > 0: the thing's own throw speed instead (a weapon's w_throwmin).
float(float xMass, float xSpeed, float xNeed) VR_Mount_Strong =
{
    if(xNeed <= 0)
    {
        xNeed = throwweight(1, xMass, 1);
    }
    return xMass >= cvar_hget(cvarh_vr_prop_impact_min_mass) && xSpeed >= xNeed;
};

// A thing's touch on mounted `m` (self's touch hands it `other`): a physics object (not a player, not a living
// monster) heavy and fast enough detaches it. A hand's throw counts as it always did (its mass's throw speed); a thing
// not thrown (dropped, flung by a blast) by the same rule. TRUE: detached.
float(entity m, entity xThing) VR_Mount_Touch =
{
    if(!m.vr_mount || xThing == world || (xThing.flags & FL_CLIENT) || ((xThing.flags & FL_MONSTER) && xThing.health > 0))
    {
        return FALSE;
    }
    float near = VR_Prop_Approach(xThing, m, xThing.velocity);
    float mass = VR_Thrown_Mass(xThing);
    float thrown = xThing.throwhit == QVR_THROWHIT_NEVER_HIT;
    // (In a hand's throw: its throw speed alone, no least mass, as before.)
    if(thrown ? near < VR_Thrown_HitSpeed(xThing) : !VR_Mount_Strong(mass, near, VR_Thrown_HitSpeed(xThing)))
    {
        if(cvar_hget(cvarh_developer)) dprint(sprintf("mount: %s past %s at %g u/s (%.1f kg; a throw of it from %g)\n",
            xThing.classname, m.classname, near, mass, VR_Thrown_HitSpeed(xThing)));
        return FALSE;
    }
    if(cvar_hget(cvarh_developer)) dprint(sprintf("mount: %s struck off by %s at %g u/s (%.1f kg)%s\n", m.classname,
        xThing.classname, near, mass, thrown ? ", thrown" : ""));
    VR_Mount_Detach(m, xThing, normalize(xThing.velocity));
    return TRUE;
};

// A ragdoll's strike on mounted `m` (the engine's body hits: vr_box3d_step.cpp callBodyHits, through
// VR_KdTrig_BodyHit): `xMass` kg going `xVel`. TRUE: detached.
float(entity m, entity xBody, float xMass, vector xVel) VR_Mount_BodyHit =
{
    vector mid = (m.absmin + m.absmax) * 0.5;
    vector to = normalize(mid - (xBody.absmin + xBody.absmax) * 0.5);
    float near = max(0, xVel * to);
    near = max(near, vlen(xVel) * 0.5); // (a body's box is large: a glancing strike at its edge still meets it)
    if(!VR_Mount_Strong(xMass, near, 0))
    {
        if(cvar_hget(cvarh_developer)) dprint(sprintf("mount: %s's body past %s at %g u/s (%.1f kg)\n", xBody.classname,
            m.classname, near, xMass));
        return FALSE;
    }
    if(cvar_hget(cvarh_developer)) dprint(sprintf("mount: %s struck off by %s's body at %g u/s (%.1f kg)\n", m.classname,
        xBody.classname, near, xMass));
    VR_Mount_Detach(m, xBody, normalize(xVel));
    return TRUE;
};
