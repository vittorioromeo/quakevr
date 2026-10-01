// vr_ao.cpp -- dynamic ambient occlusion: see vr_ao.hpp.

#include "vr_ao.hpp"
#include "vr_view.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"
#include "vr_jobs.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Copysign.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Log2.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"
#include "vr_zancle.hpp"

#include <string.h>

extern "C" {
extern cvar_t r_lerpmove; // gl_rmain.c
#ifdef _WIN32
// kernel32 (declared here rather than through <windows.h>, as vr_main.cpp does)
__declspec(dllimport) void* __stdcall GetCurrentThread(void);
__declspec(dllimport) int __stdcall SetThreadPriority(void* thread, int priority);
#endif
}

using namespace qvr;

namespace
{

// ----------------------------------------------------------------------------
// The shaders' uniform block (AO_FUNCTIONS in vr_glsl.h; std140).

constexpr int MAX_OCCLUDERS = 64; // two 32-bit masks per tile
constexpr int VECS = 6;           // vec4s per occluder
constexpr int TILES = LIGHT_TILES_X * LIGHT_TILES_Y;

struct GpuBlock
{
    float params[4];                    // x occluders
    float occ[MAX_OCCLUDERS * VECS][4]; // centre + reach; 3 rows into its own space; size + kind; strength, reach, -, group
    za::U32 tiles[TILES / 2][4];  // two tiles' masks per uvec4
    za::U32 slices[LIGHT_TILES_Z / 2][4]; // two depth slices' masks per uvec4
};
static_assert(sizeof(GpuBlock) == 16 + MAX_OCCLUDERS * VECS * 16 + TILES * 8 + LIGHT_TILES_Z * 8);

constexpr float PLAYER_GROUP = 1.f;
constexpr float MAX_DISTANCE = 2048.f; // occluders farther than this from the eye are left out

struct Occluder
{
    glm::vec3 centre{0.f};
    glm::mat3 axes{1.f};    // columns: the shape's own axes (unit)
    glm::vec3 size{1.f};    // an ellipsoid's radii or a box's half sizes, along the axes
    bool box{false};
    float strength{0.f};
    float reach{0.f};       // an ellipsoid's in radii, a box's in units from its faces
    float influence{0.f};   // the sphere round the centre it reaches
    float group{0.f};
    float distance{0.f};    // from the eye, to its reach (for the choice)
    const entity_t* owner{nullptr};
};

za::Vector<Occluder> candidates;
za::Vector<Occluder> chosen;

// The choice's buffers, each frame (build: the main thread; the bake's worker has its own).
struct AoScratch
{
    za::Vector<const entity_t*> owners; // each candidate's entity
    za::Vector<int> order;              // the candidates in view, nearest first
    auto members() { return qvr::mem::list(owners, order); }
};
mem::Scratch<AoScratch> scratch{"ao"};
// The chosen occluders' models and their groups (a model's shapes share one: they don't darken the model itself), at
// most MAX_OCCLUDERS, filled each frame: a flat array searched in order (build, and each model drawn in each eye).
struct Group
{
    const entity_t* entity;
    float group;
};
za::Array<Group, MAX_OCCLUDERS> groups;
int groupCount = 0;

[[nodiscard]] float groupOf(const entity_t* e)
{
    for(int i = 0; i < groupCount; i++)
    {
        if(groups[i].entity == e)
        {
            return groups[i].group;
        }
    }
    return 0.f;
}
GpuBlock block{};
int builtFrame = -1;
double buildSeconds = 0.0; // vr_ao_show
long long buildCount = 0;

// Brush models the map's lighting saw ("_shadow"): only their movement adds occlusion. Keyed by submodel number.
za::Vector<bool> bakedSubmodels;
int bakedGeneration = -1;
ankerl::unordered_dense::map<const qmodel_t*, bool> brushDrawable; // not all liquid or sky
int brushGeneration = -1;

void parseBakedSubmodels()
{
    if(bakedGeneration == worldGeneration() && !bakedSubmodels.empty())
    {
        return;
    }
    bakedGeneration = worldGeneration();
    bakedSubmodels.clear();
    bakedSubmodels.resize(4096, false);
    const char* data = cl.worldmodel ? cl.worldmodel->entities : nullptr;
    while(data)
    {
        data = COM_Parse(data);
        if(!data || com_token[0] != '{')
        {
            break;
        }
        int submodel = -1;
        bool shadow = false;
        while(data)
        {
            data = COM_Parse(data);
            if(!data || com_token[0] == '}')
            {
                break;
            }
            char key[64];
            q_strlcpy(key, com_token, sizeof(key));
            data = COM_Parse(data);
            if(!data)
            {
                break;
            }
            if(!strcmp(key, "model") && com_token[0] == '*')
            {
                submodel = atoi(com_token + 1);
            }
            else if(!strcmp(key, "_shadow") && atof(com_token) != 0.0)
            {
                shadow = true;
            }
        }
        if(shadow && submodel > 0 && submodel < static_cast<int>(bakedSubmodels.size()))
        {
            bakedSubmodels[submodel] = true;
        }
    }
}

bool drawableBrush(const qmodel_t* m)
{
    if(brushGeneration != worldGeneration())
    {
        brushGeneration = worldGeneration();
        brushDrawable.clear();
    }
    const auto it = brushDrawable.find(m);
    if(it != brushDrawable.end())
    {
        return it->second;
    }
    bool solid = false;
    if(m->surfaces)
    {
        for(int i = 0; i < m->nummodelsurfaces && !solid; i++)
        {
            const msurface_t& s = m->surfaces[m->firstmodelsurface + i];
            solid = (s.flags & (SURF_DRAWTURB | SURF_DRAWSKY)) == 0;
        }
    }
    brushDrawable[m] = solid;
    return solid;
}

float m_sq(float x, float y, float z)
{
    return x * x + y * y + z * z;
}

glm::vec3 transformPoint(const float m[16], const glm::vec3& p)
{
    return {m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12], m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
        m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14]};
}

// A shape from a box in a model's own space (`lo` .. `hi`) through its drawn matrix `m`: its centre, unit axes, and
// half sizes in world units times `fill` per axis.
void shapeFromBox(const float m[16], const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& fill, Occluder& o)
{
    o.centre = transformPoint(m, (lo + hi) * 0.5f);
    const glm::vec3 half = (hi - lo) * 0.5f;
    for(int i = 0; i < 3; i++)
    {
        const glm::vec3 col{m[i * 4], m[i * 4 + 1], m[i * 4 + 2]};
        const float len = glm::length(col);
        o.axes[i] = len > 1e-6f ? col / len : glm::vec3{i == 0, i == 1, i == 2};
        o.size[i] = za::max(half[i] * len * fill[i], 0.25f);
    }
    // Orthonormal (a mirrored or sheared matrix would not be).
    o.axes[0] = glm::normalize(o.axes[0]);
    o.axes[1] = glm::normalize(o.axes[1] - o.axes[0] * glm::dot(o.axes[0], o.axes[1]));
    o.axes[2] = glm::cross(o.axes[0], o.axes[1]);
}

// Where the renderer draws an alias entity this frame (R_SetupEntityTransform's interpolation, read without changing
// the entity: it is drawn after the world).
void lerpedTransform(const entity_t* e, vec3_t origin, vec3_t angles)
{
    VectorCopy(e->origin, origin);
    VectorCopy(e->angles, angles);
    if(!r_lerpmove.value || e == &cl.viewent || !(e->lerpflags & LERP_MOVESTEP) || (e->lerpflags & LERP_RESETMOVE))
    {
        return;
    }
    vec3_t from, to, afrom, ato;
    float start;
    if(!VectorCompare(e->origin, e->currentorigin) || !VectorCompare(e->angles, e->currentangles))
    {
        VectorCopy(e->currentorigin, from); // a move starting this frame
        VectorCopy(e->origin, to);
        VectorCopy(e->currentangles, afrom);
        VectorCopy(e->angles, ato);
        start = static_cast<float>(cl.time);
    }
    else
    {
        VectorCopy(e->previousorigin, from);
        VectorCopy(e->currentorigin, to);
        VectorCopy(e->previousangles, afrom);
        VectorCopy(e->currentangles, ato);
        start = e->movelerpstart;
    }
    const float blend = (e->lerpflags & LERP_FINISH)
                            ? za::clamp(static_cast<float>(cl.time - start) / za::max(e->lerpfinish - start, 1e-4f), 0.f, 1.f)
                            : za::clamp(static_cast<float>(cl.time - start) / 0.1f, 0.f, 1.f);
    for(int i = 0; i < 3; i++)
    {
        origin[i] = from[i] + (to[i] - from[i]) * blend;
        float d = ato[i] - afrom[i];
        d = d > 180.f ? d - 360.f : d < -180.f ? d + 360.f : d;
        angles[i] = afrom[i] + d * blend;
    }
}

void aliasMatrix(const entity_t* e, const aliashdr_t* hdr, float m[16])
{
    vec3_t origin, angles;
    lerpedTransform(e, origin, angles);
    R_EntityMatrix(m, origin, angles, e->scale);
    VR_AliasPreTransform(e, m);
    ApplyTranslation(m, hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]);
    ApplyScale(m, hdr->scale[0], hdr->scale[1], hdr->scale[2]);
    VR_AliasPostTransform(e, m);
}

void addEllipsoid(Occluder o, float strength, float reach, float group)
{
    if(qza::maxOf(o.size.x, o.size.y, o.size.z) < 1.5f)
    {
        return;
    }
    o.box = false;
    o.strength = strength;
    o.reach = reach;
    o.influence = qza::maxOf(o.size.x, o.size.y, o.size.z) * reach;
    o.group = group;
    candidates.pushBack(o);
}

// An ellipsoid round the segment a..b, `radius` across, `extra` beyond each end.
void addLimb(const glm::vec3& a, const glm::vec3& b, float radius, float extra, float strength, float reach)
{
    const glm::vec3 d = b - a;
    const float len = glm::length(d);
    if(len < 1e-3f)
    {
        return;
    }
    Occluder o;
    o.centre = (a + b) * 0.5f;
    o.axes[0] = d / len;
    const glm::vec3 ref = qza::abs(o.axes[0].z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
    o.axes[1] = glm::normalize(glm::cross(ref, o.axes[0]));
    o.axes[2] = glm::cross(o.axes[0], o.axes[1]);
    o.size = {len * 0.5f + extra, radius, radius};
    addEllipsoid(o, strength, reach, PLAYER_GROUP);
}

// Your body (vr_avatar.cpp): the posed joints from the skinning matrices (the joint's bind position, posed, through
// the entity's matrix), one ellipsoid for the torso and one for each drawn limb; the hands from the forearms.
void addBody(const entity_t* e, const aliashdr_t* hdr, float strength, float reach)
{
    const float* skin = nullptr;
    const int count = VR_AliasBonePoses(e, &skin);
    if(!skin || count != hdr->numbones)
    {
        return;
    }
    float m[16];
    aliasMatrix(e, hdr, m);
    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
    const auto joint = [&](const char* name, glm::vec3& out, float* size) {
        for(int i = 0; i < hdr->numbones; i++)
        {
            if(strcmp(bones[i].name, name) != 0)
            {
                continue;
            }
            const float* inv = bones[i].inverse.mat; // [R^T | -R^T p]
            const glm::vec3 t{inv[3], inv[7], inv[11]};
            const glm::vec3 bind{-(inv[0] * t.x + inv[4] * t.y + inv[8] * t.z), -(inv[1] * t.x + inv[5] * t.y + inv[9] * t.z),
                -(inv[2] * t.x + inv[6] * t.y + inv[10] * t.z)};
            const float* s = skin + i * 12;
            const glm::vec3 posed{s[0] * bind.x + s[1] * bind.y + s[2] * bind.z + s[3],
                s[4] * bind.x + s[5] * bind.y + s[6] * bind.z + s[7], s[8] * bind.x + s[9] * bind.y + s[10] * bind.z + s[11]};
            out = transformPoint(m, posed);
            if(size) // a collapsed (undrawn) bone's matrix shrinks everything to a point
            {
                *size = za::sqrt(s[0] * s[0] + s[4] * s[4] + s[8] * s[8]) + za::sqrt(s[1] * s[1] + s[5] * s[5] + s[9] * s[9]);
            }
            return true;
        }
        return false;
    };
    glm::vec3 pelvis, neck, chest, ul, ur;
    float s0 = 0.f, s1 = 0.f;
    if(!joint("pelvis", pelvis, nullptr) || !joint("neck", neck, nullptr) || !joint("chest", chest, &s0) ||
       !joint("upperarm_l", ul, nullptr) || !joint("upperarm_r", ur, nullptr) || s0 < 0.1f)
    {
        return;
    }
    const float shoulders = glm::distance(ul, ur);
    // The torso: pelvis to neck, as wide as the shoulders, two thirds as deep.
    {
        Occluder o;
        const glm::vec3 up = neck - pelvis;
        const float len = glm::length(up);
        if(len > 1e-3f)
        {
            o.centre = (pelvis + neck) * 0.5f;
            o.axes[2] = up / len;
            glm::vec3 side = ul - ur;
            side -= o.axes[2] * glm::dot(side, o.axes[2]);
            o.axes[1] = glm::length(side) > 1e-3f ? glm::normalize(side) : glm::vec3{0.f, 1.f, 0.f};
            o.axes[0] = glm::cross(o.axes[1], o.axes[2]);
            o.size = {shoulders * 0.3f, shoulders * 0.48f, len * 0.6f};
            addEllipsoid(o, strength, reach, PLAYER_GROUP);
        }
    }
    // The whole standing body, neck to floor (the player's hull's bottom), fainter: the parts alone are too thin to
    // darken the floor round your feet (legs drawn or not), as a monster's one ellipsoid does round its.
    {
        const entity_t* player = &cl_entities[cl.viewentity];
        const float floorZ = player->origin[2] - 24.f;
        const float top = neck.z;
        if(top > floorZ + 8.f)
        {
            Occluder o;
            o.centre = {pelvis.x, pelvis.y, (floorZ + top) * 0.5f};
            glm::vec3 side = ul - ur;
            side.z = 0.f;
            o.axes[1] = glm::length(side) > 1e-3f ? glm::normalize(side) : glm::vec3{0.f, 1.f, 0.f};
            o.axes[2] = {0.f, 0.f, 1.f};
            o.axes[0] = glm::cross(o.axes[1], o.axes[2]);
            o.size = {shoulders * 0.6f, shoulders * 0.75f, (top - floorZ) * 0.5f};
            addEllipsoid(o, strength * 0.8f, reach, PLAYER_GROUP);
        }
    }
    // Limbs, when drawn (the legs only in the full body, vr_body_mode 3).
    struct Limb
    {
        const char *from, *to;
        float radius; // times the segment's length
    };
    static constexpr Limb limbs[] = {{"upperarm_l", "forearm_l", 0.2f}, {"forearm_l", "hand_l", 0.17f},
        {"upperarm_r", "forearm_r", 0.2f}, {"forearm_r", "hand_r", 0.17f}, {"thigh_l", "calf_l", 0.22f},
        {"calf_l", "foot_l", 0.17f}, {"thigh_r", "calf_r", 0.22f}, {"calf_r", "foot_r", 0.17f}};
    float calf = 0.f;
    for(const Limb& l : limbs)
    {
        glm::vec3 a, b;
        if(!joint(l.from, a, &s0) || !joint(l.to, b, &s1) || s0 < 0.1f)
        {
            continue;
        }
        const float len = glm::distance(a, b);
        addLimb(a, b, len * l.radius, len * 0.05f, strength, reach);
        if(l.from[0] == 'c')
        {
            calf = len;
            // The foot: a flat ellipsoid forward of the ankle (the way the body faces), its sole on the floor.
            glm::vec3 fwd = glm::cross(ul - ur, glm::vec3{0.f, 0.f, 1.f});
            fwd = glm::length(fwd) > 1e-3f ? glm::normalize(fwd) : glm::vec3{1.f, 0.f, 0.f};
            Occluder o;
            o.centre = b + fwd * (0.2f * calf) - glm::vec3{0.f, 0.f, 0.3f * calf}; // the ankle is high in the boot
            o.axes[0] = fwd;
            o.axes[2] = glm::vec3{0.f, 0.f, 1.f};
            o.axes[1] = glm::cross(o.axes[2], o.axes[0]);
            o.size = {0.34f * calf, 0.14f * calf, 0.22f * calf};
            addEllipsoid(o, strength, reach, PLAYER_GROUP);
        }
    }
    // The hands (drawn as their own models): from each wrist along the forearm.
    for(int hand = 0; hand < 2; hand++)
    {
        glm::vec3 wrist, dir;
        if(avatar::forearm(hand, wrist, dir) && glm::length(dir) > 1e-3f)
        {
            dir = glm::normalize(dir);
            addLimb(wrist, wrist + dir * (shoulders * 0.45f), shoulders * 0.12f, 0.f, strength, reach);
        }
    }
}

void addAlias(const entity_t* e, float strength, float reach)
{
    const qmodel_t* model = e->model;
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(VR_AliasBonePoses(e, nullptr))
    {
        if(!view::handBonePoses(e, nullptr)) // the jointed hands: the body's shapes stand for them
        {
            addBody(e, hdr, strength, reach);
        }
        return;
    }
    if(VR_IsViewEntity(e) || e == &cl.viewent || e == &cl_entities[cl.viewentity])
    {
        return; // hands, held and holstered weapons: the body's shapes stand for them
    }
    if((model->flags & (EF_ROCKET | EF_GRENADE | EF_TRACER | EF_TRACER2 | EF_TRACER3 | EF_ZOMGIB | MOD_NOSHADOW)) ||
       (e->alpha != ENTALPHA_DEFAULT && ENTALPHA_DECODE(e->alpha) < 0.5f))
    {
        return; // projectiles, flames, beams, see-through things
    }
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || hdr->numframes <= 0)
    {
        return;
    }
    const int frame = e->frame >= 0 && e->frame < hdr->numframes ? e->frame : 0;
    const maliasframedesc_t& f = hdr->frames[frame];
    const glm::vec3 lo{f.bboxmin.v[0], f.bboxmin.v[1], f.bboxmin.v[2]};
    const glm::vec3 hi{f.bboxmax.v[0], f.bboxmax.v[1], f.bboxmax.v[2]};
    float m[16];
    aliasMatrix(e, hdr, m);
    Occluder o;
    // Inside the frame's box, a little narrower than it (arms and weapons reach its corners); as tall, so that it
    // rests on the floor where the model stands.
    shapeFromBox(m, lo, hi, {0.8f, 0.8f, 1.f}, o);
    addEllipsoid(o, strength, reach, -1.f); // its own group, given below
}

void addBrush(const entity_t* e, float strength)
{
    const qmodel_t* model = e->model;
    if(!model || (e->alpha != ENTALPHA_DEFAULT && ENTALPHA_DECODE(e->alpha) < 1.f) || !drawableBrush(model))
    {
        return;
    }
    // A brush model the map's lighting saw where it was built: only as much as it has moved from there.
    if(model->name[0] == '*')
    {
        parseBakedSubmodels();
        const int n = atoi(model->name + 1);
        if(n > 0 && n < static_cast<int>(bakedSubmodels.size()) && bakedSubmodels[n])
        {
            strength *= za::clamp(VectorLength(e->origin) / 16.f, 0.f, 1.f);
        }
    }
    if(strength <= 0.f)
    {
        return;
    }
    vec3_t angles{-e->angles[0], e->angles[1], e->angles[2]}; // as R_InitBModelInstance
    vec3_t origin;
    VectorCopy(e->origin, origin);
    float m[16];
    R_EntityMatrix(m, origin, angles, e->scale);
    VR_BrushTransform(e, m);
    Occluder o;
    shapeFromBox(m, glm::vec3{model->mins[0], model->mins[1], model->mins[2]},
        glm::vec3{model->maxs[0], model->maxs[1], model->maxs[2]}, glm::vec3{1.f}, o);
    o.box = true;
    o.strength = strength * 0.75f; // big boxes close by fill much of a wall's hemisphere: a little lighter than the ellipsoids
    o.reach = za::clamp(qza::maxOf(o.size.x, o.size.y, o.size.z) * 0.6f, 16.f, 64.f);
    o.influence = glm::length(o.size) + o.reach;
    o.group = -1.f;
    candidates.pushBack(o);
}

// Chooses the frame's occluders (once, for both eyes): the nearest MAX_OCCLUDERS that can reach the view.
void build()
{
    const auto t0 = qza::nowNs();
    candidates.clear();
    chosen.clear();
    groupCount = 0;
    const float dynamic = za::clamp(vr_ao_dynamic.value, 0.f, 2.f);
    const float brush = za::clamp(vr_ao_brush.value, 0.f, 2.f);
    const float reach = za::clamp(vr_ao_dynamic_range.value, 1.25f, 5.f);
    if(!cl.worldmodel || (dynamic <= 0.f && brush <= 0.f))
    {
        return;
    }
    za::Vector<const entity_t*>& owners = scratch.owners;
    owners.clear();
    for(int i = 0; i < cl_numvisedicts; i++)
    {
        const entity_t* e = cl_visedicts[i];
        if(!e || !e->model)
        {
            continue;
        }
        if(e->model->type == mod_alias && dynamic > 0.f)
        {
            addAlias(e, dynamic, reach);
        }
        else if(e->model->type == mod_brush && brush > 0.f && e != &cl_entities[0])
        {
            addBrush(e, brush);
        }
        owners.resize(candidates.size(), e);
    }

    // Not behind the view (with room for the other eye), not too far.
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    vec3_t fwdv, rightv, upv;
    AngleVectors(r_refdef.viewangles, fwdv, rightv, upv);
    const glm::vec3 fwd{fwdv[0], fwdv[1], fwdv[2]};
    za::Vector<int>& order = scratch.order;
    order.clear();
    for(int i = 0; i < static_cast<int>(candidates.size()); i++)
    {
        Occluder& o = candidates[i];
        const glm::vec3 d = o.centre - eye;
        o.distance = glm::length(d) - o.influence;
        if(o.distance > MAX_DISTANCE || glm::dot(d, fwd) < -o.influence - 64.f)
        {
            continue;
        }
        order.pushBack(i);
    }
    const size_t keep = za::min(order.size(), static_cast<size_t>(MAX_OCCLUDERS));
    // (All sorted: the first `keep` are std::partial_sort's, but for the order of equal distances.)
    za::quickSort(order.begin(), order.end(), [](int a, int b) { return candidates[a].distance < candidates[b].distance; });
    float nextGroup = PLAYER_GROUP + 1.f;
    for(size_t k = 0; k < keep; k++)
    {
        Occluder o = candidates[order[k]];
        const entity_t* e = owners[order[k]];
        o.owner = e;
        if(o.group < 0.f)
        {
            o.group = groupOf(e);
            if(o.group == 0.f) // (a chosen occluder's own group is never 0: they start at PLAYER_GROUP + 1)
            {
                o.group = nextGroup++;
                groups[static_cast<za::SizeT>(groupCount++)] = {e, o.group};
            }
        }
        chosen.pushBack(o);
    }

    // The block's occluders (the tiles are each eye's).
    block.params[0] = static_cast<float>(chosen.size());
    block.params[1] = block.params[2] = block.params[3] = 0.f;
    for(size_t i = 0; i < chosen.size(); i++)
    {
        const Occluder& o = chosen[i];
        float(*v)[4] = &block.occ[i * VECS];
        v[0][0] = o.centre.x;
        v[0][1] = o.centre.y;
        v[0][2] = o.centre.z;
        v[0][3] = o.influence;
        for(int r = 0; r < 3; r++)
        {
            // An ellipsoid's rows take the point into its unit sphere, a box's into its frame.
            const float k = o.box ? 1.f : 1.f / o.size[r];
            const glm::vec3 a = o.axes[r] * k;
            v[1 + r][0] = a.x;
            v[1 + r][1] = a.y;
            v[1 + r][2] = a.z;
            v[1 + r][3] = -glm::dot(a, o.centre);
        }
        // An ellipsoid's radii squared (its normals' scale in the unit sphere's space), a box's half sizes.
        v[4][0] = o.box ? o.size.x : o.size.x * o.size.x;
        v[4][1] = o.box ? o.size.y : o.size.y * o.size.y;
        v[4][2] = o.box ? o.size.z : o.size.z * o.size.z;
        v[4][3] = o.box ? 1.f : 0.f;
        v[5][0] = o.strength;
        v[5][1] = o.reach;
        v[5][2] = 0.f;
        v[5][3] = o.group;
    }
    buildSeconds += qza::secondsSince(t0);
    buildCount++;
}

// This eye's tiles: each occluder's reach projected with the eye's view-projection (a box round it; the whole view
// when it reaches behind the eye).
void binTiles()
{
    ZA_MEMSET(block.tiles, 0, sizeof(block.tiles));
    ZA_MEMSET(block.slices, 0, sizeof(block.slices));
    const float depthScale = za::sqrt(m_sq(r_matviewproj[3], r_matviewproj[7], r_matviewproj[11])); // w per unit along the view
    const float* m = r_matviewproj;
    for(size_t i = 0; i < chosen.size(); i++)
    {
        const Occluder& o = chosen[i];
        bool culled = false;
        for(int j = 0; j < 4 && !culled; j++)
        {
            const mplane_t& p = frustum[j];
            culled = p.normal[0] * o.centre.x + p.normal[1] * o.centre.y + p.normal[2] * o.centre.z - p.dist + o.influence < 0.f;
        }
        if(culled)
        {
            continue;
        }
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        bool whole = false;
        for(int c = 0; c < 8 && !whole; c++)
        {
            const glm::vec3 p = o.centre + glm::vec3{(c & 1) ? o.influence : -o.influence,
                                               (c & 2) ? o.influence : -o.influence, (c & 4) ? o.influence : -o.influence};
            const float w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
            if(w < 1.f)
            {
                whole = true;
                break;
            }
            const float x = (m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12]) / w;
            const float y = (m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13]) / w;
            x0 = za::min(x0, x);
            x1 = za::max(x1, x);
            y0 = za::min(y0, y);
            y1 = za::max(y1, y);
        }
        int tx0 = 0, tx1 = LIGHT_TILES_X - 1, ty0 = 0, ty1 = LIGHT_TILES_Y - 1;
        if(!whole)
        {
            if(x1 < -1.f || x0 > 1.f || y1 < -1.f || y0 > 1.f)
            {
                continue;
            }
            tx0 = za::clamp(static_cast<int>(za::floor((x0 * 0.5f + 0.5f) * LIGHT_TILES_X)), 0, LIGHT_TILES_X - 1);
            tx1 = za::clamp(static_cast<int>(za::floor((x1 * 0.5f + 0.5f) * LIGHT_TILES_X)), 0, LIGHT_TILES_X - 1);
            ty0 = za::clamp(static_cast<int>(za::floor((y0 * 0.5f + 0.5f) * LIGHT_TILES_Y)), 0, LIGHT_TILES_Y - 1);
            ty1 = za::clamp(static_cast<int>(za::floor((y1 * 0.5f + 0.5f) * LIGHT_TILES_Y)), 0, LIGHT_TILES_Y - 1);
        }
        const za::U32 bit = 1u << (i & 31);
        const int word = static_cast<int>(i >> 5);
        // The depth slices its reach spans (the light clusters' logarithmic slices: gl_rmain.c's ZLogScale, ZLogBias).
        const float wc = m[3] * o.centre.x + m[7] * o.centre.y + m[11] * o.centre.z + m[15];
        const auto slice = [](float w) {
            return w <= 1e-3f ? 0
                              : za::clamp(static_cast<int>(za::floor(za::log2(w) * r_framedata.zlogscale + r_framedata.zlogbias)),
                                    0, LIGHT_TILES_Z - 1);
        };
        const int z0 = slice(wc - o.influence * depthScale), z1 = slice(wc + o.influence * depthScale);
        for(int z = z0; z <= z1; z++)
        {
            block.slices[z >> 1][(z & 1) * 2 + word] |= bit;
        }
        for(int ty = ty0; ty <= ty1; ty++)
        {
            for(int tx = tx0; tx <= tx1; tx++)
            {
                const int t = tx + ty * LIGHT_TILES_X;
                block.tiles[t >> 1][(t & 1) * 2 + word] |= bit;
            }
        }
    }
}

// ----------------------------------------------------------------------------
// Per-vertex occlusion baked into Quake models.

constexpr int RAYS = 24;

struct Baked
{
    za::U64 hash{0};
    za::Vector<unsigned char> vis;
};
// By name; looked up by a model's name without making a za::String of it (a transparent hash).
struct NameHash
{
    using is_transparent = void;
    [[nodiscard]] za::SizeT operator()(za::StringView s) const { return ankerl::unordered_dense::hash<za::StringView>{}(s); }
};
struct NameEqual
{
    using is_transparent = void;
    [[nodiscard]] bool operator()(za::StringView a, za::StringView b) const { return a == b; }
};
ankerl::unordered_dense::map<za::String, Baked, NameHash, NameEqual> baked;
double bakeSeconds = 0.0; // vr_ao_show
int bakeModels = 0;
int bakeHits = 0;
double queueSeconds = 0.0; // the main thread's share: hashing, copying the poses
za::String slowestModel;
double slowestSeconds = 0.0;

za::Array<glm::vec3, RAYS> rayDirections()
{
    // Cosine-weighted over the hemisphere round +z (Hammersley points): each ray weighs the same.
    za::Array<glm::vec3, RAYS> d{};
    for(int i = 0; i < RAYS; i++)
    {
        za::U32 b = static_cast<za::U32>(i);
        b = (b << 16u) | (b >> 16u);
        b = ((b & 0x55555555u) << 1u) | ((b & 0xAAAAAAAAu) >> 1u);
        b = ((b & 0x33333333u) << 2u) | ((b & 0xCCCCCCCCu) >> 2u);
        b = ((b & 0x0F0F0F0Fu) << 4u) | ((b & 0xF0F0F0F0u) >> 4u);
        b = ((b & 0x00FF00FFu) << 8u) | ((b & 0xFF00FF00u) >> 8u);
        const float u = (static_cast<float>(i) + 0.5f) / RAYS;
        const float v = static_cast<float>(b) * 2.3283064365386963e-10f;
        const float r = za::sqrt(u), phi = 6.2831853f * v;
        d[i] = {r * za::cos(phi), r * za::sin(phi), za::sqrt(za::max(0.f, 1.f - u))};
    }
    return d;
}
// Made before main and never changed: read by the bake's pose threads at once.
const za::Array<glm::vec3, RAYS> rayDirs = rayDirections();

struct Tri
{
    za::U16 v[3];
};

struct PoseJob
{
    const trivertx_t* verts{nullptr}; // numposes x numverts
    int numverts{0};
    glm::vec3 scale{1.f}, origin{0.f};
    const za::Vector<Tri>* tris{nullptr};
    const za::Vector<za::Vector<int>>* ring{nullptr}; // each vertex's neighbours (sharing a triangle)
    float reach{1.f};
    unsigned char* out{nullptr};
    const za::Atomic<bool>* stop{nullptr}; // the game quitting: the bake given up at once
};

// One pose: for each vertex, the share of cosine-weighted rays over its hemisphere that the model's own triangles
// stop within `reach` (a hit counts less the farther it is: 1 - t / reach).
void bakePose(const PoseJob& job, int pose, za::Vector<int>& cand)
{
    const za::Array<glm::vec3, RAYS>& dirs = rayDirs;
    const trivertx_t* tv = job.verts + static_cast<size_t>(pose) * job.numverts;
    const int nv = job.numverts;
    za::Vector<glm::vec3> p(nv);
    for(int i = 0; i < nv; i++)
    {
        p[i] = glm::vec3{tv[i].v[0], tv[i].v[1], tv[i].v[2]} * job.scale + job.origin;
    }
    const za::Vector<Tri>& tris = *job.tris;
    const size_t nt = tris.size();
    za::Vector<glm::vec3> v0(nt), e1(nt), e2(nt), mid(nt);
    za::Vector<float> rad(nt);
    for(size_t t = 0; t < nt; t++)
    {
        const glm::vec3 a = p[tris[t].v[0]], b = p[tris[t].v[1]], c = p[tris[t].v[2]];
        v0[t] = a;
        e1[t] = b - a;
        e2[t] = c - a;
        mid[t] = (a + b + c) * (1.f / 3.f);
        rad[t] = za::sqrt(qza::maxOf(glm::dot(a - mid[t], a - mid[t]), glm::dot(b - mid[t], b - mid[t]), glm::dot(c - mid[t], c - mid[t])));
    }
    const float reach = job.reach;
    const float tmin = reach * 0.02f;
    unsigned char* out = job.out + static_cast<size_t>(pose) * nv;
    za::Vector<int> mark(nv, -1);
    for(int i = 0; i < nv; i++)
    {
        if(job.stop && job.stop->loadRelaxed())
        {
            return;
        }
        // Its own triangles and its neighbours' are left out: on a curved surface of byte-rounded vertices the rays
        // near its horizon would graze them (a speckle of false occlusion over the whole model).
        mark[i] = i;
        for(const int k : (*job.ring)[i])
        {
            mark[k] = i;
        }
        const float* nn = r_avertexnormals[za::min<int>(tv[i].lightnormalindex, NUMVERTEXNORMALS - 1)];
        const glm::vec3 n{nn[0], nn[1], nn[2]};
        const glm::vec3 o = p[i] + n * (reach * 0.03f);
        cand.clear();
        for(size_t t = 0; t < nt; t++)
        {
            const Tri& tr = tris[t];
            if(mark[tr.v[0]] == i || mark[tr.v[1]] == i || mark[tr.v[2]] == i)
            {
                continue;
            }
            const glm::vec3 d = mid[t] - p[i];
            const float r = reach + rad[t];
            if(glm::dot(d, d) > r * r || glm::dot(d, n) < -rad[t])
            {
                continue;
            }
            cand.pushBack(static_cast<int>(t));
        }
        float occ = 0.f;
        if(!cand.empty())
        {
            // Duff et al.'s frame round the normal.
            const float sign = za::copysign(1.f, n.z);
            const float a = -1.f / (sign + n.z);
            const float b = n.x * n.y * a;
            const glm::vec3 tx{1.f + sign * n.x * n.x * a, sign * b, -sign * n.x};
            const glm::vec3 ty{b, sign + n.y * n.y * a, -n.y};
            for(const glm::vec3& l : dirs)
            {
                const glm::vec3 d = tx * l.x + ty * l.y + n * l.z;
                float best = reach;
                for(const int t : cand)
                {
                    const glm::vec3 pv = glm::cross(d, e2[t]);
                    const float det = glm::dot(e1[t], pv);
                    if(qza::abs(det) < 1e-9f)
                    {
                        continue;
                    }
                    const float inv = 1.f / det;
                    const glm::vec3 s = o - v0[t];
                    const float u = glm::dot(s, pv) * inv;
                    if(u < 0.f || u > 1.f)
                    {
                        continue;
                    }
                    const glm::vec3 q = glm::cross(s, e1[t]);
                    const float v = glm::dot(d, q) * inv;
                    if(v < 0.f || u + v > 1.f)
                    {
                        continue;
                    }
                    const float dist = glm::dot(e2[t], q) * inv;
                    if(dist > tmin && dist < best)
                    {
                        best = dist;
                    }
                }
                if(best < reach)
                {
                    const float f = best / reach;
                    occ += 1.f - f; // nearer hits count more (linearly: 1 - f^2 darkened the models too much)
                }
            }
        }
        const float vis = 1.f - occ / RAYS;
        out[i] = static_cast<unsigned char>(za::clamp(vis * 255.f + 0.5f, 0.f, 255.f));
    }
}

} // namespace

namespace
{

// A model's occlusion is baked on the game's threads (vr_jobs.hpp: one task bakes the queued models in turn, each one's
// poses shared out among up to BAKE_THREADS threads) from a copy of its poses, so that loading a map is not held up
// (e1m1's 69 models took 0.86 s on 15 threads): until it is done the model has none (its vertex buffer holds 255), then
// its vertex buffer is built again (integrateBakes, each frame).
constexpr int BAKE_THREADS = 4;

struct BakeJob
{
    za::String name;
    qmodel_t* model{nullptr};
    za::U64 hash{0};
    int numverts{0};
    int numposes{0};
    za::Vector<trivertx_t> verts;
    za::Vector<Tri> tris;
    glm::vec3 scale{1.f}, origin{0.f};
    za::Vector<unsigned char> vis;
    double seconds{0.0};
};

struct BakeQueue
{
    za::AtomicMutex mutex;
    za::Vector<za::UniquePtr<BakeJob>> pending; // (first in, first out)
    za::Vector<za::UniquePtr<BakeJob>> done;
    ankerl::unordered_dense::map<za::String, za::U64, NameHash, NameEqual> queued; // queued or being baked: name, hash
    bool running{false};            // a task bakes the queued models (until none is left)
    jobs::Future<void> runner;      // that task (the main thread's: waited for at shutdown, ao::shutdown)
    za::Atomic<bool> stop{false};  // set at shutdown: the bake under way gives up
};

// Made before main and never destroyed (its task is finished before the game exits: ao::shutdown).
BakeQueue* const theBakeQueue = new BakeQueue;

BakeQueue& bakeQueue()
{
    return *theBakeQueue;
}

// FNV-1a of a model's poses, triangles and scale: the same name from another game folder is another model.
za::U64 modelHash(const aliashdr_t* hdr)
{
    za::U64 h = 1469598103934665603ull;
    const auto mix = [&h](const void* data, size_t bytes) {
        const auto* b = static_cast<const unsigned char*>(data);
        for(size_t i = 0; i < bytes; i++)
        {
            h = (h ^ b[i]) * 1099511628211ull;
        }
    };
    mix(reinterpret_cast<const byte*>(hdr) + hdr->vertexes, static_cast<size_t>(hdr->numposes) * hdr->numverts * sizeof(trivertx_t));
    mix(reinterpret_cast<const byte*>(hdr) + hdr->indexes, static_cast<size_t>(hdr->numindexes) * sizeof(unsigned short));
    mix(hdr->scale, sizeof(hdr->scale));
    return h;
}

// The bakes run below the game's threads, so that one finishing after a map has loaded never takes a frame's time: the
// pool's thread lowered while it bakes, then put back.
struct BelowNormal
{
#ifdef _WIN32
    int was = GetThreadPriority(GetCurrentThread());
    BelowNormal()
    {
        SetThreadPriority(GetCurrentThread(), -1); // THREAD_PRIORITY_BELOW_NORMAL
    }
    ~BelowNormal()
    {
        SetThreadPriority(GetCurrentThread(), was);
    }
#endif
    BelowNormal(const BelowNormal&) = delete;
    BelowNormal& operator=(const BelowNormal&) = delete;
};

void runBake(BakeJob& job)
{
    const auto t0 = qza::nowNs();
    // Reach: an eighth of the first pose's diagonal (a grunt's 9 units, a gun's few), within 1 .. 16 units.
    glm::vec3 lo{1e9f}, hi{-1e9f};
    for(int i = 0; i < job.numverts; i++)
    {
        const glm::vec3 p = glm::vec3{job.verts[i].v[0], job.verts[i].v[1], job.verts[i].v[2]} * job.scale + job.origin;
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
    job.vis.clear();
    job.vis.resize(static_cast<size_t>(job.numposes) * job.numverts, 255);
    PoseJob pj;
    pj.verts = job.verts.data();
    pj.numverts = job.numverts;
    pj.scale = job.scale;
    pj.origin = job.origin;
    pj.tris = &job.tris;
    za::Vector<za::Vector<int>> ring(job.numverts);
    for(const Tri& t : job.tris)
    {
        for(int a = 0; a < 3; a++)
        {
            for(int b = 0; b < 3; b++)
            {
                if(a != b && za::find(ring[t.v[a]].begin(), ring[t.v[a]].end(), t.v[b]) == ring[t.v[a]].end())
                {
                    ring[t.v[a]].pushBack(t.v[b]);
                }
            }
        }
    }
    pj.ring = &ring;
    pj.reach = za::clamp(glm::distance(lo, hi) * 0.125f, 1.f, 16.f);
    pj.out = job.vis.data();
    pj.stop = &bakeQueue().stop;

    // The poses shared out between threads, every `threads`th to each (they only read the copy and write their own poses).
    const int poses = job.numposes;
    const int threads = za::clamp(za::min(jobs::hardwareThreads() / 2, BAKE_THREADS), 1, poses);
    jobs::parallelFor(static_cast<za::SizeT>(threads), 1, [&pj, poses, threads](za::SizeT t0, za::SizeT t1) {
        const BelowNormal low;
        za::Vector<int> cand;
        for(int first = static_cast<int>(t0); first < static_cast<int>(t1); first++)
        {
            for(int f = first; f < poses && !pj.stop->loadRelaxed(); f += threads)
            {
                bakePose(pj, f, cand);
            }
        }
    });
    job.seconds = qza::secondsSince(t0);
}

// The bake task: the queued models baked in turn, until none is left (or the game stops).
void bakeQueued()
{
    BakeQueue& q = bakeQueue();
    const BelowNormal low;
    for(;;)
    {
        za::UniquePtr<BakeJob> job;
        {
            za::LockGuard lock(q.mutex);
            if(q.stop.loadSeqCst() || q.pending.empty())
            {
                q.running = false;
                return;
            }
            job = ZA_MOVE(q.pending.front());
            q.pending.erase(q.pending.begin());
        }
        runBake(*job);
        za::LockGuard lock(q.mutex);
        if(q.stop.loadSeqCst())
        {
            q.running = false;
            return; // (given up part way)
        }
        q.done.pushBack(ZA_MOVE(job));
    }
}

// Main thread, each frame: finished bakes into the cache, and their models' vertex buffers built again with them.
void integrateBakes()
{
    BakeQueue& q = bakeQueue();
    za::Vector<za::UniquePtr<BakeJob>> finished;
    {
        za::LockGuard lock(q.mutex);
        if(q.done.empty())
        {
            return;
        }
        finished.swap(q.done);
        for(const auto& job : finished)
        {
            q.queued.erase(job->name);
        }
    }
    for(auto& job : finished)
    {
        Baked& entry = baked[job->name];
        entry.hash = job->hash;
        entry.vis = ZA_MOVE(job->vis);
        bakeSeconds += job->seconds;
        bakeModels++;
        if(job->seconds > slowestSeconds)
        {
            slowestSeconds = job->seconds;
            slowestModel = job->name;
        }
        double mean = 0.0;
        za::U32 texels = 2166136261u; // (FNV-1a: the same bake whatever threads made it)
        for(const unsigned char v : entry.vis)
        {
            mean += v;
            texels = (texels ^ v) * 16777619u;
        }
        mean /= za::max<size_t>(entry.vis.size(), 1) * 255.0;
        Con_DPrintf("vr_ao: %s: %d poses x %d vertices, %d triangles baked in %.1f ms, open %.2f on average (%08x)\n",
            job->name.cStr(), job->numposes, job->numverts, static_cast<int>(job->tris.size()), job->seconds * 1000.0, mean,
            texels);
        qmodel_t* m = job->model;
        if(m && m->type == mod_alias && job->name == m->name)
        {
            auto* hdr = static_cast<aliashdr_t*>(Mod_Extradata(m));
            if(hdr && hdr->poseverttype == aliashdr_t::PV_QUAKE1 && modelHash(hdr) == job->hash)
            {
                GLMesh_LoadVertexBuffer(m, hdr);
            }
        }
    }
}

} // namespace

extern "C" const unsigned char* VR_AliasVertexAO(qmodel_t* model, const void* aliashdr)
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!model || !hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || hdr->nextsurface || hdr->numverts <= 0 ||
       hdr->numposes <= 0 || hdr->numindexes < 3)
    {
        return nullptr;
    }
    if(vr_mock_fast.value >= 2.f)
    {
        return nullptr; // a test run that doesn't draw (VR_SkipScreen): not seconds of every core's time at its start
    }
    const auto t0 = qza::nowNs();
    struct Timer
    {
        za::I64 start;
        ~Timer() { queueSeconds += qza::secondsSince(start); }
    } timer{t0};
    const size_t count = static_cast<size_t>(hdr->numposes) * hdr->numverts;
    const za::U64 h = modelHash(hdr);
    const auto it = baked.find(za::StringView{model->name});
    if(it != baked.end() && it->second.hash == h && it->second.vis.size() == count)
    {
        bakeHits++;
        return it->second.vis.data();
    }

    BakeQueue& q = bakeQueue();
    qza::UniqueLock lock(q.mutex);
    if(const auto qi = q.queued.find(za::StringView{model->name}); qi != q.queued.end() && qi->second == h)
    {
        return nullptr; // on its way
    }
    auto job = za::makeUnique<BakeJob>();
    job->name = model->name;
    job->model = model;
    job->hash = h;
    job->numverts = hdr->numverts;
    job->numposes = hdr->numposes;
    const auto* verts = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    job->verts.assignRange(verts, verts + count);
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    job->tris.reserve(hdr->numindexes / 3);
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        Tri t;
        bool ok = true;
        for(int k = 0; k < 3; k++)
        {
            const int vbo = idx[i + k];
            ok = ok && vbo < hdr->numverts_vbo;
            t.v[k] = ok ? desc[vbo].vertindex : 0;
        }
        if(ok && t.v[0] != t.v[1] && t.v[1] != t.v[2] && t.v[0] != t.v[2])
        {
            job->tris.pushBack(t);
        }
    }
    job->scale = {hdr->scale[0], hdr->scale[1], hdr->scale[2]};
    job->origin = {hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
    q.queued[model->name] = h;
    q.pending.pushBack(ZA_MOVE(job));
    if(q.running || q.stop.loadSeqCst())
    {
        return nullptr; // (the task under way takes it next)
    }
    q.running = true;
    lock.unlock();
    q.runner = jobs::async(bakeQueued); // (the one before has returned: running was false)
    return nullptr;
}

extern "C" void VR_AliasAO(const entity_t* e, float out[4])
{
    out[0] = 0.f;
    out[1] = za::clamp(vr_ao_models.value, 0.f, 2.f);
    out[2] = out[3] = 0.f;
    if(!e)
    {
        return;
    }
    if(VR_IsViewEntity(e) || VR_AliasBonePoses(e, nullptr) || e == &cl.viewent)
    {
        out[0] = PLAYER_GROUP;
        return;
    }
    out[0] = groupOf(e);
}

extern "C" float VR_BrushAOSelf(const entity_t* e)
{
    return groupOf(e);
}

namespace
{

void show_f()
{
    Con_Printf("vr_ao: %d occluders this frame (of %d candidates), choosing them %.3f ms a frame on average\n",
        static_cast<int>(chosen.size()), static_cast<int>(candidates.size()),
        buildCount ? buildSeconds * 1000.0 / static_cast<double>(buildCount) : 0.0);
    Con_Printf("vr_ao: models' own occlusion: %d baked in %.1f ms on the worker (slowest %s, %.1f ms), %d from the cache; "
               "%.1f ms on the main thread\n",
        bakeModels, bakeSeconds * 1000.0, slowestModel.cStr(), slowestSeconds * 1000.0, bakeHits, queueSeconds * 1000.0);
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    for(const Occluder& o : chosen)
    {
        Con_Printf("  %s group %g at %.0f %.0f %.0f size %.1f %.1f %.1f strength %.2f reach %.1f, %.0f away (%s at %.0f %.0f %.0f)\n",
            o.box ? "box" : "ellipsoid", o.group, o.centre.x, o.centre.y, o.centre.z, o.size.x, o.size.y, o.size.z,
            o.strength, o.reach, glm::distance(o.centre, eye), o.owner && o.owner->model ? o.owner->model->name : "?",
            o.owner ? o.owner->origin[0] : 0.f, o.owner ? o.owner->origin[1] : 0.f, o.owner ? o.owner->origin[2] : 0.f);
    }
}

} // namespace

void ao::shutdown()
{
    // The bake task (and its poses' chunks) finished before the game's data and the process go away: a bake under way
    // gives up at its next vertex.
    BakeQueue& q = bakeQueue();
    bool busy = false;
    {
        za::LockGuard lock(q.mutex);
        q.stop.storeSeqCst(true);
        busy = !q.queued.empty();
        q.pending.clear();
    }
    if(q.runner.valid())
    {
        const double t0 = Sys_DoubleTime();
        q.runner.wait();
        if(busy)
        {
            Con_DPrintf("vr_ao: the bake task stopped in %.1f ms\n", (Sys_DoubleTime() - t0) * 1000.0);
        }
    }
}

void ao::onGameDirChanged()
{
    baked.clear(); // (a bake still on the worker files its result afterwards: checked by hash before use)
    brushDrawable.clear();
    brushGeneration = -1;
    bakedSubmodels.clear();
    bakedGeneration = -1;
    candidates.clear();
    chosen.clear();
    groupCount = 0;
    builtFrame = -1;
}

void ao::init()
{
    Cmd_AddCommand("vr_ao_show", show_f);
}

void ao::upload()
{
    if(builtFrame != host_framecount)
    {
        builtFrame = host_framecount;
        integrateBakes();
        QVR_PROFILE("ao occluders");
        build();
    }
    if(chosen.empty())
    {
        block.params[0] = 0.f;
    }
    else
    {
        binTiles();
    }
    GLuint buf;
    GLbyte* ofs;
    GL_Upload(GL_UNIFORM_BUFFER, &block, sizeof(block), &buf, &ofs);
    GL_BindBufferRange(GL_UNIFORM_BUFFER, 2, buf, reinterpret_cast<GLintptr>(ofs), static_cast<GLsizeiptr>(sizeof(block)));
}
