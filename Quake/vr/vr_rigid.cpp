// vr_rigid.cpp -- what the rigid bodies (entities whose QC sets .vr_rigid: thrown weapons, ammo and health
// boxes, backpacks, armour, gibs, heads) need around Box3D (vr_box3d.cpp), which moves them:
// - VR_RigidToss, SV_Physics_Toss's hook: items and rigid bodies are kept in the world (keepInWorld), the water
//   transition of what Quake still moves is seen first, and a rigid body is handed to Box3D.
// - The drawn model's box (localBox), for the hands (pointInModelBox, modelCentre) and keepInWorld.
// - Held objects' angles (carryAngles), and the vr_rigid_place test command.
//
// (Until the simplification of 2026-09-28 this was also Quake VR's own solver, vr_physics_engine 0: each body an
// oriented box colliding by its corners, never with the others. It is kept on the branch
// archive/old-solver-stacking; docs/vr-port/ROUND21.md, "Simplification: Box3D only".)

#include "vr_box3d.hpp"
#include "vr_carry2h.hpp"
#include "vr_engine.hpp"
#include "vr_held.hpp"
#include "vr_physics.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"

#include <unordered_map>
#include <vector>

using namespace qvr;
using namespace qvr::progs;

namespace
{

[[nodiscard]] glm::vec3 toGlm(const vec3_t v)
{
    return {v[0], v[1], v[2]};
}

void fromGlm(const glm::vec3& g, vec3_t v)
{
    v[0] = g.x;
    v[1] = g.y;
    v[2] = g.z;
}

[[nodiscard]] qmodel_t* modelOf(edict_t* ent)
{
    const int index = static_cast<int>(ent->v.modelindex);
    return index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
}

// Whether `ent` is drawn as a brush model (the ammo and health boxes, maps/b_*.bsp).
[[nodiscard]] bool brushModel(edict_t* ent)
{
    const qmodel_t* model = modelOf(ent);
    return model && model->type == mod_brush;
}

// Model axes of an entity's angles as the renderer turns it (brush and alias models differ in
// pitch), and back: see vr_held.hpp.
[[nodiscard]] glm::mat3 axesFromAngles(const vec3_t angles, bool brush)
{
    return held::axesFromAngles(angles, brush);
}

void anglesFromAxes(const glm::mat3& m, vec3_t out, bool brush)
{
    held::anglesFromAxes(m, out, brush);
}

[[nodiscard]] glm::mat3 orthonormalize(const glm::mat3& m)
{
    const glm::vec3 x = glm::normalize(m[0]);
    const glm::vec3 y = glm::normalize(m[1] - x * glm::dot(x, m[1]));
    return glm::mat3{x, y, glm::cross(x, y)};
}

// The box the model is drawn in, in its axes, relative to the entity's origin (as vr_render.cpp
// transforms alias models: the networked scale about model_scale_origin, the weapon scaling,
// then the model's own, the post scale and the networked offset on raw vertices).
void localBox(edict_t* ent, glm::vec3& lo, glm::vec3& hi)
{
    qmodel_t* model = modelOf(ent);
    if(!model || model->type != mod_alias)
    {
        lo = toGlm(ent->v.mins);
        hi = toGlm(ent->v.maxs);
        return;
    }

    const FieldOffsets& f = fields();
    held::modelBox(model, fieldVec(ent, f.model_scale), fieldVec(ent, f.model_scale_origin), fieldVec(ent, f.model_offset), lo, hi);

    // Never thinner than a unit, so that the corners span a volume.
    const glm::vec3 centre = (lo + hi) * 0.5f;
    const glm::vec3 half = glm::max((hi - lo) * 0.5f, glm::vec3{0.5f});
    lo = centre - half;
    hi = centre + half;
}

// Items must never fall out of the world. Quake lets an entity whose box is buried in the level
// (a trace that starts and ends in solid) move freely, and it falls forever: a small ammo box
// clips with the player-sized hull, which a force grab that ends at a hand near a wall or a low
// ceiling can bury. A rigid body whose centre is inside a wall falls through Box3D's world mesh
// the same way (the mesh has no inside). So the last place each item or rigid body was free is
// kept, and one found buried goes back there, still.
struct FreePlace
{
    glm::vec3 origin{0.f};
    bool valid = false;
};
std::vector<FreePlace> freePlaces; // by entity number, for this server

[[nodiscard]] bool buried(edict_t* ent, bool rigid)
{
    if(rigid)
    {
        glm::vec3 lo, hi;
        localBox(ent, lo, hi);
        const glm::vec3 centre = toGlm(ent->v.origin) + axesFromAngles(ent->v.angles, brushModel(ent)) * ((lo + hi) * 0.5f);
        vec3_t c{centre.x, centre.y, centre.z};
        return SV_PointContents(c) == CONTENTS_SOLID;
    }
    const trace_t tr = SV_Move(ent->v.origin, ent->v.mins, ent->v.maxs, ent->v.origin, MOVE_NOMONSTERS, ent);
    return tr.allsolid;
}

void keepInWorld(edict_t* ent, bool rigid)
{
    const bool item = (static_cast<int>(ent->v.flags) & (FL_ITEM | physics::FL_FORCEGRABBABLE)) != 0;
    if(!rigid && !item)
    {
        return;
    }

    const int num = NUM_FOR_EDICT(ent);
    if(num >= static_cast<int>(freePlaces.size()))
    {
        freePlaces.resize(static_cast<size_t>(num) + 64);
    }
    FreePlace& place = freePlaces[num];

    // Resting where it was last found free: whether it is buried changes nothing (buried or not, it
    // stays and its free place is this one), so it is not asked (a trace every frame for every item).
    if(place.valid && (static_cast<int>(ent->v.flags) & FL_ONGROUND) && toGlm(ent->v.origin) == place.origin)
    {
        return;
    }
    if(!buried(ent, rigid))
    {
        place.origin = toGlm(ent->v.origin);
        place.valid = true;
        return;
    }
    // Resting on the ground it does not move (Quake skips it): only one moving is at risk.
    if(!place.valid || (static_cast<int>(ent->v.flags) & FL_ONGROUND))
    {
        return;
    }
    // Already put back there and still buried (a door or a lift moved in): moving it back again would
    // loop every frame. Leave it to fall or be moved.
    if(toGlm(ent->v.origin) == place.origin)
    {
        place.valid = false;
        return;
    }
    Con_DPrintf("VR: %s buried at %.0f %.0f %.0f, back to %.0f %.0f %.0f\n", PR_GetString(ent->v.classname), ent->v.origin[0],
        ent->v.origin[1], ent->v.origin[2], place.origin.x, place.origin.y, place.origin.z);
    fromGlm(place.origin, ent->v.origin);
    VectorCopy(vec3_origin, ent->v.velocity);
    VectorCopy(vec3_origin, ent->v.avelocity);
    if(rigid)
    {
        setFieldVec(ent, fields().vr_spin, glm::vec3{0.f}); // put back still, not spinning on
    }
    SV_LinkEdict(ent, false);
}

// Held objects (QC's carryangles): the object keeps the turn it had in the hand when gripped. At
// the grip (`grab`), the object's rotation relative to the hand's is kept; after, the object's
// angles are the hand's rotation times that, in the object's own convention (brush or alias).
std::unordered_map<int, glm::mat3> carried; // entity -> its axes in the hand's frame

// For physics tests: "vr_rigid_place <entity> <x> <y> <z> [<pitch> <yaw> <roll> [<vx> <vy> <vz> [<sx> <sy> <sz>]]]"
// puts a rigid body there, turned so, moving so and spinning so (radians per second), awake;
// "vr_rigid_place <entity> main|off [<forward> <left> <up> [<pitch> <yaw> <roll>]]" at the first player's hand
// (and so far from it along its axes), still. <entity>: its number, a classname (the newest), or "new" (the
// newest rigid body: a backpack just dropped).
void place_f()
{
    if(!sv.active || Cmd_Argc() < 3)
    {
        Con_Printf("usage: vr_rigid_place <number | classname | new> <x> <y> <z> [<pitch> <yaw> <roll> [<vx> <vy> <vz> [<sx> <sy> <sz>]]]\n"
                   "       vr_rigid_place <number | classname | new> main|off [<forward> <left> <up> [<pitch> <yaw> <roll>]]\n");
        return;
    }
    qcvm_t* oldVm = nullptr;
    PR_PushQCVM(&sv.qcvm, &oldVm);
    struct Restore
    {
        qcvm_t* vm;
        ~Restore() { PR_PopQCVM(vm); }
    } restore{oldVm};
    const FieldOffsets& f = fields();
    edict_t* ent = nullptr;
    const char* which = Cmd_Argv(1);
    if(which[0] >= '0' && which[0] <= '9')
    {
        const int num = Q_atoi(which);
        ent = num > 0 && num < qcvm->num_edicts ? EDICT_NUM(num) : nullptr;
    }
    else
    {
        for(int i = qcvm->num_edicts - 1; i > 0 && !ent; i--)
        {
            edict_t* e = EDICT_NUM(i);
            const bool newest = !strcmp(which, "new") && f.vr_rigid >= 0 && fieldFloat(e, f.vr_rigid) != 0.f;
            if(!e->free && (newest || !strcmp(PR_GetString(e->v.classname), which)))
            {
                ent = e;
            }
        }
    }
    if(!ent || ent->free)
    {
        Con_Printf("vr_rigid_place: no such entity\n");
        return;
    }
    const auto arg = [](int i) { return i < Cmd_Argc() ? Q_atof(Cmd_Argv(i)) : 0.f; };
    if(const bool main = !strcmp(Cmd_Argv(2), "main"); main || !strcmp(Cmd_Argv(2), "off"))
    {
        // At the first player's hand, and so far along its forward, left and up.
        edict_t* player = EDICT_NUM(1);
        const glm::vec3 angles = fieldVec(player, main ? f.handrot : f.offhandrot);
        const glm::vec3 at = fieldVec(player, main ? f.handpos : f.offhandpos) +
                             held::axesFromAngles(&angles[0], true) * glm::vec3{arg(3), arg(4), arg(5)};
        fromGlm(at, ent->v.origin);
        if(Cmd_Argc() >= 9)
        {
            for(int i = 0; i < 3; i++)
            {
                ent->v.angles[i] = arg(6 + i);
            }
        }
        VectorCopy(vec3_origin, ent->v.velocity);
        setFieldVec(ent, f.vr_spin, glm::vec3{0.f});
        SV_LinkEdict(ent, false);
        Con_Printf("vr_rigid_place: %d %s at the %s hand\n", NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname), main ? "main" : "off");
        return;
    }
    if(Cmd_Argc() < 5)
    {
        Con_Printf("vr_rigid_place: where?\n");
        return;
    }
    for(int i = 0; i < 3; i++)
    {
        ent->v.origin[i] = arg(2 + i);
        ent->v.angles[i] = arg(5 + i);
        ent->v.velocity[i] = arg(8 + i);
    }
    setFieldVec(ent, f.vr_spin, glm::vec3{arg(11), arg(12), arg(13)});
    ent->v.flags = static_cast<float>(static_cast<int>(ent->v.flags) & ~FL_ONGROUND);
    SV_LinkEdict(ent, false);
    Con_Printf("vr_rigid_place: %d %s\n", NUM_FOR_EDICT(ent), PR_GetString(ent->v.classname));
}

} // namespace

// SV_Physics_Toss, after the think. Items and rigid bodies are kept in the world first; a rigid body is then
// Box3D's (nonzero: Quake's toss leaves it), and everything else goes on with Quake's toss (zero).
extern "C" int VR_RigidToss(edict_t* ent)
{
    QVR_PROFILE("rigid bodies");
    if(static bool registered = false; !registered) // a test command (no init hook of its own)
    {
        registered = true;
        Cmd_AddCommand("vr_rigid_place", place_f);
    }
    const FieldOffsets& f = fields();
    const bool rigid = f.vr_rigid >= 0 && fieldFloat(ent, f.vr_rigid) != 0.f;
    keepInWorld(ent, rigid);

    // Its first move: SV_CheckWaterTransition takes the contents it ends in as where it spawned, so a
    // rocket or a nail fired into water from close by never went in (no splash, vr_physics.cpp). One
    // starting in the open is in the open.
    if(ent->v.watertype == 0.f && SV_PointContents(ent->v.origin) == CONTENTS_EMPTY)
    {
        ent->v.watertype = CONTENTS_EMPTY;
    }

    const int movetype = static_cast<int>(ent->v.movetype);
    if(!rigid || (movetype != MOVETYPE_TOSS && movetype != MOVETYPE_BOUNCE))
    {
        physics::predictWaterEntry(ent); // its splash, if it goes into water in this move
        return 0;
    }

    // Box3D moves it, with the others, at the frame's end (VR_PhysicsFrameEnd; its water transitions too).
    return box3d::toss(ent) ? 1 : 0;
}

namespace qvr::physics
{

void carryAngles(edict_t* ent, const float* handAngles, bool grab, float* out)
{
    const bool brush = brushModel(ent);
    const glm::mat3 hand = axesFromAngles(handAngles, true); // view angles: pitch as a brush model's
    const int num = NUM_FOR_EDICT(ent);
    const auto it = carried.find(num);
    if(grab || it == carried.end())
    {
        carried[num] = glm::transpose(hand) * axesFromAngles(ent->v.angles, brush);
        VectorCopy(ent->v.angles, out);
        return;
    }
    anglesFromAxes(orthonormalize(hand * it->second), out, brush);
}

bool pointInModelBox(edict_t* ent, const glm::vec3& p, float margin)
{
    glm::vec3 lo, hi;
    localBox(ent, lo, hi);
    const glm::mat3 axes = axesFromAngles(ent->v.angles, brushModel(ent));
    const glm::vec3 local = glm::transpose(axes) * (p - toGlm(ent->v.origin));
    // Thin things (a dropped gun) at least 6 units thick, so a hand can still find them.
    const glm::vec3 half = glm::max((hi - lo) * 0.5f, glm::vec3{3.f}) + glm::vec3{margin};
    return glm::all(glm::lessThanEqual(glm::abs(local - (lo + hi) * 0.5f), half));
}

glm::vec3 modelCentre(edict_t* ent)
{
    if(!modelOf(ent))
    {
        return toGlm(ent->v.origin) + (toGlm(ent->v.mins) + toGlm(ent->v.maxs)) * 0.5f;
    }
    glm::vec3 lo, hi;
    localBox(ent, lo, hi);
    return toGlm(ent->v.origin) + axesFromAngles(ent->v.angles, brushModel(ent)) * ((lo + hi) * 0.5f);
}

void resetRigidBodies()
{
    freePlaces.clear();
    carried.clear();
    carry2h::resetServer();
}

} // namespace qvr::physics
