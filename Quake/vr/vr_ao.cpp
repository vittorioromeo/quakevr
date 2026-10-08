// vr_ao.cpp -- dynamic ambient occlusion: see vr_ao.hpp.

#include "vr_modelmetadata.hpp"
#include "vr_ao.hpp"
#include "vr_portals.hpp"
#include "vr_view.hpp"
#include "vr_engine.hpp"
#include "vr_avatar.hpp"
#include "vr_cvars.hpp"
#include "vr_main.hpp"
#include "vr_jobs.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_files.hpp"
#include "vr_sha256.hpp"

#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/Memset.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Chrono/Clock.hpp"
#include "Zancle/Concurrency/Atomic.hpp"
#include "Zancle/Concurrency/AtomicMutex.hpp"
#include "Zancle/Concurrency/LockGuard.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
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

#if defined(SDL_FRAMEWORK) || defined(NO_SDL_CONFIG)
#include <SDL2/SDL.h>
#else
#include "SDL.h"
#endif

#include <immintrin.h> // (the bake's rays: SSE2, x64's baseline, four at a time; AVX, when there, eight)
#include <stdio.h>
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
    bool starting = false;
    if(!VectorCompare(e->origin, e->currentorigin) || !VectorCompare(e->angles, e->currentangles))
    {
        R_MoveLerpStart(e, from, afrom); // a move starting this frame (where the last one is drawn)
        VectorCopy(e->origin, to);
        VectorCopy(e->angles, ato);
        starting = true;
    }
    else
    {
        VectorCopy(e->previousorigin, from);
        VectorCopy(e->currentorigin, to);
        VectorCopy(e->previousangles, afrom);
        VectorCopy(e->currentangles, ato);
    }
    const float blend = starting ? 0.f : R_MoveLerpBlend(e); // (a move starting: at its start)
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
    if(za::max(o.size.x, o.size.y, o.size.z) < 1.5f)
    {
        return;
    }
    o.box = false;
    o.strength = strength;
    o.reach = reach;
    o.influence = za::max(o.size.x, o.size.y, o.size.z) * reach;
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
    const glm::vec3 ref = za::abs(o.axes[0].z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
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
        const int i = modelmeta::boneIndex(e->model, name);
        if(i >= 0)
        {
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
    if(qvr::modelmeta::has(model, qvr::modelmeta::Trait::Submodel))
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
    o.reach = za::clamp(za::max(o.size.x, o.size.y, o.size.z) * 0.6f, 16.f, 64.f);
    o.influence = glm::length(o.size) + o.reach;
    o.group = -1.f;
    candidates.pushBack(o);
}

// Chooses the frame's occluders (once, for both eyes): the nearest MAX_OCCLUDERS that can reach the view.
void build()
{
    const auto t0 = za::Clock::nowNanoseconds();
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
    buildSeconds += za::nanosecondsToSeconds(za::Clock::nowNanoseconds() - t0);
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
// The bake's other numbers (all of them: the disk cache's key holds them and the rays, so a change here bakes again).
constexpr float REACH_SHARE = 0.125f; // reach: this share of the first pose's diagonal ...
constexpr float REACH_MIN = 1.f;      // ... within these units
constexpr float REACH_MAX = 16.f;
constexpr float NEAREST_SHARE = 0.02f; // hits nearer than this share of the reach are left out
constexpr float LIFT_SHARE = 0.03f;    // the rays start this share of the reach off the surface, along its normal

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
// The disk cache (vr_ao_cache_info), counted as finished bakes are taken in (the main thread).
int diskHits = 0;         // read from their files
double diskSeconds = 0.0; // those reads (the bake task's thread: hashing, reading, checking)
int diskMisses = 0;       // baked: no file (or one not whole, not this model's)
int diskRejected = 0;     // of those, a file was there
int diskWritten = 0;
int checkSame = 0;        // vr_ao_cache 2: baked anyway, the same as the file
int checkDiffered = 0;

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
    bool avx{false};                       // the rays eight at a time (raysAvx), not four
};

// The reference bake (vr_ao_bench reference; the bake before 2026-10-06): every triangle tested for being in reach,
// then each ray against each candidate in turn. bakePose must give the same bytes.
void bakePoseReference(const PoseJob& job, int pose, za::Vector<int>& cand)
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
        rad[t] = za::sqrt(za::max(glm::dot(a - mid[t], a - mid[t]), glm::dot(b - mid[t], b - mid[t]), glm::dot(c - mid[t], c - mid[t])));
    }
    const float reach = job.reach;
    const float tmin = reach * NEAREST_SHARE;
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
        const glm::vec3 o = p[i] + n * (reach * LIFT_SHARE);
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
                    if(za::abs(det) < 1e-9f)
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

// A pose thread's buffers, kept from one pose to the next (a local of its chunk: vr_jobs' threads never share one).
struct PoseScratch
{
    za::Vector<glm::vec3> p;              // the pose's vertices
    za::Vector<glm::vec3> v0, e1, e2, mid; // its triangles: a corner, two edges, the centre
    za::Vector<float> rad;                // and the radius round the centre
    za::Vector<int> mark;                 // the vertex whose own and neighbours' triangles these are (left out)
    za::Vector<int> cellStart, cellItems; // the grid: each cell's triangles (cellItems[cellStart[c] .. cellStart[c + 1]])
    za::Vector<glm::vec4> cellMidRad;     // and their centres and radii, in the same order
    za::Vector<float> cellRadius;         // each cell's widest radius
    za::Vector<int> cellOf;               // each triangle's cell (-1: in `big`)
    za::Vector<int> big;                  // the triangles too wide for the grid
    za::Vector<int> cand;                 // a vertex's triangles in reach
    // The candidates' numbers that don't depend on the ray, one array each (read four rays at a time).
    za::Vector<float> e1x, e1y, e1z, e2x, e2y, e2z, sx, sy, sz, qx, qy, qz, e2q;
};

// The triangles of a pose in a uniform grid of cubes half the reach wide (at most GRID_MAX a side), each in the cell
// of its centre; those wider than half the reach (radius round the centre) in a list of their own, tested for every
// vertex. A vertex's triangles in reach (the centre within the reach plus the radius) are then all in the cells within
// the reach plus the widest radius of the grid's triangles (bigRadius), each cell skipped when its box is farther than
// the reach plus its own widest radius. The boxes are widened by `margin`: the test that picks the candidates (the
// same as ever) is done in floats.
constexpr int GRID_MAX = 64;

struct Grid
{
    glm::vec3 lo{0.f};
    float size{1.f};
    float inv{1.f};
    int n[3]{1, 1, 1};
    float margin{0.f};
    float gridRadius{0.f}; // the widest radius of the grid's triangles

    [[nodiscard]] int cell(float x, int axis) const
    {
        return za::clamp(static_cast<int>((x - lo[axis]) * inv), 0, n[axis] - 1);
    }
};

[[nodiscard]] Grid buildGrid(PoseScratch& s, float reach)
{
    const size_t nt = s.mid.size();
    Grid g;
    g.margin = 0.01f + reach * 0.01f;
    const float wide = reach * 0.5f;
    glm::vec3 lo{1e30f}, hi{-1e30f};
    s.big.clear();
    for(size_t t = 0; t < nt; t++)
    {
        if(s.rad[t] > wide)
        {
            s.big.pushBack(static_cast<int>(t));
            continue;
        }
        lo = glm::min(lo, s.mid[t]);
        hi = glm::max(hi, s.mid[t]);
        g.gridRadius = za::max(g.gridRadius, s.rad[t]);
    }
    if(s.big.size() == nt)
    {
        lo = hi = glm::vec3{0.f};
    }
    g.lo = lo - glm::vec3{g.margin};
    const glm::vec3 extent = hi - lo + glm::vec3{2.f * g.margin};
    g.size = za::max(reach * 0.5f, za::max(extent.x, extent.y, extent.z) / static_cast<float>(GRID_MAX));
    g.inv = 1.f / g.size;
    for(int a = 0; a < 3; a++)
    {
        g.n[a] = za::clamp(static_cast<int>(extent[a] * g.inv) + 1, 1, GRID_MAX);
    }
    const int cells = g.n[0] * g.n[1] * g.n[2];
    s.cellStart.clear();
    s.cellStart.resize(static_cast<size_t>(cells) + 1, 0);
    s.cellRadius.clear();
    s.cellRadius.resize(static_cast<size_t>(cells), 0.f);
    s.cellOf.resize(nt);
    for(size_t t = 0; t < nt; t++)
    {
        if(s.rad[t] > wide)
        {
            s.cellOf[t] = -1;
            continue;
        }
        const int c = g.cell(s.mid[t].x, 0) + g.n[0] * (g.cell(s.mid[t].y, 1) + g.n[1] * g.cell(s.mid[t].z, 2));
        s.cellOf[t] = c;
        s.cellStart[static_cast<size_t>(c) + 1]++;
        s.cellRadius[static_cast<size_t>(c)] = za::max(s.cellRadius[static_cast<size_t>(c)], s.rad[t]);
    }
    for(int c = 0; c < cells; c++)
    {
        s.cellStart[static_cast<size_t>(c) + 1] += s.cellStart[static_cast<size_t>(c)];
    }
    s.cellItems.clear();
    s.cellItems.resize(static_cast<size_t>(s.cellStart[static_cast<size_t>(cells)]), 0);
    s.cellMidRad.resize(s.cellItems.size());
    za::Vector<int>& fill = s.cand; // (each cell's next free place; cand is free until the vertices)
    fill.assignRange(s.cellStart.begin(), s.cellStart.end() - 1);
    for(size_t t = 0; t < nt; t++)
    {
        if(s.cellOf[t] >= 0)
        {
            const size_t k = static_cast<size_t>(fill[static_cast<size_t>(s.cellOf[t])]++);
            s.cellItems[k] = static_cast<int>(t);
            s.cellMidRad[k] = glm::vec4{s.mid[t], s.rad[t]};
        }
    }
    return g;
}

// A vertex's rays against its candidates (PoseScratch's arrays, nc of them): each ray's nearest hit farther than tmin,
// or `reach`, into best[RAYS]. Möller and Trumbore's test, the same float operations in the same order as the
// reference's for each ray (and so the same bits), several rays at a time: four (SSE2, x64's baseline) or eight (AVX,
// when the processor has it: raysAvx). A test the reference skips (det near 0; u, v outside the triangle) is a lane
// left out; NaNs fall the same way (each "continue" is a comparison's negation).
struct RayInput
{
    const PoseScratch* s;
    int nc;
    const float* dx; // the rays' directions, RAYS each (aligned to 32 bytes)
    const float* dy;
    const float* dz;
    float tmin;
    float reach;
    float* best;
};

void raysSse(const RayInput& in)
{
    const PoseScratch& s = *in.s;
    const __m128 vtmin = _mm_set1_ps(in.tmin), vzero = _mm_setzero_ps(), vone = _mm_set1_ps(1.f);
    const __m128 vtiny = _mm_set1_ps(1e-9f), absMask = _mm_castsi128_ps(_mm_set1_epi32(0x7fffffff));
    for(int r = 0; r < RAYS; r += 4)
    {
        const __m128 ddx = _mm_load_ps(in.dx + r), ddy = _mm_load_ps(in.dy + r), ddz = _mm_load_ps(in.dz + r);
        __m128 vbest = _mm_set1_ps(in.reach);
        for(int c = 0; c < in.nc; c++)
        {
            const __m128 e1x = _mm_set1_ps(s.e1x[c]), e1y = _mm_set1_ps(s.e1y[c]), e1z = _mm_set1_ps(s.e1z[c]);
            const __m128 e2x = _mm_set1_ps(s.e2x[c]), e2y = _mm_set1_ps(s.e2y[c]), e2z = _mm_set1_ps(s.e2z[c]);
            // pv = cross(d, e2); det = dot(e1, pv)
            const __m128 pvx = _mm_sub_ps(_mm_mul_ps(ddy, e2z), _mm_mul_ps(e2y, ddz));
            const __m128 pvy = _mm_sub_ps(_mm_mul_ps(ddz, e2x), _mm_mul_ps(e2z, ddx));
            const __m128 pvz = _mm_sub_ps(_mm_mul_ps(ddx, e2y), _mm_mul_ps(e2x, ddy));
            const __m128 det = _mm_add_ps(_mm_add_ps(_mm_mul_ps(e1x, pvx), _mm_mul_ps(e1y, pvy)), _mm_mul_ps(e1z, pvz));
            __m128 ok = _mm_cmpnlt_ps(_mm_and_ps(det, absMask), vtiny);
            const __m128 inv = _mm_div_ps(vone, det);
            // u = dot(s, pv) * inv
            const __m128 sx = _mm_set1_ps(s.sx[c]), sy = _mm_set1_ps(s.sy[c]), sz = _mm_set1_ps(s.sz[c]);
            const __m128 u = _mm_mul_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(sx, pvx), _mm_mul_ps(sy, pvy)), _mm_mul_ps(sz, pvz)), inv);
            ok = _mm_and_ps(ok, _mm_and_ps(_mm_cmpnlt_ps(u, vzero), _mm_cmpngt_ps(u, vone)));
            // v = dot(d, q) * inv
            const __m128 qx = _mm_set1_ps(s.qx[c]), qy = _mm_set1_ps(s.qy[c]), qz = _mm_set1_ps(s.qz[c]);
            const __m128 v = _mm_mul_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(ddx, qx), _mm_mul_ps(ddy, qy)), _mm_mul_ps(ddz, qz)), inv);
            ok = _mm_and_ps(ok, _mm_and_ps(_mm_cmpnlt_ps(v, vzero), _mm_cmpngt_ps(_mm_add_ps(u, v), vone)));
            // dist = dot(e2, q) * inv
            const __m128 dist = _mm_mul_ps(_mm_set1_ps(s.e2q[c]), inv);
            ok = _mm_and_ps(ok, _mm_and_ps(_mm_cmpgt_ps(dist, vtmin), _mm_cmplt_ps(dist, vbest)));
            vbest = _mm_or_ps(_mm_and_ps(ok, dist), _mm_andnot_ps(ok, vbest));
        }
        _mm_store_ps(in.best + r, vbest);
    }
}

#if defined(__clang__) || defined(__GNUC__)
#define QVR_TARGET_AVX __attribute__((target("avx")))
#else
#define QVR_TARGET_AVX
#endif

// raysSse's operations, eight lanes wide (no FMA: the build contracts nothing, and neither does this).
QVR_TARGET_AVX void raysAvx(const RayInput& in)
{
    static_assert(RAYS % 8 == 0);
    const PoseScratch& s = *in.s;
    const __m256 vtmin = _mm256_set1_ps(in.tmin), vzero = _mm256_setzero_ps(), vone = _mm256_set1_ps(1.f);
    const __m256 vtiny = _mm256_set1_ps(1e-9f), absMask = _mm256_castsi256_ps(_mm256_set1_epi32(0x7fffffff));
    for(int r = 0; r < RAYS; r += 8)
    {
        const __m256 ddx = _mm256_load_ps(in.dx + r), ddy = _mm256_load_ps(in.dy + r), ddz = _mm256_load_ps(in.dz + r);
        __m256 vbest = _mm256_set1_ps(in.reach);
        for(int c = 0; c < in.nc; c++)
        {
            const __m256 e1x = _mm256_set1_ps(s.e1x[c]), e1y = _mm256_set1_ps(s.e1y[c]), e1z = _mm256_set1_ps(s.e1z[c]);
            const __m256 e2x = _mm256_set1_ps(s.e2x[c]), e2y = _mm256_set1_ps(s.e2y[c]), e2z = _mm256_set1_ps(s.e2z[c]);
            const __m256 pvx = _mm256_sub_ps(_mm256_mul_ps(ddy, e2z), _mm256_mul_ps(e2y, ddz));
            const __m256 pvy = _mm256_sub_ps(_mm256_mul_ps(ddz, e2x), _mm256_mul_ps(e2z, ddx));
            const __m256 pvz = _mm256_sub_ps(_mm256_mul_ps(ddx, e2y), _mm256_mul_ps(e2x, ddy));
            const __m256 det = _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(e1x, pvx), _mm256_mul_ps(e1y, pvy)), _mm256_mul_ps(e1z, pvz));
            __m256 ok = _mm256_cmp_ps(_mm256_and_ps(det, absMask), vtiny, _CMP_NLT_UQ);
            const __m256 inv = _mm256_div_ps(vone, det);
            const __m256 sx = _mm256_set1_ps(s.sx[c]), sy = _mm256_set1_ps(s.sy[c]), sz = _mm256_set1_ps(s.sz[c]);
            const __m256 u =
                _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(sx, pvx), _mm256_mul_ps(sy, pvy)), _mm256_mul_ps(sz, pvz)), inv);
            ok = _mm256_and_ps(ok, _mm256_and_ps(_mm256_cmp_ps(u, vzero, _CMP_NLT_UQ), _mm256_cmp_ps(u, vone, _CMP_NGT_UQ)));
            const __m256 qx = _mm256_set1_ps(s.qx[c]), qy = _mm256_set1_ps(s.qy[c]), qz = _mm256_set1_ps(s.qz[c]);
            const __m256 v =
                _mm256_mul_ps(_mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(ddx, qx), _mm256_mul_ps(ddy, qy)), _mm256_mul_ps(ddz, qz)), inv);
            ok = _mm256_and_ps(ok, _mm256_and_ps(_mm256_cmp_ps(v, vzero, _CMP_NLT_UQ),
                                       _mm256_cmp_ps(_mm256_add_ps(u, v), vone, _CMP_NGT_UQ)));
            const __m256 dist = _mm256_mul_ps(_mm256_set1_ps(s.e2q[c]), inv);
            ok = _mm256_and_ps(ok, _mm256_and_ps(_mm256_cmp_ps(dist, vtmin, _CMP_GT_OQ), _mm256_cmp_ps(dist, vbest, _CMP_LT_OQ)));
            vbest = _mm256_blendv_ps(vbest, dist, ok);
        }
        _mm256_store_ps(in.best + r, vbest);
    }
}

// Whether raysAvx may run (the processor and the system: SDL's check). Made before main, read by the bake's threads.
const bool cpuHasAvx = SDL_HasAVX() == SDL_TRUE;

// One pose: for each vertex, the share of cosine-weighted rays over its hemisphere that the model's own triangles
// stop within `reach` (a hit counts less the farther it is: 1 - t / reach). The same bytes as testing every triangle
// in reach one ray at a time (bakePoseReference; vr_ao_bench reference): the candidates come from a grid but pass the
// same test, and the rays, eight or four at a time (raysAvx, raysSse), go through the same float operations in the same
// order, the ray-independent ones (s, q, e2 . q) done once per candidate; the nearest hit is the same whatever order
// the candidates come in. (The firing range's 112 models on one thread: 3.1 s, the reference's 24.9 s.)
void bakePose(const PoseJob& job, int pose, PoseScratch& s)
{
    const za::Array<glm::vec3, RAYS>& dirs = rayDirs;
    static_assert(RAYS % 4 == 0);
    const trivertx_t* tv = job.verts + static_cast<size_t>(pose) * job.numverts;
    const int nv = job.numverts;
    s.p.resize(static_cast<size_t>(nv));
    for(int i = 0; i < nv; i++)
    {
        s.p[i] = glm::vec3{tv[i].v[0], tv[i].v[1], tv[i].v[2]} * job.scale + job.origin;
    }
    const za::Vector<Tri>& tris = *job.tris;
    const size_t nt = tris.size();
    for(za::Vector<glm::vec3>* v : {&s.v0, &s.e1, &s.e2, &s.mid})
    {
        v->resize(nt);
    }
    s.rad.resize(nt);
    for(size_t t = 0; t < nt; t++)
    {
        const glm::vec3 a = s.p[tris[t].v[0]], b = s.p[tris[t].v[1]], c = s.p[tris[t].v[2]];
        s.v0[t] = a;
        s.e1[t] = b - a;
        s.e2[t] = c - a;
        s.mid[t] = (a + b + c) * (1.f / 3.f);
        const glm::vec3 m = s.mid[t];
        s.rad[t] = za::sqrt(za::max(glm::dot(a - m, a - m), glm::dot(b - m, b - m), glm::dot(c - m, c - m)));
    }
    const float reach = job.reach;
    const float tmin = reach * NEAREST_SHARE;
    unsigned char* out = job.out + static_cast<size_t>(pose) * nv;
    const Grid grid = buildGrid(s, reach);
    s.cand.resize(nt); // (room for every triangle: each vertex's candidates are written whether they pass or not)
    s.mark.clear();
    s.mark.resize(static_cast<size_t>(nv), -1);
    for(za::Vector<float>* v : {&s.e1x, &s.e1y, &s.e1z, &s.e2x, &s.e2y, &s.e2z, &s.sx, &s.sy, &s.sz, &s.qx, &s.qy, &s.qz, &s.e2q})
    {
        v->resize(nt);
    }
    alignas(32) float dx[RAYS], dy[RAYS], dz[RAYS], best[RAYS];
    for(int i = 0; i < nv; i++)
    {
        if(job.stop && job.stop->loadRelaxed())
        {
            return;
        }
        // Its own triangles and its neighbours' are left out: on a curved surface of byte-rounded vertices the rays
        // near its horizon would graze them (a speckle of false occlusion over the whole model).
        s.mark[i] = i;
        for(const int k : (*job.ring)[i])
        {
            s.mark[k] = i;
        }
        const float* nn = r_avertexnormals[za::min<int>(tv[i].lightnormalindex, NUMVERTEXNORMALS - 1)];
        const glm::vec3 n{nn[0], nn[1], nn[2]};
        const glm::vec3 pi = s.p[i];
        const glm::vec3 o = pi + n * (reach * LIFT_SHARE);
        int* const cand = s.cand.data();
        // (the same test as the reference's, for every triangle it reaches)
        int nc = 0;
        const auto consider = [&](int t, const glm::vec4& midRad) {
            const Tri& tr = tris[static_cast<size_t>(t)];
            const glm::vec3 d = glm::vec3{midRad} - pi;
            const float r = reach + midRad.w;
            // (without branches: a quarter of them pass, unpredictably)
            const int keep = static_cast<int>(!(glm::dot(d, d) > r * r)) & static_cast<int>(!(glm::dot(d, n) < -midRad.w)) &
                             static_cast<int>(s.mark[tr.v[0]] != i) & static_cast<int>(s.mark[tr.v[1]] != i) &
                             static_cast<int>(s.mark[tr.v[2]] != i);
            cand[nc] = t;
            nc += keep;
        };
        for(const int t : s.big)
        {
            consider(t, glm::vec4{s.mid[t], s.rad[t]});
        }
        const float qr = reach + grid.gridRadius + grid.margin;
        const int x0 = grid.cell(pi.x - qr, 0), x1 = grid.cell(pi.x + qr, 0);
        const int y0 = grid.cell(pi.y - qr, 1), y1 = grid.cell(pi.y + qr, 1);
        const int z0 = grid.cell(pi.z - qr, 2), z1 = grid.cell(pi.z + qr, 2);
        for(int z = z0; z <= z1; z++)
        {
            // The distance from the vertex to the cell's box (widened by the margin), along each axis.
            const float cz = grid.lo.z + static_cast<float>(z) * grid.size;
            const float gz = za::max(0.f, za::max(cz - grid.margin - pi.z, pi.z - (cz + grid.size + grid.margin)));
            for(int y = y0; y <= y1; y++)
            {
                const float cy = grid.lo.y + static_cast<float>(y) * grid.size;
                const float gy = za::max(0.f, za::max(cy - grid.margin - pi.y, pi.y - (cy + grid.size + grid.margin)));
                for(int x = x0; x <= x1; x++)
                {
                    const size_t c = static_cast<size_t>(x + grid.n[0] * (y + grid.n[1] * z));
                    if(s.cellStart[c] == s.cellStart[c + 1])
                    {
                        continue;
                    }
                    const float cx = grid.lo.x + static_cast<float>(x) * grid.size;
                    const float gx = za::max(0.f, za::max(cx - grid.margin - pi.x, pi.x - (cx + grid.size + grid.margin)));
                    const float within = reach + s.cellRadius[c] + grid.margin;
                    if(gx * gx + gy * gy + gz * gz > within * within)
                    {
                        continue;
                    }
                    for(int k = s.cellStart[c]; k < s.cellStart[c + 1]; k++)
                    {
                        consider(s.cellItems[static_cast<size_t>(k)], s.cellMidRad[static_cast<size_t>(k)]);
                    }
                }
            }
        }
        float occ = 0.f;
        if(nc > 0)
        {
            for(int c = 0; c < nc; c++)
            {
                const int t = s.cand[static_cast<size_t>(c)];
                const glm::vec3 sv = o - s.v0[t];
                const glm::vec3 q = glm::cross(sv, s.e1[t]);
                s.e1x[c] = s.e1[t].x;
                s.e1y[c] = s.e1[t].y;
                s.e1z[c] = s.e1[t].z;
                s.e2x[c] = s.e2[t].x;
                s.e2y[c] = s.e2[t].y;
                s.e2z[c] = s.e2[t].z;
                s.sx[c] = sv.x;
                s.sy[c] = sv.y;
                s.sz[c] = sv.z;
                s.qx[c] = q.x;
                s.qy[c] = q.y;
                s.qz[c] = q.z;
                s.e2q[c] = glm::dot(s.e2[t], q);
            }
            // Duff et al.'s frame round the normal.
            const float sign = za::copysign(1.f, n.z);
            const float a = -1.f / (sign + n.z);
            const float b = n.x * n.y * a;
            const glm::vec3 tx{1.f + sign * n.x * n.x * a, sign * b, -sign * n.x};
            const glm::vec3 ty{b, sign + n.y * n.y * a, -n.y};
            for(int r = 0; r < RAYS; r++)
            {
                const glm::vec3& l = dirs[r];
                const glm::vec3 d = tx * l.x + ty * l.y + n * l.z;
                dx[r] = d.x;
                dy[r] = d.y;
                dz[r] = d.z;
            }
            const RayInput in{&s, nc, dx, dy, dz, tmin, reach, best};
            if(job.avx)
            {
                raysAvx(in);
            }
            else
            {
                raysSse(in);
            }
            for(int r = 0; r < RAYS; r++)
            {
                if(best[r] < reach)
                {
                    const float f = best[r] / reach;
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
    // The disk cache (vr_ao_cache, read when it was queued): its folder, and what the bake task did with it.
    int cacheMode{0};
    za::String cacheRoot; // <gamedir>/cache/ao
    bool fromDisk{false}; // read, not baked
    bool rejected{false}; // a file was there, but not a whole one of this model's (baked again, and written over)
    bool written{false};
    int checked{0};       // vr_ao_cache 2: baked anyway and compared with the file: 1 the same, -1 different
    bool aborted{false};  // the game stopped part way
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

// ----------------------------------------------------------------------------
// The bakes kept on disk (vr_ao_cache): `<gamedir>/cache/ao/v<BAKE_VERSION>/<key>.ao`, as the normal maps' cache
// (vr_texcache.cpp) keeps its files. <key> is the SHA-256 of everything a bake reads: its numbers (RAYS, the shares,
// the rays' directions), Quake's vertex normals, and the model's poses (with their normal indices), triangles, scale
// and origin; so a changed model, or changed numbers, never reads an old file. A file is read and checked (magic,
// version, sizes, the whole key, its bytes' FNV-1a) on the bake task's thread, never the main thread; anything amiss
// is baked again and written over. Written beside its place and renamed (another copy of the game never reads half a
// file). The bake task's first write in each game directory removes the other versions' folders, and the oldest files
// past CACHE_BUDGET.

// Bump it when the bake's bytes or the file's layout change (not for a faster bake of the same bytes: the grid and the
// SIMD rays of 2026-10-06 kept version 1, checked against the reference and the files).
constexpr za::U32 BAKE_VERSION = 1;
constexpr za::U64 CACHE_BUDGET = 64ull << 20; // bytes in the version's folder (a game's full set is a few MB)
constexpr char FILE_MAGIC[4] = {'Q', 'V', 'R', 'A'};

struct FileHeader
{
    char magic[4];
    za::U32 version;
    za::U32 numverts;
    za::U32 numposes;
    za::U8 key[32];
    za::U64 payloadBytes;
    za::U64 payloadHash; // FNV-1a (64-bit) of the payload: numposes x numverts bytes, the poses in turn
};
static_assert(sizeof(FileHeader) == 64);

[[nodiscard]] za::U64 fnv64(const unsigned char* b, size_t n)
{
    za::U64 h = 1469598103934665603ull;
    for(size_t i = 0; i < n; i++)
    {
        h = (h ^ b[i]) * 1099511628211ull;
    }
    return h;
}

[[nodiscard]] sha256::Digest bakeKey(const BakeJob& job)
{
    za::Vector<unsigned char> bytes;
    const auto put = [&bytes](const void* data, size_t n) {
        const size_t at = bytes.size();
        bytes.resize(at + n);
        memcpy(bytes.data() + at, data, n);
    };
    const char tag[8] = {'q', 'v', 'r', 'a', 'o', 'k', 'e', 'y'};
    put(tag, sizeof(tag));
    const za::U32 counts[5] = {BAKE_VERSION, static_cast<za::U32>(RAYS), static_cast<za::U32>(job.numverts),
        static_cast<za::U32>(job.numposes), static_cast<za::U32>(job.tris.size())};
    put(counts, sizeof(counts));
    const float numbers[5] = {REACH_SHARE, REACH_MIN, REACH_MAX, NEAREST_SHARE, LIFT_SHARE};
    put(numbers, sizeof(numbers));
    put(rayDirs.data(), sizeof(glm::vec3) * RAYS);
    put(r_avertexnormals, sizeof(r_avertexnormals));
    put(&job.scale, sizeof(job.scale));
    put(&job.origin, sizeof(job.origin));
    put(job.verts.data(), job.verts.size() * sizeof(trivertx_t));
    put(job.tris.data(), job.tris.size() * sizeof(Tri));
    return sha256::of(bytes.data(), bytes.size());
}

[[nodiscard]] za::String versionDir(const za::String& root)
{
    return root + va("/v%u", BAKE_VERSION);
}

[[nodiscard]] za::String fileFor(const za::String& root, const sha256::Digest& key)
{
    char hex[65];
    sha256::toHex(key, hex);
    hex[32] = '\0'; // (128 bits name it; the whole key is checked in the file)
    return versionDir(root) + "/" + hex + ".ao";
}

enum class FileRead
{
    Missing,
    Rejected,
    Read
};

// The model's file, checked whole: its bytes into `vis` (numposes x numverts).
[[nodiscard]] FileRead readBake(const za::String& path, const BakeJob& job, const sha256::Digest& key, za::Vector<unsigned char>& vis)
{
    FILE* in = Sys_fopen(path.cStr(), "rb");
    if(!in)
    {
        return FileRead::Missing;
    }
    const size_t count = static_cast<size_t>(job.numposes) * job.numverts;
    FileHeader h;
    bool ok = fread(&h, 1, sizeof(h), in) == sizeof(h) && memcmp(h.magic, FILE_MAGIC, 4) == 0 && h.version == BAKE_VERSION &&
              h.numverts == static_cast<za::U32>(job.numverts) && h.numposes == static_cast<za::U32>(job.numposes) &&
              memcmp(h.key, key.bytes, sizeof(h.key)) == 0 && h.payloadBytes == count;
    if(ok)
    {
        vis.resize(count);
        char extra;
        ok = fread(vis.data(), 1, count, in) == count && fread(&extra, 1, 1, in) == 0 && fnv64(vis.data(), count) == h.payloadHash;
    }
    fclose(in);
    return ok ? FileRead::Read : FileRead::Rejected;
}

[[nodiscard]] bool writeBake(const za::String& path, const BakeJob& job, const sha256::Digest& key)
{
    FileHeader h{};
    memcpy(h.magic, FILE_MAGIC, 4);
    h.version = BAKE_VERSION;
    h.numverts = static_cast<za::U32>(job.numverts);
    h.numposes = static_cast<za::U32>(job.numposes);
    memcpy(h.key, key.bytes, sizeof(h.key));
    h.payloadBytes = job.vis.size();
    h.payloadHash = fnv64(job.vis.data(), job.vis.size());
    char suffix[32];
    snprintf(suffix, sizeof(suffix), ".%llx.tmp", static_cast<unsigned long long>(za::Clock::nowNanoseconds()) & 0xffffffffull);
    const za::String tmp = path + suffix;
    FILE* out = Sys_fopen(tmp.cStr(), "wb");
    if(!out)
    {
        return false;
    }
    bool ok = fwrite(&h, 1, sizeof(h), out) == sizeof(h) && fwrite(job.vis.data(), 1, job.vis.size(), out) == job.vis.size();
    ok = fclose(out) == 0 && ok;
    if(!ok || !files::rename(tmp.cStr(), path.cStr()))
    {
        files::remove(tmp.cStr());
        return false;
    }
    return true;
}

// The game directories' cache folders pruned this session (the bake task's alone: one runs at a time, each started
// after the last returned).
za::Vector<za::String> prunedRoots;

// The bake task's first write under a game directory: the other versions' folders removed (never read again), then
// the oldest files (by their last write) until the version's folder is within CACHE_BUDGET.
void pruneOnce(const za::String& root)
{
    if(za::find(prunedRoots.begin(), prunedRoots.end(), root) != prunedRoots.end())
    {
        return;
    }
    prunedRoots.pushBack(root);
    char keep[16];
    snprintf(keep, sizeof(keep), "v%u", BAKE_VERSION);
    za::Vector<za::String> others;
    files::forEachEntry(root.cStr(), [&](const char* name, bool isDirectory) {
        if(!isDirectory || strcmp(name, keep) != 0)
        {
            others.pushBack(root + "/" + name);
        }
    });
    for(const za::String& other : others)
    {
        files::removeAll(other.cStr());
    }
    struct Entry
    {
        za::String path;
        za::I64 time;
        za::U64 bytes;
    };
    za::Vector<Entry> entries;
    za::U64 total = 0;
    const za::String dir = versionDir(root);
    files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
        if(!isDirectory)
        {
            Entry e{dir + "/" + name, 0, 0};
            e.time = files::lastWriteTime(e.path.cStr());
            e.bytes = files::fileSize(e.path.cStr());
            total += e.bytes;
            entries.pushBack(ZA_MOVE(e));
        }
    });
    if(total <= CACHE_BUDGET)
    {
        return;
    }
    za::quickSort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.time < b.time; });
    for(const Entry& e : entries)
    {
        if(total <= CACHE_BUDGET * 3 / 4)
        {
            break;
        }
        if(files::remove(e.path.cStr()))
        {
            total -= e.bytes;
        }
    }
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
#else
    BelowNormal() {} // a const object needs a user-provided constructor
#endif
    BelowNormal(const BelowNormal&) = delete;
    BelowNormal& operator=(const BelowNormal&) = delete;
};

jobs::Site bakeSite{"ao bake"}; // (its parallelFor: vr_jobs_sites)

// How a model is baked: the fast bake (AVX when the processor has it), the fast bake four rays at a time, or the
// reference (vr_ao_bench compares them; the same bytes).
enum class BakeKind
{
    Fast,
    Sse,
    Reference
};

void bakeModel(BakeJob& job, int maxThreads, BakeKind kind);

void runBake(BakeJob& job)
{
    const auto t0 = za::Clock::nowNanoseconds();
    // The disk cache first: a file of this very bake is all it takes.
    sha256::Digest key{};
    za::String path;
    za::Vector<unsigned char> fromFile;
    FileRead read = FileRead::Missing;
    if(job.cacheMode > 0)
    {
        key = bakeKey(job);
        path = fileFor(job.cacheRoot, key);
        read = readBake(path, job, key, fromFile);
        job.rejected = read == FileRead::Rejected;
        if(read == FileRead::Read && job.cacheMode == 1)
        {
            job.vis = ZA_MOVE(fromFile);
            job.fromDisk = true;
            job.seconds = za::nanosecondsToSeconds(za::Clock::nowNanoseconds() - t0);
            return;
        }
    }
    bakeModel(job, BAKE_THREADS, BakeKind::Fast);
    job.seconds = za::nanosecondsToSeconds(za::Clock::nowNanoseconds() - t0);
    job.aborted = bakeQueue().stop.loadRelaxed();
    if(job.cacheMode <= 0 || job.aborted)
    {
        return;
    }
    if(read == FileRead::Read) // vr_ao_cache 2: baked anyway, and compared
    {
        job.checked = fromFile.size() == job.vis.size() && memcmp(fromFile.data(), job.vis.data(), job.vis.size()) == 0 ? 1 : -1;
        if(job.checked > 0)
        {
            return;
        }
    }
    pruneOnce(job.cacheRoot);
    files::createDirectories(versionDir(job.cacheRoot).cStr());
    job.written = writeBake(path, job, key);
}

// The bake itself, into job.vis: its poses shared out among up to `maxThreads` of the pool's threads (1: all on this
// one).
void bakeModel(BakeJob& job, int maxThreads, BakeKind kind)
{
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
    pj.reach = za::clamp(glm::distance(lo, hi) * REACH_SHARE, REACH_MIN, REACH_MAX);
    pj.out = job.vis.data();
    pj.stop = &bakeQueue().stop;
    pj.avx = kind == BakeKind::Fast && cpuHasAvx;
    const bool reference = kind == BakeKind::Reference;

    // The poses shared out between threads, every `threads`th to each (they only read the copy and write their own poses).
    const int poses = job.numposes;
    const int threads = za::clamp(za::min(jobs::hardwareThreads() / 2, maxThreads), 1, poses);
    const auto bakePoses = [&pj, poses, threads, reference](int first) {
        PoseScratch buffers;
        za::Vector<int> cand;
        for(int f = first; f < poses && !pj.stop->loadRelaxed(); f += threads)
        {
            if(reference)
            {
                bakePoseReference(pj, f, cand);
            }
            else
            {
                bakePose(pj, f, buffers);
            }
        }
    };
    if(threads == 1)
    {
        bakePoses(0);
        return;
    }
    jobs::parallelFor(bakeSite, static_cast<za::SizeT>(threads), 1, [&bakePoses](za::SizeT t0, za::SizeT t1) {
        const BelowNormal low;
        for(int first = static_cast<int>(t0); first < static_cast<int>(t1); first++)
        {
            bakePoses(first);
        }
    });
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
        if(job->fromDisk)
        {
            diskHits++;
            diskSeconds += job->seconds;
        }
        else
        {
            bakeSeconds += job->seconds;
            bakeModels++;
            if(job->seconds > slowestSeconds)
            {
                slowestSeconds = job->seconds;
                slowestModel = job->name;
            }
            diskMisses += job->cacheMode > 0 && job->checked == 0;
            diskRejected += job->rejected;
            diskWritten += job->written;
            checkSame += job->checked > 0;
            if(job->checked < 0)
            {
                checkDiffered++;
                Con_Warning("vr_ao_cache 2: %s's bake differs from its file (%d of %d so far)\n", job->name.cStr(), checkDiffered,
                    checkSame + checkDiffered);
            }
        }
        double mean = 0.0;
        za::U32 texels = 2166136261u; // (FNV-1a: the same bake whatever threads made it)
        for(const unsigned char v : entry.vis)
        {
            mean += v;
            texels = (texels ^ v) * 16777619u;
        }
        mean /= za::max<size_t>(entry.vis.size(), 1) * 255.0;
        Con_DPrintf("vr_ao: %s: %d poses x %d vertices, %d triangles %s in %.1f ms, open %.2f on average (%08x)\n",
            job->name.cStr(), job->numposes, job->numverts, static_cast<int>(job->tris.size()),
            job->fromDisk ? "read from the disk cache" : "baked", job->seconds * 1000.0, mean, texels);
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

namespace
{

// A bake's copy of a model's poses and triangles (the bake reads nothing else of the model's).
[[nodiscard]] za::UniquePtr<BakeJob> makeJob(qmodel_t* model, const aliashdr_t* hdr, za::U64 hash)
{
    const size_t count = static_cast<size_t>(hdr->numposes) * hdr->numverts;
    auto job = za::makeUnique<BakeJob>();
    job->name = model->name;
    job->model = model;
    job->hash = hash;
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
    return job;
}

[[nodiscard]] bool bakeable(const qmodel_t* model, const aliashdr_t* hdr)
{
    return model && hdr && hdr->poseverttype == aliashdr_t::PV_QUAKE1 && !hdr->nextsurface && hdr->numverts > 0 &&
           hdr->numposes > 0 && hdr->numindexes >= 3;
}

} // namespace

extern "C" const unsigned char* VR_AliasVertexAO(qmodel_t* model, const void* aliashdr)
{
    const auto* hdr = static_cast<const aliashdr_t*>(aliashdr);
    if(!bakeable(model, hdr))
    {
        return nullptr;
    }
    if(vr_mock_fast.value >= 2.f)
    {
        return nullptr; // a test run that doesn't draw (VR_SkipScreen): not seconds of every core's time at its start
    }
    const auto t0 = za::Clock::nowNanoseconds();
    struct Timer
    {
        za::I64 start;
        ~Timer() { queueSeconds += za::nanosecondsToSeconds(za::Clock::nowNanoseconds() - start); }
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
    {
        za::LockGuard lock(q.mutex); // (the task is started outside it)
        if(const auto qi = q.queued.find(za::StringView{model->name}); qi != q.queued.end() && qi->second == h)
        {
            return nullptr; // on its way
        }
        auto job = makeJob(model, hdr, h);
        job->cacheMode = static_cast<int>(za::clamp(vr_ao_cache.value, 0.f, 2.f));
        job->cacheRoot = za::String{com_gamedir} + "/cache/ao";
        q.queued[model->name] = h;
        q.pending.pushBack(ZA_MOVE(job));
        if(q.running || q.stop.loadSeqCst())
        {
            return nullptr; // (the task under way takes it next)
        }
        q.running = true;
    }
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
    Con_Printf("vr_ao: models' own occlusion: %d baked in %.1f ms on the worker (slowest %s, %.1f ms), %d read from the "
               "disk cache in %.1f ms, %d from memory; %.1f ms on the main thread\n",
        bakeModels, bakeSeconds * 1000.0, slowestModel.cStr(), slowestSeconds * 1000.0, diskHits, diskSeconds * 1000.0,
        bakeHits, queueSeconds * 1000.0);
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

// vr_ao_finish: waits here for the models' occlusion bakes under way (the map's models, a spawn's), then takes them in:
// a benchmark's set-up, so that a load's background work never runs inside its measured window.
namespace
{
void finish_f()
{
    BakeQueue& q = bakeQueue();
    const double t0 = Sys_DoubleTime();
    int queued = 0;
    {
        za::LockGuard lock(q.mutex);
        queued = static_cast<int>(q.queued.size());
    }
    if(q.runner.valid())
    {
        q.runner.wait(); // (only this thread queues bakes: none is added meanwhile)
    }
    integrateBakes();
    Con_Printf("vr_ao_finish: %d models' bakes waited for, %.1f ms\n", queued, (Sys_DoubleTime() - t0) * 1000.0);
}
} // namespace

namespace
{
// vr_ao_cache_info: the models' bakes kept on disk this session, and the folder's files.
void cacheInfo_f()
{
    Con_Printf("vr_ao_cache %d: %d read from disk (%.1f ms on the bake task, %.2f ms each), %d baked for want of a file (%d "
               "of them rejected: not whole, or not this model's), %d written\n",
        static_cast<int>(vr_ao_cache.value), diskHits, diskSeconds * 1000.0, diskHits ? diskSeconds * 1000.0 / diskHits : 0.0,
        diskMisses, diskRejected, diskWritten);
    if(checkSame + checkDiffered > 0)
    {
        Con_Printf("vr_ao_cache 2: %d bakes compared with their files, %d differed\n", checkSame + checkDiffered, checkDiffered);
    }
    const za::String dir = versionDir(za::String{com_gamedir} + "/cache/ao");
    int count = 0;
    za::U64 bytes = 0;
    files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
        if(!isDirectory)
        {
            count++;
            bytes += files::fileSize((dir + "/" + name).cStr());
        }
    });
    Con_Printf("%s: %d files, %.2f MB (at most %.0f MB kept)\n", dir.cStr(), count, bytes / 1048576.0, CACHE_BUDGET / 1048576.0);
}
} // namespace

namespace
{
// vr_ao_bench [name part] [sse] [reference]: the loaded models' occlusion (those whose names hold the part) baked again
// here, on the main thread alone, and timed (nothing is kept): the fast bake (eight rays at a time with AVX); with
// "sse", the fast bake four rays at a time too; with "reference", the brute-force bake too (every triangle, one ray at
// a time). Each other bake's bytes are compared with the fast bake's: they must be the same. Each model's bytes' FNV-1a,
// as developer 1 prints for the bakes.
void bench_f()
{
    const char* part = "";
    bool kinds[3] = {true, false, false}; // Fast, Sse, Reference
    for(int a = 1; a < Cmd_Argc(); a++)
    {
        if(!strcmp(Cmd_Argv(a), "sse"))
        {
            kinds[1] = true;
        }
        else if(!strcmp(Cmd_Argv(a), "reference"))
        {
            kinds[2] = true;
        }
        else
        {
            part = Cmd_Argv(a);
        }
    }
    static constexpr const char* names[3] = {"fast", "sse", "reference"};
    double total[3] = {};
    int models = 0, differed = 0;
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        qmodel_t* m = cl.model_precache[i];
        if(m->type != mod_alias || !strstr(m->name, part))
        {
            continue;
        }
        const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(m));
        if(!bakeable(m, hdr))
        {
            continue;
        }
        auto job = makeJob(m, hdr, 0);
        za::Vector<unsigned char> fast;
        double ms[3] = {};
        bool same = true;
        for(int k = 0; k < 3; k++)
        {
            if(!kinds[k])
            {
                continue;
            }
            const auto t0 = za::Clock::nowNanoseconds();
            bakeModel(*job, 1, static_cast<BakeKind>(k));
            ms[k] = za::nanosecondsToSeconds(za::Clock::nowNanoseconds() - t0) * 1000.0;
            total[k] += ms[k];
            if(k == 0)
            {
                fast = ZA_MOVE(job->vis);
            }
            else
            {
                same = same && fast.size() == job->vis.size() && memcmp(fast.data(), job->vis.data(), fast.size()) == 0;
            }
        }
        za::U32 texels = 2166136261u;
        for(const unsigned char v : fast)
        {
            texels = (texels ^ v) * 16777619u;
        }
        models++;
        differed += !same;
        char line[256];
        int at = snprintf(line, sizeof(line), "vr_ao_bench: %s: %d poses x %d vertices, %d triangles: %s %.1f ms", m->name,
            job->numposes, job->numverts, static_cast<int>(job->tris.size()), cpuHasAvx ? "avx" : "sse", ms[0]);
        for(int k = 1; k < 3; k++)
        {
            if(kinds[k] && at > 0 && at < static_cast<int>(sizeof(line)))
            {
                at += snprintf(line + at, sizeof(line) - static_cast<size_t>(at), ", %s %.1f ms", names[k], ms[k]);
            }
        }
        Con_Printf("%s%s (%08x)\n", line, kinds[1] || kinds[2] ? (same ? ", the same" : ", DIFFERENT") : "", texels);
    }
    Con_Printf("vr_ao_bench: %d models, on one thread: fast (%s) %.1f ms", models, cpuHasAvx ? "avx" : "sse", total[0]);
    for(int k = 1; k < 3; k++)
    {
        if(kinds[k])
        {
            Con_Printf(", %s %.1f ms (%.1fx)", names[k], total[k], total[k] / za::max(total[0], 1e-3));
        }
    }
    if(kinds[1] || kinds[2])
    {
        Con_Printf("; %d differed\n", differed);
    }
    else
    {
        Con_Printf("\n");
    }
}
} // namespace

void ao::init()
{
    Cmd_AddCommand("vr_ao_bench", bench_f);
    Cmd_AddCommand("vr_ao_show", show_f);
    Cmd_AddCommand("vr_ao_cache_info", cacheInfo_f);
    Cmd_AddCommand("vr_ao_finish", finish_f);
}

void ao::upload()
{
    // (through a teleporter, vr_portals.cpp: the last frame's occluders, chosen round the eyes, not there)
    if(builtFrame != host_framecount && !portals::viewing())
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
