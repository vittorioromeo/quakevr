// vr_held.cpp -- see vr_held.hpp.

#include "vr_held.hpp"
#include "vr_carry2h.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_profile.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_throw.hpp"
#include "vr_units.hpp"
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

glm::vec3 drawnCentre(int num)
{
    const entity_t& e = cl_entities[num];
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    if(!e.model || (e.model->type != mod_alias && e.model->type != mod_brush))
    {
        return origin;
    }

    const client::EntityVr* net = client::entityVr(num);
    const glm::vec3 zero{0.f};
    glm::vec3 lo, hi;
    modelBox(e.model, net ? net->scale : zero, net ? net->scaleOrigin : zero, net ? net->offset : zero, lo, hi);
    const bool brush = e.model->type == mod_brush;
    return origin + axesFromAngles(e.angles, brush) * ((lo + hi) * 0.5f * ENTSCALE_DECODE(e.scale)); // and Ironwail's scale
}

float surfaceDistance(edict_t* ent, const glm::vec3& point, glm::vec3* nearest)
{
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    thread_local std::vector<Triangle> triangles;
    if(!model || !drawnTriangles(ent, model, triangles))
    {
        return -1.f;
    }
    const glm::mat3 axes = axesFromAngles(ent->v.angles, model->type == mod_brush);
    const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    const glm::vec3 p = glm::transpose(axes) * (point - origin);
    float best = std::numeric_limits<float>::max();
    glm::vec3 at{0.f};
    for(const Triangle& t : triangles)
    {
        // The point of the triangle nearest p (Ericson, Real-Time Collision Detection, 5.1.5).
        const glm::vec3 &a = t.p[0], &b = t.p[1], &c = t.p[2];
        const glm::vec3 ab = b - a, ac = c - a, ap = p - a;
        glm::vec3 q;
        const float d1 = glm::dot(ab, ap), d2 = glm::dot(ac, ap);
        const glm::vec3 bp = p - b;
        const float d3 = glm::dot(ab, bp), d4 = glm::dot(ac, bp);
        const glm::vec3 cp = p - c;
        const float d5 = glm::dot(ab, cp), d6 = glm::dot(ac, cp);
        const float vc = d1 * d4 - d3 * d2, vb = d5 * d2 - d1 * d6, va = d3 * d6 - d5 * d4;
        if(d1 <= 0.f && d2 <= 0.f)
        {
            q = a;
        }
        else if(d3 >= 0.f && d4 <= d3)
        {
            q = b;
        }
        else if(vc <= 0.f && d1 >= 0.f && d3 <= 0.f)
        {
            q = a + ab * (d1 / (d1 - d3));
        }
        else if(d6 >= 0.f && d5 <= d6)
        {
            q = c;
        }
        else if(vb <= 0.f && d2 >= 0.f && d6 <= 0.f)
        {
            q = a + ac * (d2 / (d2 - d6));
        }
        else if(va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f)
        {
            q = b + (c - b) * ((d4 - d3) / ((d4 - d3) + (d5 - d6)));
        }
        else
        {
            const float denom = 1.f / (va + vb + vc);
            q = a + ab * (vb * denom) + ac * (vc * denom);
        }
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

namespace
{

// vr_debug_carry: per hand, the last probe (the server's), shown for a moment.
struct CarryProbe
{
    double time{-1.0};
    glm::vec3 corners[8]{};
    glm::vec3 at{0.f}, nearest{0.f};
    float distance{0.f}, reach{0.f};
};
CarryProbe carryProbes[2];

} // namespace

void noteCarryProbe(int hand, edict_t* ent, const glm::vec3& at, float distance, const glm::vec3& nearest, float reach)
{
    if(!vr_debug_carry.value || hand < 0 || hand > 1)
    {
        return;
    }
    CarryProbe& p = carryProbes[hand];
    p.time = realtime;
    p.at = at;
    p.nearest = nearest;
    p.distance = distance;
    p.reach = reach;
    // The model's turned box (as the touch test's), its corners in the world.
    const int index = static_cast<int>(ent->v.modelindex);
    const qmodel_t* model = index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
    glm::vec3 lo{ent->v.mins[0], ent->v.mins[1], ent->v.mins[2]}, hi{ent->v.maxs[0], ent->v.maxs[1], ent->v.maxs[2]};
    if(model && model->type == mod_alias)
    {
        using namespace progs;
        const FieldOffsets& f = fields();
        modelBox(model, fieldVec(ent, f.model_scale), fieldVec(ent, f.model_scale_origin), fieldVec(ent, f.model_offset), lo, hi);
    }
    const glm::mat3 axes = axesFromAngles(ent->v.angles, model && model->type == mod_brush);
    const glm::vec3 origin{ent->v.origin[0], ent->v.origin[1], ent->v.origin[2]};
    for(int i = 0; i < 8; i++)
    {
        p.corners[i] = origin + axes * glm::vec3{(i & 1) ? hi.x : lo.x, (i & 2) ? hi.y : lo.y, (i & 4) ? hi.z : lo.z};
    }
}

void drawCarryProbes()
{
    if(!vr_debug_carry.value)
    {
        return;
    }
    for(const CarryProbe& p : carryProbes)
    {
        if(p.time < 0.0 || realtime - p.time > 0.1)
        {
            continue;
        }
        const bool within = p.reach <= 0.f || (p.distance >= 0.f && p.distance <= p.reach);
        const glm::vec4 colour = within ? glm::vec4{0.2f, 1.f, 0.3f, 1.f} : glm::vec4{1.f, 0.25f, 0.2f, 1.f};
        constexpr int edges[12][2] = {{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
        for(const auto& e : edges)
        {
            lines::line(p.corners[e[0]], p.corners[e[1]], 0.2f, colour, colour);
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
    // Never pushed further out than this (round 21, second pass: was half a metre; a thing is taken only within
    // vr_carry_reach of its surface now, so a big thing gripped deep inside its box no longer floats away), nor drawn
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
    thread_local std::vector<Triangle> triangles;
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

// For this long after the server first says a hand holds an object, its place in the hand is
// still taken from where the server has it (a caught object's jump to the hand may come a packet
// late); then it is kept.
constexpr double placeTime = 0.1;

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

[[nodiscard]] bool valid(int ent, const qmodel_t* model)
{
    return ent > 0 && ent < cl.num_entities && cl_entities[ent].model &&
           (!model || cl_entities[ent].model == model) && cl_entities[ent].model->type != mod_sprite;
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
        hd.since = cl.time;
    }

    hd.drawn = false;
    if(!hd.ent || !s.valid || !valid(hd.ent, hd.placed ? hd.model : nullptr))
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
    if(!hd.placed || cl.time - hd.since < placeTime)
    {
        const glm::vec3 o{e.msg_origins[0][0], e.msg_origins[0][1], e.msg_origins[0][2]};
        hd.pos = glm::transpose(hand) * (o - s.pos[h]);
        hd.rot = glm::transpose(hand) * held::axesFromAngles(e.msg_angles[0], brush);
        hd.model = e.model;
        hd.placed = true;
    }

    hd.lastPos = s.pos[h] + hand * hd.pos;
    hd.lastRot = hand * hd.rot;
    place(e, hd.lastPos, hd.lastRot);
    hd.drawn = true;
}

// Held in both hands no more (one let go, or both): each hand drawn on its grip eases back onto its controller, and
// the hand still holding it holds it from where it is (as the server does), not from where it first took it.
void leaveBoth()
{
    for(int h = 0; h < 2; h++)
    {
        handEase[h] = {cl.time, both.drawn[h].pos - both.hands[h].pos,
            glm::normalize(both.drawn[h].rot * glm::inverse(both.hands[h].rot))};
        Held& hd = holding[h];
        if(hd.ent == both.ent && valid(hd.ent, both.model))
        {
            const glm::mat3 hand = glm::mat3_cast(both.hands[h].rot);
            hd.pos = glm::transpose(hand) * (both.object.pos - both.hands[h].pos);
            hd.rot = glm::transpose(hand) * glm::mat3_cast(both.object.rot);
            hd.model = both.model;
            hd.placed = true;
            hd.since = cl.time - placeTime; // kept (not taken again from the server's place)
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
    if(!ent || !s.valid || !valid(ent, both.ent == ent ? both.model : nullptr))
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
        both.hold = carry2h::record(object, hands);
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

int heldEntity(int hand)
{
    const Held& hd = holding[hand];
    return hd.drawn && valid(hd.ent, hd.model) ? hd.ent : 0;
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
        bothFrame(s, bothEnt);
        for(Easing& ea : easing)
        {
            easeFrame(ea);
        }
        trace(s);
    }

    // Held in both hands: its centre (as drawn: in the hands this frame, or where the server has it) from the middle
    // of the hands, for a two-handed throw.
    if(bothEnt && s.valid && valid(bothEnt, nullptr))
    {
        bothCentre = (held::drawnCentre(bothEnt) - (s.pos[0] + s.pos[1]) * 0.5f) / units::metresToUnits();
    }
}
