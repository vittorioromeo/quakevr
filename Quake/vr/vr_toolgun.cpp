// vr_toolgun.cpp -- the toolgun (vr_toolgun.hpp; docs/vr-port/TOOLGUN.md; QC vr_toolgun.qc): a debug and sandbox tool
// held as a pistol, as Garry's Mod's. Its hand's trigger uses the tool chosen on its menu (vr_toolgun_tool) on what it
// points at:
//   Spawn    a ghost of the thing chosen (the menu's list, or what the picker shot) where the gun points; the trigger
//            makes it there (its spawn function, as a map would: box3d::spawnClass);
//   Remove   what it points at glows red; the trigger removes it (QC VR_Toolgun_Remove: as the scene's cleanup does);
//   Physgun  the trigger held drags what it points at at its distance (a prop pinned: a kinematic body following the
//            beam, pushing the others; a monster or a pickup moved), let go it falls (thrown by the beam's swing); the
//            other hand's trigger meanwhile freezes it there (pinned until the physgun takes it again);
//   Scale    a prop's box drawn with handles on its faces and corners: aim at one, hold the trigger and pull or push it
//            (proportionally, or along that face's axis with Proportional off);
//   Joint    two props shot one after the other are joined (box3d::addToolJoint: a weld, a ball, a hinge, a slider, a
//            rope, a spring); Unjoin removes a prop's joints.
// X/A held on the toolgun's hand: the sticks move the ghost or what the physgun holds (the gun's stick forward and back
// and turns it, the other up, down and sideways), or scale the aimed prop (the gun's stick, up and down); no walking or
// turning meanwhile (vr_input.cpp). B/Y opens and closes the menu on the gun (vr_panel.cpp draws the VR Settings' page
// there; vr_menu_toolgun.inc's pages). Single player: the listen server's edicts, changed between its frames (as a console
// command does).

#include "vr_toolgun.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <string.h>

using namespace qvr;
using namespace qvr::progs;
using qvr::toolgun::Tool;

namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// The spawn list (the menu's Toolgun - Spawn page): what is made, how it looks as a ghost, how high above the floor
// its origin stands (the box's bottom below it), and a float field set before its spawn function.

struct Spawnable
{
    const char* section;
    const char* label;
    const char* classname;
    const char* model;
    float lift;
    const char* key;
    float value;
    int skin;
    bool setModel; // the model is given to its spawn function (a rock's, a brick's)
};

constexpr Spawnable spawnables[] = {
    {"Monsters", "Grunt", "monster_army", "progs/soldier.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Enforcer", "monster_enforcer", "progs/enforcer.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Rottweiler", "monster_dog", "progs/dog.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Zombie", "monster_zombie", "progs/zombie.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Knight", "monster_knight", "progs/knight.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Death Knight", "monster_hell_knight", "progs/hknight.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Ogre", "monster_ogre", "progs/ogre.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Scrag", "monster_wizard", "progs/wizard.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Fiend", "monster_demon1", "progs/demon.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Spawn", "monster_tarbaby", "progs/tarbaby.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Vore", "monster_shalrath", "progs/shalrath.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Monsters", "Shambler", "monster_shambler", "progs/shambler.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Props", "Wooden Crate", "vr_crate", "progs/vr_crate1.mdl", 0.f, nullptr, 0.f, 0, false},
    {"Props", "Large Crate", "vr_crate", "progs/vr_crate2.mdl", 0.f, "spawnflags", 1.f, 0, false},
    {"Props", "Barrel", "vr_barrel", "progs/vr_barrel.mdl", 16.f, nullptr, 0.f, 0, false},
    {"Props", "Explosive Box", "misc_explobox", "maps/b_explob.bsp", 0.f, nullptr, 0.f, 0, false},
    {"Props", "Small Explosive Box", "misc_explobox2", "maps/b_exbox2.bsp", 0.f, nullptr, 0.f, 0, false},
    {"Props", "Rock", "vr_debris_piece", "progs/vr_rock1.mdl", 0.f, nullptr, 0.f, 0, true},
    {"Props", "Brick", "vr_debris_piece", "progs/vr_brick0.mdl", 0.f, nullptr, 0.f, 0, true},
    {"Weapons", "Axe", "func_weapon_grabbable", "progs/v_axe.mdl", 8.f, "weapon", 2.f, 0, false},
    {"Weapons", "Crowbar", "func_weapon_grabbable", "progs/v_crowbar.mdl", 8.f, "weapon", 17.f, 0, false},
    {"Weapons", "Knight's Sword", "func_weapon_grabbable", "progs/v_ksword.mdl", 8.f, "weapon", 13.f, 0, false},
    {"Weapons", "Shotgun", "func_weapon_grabbable", "progs/v_shot.mdl", 8.f, "weapon", 4.f, 0, false},
    {"Weapons", "Super Shotgun", "func_weapon_grabbable", "progs/v_shot2.mdl", 8.f, "weapon", 5.f, 0, false},
    {"Weapons", "Nailgun", "func_weapon_grabbable", "progs/v_nail.mdl", 8.f, "weapon", 6.f, 0, false},
    {"Weapons", "Super Nailgun", "func_weapon_grabbable", "progs/v_nail2.mdl", 8.f, "weapon", 7.f, 0, false},
    {"Weapons", "Grenade Launcher", "func_weapon_grabbable", "progs/v_rock.mdl", 8.f, "weapon", 8.f, 0, false},
    {"Weapons", "Rocket Launcher", "func_weapon_grabbable", "progs/v_rock2.mdl", 8.f, "weapon", 10.f, 0, false},
    {"Weapons", "Thunderbolt", "func_weapon_grabbable", "progs/v_light.mdl", 8.f, "weapon", 11.f, 0, false},
    {"Weapons", "Grunt's Shotgun", "func_weapon_grabbable", "progs/v_gruntgun.mdl", 8.f, "weapon", 15.f, 0, false},
    {"Weapons", "Enforcer's Rifle", "func_weapon_grabbable", "progs/v_enfrifle.mdl", 8.f, "weapon", 16.f, 0, false},
    {"Weapons", "Chainsaw", "func_weapon_grabbable", "progs/v_chainsaw.mdl", 8.f, "weapon", 14.f, 0, false},
    {"Weapons", "Toolgun", "func_weapon_grabbable", "progs/v_toolgun.mdl", 8.f, "weapon", 19.f, 0, false},
    {"Items", "Health", "item_health", "maps/b_bh25.bsp", 0.f, nullptr, 0.f, 0, false},
    {"Items", "Megahealth", "item_health", "maps/b_bh100.bsp", 0.f, "spawnflags", 2.f, 0, false},
    {"Items", "Green Armour", "item_armor1", "progs/armor.mdl", 0.f, nullptr, 0.f, 0, false},
    {"Items", "Yellow Armour", "item_armor2", "progs/armor.mdl", 0.f, nullptr, 0.f, 1, false},
    {"Items", "Red Armour", "item_armorInv", "progs/armor.mdl", 0.f, nullptr, 0.f, 2, false},
    {"Items", "Shells", "item_shells", "maps/b_shell0.bsp", 0.f, nullptr, 0.f, 0, false},
    {"Items", "Nails", "item_spikes", "maps/b_nail0.bsp", 0.f, nullptr, 0.f, 0, false},
    {"Items", "Rockets", "item_rockets", "maps/b_rock0.bsp", 0.f, nullptr, 0.f, 0, false},
    {"Items", "Cells", "item_cells", "maps/b_batt0.bsp", 0.f, nullptr, 0.f, 0, false},
    {"Items", "Quad Damage", "item_artifact_super_damage", "progs/quaddama.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Items", "Pentagram", "item_artifact_invulnerability", "progs/invulner.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Items", "Ring of Shadows", "item_artifact_invisibility", "progs/invisibl.mdl", 24.f, nullptr, 0.f, 0, false},
    {"Items", "Biosuit", "item_artifact_envirosuit", "progs/suit.mdl", 24.f, nullptr, 0.f, 0, false},
};
constexpr int spawnableTotal = static_cast<int>(sizeof(spawnables) / sizeof(spawnables[0]));

constexpr const char* toolNames[] = {"Spawn", "Remove", "Physgun", "Scale", "Joint"};
constexpr const char* jointNames[] = {"Weld", "Ball", "Hinge", "Slider", "Rope", "Spring", "Unjoin"};
constexpr int unjoin = static_cast<int>(box3d::ToolJoint::Count); // vr_toolgun_joint's last: removes a prop's joints

// ---------------------------------------------------------------------------------------------------------------------
// The toolgun's state (the main thread: the input, the host frame, the client's temp entities).

// What the spawn tool makes: a list entry, or what the picker shot (its class and look).
struct Selection
{
    bool valid{false};
    int entry{-1}; // the list's, -1: picked
    char label[64]{};
    char classname[64]{};
    char model[64]{};
    char key[16]{};
    float value{0.f};
    float lift{0.f};
    int skin{0};
    int frame{0};
    bool setModel{false};
};

// Where the gun points this frame.
struct Aim
{
    bool valid{false};
    glm::vec3 from{0.f}, dir{0.f};
    glm::vec3 end{0.f};    // the world's hit (or the range's end)
    glm::vec3 normal{0.f}; // the world's surface there (zero: nothing hit)
    float worldDistance{0.f};
    int target{0}; // the entity met first, before the world (0: none)
    float targetDistance{0.f};
};

// The physgun's hold.
struct Drag
{
    int num{0};
    bool prop{false};      // a rigid prop: pinned while held (its body follows)
    float distance{0.f};   // along the beam to the grab point
    glm::vec3 offset{0.f}; // the entity's origin from the grab point, as taken
    glm::vec3 angles{0.f}; // its angles as taken
    glm::vec3 slide{0.f};  // the sticks' moves since (right, up) and the turn (pitch, yaw, roll)
    glm::vec3 turn{0.f};
    glm::vec3 last{0.f};     // its origin last frame
    glm::vec3 velocity{0.f}; // eased: its throw on letting go
};

// The scale tool's gizmo on a prop: its oriented box (as drawn: model_scale about model_scale_origin) and handles.
struct Gizmo
{
    int num{0};
    glm::vec3 centre{0.f};
    glm::mat3 axes{1.f}; // the prop's (forward, left, up)
    glm::vec3 half{0.f}; // along them
    int hover{-1};       // the handle aimed at: 0..5 faces (axis * 2 + side), 6..13 corners; -1 none
    int held{-1};        // the handle the trigger holds
    glm::vec3 scale0{1.f}; // the prop's scale as the handle was taken (1 + model_scale)
    float along0{0.f};     // the handle's distance from the centre along its way then
};

struct ToolgunState
{
    Selection sel;
    glm::vec3 offset{0.f}; // the ghost's: forward (the gun's way, level), right, up (units)
    glm::vec3 turn{0.f};   // and its turn: pitch, yaw (added to facing you), roll (degrees)
    bool picking{false};   // the next shot picks the selection

    int hand{-1};            // the toolgun's hand this frame
    bool adjust{false};      // X/A held: the sticks are the tool's
    bool trigger{false};     // the toolgun's trigger held
    bool firePressed{false}; // pressed since the last frame
    bool freezePressed{false}; // the other hand's trigger, while the physgun holds something
    bool taken[HAND_COUNT][6]{}; // presses taken (their releases are too)
    glm::vec2 stick[HAND_COUNT]{glm::vec2{0.f}, glm::vec2{0.f}};

    Aim aim;
    bool ghost{false};
    glm::vec3 ghostOrigin{0.f}, ghostAngles{0.f};
    Drag drag;
    Gizmo gizmo;
    int jointFirst{0};
    glm::vec3 jointLocal{0.f}; // the first point, in the first prop's frame (its origin and axes)
    int glowNum{0};
    float glow{0.f};
    int glow2{0};
    const entity_t* ghostEnt{nullptr}; // the ghost's temp entity this frame (glowing as a hologram)
};
ToolgunState tg;

// Texts handed out by pointer (stateLine, the gun's screen): valid until the same function's next call.
struct Readouts
{
    char state[192];
    char screen[96];
    char label[64];
};
Readouts readouts;

// The server's VM while the toolgun changes its edicts (from the host frame and the console, as a command does).
struct ServerVm
{
    qcvm_t* old{nullptr};
    ServerVm() { PR_PushQCVM(&sv.qcvm, &old); }
    ~ServerVm() { PR_PopQCVM(old); }
};

[[nodiscard]] Tool currentTool()
{
    return static_cast<Tool>(za::clamp(static_cast<int>(vr_toolgun_tool.value), 0, static_cast<int>(Tool::Count) - 1));
}

[[nodiscard]] int jointKind()
{
    return za::clamp(static_cast<int>(vr_toolgun_joint.value), 0, unjoin);
}

[[nodiscard]] bool singlePlayer()
{
    return sv.active && svs.maxclients == 1 && cls.state == ca_connected;
}

void copyText(char* dst, size_t size, const char* src)
{
    q_strlcpy(dst, src ? src : "", size);
}

void select(int i)
{
    if(i < 0 || i >= spawnableTotal)
    {
        tg.sel = {};
        return;
    }
    const Spawnable& s = spawnables[i];
    Selection& sel = tg.sel;
    sel = {};
    sel.valid = true;
    sel.entry = i;
    copyText(sel.label, sizeof(sel.label), s.label);
    copyText(sel.classname, sizeof(sel.classname), s.classname);
    copyText(sel.model, sizeof(sel.model), s.model);
    copyText(sel.key, sizeof(sel.key), s.key);
    sel.value = s.value;
    sel.lift = s.lift;
    sel.skin = s.skin;
    sel.setModel = s.setModel;
    tg.picking = false;
}

// The edicts a player's hands hold (carried props: not targets).
[[nodiscard]] bool heldByPlayer(int num)
{
    edict_t* player = EDICT_NUM(1);
    for(const int ofs : {fields().mainhand_held, fields().offhand_held})
    {
        const int held = ofs >= 0 ? fieldInt(player, ofs) : 0;
        if(held > 0 && NUM_FOR_EDICT(PROG_TO_EDICT(held)) == num)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool isRigidProp(edict_t* e)
{
    return fieldFloatOr(e, fields().vr_rigid, 0.f) != 0.f && e->v.modelindex != 0.f;
}

// Where the ray `from` + t `dir` (unit) enters the box lo..hi (t >= 0), and the box's face it enters by (its normal).
[[nodiscard]] bool rayBox(const glm::vec3& from, const glm::vec3& dir, const glm::vec3& lo, const glm::vec3& hi, float& t,
    glm::vec3* normal = nullptr)
{
    float enter = 0.f, leave = 1e30f;
    int axis = -1;
    for(int k = 0; k < 3; k++)
    {
        if(za::fabs(dir[k]) < 1e-6f)
        {
            if(from[k] < lo[k] || from[k] > hi[k])
            {
                return false;
            }
            continue;
        }
        float a = (lo[k] - from[k]) / dir[k], b = (hi[k] - from[k]) / dir[k];
        if(a > b)
        {
            const float c = a;
            a = b;
            b = c;
        }
        if(a > enter)
        {
            enter = a;
            axis = k;
        }
        leave = za::min(leave, b);
        if(enter > leave)
        {
            return false;
        }
    }
    t = enter;
    if(normal)
    {
        *normal = glm::vec3{0.f};
        if(axis >= 0)
        {
            (*normal)[axis] = dir[axis] > 0.f ? -1.f : 1.f;
        }
    }
    return true;
}

// The entity the aim meets first, within `range`: its box (a little grown: thin things), not the world, the player,
// what his hands hold, nor anything not drawn. 0: none.
[[nodiscard]] int pickTarget(const glm::vec3& from, const glm::vec3& dir, float range, float& distance)
{
    int best = 0;
    distance = range;
    for(int i = svs.maxclients + 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || e->v.modelindex == 0.f || heldByPlayer(i))
        {
            continue;
        }
        const glm::vec3 lo = glm::vec3{e->v.absmin[0], e->v.absmin[1], e->v.absmin[2]} - glm::vec3{1.f};
        const glm::vec3 hi = glm::vec3{e->v.absmax[0], e->v.absmax[1], e->v.absmax[2]} + glm::vec3{1.f};
        float t = 0.f;
        if(rayBox(from, dir, lo, hi, t) && t < distance && t > 0.f)
        {
            distance = t;
            best = i;
        }
    }
    return best;
}

// The tracked controller of `hand` in the world (as the menu's laser: vr_menuui.cpp pointerRay): its place and its aim's
// axes (forward, right, up), not the drawn hand's (which follows the held weapon's weight).
[[nodiscard]] bool controllerFrame(const hands::State& s, int hand, glm::vec3& origin, glm::vec3& fwd, glm::vec3& right,
    glm::vec3& up)
{
    const TrackingState& t = tracking();
    if(!s.valid || !t.head.valid || !t.hands[hand].valid)
    {
        return false;
    }
    const auto quakeFromTracking = [](const glm::vec3& v) { return glm::vec3{-v.z, -v.x, v.y}; };
    const float turn = hands::playSpaceYaw();
    const glm::quat aim = hands::aimedController(t.hands[hand].orientation, hand);
    origin = s.head + hands::rotateYaw(quakeFromTracking(t.hands[hand].position - t.head.position) * units::metresToUnits(), turn);
    fwd = glm::normalize(hands::rotateYaw(quakeFromTracking(aim * glm::vec3{0.f, 0.f, -1.f}), turn));
    right = glm::normalize(hands::rotateYaw(quakeFromTracking(aim * glm::vec3{1.f, 0.f, 0.f}), turn));
    up = glm::normalize(hands::rotateYaw(quakeFromTracking(aim * glm::vec3{0.f, 1.f, 0.f}), turn));
    return true;
}

// The gun's level forward and right (the ghost's offsets are along them).
void levelAxes(const glm::vec3& dir, glm::vec3& fwd, glm::vec3& right)
{
    fwd = glm::vec3{dir.x, dir.y, 0.f};
    fwd = glm::length(fwd) > 1e-3f ? glm::normalize(fwd) : glm::vec3{1.f, 0.f, 0.f};
    right = glm::vec3{fwd.y, -fwd.x, 0.f};
}

[[nodiscard]] float yawOf(const glm::vec3& dir)
{
    return glm::degrees(za::atan2(dir.y, dir.x));
}

// The aim this frame: from the gun's muzzle along the drawn hand's aim (the barrel's way).
void updateAim(const hands::State& s)
{
    Aim& a = tg.aim;
    a = {};
    if(tg.hand < 0 || !s.valid)
    {
        return;
    }
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[tg.hand], fwd, right, up);
    a.valid = true;
    a.from = s.muzzleValid[tg.hand] ? s.muzzle[tg.hand] : s.pos[tg.hand];
    a.dir = fwd;
    const float range = za::max(vr_toolgun_range.value, 64.f);
    vec3_t start, end;
    for(int k = 0; k < 3; k++)
    {
        start[k] = a.from[k];
        end[k] = a.from[k] + a.dir[k] * range;
    }
    const trace_t tr = SV_Move(start, vec3_origin, vec3_origin, end, MOVE_NOMONSTERS, EDICT_NUM(1));
    a.worldDistance = range * tr.fraction;
    a.end = a.from + a.dir * a.worldDistance;
    if(tr.fraction < 1.f)
    {
        a.normal = glm::vec3{tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2]};
    }
    a.target = pickTarget(a.from, a.dir, a.worldDistance, a.targetDistance);
}

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
    q_snprintf(sel.label, sizeof(sel.label), "%s (picked)", sel.classname);
    Con_Printf("toolgun: picked %s (%s)\n", sel.classname, sel.model);
}

// Where the ghost stands: on what the gun points at (lifted onto a floor; out from a wall or a ceiling), moved by the
// offsets along the gun's level way, facing you, turned by the turn.
void updateGhost()
{
    tg.ghost = false;
    const Aim& a = tg.aim;
    if(!a.valid || !tg.sel.valid || tg.picking || currentTool() != Tool::Spawn)
    {
        return;
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
    glm::vec3 fwd, right;
    levelAxes(a.dir, fwd, right);
    tg.ghostOrigin = point + fwd * tg.offset.x + right * tg.offset.y + glm::vec3{0.f, 0.f, tg.offset.z};
    tg.ghostAngles = glm::vec3{tg.turn.x, yawOf(a.dir) + 180.f + tg.turn.y, tg.turn.z};
    tg.ghost = true;
}

void spawnHere()
{
    if(!tg.ghost)
    {
        Con_Printf("toolgun: nothing to spawn (choose it on the menu, or pick it)\n");
        return;
    }
    const Selection& sel = tg.sel;
    edict_t* e = box3d::spawnClass(sel.classname, tg.ghostOrigin, tg.ghostAngles, sel.setModel ? sel.model : nullptr,
        sel.key[0] ? sel.key : nullptr, sel.value);
    if(e)
    {
        Con_Printf("toolgun: spawned %d %s at %.0f %.0f %.0f\n", NUM_FOR_EDICT(e), sel.classname, e->v.origin[0],
            e->v.origin[1], e->v.origin[2]);
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Remove.

void removeTarget(int num)
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
    Con_Printf("toolgun: removed %d %s\n", num, name);
}

// ---------------------------------------------------------------------------------------------------------------------
// The physgun.

void dragStart(int num)
{
    edict_t* e = EDICT_NUM(num);
    Drag& d = tg.drag;
    d = {};
    d.num = num;
    d.prop = isRigidProp(e);
    d.distance = tg.aim.targetDistance;
    const glm::vec3 grab = tg.aim.from + tg.aim.dir * d.distance;
    d.offset = glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]} - grab;
    d.angles = glm::vec3{e->v.angles[0], e->v.angles[1], e->v.angles[2]};
    d.last = glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
    if(d.prop)
    {
        box3d::setPinned(num, true);
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
    const glm::quat spin = glm::angleAxis(glm::radians(d.turn.y), glm::vec3{0.f, 0.f, 1.f});
    const glm::vec3 to = grab + spin * d.offset + right * d.slide.x + glm::vec3{0.f, 0.f, d.slide.y};
    e->v.origin[0] = to.x;
    e->v.origin[1] = to.y;
    e->v.origin[2] = to.z;
    e->v.angles[0] = d.angles.x + d.turn.x;
    e->v.angles[1] = d.angles.y + d.turn.y;
    e->v.angles[2] = d.angles.z + d.turn.z;
    VectorCopy(vec3_origin, e->v.velocity);
    if(!d.prop)
    {
        e->v.flags = static_cast<float>(static_cast<int>(e->v.flags) | FL_ONGROUND); // (held up: no fall while held)
    }
    SV_LinkEdict(e, false);
    if(dt > 0.f)
    {
        const glm::vec3 v = (to - d.last) / dt;
        d.velocity += (v - d.velocity) * za::min(1.f, dt * 12.f);
    }
    d.last = to;
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
    const glm::vec3 lo = o + (glm::vec3{m->mins[0], m->mins[1], m->mins[2]} - o) * k;
    const glm::vec3 hi = o + (glm::vec3{m->maxs[0], m->maxs[1], m->maxs[2]} - o) * k;
    g.num = num;
    g.axes = held::axesFromAngles(e->v.angles, m->type == mod_brush);
    g.half = (hi - lo) * 0.5f;
    g.centre = glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]} + g.axes * ((lo + hi) * 0.5f);
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

// How far along the line from the centre through handle `h` the aim's line passes nearest it.
[[nodiscard]] float aimAlong(const Gizmo& g, int h)
{
    const glm::vec3 d = glm::normalize(g.axes * (handleLocal(h) * g.half));
    const glm::vec3 w = g.centre - tg.aim.from;
    const float b = glm::dot(d, tg.aim.dir), dd = glm::dot(d, w), de = glm::dot(tg.aim.dir, w);
    const float den = 1.f - b * b;
    if(den < 1e-4f)
    {
        return glm::dot(handlePoint(g, h) - g.centre, d);
    }
    return (b * de - dd) / den;
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
            (glm::vec3{m->mins[0], m->mins[1], m->mins[2]} + glm::vec3{m->maxs[0], m->maxs[1], m->maxs[2]}) * 0.5f);
    }
    setFieldVec(e, fields().model_scale, clamped - glm::vec3{1.f});
    // Its Quake box grown with it (its touches, the toolgun's aim), about its origin (its body is made again from the
    // drawn box: box3d's stale).
    Gizmo g;
    if(gizmoBox(num, g))
    {
        glm::vec3 lo{1e30f}, hi{-1e30f};
        for(int c = 0; c < 8; c++)
        {
            const glm::vec3 p = handlePoint(g, 6 + c) - glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]};
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
        const float along = aimAlong(g, g.held);
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
    g.along0 = aimAlong(g, g.held);
}

void drawGizmo(const Gizmo& g)
{
    if(g.num <= 0)
    {
        return;
    }
    const glm::vec4 edge{1.f, 0.85f, 0.2f, 0.8f};
    for(int a = 0; a < 8; a++)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            const int b = a | bit;
            if(b != a)
            {
                lines::line(handlePoint(g, 6 + a), handlePoint(g, 6 + b), 0.12f, edge, edge);
            }
        }
    }
    for(int h = 0; h < 14; h++)
    {
        const bool on = h == g.hover;
        const glm::vec4 c = h < 6 ? glm::vec4{0.3f, 0.9f, 1.f, 1.f} : glm::vec4{1.f, 1.f, 1.f, 1.f};
        lines::glowPoint(handlePoint(g, h), on ? 2.4f : 1.2f, on ? glm::vec4{1.f, 0.4f, 0.1f, 1.f} : c);
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
    const glm::vec3 atA = glm::vec3{first->v.origin[0], first->v.origin[1], first->v.origin[2]} +
                          held::axesFromAngles(first->v.angles, false) * tg.jointLocal;
    const auto kind = static_cast<box3d::ToolJoint>(jointKind());
    const bool made = box3d::addToolJoint(tg.jointFirst, num, kind, atA, at, normal);
    Con_Printf("toolgun: %s %d to %d: %s\n", jointNames[jointKind()], tg.jointFirst, num, made ? "joined" : "not joined (no body)");
    tg.jointFirst = 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// The sticks (X/A held).

[[nodiscard]] float deadzone(float v)
{
    return za::fabs(v) < 0.15f ? 0.f : v;
}

void sticksFrame(float dt)
{
    if(!tg.adjust || tg.hand < 0 || key_dest != key_game)
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
    default:
        tg.offset += glm::vec3{push, side, lift};
        tg.turn[axis] += turn;
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The look: the beam, the target's glow, the gun's screen.

[[nodiscard]] glm::vec4 toolColor()
{
    switch(currentTool())
    {
    case Tool::Spawn: return {0.3f, 0.9f, 1.f, 1.f};
    case Tool::Remove: return {1.f, 0.15f, 0.1f, 1.f};
    case Tool::Physgun: return {0.35f, 0.55f, 1.f, 1.f};
    case Tool::Scale: return {1.f, 0.85f, 0.2f, 1.f};
    case Tool::Joint: return {0.4f, 1.f, 0.4f, 1.f};
    default: return {1.f, 1.f, 1.f, 1.f};
    }
}

void drawLook(const hands::State& s)
{
    const Aim& a = tg.aim;
    if(!a.valid)
    {
        return;
    }
    const glm::vec4 c = toolColor();
    const float distance = tg.drag.num ? tg.drag.distance : a.target ? a.targetDistance : a.worldDistance;
    const glm::vec4 faint{c.r, c.g, c.b, 0.12f};
    lines::glow(a.from, a.from + a.dir * distance, tg.drag.num ? 0.35f : 0.12f, glm::vec4{c.r, c.g, c.b, 0.5f}, faint);
    lines::glowPoint(a.from + a.dir * distance, 0.8f, c);
    if(tg.ghost)
    {
        lines::glowPoint(tg.ghostOrigin, 1.f, c);
    }
    if(tg.jointFirst > 0 && tg.jointFirst < qcvm->num_edicts && !EDICT_NUM(tg.jointFirst)->free)
    {
        edict_t* first = EDICT_NUM(tg.jointFirst);
        const glm::vec3 atA = glm::vec3{first->v.origin[0], first->v.origin[1], first->v.origin[2]} +
                              held::axesFromAngles(first->v.angles, false) * tg.jointLocal;
        lines::line(atA, a.from + a.dir * distance, 0.15f, c, c);
    }
    if(currentTool() == Tool::Scale)
    {
        drawGizmo(tg.gizmo);
    }

    // The gun's screen: the tool and what it does, over the back of the gun, facing you.
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[tg.hand], fwd, right, up);
    const glm::vec3 at = s.pos[tg.hand] + up * 4.2f - fwd * 1.5f;
    text3d::queue(qvr::toolgun::stateLine(), at, hands::anglesFromVectors(-fwd, up), text3d::Align::Centre, 0.05f, true);
}

} // namespace

namespace qvr::toolgun
{

int heldHand()
{
    if(!singlePlayer() || cl.stats[STAT_HEALTH] <= 0)
    {
        return -1;
    }
    if(cl.stats[protocol::STAT_QVR_WEAPON] == weaponId)
    {
        return HAND_MAIN;
    }
    if(cl.stats[protocol::STAT_QVR_WEAPON2] == weaponId)
    {
        return HAND_OFF;
    }
    return -1;
}

bool menuOnGun()
{
    return menu::toolgunPageShown() && heldHand() >= 0;
}

bool menuFrame(const hands::State& s, glm::vec3& centre, glm::vec3& right, glm::vec3& up)
{
    const int hand = heldHand();
    if(hand < 0 || !menu::toolgunPageShown())
    {
        return false;
    }
    glm::vec3 origin, fwd, r, u;
    if(!controllerFrame(s, hand, origin, fwd, r, u))
    {
        return false;
    }
    // Over the gun, leaning back from upright by the tilt, facing you (its normal: right x up, back along the gun).
    const float tilt = glm::radians(vr_toolgun_menu_tilt.value);
    up = glm::normalize(u * za::cos(tilt) + fwd * za::sin(tilt));
    right = r;
    centre = origin + u * vr_toolgun_menu_up.value + up * (za::max(vr_toolgun_menu_height.value, 2.f) * 0.5f) +
             fwd * vr_toolgun_menu_forward.value;
    return true;
}

bool button(int hand, posing::Button b, bool down)
{
    const int bi = static_cast<int>(b);
    if(hand < 0 || hand >= HAND_COUNT || bi < 0 || bi >= 6)
    {
        return false;
    }
    bool& taken = tg.taken[hand][bi];
    if(!down)
    {
        if(!taken)
        {
            return false;
        }
        taken = false;
        if(b == posing::Button::Primary)
        {
            tg.adjust = false;
        }
        else if(b == posing::Button::Trigger && hand == tg.hand)
        {
            tg.trigger = false;
        }
        return true;
    }
    const int gun = heldHand();
    if(gun < 0)
    {
        return false;
    }
    tg.hand = gun;
    if(hand == gun)
    {
        switch(b)
        {
        case posing::Button::Secondary:
            if(menuOnGun())
            {
                menu::closeToolgun();
            }
            else if(key_dest == key_game)
            {
                menu::openToolgun();
            }
            taken = true;
            return true;
        case posing::Button::Primary:
            tg.adjust = key_dest == key_game;
            taken = true;
            return true;
        case posing::Button::Trigger:
            if(key_dest == key_game)
            {
                tg.trigger = true;
                tg.firePressed = true;
            }
            taken = true;
            return true;
        default: return false;
        }
    }
    // The other hand's Y, the gun in the right hand, its menu on the gun: Back (as the right hand's B is, the gun's own
    // B closing the menu).
    if(b == posing::Button::Secondary && gun == HAND_MAIN && hand == HAND_OFF && menuOnGun())
    {
        Key_Event(K_BBUTTON, true);
        Key_Event(K_BBUTTON, false);
        taken = true;
        return true;
    }
    // The other hand's trigger: what the physgun holds frozen there.
    if(b == posing::Button::Trigger && key_dest == key_game && tg.drag.num)
    {
        tg.freezePressed = true;
        taken = true;
        return true;
    }
    return false;
}

bool sticksTaken()
{
    return tg.adjust && heldHand() >= 0 && key_dest == key_game;
}

void sticks(const glm::vec2& off, const glm::vec2& main)
{
    tg.stick[HAND_OFF] = off;
    tg.stick[HAND_MAIN] = main;
}

void frame(const hands::State& s)
{
    tg.hand = heldHand();
    tg.glowNum = 0;
    tg.glow2 = 0;
    tg.ghost = false;
    if(tg.hand < 0)
    {
        // Put down (or dead, or no game): what the physgun held falls, the tool's holds end.
        if(tg.drag.num && sv.active)
        {
            const ServerVm vm;
            dragEnd(false);
        }
        tg.drag = {};
        tg.gizmo = {};
        tg.adjust = tg.trigger = tg.firePressed = tg.freezePressed = false;
        tg.aim = {};
        return;
    }
    const ServerVm vm;
    const float dt = static_cast<float>(za::clamp(host_frametime, 0.0, 0.1));
    updateAim(s);
    sticksFrame(dt);
    const bool fire = tg.firePressed;
    const bool freeze = tg.freezePressed;
    tg.firePressed = tg.freezePressed = false;
    const Aim& a = tg.aim;
    const Tool tool = currentTool();
    if(tool != Tool::Physgun && tg.drag.num)
    {
        dragEnd(false);
    }
    if(tool != Tool::Scale)
    {
        tg.gizmo = {};
    }
    if(tool != Tool::Joint)
    {
        tg.jointFirst = 0;
    }
    switch(tool)
    {
    case Tool::Spawn:
        if(tg.picking)
        {
            tg.glowNum = a.target;
            tg.glow = 0.8f;
            if(fire && a.target)
            {
                pick(a.target);
                tg.picking = false;
            }
            break;
        }
        updateGhost();
        if(fire)
        {
            spawnHere();
        }
        break;
    case Tool::Remove:
        tg.glowNum = a.target;
        tg.glow = 2.9f; // (red)
        if(fire && a.target)
        {
            removeTarget(a.target);
            updateAim(s);
        }
        break;
    case Tool::Physgun:
        if(fire && !tg.drag.num && a.target)
        {
            dragStart(a.target);
        }
        if(tg.drag.num && freeze)
        {
            if(tg.drag.prop)
            {
                dragEnd(true);
            }
            else
            {
                Con_Printf("toolgun: only physics props freeze\n");
            }
        }
        else if(tg.drag.num && !tg.trigger)
        {
            dragEnd(false);
        }
        dragFrame(dt);
        tg.glowNum = tg.drag.num ? tg.drag.num : a.target;
        tg.glow = 0.9f;
        break;
    case Tool::Scale:
        if(!tg.trigger)
        {
            tg.gizmo.held = -1;
        }
        scaleFrame();
        if(fire)
        {
            scaleGrab();
        }
        tg.glowNum = tg.gizmo.num;
        tg.glow = 0.5f;
        break;
    case Tool::Joint:
        tg.glowNum = a.target;
        tg.glow = 0.8f;
        tg.glow2 = tg.jointFirst;
        if(fire && a.target)
        {
            glm::vec3 normal{0.f, 0.f, 1.f};
            edict_t* e = EDICT_NUM(a.target);
            float t = 0.f;
            (void)rayBox(a.from, a.dir, glm::vec3{e->v.absmin[0], e->v.absmin[1], e->v.absmin[2]},
                glm::vec3{e->v.absmax[0], e->v.absmax[1], e->v.absmax[2]}, t, &normal);
            jointShot(a.target, a.from + a.dir * a.targetDistance, normal);
        }
        break;
    default: break;
    }
    if(tg.hand >= 0 && key_dest == key_game)
    {
        drawLook(s);
    }
}

void tempEntities()
{
    tg.ghostEnt = nullptr;
    if(!tg.ghost || !tg.sel.valid || !tg.sel.model[0])
    {
        return;
    }
    // The ghost: its model see-through where it would be made. A map's own models (loaded with it), or an alias model
    // loaded now; a brush model not loaded with the map (its lightmaps are made at the map's load) only as a point.
    qmodel_t* model = nullptr;
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        if(!strcmp(cl.model_precache[i]->name, tg.sel.model))
        {
            model = cl.model_precache[i];
            break;
        }
    }
    const size_t n = strlen(tg.sel.model);
    if(!model && n > 4 && !q_strcasecmp(tg.sel.model + n - 4, ".mdl"))
    {
        model = Mod_ForName(tg.sel.model, false);
    }
    if(!model)
    {
        return;
    }
    entity_t* ent = CL_NewTempEntity();
    if(!ent)
    {
        return;
    }
    for(int k = 0; k < 3; k++)
    {
        ent->origin[k] = tg.ghostOrigin[k];
        ent->angles[k] = tg.ghostAngles[k];
    }
    ent->model = model;
    ent->skinnum = tg.sel.skin;
    ent->frame = model->numframes > 0 ? za::clamp(tg.sel.frame, 0, model->numframes - 1) : 0;
    ent->alpha = static_cast<byte>(ENTALPHA_ENCODE(0.45f));
    tg.ghostEnt = ent;
}

float entityGlow(const entity_t* e)
{
    if(e && e == tg.ghostEnt)
    {
        return 0.7f; // (the ghost: a hologram in the tool's hue)
    }
    if(!e || (!tg.glowNum && !tg.glow2) || e < cl_entities || e >= cl_entities + cl.num_entities)
    {
        return 0.f;
    }
    const int num = static_cast<int>(e - cl_entities);
    if(num == tg.glowNum)
    {
        return tg.glow;
    }
    return num == tg.glow2 ? 0.6f : 0.f;
}

int spawnableCount()
{
    return spawnableTotal;
}

const char* spawnableLabel(int i)
{
    if(i < 0 || i >= spawnableTotal)
    {
        return "";
    }
    char(&text)[64] = readouts.label;
    q_snprintf(text, sizeof(text), "%s%s", tg.sel.valid && tg.sel.entry == i ? "> " : "", spawnables[i].label);
    return text;
}

const char* spawnableSection(int i)
{
    return i >= 0 && i < spawnableTotal ? spawnables[i].section : "";
}

void selectSpawnable(int i)
{
    select(i);
    Con_Printf("toolgun: spawn %s\n", tg.sel.valid ? tg.sel.label : "nothing");
}

const char* stateLine()
{
    char(&text)[192] = readouts.state;
    switch(currentTool())
    {
    case Tool::Spawn:
        if(tg.picking)
        {
            q_snprintf(text, sizeof(text), "Spawn: shoot what to pick");
        }
        else
        {
            q_snprintf(text, sizeof(text), "Spawn: %s\noffset %.0f %.0f %.0f, turn %.0f %.0f %.0f", tg.sel.valid ? tg.sel.label : "nothing chosen",
                tg.offset.x, tg.offset.y, tg.offset.z, tg.turn.x, tg.turn.y, tg.turn.z);
        }
        break;
    case Tool::Remove: q_snprintf(text, sizeof(text), "Remove"); break;
    case Tool::Physgun:
        q_snprintf(text, sizeof(text), "Physgun%s", tg.drag.num ? (tg.drag.prop ? ": holding (other trigger freezes)" : ": holding") : "");
        break;
    case Tool::Scale:
        q_snprintf(text, sizeof(text), "Scale: %s", vr_toolgun_scale_uniform.value != 0.f ? "proportional" : "per axis");
        break;
    case Tool::Joint:
        q_snprintf(text, sizeof(text), "Joint: %s%s", jointNames[jointKind()], tg.jointFirst ? " (shoot the second)" : "");
        break;
    default: text[0] = '\0'; break;
    }
    return text;
}

} // namespace qvr::toolgun

namespace
{

// vr_toolgun_select <n | clear>: the spawn list's entry n (the menu's rows), or nothing.
void select_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_toolgun_select <0..%d | clear>\n", spawnableTotal - 1);
        return;
    }
    toolgun::selectSpawnable(!q_strcasecmp(Cmd_Argv(1), "clear") ? -1 : Q_atoi(Cmd_Argv(1)));
}

void pick_f()
{
    tg.picking = !tg.picking;
    Cvar_SetValueQuick(&vr_toolgun_tool, static_cast<float>(Tool::Spawn));
    Con_Printf("toolgun: %s\n", tg.picking ? "the next shot picks what to spawn" : "picking off");
}

void reset_f()
{
    tg.offset = tg.turn = glm::vec3{0.f};
    Con_Printf("toolgun: offsets and turn reset\n");
}

void menu_f()
{
    if(toolgun::menuOnGun())
    {
        menu::closeToolgun();
    }
    else
    {
        menu::openToolgun();
    }
}

void unfreezeAll_f()
{
    Con_Printf("toolgun: unfroze %d props\n", box3d::unpinAll());
    tg.drag = {};
}

void unjoinAll_f()
{
    Con_Printf("toolgun: removed %d joints\n", box3d::removeToolJoints(0));
}

// vr_toolgun_scale_reset: the aimed prop (or the last scaled) back to its size.
void scaleReset_f()
{
    if(!sv.active)
    {
        return;
    }
    const ServerVm vm;
    const int num = tg.gizmo.num ? tg.gizmo.num : tg.aim.target;
    if(num <= 0 || num >= qcvm->num_edicts || EDICT_NUM(num)->free)
    {
        Con_Printf("toolgun: no prop to reset\n");
        return;
    }
    setScale(num, glm::vec3{1.f});
    Con_Printf("toolgun: %d back to its size\n", num);
}

// vr_toolgun_status: the tool, the hand, the aim and what is held, pinned and joined (tests).
void status_f()
{
    const ServerVm vm;
    const Aim& a = tg.aim;
    Con_Printf("toolgun: hand %d, tool %s, menu %s, adjust %d\n", toolgun::heldHand(), toolNames[static_cast<int>(currentTool())],
        toolgun::menuOnGun() ? "on the gun" : "closed", tg.adjust ? 1 : 0);
    if(a.valid)
    {
        Con_Printf("toolgun: aim from %.1f %.1f %.1f dir %.2f %.2f %.2f, world at %.0f, target %d %s at %.0f\n", a.from.x, a.from.y,
            a.from.z, a.dir.x, a.dir.y, a.dir.z, a.worldDistance, a.target,
            a.target ? PR_GetString(EDICT_NUM(a.target)->v.classname) : "-", a.targetDistance);
    }
    Con_Printf("toolgun: selection %s, ghost %s at %.0f %.0f %.0f yaw %.0f\n", tg.sel.valid ? tg.sel.label : "none",
        tg.ghost ? "shown" : "none", tg.ghostOrigin.x, tg.ghostOrigin.y, tg.ghostOrigin.z, tg.ghostAngles.y);
    if(tg.drag.num)
    {
        edict_t* e = EDICT_NUM(tg.drag.num);
        Con_Printf("toolgun: physgun holds %d at %.0f %.0f %.0f (distance %.0f)\n", tg.drag.num, e->v.origin[0], e->v.origin[1],
            e->v.origin[2], tg.drag.distance);
    }
    if(tg.gizmo.num)
    {
        const glm::vec3 k = scaleOf(EDICT_NUM(tg.gizmo.num));
        Con_Printf("toolgun: gizmo on %d, scale %.2f %.2f %.2f, handle %d (held %d)\n", tg.gizmo.num, k.x, k.y, k.z, tg.gizmo.hover,
            tg.gizmo.held);
    }
    Con_Printf("toolgun: joints %d\n", box3d::toolJointCount(0));
}

} // namespace

void qvr::toolgun::registerCommands()
{
    Cmd_AddCommand("vr_toolgun_select", select_f);
    Cmd_AddCommand("vr_toolgun_pick", pick_f);
    Cmd_AddCommand("vr_toolgun_reset", reset_f);
    Cmd_AddCommand("vr_toolgun_menu", menu_f);
    Cmd_AddCommand("vr_toolgun_unfreeze_all", unfreezeAll_f);
    Cmd_AddCommand("vr_toolgun_unjoin_all", unjoinAll_f);
    Cmd_AddCommand("vr_toolgun_scale_reset", scaleReset_f);
    Cmd_AddCommand("vr_toolgun_status", status_f);
}
