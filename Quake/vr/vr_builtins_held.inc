// vr_builtins_held.inc -- PF_ builtins: the motion recorder, weapon hotspots, magnetic holsters, debris and crates.
// Split from vr_builtins.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// ----------------------------------------------------------------------------
// The motion recorder (vr_motion.cpp; QC vr_motion.qc): kept only while a take is recorded or played.

// void(string kind, string sub, float hand, float value, vector at, entity targ, string detail) motionevent
void PF_motionevent()
{
    motion::qcEvent(G_STRING(OFS_PARM0), G_STRING(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)), G_FLOAT(OFS_PARM3),
        G_VECTOR(OFS_PARM4), G_EDICT(OFS_PARM5), G_STRING(OFS_PARM6));
}

// void(float hand, vector at, string name) motionpoint
void PF_motionpoint()
{
    motion::qcPoint(static_cast<int>(G_FLOAT(OFS_PARM0)), G_VECTOR(OFS_PARM1), G_STRING(OFS_PARM2));
}

// void(string key, vector value) motionvalue
void PF_motionvalue()
{
    motion::qcValue(G_STRING(OFS_PARM0), G_VECTOR(OFS_PARM1));
}

// void(entity player, string kind, float hand, string detail) animevent: the animation recorder's (vr_animrec.cpp; QC
// VR_Anim_Event), the local player's alone, kept only while a recording runs.
void PF_animevent()
{
    if(G_EDICT(OFS_PARM0) != progs::localPlayer())
    {
        return;
    }
    animrec::event(G_STRING(OFS_PARM1), static_cast<int>(G_FLOAT(OFS_PARM2)), G_STRING(OFS_PARM3));
}

// Round 21: the weapons' hotspots (where the other hand may hold them; vr_weapons.hpp), as the local player's view
// draws them this frame. vector(entity player, float hand, float index) weaponhotspot: hotspot `index` (0..3) of the
// weapon in `hand` (cVR_MainHand, cVR_OffHand): a grip's point, a blade grip's middle, in the world ('0 0 0' if none);
// float(entity player, float hand, float index, float what) weaponhotspotinfo: its type (what 0: 0 none, 1 grip,
// 2 blade), bias in units (1), a blade's share of the way from the hand to the tip (2). Only the local player's (the
// server's first client, as its view draws it); others' are none.
[[nodiscard]] view::WeaponHotspot localHotspot()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    if(!sv.active || cls.state != ca_connected || NUM_FOR_EDICT(player) != 1)
    {
        return {};
    }
    return view::weaponHotspot(static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1, static_cast<int>(G_FLOAT(OFS_PARM2)));
}

void PF_weaponhotspot()
{
    const view::WeaponHotspot h = localHotspot();
    float* out = G_VECTOR(OFS_RETURN);
    out[0] = h.pos.x;
    out[1] = h.pos.y;
    out[2] = h.pos.z;
}

void PF_weaponhotspotinfo()
{
    const view::WeaponHotspot h = localHotspot();
    const int what = static_cast<int>(G_FLOAT(OFS_PARM3));
    G_FLOAT(OFS_RETURN) = what == 0 ? static_cast<float>(h.type) : what == 1 ? h.bias : h.share;
}

// float(entity player, float hand, entity weapon) weapongrabspot: the hotspot (index + 1; 0 none: its handle) by which
// `player`'s empty `hand` takes `weapon` lying about (vr_weapon_grab_hotspots: the local player's view found the hand
// nearer that grip or blade hotspot than the handle; vr_twohand.cpp groundSpot). Others' are 0.
void PF_weapongrabspot()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    edict_t* weapon = G_EDICT(OFS_PARM2);
    const int hand = static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1;
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    G_FLOAT(OFS_RETURN) = local && (vr_weapon_grab_hotspots.value || vr_weapon_grab_anywhere.value)
                              ? static_cast<float>(twohand::groundSpot(hand, NUM_FOR_EDICT(weapon)))
                              : 0.f;
}

// float(entity player, float hand) weaponanygrip: how `player`'s empty `hand` holds the weapon in its other hand: 0 not,
// 1 by a hotspot, 2 anywhere on it (vr_weapon_grab_anywhere: a free grip, or both hands on a carried weapon; or a moment
// ago). QC VRTryHandOff: letting go of the other hand leaves the weapon carried by this one where it holds it. The local
// player's only (others' are 0).
// float embedstick(entity w, entity host, vector at, vector vel, float hand): axestick::stickHeld.
void PF_embedstick()
{
    edict_t* w = G_EDICT(OFS_PARM0);
    edict_t* host = G_EDICT(OFS_PARM1);
    const float* at = G_VECTOR(OFS_PARM2);
    const float* vel = G_VECTOR(OFS_PARM3);
    const glm::vec3 place{at[0], at[1], at[2]};
    const glm::vec3 going{vel[0], vel[1], vel[2]};
    const int hand = static_cast<int>(G_FLOAT(OFS_PARM4));
    G_FLOAT(OFS_RETURN) = static_cast<float>(axestick::stickHeld(w, host, place, going, hand));
}

// float embedproppull(entity prop, vector at, vector target, vector targetVel, float force, float hertz, float lift): a hand's
// blade in a wooden crate or barrel (QC vr_embedhold.qc VR_Embed_PropSpring) pulls its point `at` towards `target` going at
// `targetVel` (units, units/s): a critically damped spring of `hertz` on its mass, its pull level (or down) `force` N at most,
// up `lift` times its weight at most, through this frame's step
// (box3d::climbPull: a force at the point, so a crate pulled high tips). The pull now, N; -1 not a solid Box3D prop.
void PF_embedproppull()
{
    edict_t* prop = G_EDICT(OFS_PARM0);
    const glm::vec3 at = geom::vec(G_VECTOR(OFS_PARM1));
    const glm::vec3 target = geom::vec(G_VECTOR(OFS_PARM2));
    const glm::vec3 targetVel = geom::vec(G_VECTOR(OFS_PARM3));
    const float most = za::max(0.f, G_FLOAT(OFS_PARM4));
    const float hertz = za::max(0.01f, G_FLOAT(OFS_PARM5));
    const float lift = za::max(0.f, G_FLOAT(OFS_PARM6));
    box3d::ClimbBody body;
    const int num = NUM_FOR_EDICT(prop);
    if(prop == qcvm->edicts || prop->free || !box3d::climbBody(num, body) || body.fixed)
    {
        G_FLOAT(OFS_RETURN) = -1.f;
        return;
    }
    const float m2u = units::metresToUnits();
    const float w = 2.f * glm::pi<float>() * hertz;
    // (m/s^2: the spring and its damping, critical)
    const glm::vec3 accel = (w * w * (target - at) + 2.f * w * (targetVel - body.pointVelocity(at))) / m2u;
    glm::vec3 force = accel * za::max(body.mass, 1.f);
    // Level (and down) at most `most`; up at most `lift` times its weight: a crate dragged, not hoisted on one blade.
    glm::vec3 level{force.x, force.y, 0.f};
    const float n = glm::length(level);
    if(n > most)
    {
        level *= most / za::max(n, 1e-6f);
    }
    const float weight = za::max(body.mass, 1.f) * za::max(sv_gravity.value / m2u, 0.f); // N (Box3D's gravity is Quake's)
    force = glm::vec3{level.x, level.y, force.z > 0.f ? za::min(force.z, lift * weight) : za::max(force.z, -most)};
    const float applied = glm::length(force);
    if(applied > 1e-3f)
    {
        box3d::propPull(num, at, force);
    }
    G_FLOAT(OFS_RETURN) = applied;
}

// float(entity prop, entity player, float hand, vector handle, vector wound, vector shift, float force, float lift) embedprophold:
// box3d::propBladeHold (`lift`: times the prop's weight); `force` under 0: that hand's hold is over.
void PF_embedprophold()
{
    edict_t* prop = G_EDICT(OFS_PARM0);
    edict_t* player = G_EDICT(OFS_PARM1);
    const int hand = static_cast<int>(G_FLOAT(OFS_PARM2)) == 0 ? 0 : 1;
    const float force = G_FLOAT(OFS_PARM6);
    G_FLOAT(OFS_RETURN) = -1.f;
    if(force < 0.f)
    {
        box3d::propBladeRelease(NUM_FOR_EDICT(player), hand);
        return;
    }
    box3d::ClimbBody body;
    const int num = NUM_FOR_EDICT(prop);
    if(prop == qcvm->edicts || prop->free || !box3d::climbBody(num, body) || body.fixed)
    {
        return;
    }
    const float weight = za::max(body.mass, 1.f) * za::max(sv_gravity.value / units::metresToUnits(), 0.f); // N
    G_FLOAT(OFS_RETURN) = box3d::propBladeHold(num, player, hand, geom::vec(G_VECTOR(OFS_PARM3)), geom::vec(G_VECTOR(OFS_PARM4)),
        geom::vec(G_VECTOR(OFS_PARM5)), force, za::max(G_FLOAT(OFS_PARM7), 0.f) * weight);
}

void PF_weaponanygrip()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    const int hand = static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1;
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    G_FLOAT(OFS_RETURN) = local ? static_cast<float>(twohand::helpKind(hand)) : 0.f;
}

// vector(entity player, float hand, float what) weapondrawnpose: the weapon `player`'s `hand` carries off its handle (the
// hand-off, a hotspot, anywhere), as the local player's view draws it: what 0 its handle's place, 1 its angles as a thrown
// weapon is given them (MakeThrown negates the pitch: then drawn as it was), 2 '1 0 0' if known ('0 0 0': not carried, or another player),
// 3 its tip (the melee's line: QC VR_Melee_Line); 4 its handle and 5 its tip in the frame of the hand carrying it as drawn
// (forward, right, up: QC places them on the hand of the move it runs, VR_Melee_FreeLine). QC: thrown from where it is drawn.
// weapondrawnpose(e, hand, 6): the muzzle of the weapon model `e` itself is drawn with, at its origin and angles, as a
// weapon lying there or held is drawn (its Weapon Offsets' model transform, its Muzzle anchor and offset, at its first
// frame); e's origin if its model is no weapon's (an armed grunt's held weapon, QC vr_armedgrunt.qc: its shots' start).
static void weaponModelMuzzle(edict_t* e, float* r)
{
    for(int i = 0; i < 3; i++)
    {
        r[i] = e->v.origin[i];
    }
    const int index = static_cast<int>(e->v.modelindex);
    qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const int slot = model && model->type == mod_alias ? weapons::slotForModel(model) : -1;
    if(slot < 0 || slot == weapons::fistSlot())
    {
        return;
    }
    entity_t ent{};
    ent.model = model;
    VectorCopy(e->v.origin, ent.origin);
    VectorCopy(e->v.angles, ent.angles);
    const glm::vec3 muzzle = view::anchorPosition(ent, false, 1.f, weapons::anchor(slot, weapons::Key::MuzzleAnchor),
        weapons::vec(slot, weapons::Key::MuzzleOffsetX, weapons::Key::MuzzleOffsetY, weapons::Key::MuzzleOffsetZ));
    for(int i = 0; i < 3; i++)
    {
        r[i] = muzzle[i];
    }
}

void PF_weapondrawnpose()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    const int hand = static_cast<int>(G_FLOAT(OFS_PARM1)) == 0 ? 0 : 1;
    const int what = static_cast<int>(G_FLOAT(OFS_PARM2));
    if(what == 6)
    {
        weaponModelMuzzle(player, G_VECTOR(OFS_RETURN));
        return;
    }
    if(what == 21 || what == 22)
    {
        // weapondrawnpose(p, 0, 21): an armed humanoid's held weapon `p` as the arms drew it lately (this listen server's
        // own view): its drawn forward's turn off the server's (world; the axis times degrees); 22 its drawn origin less
        // the server's. '0 0 0' (both): not drawn lately (a dedicated server, out of view). vr_agrunt_shot_drawn.
        glm::vec3 turn{0.f}, off{0.f};
        const bool local = sv.active && cls.state == ca_connected;
        if(!local || !hitreact::drawnWeaponTurn(NUM_FOR_EDICT(player), turn, off))
        {
            turn = off = glm::vec3{0.f};
        }
        const glm::vec3 out = what == 21 ? turn : off;
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(what >= 30 && what <= 32)
    {
        // weapondrawnpose(p, 0, 30 / 31 / 32): an armed humanoid's held weapon `p` as drawn lately (this listen server's own
        // view, alive or on its ragdoll's hand: hitreact::drawnWeapon): its origin, its forward, its up (world); '0 0 0'
        // (all): not drawn lately. QC VR_AG_DrawnFrame: the disarm volumes, a ragdoll's weapon.
        glm::vec3 at{0.f}, fw{0.f}, up{0.f};
        const bool local = sv.active && cls.state == ca_connected;
        if(!local || !hitreact::drawnWeapon(NUM_FOR_EDICT(player), at, fw, up))
        {
            at = fw = up = glm::vec3{0.f};
        }
        progs::returnVec(what == 30 ? at : what == 31 ? fw : up);
        return;
    }
    if(what >= 33 && what <= 35)
    {
        // weapondrawnpose(p, 0, 33 / 34 / 35): weapon `p`'s drawn model along its long axis (held::drawnExtent), in its own
        // axes (forward, left, up) from its origin: 33 its back end, 34 its front end, 35 '<its most off that line>
        // <its length> 0'; '0 0 0' (all): no drawn surface. QC VR_AG_WeaponLine: its disarm volumes the weapon's own size.
        glm::vec3 back{0.f}, front{0.f};
        float radius = 0.f;
        if(!held::drawnExtent(player, back, front, radius))
        {
            back = front = glm::vec3{0.f};
            radius = 0.f;
        }
        progs::returnVec(what == 33 ? back : what == 34 ? front : glm::vec3{radius, glm::distance(front, back), 0.f});
        return;
    }
    if(what == 36)
    {
        // weapondrawnpose(g, side, 36): armed humanoid `g`'s ragdoll's hand bone holding its weapon on `side` (0 the main
        // weapon's, 1 the left one's: as ragdollProps draws it, agarms::ragdollGrip) as '<bone> 0 0'; -1 none. QC
        // VR_AG_RagProp: the limb its weapon rides (.vr_ag_rb), not the limb nearest it as the ragdoll began (a two-handed
        // weapon's support hand: the off hand, his rocket launcher note).
        rigpose::Rigid grip;
        int bone = -1;
        const int mi = static_cast<int>(player->v.modelindex);
        const qmodel_t* model = mi > 0 && mi < MAX_MODELS ? sv.models[mi] : nullptr;
        if(!model || !agarms::ragdollGrip(model, hand, grip, bone))
        {
            bone = -1;
        }
        progs::returnVec({static_cast<float>(bone), 0.f, 0.f});
        return;
    }
    if(what == 39)
    {
        // weapondrawnpose(g, hand, 39): armed humanoid `g`'s ragdoll's arm held by your `hand` (QC's) in a tug, as drawn
        // now (ragdoll::drawnTugStretch: vr_agrunt_tug_ik_stretch on the physical tug's drawn pose) as '<factor> <the held
        // point's distance from your hand> <the same without the stretch>' (units); '0 0 0' no ragdoll. QC test 19.
        float k = 0.f, gap = 0.f, gap0 = 0.f;
        const bool local = sv.active && cls.state == ca_connected;
        if(!local || !ragdoll::drawnTugStretch(NUM_FOR_EDICT(player), hand, k, gap, gap0))
        {
            k = gap = gap0 = 0.f;
        }
        progs::returnVec({k, gap, gap0});
        return;
    }
    if(what == 43)
    {
        // weapondrawnpose(w, hand, 43): the reverse grip (ROUND25.md "revgrip"; QC VR_RevGrip_Choose): how this listen server's own
        // `hand` (QC's) takes the loose weapon `w` now: '<1 reversed, 0 upright> <1 it may be held reversed, else 0> <the cosine
        // of its blade's way and the thumb's side's>' (view::reverseGripChoice).
        const int index = static_cast<int>(player->v.modelindex);
        entity_t ent{};
        ent.model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
        VectorCopy(player->v.origin, ent.origin);
        VectorCopy(player->v.angles, ent.angles);
        float cosine = 1.f;
        const int choice = sv.active && cls.state == ca_connected ? view::reverseGripChoice(ent, hand, cosine) : -1;
        progs::returnVec({choice > 0 ? 1.f : 0.f, choice >= 0 ? 1.f : 0.f, cosine});
        return;
    }
    if(what == 44 || what == 45)
    {
        // weapondrawnpose(player, hand, 44 / 45): this listen server's own `hand`'s weapon as drawn last, in the hand's frame
        // (tests: QC vr_revgrip_test.qc): 44 its forward axis, 45 its up axis; '0 0 0' none. (view::heldWeaponAxes)
        glm::vec3 forward{0.f}, up{0.f};
        float flip = 0.f;
        const bool drawn = sv.active && cls.state == ca_connected && view::heldWeaponAxes(hand, forward, up, flip);
        progs::returnVec(!drawn ? glm::vec3{0.f} : what == 44 ? forward : up);
        return;
    }
    if(what == 46)
    {
        // weapondrawnpose(player, hand, 46): '<its share of the reverse grip as drawn (0..1: tests)> <1: the weapon held may be
        // held reversed (QC VR_RevGrip_HandFrame: its eject button flips it)> 0'.
        glm::vec3 forward{0.f}, up{0.f};
        float flip = 0.f;
        const bool local = sv.active && cls.state == ca_connected;
        (void)(local && view::heldWeaponAxes(hand, forward, up, flip));
        // (z: 1 in flat play too, the flat holster menu's FLIP GRIP button's: view::heldGripFlippable)
        progs::returnVec({flip, local && view::heldMayReverse(hand) ? 1.f : 0.f, local && view::heldGripFlippable(hand) ? 1.f : 0.f});
        return;
    }
    if(what == 40)
    {
        // weapondrawnpose(player, hand, 40): this listen server's own `hand` (QC's) as drawn against the models
        // (vr_modelcollide.cpp) as '<the live monster it went into (Through; 0 none)> <how far it is drawn from where it is
        // tracked (units)> 0'. QC vr_wornpack_test 4.
        const bool local = sv.active && cls.state == ca_connected && hand >= 0 && hand <= 1;
        progs::returnVec({local ? static_cast<float>(modelcollide::throughBody(hand)) : 0.f,
            local ? glm::length(modelcollide::drawnOffset(hand)) : 0.f, 0.f});
        return;
    }
    if(what == 37 || what == 38 || what == 41 || what == 42)
    {
        // weapondrawnpose(g, side, 37 / 38): armed humanoid `g`'s forearm on `side` (0 right: its skin's forearm,
        // agarms::armPart 1, the cyan hull's; was the ragdoll rig's parent of the hand bone: on a rig with no hand bone
        // the upper arm), the middle of its vertices: 37 as its body is drawn (drawnpose::armedMesh: the shots' and
        // blows' body), 38 as its animation frame has them (hitmodel::vertexPoint, the frame's); '0 0 0' none (not drawn
        // lately, no skin). 41 / 42: as 37, its hand (armPart 0) / its upper arm's top (armPart 2). QC test 79
        // (vr_stalepose_test.qc), 16.
        glm::vec3 sum{0.f};
        int n = 0;
        const int mi = static_cast<int>(player->v.modelindex);
        const qmodel_t* model = mi > 0 && mi < MAX_MODELS ? sv.models[mi] : nullptr;
        const za::Vector<int>* fore = nullptr;
        za::Vector<glm::vec3> drawn; // (a test's query: made each call)
        const int part = what == 41 ? 0 : what == 42 ? 2 : 1;
        if(model && agarms::armPart(model, hand, part, fore) && (what == 38 || drawnpose::armedMesh(player, drawn, nullptr)))
        {
            for(const int v : *fore)
            {
                glm::vec3 p;
                if((what != 38 && (v < 0 || static_cast<za::SizeT>(v) >= drawn.size())) ||
                   (what == 38 && !hitmodel::vertexPoint(player, v, p)))
                {
                    continue;
                }
                sum += what != 38 ? drawn[static_cast<za::SizeT>(v)] : p;
                n++;
            }
        }
        progs::returnVec(n > 0 ? sum / static_cast<float>(n) : glm::vec3{0.f});
        return;
    }
    if(what == 29)
    {
        // weapondrawnpose(e, side, 29): armed humanoid `e`'s drawn hand `side` (0 right) less its head's middle (model
        // space: x forward, y left, z up), as this listen server's own view drew it this frame or the last; '0 0 0' not
        // drawn lately (agarms::drawnHandFromHead; QC test 90, the fists' guard as the Pose Studio shows it).
        glm::vec3 rel{0.f};
        int frame = -1;
        const bool local = sv.active && cls.state == ca_connected;
        if(!local || !agarms::drawnHandFromHead(NUM_FOR_EDICT(player), hand, rel, frame) || host_framecount - frame > 2)
        {
            rel = glm::vec3{0.f};
        }
        progs::returnVec(rel);
        return;
    }
    if(what == 20)
    {
        // weapondrawnpose(e, 0, 20): '<e's entity number> 0 0' (the glitch finder's subject: vr_agglitch_ent).
        progs::returnVec({static_cast<float>(NUM_FOR_EDICT(player)), 0.f, 0.f});
        return;
    }
    if(what == 23)
    {
        // weapondrawnpose(e, +-(code + share), 23): the Pose Studio's Live subject `e` had key `code` laid on at that
        // share this frame (negative: mirrored, a left hook's); recorded with its frame (vr_poselive.hpp).
        poselive::qcKey(NUM_FOR_EDICT(player), G_FLOAT(OFS_PARM1));
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = 0.f;
        }
        return;
    }
    if(what == 24)
    {
        // weapondrawnpose(world, kind * 1000 + code, 24): the Weapon Holds (vr_agholds.inc; QC VR_AGH_Hands): '<hands>
        // <setting> <class>', hands 1 or 2 (0: no row for that weapon), setting 0 Auto, 1 One-Handed, 2 Two-Handed; class
        // its pose category (agpose::Class: the Pose Category's, a gun with no row or a melee weapon agpose::Gun).
        const int arg = static_cast<int>(G_FLOAT(OFS_PARM1));
        int set = 0;
        const int h = agholds::hands(arg / 1000, arg % 1000, &set);
        progs::returnVec({static_cast<float>(h), static_cast<float>(set), static_cast<float>(agholds::poseClass(arg / 1000, arg % 1000, nullptr))});
        return;
    }
    if(what == 28)
    {
        // weapondrawnpose(world, kind * 100 + class, 28): '<factor> 0 0', the IK stretch of that kind and pose class
        // (agpose::stretch; 1 none), sent in the pose message's off-hand block (QC VR_AGP_Pose).
        const int arg = static_cast<int>(G_FLOAT(OFS_PARM1));
        progs::returnVec({agpose::stretch(arg / 100, arg % 100), 0.f, 0.f});
        return;
    }
    if(what >= 25 && what <= 27)
    {
        // weapondrawnpose(world, code, 25..27): pose key `code`'s off-hand parts (agpose::GripPos, GripTurn, OffHand:
        // x its OffMode), '0 0 0' none.
        const agpose::Key k = agpose::value(static_cast<int>(G_FLOAT(OFS_PARM1)));
        const glm::vec3 out = k.v[agpose::GripPos + (what - 25)];
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(what >= 11 && what <= 19)
    {
        // The armed humanoids' pose keys (vr_agpose.hpp; QC vr_armedgrunt_pose.qc VR_AGK_*), the builtin slots being full:
        // `hand` a key's code, 11..17 its parts (agpose::Part; 25..27 the off hand's, above), 18 '1 0 0' if it has one
        // ('0 0 0': all zero). 19 the Pose Studio's hold on `player` (an armed humanoid; `hand` its kind * 100 + its weapon
        // class): posestudio::qcHold.
        const int arg = static_cast<int>(G_FLOAT(OFS_PARM1));
        glm::vec3 out{0.f};
        if(what == 19 && arg < 0)
        {
            // (`hand` -1: the Rig Studio's strike timeline on `player`: x a play's speed, y its time, z 1 while it drives it)
            out = rigstudio::strikeQuery(NUM_FOR_EDICT(player));
        }
        else if(what == 19)
        {
            out = posestudio::qcHold(NUM_FOR_EDICT(player), arg / 100, arg % 100);
            glm::vec3 studio;
            if(rigstudio::qcHold(NUM_FOR_EDICT(player), arg / 100, arg % 100, studio))
            {
                out = studio; // (the Rig Studio's subject: its aim target's Aim stance, or the Pose Handles' key)
            }
        }
        else
        {
            agpose::Key k;
            const bool has = agpose::get(arg, k);
            out = what == 18 ? glm::vec3{has ? 1.f : 0.f, 0.f, 0.f} : k.v[what - 11];
        }
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(what >= 8 && what <= 10)
    {
        // The weapon model `player` (a weapon entity) is drawn with, in its own frame (view::weaponLocalGeometry): 8 its
        // muzzle (a blade's tip), 9 its butt, 10 its second grip (z +1000: a Cup's, no point); '0 0 0' none.
        const int index = static_cast<int>(player->v.modelindex);
        qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
        glm::vec3 muzzle{0.f}, butt{0.f}, second{0.f};
        int type = 0;
        glm::vec3 out{0.f};
        if(view::weaponLocalGeometry(model, muzzle, butt, second, type))
        {
            out = what == 8 ? muzzle : what == 9 ? butt : (type == 2 ? glm::vec3{0.f, 0.f, 1000.f} : second);
        }
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(what == 7)
    {
        // x: the Weapon Offsets slot (-1 none) of the weapon `player`'s `hand` holds (its weaponmodel, weaponmodel2): the
        // blade's shape (QC VR_Melee_ShapeOf; admeleebend, admeleehead with -1 - slot).
        const FieldOffsets& f = fields();
        const char* name = hand ? PR_GetString(player->v.weaponmodel)
                                : (f.weaponmodel2 >= 0 ? PR_GetString(fieldInt(player, f.weaponmodel2)) : "");
        G_VECTOR(OFS_RETURN)[0] = static_cast<float>(name && *name ? weapons::slotForName(name) : -1);
        G_VECTOR(OFS_RETURN)[1] = G_VECTOR(OFS_RETURN)[2] = 0.f;
        return;
    }
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    glm::vec3 pos{0.f}, rot{0.f}, tip{0.f};
    bool mirrored = false;
    bool known = local && twohand::carrying(hand) && view::carriedWeaponPose(hand, pos, rot, mirrored, tip);
    if(known && (what == 4 || what == 5))
    {
        known = view::carriedWeaponInHand(hand, pos, tip);
        rot = what == 4 ? pos : tip;
    }
    const glm::vec3 out = !known ? glm::vec3{0.f} : what == 0 ? pos : what == 1 || what >= 4 ? rot : what == 3 ? tip : glm::vec3{1.f, 0.f, 0.f};
    for(int i = 0; i < 3; i++)
    {
        G_VECTOR(OFS_RETURN)[i] = out[i];
    }
}

// The armed humanoids' lookups in one call each (PERF_1102.md, "Follow-up": weapondrawnpose's ~30 calls a humanoid a frame),
// their results in QC globals (Bindings::agkPart, agDrawn, agExtent; QC builtins.qc) as traceline's in trace_*.
static void setGlobalVec(float* g, const glm::vec3& v)
{
    if(g)
    {
        g[0] = v.x;
        g[1] = v.y;
        g[2] = v.z;
    }
}

// vector(float code) agposekey: pose key `code` (QC VR_AGK_Add): '1 0 0' and its parts (agpose::RPos..Torso, GripPos,
// GripTurn: weapondrawnpose(world, code, 11..17, 25, 26)) in vr_agk_k_*; '0 0 0' none (weapondrawnpose 18), them left.
void PF_agposekey()
{
    agpose::Key k;
    const bool has = agpose::get(static_cast<int>(G_FLOAT(OFS_PARM0)), k);
    if(has)
    {
        const Bindings& b = bindings();
        for(int i = 0; i < agpose::numOldParts; i++)
        {
            setGlobalVec(b.agkPart[i], k.v[i]);
        }
        setGlobalVec(b.agkPart[agpose::numOldParts], k.v[agpose::GripPos]);
        setGlobalVec(b.agkPart[agpose::numOldParts + 1], k.v[agpose::GripTurn]);
    }
    progs::returnVec({has ? 1.f : 0.f, 0.f, 0.f});
}

// A strike's act's phases' lengths from QC (VR_AGP_Times: '<wind-up> <strike> <hold>' and the recovery).
static agpose::ActTimes actTimesParm(int vecParm, int recParm)
{
    agpose::ActTimes a;
    const float* v = G_VECTOR(vecParm);
    a.t[0] = v[0];
    a.t[1] = v[1];
    a.t[2] = v[2];
    a.t[3] = G_FLOAT(recParm);
    return a;
}

// vector(float code, float s, vector times, float trec) agstrikeblend: strike `code`'s keys (its kind, class and pose;
// any phase) either side of `s` seconds into its act (QC VR_AGK_Keys; agpose::actBlend): '<phase a> <phase b> <b's
// share>', -1 the stance.
void PF_agstrikeblend()
{
    const int c = static_cast<int>(G_FLOAT(OFS_PARM0));
    const agpose::ActBlend b = agpose::actBlend(agpose::kindOf(c), agpose::classOf(c), agpose::poseOf(c),
        actTimesParm(OFS_PARM2, OFS_PARM3), G_FLOAT(OFS_PARM1));
    progs::returnVec({static_cast<float>(b.a), static_cast<float>(b.b), b.f});
}

// float(float code, vector times, float trec) agkeytime: when key `code` is whole in its act (s; agpose::keyTime: its
// place, the Cast's past its raise; QC VR_AGK_KeyTime).
void PF_agkeytime()
{
    G_FLOAT(OFS_RETURN) = agpose::keyTime(static_cast<int>(G_FLOAT(OFS_PARM0)), actTimesParm(OFS_PARM1, OFS_PARM2));
}

// float(entity p) agdrawnframe: armed humanoid's held weapon `p` as drawn lately (weapondrawnpose(p, 0, 30..32)): TRUE
// and its origin, forward and up in vr_ag_df_o, _fw, _up (QC VR_AG_DrawnFrame's); FALSE not drawn lately (them left).
void PF_agdrawnframe()
{
    glm::vec3 at{0.f}, fw{0.f}, up{0.f};
    const bool local = sv.active && cls.state == ca_connected;
    const bool drawn = local && hitreact::drawnWeapon(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), at, fw, up) && fw != glm::vec3{0.f};
    if(drawn)
    {
        const Bindings& b = bindings();
        setGlobalVec(b.agDrawn[0], at);
        setGlobalVec(b.agDrawn[1], fw);
        setGlobalVec(b.agDrawn[2], up);
    }
    G_FLOAT(OFS_RETURN) = drawn ? 1.f : 0.f;
}

// vector(entity p) agweaponextent: weapon `p`'s drawn model along its long axis (held::drawnExtent; weapondrawnpose(p, 0,
// 33..35)): '<its most off that line> <its length> 0', its back and front ends (its own axes) in vr_ag_wx_back, _front;
// all '0 0 0' with no drawn surface.
void PF_agweaponextent()
{
    glm::vec3 back{0.f}, front{0.f};
    float radius = 0.f;
    if(!held::drawnExtent(G_EDICT(OFS_PARM0), back, front, radius))
    {
        back = front = glm::vec3{0.f};
        radius = 0.f;
    }
    const Bindings& b = bindings();
    setGlobalVec(b.agExtent[0], back);
    setGlobalVec(b.agExtent[1], front);
    progs::returnVec({radius, glm::distance(front, back), 0.f});
}

// agposesend's arithmetic as the QuakeC VM does it, for the same bytes: rint (pr_cmds.c PF_rint: its half added in
// double), min and max (PF_min, PF_max with two), a vector's dot product (pr_exec.c OP_MUL_V).
float agpsRint(float f)
{
    return f > 0 ? static_cast<float>(static_cast<int>(f + 0.5)) : static_cast<float>(static_cast<int>(f - 0.5));
}
float agpsMin(float a, float b)
{
    return a > b ? b : a;
}
float agpsMax(float a, float b)
{
    return a < b ? b : a;
}
float agpsDot(const float* a, const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
// QC WriteByte, WriteShort, WriteCoord (MSG_BROADCAST: VR_BroadcastDest's room kept as for QuakeC's own writes)
void agpsByte(float v)
{
    sizebuf_t* d = VR_BroadcastDest(1);
    MSG_WriteByte(d, static_cast<int>(v));
    VR_BroadcastWritten(d);
}
void agpsShort(float v)
{
    sizebuf_t* d = VR_BroadcastDest(2);
    MSG_WriteShort(d, static_cast<int>(v));
    VR_BroadcastWritten(d);
}
void agpsVec(const float* v) // (VR_AGP_WriteVec)
{
    for(int i = 0; i < 3; i++)
    {
        sizebuf_t* d = VR_BroadcastDest(4);
        MSG_WriteCoord(d, v[i], sv.protocolflags);
        VR_BroadcastWritten(d);
    }
}
void agpsDir(const float* v) // (VR_AGP_WriteDir)
{
    for(int i = 0; i < 3; i++)
    {
        agpsShort(agpsRint(v[i] * 32000));
    }
}
void agpsShorts(const float* v, float per) // (VR_AGP_WriteShorts)
{
    for(int i = 0; i < 3; i++)
    {
        agpsShort(agpsRint(agpsMax(-32000, agpsMin(32000, v[i] * per))));
    }
}
void agpsLocalDir(const float* w, const Bindings& b) // (VR_AGP_WriteDir(VR_AGP_ToLocal(w)))
{
    const float o[3] = {agpsDot(w, b.agPose[Bindings::agpF]), -agpsDot(w, b.agPose[Bindings::agpR]), w[2]};
    agpsDir(o);
}

// float(entity g, entity p, entity pL, float fl, float bytes, vector gx, vector gy, vector gz) agposesend: armed humanoid
// `g`'s pose this server frame (QVR_SVC_ARMPOSE) to MSG_BROADCAST, the bytes QC VR_AGP_Pose's ~50 Write calls made, from
// its globals (vr_agp_*, vr_agk_*; vr_agp_waist) and: `p` its weapon (world: reaching, or fists), `pL` its left one, `fl`
// the flags byte + 256 x the off-hand block's byte + 65536 with the body's block, `bytes` the IK stretch's byte + 256 x
// the take's, `gx`, `gy`, `gz` the gun's axes. FALSE: not written (vr_agposesend 0, or a global missing): QC writes it.
void PF_agposesend()
{
    const Bindings& b = bindings();
    G_FLOAT(OFS_RETURN) = 0.f;
    if(vr_agposesend.value == 0.f)
    {
        return;
    }
    for(const float* g : b.agPose)
    {
        if(!g)
        {
            return;
        }
    }
    const int fl = static_cast<int>(G_FLOAT(OFS_PARM3));
    const int bytes = static_cast<int>(G_FLOAT(OFS_PARM4));
    const int sendFl = fl & 255, offFl = (fl >> 8) & 255;
    const bool body = (fl & 65536) != 0;
    const float* gx = G_VECTOR(OFS_PARM5);
    const float* gy = G_VECTOR(OFS_PARM6);
    const float* gz = G_VECTOR(OFS_PARM7);
    const auto glob = [&b](int i) { return b.agPose[i]; };
    agpsByte(39); // svc_quakevr
    agpsByte(36); // QVR_SVC_ARMPOSE
    agpsShort(static_cast<float>(G_EDICTNUM(OFS_PARM0)));
    agpsShort(static_cast<float>(G_EDICTNUM(OFS_PARM1)));
    agpsByte(static_cast<float>(sendFl));
    agpsByte(agpsRint(agpsMin(1, agpsMax(0, *glob(Bindings::agpWr))) * 255));
    agpsVec(glob(Bindings::agpV));
    agpsLocalDir(glob(Bindings::agpFw), b);
    agpsLocalDir(glob(Bindings::agpUp), b);
    for(const int w : {Bindings::agpFw, Bindings::agpUp})
    {
        const float f[3] = {agpsDot(glob(w), gx), agpsDot(glob(w), gy), agpsDot(glob(w), gz)};
        agpsDir(f);
    }
    agpsByte(agpsRint(agpsMin(1, agpsMax(0, *glob(Bindings::agpWo))) * 255));
    agpsByte(agpsRint(agpsMin(1, agpsMax(0, *glob(Bindings::agpWl))) * 255));
    agpsVec(glob(Bindings::agpVl));
    if(sendFl & 8) // VR_AGP_LEFTWPN
    {
        agpsShort(static_cast<float>(G_EDICTNUM(OFS_PARM2)));
        agpsLocalDir(glob(Bindings::agpFwl), b);
        agpsLocalDir(glob(Bindings::agpUpl), b);
    }
    if(body)
    {
        const float* tq = glob(Bindings::agpTq);
        for(int i = 0; i < 3; i++)
        {
            agpsShort(agpsRint(tq[i] * 100));
        }
        agpsDir(glob(Bindings::agpElr));
        agpsDir(glob(Bindings::agpEll));
        agpsVec(glob(Bindings::agpWaist));
        if(sendFl & 1) // VR_AGP_FIXED
        {
            agpsByte(static_cast<float>((bytes >> 8) & 255));
        }
    }
    if(offFl)
    {
        agpsByte(static_cast<float>(offFl));
        if(offFl & 4)
        {
            agpsShorts(glob(Bindings::agkGp), 100);
            agpsShorts(glob(Bindings::agkGr), 10);
        }
        if(offFl & 8)
        {
            agpsShorts(glob(Bindings::agkLp), 100);
            agpsShorts(glob(Bindings::agkLr), 10);
        }
        if(offFl & 16)
        {
            agpsByte(static_cast<float>(bytes & 255));
        }
    }
    G_FLOAT(OFS_RETURN) = 1.f;
}

// vector(float kind, float f) admeleebend: AD sword kind `kind`'s blade at share `f` of the line from the hand to its tip,
// off that line (admelee::bend, as shares of its length): x in the blade's plane (the model's +y crossed with the line),
// y along the model's +y; z 1 if the blade is curved (0: straight, nothing off it). QC VR_Melee_At.
// A kind below 0: Weapon Offsets slot -1 - kind's own curve (Blade Spine, vr_blade.hpp; else its model's, a curved AD
// sword's) and its blade start's bend (Blade From); z 1 if it has either: Edit Blade Shape.
void PF_admeleebend()
{
    if(const int kind = static_cast<int>(G_FLOAT(OFS_PARM0)); kind < 0)
    {
        const blade::Shape s = blade::shape(-1 - kind);
        const float f = G_FLOAT(OFS_PARM1);
        glm::vec2 k{0.f};
        if(s.spine)
        {
            k = blade::spineAt(s, f);
        }
        else if(const cvar_t* id = weapons::cvar(-1 - kind, weapons::Key::ID); id && f > 0.f)
        {
            k = admelee::bend(admelee::kindOfModel(id->string), f);
        }
        k += blade::fromAt(s, f);
        progs::returnVec({k.x, k.y, s.spine || blade::hasFrom(s) ? 1.f : 0.f});
        return;
    }
    const glm::vec2 b = admelee::bend(static_cast<int>(G_FLOAT(OFS_PARM0)), G_FLOAT(OFS_PARM1));
    progs::returnVec({b.x, b.y, b != glm::vec2{0.f} ? 1.f : 0.f});
}

// vector(float kind, float which) admeleehead: AD heavy kind `kind`'s (the hammer, the mace) head's box (admelee::head,
// shares of the line from the hand to its tip): `which` 0 its low corner, 1 its high; '0 0 0' for none. QC
// VR_Melee_Sweep (the head's volume strikes).
// A kind below 0: Weapon Offsets slot -1 - kind's own blade shape (vr_blade.hpp; Edit Blade Shape): `which` 0 and 1 its
// own head's corners ('0 0 0': none of its own), 2 '<near end> <width> <flags>' (the near end's share behind the hand, -1
// the kind's own, QC VR_Melee_NearBack; the swept points' radius's multiplier; 1 a curve of its own + 2 a head of its own
// + 4 a blade start of its own + 8 a line start or tip of its own), 3 '<blade start> <off> <side>' (Blade From: its share
// of the line, -1 the kind's own), 4 the line's start off the hand and 5 its tip off the muzzle (Blade Org, Blade Tip:
// units along the hand's forward, right, up).
void PF_admeleehead()
{
    glm::vec3 lo{0.f}, hi{0.f};
    if(const int kind = static_cast<int>(G_FLOAT(OFS_PARM0)); kind < 0)
    {
        const blade::Shape s = blade::shape(-1 - kind);
        const int which = static_cast<int>(G_FLOAT(OFS_PARM1));
        const glm::vec3 out = which == 2   ? glm::vec3{s.nearBack, s.width,
                                                 (s.spine ? 1.f : 0.f) + (s.head ? 2.f : 0.f) + (blade::hasFrom(s) ? 4.f : 0.f) +
                                                     (blade::hasEnds(s) ? 8.f : 0.f)}
                              : which == 3 ? glm::vec3{blade::hasFrom(s) ? s.from : -1.f, s.fromOff.x, s.fromOff.y}
                              : which == 4 ? s.origin
                              : which == 5 ? s.tipOffset
                              : !s.head    ? glm::vec3{0.f}
                              : which == 1 ? s.headHi
                                           : s.headLo;
        for(int i = 0; i < 3; i++)
        {
            G_VECTOR(OFS_RETURN)[i] = out[i];
        }
        return;
    }
    if(!admelee::head(static_cast<int>(G_FLOAT(OFS_PARM0)), lo, hi))
    {
        lo = hi = glm::vec3{0.f};
    }
    const glm::vec3& out = G_FLOAT(OFS_PARM1) != 0.f ? hi : lo;
    for(int i = 0; i < 3; i++)
    {
        G_VECTOR(OFS_RETURN)[i] = out[i];
    }
}

// void(float hand, float what, vector a, vector b, float v) meleeshow: Show Melee Blades' data (vr_meleeshow.hpp; QC
// VR_Melee_Show, sent only while vr_debug_melee_blades is on).
void PF_meleeshow()
{
    const float* a = G_VECTOR(OFS_PARM2);
    const float* b = G_VECTOR(OFS_PARM3);
    meleeshow::add(static_cast<int>(G_FLOAT(OFS_PARM0)), static_cast<int>(G_FLOAT(OFS_PARM1)),
        geom::vec(a), geom::vec(b), G_FLOAT(OFS_PARM4));
}

// Magnetic holsters (QC vr_magholster.qc; vr_magholster.cpp). float(entity player) magholsterpoll: what the local
// player asked since the last call, taken (1 the off hand's bind button at a holster, 2 the main hand's, 4 every bound
// weapon home at once: vr_magholster_return); others' 0. vector(entity player, float slot) holsterpos: holster `slot`'s
// place (body::Holster, the holsterweapon<slot> fields' order) as the local player's body has it ('0 0 0': unknown, or
// another player: QC's own estimate then).
void PF_magholsterpoll()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    G_FLOAT(OFS_RETURN) = local ? static_cast<float>(magholster::poll()) : 0.f;
}

void PF_holsterpos()
{
    edict_t* player = G_EDICT(OFS_PARM0);
    const int slot = static_cast<int>(G_FLOAT(OFS_PARM1));
    const bool local = sv.active && cls.state == ca_connected && NUM_FOR_EDICT(player) == 1;
    glm::vec3 out{0.f};
    if(local && slot >= 0 && slot < body::HolsterCount && hands::current().valid)
    {
        out = body::holsterPosition(hands::current(), static_cast<body::Holster>(slot));
    }
    for(int i = 0; i < 3; i++)
    {
        G_VECTOR(OFS_RETURN)[i] = out[i];
    }
}

// Rocks and bricks lying about (vr_debris.cpp, QC vr_debris.qc). float() debrisplan: places them (after the map's
// entities), how many; string(float i) debrismodel: piece i's model (to set); float(entity e, float i) debrisput: puts
// piece i on e (its model set): its skin, size, turn, place resting on the floor and box; its kind (1 a rock, 2 a
// brick; 0 none).
void PF_debrisplan()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(debris::plan());
}

void PF_debrismodel()
{
    G_INT(OFS_RETURN) = PR_SetEngineString(debris::modelOf(static_cast<int>(G_FLOAT(OFS_PARM0))));
}

void PF_debrisput()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(debris::put(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1))));
}

// float(entity e) debrisplace: a piece the map placed (vr_debris_piece, its model set): sized, turned by its yaw and
// resting on the floor at its origin, its box; its kind (1 a rock, 2 a brick; 0: not one of their models).
void PF_debrisplace()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(debris::putPlaced(G_EDICT(OFS_PARM0)));
}

// Wooden crates lying about (vr_crates.cpp, QC vr_crates.qc), as the rocks and bricks: float() crateplan, string(float
// i) cratemodel, float(entity e, float i) crateput (its kind: 1 small, 2 large; 0 none), float(entity e) crateplace (a
// crate the map or a test placed, its model set: level, turned by its yaw, resting on the floor under its origin).
void PF_crateplan()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(crates::plan());
}

void PF_cratemodel()
{
    G_INT(OFS_RETURN) = PR_SetEngineString(crates::modelOf(static_cast<int>(G_FLOAT(OFS_PARM0))));
}

void PF_crateput()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(crates::put(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1))));
}

// float(float i) cratecrowbar: what the plan puts on crate i, bits: a crowbar (vr_crate_crowbar: 1), a flare gun
// (vr_crate_flaregun: 2; no builtin numbers left for one of its own); float(entity e, float i, float what)
// cratecrowbarput: lays `e` (a thrown weapon, its model set), that one of them, on it (crates::putCrowbar).
void PF_cratecrowbar()
{
    const int i = static_cast<int>(G_FLOAT(OFS_PARM0));
    G_FLOAT(OFS_RETURN) = static_cast<float>((crates::hasCrowbar(i) ? crates::TopCrowbar : 0) | (crates::hasFlareGun(i) ? crates::TopFlareGun : 0));
}

void PF_cratecrowbarput()
{
    const crates::TopItem what = G_FLOAT(OFS_PARM2) == static_cast<float>(crates::TopFlareGun) ? crates::TopFlareGun : crates::TopCrowbar;
    G_FLOAT(OFS_RETURN) = crates::putCrowbar(G_EDICT(OFS_PARM0), static_cast<int>(G_FLOAT(OFS_PARM1)), what) ? 1.f : 0.f;
}

void PF_crateplace()
{
    G_FLOAT(OFS_RETURN) = static_cast<float>(crates::putPlaced(G_EDICT(OFS_PARM0)));
}

// entity sightblocked(vector start, vector end, entity ignore, entity ignore2): the solid prop that blocks sight
// (.vr_blocksight: a crate, an explosive box; not one in a hand) between `start` and `end`, met by its shape as it lies;
// world if none (crates::sightBlocked). QC's visible() (ai.qc): monsters don't see you behind one.
void PF_sightblocked()
{
    const float* a = G_VECTOR(OFS_PARM0);
    const float* b = G_VECTOR(OFS_PARM1);
    const int ignoreA = qcvm->argc > 2 ? NUM_FOR_EDICT(G_EDICT(OFS_PARM2)) : 0;
    const int ignoreB = qcvm->argc > 3 ? NUM_FOR_EDICT(G_EDICT(OFS_PARM3)) : 0;
    const int num = crates::sightBlocked(geom::vec(a), geom::vec(b), ignoreA, ignoreB);
    G_INT(OFS_RETURN) = EDICT_TO_PROG(EDICT_NUM(num));
}

// float heldshape(entity prop, vector start, vector end): where the line from `start` to `end` first meets the shape of
// `prop` held in a hand (its held body, as the hand holds it), as a fraction of it; 1 if it doesn't or `prop` isn't held
// (box3d::heldRay). A crate held up stops monsters' shots (weapons.qc VR_Crate_ShieldShot).
void PF_heldshape()
{
    const float* a = G_VECTOR(OFS_PARM1);
    const float* b = G_VECTOR(OFS_PARM2);
    G_FLOAT(OFS_RETURN) = box3d::heldRay(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), geom::vec(a),
        geom::vec(b));
}

// float heldbox(entity prop, vector mins, vector maxs, float reach): whether the shape of `prop` held in a hand meets the
// box mins..maxs (world) grown by `reach` units: 1 yes, 0 no, -1 `prop` has no held body (box3d::heldBox). A held prop
// presses a wall button it touches (buttons.qc VR_Buttons_PropFrame).
void PF_heldbox()
{
    const float* lo = G_VECTOR(OFS_PARM1);
    const float* hi = G_VECTOR(OFS_PARM2);
    G_FLOAT(OFS_RETURN) = static_cast<float>(box3d::heldBox(NUM_FOR_EDICT(G_EDICT(OFS_PARM0)), geom::vec(lo),
        geom::vec(hi), G_FLOAT(OFS_PARM3)));
}
