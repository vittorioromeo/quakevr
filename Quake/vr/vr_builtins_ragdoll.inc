// vr_builtins_ragdoll.inc -- PF_ builtins: prop pushes, ragdolls, knockdowns, decapitation, limb gore.
// Split from vr_builtins.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// A push of a hand on a prop (physicspush(e, at, velocity)): the point `at` of the rigid body `e` gets at least the
// velocity's speed along it (Box3D: an impulse there, so a tall box pushed high tips over; vr_box3d.cpp). False if it is
// not Box3D's. Optional `mass` (kg, 0: none): what pushes has that mass (a hand's, a weapon's: box3d::push); `bounce`
// (0..1) its restitution, `most` (m/s, 0: none) the point's gain at most (a blow on a ragdoll: vr_melee_ragdoll_*).
void PF_physicspush()
{
    const float* at = G_VECTOR(OFS_PARM1);
    const float* v = G_VECTOR(OFS_PARM2);
    const float mass = qcvm->argc > 3 ? G_FLOAT(OFS_PARM3) : 0.f;
    const float bounce = qcvm->argc > 4 ? G_FLOAT(OFS_PARM4) : 0.f;
    const float most = qcvm->argc > 5 ? G_FLOAT(OFS_PARM5) : 0.f;
    G_FLOAT(OFS_RETURN) = box3d::push(G_EDICT(OFS_PARM0), geom::vec(at), geom::vec(v), mass,
                              bounce, most)
                              ? 1.f
                              : 0.f;
}

// entity physicsshot(vector start, vector end, vector vel, float mass): a shot's push (box3d::shot): the first loose prop
// between `start` and `end` pushed where the shot goes in, as physicspush. That prop, or world if none.
void PF_physicsshot()
{
    const float* a = G_VECTOR(OFS_PARM0);
    const float* b = G_VECTOR(OFS_PARM1);
    const float* v = G_VECTOR(OFS_PARM2);
    const int num = box3d::shot(geom::vec(a), geom::vec(b), geom::vec(v),
        G_FLOAT(OFS_PARM3));
    G_INT(OFS_RETURN) = EDICT_TO_PROG(EDICT_NUM(num));
}

// Ragdolls (vr_box3d.cpp, "Ragdolls"; vr_carry.qc, vr_wpnforcegrab.qc, vr_burning.qc). float isragdoll(entity e).
void PF_isragdoll()
{
    G_FLOAT(OFS_RETURN) = box3d::isRagdoll(NUM_FOR_EDICT(G_EDICT(OFS_PARM0))) ? 1.f : 0.f;
}

// Knockdowns (vr_box3d.cpp "Knockdowns"; QC vr_knockdown.qc). float canragdoll(entity e): e can be knocked down as a
// ragdoll (a monster with a rig, ragdolls on).
void PF_canragdoll()
{
    G_FLOAT(OFS_RETURN) = box3d::canRagdoll(G_EDICT(OFS_PARM0)) ? 1.f : 0.f;
}

// float ragdollknockdown(entity e): e (.vr_knockdown 1, touchable, not solid) a ragdoll now, alive.
void PF_ragdollknockdown()
{
    G_FLOAT(OFS_RETURN) = box3d::ragdollKnockdown(G_EDICT(OFS_PARM0)) ? 1.f : 0.f;
}

// float ragdollshovetopple(entity e, vector dir, float strength, float ledge, float reach): e, just knocked down by a
// shove along dir, carried on and turned over about its feet that way (foegrab::shoveTopple: reach the units the shove
// carries a standing one; ledge nonzero: shoved over a ledge, pushed whole and turned as it goes, reach the units to
// past its edge). 0: no ragdoll.
void PF_ragdollshovetopple()
{
    const glm::vec3 dir = progs::vecParm(OFS_PARM1);
    const bool ledge = qcvm->argc > 3 && G_FLOAT(OFS_PARM3) != 0.f;
    const float reach = qcvm->argc > 4 ? G_FLOAT(OFS_PARM4) : 0.f;
    G_FLOAT(OFS_RETURN) = foegrab::shoveTopple(G_EDICT(OFS_PARM0), dir, G_FLOAT(OFS_PARM2), ledge, reach) ? 1.f : 0.f;
}

// float ragdollcanstagger(entity e): whether a shove makes e stagger as a ragdoll (vr_ragdoll_active_stagger, its kind's).
void PF_ragdollcanstagger()
{
    G_FLOAT(OFS_RETURN) = box3d::ragdollCanStagger(G_EDICT(OFS_PARM0)) ? 1.f : 0.f;
}

// float ragdolldrown(entity e, float begin): a drowning monster struggling as a ragdoll (vr_ragdoll_active_drown; QC
// VR_Knockdown_Drown): begin 0, whether e can (its kind driven alive, Swim and Flail on); begin 1, its knockdown's
// ragdoll just made: its living swim begun frantic.
void PF_ragdolldrown()
{
    edict_t* ent = G_EDICT(OFS_PARM0);
    G_FLOAT(OFS_RETURN) = (G_FLOAT(OFS_PARM1) != 0.f ? box3d::ragdollDrowning(ent) : box3d::ragdollCanDrown(ent)) ? 1.f : 0.f;
}

// float ragdollguard(entity e, vector at, float hold): a living monster's ragdoll guarding its head from a blow coming
// from `at` (units) for `hold` s (vr_ragdoll_active_guard; QC VR_Guard_Raise). 0: it can't.
void PF_ragdollguard()
{
    const float* at = G_VECTOR(OFS_PARM1);
    G_FLOAT(OFS_RETURN) = box3d::ragdollGuard(G_EDICT(OFS_PARM0), geom::vec(at), G_FLOAT(OFS_PARM2)) ? 1.f : 0.f;
}

// float ragdollstagger(entity e, vector dir, float push, float balance, float reach): e, just knocked down by a shove
// along dir (its ragdoll made), staggers instead (push: the shove's; balance 0 about to fall .. 1 steady; reach: the
// units the shove carries a standing one). 0: it can't (a knockdown, as before). Optional `recover` (s, a parry's): up
// again then, however it stands; `sure`: it never goes over by itself (level ground); `far`: its travel's limit times
// this (at least 1: a two-hand or running shove's).
void PF_ragdollstagger()
{
    const float* d = G_VECTOR(OFS_PARM1);
    const float recover = qcvm->argc > 5 ? G_FLOAT(OFS_PARM5) : -1.f;
    const bool sure = qcvm->argc > 6 && G_FLOAT(OFS_PARM6) != 0.f;
    const float farther = qcvm->argc > 7 ? G_FLOAT(OFS_PARM7) : 1.f;
    G_FLOAT(OFS_RETURN) = box3d::ragdollStagger(G_EDICT(OFS_PARM0), geom::vec(d), G_FLOAT(OFS_PARM2),
                              G_FLOAT(OFS_PARM3), G_FLOAT(OFS_PARM4), recover > 0.f ? recover : -1.f, sure, farther)
                              ? 1.f
                              : 0.f;
}

// float ragdollstaggered(entity e, vector mins, vector maxs): e's stagger: 1 going, 0 fallen (down now), 2 recovered (up
// where it stands, its box mins..maxs, handed back to its animation: .vr_knockdown 2).
void PF_ragdollstaggered()
{
    const float* lo = G_VECTOR(OFS_PARM1);
    const float* hi = G_VECTOR(OFS_PARM2);
    G_FLOAT(OFS_RETURN) = static_cast<float>(
        box3d::ragdollStaggered(G_EDICT(OFS_PARM0), geom::vec(lo), geom::vec(hi)));
}

// float ragdollgetup(entity e, float frameA, float frameB, vector mins, vector maxs, float range, optional float anywhere):
// e gets up from its ragdoll, starting from frameA or frameB (whichever fits how it lies; -1 none): 0 no room within
// range (it stays down), else 1 or 2 (the frame chosen), e at the place found, turned to it (.vr_knockdown 2). In a
// liquid: where it can stand with its head out (box3d standSpot); anywhere: its box free, floor or none. Its body rising
// first (vr_ragdoll_active_getup): -1 while it does, then 3 or 4 (risen: standing in its get-up's last frame). Held in a
// liquid by a hand: -1 (it stays down until let go of).
void PF_ragdollgetup()
{
    const float* lo = G_VECTOR(OFS_PARM3);
    const float* hi = G_VECTOR(OFS_PARM4);
    G_FLOAT(OFS_RETURN) = static_cast<float>(box3d::ragdollGetUp(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)),
        static_cast<int>(G_FLOAT(OFS_PARM2)), geom::vec(lo), geom::vec(hi), G_FLOAT(OFS_PARM5),
        qcvm->argc > 6 && G_FLOAT(OFS_PARM6) != 0.f));
}

// float ragdollgrab(entity e, entity player, float hand): the hand takes the limb of e's ragdoll it is on.
// [float tug, vector grip, float strength, float force]: the armed monsters' tug-of-war instead (QC ragdolltug,
// vr_armedgrunt_play.qc; the builtins' table is full): tug 1 the hand holds e's limb nearest the weapon's grip (e
// staggering: standing its ground), its arm pulling at `strength`, the hold `force` N at most (1: it does); 2 the hold's
// pull now (N; -1 none); 3 over (let go; its stagger up again `strength` s on).
void PF_ragdollgrab()
{
    const int tug = qcvm->argc > 3 ? static_cast<int>(G_FLOAT(OFS_PARM3)) : 0;
    if(tug > 0)
    {
        const float* g = qcvm->argc > 4 ? G_VECTOR(OFS_PARM4) : nullptr;
        G_FLOAT(OFS_RETURN) = box3d::ragdollTug(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)),
            tug - 1, g ? geom::vec(g) : glm::vec3{0.f}, qcvm->argc > 5 ? G_FLOAT(OFS_PARM5) : 1.f,
            qcvm->argc > 6 ? G_FLOAT(OFS_PARM6) : 0.f);
        return;
    }
    G_FLOAT(OFS_RETURN) =
        box3d::ragdollGrab(G_EDICT(OFS_PARM0), G_EDICT(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2))) ? 1.f : 0.f;
}

// void ragdollrelease(entity player, float hand, vector velocity): the hand lets go (a held limb keeps the velocity).
void PF_ragdollrelease()
{
    const float* v = G_VECTOR(OFS_PARM2);
    box3d::ragdollRelease(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)), geom::vec(v));
}

// float ragdollfling(entity e, vector velocity): e's ragdoll thrown now, every part set going at `velocity` (units/s;
// zero: as it goes); the ragdoll triggers' Thrown Body watches it (QC vr_kdtrig.qc's tests). 0: no ragdoll.
void PF_ragdollfling()
{
    const float* v = G_VECTOR(OFS_PARM1);
    const glm::vec3 vel{v[0], v[1], v[2]};
    G_FLOAT(OFS_RETURN) = box3d::ragdollMarkThrown(G_EDICT(OFS_PARM0), &vel) ? 1.f : 0.f;
}

// float ragdollheld(entity player, float hand): 0 nothing, 1 holding a ragdoll's limb.
void PF_ragdollheld()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(box3d::ragdollHeld(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1))));
}

// float ragdollreach(entity player, float hand): how far (units) the held limb's point is from the hand; -1 none.
void PF_ragdollreach()
{
    G_FLOAT(OFS_RETURN) = box3d::ragdollHandReach(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)));
}

// float ragdollbone(entity e, vector p, [float within]): the limb of e's ragdoll nearest p; -1 if e isn't one (or, with
// `within`, if its surface is farther from p than that: the landing crush's feet, vr_ragimpact.qc). `within`
// QVR_ARMOR_PIECE_AT (-1000; the builtins' table is full): the armed humanoid's armour piece a hit at p strikes
// (vr_agarmor.cpp pieceAt: its bone as drawn; -1 no plate there, -2 no rig).
void PF_ragdollbone()
{
    const float* p = G_VECTOR(OFS_PARM1);
    const float within = qcvm->argc > 2 ? G_FLOAT(OFS_PARM2) : -1.f;
    if(within == -1000.f)
    {
        G_FLOAT(OFS_RETURN) = static_cast<float>(agarmor::pieceAt(G_EDICT(OFS_PARM0), p));
        return;
    }
    if(within == -1001.f) // QVR_ARMOR_CLIENT: the client's word on e's armour (p_x: 0 type, 1 pieces, 2 wear; -1 none)
    {
        G_FLOAT(OFS_RETURN) = static_cast<float>(agarmor::clientState(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), static_cast<int>(p[0])));
        return;
    }
    if(within == -1002.f) // QVR_ARMOR_PIECE_BONES: the rig's bones piece p_x rides (bits, as ragdolllimbs')
    {
        G_FLOAT(OFS_RETURN) = static_cast<float>(agarmor::pieceBones(G_EDICT(OFS_PARM0), static_cast<int>(p[0])));
        return;
    }
    G_FLOAT(OFS_RETURN) =
        static_cast<float>(box3d::ragdollBone(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), geom::vec(p), within));
}

// entity ragdollinbox(vector mins, vector maxs, entity after): the next ragdoll (edict order, after `after`; world: from
// the first) with a part in the world box mins..maxs; world if none. The buzzsaw's blade (rogue_buzzsaw.qc).
void PF_ragdollinbox()
{
    const float* lo = G_VECTOR(OFS_PARM0);
    const float* hi = G_VECTOR(OFS_PARM1);
    const int num = box3d::ragdollInBox(geom::vec(lo), geom::vec(hi),
        NUM_FOR_EDICT(G_EDICT(OFS_PARM2)));
    G_INT(OFS_RETURN) = EDICT_TO_PROG(EDICT_NUM(num)); // (0: world)
}

// float ragdollboxpart(entity e, vector mins, vector maxs, float after): the next part of e's ragdoll (index above
// `after`; -1: from the first) in the world box mins..maxs, -1 none; trace_endpos its point nearest the box's middle.
void PF_ragdollboxpart()
{
    const float* lo = G_VECTOR(OFS_PARM1);
    const float* hi = G_VECTOR(OFS_PARM2);
    glm::vec3 at{0.f};
    const int part = box3d::ragdollPartInBox(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), geom::vec(lo),
        geom::vec(hi), static_cast<int>(G_FLOAT(OFS_PARM3)), at);
    if(part >= 0)
    {
        for(int i = 0; i < 3; i++)
        {
            pr_global_struct->trace_endpos[i] = at[i];
        }
    }
    G_FLOAT(OFS_RETURN) = static_cast<float>(part);
}

// vector ragdollpoint(entity e, float bone, vector p, float toWorld): p from the world into limb `bone`'s space (units),
// or back.
void PF_ragdollpoint()
{
    const float* p = G_VECTOR(OFS_PARM2);
    const glm::vec3 v = box3d::ragdollPoint(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), static_cast<int>(G_FLOAT(OFS_PARM1)),
        geom::vec(p), G_FLOAT(OFS_PARM3) != 0.f);
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// Decapitation (vr_box3d.cpp, "Decapitation"; vr_decap.qc). float ragdolldecap(entity e, vector blade, [float settle]):
// e's ragdoll (made now if it is a dead monster with a rig and none yet) loses its head, launched by the blade's
// velocity; 1 if it did. `settle` (1 if left out): its parts' motion times this at its next step, after this frame's
// knocks (a head pop's: vr_decap_pop_body_speed).
void PF_ragdolldecap()
{
    const float* v = G_VECTOR(OFS_PARM1);
    const float settle = qcvm->argc > 2 ? G_FLOAT(OFS_PARM2) : 1.f;
    G_FLOAT(OFS_RETURN) = box3d::ragdollDecap(G_EDICT(OFS_PARM0), geom::vec(v), settle) ? 1.f : 0.f;
}

// vector ragdollcut(entity e, float what): after the cut, 0 the head's middle, 1 its angles, 2 its velocity, 3 its
// spin (rad/s: .vr_spin); the stump now, 4 the neck, 5 the way out of it.
void PF_ragdollcut()
{
    const int bone = qcvm->argc > 2 ? static_cast<int>(G_FLOAT(OFS_PARM2)) : -1;
    const glm::vec3 v = box3d::ragdollCut(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), static_cast<int>(G_FLOAT(OFS_PARM1)), bone);
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// Limb gore (vr_box3d.cpp, "Limb gore"; vr_limbs.qc). float ragdollcutlimb(entity e, float bone, vector blade, [float
// settle]): as ragdolldecap, the limb at joint `bone` (-1 the head) cut off; ragdollcut's 0-3 the piece after it.
void PF_ragdollcutlimb()
{
    const float* v = G_VECTOR(OFS_PARM2);
    const float settle = qcvm->argc > 3 ? G_FLOAT(OFS_PARM3) : 1.f;
    G_FLOAT(OFS_RETURN) = box3d::ragdollCutLimb(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)), geom::vec(v),
                              settle) ? 1.f : 0.f;
}

// float ragdolllimb(entity e, vector p): the joint a hit at p would cut (box3d::limbAt: the head's bone for the head; -1
// the torso, -2 no rig).
void PF_ragdolllimb()
{
    const float* p = G_VECTOR(OFS_PARM1);
    G_FLOAT(OFS_RETURN) = static_cast<float>(box3d::limbAt(G_EDICT(OFS_PARM0), geom::vec(p)));
}

// float ragdolllimbs(entity e, float what, [float bone]): box3d::limbInfo (0 the limb joints not cut, bits; 1 the
// head's bones; 2 the bones cut off; 3 the head bone, -1 none; 4 what a cut at `bone` takes now; 5 `bone`'s parent; 6 the
// joint of limb `bone` by name: 0 the left shoulder, 1 the right, 2 the left hip, 3 the right, 4-7 their elbow or knee, 0
// none).
void PF_ragdolllimbs()
{
    const int what = static_cast<int>(G_FLOAT(OFS_PARM1));
    const int bone = qcvm->argc > 2 ? static_cast<int>(G_FLOAT(OFS_PARM2)) : -1;
    const uint32_t v = box3d::limbInfo(G_EDICT(OFS_PARM0), what, bone);
    G_FLOAT(OFS_RETURN) = (what == 3 || what == 5) && v == ~0u ? -1.f : static_cast<float>(v);
}

// vector limbpiece(entity e, float bone, float what): the piece a cut at `bone` (-1 the head) takes (box3d::limbPiece): 0
// {kg it weighs (its share of the ragdoll's mass by the kind of limb), the ragdoll's mass, the share}, 1 its cut end in
// its model's space.
// `what` 10 / 11 (the builtins' table being full): armour piece `bone` of an armed humanoid as its own model
// ("<model>#plate<piece>", vr_agarmor.cpp): its origin / its angles now, as drawn; 12 / 13 / 14 a point on its outer face
// near one end / the other / at its middle (15% / 85% / 50% along it: the tests' hits on a plate).
void PF_limbpiece()
{
    const int what = static_cast<int>(G_FLOAT(OFS_PARM2));
    if(what >= 10 && what <= 14)
    {
        agarmor::plateWhere(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)), what - 10, G_VECTOR(OFS_RETURN));
        return;
    }
    const glm::vec3 v = box3d::limbPiece(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)), static_cast<int>(G_FLOAT(OFS_PARM2)));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// string limbmodel(entity e, float bone): the model of e's limb at joint `bone` (vr_limbmodel.cpp), "" none.
void PF_limbmodel()
{
    G_INT(OFS_RETURN) = PR_SetEngineString(box3d::limbModel(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1))));
}

// vector limbplace(entity e, float bone, float what): where e's limb at `bone` is now: 0 its model's origin, 1 its
// angles, 2 its velocity, 3 a point on its own bone's surface (the tests' hit).
void PF_limbplace()
{
    const glm::vec3 v = box3d::limbPlace(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)), static_cast<int>(G_FLOAT(OFS_PARM2)));
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = v.x;
    out[1] = v.y;
    out[2] = v.z;
}

// float ragdollhead(entity e, vector p, [float neck]): 1 if p is on the head of e's ragdoll (its head part, or at the
// neck: within 4 units of it, or `neck` units if more: the melee's vr_decap_neck), 0 not (or headless), -1 e isn't a
// ragdoll.
void PF_ragdollhead()
{
    const float* p = G_VECTOR(OFS_PARM1);
    const float neck = qcvm->argc > 2 ? G_FLOAT(OFS_PARM2) : 0.f;
    G_FLOAT(OFS_RETURN) =
        static_cast<float>(box3d::ragdollHeadAt(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), geom::vec(p), neck));
}

// float physicsdamp(entity e, vector relativeTo, float keep, float keepSpin, float maxSpeed, vector add): the Box3D prop's
// velocity relative to `relativeTo` kept by `keep`, no faster than `maxSpeed` (0: any), `add` added; its spin kept by
// `keepSpin` (box3d::damp: on its body, so a physicspush after it this frame adds to it). False if it is not Box3D's.
void PF_physicsdamp()
{
    const float* rel = G_VECTOR(OFS_PARM1);
    const float* add = G_VECTOR(OFS_PARM5);
    G_FLOAT(OFS_RETURN) = box3d::damp(G_EDICT(OFS_PARM0), geom::vec(rel), G_FLOAT(OFS_PARM2),
                              G_FLOAT(OFS_PARM3), G_FLOAT(OFS_PARM4), geom::vec(add))
                              ? 1.f
                              : 0.f;
}
