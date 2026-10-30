// vr_toolgun_physgun.inc -- the toolgun's physgun (drag, freeze), the scale tool's gizmo, joints and the sticks' moves.
// Split from vr_toolgun.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// ---------------------------------------------------------------------------------------------------------------------
// The physgun.

void dragStart(int num)
{
    if(brushes::ofEntity(num))
    {
        Con_Printf("toolgun: a box moves with the Brush tool (on its grid)\n");
        return;
    }
    edict_t* e = EDICT_NUM(num);
    Drag& d = tg.drag;
    d = {};
    d.num = num;
    d.prop = isRigidProp(e);
    d.distance = tg.aim.targetDistance;
    const glm::vec3 grab = tg.aim.from + tg.aim.dir * d.distance;
    d.offset = geom::vec(e->v.origin) - grab;
    d.angles = geom::vec(e->v.angles);
    d.last = geom::vec(e->v.origin);
    const int mi = static_cast<int>(e->v.modelindex);
    d.brush = mi > 0 && mi < MAX_MODELS && sv.models[mi] && sv.models[mi]->type == mod_brush;
    d.hand0 = held::axesFromAngles(&tg.handRot[0], true);
    d.handYaw = tg.handRot.y;
    d.handTurn = 0.f;
    d.axes0 = held::axesFromAngles(e->v.angles, d.brush);
    d.since = qcvm->time;
    if(d.prop)
    {
        box3d::setPinned(num, true);
    }
    else if(box3d::isRagdoll(num))
    {
        d.part = box3d::ragdollBone(num, grab); // (the old way's: the part nearest, for vr_toolgun_status)
        float along = 0.f;
        if(box3d::physgunRagdollGrab(num, tg.aim.from, tg.aim.dir, tg.aim.worldDistance, grab, d.part, along))
        {
            d.ragdoll = true;
            d.distance = za::max(along, 16.f);
            d.last = tg.aim.from + tg.aim.dir * d.distance;
        }
        Con_Printf("toolgun: physgun holds %d %s's ragdoll by part %d %s at %.0f units\n", num, PR_GetString(e->v.classname),
            d.part, d.ragdoll ? "on a spring" : "by its origin", d.distance);
        return;
    }
    Con_Printf("toolgun: physgun holds %d %s at %.0f units\n", num, PR_GetString(e->v.classname), d.distance);
}

void dragEnd(bool freeze)
{
    Drag& d = tg.drag;
    if(!d.num || d.num >= qcvm->num_edicts || EDICT_NUM(d.num)->free)
    {
        d = {};
        return;
    }
    edict_t* e = EDICT_NUM(d.num);
    if(d.ragdoll)
    {
        // Let go: the spring gone, the held part keeps the beam's swing (its entity was never moved).
        glm::vec3 v = d.velocity;
        const float speed = glm::length(v);
        v *= speed > 1000.f ? 1000.f / speed : 1.f;
        box3d::physgunRagdollRelease(v);
        Con_Printf("toolgun: physgun let go of %d %s's ragdoll at %.0f u/s\n", d.num, PR_GetString(e->v.classname), glm::length(v));
        d = {};
        return;
    }
    e->v.flags = static_cast<float>(static_cast<int>(e->v.flags) & ~FL_ONGROUND);
    if(d.prop && freeze)
    {
        VectorCopy(vec3_origin, e->v.velocity);
        Con_Printf("toolgun: froze %d %s\n", d.num, PR_GetString(e->v.classname));
    }
    else
    {
        // Let go: it falls, with the beam's swing (a prop: its body made dynamic again with it).
        glm::vec3 v = d.velocity;
        const float speed = glm::length(v);
        if(speed > 1000.f)
        {
            v *= 1000.f / speed;
        }
        e->v.velocity[0] = v.x;
        e->v.velocity[1] = v.y;
        e->v.velocity[2] = v.z;
        if(d.prop)
        {
            box3d::setPinned(d.num, false);
            setFieldVec(e, fields().vr_spin, glm::vec3{0.f});
        }
        Con_Printf("toolgun: physgun let go of %d %s at %.0f u/s\n", d.num, PR_GetString(e->v.classname), glm::length(v));
    }
    d = {};
}

void dragFrame(float dt)
{
    Drag& d = tg.drag;
    if(!d.num)
    {
        return;
    }
    if(d.num >= qcvm->num_edicts || EDICT_NUM(d.num)->free || !tg.aim.valid)
    {
        d = {};
        return;
    }
    edict_t* e = EDICT_NUM(d.num);
    glm::vec3 fwd, right;
    levelAxes(tg.aim.dir, fwd, right);
    const glm::vec3 grab = tg.aim.from + tg.aim.dir * za::max(d.distance, 16.f);
    const auto follow = [dt](const glm::vec3& to) {
        Drag& d = tg.drag;
        if(dt > 0.f)
        {
            const glm::vec3 v = (to - d.last) / dt;
            d.velocity += (v - d.velocity) * za::min(1.f, dt * 12.f);
        }
        d.last = to;
    };
    if(d.ragdoll)
    {
        // A ragdoll: its spring's target the beam's point (the sticks' moves on it); nothing turns it.
        const glm::vec3 to = grab + right * d.slide.x + glm::vec3{0.f, 0.f, d.slide.y};
        if(!box3d::physgunRagdollMove(to))
        {
            Con_Printf("toolgun: physgun lost %d (its ragdoll gone)\n", d.num);
            d = {};
            return;
        }
        follow(to);
        return;
    }
    // X/A held: its angles snap to whole steps of vr_toolgun_snap degrees; let go, the snap is kept (baked into the turn).
    const float step = tg.adjust ? vr_toolgun_snap.value : 0.f;
    if(step <= 0.f && d.snapped)
    {
        d.stickTurn = d.snapFix * d.stickTurn;
        d.turn += d.snapDelta;
    }
    d.snapped = step > 0.f;
    d.snapFix = glm::mat3{1.f};
    d.snapDelta = glm::vec3{0.f};
    const auto snap = [step](float& a) {
        if(step > 0.f)
        {
            a = za::round(a / step) * step;
        }
    };
    d.handTurn += za::remainder(tg.handRot.y - d.handYaw, 360.f) * handTurnWeight(tg.handRot.x);
    d.handYaw = tg.handRot.y;
    glm::vec3 to;
    float angles[3];
    if(vr_toolgun_physgun_wrist.value != 0.f)
    {
        // Turned as the hand turns since it was taken (its offset from the beam's point too: as if on a skewer), the
        // sticks' turn on top; the body's turn turns the hand, so it turns with you.
        const glm::mat3 hand = held::axesFromAngles(&tg.handRot[0], true);
        glm::mat3 turn = d.stickTurn * hand * glm::transpose(d.hand0);
        held::anglesFromAxes(turn * d.axes0, angles, d.brush);
        if(!d.prop)
        {
            // A monster or a pickup only turns about the up axis (tilted, a monster would walk on so): as the hand's
            // way turns (not its twist), and the sticks' yaw.
            angles[0] = d.angles.x;
            angles[1] = d.angles.y + d.handTurn + d.turn.y;
            angles[2] = d.angles.z;
            const float unsnapped = angles[1];
            snap(angles[1]);
            d.snapDelta.y = angles[1] - unsnapped;
            turn =glm::mat3_cast(glm::angleAxis(glm::radians(angles[1] - d.angles.y), glm::vec3{0.f, 0.f, 1.f}));
        }
        else if(step > 0.f)
        {
            for(float& a : angles)
            {
                snap(a);
            }
            const glm::mat3 unsnapped = turn;
            turn = held::axesFromAngles(angles, d.brush) * glm::transpose(d.axes0);
            d.snapFix = turn * glm::transpose(unsnapped);
        }
        to = grab + turn * d.offset;
    }
    else
    {
        const glm::quat spin = glm::angleAxis(glm::radians(d.turn.y), glm::vec3{0.f, 0.f, 1.f});
        to = grab + spin * d.offset;
        for(int k = 0; k < 3; k++)
        {
            angles[k] = d.angles[k] + d.turn[k];
            snap(angles[k]);
            d.snapDelta[k] = angles[k] - (d.angles[k] + d.turn[k]);
        }
    }
    to += right * d.slide.x + glm::vec3{0.f, 0.f, d.slide.y};
    e->v.origin[0] = to.x;
    e->v.origin[1] = to.y;
    e->v.origin[2] = to.z;
    e->v.angles[0] = angles[0];
    e->v.angles[1] = angles[1];
    e->v.angles[2] = angles[2];
    VectorCopy(vec3_origin, e->v.velocity);
    if(!d.prop)
    {
        e->v.flags = static_cast<float>(static_cast<int>(e->v.flags) | FL_ONGROUND); // (held up: no fall while held)
    }
    SV_LinkEdict(e, false);
    follow(to);
}

// ---------------------------------------------------------------------------------------------------------------------
// The scale tool's gizmo.

[[nodiscard]] glm::vec3 scaleOf(edict_t* e)
{
    return glm::vec3{1.f} + fieldVec(e, fields().model_scale);
}

// The prop's drawn box (its model's bounds scaled about model_scale_origin, turned with it) into `g`. False: not a
// prop with a model.
[[nodiscard]] bool gizmoBox(int num, Gizmo& g)
{
    edict_t* e = EDICT_NUM(num);
    const int mi = static_cast<int>(e->v.modelindex);
    if(!isRigidProp(e) || mi <= 0 || mi >= MAX_MODELS || !sv.models[mi])
    {
        return false;
    }
    const qmodel_t* m = sv.models[mi];
    const glm::vec3 k = scaleOf(e), o = fieldVec(e, fields().model_scale_origin);
    const glm::vec3 lo = o + (geom::vec(m->mins) - o) * k;
    const glm::vec3 hi = o + (geom::vec(m->maxs) - o) * k;
    g.num = num;
    g.axes = held::axesFromAngles(e->v.angles, m->type == mod_brush);
    g.half = (hi - lo) * 0.5f;
    g.centre = geom::vec(e->v.origin) + g.axes * ((lo + hi) * 0.5f);
    return true;
}

// Handle `h`'s way from the centre (unit, world) and its point.
[[nodiscard]] glm::vec3 handleLocal(int h)
{
    if(h < 6)
    {
        glm::vec3 v{0.f};
        v[h / 2] = (h & 1) ? 1.f : -1.f;
        return v;
    }
    const int c = h - 6;
    return glm::vec3{(c & 1) ? 1.f : -1.f, (c & 2) ? 1.f : -1.f, (c & 4) ? 1.f : -1.f};
}

[[nodiscard]] glm::vec3 handlePoint(const Gizmo& g, int h)
{
    return g.centre + g.axes * (handleLocal(h) * g.half);
}

// The handle nearest the aim's line, within reach of it (a few units, more far away); -1 none.
[[nodiscard]] int aimedHandle(const Gizmo& g)
{
    int best = -1;
    float bestOff = 1e30f;
    for(int h = 0; h < 14; h++)
    {
        const glm::vec3 p = handlePoint(g, h);
        const float along = glm::dot(p - tg.aim.from, tg.aim.dir);
        if(along <= 0.f)
        {
            continue;
        }
        const float off = glm::length(p - (tg.aim.from + tg.aim.dir * along));
        if(off < 2.5f + along * 0.03f && off < bestOff)
        {
            bestOff = off;
            best = h;
        }
    }
    return best;
}

void setScale(int num, const glm::vec3& k)
{
    edict_t* e = EDICT_NUM(num);
    const glm::vec3 clamped = glm::clamp(k, glm::vec3{0.1f}, glm::vec3{10.f});
    const glm::vec3 old = scaleOf(e);
    const int mi = static_cast<int>(e->v.modelindex);
    if(old == glm::vec3{1.f} && mi > 0 && mi < MAX_MODELS && sv.models[mi])
    {
        // Scaled about its middle (its first scaling: as it is drawn now nothing moves).
        const qmodel_t* m = sv.models[mi];
        setFieldVec(e, fields().model_scale_origin,
            (geom::vec(m->mins) + geom::vec(m->maxs)) * 0.5f);
    }
    setFieldVec(e, fields().model_scale, clamped - glm::vec3{1.f});
    e->v.flags = static_cast<float>(static_cast<int>(e->v.flags) & ~FL_ONGROUND); // (awake: it settles at its new size)
    // Its Quake box grown with it (its touches, the toolgun's aim), about its origin (its body is made again from the
    // drawn box: box3d's stale).
    Gizmo g;
    if(gizmoBox(num, g))
    {
        glm::vec3 lo{1e30f}, hi{-1e30f};
        for(int c = 0; c < 8; c++)
        {
            const glm::vec3 p = handlePoint(g, 6 + c) - geom::vec(e->v.origin);
            lo = glm::min(lo, p);
            hi = glm::max(hi, p);
        }
        for(int k3 = 0; k3 < 3; k3++)
        {
            e->v.mins[k3] = lo[k3];
            e->v.maxs[k3] = hi[k3];
            e->v.size[k3] = hi[k3] - lo[k3];
        }
        SV_LinkEdict(e, false);
    }
}

void scaleFrame()
{
    Gizmo& g = tg.gizmo;
    const int num = g.held >= 0 ? g.num : tg.aim.target;
    if(num <= 0 || num >= qcvm->num_edicts || EDICT_NUM(num)->free)
    {
        g = {};
        return;
    }
    Gizmo now = g;
    if(!gizmoBox(num, now))
    {
        g = {};
        return;
    }
    now.hover = now.held >= 0 ? now.held : aimedHandle(now);
    g = now;
    if(g.held >= 0)
    {
        const float along = glm::dot(tg.aim.from + tg.aim.dir * g.reach0 - g.centre0, g.way0);
        if(za::fabs(g.along0) > 0.5f)
        {
            const float r = za::clamp(along / g.along0, 0.05f, 20.f);
            glm::vec3 k = g.scale0 * r;
            if(g.held < 6 && vr_toolgun_scale_uniform.value == 0.f)
            {
                k = g.scale0;
                k[g.held / 2] = g.scale0[g.held / 2] * r; // (one face: along its axis alone)
            }
            setScale(num, k);
        }
    }
}

void scaleGrab()
{
    Gizmo& g = tg.gizmo;
    if(g.num <= 0 || g.hover < 0)
    {
        return;
    }
    g.held = g.hover;
    g.scale0 = scaleOf(EDICT_NUM(g.num));
    const glm::vec3 handle = handlePoint(g, g.held);
    g.centre0 = g.centre;
    g.way0 = glm::length(handle - g.centre) > 1e-3f ? glm::normalize(handle - g.centre) : glm::vec3{0.f, 0.f, 1.f};
    g.along0 = glm::dot(handle - g.centre, g.way0);
    g.reach0 = glm::length(handle - tg.aim.from);
}

void drawGizmo(const Gizmo& g)
{
    if(g.num <= 0)
    {
        return;
    }
    // Its edges orange, its handles squares: the faces' cyan, the corners' white, the one aimed at (or held) large and
    // orange, glowing.
    const glm::vec4 edge{1.f, 0.55f, 0.1f, 1.f};
    for(int a = 0; a < 8; a++)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            const int b = a | bit;
            if(b != a)
            {
                lines::line(handlePoint(g, 6 + a), handlePoint(g, 6 + b), 0.3f, edge, edge);
            }
        }
    }
    for(int h = 0; h < 14; h++)
    {
        const glm::vec3 p = handlePoint(g, h);
        if(h == g.hover)
        {
            lines::point(p, 3.f, glm::vec4{1.f, 0.4f, 0.1f, 1.f});
            lines::glowPoint(p, 5.f, glm::vec4{1.f, 0.5f, 0.1f, 0.8f});
            continue;
        }
        lines::point(p, 1.6f, h < 6 ? glm::vec4{0.3f, 0.9f, 1.f, 1.f} : glm::vec4{1.f, 1.f, 1.f, 1.f});
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Joints.

void jointShot(int num, const glm::vec3& at, const glm::vec3& normal)
{
    edict_t* e = EDICT_NUM(num);
    if(!isRigidProp(e))
    {
        Con_Printf("toolgun: %s is not a physics prop\n", PR_GetString(e->v.classname));
        return;
    }
    const glm::vec3 origin{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
    if(jointKind() == unjoin)
    {
        Con_Printf("toolgun: removed %d joints of %d\n", box3d::removeToolJoints(num), num);
        tg.jointFirst = 0;
        return;
    }
    if(!tg.jointFirst || tg.jointFirst == num || tg.jointFirst >= qcvm->num_edicts || EDICT_NUM(tg.jointFirst)->free)
    {
        tg.jointFirst = num;
        tg.jointLocal = glm::transpose(held::axesFromAngles(e->v.angles, false)) * (at - origin);
        Con_Printf("toolgun: joint from %d %s\n", num, PR_GetString(e->v.classname));
        return;
    }
    edict_t* first = EDICT_NUM(tg.jointFirst);
    const glm::vec3 atA = geom::vec(first->v.origin) +
                          held::axesFromAngles(first->v.angles, false) * tg.jointLocal;
    const auto kind = static_cast<box3d::ToolJoint>(jointKind());
    const bool made = box3d::addToolJoint(tg.jointFirst, num, kind, atA, at, normal);
    Con_Printf("toolgun: %s %d to %d: %s\n", jointNames[jointKind()], tg.jointFirst, num, made ? "joined" : "not joined (no body)");
    tg.jointFirst = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// The sticks: always while the physgun holds something, else with X/A held.

// (vr_deadzone's, as the menus' and turning's: past it rescaled from 0; round 23, his decision D5: was a fixed 0.15,
// the push kept as it was past it.)
[[nodiscard]] float deadzone(float v)
{
    return qvr::input::stickAxis(v);
}

// The physgun holds something: both sticks are its (no X/A needed), and stick presses do nothing.
[[nodiscard]] bool physgunHolds()
{
    return tg.drag.num != 0 && currentTool() == Tool::Physgun;
}

// A/X held on the gun's hand gives it the sticks, except with the spawn tool (its ghost takes none: ghostHoldFrame).
[[nodiscard]] bool adjustTakesSticks()
{
    return tg.adjust && currentTool() != Tool::Spawn;
}

void sticksFrame(float dt)
{
    if(!(adjustTakesSticks() || physgunHolds()) || tg.hand < 0 || key_dest != key_game)
    {
        return;
    }
    const glm::vec2 gun = tg.stick[tg.hand], other = tg.stick[1 - tg.hand];
    const float move = vr_toolgun_move_speed.value * dt, spin = vr_toolgun_turn_speed.value * dt;
    const int axis = za::clamp(static_cast<int>(vr_toolgun_turn_axis.value), 0, 2);
    const float push = deadzone(gun.y) * move, turn = -deadzone(gun.x) * spin;
    const float side = deadzone(other.x) * move, lift = deadzone(other.y) * move;
    switch(currentTool())
    {
    case Tool::Physgun:
        if(tg.drag.num)
        {
            tg.drag.distance = za::max(tg.drag.distance + push, 16.f);
            tg.drag.slide += glm::vec3{side, lift, 0.f};
            tg.drag.turn[axis] += turn;
            if(turn != 0.f)
            {
                // (the wrist's way: about the world's up, or the gun's level right or forward)
                glm::vec3 fwd, right;
                levelAxes(tg.aim.dir, fwd, right);
                const glm::vec3 about = axis == 1 ? glm::vec3{0.f, 0.f, 1.f} : axis == 0 ? right : fwd;
                tg.drag.stickTurn = glm::mat3_cast(glm::angleAxis(glm::radians(turn), about)) * tg.drag.stickTurn;
            }
        }
        break;
    case Tool::Scale:
    {
        const int num = tg.gizmo.num;
        if(num > 0 && deadzone(gun.y) != 0.f && tg.gizmo.held < 0)
        {
            setScale(num, scaleOf(EDICT_NUM(num)) * (1.f + deadzone(gun.y) * dt));
        }
        break;
    }
    case Tool::Brush: toolgun::brushtool::push(push); break;
    default:
        // The spawn tool's ghost takes no stick (its hold: ghostHoldFrame); Remove and Joint have none.
        break;
    }
}

// The ghost held by a hand: its offsets follow that hand's translation (1:1, along the gun's level forward, right and
// up, so walking carries it with the aim), its yaw follows the gun hand's way (ghostTurnRate times over, the ghost fixed
// where it stood meanwhile: GhostHold). Only the hand's movement counts, from
// the moment its A/X was pressed: its place then, and its rotation, move the ghost nothing.
// The turn is the gun hand's A/X held (tg.adjust, as for the other tools) while the ghost shows, decided each frame: a
// tool change with A/X still held hands it to the next tool's sticks (and the stick press keeps going back), and a
// ghost that shows up under a held A/X starts turning from the hand's way then.
void ghostHoldFrame(const hands::State& s, float dt)
{
    const bool turning = tg.adjust && tg.hand >= 0 && key_dest == key_game && ghostShowing();
    const bool wasTurning = turning && ghostHold.turn == tg.hand;
    const float before = ghostHold.yaw;
    ghostHold.turn = turning ? tg.hand : -1;
    // Turning fixes the ghost where it stands at the press (updateGhost); let go, it follows the aim again.
    if(!turning)
    {
        ghostHold.fixed = false;
    }
    else if(!ghostHold.fixed)
    {
        ghostHold.fixed = true;
        ghostHold.point = ghostPoint(tg.aim);
        ghostHold.dir = tg.aim.dir;
    }
    // The hand's yaw in the play space: the player's own turn (the stick's, still the player's) turns the ghost nothing.
    // (Kept current while not turning, so the turn does not jump when it starts.)
    ghostHold.yaw = tg.handRot.y - s.turnYaw;
    if(dt <= 0.f || key_dest != key_game || !ghostShowing())
    {
        return;
    }
    if(ghostHold.move >= 0 && s.valid)
    {
        glm::vec3 fwd, right;
        levelAxes(ghostDir(), fwd, right);
        const glm::vec3 gone = s.vel[ghostHold.move] * dt * units::metresToUnits(); // metres a second in Quake axes,
        // without the player's own (hands::State::vel is metres: the offsets are units)
        // The offsets are components along the gun's level way, not a world vector: add the components.
        tg.offset += glm::vec3{glm::dot(gone, fwd), glm::dot(gone, right), gone.z};
        ghostHold.seen += gone;
    }
    if(wasTurning)
    {
        const float turned = za::remainder(ghostHold.yaw - before, 360.f) * handTurnWeight(tg.handRot.x) * ghostTurnRate;
        tg.turn.y += turned;
    }
}
