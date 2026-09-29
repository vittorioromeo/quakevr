// vr_held.cpp -- see vr_held.hpp.

#include "vr_held.hpp"
#include "vr_carry2h.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_fatigue.hpp"
#include "vr_grip.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_physics.hpp"
#include "vr_progs.hpp"
#include "vr_props.hpp"
#include "vr_protocol.hpp"
#include "vr_throw.hpp"
#include "vr_trace.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace qvr;

namespace
{

// How vr_render.cpp draws a model's vertices, in its axes relative to the entity's origin: the
// networked scale about model_scale_origin, the weapon scaling, then the model's own, the post
// scale and the networked offset on raw vertices (brush models: the offset, then the scale).
struct DrawnTransform
{
    bool alias{false};
    weapons::ModelTransform t{};
    glm::vec3 so{0.f}, hs{1.f};
    glm::vec3 netScale{1.f}, scaleOrigin{0.f}, offset{0.f};

    DrawnTransform(const qmodel_t* model, const glm::vec3& scale, const glm::vec3& origin, const glm::vec3& off)
        : alias{model->type == mod_alias}, netScale{glm::vec3{1.f} + scale}, scaleOrigin{origin}, offset{off}
    {
        if(alias)
        {
            const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
            t = weapons::modelTransform(model);
            so = glm::vec3{hdr->scale_origin[0], hdr->scale_origin[1], hdr->scale_origin[2]};
            hs = glm::vec3{hdr->scale[0], hdr->scale[1], hdr->scale[2]};
        }
    }

    // A vertex as stored: an alias model's raw one (0..255 an axis), a brush model's in its space.
    [[nodiscard]] glm::vec3 stored(const glm::vec3& v) const
    {
        glm::vec3 p;
        if(!alias)
        {
            p = v + offset;
        }
        else if(t.active)
        {
            p = so + hs * ((v + offset) * t.scale);
            p = (t.offset + p) * t.k;
        }
        else
        {
            p = so + hs * (v + offset);
        }
        return scaleOrigin + (p - scaleOrigin) * netScale;
    }

    // A point in the model's own space (its bounds).
    [[nodiscard]] glm::vec3 modelPoint(const glm::vec3& v) const { return stored(alias ? (v - so) / hs : v); }
};

struct Triangle
{
    glm::vec3 p[3];
};

// The surface of the model `ent` is drawn with, as triangles in its axes relative to its origin: an
// alias model's current frame (its first pose), a brush model's faces. False if it has none to give.
bool drawnTriangles(edict_t* ent, const qmodel_t* model, std::vector<Triangle>& out)
{
    using namespace progs;
    const FieldOffsets& f = fields();
    const DrawnTransform xf{model, fieldVec(ent, f.model_scale), fieldVec(ent, f.model_scale_origin), fieldVec(ent, f.model_offset)};
    out.clear();

    if(model->type == mod_brush)
    {
        for(int i = 0; i < model->nummodelsurfaces; i++)
        {
            const msurface_t& surf = model->surfaces[model->firstmodelsurface + i];
            const auto vertex = [&](int k) {
                const int e = model->surfedges[surf.firstedge + k];
                const mvertex_t& v = model->vertexes[e >= 0 ? model->edges[e].v[0] : model->edges[-e].v[1]];
                return xf.stored(glm::vec3{v.position[0], v.position[1], v.position[2]});
            };
            for(int k = 2; k < surf.numedges; k++)
            {
                out.push_back({{vertex(0), vertex(k - 1), vertex(k)}});
            }
        }
        return !out.empty();
    }

    if(model->type != mod_alias)
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || !hdr->indexes || !hdr->meshdesc || hdr->numframes <= 0)
    {
        return false;
    }
    const int frame = static_cast<int>(ent->v.frame);
    const int pose = hdr->frames[frame >= 0 && frame < hdr->numframes ? frame : 0].firstpose;
    const auto* base = reinterpret_cast<const byte*>(hdr);
    const auto* verts = reinterpret_cast<const trivertx_t*>(base + hdr->vertexes) + pose * hdr->numverts;
    const auto* mesh = reinterpret_cast<const aliasmesh_t*>(base + hdr->meshdesc);
    const auto* indexes = reinterpret_cast<const unsigned short*>(base + hdr->indexes);
    const auto vertex = [&](int i) {
        const trivertx_t& v = verts[mesh[indexes[i]].vertindex];
        return xf.stored(glm::vec3{v.v[0], v.v[1], v.v[2]});
    };
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        out.push_back({{vertex(i), vertex(i + 1), vertex(i + 2)}});
    }
    return !out.empty();
}

// The drawn fist (hand_base.mdl and the finger models, all curled: grip and trigger held) as seen
// along its palm's normal: how far its surface reaches towards the palm's side, in cm from the grip
// (the tracked hand's pose, the controller's), over a 1 cm grid of the hand's forward and up. Measured
// from the drawn models' triangles as the view places them (vr_view.cpp: the finger offsets and the
// fist's angles), at the defaults the hand offsets were tuned at (vr_world_scale 1.25,
// vr_gunmodelscale 0.7); the hand scales with weapons::offsetScale(). Mirrored, the off hand's is the
// same. The grip is at the fist's front top: the fingers curl round below and behind it, their
// knuckles furthest out (4.2 cm).
constexpr float none = -99.f;
constexpr int fistForward0 = -19; // the first column's cell: -19..-18 cm along the hand's forward
constexpr int fistUp0 = -2;       // the first row's: -2..-1 cm up
constexpr int fistColumns = 19;
constexpr int fistRows = 12;
constexpr float fistSurface[fistRows][fistColumns] = {
    { none,  none,  none,  none,  none,  none,  none,  -0.3f,   1.0f,   1.9f,   2.7f,   3.7f,   3.8f,   3.0f,  none,  none,  none,  none,  none}, // up -2
    { none,  none,  none,  none,  -2.2f,  -1.3f,   0.4f,   1.9f,   2.6f,   3.2f,   3.7f,   4.1f,   4.2f,   3.9f,   3.0f,  none,  none,  none,  none}, // up -3
    { none,  none,  none,  -1.8f,  -0.8f,   1.5f,   2.0f,   2.5f,   3.0f,   3.2f,   3.6f,   3.8f,   4.1f,   3.9f,   3.6f,   3.0f,  -0.4f,  -1.7f,  none}, // up -4
    { -1.7f,  -1.4f,  -1.0f,  -0.4f,   0.7f,   1.6f,   2.1f,   2.6f,   3.0f,   3.2f,   3.3f,   3.4f,   4.0f,   3.9f,   3.6f,   3.2f,   1.7f,   0.8f,  -0.3f}, // up -5
    { -0.8f,  -0.7f,  -0.7f,  -0.4f,   0.9f,   1.7f,   2.2f,   2.7f,   3.0f,   3.1f,   2.7f,   2.9f,   3.0f,   3.8f,   3.6f,   2.7f,   1.8f,   0.8f,  -0.3f}, // up -6
    { -0.8f,  -0.8f,  -0.8f,  -0.5f,   0.9f,   1.7f,   2.2f,   2.7f,   2.8f,   2.4f,   0.9f,   2.6f,   2.7f,   2.9f,   3.0f,   2.2f,   0.9f,  -0.2f,  none}, // up -7
    { -0.9f,  -0.8f,  -0.8f,  -0.6f,   0.3f,   1.2f,   1.9f,   2.3f,   2.1f,   1.7f,   0.9f,   2.5f,   2.6f,   2.7f,   2.7f,   1.7f,   0.4f,  -0.9f,  none}, // up -8
    { -1.0f,  -0.9f,  -0.9f,  -0.7f,   0.2f,   0.8f,   1.3f,   1.4f,   1.3f,   1.1f,   0.4f,   2.0f,   2.2f,   2.4f,   2.4f,   1.2f,  -0.4f,  -1.7f,  none}, // up -9
    { none,  -1.9f,  -1.3f,  -0.7f,   0.2f,   0.5f,   0.6f,   0.8f,   0.7f,   0.4f,  -0.1f,   1.5f,   1.6f,   1.8f,   1.8f,   0.8f,  -0.7f,  none,  none}, // up -10
    { none,  none,  none,  -2.6f,  -0.9f,  -0.2f,   0.0f,   0.2f,   0.2f,  -0.3f,  -0.5f,   0.8f,   1.0f,   1.2f,   1.3f,  -0.1f,  -2.1f,  none,  none}, // up -11
    { none,  none,  none,  none,  none,  none,  -2.2f,  -1.6f,  -1.0f,  -0.7f,  -0.7f,   0.1f,   0.4f,   0.6f,   0.7f,  -0.5f,  none,  none,  none}, // up -12
    { none,  none,  none,  none,  none,  none,  none,  none,  none,  none,  none,  none,  none,  -0.0f,  -0.0f,  none,  none,  none,  none}, // up -13
};

// Extra gap for the model (vr_held_fit_gaps: "name=cm ...", a name matching the end of the model's).
float modelGap(const qmodel_t* model)
{
    const char* list = vr_held_fit_gaps.string;
    const size_t nameLength = strlen(model->name);
    while(list && *list)
    {
        while(*list == ' ' || *list == ',' || *list == ';')
        {
            list++;
        }
        const char* eq = strchr(list, '=');
        if(!eq)
        {
            break;
        }
        const size_t length = static_cast<size_t>(eq - list);
        const float cm = static_cast<float>(atof(eq + 1));
        if(length > 0 && length <= nameLength && !q_strncasecmp(model->name + nameLength - length, list, static_cast<int>(length)))
        {
            return cm;
        }
        list = eq + 1;
        while(*list && *list != ' ' && *list != ',' && *list != ';')
        {
            list++;
        }
    }
    return 0.f;
}

// The held things' shape tests' buffers (the server's frame and the client's view: the main thread). Each function its
// own: drawnVertices runs inside others' tests (a hull being made).
struct HeldScratch
{
    std::vector<Triangle> verticesTris;   // drawnVertices
    std::vector<Triangle> distanceTris;   // surfaceDistance
    std::vector<Triangle> fistTris;       // fistContact: the thing's triangles
    std::vector<glm::vec4> fistLocal;     // the fist's spheres in the thing's axes
    std::vector<float> nearest;           // each sphere's nearest distance
    std::vector<glm::vec3> nearestAt;     // and point
    std::vector<char> inside;             // and whether its middle is inside
    std::vector<glm::vec4> grabSpheres;   // grabTouch: the fist's spheres in the world
    std::vector<Triangle> fitTris;        // surfaceFit
    auto members()
    {
        return std::tie(verticesTris, distanceTris, fistTris, fistLocal, nearest, nearestAt, inside, grabSpheres, fitTris);
    }
};
mem::Scratch<HeldScratch> scratch{"held"};

} // namespace

namespace qvr::held
{

glm::mat3 axesFromAngles(const float* angles, bool brush)
{
    vec3_t a{brush ? angles[0] : -angles[0], angles[1], angles[2]}, f, r, u;
    AngleVectors(a, f, r, u);
    return glm::mat3{glm::vec3{f[0], f[1], f[2]}, -glm::vec3{r[0], r[1], r[2]}, glm::vec3{u[0], u[1], u[2]}};
}

void anglesFromAxes(const glm::mat3& m, float* out, bool brush)
{
    const glm::vec3 a = hands::anglesFromVectors(glm::normalize(m[0]), glm::normalize(m[2]));
    out[0] = brush ? a.x : -a.x;
    out[1] = a.y;
    out[2] = a.z;
}

void modelBox(const qmodel_t* model, const glm::vec3& scale, const glm::vec3& scaleOrigin, const glm::vec3& offset,
    glm::vec3& lo, glm::vec3& hi)
{
    lo = glm::vec3{0.f};
    hi = glm::vec3{0.f};
    if(!model || (model->type != mod_alias && model->type != mod_brush))
    {
        return;
    }

    const DrawnTransform xf{model, scale, scaleOrigin, offset};
    lo = glm::vec3{1e9f};
    hi = glm::vec3{-1e9f};
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 p = xf.modelPoint(glm::vec3{(i & 1) ? model->maxs[0] : model->mins[0],
            (i & 2) ? model->maxs[1] : model->mins[1], (i & 4) ? model->maxs[2] : model->mins[2]});
        lo = glm::min(lo, p);
        hi = glm::max(hi, p);
    }
}

bool drawnBox(int num, glm::vec3& lo, glm::vec3& hi)
{
    const entity_t& e = cl_entities[num];
    if(!e.model || (e.model->type != mod_alias && e.model->type != mod_brush))
    {
        return false;
    }
    const client::EntityVr* net = client::entityVr(num);
    const glm::vec3 zero{0.f};
    modelBox(e.model, net ? net->scale : zero, net ? net->scaleOrigin : zero, net ? net->offset : zero, lo, hi);
    lo *= ENTSCALE_DECODE(e.scale); // and Ironwail's scale
    hi *= ENTSCALE_DECODE(e.scale);
    return true;
}

glm::vec3 drawnCentre(int num)
{
    const entity_t& e = cl_entities[num];
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    glm::vec3 lo, hi;
    if(!drawnBox(num, lo, hi))
    {
        return origin;
    }
    const bool brush = e.model->type == mod_brush;
    return origin + axesFromAngles(e.angles, brush) * ((lo + hi) * 0.5f);
}

bool modelVertices(const qmodel_t* model, bool mirrored, std::vector<glm::vec3>& out)
{
    out.clear();
    if(!model || model->type != mod_alias)
    {
        return false;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes || hdr->numframes <= 0 || hdr->numverts <= 0)
    {
        return false;
    }
    const DrawnTransform xf{model, glm::vec3{0.f}, glm::vec3{0.f}, glm::vec3{0.f}};
    const auto* verts = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes) +
                        hdr->frames[0].firstpose * hdr->numverts;
    out.reserve(static_cast<size_t>(hdr->numverts));
    for(int i = 0; i < hdr->numverts; i++)
    {
        glm::vec3 p = xf.stored(glm::vec3{verts[i].v[0], verts[i].v[1], verts[i].v[2]});
        if(mirrored)
        {
            p.y = -p.y;
        }
        out.push_back(p);
    }
    return true;
}

bool drawnVertices(edict_t* ent, std::vector<glm::vec3>& out)
{
    out.clear();
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    std::vector<Triangle>& triangles = scratch.verticesTris;
    if(!model || !drawnTriangles(ent, model, triangles))
    {
        return false;
    }
    out.reserve(triangles.size() * 3);
    for(const Triangle& t : triangles)
    {
        out.insert(out.end(), t.p, t.p + 3);
    }
    return true;
}

namespace
{

// The point of the triangle nearest p (Ericson, Real-Time Collision Detection, 5.1.5).
[[nodiscard]] glm::vec3 nearestOn(const Triangle& t, const glm::vec3& p)
{
    const glm::vec3 &a = t.p[0], &b = t.p[1], &c = t.p[2];
    const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
    const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
    if(d1 <= 0.f && d2 <= 0.f)
    {
        return a;
    }
    const glm::vec3 bp = p - b;
    const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
    if(d3 >= 0.f && d4 <= d3)
    {
        return b;
    }
    const float vc = d1 * d4 - d3 * d2;
    if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
    {
        return a + ab * (d1 / (d1 - d3));
    }
    const glm::vec3 cp = p - c;
    const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
    if(d6 >= 0.f && d5 <= d6)
    {
        return c;
    }
    const float vb = d5 * d2 - d1 * d6;
    if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
    {
        return a + ac * (d2 / (d2 - d6));
    }
    const float va = d3 * d6 - d5 * d4;
    if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
    {
        return b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
    }
    const float denom = 1.f / (va + vb + vc);
    return a + ab * (vb * denom) + ac * (vc * denom);
}

// `ent`'s drawn triangles in its axes relative to its origin, and those axes and origin.
bool entityTriangles(edict_t* ent, std::vector<Triangle>& out, glm::mat3& axes, glm::vec3& origin)
{
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(!model || !drawnTriangles(ent, model, out))
    {
        return false;
    }
    axes = axesFromAngles(ent->v.angles, model->type == mod_brush);
    origin = glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    return true;
}

// The box `ent`'s model is drawn in (the networked scale and offset), in its axes relative to its origin.
void drawnBox(edict_t* ent, glm::vec3& lo, glm::vec3& hi)
{
    using namespace progs;
    const FieldOffsets& f = fields();
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    lo = glm::vec3{ent->v.mins[0], ent->v.mins[1], ent->v.mins[2]};
    hi = glm::vec3{ent->v.maxs[0], ent->v.maxs[1], ent->v.maxs[2]};
    if(model && (model->type == mod_alias || model->type == mod_brush))
    {
        const glm::vec3 zero{0.f};
        modelBox(model, f.model_scale >= 0 ? fieldVec(ent, f.model_scale) : zero,
            f.model_scale_origin >= 0 ? fieldVec(ent, f.model_scale_origin) : zero,
            f.model_offset >= 0 ? fieldVec(ent, f.model_offset) : zero, lo, hi);
    }
}

// Whether the world point `p` is within `margin` of the box `ent` is drawn in.
bool nearDrawnBox(edict_t* ent, const glm::vec3& p, float margin)
{
    glm::vec3 lo, hi;
    drawnBox(ent, lo, hi);
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    const glm::mat3 axes = axesFromAngles(ent->v.angles, model && model->type == mod_brush);
    const glm::vec3 local = glm::transpose(axes) * (p - glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]});
    return glm::all(glm::lessThanEqual(glm::abs(local - (lo + hi) * 0.5f), (hi - lo) * 0.5f + glm::vec3{margin}));
}

// The fists in their hands' frames (setFist).
std::vector<glm::vec4> fists[2];

// A sphere round all of `spheres`: its middle, and its radius.
float bounds(const std::vector<glm::vec4>& spheres, glm::vec3& centre)
{
    centre = glm::vec3{0.f};
    for(const glm::vec4& s : spheres)
    {
        centre += glm::vec3{s};
    }
    centre /= static_cast<float>(std::max<size_t>(spheres.size(), 1));
    float bound = 0.f;
    for(const glm::vec4& s : spheres)
    {
        bound = std::fmax(bound, glm::distance(glm::vec3{s}, centre) + s.w);
    }
    return bound;
}

} // namespace

float surfaceDistance(edict_t* ent, const glm::vec3& point, glm::vec3* nearest)
{
    std::vector<Triangle>& triangles = scratch.distanceTris;
    glm::mat3 axes;
    glm::vec3 origin;
    if(!entityTriangles(ent, triangles, axes, origin))
    {
        return -1.f;
    }
    const glm::vec3 p = glm::transpose(axes) * (point - origin);
    float best = std::numeric_limits<float>::max();
    glm::vec3 at{0.f};
    for(const Triangle& t : triangles)
    {
        const glm::vec3 q = nearestOn(t, p);
        const float d = glm::distance(p, q);
        if(d < best)
        {
            best = d;
            at = q;
        }
    }
    if(nearest)
    {
        *nearest = origin + axes * at;
    }
    return best;
}

void setFist(int hand, const std::vector<glm::vec4>& spheres)
{
    if(hand >= 0 && hand < 2)
    {
        fists[hand] = spheres;
    }
}

void fistInWorld(int hand, const glm::vec3& pos, const glm::vec3& angles, std::vector<glm::vec4>& out)
{
    out.clear();
    if(hand < 0 || hand > 1)
    {
        return;
    }
    const glm::mat3 axes = axesFromAngles(&angles[0], true);
    for(const glm::vec4& s : fists[hand])
    {
        out.push_back(glm::vec4{pos + axes * glm::vec3{s}, s.w});
    }
}

bool fistContact(edict_t* ent, const std::vector<glm::vec4>& spheres, float reach, FistContact& out)
{
    out = FistContact{};
    out.gap = std::numeric_limits<float>::max();
    std::vector<Triangle>& triangles = scratch.fistTris;
    glm::mat3 axes;
    glm::vec3 origin;
    if(spheres.empty() || !entityTriangles(ent, triangles, axes, origin))
    {
        return false;
    }

    // The fist in the thing's axes, and a sphere round all of it.
    std::vector<glm::vec4>& local = scratch.fistLocal;
    local.clear();
    for(const glm::vec4& s : spheres)
    {
        local.push_back(glm::vec4{glm::transpose(axes) * (glm::vec3{s} - origin), s.w});
    }
    glm::vec3 centre;
    const float bound = bounds(local, centre);

    // Each sphere against the triangles near the fist (within twice its bounds and the reach: every triangle a sphere
    // near the surface is nearest to, and the nearest to any sphere sunk in). A sphere's middle is inside when its
    // nearest triangle faces away from it.
    std::vector<float>& nearest = scratch.nearest;
    std::vector<glm::vec3>& nearestAt = scratch.nearestAt;
    std::vector<char>& inside = scratch.inside;
    nearest.assign(local.size(), std::numeric_limits<float>::max());
    nearestAt.assign(local.size(), glm::vec3{0.f});
    inside.assign(local.size(), 0);
    // Which way the triangles are wound (a brush model's faces one way, an alias model's the other): the sign of the
    // volume they enclose.
    float volume = 0.f;
    for(const Triangle& t : triangles)
    {
        volume += glm::dot(t.p[0], glm::cross(t.p[1], t.p[2]));
    }
    const float outwards = volume < 0.f ? -1.f : 1.f;
    const float keep = 2.f * bound + std::fmax(reach, 0.f);
    bool any = false;
    for(const Triangle& t : triangles)
    {
        const glm::vec3 mid = (t.p[0] + t.p[1] + t.p[2]) / 3.f;
        const float size =
            std::fmax(glm::distance(mid, t.p[0]), std::fmax(glm::distance(mid, t.p[1]), glm::distance(mid, t.p[2])));
        if(glm::distance(mid, centre) - size > keep)
        {
            continue;
        }
        any = true;
        const glm::vec3 n = outwards * glm::cross(t.p[1] - t.p[0], t.p[2] - t.p[0]);
        for(size_t i = 0; i < local.size(); i++)
        {
            const glm::vec3 c{local[i]};
            const glm::vec3 q = nearestOn(t, c);
            const float d = glm::distance(c, q);
            if(d < nearest[i])
            {
                nearest[i] = d;
                nearestAt[i] = q;
                inside[i] = glm::dot(c - q, n) < 0.f;
            }
        }
    }
    if(!any)
    {
        return false;
    }
    for(size_t i = 0; i < local.size(); i++)
    {
        if(nearest[i] == std::numeric_limits<float>::max())
        {
            continue;
        }
        const float gap = inside[i] ? -nearest[i] - local[i].w : nearest[i] - local[i].w;
        if(gap < out.gap)
        {
            out.gap = gap;
            out.sphere = static_cast<int>(i);
            out.from = glm::vec3{spheres[i]};
            out.at = origin + axes * nearestAt[i];
        }
    }
    return out.sphere >= 0;
}

bool grabTouch(edict_t* ent, edict_t* player, int hand, float slack)
{
    using namespace progs;
    const FieldOffsets& f = fields();
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    const int posField = hand == 0 ? f.offhandpos : f.handpos;
    const int rotField = hand == 0 ? f.offhandrot : f.handrot;
    if(posField < 0 || rotField < 0)
    {
        return false;
    }
    const glm::vec3 pos = fieldVec(player, posField);
    const glm::vec3 angles = fieldVec(player, rotField);
    const float m2u = units::metresToUnits();
    const bool debug = vr_debug_carry.value || vr_debug_physics_shapes.value;

    if(fists[hand].empty())
    {
        // Not known (no jointed hand model): the old test, the hand's point in its box and near its surface.
        constexpr float legacyReach = 0.08f; // metres
        if(!physics::pointInModelBox(ent, pos, 2.f))
        {
            return false;
        }
        glm::vec3 nearest{0.f};
        const float distance = surfaceDistance(ent, pos, &nearest);
        noteCarryProbe(hand, ent, pos, distance, nearest, legacyReach * m2u);
        return distance < 0.f || distance <= legacyReach * m2u;
    }

    std::vector<glm::vec4>& spheres = scratch.grabSpheres;
    fistInWorld(hand, pos, angles, spheres);
    const float allowed = vr_carry_grab_bias.value * 0.01f * m2u + slack;
    // Nothing of it near (the fist's bounds farther than it may be from the box the thing is drawn in: not its entity
    // box, which for the brush-model ammo and health boxes is a small cube round their origin, a corner): no
    // triangles to test.
    glm::vec3 centre;
    const float bound = bounds(spheres, centre);
    if(!nearDrawnBox(ent, centre, bound + std::fmax(allowed, 0.f)))
    {
        if(vr_debug_carry.value >= 3.f)
        {
            Con_Printf("grab: %s hand, %s: the fist is not near the box it is drawn in\n", hand == 0 ? "off" : "main",
                PR_GetString(ent->v.classname));
        }
        return false;
    }
    FistContact c;
    const bool found = fistContact(ent, spheres, std::fmax(allowed, 0.f) + 1.f, c);
    const bool touches = found && c.gap <= allowed;
    if(debug)
    {
        noteCarryProbe(hand, ent, found ? c.from : centre, found ? c.gap : -1.f, found ? c.at : centre, allowed, &spheres,
            touches ? c.sphere : -1);
    }
    if(vr_debug_carry.value >= 2.f)
    {
        Con_Printf("grab: %s hand, %s: the fist %s, %.2f cm from its surface (allowed %.2f cm)\n", hand == 0 ? "off" : "main",
            PR_GetString(ent->v.classname), touches ? "touches" : "doesn't touch", found ? c.gap / m2u * 100.f : 999.f,
            allowed / m2u * 100.f);
    }
    return touches;
}

namespace
{

// vr_debug_carry: per hand, the last probe (the server's), shown for a moment.
struct CarryProbe
{
    double time{-1.0};
    glm::vec3 corners[8]{};
    glm::vec3 at{0.f}, nearest{0.f};
    float distance{0.f}, reach{0.f};
    std::vector<glm::vec4> fist; // the fist tested (world), if it was
    int touching{-1};            // its sphere that touches
};
CarryProbe carryProbes[2];

} // namespace

void noteCarryProbe(int hand, edict_t* ent, const glm::vec3& at, float distance, const glm::vec3& nearest, float reach,
    const std::vector<glm::vec4>* fist, int touching)
{
    if((!vr_debug_carry.value && !vr_debug_physics_shapes.value) || hand < 0 || hand > 1)
    {
        return;
    }
    CarryProbe& p = carryProbes[hand];
    p.time = realtime;
    p.at = at;
    p.nearest = nearest;
    p.distance = distance;
    p.reach = reach;
    p.fist.clear();
    if(fist)
    {
        p.fist = *fist;
    }
    p.touching = touching;
    // The model's turned box (as the touch test's), its corners in the world.
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    glm::vec3 lo, hi;
    drawnBox(ent, lo, hi);
    const glm::mat3 axes = axesFromAngles(ent->v.angles, model && model->type == mod_brush);
    const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    for(int i = 0; i < 8; i++)
    {
        p.corners[i] = origin + axes * glm::vec3{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z};
    }
}

void drawCarryProbes()
{
    if(!vr_debug_carry.value && !vr_debug_physics_shapes.value)
    {
        return;
    }
    for(const CarryProbe& p : carryProbes)
    {
        if(p.time < 0.0 || realtime - p.time > 0.1)
        {
            continue;
        }
        // The fist tested: in reach if its nearest sphere is (its gap may be negative: sunk in); the old point test by
        // its distance.
        const bool within = !p.fist.empty() ? p.touching >= 0 : p.reach <= 0.f || (p.distance >= 0.f && p.distance <= p.reach);
        const glm::vec4 colour = within ? glm::vec4{0.2f, 1.f, 0.3f, 1.f} : glm::vec4{1.f, 0.25f, 0.2f, 1.f};
        constexpr int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for(const auto& e : edges)
        {
            lines::line(p.corners[e[0]], p.corners[e[1]], 0.2f, colour, colour);
        }
        if(!p.fist.empty())
        {
            // The fist's spheres (faint; the touching one bright), the nearest of them to the surface's nearest point.
            for(size_t i = 0; i < p.fist.size(); i++)
            {
                const bool touching = static_cast<int>(i) == p.touching;
                lines::point(glm::vec3{p.fist[i]}, 2.f * p.fist[i].w,
                    touching ? glm::vec4{0.2f, 1.f, 0.3f, 0.6f} : glm::vec4{0.8f, 0.8f, 0.8f, 0.25f});
            }
            lines::line(p.at, p.nearest, 0.2f, colour, colour);
            lines::point(p.nearest, 0.8f, colour);
            continue;
        }
        if(p.distance >= 0.f)
        {
            lines::line(p.at, p.nearest, 0.2f, colour, colour);
            lines::point(p.nearest, 0.8f, colour);
        }
        lines::point(p.at, 2.f * std::fmax(p.reach, 0.5f), glm::vec4{colour.r, colour.g, colour.b, 0.2f});
    }
}

glm::vec3 surfaceFit(edict_t* ent, const glm::vec3& hand, const glm::vec3& palm)
{
    // Never pushed further out than this (round 21, second pass: was half a metre; a thing is taken only where
    // the fist touches it now, so a big thing gripped deep inside its box no longer floats away), nor drawn
    // in further than this (the fingers short of it: a hand reaches a little way round what it grips).
    constexpr float mostPush = 0.15f; // metres
    constexpr float mostPull = 0.08f;

    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    if(!vr_held_surface_fit.value || !model || glm::length(palm) < 0.5f)
    {
        return glm::vec3{0.f};
    }

    // The hand's axes: the player's whose hand grips (its forward and up; the palm faces its side).
    const glm::vec3 p = glm::normalize(palm);
    glm::vec3 forward{0.f}, up{0.f};
    {
        using namespace progs;
        const FieldOffsets& f = fields();
        for(int i = 1; i <= svs.maxclients && glm::length(forward) == 0.f; i++)
        {
            edict_t* player = EDICT_NUM(i);
            for(const auto& [pos, rot] : {std::pair{f.handpos, f.handrot}, std::pair{f.offhandpos, f.offhandrot}})
            {
                if(pos >= 0 && rot >= 0 && fieldVec(player, pos) == hand)
                {
                    const glm::vec3 angles = fieldVec(player, rot);
                    const glm::mat3 axes = axesFromAngles(&angles[0], true);
                    forward = axes[0];
                    up = axes[2];
                    break;
                }
            }
        }
    }
    if(glm::length(forward) == 0.f || std::fabs(glm::dot(forward, p)) > 0.1f) // not found: any upright pair across the palm
    {
        up = std::fabs(p.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
        forward = glm::normalize(glm::cross(p, up));
        up = glm::cross(forward, p);
    }

    // What it looks like: its drawn surface, or else its box.
    std::vector<Triangle>& triangles = scratch.fitTris;
    if(!drawnTriangles(ent, model, triangles))
    {
        glm::vec3 lo{ent->v.mins[0], ent->v.mins[1], ent->v.mins[2]};
        glm::vec3 hi{ent->v.maxs[0], ent->v.maxs[1], ent->v.maxs[2]};
        if(model->type == mod_alias)
        {
            using namespace progs;
            const FieldOffsets& f = fields();
            modelBox(model, fieldVec(ent, f.model_scale), fieldVec(ent, f.model_scale_origin), fieldVec(ent, f.model_offset), lo, hi);
        }
        const auto corner = [&](int i) { return glm::vec3{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z}; };
        constexpr int faces[6][4] = {{0, 2, 6, 4}, {1, 5, 7, 3}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 1, 3, 2}, {4, 6, 7, 5}};
        for(const auto& q : faces)
        {
            triangles.push_back({{corner(q[0]), corner(q[1]), corner(q[2])}});
            triangles.push_back({{corner(q[0]), corner(q[2]), corner(q[3])}});
        }
    }

    // In the hand's frame (forward, towards the palm's side, up) from the grip.
    const bool brush = model->type == mod_brush;
    const glm::mat3 toHand = glm::transpose(glm::mat3{forward, p, up});
    const glm::mat3 axes = toHand * axesFromAngles(ent->v.angles, brush);
    const glm::vec3 from = toHand * (glm::vec3{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]} - hand);
    for(Triangle& t : triangles)
    {
        for(glm::vec3& v : t.p)
        {
            v = from + axes * v;
        }
    }

    // Over each cell of the fist, where the object's surface nearest the back of the hand is (along
    // the palm's normal through the cell's middle): it goes just clear of the fingers there, and so
    // the object is moved along the normal until the tightest cell touches (out of the fist, or in to
    // it). Moving it that way changes nothing of what is over which cell.
    const float m2u = units::metresToUnits();
    const float cm = 0.01f * units::perMetre * 1.25f * weapons::offsetScale(); // the hand's cm (as measured), in units
    const float gap = (vr_held_fit_gap.value + modelGap(model)) * 0.01f * m2u;
    float move = -std::numeric_limits<float>::max();
    for(int row = 0; row < fistRows; row++)
    {
        for(int column = 0; column < fistColumns; column++)
        {
            const float surface = fistSurface[row][column];
            if(surface == none)
            {
                continue;
            }
            const float x = (static_cast<float>(fistForward0 + column) + 0.5f) * cm;
            const float z = (static_cast<float>(fistUp0 - row) + 0.5f) * cm;
            float nearest = std::numeric_limits<float>::max();
            for(const Triangle& t : triangles)
            {
                // Where the line (x, z) crosses it: barycentric in the forward-up plane.
                const glm::vec2 a{t.p[0].x, t.p[0].z}, b{t.p[1].x, t.p[1].z}, c{t.p[2].x, t.p[2].z};
                const float det = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
                if(std::fabs(det) < 1e-6f)
                {
                    continue;
                }
                const float u = ((x - a.x) * (c.y - a.y) - (c.x - a.x) * (z - a.y)) / det;
                const float v = ((b.x - a.x) * (z - a.y) - (x - a.x) * (b.y - a.y)) / det;
                if(u < 0.f || v < 0.f || u + v > 1.f)
                {
                    continue;
                }
                nearest = std::min(nearest, t.p[0].y + u * (t.p[1].y - t.p[0].y) + v * (t.p[2].y - t.p[0].y));
            }
            if(nearest != std::numeric_limits<float>::max())
            {
                move = std::max(move, surface * cm + gap - nearest);
            }
        }
    }
    if(move == -std::numeric_limits<float>::max())
    {
        return glm::vec3{0.f}; // nothing of it over the fist: beside it
    }

    const float t = CLAMP(-mostPull * m2u, move, mostPush * m2u);
    if(vr_debug_throw.value)
    {
        Con_Printf("carryfit %s: moved %.1f cm along the palm's normal\n", model->name, t * 100.f / m2u);
    }
    return p * t;
}

} // namespace qvr::held

namespace
{

// For this long after its place in the hand is first taken, it is still taken from where the server has it (a
// caught object's jump to the hand may come a packet late); then it is kept. Timed from the first place taken, not
// from when the server said it: a loaded game's carry stats come in the signon, before the client's clock jumps
// to the saved time, and the window would be over at its first frame.
constexpr double placeTime = 0.1;

// Frames in a row the local player has been placed in the world (signed on, its entity linked). The hands are
// computed before the entities are relinked (the move is sent first), so on the player's first frame they are
// still round the world's origin: nothing is taken from them (a place in the hand, a two-handed hold) until the
// second. (A loaded game's box was held 20 metres off the hand, drawn out of sight, until let go.)
int playerFrames = 0;
constexpr int handsInWorld = 2;

// Let go, it eases back to where the server has it over this long.
constexpr double easeTime = 0.2;

struct Held
{
    int ent{0};
    const qmodel_t* model{nullptr};
    double since{0.0};
    bool placed{false};
    glm::vec3 pos{0.f};   // in the hand's frame
    glm::mat3 rot{1.f};
    bool drawn{false};    // drawn in the hand last frame, at:
    glm::vec3 lastPos{0.f};
    glm::mat3 lastRot{1.f};
    unsigned generation{0}; // props::settingsGeneration() its place is for
    // Held in both hands until now (leaveBoth): drawn eased from where both had it (fromPos, fromRot: world) onto its
    // place in this hand, as the hand eases off its grip onto the controller (handEaseTime).
    double fromSince{-1.0};
    bool fromStarted{false};
    glm::vec3 fromPos{0.f};
    glm::quat fromRot{1.f, 0.f, 0.f, 0.f};
    glm::vec3 fromOffset{0.f};
    glm::quat fromTurn{1.f, 0.f, 0.f, 0.f};
};

struct Easing
{
    int ent{0};
    const qmodel_t* model{nullptr};
    double since{0.0};
    bool started{false};
    glm::vec3 lastPos{0.f};
    glm::mat3 lastRot{1.f};
    glm::vec3 offset{0.f};
    glm::quat turn{1.f, 0.f, 0.f, 0.f};
};

Held holding[2];  // [0] off hand, [1] main hand (as hands::State)
Easing easing[2];

// Held in both hands (vr_carry2h.hpp): the hold kept when the second hand took it, and this frame's solve.
struct Both
{
    int ent{0};
    const qmodel_t* model{nullptr};
    carry2h::Hold hold;
    carry2h::Frame object;   // drawn this frame
    carry2h::Frame hands[2]; // the controllers this frame
    carry2h::Frame drawn[2]; // each hand drawn on its grip this frame
};
Both both;
// For the throw (bothHandsThrow): what each hand holds (the carry stats, drawn in the hand or not), and the object's
// centre from the middle of the hands (metres, world axes), the last frame held in both.
int carried[2]{0, 0};
glm::vec3 bothCentre{0.f};

// Let go of by one hand, or both: a hand drawn off its controller (on its grip) eases back onto it.
constexpr double handEaseTime = 0.15;
struct HandEase
{
    double since{-1.0};
    glm::vec3 offset{0.f};
    glm::quat turn{1.f, 0.f, 0.f, 0.f};
};
HandEase handEase[2];

// When a hand let go of what it held in both (tracking clock), for the other's two-handed throw.
struct BothRelease
{
    int ent{0};
    double at{-1.0};
};
BothRelease bothRelease[2];

// The most a drawn hand turns off its controller to stay on its grip (a hand's own twist the other doesn't share).
constexpr float mostHandTurn = glm::radians(40.f);

// Props held one in each hand meet (ROUND21.md, "Held props meet"): their drawn boxes are kept apart, each hand and what
// it holds drawn moved back along the way they meet by half how deep they would be, at most vr_held_collide_max (the
// looks only: the server has them in the hands, the throw is the hands'). Eased in and out, a buzz as they meet. So do a
// prop in one hand and the weapon in the other (the weapon's drawn box; its hand drawn moved back by the view:
// modelcollide::beginView, drawnPush), and a prop and the walls (wallFrame; ROUND21.md, "Held props against weapons,
// monsters and walls").
struct Meet
{
    glm::vec3 offset[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // each hand's, drawn now (units)
    double last{-1.0};                                  // cl.time of the last frame
    bool touching{false};
    bool weapon{false};                                 // a prop against the other hand's weapon (not two props)
    glm::vec3 wall[2]{glm::vec3{0.f}, glm::vec3{0.f}};  // each hand's prop held out of the walls, drawn now (units)
    bool wallTouching[2]{false, false};
    glm::vec3 viewPush[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // and out of the monsters (the view's: viewPush)
};
Meet meet;

// A held weapon's box: its model's first pose (as its body in Box3D: held::modelVertices), by model and mirrored (the
// off hand's), in its entity's axes; each hand's last.
struct WeaponBox
{
    const qmodel_t* model{nullptr};
    bool mirrored{false};
    std::string name;
    bool valid{false};
    glm::vec3 lo{0.f}, hi{0.f};
};
WeaponBox weaponBoxes[2];
double wallLast[2]{0.0, 0.0}; // cl.time of each hand's last wallFrame

// The hands this frame's held things were drawn from (vr_carry_check: the hands move on before the next frame's check).
hands::State drawnFrom;
constexpr double meetEaseTime = 0.04; // s: the offsets follow the push this fast (and back when apart)

struct Box
{
    glm::vec3 centre;
    glm::mat3 axes; // columns: its unit axes
    glm::vec3 half;
};

// How deep two boxes overlap along the axis they are least deep on (separating axes: each's 3 faces and the 9 edge
// pairs), with that axis from `a` to `b` in `normal`; 0 apart.
[[nodiscard]] float overlap(const Box& a, const Box& b, glm::vec3& normal)
{
    const glm::vec3 d = b.centre - a.centre;
    float least = std::numeric_limits<float>::max();
    const auto test = [&](glm::vec3 axis) {
        const float len = glm::length(axis);
        if(len < 1e-4f)
        {
            return true; // (parallel edges: the faces' axes decide)
        }
        axis /= len;
        float ra = 0.f, rb = 0.f;
        for(int i = 0; i < 3; i++)
        {
            ra += a.half[i] * std::fabs(glm::dot(a.axes[i], axis));
            rb += b.half[i] * std::fabs(glm::dot(b.axes[i], axis));
        }
        const float along = glm::dot(d, axis);
        const float depth = ra + rb - std::fabs(along);
        if(depth <= 0.f)
        {
            return false;
        }
        if(depth < least)
        {
            least = depth;
            normal = along < 0.f ? -axis : axis;
        }
        return true;
    };
    for(int i = 0; i < 3; i++)
    {
        if(!test(a.axes[i]) || !test(b.axes[i]))
        {
            return 0.f;
        }
    }
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            if(!test(glm::cross(a.axes[i], b.axes[j])))
            {
                return 0.f;
            }
        }
    }
    return least == std::numeric_limits<float>::max() ? 0.f : least;
}

[[nodiscard]] bool valid(int ent, const qmodel_t* model)
{
    return ent > 0 && ent < cl.num_entities && cl_entities[ent].model &&
           (!model || cl_entities[ent].model == model) && cl_entities[ent].model->type != mod_sprite;
}

// A listen server's edict `num` (nullptr: a remote server, or none there).
[[nodiscard]] edict_t* serverEdict(int num)
{
    if(!sv.active || num <= 0 || num >= sv.qcvm.num_edicts)
    {
        return nullptr;
    }
    edict_t* ed = reinterpret_cast<edict_t*>(reinterpret_cast<byte*>(sv.qcvm.edicts) + num * sv.qcvm.edict_size);
    return ed->free ? nullptr : ed;
}

// A listen server: where the server has entity `ent` (drawn with `model`) in the local player's hand `h`: its place
// and turn in the server's hand (the move's, moved with the body: vr_server.cpp rebaseHands), both as the server's last
// frame left them. Exact, whatever the hand and the body did since. Taken from where the server has it and this
// frame's controller instead, a hand or a body on the move (the server runs at 72 Hz, the headset faster; the
// player's origin is interpolated) left it that far off the hand for as long as it was held (a force grab caught on
// the move; ROUND21.md, "Held props: no gap after two hands"). False: a remote server (or not there).
[[nodiscard]] bool serverInHand(int ent, int h, const qmodel_t* model, glm::vec3& pos, glm::mat3& rot)
{
    using namespace progs;
    const FieldOffsets& f = fields();
    const int posField = h == 0 ? f.offhandpos : f.handpos;
    const int rotField = h == 0 ? f.offhandrot : f.handrot;
    edict_t* ed = serverEdict(ent);
    edict_t* player = serverEdict(cl.viewentity);
    if(!ed || !player || posField < 0 || rotField < 0)
    {
        return false;
    }
    const int index = static_cast<int>(ed->v.modelindex);
    if(index <= 0 || index >= MAX_MODELS || sv.models[index] != model)
    {
        return false;
    }
    const glm::vec3 angles = fieldVec(player, rotField);
    const glm::mat3 hand = held::axesFromAngles(&angles[0], true);
    if(f.carry_offset >= 0)
    {
        // Its place in the hand as the server keeps it (QC's .carry_offset: forward, right, up), where it goes from the
        // next frame on even if a wall held it short this one (VR_Carry_Follow), or a catch put it elsewhere.
        const glm::vec3 o = fieldVec(ed, f.carry_offset);
        pos = {o.x, -o.y, o.z};
    }
    else
    {
        pos = glm::transpose(hand) * (glm::vec3{ed->v.origin[0], ed->v.origin[1], ed->v.origin[2]} - fieldVec(player, posField));
    }
    rot = glm::transpose(hand) * held::axesFromAngles(ed->v.angles, model->type == mod_brush);
    return true;
}

void reset()
{
    for(int h = 0; h < 2; h++)
    {
        holding[h] = Held{};
        easing[h] = Easing{};
        handEase[h] = HandEase{};
        bothRelease[h] = BothRelease{};
    }
    both = Both{};
    meet = Meet{};
}

// The weapon drawn in hand `h` as a box in the world: where it is in the hand as drawn last frame (view::drawnWeapon),
// placed from this frame's hand `s`. False: none (the fist, or not drawn lately).
[[nodiscard]] bool weaponBox(const hands::State& s, int h, Box& out)
{
    const view::DrawnWeapon& d = view::drawnWeapon(h);
    if(!d.model || d.when < 0.0 || realtime - d.when > 0.5 || !s.valid)
    {
        return false;
    }
    WeaponBox& wb = weaponBoxes[h];
    if(wb.model != d.model || wb.mirrored != d.mirrored || wb.name != d.model->name)
    {
        wb.model = d.model;
        wb.mirrored = d.mirrored;
        wb.name = d.model->name;
        std::vector<glm::vec3> vertices;
        wb.valid = held::modelVertices(d.model, d.mirrored, vertices) && !vertices.empty();
        wb.lo = glm::vec3{1e9f};
        wb.hi = glm::vec3{-1e9f};
        for(const glm::vec3& v : vertices)
        {
            wb.lo = glm::min(wb.lo, v);
            wb.hi = glm::max(wb.hi, v);
        }
    }
    if(!wb.valid)
    {
        return false;
    }
    glm::mat4 hand{held::axesFromAngles(&s.rot[h][0], true)};
    hand[3] = glm::vec4{s.pos[h], 1.f};
    const glm::mat4 m = hand * d.inHand;
    const glm::vec3 half = (wb.hi - wb.lo) * 0.5f;
    out.centre = glm::vec3{m * glm::vec4{(wb.lo + wb.hi) * 0.5f, 1.f}};
    for(int i = 0; i < 3; i++)
    {
        const glm::vec3 axis{m[i]};
        const float len = glm::length(axis);
        out.axes[i] = len > 1e-6f ? axis / len : glm::vec3{0.f};
        out.half[i] = half[i] * len;
    }
    return true;
}

// The drawn box of the prop `hd` holds, where the hand has it now.
[[nodiscard]] bool propBox(const Held& hd, Box& out)
{
    glm::vec3 lo, hi;
    if(!held::drawnBox(hd.ent, lo, hi))
    {
        return false;
    }
    out = Box{hd.lastPos + hd.lastRot * ((lo + hi) * 0.5f), hd.lastRot, (hi - lo) * 0.5f};
    return true;
}

[[nodiscard]] carry2h::Frame controller(const hands::State& s, int h)
{
    return {s.pos[h], carry2h::fromAngles(&s.rot[h][0], true)};
}

void place(entity_t& e, const glm::vec3& pos, const glm::mat3& rot)
{
    e.origin[0] = pos.x;
    e.origin[1] = pos.y;
    e.origin[2] = pos.z;
    held::anglesFromAxes(rot, e.angles, e.model->type == mod_brush);
}

void holdFrame(int h, const hands::State& s, int bothEnt)
{
    Held& hd = holding[h];
    const int want = cl.stats[h == 1 ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
    if(want != hd.ent)
    {
        // (Let go of by both hands at once: eased once.)
        if(hd.ent && hd.drawn && easing[0].ent != hd.ent && easing[1].ent != hd.ent)
        {
            easing[h] = Easing{hd.ent, hd.model, cl.time, false, hd.lastPos, hd.lastRot};
        }
        hd = Held{};
        hd.ent = want;
    }

    hd.drawn = false;
    if(!hd.ent || !s.valid || playerFrames < handsInWorld || !valid(hd.ent, hd.placed ? hd.model : nullptr))
    {
        return;
    }

    if(hd.ent == bothEnt)
    {
        return; // held in both hands: bothFrame places it
    }

    entity_t& e = cl_entities[hd.ent];
    const bool brush = e.model->type == mod_brush;
    const glm::mat3 hand = held::axesFromAngles(&s.rot[h][0], true);
    if(!hd.placed)
    {
        hd.since = cl.time;
    }
    if(!hd.placed || cl.time - hd.since < placeTime)
    {
        if(!serverInHand(hd.ent, h, e.model, hd.pos, hd.rot))
        {
            // A remote server: from where it has it, and this frame's controller.
            const glm::vec3 o{e.msg_origins[0][0], e.msg_origins[0][1], e.msg_origins[0][2]};
            hd.pos = glm::transpose(hand) * (o - s.pos[h]);
            hd.rot = glm::transpose(hand) * held::axesFromAngles(e.msg_angles[0], brush);
        }
        hd.model = e.model;
        hd.placed = true;
        hd.generation = props::settingsGeneration();
    }
    else if(hd.generation != props::settingsGeneration())
    {
        // Its Held Object Offsets changed while it is held (a slider, its Grip mode): placed again at once, where the
        // server places it (a listen server: vr_grip.hpp); a remote server's place is taken again (for placeTime).
        hd.generation = props::settingsGeneration();
        grip::Place p;
        if(grip::serverPlace(hd.ent, p))
        {
            hd.pos = p.pos;
            hd.rot = p.rot;
            if(developer.value || vr_debug_carry.value)
            {
                Con_Printf("held: %d placed again in the %s hand at %.2f %.2f %.2f (generation %u, frame %d)\n", hd.ent,
                    h == 1 ? "main" : "off", hd.pos.x, hd.pos.y, hd.pos.z, hd.generation, host_framecount);
            }
        }
        else
        {
            hd.since = cl.time;
        }
    }

    // Drawn in the drawn hand: tired arms shake it (looks only: vr_fatigue.cpp), as the hand drawn round it.
    glm::vec3 shakePos, shakeAngles;
    fatigue::shake(h, shakePos, shakeAngles);
    const glm::vec3 shakenRot = s.rot[h] + shakeAngles;
    const glm::mat3 drawnAxes = held::axesFromAngles(&shakenRot[0], true);
    hd.lastPos = s.pos[h] + shakePos + drawnAxes * hd.pos;
    hd.lastRot = drawnAxes * hd.rot;
    if(hd.fromSince >= 0.0)
    {
        // Just let go of by the other hand: from where both had it onto its place in this one (leaveBoth).
        const double t = cl.time - hd.fromSince;
        if(!hd.fromStarted)
        {
            hd.fromStarted = true;
            hd.fromOffset = hd.fromPos - hd.lastPos;
            hd.fromTurn = glm::normalize(hd.fromRot * glm::inverse(glm::quat_cast(hd.lastRot)));
        }
        if(t < 0.0 || t >= handEaseTime || glm::length(hd.fromOffset) > 64.f)
        {
            hd.fromSince = -1.0;
        }
        else
        {
            float w = 1.f - static_cast<float>(t / handEaseTime);
            w = w * w * (3.f - 2.f * w); // as the hand eases (drawnHand)
            hd.lastPos += hd.fromOffset * w;
            hd.lastRot = glm::mat3_cast(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, hd.fromTurn, w)) * hd.lastRot;
        }
    }
    place(e, hd.lastPos, hd.lastRot);
    hd.drawn = true;
}

// After both hands' holdFrame: props held one in each hand kept apart, or a prop and the other hand's weapon (Meet).
void meetFrame(const hands::State& s)
{
    const double dt = meet.last < 0.0 ? 0.0 : std::clamp(cl.time - meet.last, 0.0, 0.1);
    meet.last = cl.time;
    glm::vec3 want[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float depth = 0.f;
    Held& a = holding[0];
    Held& b = holding[1];
    // A prop held in one hand (not in both), against the other hand's prop or its weapon.
    const bool propA = a.drawn && a.ent != both.ent;
    const bool propB = b.drawn && b.ent != both.ent;
    Box boxA{}, boxB{};
    bool haveA = false, haveB = false;
    meet.weapon = !propA || !propB;
    if(vr_held_collide.value && (propA || propB) && a.ent != b.ent)
    {
        haveA = propA ? propBox(a, boxA) : !a.ent && weaponBox(s, 0, boxA);
        haveB = propB ? propBox(b, boxB) : !b.ent && weaponBox(s, 1, boxB);
    }
    if(haveA && haveB)
    {
        // Where the hands have them: each drawn moved back along the way they are least deep by half of it, at most
        // vr_held_collide_max (deeper, they overlap by the rest).
        glm::vec3 n{0.f};
        depth = overlap(boxA, boxB, n);
        if(depth > 0.f)
        {
            const float most = std::max(vr_held_collide_max.value, 0.f) * units::metresToUnits() / 100.f;
            const float each = std::min(depth * 0.5f, most);
            want[0] = -n * each;
            want[1] = n * each;
        }
    }

    // A buzz in both hands as they meet.
    const bool touching = depth > 0.f;
    if(touching && !meet.touching && !vr_disablehaptics.value)
    {
        if(Backend* be = backend())
        {
            be->haptic(0, 0.04f, 120.f, 0.35f);
            be->haptic(1, 0.04f, 120.f, 0.35f);
        }
    }
    if(touching != meet.touching && vr_debug_carry.value)
    {
        Con_Printf("held: %d and %d %s%s (boxes' half sizes %.1f %.1f %.1f and %.1f %.1f %.1f units; the hands %.1f units apart)\n",
            a.ent, b.ent, touching ? "meet" : "apart", meet.weapon ? " (a weapon)" : "", boxA.half.x, boxA.half.y,
            boxA.half.z, boxB.half.x, boxB.half.y, boxB.half.z, glm::distance(s.pos[0], s.pos[1]));
    }
    meet.touching = touching;

    const float w = dt > 0.0 ? static_cast<float>(std::min(1.0, dt / meetEaseTime)) : 1.f;
    for(int h = 0; h < 2; h++)
    {
        Held& hd = holding[h];
        meet.offset[h] += (want[h] - meet.offset[h]) * w;
        if(glm::length(meet.offset[h]) < 0.01f)
        {
            meet.offset[h] = glm::vec3{0.f};
            continue;
        }
        if(!hd.drawn)
        {
            continue; // a weapon: its hand drawn moved by the view (drawnPush)
        }
        hd.lastPos += meet.offset[h];
        place(cl_entities[hd.ent], hd.lastPos, hd.lastRot);
    }
    if(vr_debug_carry.value && touching)
    {
        Con_Printf("held: %d and %d%s meet %.1f cm deep (%.1f cm apart); drawn moved apart by %.1f and %.1f cm\n", a.ent,
            b.ent, meet.weapon ? " (a weapon)" : "", depth / units::metresToUnits() * 100.f,
            glm::distance(boxA.centre, boxB.centre) / units::metresToUnits() * 100.f, glm::length(meet.offset[0]) / units::metresToUnits() * 100.f,
            glm::length(meet.offset[1]) / units::metresToUnits() * 100.f);
    }
}

// A prop held in one hand against the walls (and the level's brush entities: doors, lifts), after meetFrame: drawn held
// out of them, the hand with it, as far as vr_held_collide_wall_max (deeper, it goes in by the rest; the server drops it
// when the hand is 32 units past where it stops: VR_Carry_Follow). The test: lines from the hand (out of the walls:
// handpose) to its drawn box's corners and faces' middles; each that meets a wall, a plane there the point must be moved
// back out of; the push the least move out of them all (Gauss-Seidel), tested again from there (three rounds at most).
// Looks only, as meetFrame. Out at once; back in over meetEaseTime; a buzz in the hand as it touches.
void wallFrame(const hands::State& s)
{
    const float m2u = units::metresToUnits();
    const float most = std::max(vr_held_collide_wall_max.value, 0.f) * m2u / 100.f;
    constexpr float margin = 0.25f; // units off the wall
    for(int h = 0; h < 2; h++)
    {
        Held& hd = holding[h];
        glm::vec3 want{0.f};
        float deepest = 0.f;
        Box box;
        if(vr_held_collide_walls.value && most > 0.f && hd.drawn && hd.ent != both.ent && s.valid && propBox(hd, box))
        {
            glm::vec3 points[14];
            int n = 0;
            for(int i = 0; i < 8; i++)
            {
                const glm::vec3 c{(i & 1) ? 1.f : -1.f, (i & 2) ? 1.f : -1.f, (i & 4) ? 1.f : -1.f};
                points[n++] = box.centre + box.axes * (c * box.half);
            }
            for(int i = 0; i < 3; i++)
            {
                points[n++] = box.centre + box.axes[i] * box.half[i];
                points[n++] = box.centre - box.axes[i] * box.half[i];
            }
            const glm::vec3 grip = s.pos[h] + meet.offset[h];
            struct Plane
            {
                glm::vec3 n;
                float c;
            };
            Plane planes[3 * 14];
            int count = 0;
            glm::vec3 p{0.f};
            for(int round = 0; round < 3; round++)
            {
                bool found = false;
                for(const glm::vec3& point : points)
                {
                    const glm::vec3 end = point + p;
                    const trace_t tr = worldtrace::world(grip + p, end, true);
                    if(tr.startsolid || tr.allsolid || tr.fraction >= 1.f)
                    {
                        continue;
                    }
                    const glm::vec3 wallN = worldtrace::normal(tr);
                    const float depth = glm::dot(worldtrace::endPos(tr) - end, wallN); // behind the wall
                    if(depth + margin <= 0.f || count >= 3 * 14)
                    {
                        continue;
                    }
                    deepest = std::max(deepest, depth);
                    planes[count++] = Plane{wallN, depth + margin + glm::dot(p, wallN)};
                    found = true;
                }
                if(!found)
                {
                    break;
                }
                for(int sweep = 0; sweep < 16; sweep++)
                {
                    float worst = 0.f;
                    for(int i = 0; i < count; i++)
                    {
                        const float short_ = planes[i].c - glm::dot(p, planes[i].n);
                        if(short_ > 0.f)
                        {
                            p += planes[i].n * short_;
                            worst = std::max(worst, short_);
                        }
                    }
                    if(worst < 0.01f)
                    {
                        break;
                    }
                }
                if(glm::length(p) > 2.f * most)
                {
                    break;
                }
            }
            want = glm::length(p) > most ? p * (most / glm::length(p)) : p;
        }

        // Out at once, back over meetEaseTime.
        glm::vec3& wall = meet.wall[h];
        if(glm::dot(want - wall, want) > 0.f)
        {
            wall = want;
        }
        else
        {
            const float w = static_cast<float>(std::min(1.0, std::max(cl.time - wallLast[h], 0.0) / meetEaseTime));
            wall += (want - wall) * w;
        }
        wallLast[h] = cl.time;
        if(glm::length(wall) < 0.01f || !hd.drawn)
        {
            wall = glm::vec3{0.f};
        }

        const bool touching = want != glm::vec3{0.f};
        if(touching && !meet.wallTouching[h] && !vr_disablehaptics.value)
        {
            if(Backend* be = backend())
            {
                be->haptic(h, 0.03f, 90.f, 0.3f);
            }
        }
        if(vr_debug_carry.value && (touching || meet.wallTouching[h]))
        {
            Con_Printf("held: %d in the %s hand %s a wall: %.1f cm deep; drawn moved back %.1f cm (%.2f %.2f %.2f)\n", hd.ent,
                h == 1 ? "main" : "off", touching ? "against" : "off", deepest / m2u * 100.f, glm::length(wall) / m2u * 100.f,
                wall.x, wall.y, wall.z);
        }
        meet.wallTouching[h] = touching;
        if(wall != glm::vec3{0.f})
        {
            hd.lastPos += wall;
            place(cl_entities[hd.ent], hd.lastPos, hd.lastRot);
        }
    }
}

// Held in both hands no more (one let go, or both): each hand drawn on its grip eases back onto its controller. The
// hand still holding it holds it where the server now has it, moved onto that hand's grip (carry2h::keep), not where
// both had it: the hand held it as far off as it was off its grip (up to the drift; the drift and the detach pulled
// off), and what hands and shots meet was there, not where it was drawn. Its place is taken again from the server
// (placeTime) and it is drawn eased onto it from where both had it, with the hand.
void leaveBoth()
{
    for(int h = 0; h < 2; h++)
    {
        handEase[h] = {cl.time, both.drawn[h].pos - both.hands[h].pos,
            glm::normalize(both.drawn[h].rot * glm::inverse(both.hands[h].rot))};
        Held& hd = holding[h];
        if(hd.ent == both.ent && valid(hd.ent, both.model))
        {
            // (Until the server's place is taken: on its grip in this hand, as carry2h::keep puts it.)
            const glm::mat3 hand = glm::mat3_cast(both.hands[h].rot);
            hd.pos = glm::transpose(hand) * -(both.object.rot * both.hold.grip[h]);
            hd.rot = glm::transpose(hand) * glm::mat3_cast(both.object.rot);
            hd.model = both.model;
            hd.placed = true;
            hd.since = cl.time;
            hd.fromSince = cl.time;
            hd.fromStarted = false;
            hd.fromPos = both.object.pos;
            hd.fromRot = both.object.rot;
        }
    }
    if(vr_debug_carry.value)
    {
        Con_Printf("carry2h: %d held in both hands no more\n", both.ent);
    }
    both = Both{};
}

// Held in both hands (`ent`, both carry stats): placed by both hands' controllers this frame (vr_carry2h.hpp), and
// each hand drawn on its grip.
void bothFrame(const hands::State& s, int ent)
{
    if(!ent || !s.valid || playerFrames < handsInWorld || !valid(ent, both.ent == ent ? both.model : nullptr))
    {
        if(ent && !valid(ent, nullptr))
        {
            both = Both{};
        }
        return;
    }

    entity_t& e = cl_entities[ent];
    const bool brush = e.model->type == mod_brush;
    const carry2h::Frame hands[2] = {controller(s, 0), controller(s, 1)};
    if(both.ent != ent)
    {
        // The second hand took it: from where it is drawn now, in the hand that held it (its place this frame), or
        // where the server has it.
        carry2h::Frame object{glm::vec3{e.msg_origins[0][0], e.msg_origins[0][1], e.msg_origins[0][2]},
            carry2h::fromAngles(e.msg_angles[0], brush)};
        for(int h = 0; h < 2; h++)
        {
            const Held& hd = holding[h];
            if(hd.ent == ent && hd.placed && hd.model == e.model)
            {
                const glm::mat3 hand = glm::mat3_cast(hands[h].rot);
                object = {hands[h].pos + hand * hd.pos, glm::normalize(glm::quat_cast(hand * hd.rot))};
                break;
            }
        }
        both = Both{};
        both.ent = ent;
        both.model = e.model;
        // A listen server: the server's own hold (its grips and turns in the hands as it took them when the second
        // hand gripped), so that both place it alike. Recorded here, a frame or more later from hands that moved since,
        // the grips differed from the server's by as much. A remote server: from where it is drawn now.
        if(!serverEdict(ent) || !carry2h::serverHold(ent, both.hold))
        {
            both.hold = carry2h::record(object, hands);
        }
        handEase[0] = handEase[1] = HandEase{};
        if(vr_debug_carry.value)
        {
            Con_Printf("carry2h: %d in both hands, grips %.1f units apart\n", ent, both.hold.span);
        }
    }

    both.hands[0] = hands[0];
    both.hands[1] = hands[1];
    both.object = carry2h::solve(both.hold, hands);
    const glm::mat3 rot = glm::mat3_cast(both.object.rot);
    place(e, both.object.pos, rot);
    for(Held& hd : holding)
    {
        hd.drawn = true;
        hd.model = e.model;
        hd.lastPos = both.object.pos;
        hd.lastRot = rot;
    }

    // Each hand on its grip, never further than vr_carry_two_hands_drift from its controller (pulled further apart,
    // it leaves the grip), nor turned more than mostHandTurn off it.
    const float m2u = units::metresToUnits();
    const float drift = std::fmax(vr_carry_two_hands_drift.value, 0.f) * 0.01f * m2u;
    for(int h = 0; h < 2; h++)
    {
        const carry2h::Frame grip = carry2h::onGrip(both.hold, both.object, h);
        glm::vec3 off = grip.pos - hands[h].pos;
        const float length = glm::length(off);
        if(length > drift)
        {
            off *= length > 0.f ? drift / length : 0.f;
        }
        glm::quat turn = grip.rot * glm::inverse(hands[h].rot);
        if(turn.w < 0.f)
        {
            turn = -turn;
        }
        const float angle = glm::angle(turn);
        const float share = drift <= 0.f ? 0.f : angle > mostHandTurn ? mostHandTurn / angle : 1.f;
        both.drawn[h] = {hands[h].pos + off,
            glm::normalize(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, turn, share) * hands[h].rot)};
    }
}

void easeFrame(Easing& ea)
{
    const double t = cl.time - ea.since;
    if(!ea.ent || holding[0].ent == ea.ent || holding[1].ent == ea.ent || t >= easeTime || t < 0.0 || !valid(ea.ent, ea.model))
    {
        ea = Easing{};
        return;
    }

    entity_t& e = cl_entities[ea.ent];
    const bool brush = e.model->type == mod_brush;
    const glm::vec3 serverPos{e.origin[0], e.origin[1], e.origin[2]};
    const glm::mat3 serverRot = held::axesFromAngles(e.angles, brush);
    if(!ea.started)
    {
        ea.started = true;
        ea.offset = ea.lastPos - serverPos;
        ea.turn = glm::quat_cast(ea.lastRot * glm::transpose(serverRot));
        if(glm::length(ea.offset) > 64.f) // gone somewhere else (a teleport): no easing
        {
            ea = Easing{};
            return;
        }
    }

    float w = 1.f - static_cast<float>(t / easeTime);
    w *= w;
    place(e, serverPos + ea.offset * w,
        glm::mat3_cast(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, ea.turn, w)) * serverRot);
}

// vr_debug_carry 2: one line a frame into carry_trace.txt (the game directory): the time; what is held (2 both hands,
// 1 the main hand, 0 the off hand, -1 nothing or easing back) and its entity; its drawn place and turn (x y z, then
// the quaternion w x y z of its axes); each hand's controller and drawn place and turn (main, then off).
void trace(const hands::State& s)
{
    static FILE* file = nullptr;
    if(vr_debug_carry.value < 2.f)
    {
        if(file)
        {
            fclose(file);
            file = nullptr;
        }
        return;
    }
    if(!s.valid)
    {
        return;
    }
    const int mode = both.ent ? 2 : holding[1].drawn ? 1 : holding[0].drawn ? 0 : -1;
    int ent = both.ent ? both.ent : holding[1].drawn ? holding[1].ent : holding[0].drawn ? holding[0].ent : 0;
    for(const Easing& ea : easing)
    {
        ent = ent ? ent : ea.ent;
    }
    if(!ent || !valid(ent, nullptr))
    {
        return;
    }
    if(!file)
    {
        file = fopen(va("%s/carry_trace.txt", com_gamedir), "w");
        if(!file)
        {
            return;
        }
    }
    const entity_t& e = cl_entities[ent];
    const auto pose = [](const glm::vec3& p, const glm::quat& q) {
        return std::string{va(" %.4f %.4f %.4f %.7f %.7f %.7f %.7f", p.x, p.y, p.z, q.w, q.x, q.y, q.z)};
    };
    std::string line = va("%.4f %d %d", cl.time, mode, ent);
    line += pose(glm::vec3{e.origin[0], e.origin[1], e.origin[2]}, carry2h::fromAngles(e.angles, e.model->type == mod_brush));
    for(int h = 1; h >= 0; h--)
    {
        glm::vec3 pos = s.pos[h], angles = s.rot[h];
        line += pose(pos, carry2h::fromAngles(&angles[0], true));
        held::drawnHand(h, pos, angles);
        line += pose(pos, carry2h::fromAngles(&angles[0], true));
    }
    line += "\n";
    fputs(line.c_str(), file);
    fflush(file);
}

} // namespace

namespace qvr::held
{

int placeInHand(int hand, glm::vec3& origin, glm::mat3& axes, bool& bothHands, glm::vec3& otherHand)
{
    if(hand < 0 || hand > 1)
    {
        return 0;
    }
    const Held& hd = holding[hand];
    if(!hd.ent || !valid(hd.ent, nullptr))
    {
        return 0;
    }
    bothHands = both.ent && hd.ent == both.ent && both.model;
    if(bothHands)
    {
        const glm::mat3 r = glm::mat3_cast(both.hands[hand].rot);
        origin = glm::transpose(r) * (both.object.pos - both.hands[hand].pos);
        axes = glm::transpose(r) * glm::mat3_cast(both.object.rot);
        otherHand = glm::transpose(r) * (both.hands[1 - hand].pos - both.hands[hand].pos);
        return hd.ent;
    }
    if(!hd.placed || !hd.drawn)
    {
        return 0;
    }
    origin = hd.pos;
    axes = hd.rot;
    return hd.ent;
}

int heldEntity(int hand)
{
    const Held& hd = holding[hand];
    return hd.drawn && valid(hd.ent, hd.model) ? hd.ent : 0;
}

bool handEmpty(int hand)
{
    using namespace protocol;
    const bool main = hand == 1;
    return cl.stats[main ? STAT_QVR_WEAPON : STAT_QVR_WEAPON2] == 0 && // QC's WID_FIST
           cl.stats[main ? STAT_QVR_CARRYMAIN : STAT_QVR_CARRYOFF] == 0;
}

bool drawnHand(int hand, glm::vec3& pos, glm::vec3& angles)
{
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    if(both.ent && holding[hand].ent == both.ent && holding[hand].drawn)
    {
        pos = both.drawn[hand].pos;
        carry2h::toAngles(both.drawn[hand].rot, &angles[0], true);
        return true;
    }
    // Moved back with what it holds, pressed against what the other holds (meetFrame) or a wall (wallFrame).
    if(holding[hand].drawn && (meet.offset[hand] != glm::vec3{0.f} || meet.wall[hand] != glm::vec3{0.f}))
    {
        pos += meet.offset[hand] + meet.wall[hand];
        return true;
    }
    const HandEase& he = handEase[hand];
    const double t = cl.time - he.since;
    if(he.since < 0.0 || t < 0.0 || t >= handEaseTime)
    {
        return false;
    }
    float w = 1.f - static_cast<float>(t / handEaseTime);
    w = w * w * (3.f - 2.f * w);
    pos += he.offset * w;
    carry2h::toAngles(glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, he.turn, w) * carry2h::fromAngles(&angles[0], true), &angles[0], true);
    return true;
}

int heldAlone(int hand, glm::vec3* drawnOffset)
{
    if(hand < 0 || hand > 1)
    {
        return 0;
    }
    const Held& hd = holding[hand];
    if(!hd.drawn || hd.ent == both.ent || !valid(hd.ent, hd.model))
    {
        return 0;
    }
    if(drawnOffset)
    {
        *drawnOffset = meet.offset[hand] + meet.wall[hand];
    }
    return hd.ent;
}

glm::vec3 drawnPush(int hand)
{
    return (hand == 0 || hand == 1) && !holding[hand].drawn ? meet.offset[hand] : glm::vec3{0.f};
}

void viewPush(int hand, const glm::vec3& push)
{
    if(hand < 0 || hand > 1)
    {
        return;
    }
    Held& hd = holding[hand];
    if(!hd.drawn || hd.ent == both.ent || !valid(hd.ent, hd.model))
    {
        meet.viewPush[hand] = glm::vec3{0.f};
        return;
    }
    meet.viewPush[hand] = push;
    place(cl_entities[hd.ent], hd.lastPos + push, hd.lastRot); // (again in a frame drawn twice: the same)
}

bool bothHandsThrow(int hand, double at, bool release, throwing::Estimate& out)
{
    if(hand < 0 || hand > 1)
    {
        return false;
    }
    const int ent = carried[hand];
    const bool inBoth = ent && carried[1 - hand] == ent;
    const BothRelease& other = bothRelease[1 - hand];
    const double window = std::fmax(vr_carry_two_hands_window.value, 0.f);
    const bool afterOther = release && ent && other.ent == ent && at - other.at >= -0.02 && at - other.at <= window;
    if(!inBoth && !afterOther)
    {
        return false;
    }
    out = throwing::estimateBothAt(at, bothCentre);
    if(release)
    {
        bothRelease[hand] = {ent, at};
        if(vr_debug_throw.value)
        {
            Con_Printf("throw both hands (%s let go%s): %.2f m/s (%.2f %.2f %.2f), spin %.1f rad/s (%.2f %.2f %.2f)\n",
                hand == 1 ? "main" : "off", afterOther ? va(", %.0f ms after the other", (at - other.at) * 1000.0) : "",
                glm::length(out.vel), out.vel.x, out.vel.y, out.vel.z, glm::length(out.angVel), out.angVel.x, out.angVel.y,
                out.angVel.z);
        }
    }
    return true;
}

void resetClientState()
{
    reset();
    carried[0] = carried[1] = 0;
    bothCentre = glm::vec3{0.f};
    playerFrames = 0;
}

void carryCheck();

void carryCheck_f()
{
    if(!sv.active || cls.state != ca_connected)
    {
        Con_Printf("vr_carry_check: needs a local game\n");
        return;
    }
    qcvm_t* const old = qcvm;
    if(old != &sv.qcvm)
    {
        if(old)
        {
            PR_SwitchQCVM(nullptr);
        }
        PR_SwitchQCVM(&sv.qcvm);
    }
    carryCheck();
    if(old != &sv.qcvm)
    {
        PR_SwitchQCVM(nullptr);
        if(old)
        {
            PR_SwitchQCVM(old);
        }
    }
}

void carryCheck()
{
    using namespace progs;
    const FieldOffsets& f = fields();
    const hands::State& s = drawnFrom;
    const float m2u = units::metresToUnits();
    const auto turnBetween = [](const glm::mat3& a, const glm::mat3& b) {
        const float c = (glm::dot(a[0], b[0]) + glm::dot(a[1], b[1]) + glm::dot(a[2], b[2]) - 1.f) * 0.5f;
        return glm::degrees(std::acos(glm::clamp(c, -1.f, 1.f)));
    };
    float worstApart = 0.f, worstTurn = 0.f, worstGap = -999.f, worstWorld = 0.f;
    int count = 0;
    edict_t* player = EDICT_NUM(1);
    for(int h = 1; h >= 0; h--)
    {
        const int ent = cl.stats[h == 1 ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
        const int posField = h == 0 ? f.offhandpos : f.handpos;
        const int rotField = h == 0 ? f.offhandrot : f.handrot;
        if(!ent || ent >= sv.qcvm.num_edicts || !valid(ent, nullptr) || !s.valid || posField < 0 || rotField < 0)
        {
            continue;
        }
        edict_t* ed = EDICT_NUM(ent);
        const entity_t& e = cl_entities[ent];
        const bool brush = e.model->type == mod_brush;
        const glm::vec3 drawnPos{e.origin[0], e.origin[1], e.origin[2]};
        const glm::mat3 drawnRot = axesFromAngles(e.angles, brush);
        const glm::vec3 physPos{ed->v.origin[0], ed->v.origin[1], ed->v.origin[2]};
        const glm::mat3 physRot = axesFromAngles(ed->v.angles, brush);
        const float world = glm::distance(drawnPos, physPos) / m2u * 100.f;

        // Its place in the hand: drawn, from the controller; physical, from the server's hand (the move's). They differ
        // by as much as the two hands do (the server's hand is moved with the body: vr_server.cpp rebaseHands).
        const glm::vec3 svHandPos = fieldVec(player, posField);
        const glm::vec3 svHandAngles = fieldVec(player, rotField);
        const glm::mat3 ctrl = axesFromAngles(&s.rot[h][0], true);
        const glm::mat3 svHand = axesFromAngles(&svHandAngles[0], true);
        const float apart =
            glm::distance(glm::transpose(ctrl) * (drawnPos - s.pos[h]), glm::transpose(svHand) * (physPos - svHandPos)) / m2u * 100.f;
        const float turn = turnBetween(glm::transpose(ctrl) * drawnRot, glm::transpose(svHand) * physRot);
        if(f.carry_offset >= 0 && vr_debug_carry.value)
        {
            const glm::vec3 o = fieldVec(ed, f.carry_offset);
            const glm::vec3 want = svHandPos + svHand * glm::vec3{o.x, -o.y, o.z};
            const Held& hd = holding[h];
            glm::vec3 p{0.f};
            glm::mat3 r{1.f};
            const bool listen = serverInHand(ent, h, e.model, p, r);
            Con_Printf("carry check: the physical %.2f cm off its hand's place for it (.carry_offset); drawn at %.2f cm from "
                       "it in the hand (listen %d, placed %d, %.3f s ago); the player's speed %.2f; drawn %.2f cm off its place from the controller\n",
                glm::distance(want, physPos) / m2u * 100.f, glm::distance(hd.pos, glm::vec3{o.x, -o.y, o.z}) / m2u * 100.f,
                listen ? 1 : 0, hd.placed ? 1 : 0, cl.time - hd.since,
                glm::length(glm::vec3{player->v.velocity[0], player->v.velocity[1], player->v.velocity[2]}),
                glm::distance(drawnPos, s.pos[h] + ctrl * hd.pos) / m2u * 100.f);
        }

        // The drawn fist against the drawn prop: moved with it onto the physical one, measured there.
        glm::vec3 pos = s.pos[h], angles = s.rot[h];
        drawnHand(h, pos, angles);
        std::vector<glm::vec4> spheres;
        fistInWorld(h, pos, angles, spheres);
        const glm::mat3 toPhys = physRot * glm::transpose(drawnRot);
        for(glm::vec4& sp : spheres)
        {
            sp = glm::vec4{physPos + toPhys * (glm::vec3{sp} - drawnPos), sp.w};
        }
        FistContact drawnGap, physGap;
        const bool haveDrawn = fistContact(ed, spheres, 64.f * m2u, drawnGap);
        // The server's hand against the physical prop.
        fistInWorld(h, svHandPos, svHandAngles, spheres);
        const bool havePhys = fistContact(ed, spheres, 64.f * m2u, physGap);
        const float gap = haveDrawn ? drawnGap.gap / m2u * 100.f : 999.f;
        Con_Printf("carry check: %s hand holds %d (%s%s): in the hand, drawn %.2f cm %.1f deg off the physical; fist gap drawn "
                   "%.2f cm, physical %.2f cm; in the world %.2f cm apart (the controller %.2f cm off the server's hand)\n",
            h == 1 ? "main" : "off", ent, e.model->name, both.ent == ent ? ", both hands" : "", apart, turn, gap,
            havePhys ? physGap.gap / m2u * 100.f : 999.f, world, glm::distance(s.pos[h], svHandPos) / m2u * 100.f);
        worstApart = std::fmax(worstApart, apart);
        worstTurn = std::fmax(worstTurn, turn);
        worstGap = std::fmax(worstGap, gap);
        worstWorld = std::fmax(worstWorld, world);
        count++;
    }
    Con_Printf("carry check: %d held; worst in the hand %.2f cm %.1f deg off the physical, fist gap %.2f cm, world %.2f cm\n",
        count, worstApart, worstTurn, count ? worstGap : 0.f, worstWorld);
}

} // namespace qvr::held

// End of CL_RelinkEntities: the local player's held objects are drawn in the hands drawn this
// frame (and ease back to the server's position when let go).
extern "C" void VR_RelinkHeld(void)
{
    if(!(cl.protocolflags & PRFL_QUAKEVR) || cls.state != ca_connected || cls.demoplayback)
    {
        reset();
        carried[0] = carried[1] = 0;
        return;
    }

    QVR_PROFILE("held");
    const hands::State& s = hands::current();
    const bool playerPlaced = cls.signon == SIGNONS && cl.viewentity > 0 && cl.viewentity < cl.num_entities &&
                              cl_entities[cl.viewentity].msgtime > 0.0;
    playerFrames = playerPlaced ? std::min(playerFrames + 1, handsInWorld) : 0;
    carried[0] = cl.stats[protocol::STAT_QVR_CARRYOFF];
    carried[1] = cl.stats[protocol::STAT_QVR_CARRYMAIN];
    const int bothEnt = carried[1] && carried[1] == carried[0] ? carried[1] : 0;
    if(!vr_carry_local.value)
    {
        reset();
    }
    else
    {
        if(both.ent && both.ent != bothEnt)
        {
            leaveBoth();
        }
        for(int h = 0; h < 2; h++)
        {
            holdFrame(h, s, bothEnt);
        }
        meetFrame(s);
        wallFrame(s);
        bothFrame(s, bothEnt);
        for(Easing& ea : easing)
        {
            easeFrame(ea);
        }
        trace(s);
        drawnFrom = s;
    }

    // Held in both hands: its centre (as drawn: in the hands this frame, or where the server has it) from the middle
    // of the hands, for a two-handed throw.
    if(bothEnt && s.valid && valid(bothEnt, nullptr))
    {
        bothCentre = (held::drawnCentre(bothEnt) - (s.pos[0] + s.pos[1]) * 0.5f) / units::metresToUnits();
    }
}
