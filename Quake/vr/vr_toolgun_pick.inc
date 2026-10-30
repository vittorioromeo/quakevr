// vr_toolgun_pick.inc -- the toolgun's spawn and pick (the ghost, the look probe, armed armour) and remove.
// Split from vr_toolgun.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// ---------------------------------------------------------------------------------------------------------------------
// Spawn and pick.

void pick(int num)
{
    edict_t* e = EDICT_NUM(num);
    Selection& sel = tg.sel;
    sel = {};
    const char* cls = PR_GetString(e->v.classname);
    const int wid = static_cast<int>(e->v.weapon);
    if(!strcmp(cls, "thrown_weapon") || !strcmp(cls, "func_weapon_grabbable"))
    {
        // A weapon lying about: made again as a map's grabbable weapon of its id.
        copyText(sel.classname, sizeof(sel.classname), "func_weapon_grabbable");
        copyText(sel.key, sizeof(sel.key), "weapon");
        sel.value = static_cast<float>(wid);
        sel.lift = 8.f;
        // AD support: an AD melee weapon's kind (QC vr_ad_guns_defs.qc: the sword's weapon flags, bits 8-11) kept;
        // the rest of its flags are the one picked's state (a new one is made, loaded).
        const int flagsOfs = progs::fields().weaponflags;
        if(flagsOfs >= 0)
        {
            // (the sword, id 13, also keeps QC QVR_WPNFLAG_USE_SECONDARY_AMMO: the death knight's)
            const int keep = wid == 13 ? (swordKindBits | 1) : swordKindBits;
            sel.weaponFlags = static_cast<float>(static_cast<int>(fieldFloat(e, flagsOfs)) & keep);
        }
    }
    else if(!qvr::progs::findFunction(cls))
    {
        Con_Printf("toolgun: %s can't be made again (no spawn function)\n", cls);
        return;
    }
    else
    {
        copyText(sel.classname, sizeof(sel.classname), cls);
        sel.lift = za::max(-e->v.mins[2], 0.f);
        // A rock's or a brick's model is its kind; a crate's large one is its spawnflag.
        sel.setModel = !strcmp(cls, "vr_debris_piece");
        // A wall torch (on its wall or taken): hung on a wall again.
        sel.wall = !strcmp(cls, "light_torch_small_walltorch") || !strcmp(cls, "light_torch_long_walltorch");
        if(e->v.spawnflags != 0.f)
        {
            copyText(sel.key, sizeof(sel.key), "spawnflags");
            sel.value = e->v.spawnflags;
        }
    }
    sel.valid = true;
    copyText(sel.model, sizeof(sel.model), PR_GetString(e->v.model));
    sel.skin = static_cast<int>(e->v.skin);
    sel.frame = static_cast<int>(e->v.frame);
    if(!strcmp(cls, "light_torch_small_walltorch"))
    {
        copyText(sel.model, sizeof(sel.model), "progs/flame.mdl"); // (its ghost id's torch, upright: not our stick)
        sel.skin = 0;
        sel.frame = 0;
    }
    q_snprintf(sel.label, sizeof(sel.label), "%s (picked)", sel.classname);
    Con_Printf("toolgun: picked %s (%s)\n", sel.classname, sel.model);
}

// The ghost is showing: the spawn tool in hand, something chosen, the aim known, not picking it from the world. Its
// hold (ghostHold) is only for this: A/X elsewhere is the sticks' as ever.
[[nodiscard]] bool ghostShowing()
{
    return tg.hand >= 0 && tg.aim.valid && tg.sel.valid && !tg.picking && currentTool() == Tool::Spawn;
}

// A wall torch's origin (its cup) off its wall: id's maps hang theirs 6-12 units off it (e1m2's), most about 10.
constexpr float wallStandoff = 10.f;

// The aim meets a wall (the world's or a brush entity's, such as a func_wall: not a thing before it; not a floor or a
// ceiling) and the choice hangs on one (a wall torch).
[[nodiscard]] bool wallAim(const Aim& a)
{
    const bool thingFirst = a.target && a.target != a.hitEntity && a.targetDistance < a.worldDistance;
    return tg.sel.wall && !thingFirst && a.normal != glm::vec3{0.f} && za::fabs(a.normal.z) < 0.7f;
}

// The point the ghost stands on before its offsets: what the gun points at (lifted onto a floor; out from a wall or a
// ceiling; a wall torch hung on the wall).
[[nodiscard]] glm::vec3 ghostPoint(const Aim& a)
{
    if(wallAim(a))
    {
        return a.end + glm::normalize(glm::vec3{a.normal.x, a.normal.y, 0.f}) * wallStandoff;
    }
    const float distance = a.target ? za::min(a.targetDistance, a.worldDistance) : a.worldDistance;
    glm::vec3 point = a.from + a.dir * distance;
    const glm::vec3 normal = a.target && a.targetDistance < a.worldDistance ? -a.dir : a.normal;
    if(normal.z > 0.7f)
    {
        point.z += tg.sel.lift + 1.f;
    }
    else if(normal != glm::vec3{0.f})
    {
        point += normal * 16.f; // (off a wall or a ceiling: its box out of it, near enough)
    }
    return point;
}

// The aim's way the ghost goes by: the aim's, or the one it had when the turn's hold fixed the ghost.
[[nodiscard]] const glm::vec3& ghostDir()
{
    return ghostHold.fixed ? ghostHold.dir : tg.aim.dir;
}

void probeLook();
void removeTarget(int num, bool quiet = false);
void traceFrame(); // (vr_toolgun_spawn_trace)

// Where the ghost stands: on ghostPoint (or where it was fixed), moved by the offsets along the gun's level way, facing
// you, turned by the turn.
void updateGhost()
{
    tg.ghost = false;
    if(!ghostShowing())
    {
        return;
    }
    const glm::vec3 point = ghostHold.fixed ? ghostHold.point : ghostPoint(tg.aim);
    const glm::vec3& dir = ghostDir();
    glm::vec3 fwd, right;
    levelAxes(dir, fwd, right);
    tg.ghostOrigin = point + fwd * tg.offset.x + right * tg.offset.y + glm::vec3{0.f, 0.f, tg.offset.z};
    tg.ghostAngles = glm::vec3{tg.turn.x, yawOf(dir) + 180.f + tg.turn.y, tg.turn.z};
    if(!ghostHold.fixed && wallAim(tg.aim))
    {
        tg.ghostAngles.y = yawOf(tg.aim.normal) + tg.turn.y; // a wall torch faces out of its wall
    }
    tg.ghost = true;
    probeLook();
    if(tg.sel.armed)
    {
        armedRefresh();
        armedGhostRefresh();
    }
}

// Whether the choice is an item (the Items tab's, or a picked item_): its ghost is drawn from a probe (GhostLook).
[[nodiscard]] bool probedChoice(const Selection& sel)
{
    return sel.valid && (sel.entry >= 0 ? !strcmp(spawnables[sel.entry].section, "Items") : !strncmp(sel.classname, "item_", 5));
}

// The ghost's look for the choice (GhostLook), probed once: the choice made where the ghost stands on a floor, aimed at
// directly (not held, no offsets: its drop is then the placed one's), read and removed at once.
void probeLook()
{
    const Selection& sel = tg.sel;
    GhostLook& look = ghostLook;
    if(!probedChoice(sel) || !sv.active)
    {
        return;
    }
    if(look.entry != sel.entry || strcmp(look.classname, sel.classname) != 0)
    {
        look = {};
        look.entry = sel.entry;
        copyText(look.classname, sizeof(look.classname), sel.classname);
    }
    const bool onFloor = !ghostHold.fixed && tg.offset == glm::vec3{0.f} && tg.aim.normal.z > 0.7f &&
                         !(tg.aim.target && tg.aim.targetDistance < tg.aim.worldDistance);
    if(look.valid || look.tries >= 3 || !onFloor || (sel.entry >= 0 && !toolgun::spawnableAvailable(sel.entry)))
    {
        return;
    }
    look.tries++;
    edict_t* e = box3d::spawnClass(sel.classname, tg.ghostOrigin, tg.ghostAngles, sel.setModel ? sel.model : nullptr,
        sel.key[0] ? sel.key : nullptr, sel.value, true, sel.weaponFlags);
    if(!e)
    {
        return;
    }
    const progs::ServerVm vm;
    const char* model = PR_GetString(e->v.model);
    if(model[0])
    {
        look.valid = true;
        copyText(look.model, sizeof(look.model), model);
        look.skin = static_cast<int>(e->v.skin);
        look.frame = static_cast<int>(e->v.frame);
        look.delta = geom::vec(e->v.origin) - tg.ghostOrigin;
        look.turn = geom::vec(e->v.angles) - tg.ghostAngles;
        look.vr.scale = fieldVec(e, fields().model_scale);
        look.vr.scaleOrigin = fieldVec(e, fields().model_scale_origin);
        look.vr.offset = fieldVec(e, fields().model_offset);
    }
    removeTarget(NUM_FOR_EDICT(e), true);
}

// The Armed Humanoid page's armour (vr_toolgun_armed_armor, _pieces) on the armed monster just spawned (QC
// VR_AgArmor_Toolgun: None strips the skill's roll too).
void armedArmour(edict_t* e)
{
    if(!sv.active || !e)
    {
        return;
    }
    const progs::ServerVm vm;
    const func_t fn = qvr::progs::findFunction("VR_AgArmor_Toolgun");
    if(!fn || e->free)
    {
        return;
    }
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(EDICT_NUM(1));
    G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
    G_FLOAT(OFS_PARM1) = static_cast<float>(za::clamp(static_cast<int>(vr_toolgun_armed_armor.value), 0, 4));
    G_FLOAT(OFS_PARM2) = static_cast<float>(za::clamp(static_cast<int>(vr_toolgun_armed_pieces.value), 0, 2));
    PR_ExecuteProgram(fn);
}

[[nodiscard]] edict_t* spawnChoice(const Selection& sel, const glm::vec3& origin, const glm::vec3& angles)
{
    edict_t* e = box3d::spawnClass(sel.classname, origin, angles, sel.setModel ? sel.model : nullptr, sel.key[0] ? sel.key : nullptr,
        sel.value, false, sel.weaponFlags, sel.key2[0] ? sel.key2 : nullptr, sel.value2);
    if(e && sel.armed)
    {
        armedArmour(e);
    }
    return e;
}

void spawnHere()
{
    if(!tg.ghost)
    {
        Con_Printf("toolgun: nothing to spawn (choose it on the panel, or pick it)\n");
        return;
    }
    const Selection& sel = tg.sel;
    if(sel.entry >= 0 && !toolgun::spawnableAvailable(sel.entry)) { return; } // AD support
    edict_t* e = spawnChoice(sel, tg.ghostOrigin, tg.ghostAngles);
    if(e)
    {
        const glm::vec3 off = fieldVec(e, fields().model_offset);
        Con_Printf("toolgun: spawned %d %s at %.0f %.0f %.0f\n", NUM_FOR_EDICT(e), sel.classname, e->v.origin[0],
            e->v.origin[1], e->v.origin[2]);
        Con_DPrintf("toolgun: placed %s at %.1f %.1f %.1f, scale %.3f, offset %.1f %.1f %.1f\n", PR_GetString(e->v.model),
            e->v.origin[0], e->v.origin[1], e->v.origin[2], 1.f + fieldVec(e, fields().model_scale).x, off.x, off.y, off.z);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Remove.

void removeTarget(int num, bool quiet)
{
    const func_t fn = qvr::progs::findFunction("VR_Toolgun_Remove");
    if(!fn || num <= svs.maxclients)
    {
        return;
    }
    edict_t* e = EDICT_NUM(num);
    char name[64];
    copyText(name, sizeof(name), PR_GetString(e->v.classname));
    box3d::toolForget(num);
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(EDICT_NUM(1));
    G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
    PR_ExecuteProgram(fn);
    if(!quiet)
    {
        Con_Printf("toolgun: removed %d %s\n", num, name);
    }
}
