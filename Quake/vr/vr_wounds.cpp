// vr_wounds.cpp -- see vr_wounds.hpp.

#include "vr_wounds.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_modelcollide.hpp"
#include "vr_particles.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"
#include "vr_avatar.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

extern "C" qboolean Image_WritePNG(const char* name, byte* data, int width, int height, int bpp, qboolean upsidedown); // image.c

namespace qvr::wounds
{
namespace
{

constexpr int layerSize = 256;         // a mask's largest side, in texels
constexpr int maxSplats = 16;          // per draw (the shader's uniform array)
constexpr float tick = 0.1f;           // seconds between the drying, cooling and healing steps
constexpr float dryTime = 28.f;        // seconds a soaked model takes to dry (1/255 a step)
constexpr float coolTime = 4.f;        // seconds a fresh burn's embers take to go out
constexpr float healRate = 0.08f;      // of the blood a step while healing (all of it in about 1.3 s)

// The server's kinds (QC/vr_wounds.qc QVR_WOUND_*).
enum Kind : int
{
    KindShot = 1,
    KindNail = 2,
    KindMelee = 3,
    KindBlast = 4,
    KindBurn = 5,
    KindZap = 6,
    KindLava = 7,
    KindSlime = 8,
    KindLiquid = 9,
};

struct Event
{
    int num{0};
    glm::vec3 org{0.f};
    glm::vec3 dir{0.f, 0.f, -1.f};
    int kind{0};
    int amount{0};
    int extra{0};
};

// Five vec4s, as the paint shader reads them (gl_shaders.h's wound_paint_fragment_shader).
struct Splat
{
    glm::vec4 v[5]{};
};
static_assert(sizeof(Splat) == 20 * sizeof(float));

struct Mask
{
    const entity_t* ent{nullptr}; // null: free
    const qmodel_t* model{nullptr};
    int w{0}, h{0};               // its region of its layer (from the corner)
    bool view{false};             // the player's own body or hand: never taken for another
    double lastDrawn{-1e9};       // realtime
    double painted{-1e9};         // realtime of its last paint
    float wetLeft{0.f};           // seconds it may still be drying
    float hotLeft{0.f};           // seconds its embers may still glow
    float heal{0.f};              // blood and char still to take off (a heal), 0..1
    float waterline{0.f};         // its last liquid's surface (drips under it)
    int liquid{0};                // 0 water, 1 slime (the drips' colour)
    double dripNext{0.0};
};

std::vector<Event> events;
std::vector<Mask> masks; // one a layer
std::unordered_map<const entity_t*, int> maskOf;
GLuint array = 0;
GLuint fbo = 0;
int layers = 0;
double lastTick = -1.0;
int playerHealth = -1000;
std::uint32_t rng = 0x9e3779b9u;
float lastPaintMs = 0.f;
int paintsTotal = 0;
bool painting = false;
bool chanOn[3]{true, true, true}; // vr_wounds, vr_wounds_burns, vr_wounds_wet as last seen

[[nodiscard]] float rnd()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return static_cast<float>(rng & 0xFFFFFF) / static_cast<float>(0x1000000);
}

[[nodiscard]] float rnd(float lo, float hi)
{
    return lo + (hi - lo) * rnd();
}

[[nodiscard]] bool enabled()
{
    return vr_wounds.value != 0.f || vr_wounds_burns.value != 0.f || vr_wounds_wet.value != 0.f;
}

// What the options let a splat paint.
[[nodiscard]] glm::vec4 allowed(glm::vec4 paint)
{
    if(!vr_wounds.value)
    {
        paint.r = 0.f;
    }
    if(!vr_wounds_burns.value)
    {
        paint.g = paint.a = 0.f;
    }
    if(!vr_wounds_wet.value)
    {
        paint.b = 0.f;
    }
    return paint;
}

[[nodiscard]] int poolSize()
{
    return std::clamp(static_cast<int>(vr_wounds_pool.value), 8, 256);
}

void releaseTexture()
{
    if(array)
    {
        glDeleteTextures(1, &array);
        array = 0;
    }
    if(fbo)
    {
        GL_DeleteFramebuffersFunc(1, &fbo);
        fbo = 0;
    }
    layers = 0;
    masks.clear();
    maskOf.clear();
}

// The texture array and its framebuffer, `poolSize()` layers (made again, empty, when that changes).
bool ensureTexture()
{
    const int want = poolSize();
    if(array && layers == want)
    {
        return true;
    }
    releaseTexture();
    glGenTextures(1, &array);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, array);
    GL_TexStorage3DFunc(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, layerSize, layerSize, want);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 0);
    GL_GenFramebuffersFunc(1, &fbo);
    layers = want;
    masks.assign(static_cast<std::size_t>(want), Mask{});
    maskOf.clear();
    Con_DPrintf("wounds: %d masks of %dx%d (%.1f MB)\n", want, layerSize, layerSize,
        static_cast<double>(want) * layerSize * layerSize * 4.0 / (1024.0 * 1024.0));
    return true;
}

// ----------------------------------------------------------------------------
// Drawing into the masks: begun once for a frame's paints, in the view's setup (VR_SetupViewEntities, before R_RenderView).
// Nothing reads back what was bound (a glGet waits for the driver's thread): at the end the window's framebuffer is
// bound, as after the shadow maps; R_RenderScene binds the scene's target and sets its viewport before it draws.

void begin()
{
    if(painting)
    {
        return;
    }
    painting = true;
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, fbo);
}

void end()
{
    if(!painting)
    {
        return;
    }
    painting = false;
    GL_BlendEquationFunc(GL_FUNC_ADD);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_CULL_BACK | GLS_ATTRIBS(0));
    glBlendFunc(GL_ONE, GL_ZERO); // what GLS_BLEND_OPAQUE sets (the cache believes it)
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, 0);
}

void target(int layer, const Mask& m)
{
    GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, array, 0, layer);
    glViewport(0, 0, m.w, m.h);
}

// Takes `amount` (0..1 each: r g b a) off every texel of mask `layer` (all of its layer: 1 clears it).
void subtract(int layer, const glm::vec4& amount)
{
    begin();
    const Mask& m = masks[static_cast<std::size_t>(layer)];
    GL_FramebufferTextureLayerFunc(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, array, 0, layer);
    glViewport(0, 0, amount == glm::vec4{1.f} ? layerSize : m.w, amount == glm::vec4{1.f} ? layerSize : m.h);
    GL_UseProgram(glprogs.viewblend);
    GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
    glBlendFunc(GL_ONE, GL_ONE);
    GL_BlendEquationFunc(GL_FUNC_REVERSE_SUBTRACT);
    GL_Uniform4fvFunc(0, 1, &amount.x);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBlendFunc(GL_ONE, GL_ZERO);
}

void paint(int layer, entity_t* e, const std::vector<Splat>& splats)
{
    if(splats.empty())
    {
        return;
    }
    begin();
    target(layer, masks[static_cast<std::size_t>(layer)]);
    for(std::size_t i = 0; i < splats.size(); i += maxSplats)
    {
        const int n = static_cast<int>(std::min<std::size_t>(maxSplats, splats.size() - i));
        GL_BlendEquationFunc(GL_MAX);
        R_PaintAliasWounds(e, n, &splats[i].v[0].x);
    }
    GL_BlendEquationFunc(GL_FUNC_ADD);
    masks[static_cast<std::size_t>(layer)].painted = realtime;
    paintsTotal++;
}

// ----------------------------------------------------------------------------
// The masks.

// Its skin's shape: the region of a layer its mask takes (the skin's size up to 256 on its longer side). False for a
// model its mask can't be for (not an alias model, several surfaces: several skins over one layout).
[[nodiscard]] bool regionOf(const qmodel_t* model, int& w, int& h)
{
    if(!model || model->type != mod_alias)
    {
        return false;
    }
    auto* hdr = static_cast<aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || Mod_NextSurface(hdr) || hdr->numskins <= 0)
    {
        return false;
    }
    const gltexture_t* t = hdr->gltextures[0][0];
    const int sw = t ? static_cast<int>(t->source_width) : hdr->skinwidth;
    const int sh = t ? static_cast<int>(t->source_height) : hdr->skinheight;
    if(sw <= 0 || sh <= 0)
    {
        return false;
    }
    const float k = std::min(1.f, static_cast<float>(layerSize) / static_cast<float>(std::max(sw, sh)));
    w = std::clamp(static_cast<int>(std::lround(sw * k)), 4, layerSize);
    h = std::clamp(static_cast<int>(std::lround(sh * k)), 4, layerSize);
    return true;
}

// The player's body builds share their skin's layout: a mask stays across them.
[[nodiscard]] bool sameLayout(const qmodel_t* a, const qmodel_t* b)
{
    if(a == b)
    {
        return true;
    }
    return a && b && std::strncmp(a->name, "progs/vrbody", 12) == 0 && std::strncmp(b->name, "progs/vrbody", 12) == 0;
}

void freeMask(int layer)
{
    Mask& m = masks[static_cast<std::size_t>(layer)];
    if(m.ent)
    {
        maskOf.erase(m.ent);
    }
    m = Mask{};
}

// The mask of `e` (made, empty, when `create`), or -1.
int acquire(const entity_t* e, bool view, bool create)
{
    if(!ensureTexture())
    {
        return -1;
    }
    const auto it = maskOf.find(e);
    if(it != maskOf.end())
    {
        Mask& m = masks[static_cast<std::size_t>(it->second)];
        if(sameLayout(m.model, e->model))
        {
            m.model = e->model;
            return it->second;
        }
        freeMask(it->second); // another model now
    }
    if(!create)
    {
        return -1;
    }
    int w = 0, h = 0;
    if(!regionOf(e->model, w, h))
    {
        return -1;
    }

    // A free layer, else the one least worth keeping: drawn longest ago (and farther), never the player's own.
    int best = -1;
    float bestScore = -1.f;
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    for(int i = 0; i < layers; i++)
    {
        const Mask& m = masks[static_cast<std::size_t>(i)];
        if(!m.ent)
        {
            best = i;
            break;
        }
        if(m.view || m.painted == realtime)
        {
            continue;
        }
        const glm::vec3 at{m.ent->origin[0], m.ent->origin[1], m.ent->origin[2]};
        const float score = static_cast<float>(realtime - m.lastDrawn) + glm::distance(at, eye) / 300.f;
        if(score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    if(best < 0)
    {
        return -1;
    }
    freeMask(best);
    Mask& m = masks[static_cast<std::size_t>(best)];
    m.ent = e;
    m.model = e->model;
    m.w = w;
    m.h = h;
    m.view = view;
    m.lastDrawn = realtime;
    maskOf[e] = best;
    subtract(best, glm::vec4{1.f}); // empty
    return best;
}

// ----------------------------------------------------------------------------
// Splats.

[[nodiscard]] Splat woundSplat(const glm::vec3& c, float r, const glm::vec3& axis, const glm::vec3& along, float stretch,
    float depth, float facing, float run, const glm::vec4& what)
{
    Splat s;
    s.v[0] = glm::vec4{c, r};
    s.v[1] = glm::vec4{axis, 0.f};
    s.v[2] = allowed(what);
    s.v[3] = glm::vec4{along, rnd(0.f, 97.f)};
    s.v[4] = glm::vec4{depth, facing, stretch, run};
    return s;
}

// Charred fully within `inner` of `c`, raggedly out to `r`.
[[nodiscard]] Splat burnSplat(const glm::vec3& c, float r, float inner, const glm::vec3& axis, bool blast, float facing, const glm::vec4& what)
{
    Splat s;
    s.v[0] = glm::vec4{c, r};
    s.v[1] = glm::vec4{axis, blast ? 2.f : 1.f};
    s.v[2] = allowed(what);
    s.v[3] = glm::vec4{0.f, 0.f, 0.f, rnd(0.f, 97.f)};
    s.v[4] = glm::vec4{inner, facing, 0.55f, 0.f};
    return s;
}

// Everything under `surface` (world height), fully `soak` units under it; `patch` > 0: only in patches.
[[nodiscard]] Splat liquidSplat(float surface, float soak, float patch, const glm::vec4& what)
{
    Splat s;
    s.v[0] = glm::vec4{0.f, 0.f, surface, soak};
    s.v[1] = glm::vec4{0.f, 0.f, 1.f, 3.f};
    s.v[2] = allowed(what);
    s.v[3] = glm::vec4{0.f, 0.f, 0.f, rnd(0.f, 97.f)};
    s.v[4] = glm::vec4{0.f, 0.f, patch, 0.f};
    return s;
}

[[nodiscard]] bool paintsAnything(const Splat& s)
{
    return s.v[2] != glm::vec4{0.f};
}

// How big a blow's wound is (world units), how far it runs down.
struct WoundSize
{
    float r{2.f};
    float stretch{1.f};
    float run{0.f};
};

[[nodiscard]] WoundSize woundSize(int kind, int amount)
{
    WoundSize s;
    const float a = static_cast<float>(amount);
    switch(kind)
    {
        case KindShot: s.r = std::min(1.6f + 0.1f * a, 3.f); break;
        case KindMelee:
            s.r = std::min(2.4f + 0.05f * a, 5.f);
            s.stretch = rnd(1.8f, 2.6f);
            break;
        default: s.r = std::min(2.f + 0.07f * a, 3.8f); break;
    }
    s.r *= rnd(0.85f, 1.15f);
    const float runChance = kind == KindShot ? 0.45f : kind == KindMelee ? 1.f : 0.75f;
    if(rnd() < runChance)
    {
        s.run = s.r * rnd(1.5f, kind == KindMelee ? 4.f : 3.f);
    }
    return s;
}

constexpr glm::vec4 paintBlood{1.f, 0.f, 0.f, 0.f};

// ----------------------------------------------------------------------------
// Where a blow meets a model: its triangles as drawn (vr_modelcollide.cpp; Quake's alias models), wound clockwise
// seen from outside.

struct Surface
{
    std::vector<glm::vec3> tris; // three corners each
    glm::vec3 lo{0.f}, hi{0.f};
};

[[nodiscard]] glm::vec3 outward(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
{
    const glm::vec3 n = glm::cross(b - a, c - a);
    const float l = glm::length(n);
    return l > 1e-8f ? -n / l : glm::vec3{0.f, 0.f, 1.f};
}

bool surfaceOf(const entity_t& e, int num, Surface& out)
{
    if(!modelcollide::drawnTriangles(e, num, out.tris) || out.tris.empty())
    {
        return false;
    }
    out.lo = glm::vec3{1e30f};
    out.hi = glm::vec3{-1e30f};
    for(const glm::vec3& p : out.tris)
    {
        out.lo = glm::min(out.lo, p);
        out.hi = glm::max(out.hi, p);
    }
    return true;
}

// The point on the model a blow at `org` going `dir` struck: the front face the line through it meets nearest to it
// (64 units either side), else the front-facing triangle nearest the line. Its outward normal too.
bool strike(const Surface& s, const glm::vec3& org, const glm::vec3& dir, glm::vec3& at, glm::vec3& normal)
{
    const glm::vec3 a0 = org - dir * 64.f;
    const glm::vec3 d = dir * 128.f;
    float bestT = 1e30f;
    for(std::size_t i = 0; i + 2 < s.tris.size(); i += 3)
    {
        const glm::vec3& a = s.tris[i];
        const glm::vec3 e1 = s.tris[i + 1] - a, e2 = s.tris[i + 2] - a;
        const glm::vec3 pv = glm::cross(d, e2);
        const float det = glm::dot(e1, pv);
        if(det >= -1e-9f) // not going into its front
        {
            continue;
        }
        const float inv = 1.f / det;
        const glm::vec3 tv = a0 - a;
        const float u = glm::dot(tv, pv) * inv;
        if(u < 0.f || u > 1.f)
        {
            continue;
        }
        const glm::vec3 qv = glm::cross(tv, e1);
        const float v = glm::dot(d, qv) * inv;
        if(v < 0.f || u + v > 1.f)
        {
            continue;
        }
        const float t = glm::dot(e2, qv) * inv;
        if(t < 0.f || t > 1.f)
        {
            continue;
        }
        if(std::fabs(t - 0.5f) < std::fabs(bestT - 0.5f))
        {
            bestT = t;
            at = a0 + d * t;
            normal = outward(a, s.tris[i + 1], s.tris[i + 2]);
        }
    }
    if(bestT < 1e29f)
    {
        return true;
    }
    // Missed (the box was struck, not the model): the facing triangle nearest the line.
    float best = 1e30f;
    for(std::size_t i = 0; i + 2 < s.tris.size(); i += 3)
    {
        const glm::vec3 n = outward(s.tris[i], s.tris[i + 1], s.tris[i + 2]);
        if(glm::dot(n, dir) > -0.2f)
        {
            continue;
        }
        const glm::vec3 c = (s.tris[i] + s.tris[i + 1] + s.tris[i + 2]) / 3.f;
        const glm::vec3 rel = c - org;
        const float along = glm::dot(rel, dir);
        const float off = glm::length(rel - dir * along) + std::fabs(along) * 0.25f;
        if(off < best)
        {
            best = off;
            at = c;
            normal = n;
        }
    }
    return best < 24.f;
}

// Up to `count` points of the model facing `from` (a blast's centre), nearer ones likelier.
void facingPoints(const Surface& s, const glm::vec3& from, int count, std::vector<std::pair<glm::vec3, glm::vec3>>& out)
{
    const std::size_t n = s.tris.size() / 3;
    if(!n)
    {
        return;
    }
    for(int tries = 0; tries < count * 12 && static_cast<int>(out.size()) < count; tries++)
    {
        const std::size_t i = std::min(n - 1, static_cast<std::size_t>(rnd() * static_cast<float>(n))) * 3;
        const glm::vec3 c = (s.tris[i] + s.tris[i + 1] + s.tris[i + 2]) / 3.f;
        const glm::vec3 nrm = outward(s.tris[i], s.tris[i + 1], s.tris[i + 2]);
        const glm::vec3 to = from - c;
        const float d = glm::length(to);
        if(d < 1e-3f || glm::dot(nrm, to / d) < 0.25f)
        {
            continue;
        }
        out.emplace_back(c, nrm);
    }
}

// ----------------------------------------------------------------------------
// Where a blow meets the player's own body and hands (jointed models, skinned on the GPU: no triangles here): capsules
// round them as they are posed -- the torso and the legs round the pelvis, each forearm from the elbow to the wrist,
// each hand past the wrist -- and the first one the blow's line goes into.

struct Capsule
{
    glm::vec3 a{0.f}, b{0.f};
    float r{1.f};
};

std::vector<Capsule> playerCapsules(entity_t* const own[3])
{
    std::vector<Capsule> out;
    const glm::vec3 up{0.f, 0.f, 1.f};
    if(own[0])
    {
        const glm::vec3 pelvis{own[0]->origin[0], own[0]->origin[1], own[0]->origin[2]};
        out.push_back({pelvis - up * 4.f, pelvis + up * 24.f, 7.5f}); // the torso
        out.push_back({pelvis - up * 30.f, pelvis - up * 4.f, 6.f});  // the legs
    }
    for(int hand = 0; hand < 2; hand++)
    {
        glm::vec3 wrist, dir;
        if(avatar::forearm(hand, wrist, dir))
        {
            avatar::ForearmFrame f;
            const float len = avatar::forearmFrame(hand, 1.f, f) && f.length > 1.f ? f.length : 10.f;
            out.push_back({wrist - dir * len, wrist, 2.3f});
            out.push_back({wrist + dir * 1.f, wrist + dir * 5.5f, 3.2f});
        }
        else if(own[1 + hand])
        {
            const glm::vec3 c{own[1 + hand]->origin[0], own[1 + hand]->origin[1], own[1 + hand]->origin[2]};
            out.push_back({c, c, 3.5f});
        }
    }
    return out;
}

[[nodiscard]] glm::vec3 closestOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float l2 = glm::dot(ab, ab);
    const float t = l2 > 1e-6f ? std::clamp(glm::dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
    return a + ab * t;
}

// The first capsule the line through `org` going `dir` goes into (48 units either side): where, and its normal there.
bool strikeCapsules(const std::vector<Capsule>& caps, const glm::vec3& org, const glm::vec3& dir, glm::vec3& at, glm::vec3& normal)
{
    for(float s = -48.f; s <= 48.f; s += 0.4f)
    {
        const glm::vec3 p = org + dir * s;
        for(const Capsule& c : caps)
        {
            const glm::vec3 q = closestOnSegment(p, c.a, c.b);
            const glm::vec3 d = p - q;
            const float l = glm::length(d);
            if(l < c.r)
            {
                at = p;
                normal = l > 1e-4f ? d / l : -dir;
                return true;
            }
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// A wound on one model.

struct Target
{
    entity_t* ent{nullptr};
    int num{-1};     // its entity number (cl_entities), -1: the player's own body or hand
    bool view{false};
    const std::vector<Capsule>* capsules{nullptr}; // the player's: where blows meet it
    glm::vec3 shift{0.f}; // the player's: where the body is drawn off the player's box (room-scale), added to the blow
};

void onLiquid(Mask& m, const Event& ev)
{
    m.waterline = ev.org.z;
    m.liquid = ev.extra == 1 ? 1 : 0;
}

void wound(const Target& t, const Event& ev)
{
    const int layer = acquire(t.ent, t.view, true);
    if(layer < 0)
    {
        return;
    }
    Mask& m = masks[static_cast<std::size_t>(layer)];
    std::vector<Splat> splats;
    const glm::vec3 dir = glm::length(ev.dir) > 0.1f ? glm::normalize(ev.dir) : glm::vec3{0.f, 0.f, -1.f};

    Surface surf;
    const bool mesh = !t.view && surfaceOf(*t.ent, t.num, surf);

    // A blow at a point: where it meets the model (its triangles), else (the player's jointed body and hands) as it
    // goes, over what faces it within its radius of its line.
    // The player's own are seen close: smaller wounds; the hands' smaller still (a hand is a few units across).
    const float own = !t.view ? 1.f : std::strncmp(t.ent->model->name, "progs/hand", 10) == 0 ? 0.65f : 0.75f;
    const auto blow = [&](const glm::vec3& org, const glm::vec3& way, int kind, const glm::vec4& what, float scale) {
        WoundSize ws = woundSize(kind, ev.amount);
        ws.run *= own;
        const float r = ws.r * scale * own;
        glm::vec3 at, n;
        if(mesh)
        {
            if(!strike(surf, org, way, at, n))
            {
                return;
            }
            splats.push_back(woundSplat(at, r, n, way, ws.stretch, r * 0.9f + 1.f, -0.25f, ws.run, what));
        }
        else if(t.view && t.capsules)
        {
            // Not struck where the jitter put it: a little nearer the line's middle, a few times.
            bool struck = false;
            for(int i = 0; i < 6 && !struck; i++)
            {
                struck = strikeCapsules(*t.capsules, org + t.shift + (i ? glm::vec3{rnd(-6.f, 6.f), rnd(-6.f, 6.f), rnd(-8.f, 8.f)} : glm::vec3{0.f}), way, at, n);
            }
            if(!struck)
            {
                return;
            }
            // (the capsules are round, the body flatter: reach in to its surface under the point, facing it)
            splats.push_back(woundSplat(at, r, n, way, ws.stretch, r * 0.9f + 6.f, 0.f, ws.run, what));
        }
        if(vr_wounds_debug.value)
        {
            Con_Printf("wounds: kind %d on %s (layer %d): %s at %.1f %.1f %.1f r %.1f\n", kind, t.ent->model->name,
                layer, "struck", at.x, at.y, at.z, r);
        }
    };
    // The player's: spread over the body (the server's point is the box's middle, not where it struck).
    const auto spread = [&](const glm::vec3& org, const glm::vec3& way) {
        if(!t.view)
        {
            return org;
        }
        glm::vec3 side = glm::cross(way, glm::vec3{0.f, 0.f, 1.f});
        side = glm::length(side) > 0.1f ? glm::normalize(side) : glm::vec3{1.f, 0.f, 0.f};
        const glm::vec3 up = glm::normalize(glm::cross(side, way));
        return org + side * rnd(-7.f, 7.f) + up * rnd(-6.f, 20.f); // the box's middle is the pelvis: more on the chest
    };

    switch(ev.kind)
    {
        case KindShot:
        {
            const int pellets = std::max(1, ev.extra);
            for(int i = 0; i < pellets; i++)
            {
                blow(spread(ev.org, dir), dir, KindShot, paintBlood, 1.f);
            }
            break;
        }
        case KindNail:
        case KindMelee: blow(spread(ev.org, dir), dir, ev.kind, paintBlood, 1.f); break;
        case KindZap:
        case KindBurn:
        {
            const bool zap = ev.kind == KindZap;
            const float r = ((zap ? 3.f : 4.f) + std::min(static_cast<float>(ev.amount), 60.f) * 0.08f) * own;
            const glm::vec3 org = spread(ev.org, dir);
            glm::vec3 at, n;
            if(mesh ? !strike(surf, org, dir, at, n) : !(t.capsules && strikeCapsules(*t.capsules, org + t.shift, dir, at, n)))
            {
                break;
            }
            splats.push_back(burnSplat(at, r, r * 0.25f, n, false, 0.1f, glm::vec4{0.f, zap ? 0.8f : 0.9f, 0.f, zap ? 0.6f : 0.9f}));
            if(!zap) // a small wound under it
            {
                blow(org, dir, KindNail, paintBlood, 0.6f);
            }
            m.hotLeft = coolTime;
            break;
        }
        case KindBlast:
        {
            const glm::vec3 centre = ev.org + t.shift;
            // Scorched on the side facing it: fully a little past the nearest of it, fading a hand's width on.
            float nearest = 1e30f;
            if(mesh)
            {
                for(const glm::vec3& p : surf.tris)
                {
                    nearest = std::min(nearest, glm::distance(centre, p));
                }
            }
            else
            {
                nearest = std::max(0.f, glm::distance(centre, glm::vec3{t.ent->origin[0], t.ent->origin[1], t.ent->origin[2]}) - 10.f);
            }
            const float a = std::min(static_cast<float>(ev.amount), 120.f);
            const float char_ = std::clamp(a / 120.f, 0.3f, 0.75f);
            splats.push_back(burnSplat(centre, nearest + 14.f + a * 0.12f, nearest + 3.f + a * 0.05f, glm::vec3{0.f}, true, 0.05f,
                glm::vec4{0.f, char_, 0.f, char_ * 0.8f}));
            m.hotLeft = coolTime;
            const int bleeds = std::clamp(ev.amount / 14, 1, 7);
            if(mesh)
            {
                std::vector<std::pair<glm::vec3, glm::vec3>> pts;
                facingPoints(surf, centre, bleeds, pts);
                for(const auto& [p, n] : pts)
                {
                    const WoundSize ws = woundSize(KindNail, ev.amount / 3);
                    splats.push_back(woundSplat(p, ws.r, n, glm::normalize(p - centre), 1.f, ws.r + 1.f, -0.25f, ws.run, paintBlood));
                }
            }
            else
            {
                for(int i = 0; i < bleeds; i++)
                {
                    blow(spread(ev.org, dir), dir, KindNail, paintBlood, 1.f);
                }
            }
            break;
        }
        case KindLava:
            splats.push_back(liquidSplat(ev.org.z, 14.f, 0.4f, glm::vec4{0.f, 0.9f, 0.f, 0.9f}));
            m.hotLeft = coolTime;
            break;
        case KindSlime: splats.push_back(liquidSplat(ev.org.z, 20.f, 0.5f, glm::vec4{0.f, 0.6f, 0.f, 0.f})); break;
        case KindLiquid:
            if(ev.extra == 2) // lava: nothing wet (its burns come with its damage)
            {
                break;
            }
            splats.push_back(liquidSplat(ev.org.z, 30.f, 0.f, glm::vec4{0.f, 0.f, 1.f, 0.f}));
            if(vr_wounds_wet.value)
            {
                m.wetLeft = dryTime;
                onLiquid(m, ev);
            }
            break;
        default: break;
    }

    splats.erase(std::remove_if(splats.begin(), splats.end(), [](const Splat& s) { return !paintsAnything(s); }), splats.end());
    paint(layer, t.ent, splats);
}

void apply(const Event& ev)
{
    if(vr_wounds_debug.value)
    {
        Con_Printf("wounds: event kind %d on entity %d%s, amount %d, extra %d, at %.1f %.1f %.1f going %.2f %.2f %.2f\n", ev.kind,
            ev.num, ev.num == cl.viewentity ? " (you)" : "", ev.amount, ev.extra, ev.org.x, ev.org.y, ev.org.z, ev.dir.x, ev.dir.y, ev.dir.z);
    }
    std::vector<Capsule> caps;
    Target targets[3];
    int count = 0;
    if(ev.num == cl.viewentity)
    {
        // The player: its own body and jointed hands, as drawn.
        entity_t* own[3]{};
        view::woundTargets(own);
        caps = playerCapsules(own);
        if(vr_wounds_debug.value >= 2)
        {
            for(const Capsule& c : caps)
            {
                Con_Printf("wounds: capsule %.1f %.1f %.1f - %.1f %.1f %.1f r %.1f\n", c.a.x, c.a.y, c.a.z, c.b.x, c.b.y, c.b.z, c.r);
            }
        }
        const entity_t& pl = cl_entities[cl.viewentity];
        for(entity_t* e : own)
        {
            if(e)
            {
                Target& t = targets[count++];
                t.ent = e;
                t.view = true;
                t.capsules = &caps;
                if(own[0])
                {
                    t.shift = glm::vec3{own[0]->origin[0] - pl.origin[0], own[0]->origin[1] - pl.origin[1], 0.f};
                }
            }
        }
    }
    else if(ev.num > 0 && ev.num < cl.num_entities)
    {
        entity_t& e = cl_entities[ev.num];
        if(e.model && e.model->type == mod_alias && e.msgtime >= cl.mtime[1] - 0.2)
        {
            targets[count++] = Target{&e, ev.num, false, nullptr, glm::vec3{0.f}};
        }
    }
    // The same draws for each (the player's body and hands: the same point, the same wound).
    const std::uint32_t seed = rng;
    for(int i = 0; i < count; i++)
    {
        rng = seed;
        wound(targets[i], ev);
    }
    static_cast<void>(rnd());
}

// A monster's head flying off (its model now its head's, progs/h_*.mdl): the neck and the face bloodied.
void bloodyHead(entity_t& e, int num)
{
    const int layer = acquire(&e, false, true);
    Surface surf;
    if(layer < 0 || !surfaceOf(e, num, surf))
    {
        return;
    }
    std::vector<Splat> splats;
    const float lowZ = surf.lo.z + (surf.hi.z - surf.lo.z) * 0.3f;
    for(int tries = 0; tries < 60 && splats.size() < 6; tries++)
    {
        const std::size_t n = surf.tris.size() / 3;
        const std::size_t i = std::min(n - 1, static_cast<std::size_t>(rnd() * static_cast<float>(n))) * 3;
        const glm::vec3 c = (surf.tris[i] + surf.tris[i + 1] + surf.tris[i + 2]) / 3.f;
        if(splats.size() < 3 && c.z > lowZ)
        {
            continue; // the neck first
        }
        const glm::vec3 nrm = outward(surf.tris[i], surf.tris[i + 1], surf.tris[i + 2]);
        const WoundSize ws = woundSize(KindNail, 20);
        splats.push_back(woundSplat(c, ws.r * 1.2f, nrm, -nrm, 1.f, ws.r + 1.f, -0.25f, ws.run, paintBlood));
    }
    paint(layer, &e, splats);
}

// ----------------------------------------------------------------------------
// Over time: drying, cooling, healing; the masks of entities gone freed.

void steps(float dt)
{
    // Drying: 1/255 of the wetness a step (dryTime from soaked); embers cooling; the player's heal.
    const int n = std::max(1, static_cast<int>(dt / tick + 0.5f));
    for(int i = 0; i < layers; i++)
    {
        Mask& m = masks[static_cast<std::size_t>(i)];
        if(!m.ent)
        {
            continue;
        }
        glm::vec4 take{0.f};
        if(m.wetLeft > 0.f)
        {
            take.b = static_cast<float>(n) * tick / dryTime;
            m.wetLeft -= dt;
        }
        if(m.hotLeft > 0.f)
        {
            take.a = static_cast<float>(n) * tick / coolTime;
            m.hotLeft -= dt;
        }
        if(m.heal > 0.f)
        {
            const float h = std::min(m.heal, healRate * static_cast<float>(n));
            take.r = take.g = h;
            m.heal -= h;
        }
        if(take != glm::vec4{0.f})
        {
            subtract(i, take);
        }
    }
}

void checkEntities()
{
    for(int i = 0; i < layers; i++)
    {
        Mask& m = masks[static_cast<std::size_t>(i)];
        if(!m.ent || m.view)
        {
            continue;
        }
        const auto* e = const_cast<entity_t*>(m.ent);
        if(sameLayout(m.model, e->model))
        {
            continue;
        }
        // Another model: its head flying off (the monster is its head now), or the slot taken by something else.
        const int num = static_cast<int>(e - cl_entities);
        const bool head = e->model && std::strncmp(e->model->name, "progs/h_", 8) == 0;
        const bool bled = vr_wounds.value && m.painted > -1e8;
        freeMask(i);
        if(head && bled && num > 0 && num < cl.num_entities)
        {
            bloodyHead(cl_entities[num], num);
        }
    }
}

// The player: healing takes the blood (and char) off; a respawn, all of it.
void playerState()
{
    const int health = cl.stats[STAT_HEALTH];
    const bool respawned = playerHealth <= 0 && playerHealth > -1000 && health > 0;
    const bool healed = playerHealth > 0 && health > playerHealth;
    const float heal = health >= 100 ? 1.f
                                     : static_cast<float>(health - playerHealth) / static_cast<float>(std::max(1, 100 - playerHealth));
    playerHealth = health;
    if(!respawned && !healed)
    {
        return;
    }
    for(int i = 0; i < layers; i++)
    {
        Mask& m = masks[static_cast<std::size_t>(i)];
        if(!m.ent || !m.view)
        {
            continue;
        }
        if(respawned)
        {
            subtract(i, glm::vec4{1.f});
            m.wetLeft = m.hotLeft = m.heal = 0.f;
        }
        else
        {
            m.heal = std::min(1.f, m.heal + heal);
        }
    }
}

// An option turned off: what it painted comes off every mask (turned on again, marks start afresh).
void optionsChanged()
{
    const bool on[3]{vr_wounds.value != 0.f, vr_wounds_burns.value != 0.f, vr_wounds_wet.value != 0.f};
    glm::vec4 take{0.f};
    if(chanOn[0] && !on[0])
    {
        take.r = 1.f;
    }
    if(chanOn[1] && !on[1])
    {
        take.g = take.a = 1.f;
    }
    if(chanOn[2] && !on[2])
    {
        take.b = 1.f;
    }
    std::copy(on, on + 3, chanOn);
    if(take == glm::vec4{0.f})
    {
        return;
    }
    for(int i = 0; i < layers; i++)
    {
        if(masks[static_cast<std::size_t>(i)].ent)
        {
            subtract(i, take);
        }
    }
}

// Water dripping off what is wet: from under its waterline (the monsters' triangles; the player's hands).
void drips(double now)
{
    if(!vr_wounds_wet.value || !particles::enabled())
    {
        return;
    }
    for(int i = 0; i < layers; i++)
    {
        Mask& m = masks[static_cast<std::size_t>(i)];
        if(!m.ent || m.wetLeft <= 0.f || now < m.dripNext || realtime - m.lastDrawn > 0.5)
        {
            continue;
        }
        const float wet = m.wetLeft / dryTime;
        m.dripNext = now + rnd(0.5f, 1.5f) / (0.4f + 5.f * wet * wet) * (m.view ? 2.5 : 1.0);
        const glm::vec3 color = m.liquid == 1 ? glm::vec3{0.22f, 0.3f, 0.08f} : glm::vec3{0.3f, 0.34f, 0.38f};
        glm::vec3 at;
        float floorZ;
        if(m.view)
        {
            // A hand (the body is under the view: its drips are the hands')
            if(std::strncmp(m.ent->model->name, "progs/vrbody", 12) == 0)
            {
                continue;
            }
            at = glm::vec3{m.ent->origin[0], m.ent->origin[1], m.ent->origin[2]} + glm::vec3{rnd(-1.5f, 1.5f), rnd(-1.5f, 1.5f), -1.f};
            floorZ = cl_entities[cl.viewentity].origin[2] - 24.f;
        }
        else
        {
            Surface surf;
            const int num = static_cast<int>(m.ent - cl_entities);
            if(num <= 0 || num >= cl.num_entities || !surfaceOf(*m.ent, num, surf))
            {
                continue;
            }
            bool found = false;
            const std::size_t n = surf.tris.size() / 3;
            for(int tries = 0; tries < 10 && !found; tries++)
            {
                const std::size_t k = std::min(n - 1, static_cast<std::size_t>(rnd() * static_cast<float>(n))) * 3;
                const glm::vec3 c = (surf.tris[k] + surf.tris[k + 1] + surf.tris[k + 2]) / 3.f;
                const glm::vec3 nrm = outward(surf.tris[k], surf.tris[k + 1], surf.tris[k + 2]);
                if(c.z < m.waterline && nrm.z < 0.3f)
                {
                    at = c + nrm * 0.6f;
                    found = true;
                }
            }
            if(!found)
            {
                continue;
            }
            floorZ = surf.lo.z;
        }
        const float h = std::max(at.z - floorZ, 1.f);
        particles::bloodDrip(at, std::sqrt(2.f * h / std::max(sv_gravity.value, 100.f)), floorZ, 0.35f, color);
    }
}

} // namespace

void parseEvent()
{
    Event ev;
    ev.num = MSG_ReadShort();
    for(int i = 0; i < 3; i++)
    {
        ev.org[i] = MSG_ReadCoord(cl.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        ev.dir[i] = static_cast<float>(MSG_ReadChar()) / 127.f;
    }
    ev.kind = MSG_ReadByte();
    ev.amount = MSG_ReadByte();
    ev.extra = MSG_ReadByte();
    if(vr_wounds_debug.value >= 2)
    {
        Con_Printf("wounds: received kind %d on %d\n", ev.kind, ev.num);
    }
    if(enabled() && events.size() < 512)
    {
        events.push_back(ev);
    }
}

void frame()
{
    if(!enabled() || !glprogs.woundpaint[0])
    {
        if(array)
        {
            releaseTexture();
        }
        events.clear();
        chanOn[0] = vr_wounds.value != 0.f;
        chanOn[1] = vr_wounds_burns.value != 0.f;
        chanOn[2] = vr_wounds_wet.value != 0.f;
        playerHealth = cl.stats[STAT_HEALTH];
        return;
    }
    QVR_GPU_PROFILE("wounds");
    const auto t0 = Sys_DoubleTime();
    if(!ensureTexture())
    {
        return;
    }
    optionsChanged();
    checkEntities();
    playerState();
    const auto t1 = Sys_DoubleTime();
    const std::size_t received = events.size();
    for(const Event& ev : events)
    {
        apply(ev);
    }
    events.clear();
    const auto t2 = Sys_DoubleTime();

    const double now = cl.time;
    if(lastTick < 0.0 || now < lastTick || now - lastTick > 5.0)
    {
        lastTick = now;
    }
    if(now - lastTick >= tick)
    {
        steps(static_cast<float>(now - lastTick));
        lastTick = now;
    }
    end();
    drips(now);
    lastPaintMs = static_cast<float>((Sys_DoubleTime() - t0) * 1000.0);
    if(lastPaintMs > 4.f)
    {
        Con_DPrintf("wounds: a slow frame, %.1f ms (%.1f before the %d wounds, %.1f painting them)%c", static_cast<double>(lastPaintMs),
            (t1 - t0) * 1000.0, static_cast<int>(received), (t2 - t1) * 1000.0, 10);
    }
}

bool replacesSkins()
{
    return vr_wounds.value != 0.f;
}

void clear()
{
    for(int i = 0; i < layers; i++)
    {
        masks[static_cast<std::size_t>(i)] = Mask{};
    }
    maskOf.clear();
    events.clear();
    lastTick = -1.0;
    playerHealth = -1000;
}

void test_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_wounds_test <entity number | self | ahead> <kind 1 shot, 2 nail, 3 melee, 4 blast, 5 burn, 6 zap, 7 lava, 8 slime, "
                   "9 liquid> [amount] [right] [up] [extra]\n");
        return;
    }
    if(std::strcmp(Cmd_Argv(1), "all") == 0) // every model drawn with an alias model (monsters, corpses, items): a stress test
    {
        char args[256];
        q_snprintf(args, sizeof(args), "%s %s %s %s %s", Cmd_Argc() > 2 ? Cmd_Argv(2) : "1", Cmd_Argc() > 3 ? Cmd_Argv(3) : "20",
            Cmd_Argc() > 4 ? Cmd_Argv(4) : "0", Cmd_Argc() > 5 ? Cmd_Argv(5) : "0", Cmd_Argc() > 6 ? Cmd_Argv(6) : "0");
        int n = 0;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& c = cl_entities[i];
            if(i != cl.viewentity && c.model && c.model->type == mod_alias && c.msgtime >= cl.mtime[1] - 0.2 &&
                std::strncmp(c.model->name, "progs/v_", 8) != 0)
            {
                Cbuf_InsertText(va("vr_wounds_test %d %s\n", i, args));
                n++;
            }
        }
        Con_Printf("vr_wounds_test: %d models\n", n);
        return;
    }
    Event ev;
    const bool self = std::strcmp(Cmd_Argv(1), "self") == 0;
    ev.num = self ? cl.viewentity : std::atoi(Cmd_Argv(1));
    if(std::strcmp(Cmd_Argv(1), "ahead") == 0) // the model nearest the view's line, ahead
    {
        const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
        vec3_t fwd, right, upv;
        AngleVectors(r_refdef.viewangles, fwd, right, upv);
        float best = 0.6f;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& c = cl_entities[i];
            if(i == cl.viewentity || !c.model || c.model->type != mod_alias || c.msgtime < cl.mtime[1] - 0.2)
            {
                continue;
            }
            const glm::vec3 to = glm::vec3{c.origin[0], c.origin[1], c.origin[2]} - eye;
            const float cosA = glm::dot(glm::normalize(to), glm::vec3{fwd[0], fwd[1], fwd[2]});
            if(vr_wounds_debug.value)
            {
                Con_Printf("  %d %s cos %.2f dist %.0f\n", i, c.model->name, cosA, glm::length(to));
            }
            if(cosA > best && glm::length(to) < 1000.f)
            {
                best = cosA;
                ev.num = i;
            }
        }
        if(ev.num > 0)
        {
            const entity_t& c = cl_entities[ev.num];
            Con_Printf("vr_wounds_test: entity %d (%s) at %.0f %.0f %.0f\n", ev.num, c.model->name, c.origin[0], c.origin[1], c.origin[2]);
        }
    }
    ev.kind = std::atoi(Cmd_Argv(2));
    ev.amount = Cmd_Argc() > 3 ? std::atoi(Cmd_Argv(3)) : 20;
    const float right = Cmd_Argc() > 4 ? static_cast<float>(std::atof(Cmd_Argv(4))) : 0.f;
    const float up = Cmd_Argc() > 5 ? static_cast<float>(std::atof(Cmd_Argv(5))) : 0.f;
    ev.extra = Cmd_Argc() > 6 ? std::atoi(Cmd_Argv(6)) : 0;
    if(ev.num <= 0 || ev.num >= cl.num_entities || !cl_entities[ev.num].model)
    {
        Con_Printf("vr_wounds_test: no entity %d\n", ev.num);
        return;
    }
    const entity_t& e = cl_entities[ev.num];
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    const glm::vec3 mid = origin + glm::vec3{(e.model->mins[0] + e.model->maxs[0]) * 0.5f, (e.model->mins[1] + e.model->maxs[1]) * 0.5f,
                                        (e.model->mins[2] + e.model->maxs[2]) * 0.5f};
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    glm::vec3 dir;
    if(self)
    {
        const float yaw = glm::radians(cl.viewangles[YAW]);
        dir = -glm::vec3{std::cos(yaw), std::sin(yaw), 0.f}; // from ahead
    }
    else
    {
        dir = glm::normalize(mid - eye);
    }
    glm::vec3 side = glm::cross(dir, glm::vec3{0.f, 0.f, 1.f});
    side = glm::length(side) > 0.1f ? glm::normalize(side) : glm::vec3{1.f, 0.f, 0.f};
    ev.dir = dir;
    ev.org = (self ? origin + glm::vec3{0.f, 0.f, 4.f} : mid) + side * right + glm::vec3{0.f, 0.f, up};
    if(ev.kind == KindBlast)
    {
        ev.org = mid - dir * 40.f + side * right + glm::vec3{0.f, 0.f, up};
    }
    if(ev.kind >= KindLava)
    {
        ev.org = origin + glm::vec3{0.f, 0.f, e.model->mins[2] + up}; // the surface: `up` over the feet
    }
    events.push_back(ev);
}

void dump_f()
{
    if(!array)
    {
        Con_Printf("vr_wounds_dump: no masks\n");
        return;
    }
    std::vector<byte> rgba(static_cast<std::size_t>(layerSize) * layerSize * 4);
    std::vector<byte> rgb(static_cast<std::size_t>(layerSize) * layerSize * 3);
    Sys_mkdir(va("%s/wounds", com_gamedir));
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    for(int i = 0; i < layers; i++)
    {
        const Mask& m = masks[static_cast<std::size_t>(i)];
        if(!m.ent)
        {
            continue;
        }
        GL_FramebufferTextureLayerFunc(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, array, 0, i);
        glReadPixels(0, 0, m.w, m.h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        for(int p = 0; p < m.w * m.h; p++)
        {
            // r blood, g char, b wetness; heat shown as white over them
            const int heat = rgba[static_cast<std::size_t>(p) * 4 + 3];
            for(int c = 0; c < 3; c++)
            {
                rgb[static_cast<std::size_t>(p) * 3 + c] = static_cast<byte>(std::max<int>(rgba[static_cast<std::size_t>(p) * 4 + c], heat));
            }
        }
        const char* slash = std::strrchr(m.model->name, '/');
        char name[MAX_OSPATH];
        q_snprintf(name, sizeof(name), "wounds/mask_%02d_%s.png", i, slash ? slash + 1 : m.model->name);
        Image_WritePNG(name, rgb.data(), m.w, m.h, 24, true);
        Con_Printf("vr_wounds_dump: %s (%dx%d)\n", name, m.w, m.h);
    }
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
}

void info_f()
{
    int used = 0;
    for(const Mask& m : masks)
    {
        used += m.ent ? 1 : 0;
    }
    Con_Printf("wounds: %d of %d masks used (%dx%d layers, %.1f MB), %d paints so far, last frame's work %.3f ms (CPU)\n", used,
        layers, layerSize, layerSize, static_cast<double>(layers) * layerSize * layerSize * 4.0 / (1024.0 * 1024.0), paintsTotal,
        static_cast<double>(lastPaintMs));
    for(int i = 0; i < layers; i++)
    {
        const Mask& m = masks[static_cast<std::size_t>(i)];
        if(m.ent)
        {
            Con_Printf("  %2d %s%s %dx%d, drawn %.1f s ago%s%s\n", i, m.model ? m.model->name : "?", m.view ? " (you)" : "", m.w, m.h,
                realtime - m.lastDrawn, m.wetLeft > 0.f ? ", wet" : "", m.hotLeft > 0.f ? ", hot" : "");
        }
    }
}

} // namespace qvr::wounds

extern "C" void VR_AliasWound(const entity_t* e, float out[4])
{
    using namespace qvr::wounds;
    out[0] = out[1] = out[2] = out[3] = 0.f;
    if(!array)
    {
        return;
    }
    const auto it = maskOf.find(e);
    if(it == maskOf.end())
    {
        return;
    }
    Mask& m = masks[static_cast<std::size_t>(it->second)];
    if(!sameLayout(m.model, e->model))
    {
        return;
    }
    m.lastDrawn = realtime;
    out[0] = static_cast<float>(it->second + 1);
    out[1] = static_cast<float>(m.w);
    out[2] = static_cast<float>(m.h);
    out[3] = static_cast<float>(std::fmod(cl.time, 1000.0));
}

extern "C" unsigned VR_WoundTexture(void)
{
    return qvr::wounds::array;
}
