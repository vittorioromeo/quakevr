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

#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Remove.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmod.hpp"
#include "Zancle/Math/Lround.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <string.h>

extern "C" qboolean Image_WritePNG(const char* name, byte* data, int width, int height, int bpp, qboolean upsidedown); // image.c

namespace qvr::wounds
{
namespace
{

constexpr int layerSize = 256;         // a mask's largest side, in texels
constexpr int ownSlots = 4;            // the fine masks (vr_wounds_own_res): your body and two hands, the body's right side (the last)
constexpr int maxSplats = 16;          // per draw (the shader's uniform array)
constexpr float tick = 0.1f;           // seconds between the drying, cooling and healing steps
constexpr float dryTime = 28.f;        // seconds a soaked model takes to dry (1/255 a step)
constexpr float coolTime = 4.f;        // seconds a fresh burn's embers take to go out
constexpr float healRate = 0.08f;      // of the blood a step while healing (all of it in about 1.3 s)

// The server's kinds (QC/vr_wounds.qc QVR_WOUND_*).
enum Kind : int
{
    KindClear = -1, // QVR_SVC_WOUNDCLEAR: the entity was removed
    KindShot = 1,
    KindNail = 2,
    KindMelee = 3,
    KindBlast = 4,
    KindBurn = 5,
    KindZap = 6,
    KindLava = 7,
    KindSlime = 8,
    KindLiquid = 9,
    KindGear = 10, // a weapon thrown from a hand (extra: 0 the off hand, 1 the main hand): its blood goes with it (QC DropWeaponInHand)
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
    double lastDrawn{-1e9};       // vr_gametime
    double painted{-1e9};         // vr_gametime of its last paint
    float wetLeft{0.f};           // seconds it may still be drying
    float hotLeft{0.f};           // seconds its embers may still glow
    float heal{0.f};              // blood and char still to take off (a heal), 0..1
    float waterline{0.f};         // its last liquid's surface (drips under it)
    int liquid{0};                // 0 water, 1 slime (the drips' colour)
    double dripNext{0.0};
    float sides[2]{0.f, 0.f};     // your body's: the bones of its right side (bits 0..23, 24..47), painted into the last fine
                                  // layer (its arms and legs share their skin's texels, mirrored): 0 none
    bool gear{false};             // a weapon's or a prop's you held: washed, kept dropped and held again (vr_gore_gear)
    glm::vec3 lastAt{0.f};        // gear in the world: where it was last frame (gone far: its slot is another's now)
};

za::Vector<Event> events;
za::Vector<Mask> masks; // one a layer
ankerl::unordered_dense::map<const entity_t*, int> maskOf;
GLuint array = 0;
GLuint fbo = 0;
int layers = 0;       // the pool's (array): masks 0 .. layers - 1
GLuint fineArray = 0; // your own body's and hands' finer masks (vr_wounds_own_res): masks layers .. layers + ownSlots - 1
GLuint bloodArray = 0; // ... and the blood on them that isn't theirs (spatter, gibs: one channel, as fine): healing leaves it
bool foreign = false;  // painting blood that isn't yours (paintOnYou's): into bloodArray, for the fine masks
int fineSize = 0;     // their side in texels (0: none; yours in the pool)
double lastTick = -1.0;
int playerHealth = -1000;
za::U32 rng = 0x9e3779b9u;
float lastPaintMs = 0.f;
int paintsTotal = 0;
bool painting = false;
bool chanOn[3]{true, true, true}; // vr_wounds, vr_wounds_burns, vr_wounds_wet as last seen
bool bloodOnly = false;           // re-opening the player's wounds (reopen): their blood only, no char, no heat

// Blood on your gear (vr_gore_gear): a weapon's mask let go by a hand (thrown, holstered) or by a weapon taken from the
// world (removed there), kept under a key of its own until a hand or the thrown weapon takes it.
struct Loose
{
    int layer{-1};     // its mask (-1: this slot is free)
    double since{0.0}; // cl.time it was let go
    int hand{-1};      // the hand that let it go (-1: an entity removed in the world)
    int ent{0};        // that entity's number
};
constexpr int maxLoose = 6;
entity_t looseKeys[maxLoose]{}; // what each is kept under: its model, where it was
Loose loose[maxLoose];
entity_t* heldEnt[2]{nullptr, nullptr};        // the entity each hand's weapon was drawn with last frame (null: none)
const qmodel_t* heldModel[2]{nullptr, nullptr}; // and its model
double heldSince[2]{-1e9, -1e9};               // cl.time it was taken
struct Drop
{
    int ent{0};  // the weapon thrown (QVR_WOUND_GEAR)
    int hand{0}; // from which hand
    double at{0.0};
};
za::Vector<Drop> drops; // weapons thrown whose entity has not come yet
struct GibSeen
{
    glm::vec3 at{0.f};
    double seen{-1e9};   // cl.time
    double struck{-1e9}; // cl.time it last bloodied you
};
ankerl::unordered_dense::map<int, GibSeen> gibsSeen; // the gibs flying round you (vr_gore_spatter_gibs)
za::Vector<glm::vec3> spatteredNow;                  // this frame's spatters' centres (a blast's pellets: one)

[[nodiscard]] int looseIndex(const entity_t* e)
{
    return e >= looseKeys && e < looseKeys + maxLoose ? static_cast<int>(e - looseKeys) : -1;
}

void resetGear()
{
    for(Loose& l : loose)
    {
        l = Loose{};
    }
    heldEnt[0] = heldEnt[1] = nullptr;
    heldModel[0] = heldModel[1] = nullptr;
    heldSince[0] = heldSince[1] = -1e9;
    drops.clear();
    gibsSeen.clear();
    spatteredNow.clear();
}

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
    return za::clamp(static_cast<int>(vr_wounds_pool.value), 8, 256);
}

// vr_wounds_own_res: 0, or a power of two from 512 to 2048.
[[nodiscard]] int ownRes()
{
    const int want = static_cast<int>(vr_wounds_own_res.value);
    if(want <= 0)
    {
        return 0;
    }
    int res = 512;
    while(res < want && res < 2048)
    {
        res *= 2;
    }
    return res;
}

[[nodiscard]] int maskCount()
{
    return static_cast<int>(masks.size());
}

// Where mask `layer` lives: the pool's array or the fine one, its layer there, its side.
[[nodiscard]] bool isFine(int layer)
{
    return layer >= layers;
}

[[nodiscard]] GLuint textureOf(int layer)
{
    return isFine(layer) ? fineArray : array;
}

[[nodiscard]] int layerIn(int layer)
{
    return isFine(layer) ? layer - layers : layer;
}

[[nodiscard]] int sideOf(int layer)
{
    return isFine(layer) ? fineSize : layerSize;
}

// The last fine layer: your body's right side (-1: none, your body in the pool, one layer, both arms on the same texels).
[[nodiscard]] int twinLayer()
{
    return fineSize > 0 ? layers + ownSlots - 1 : -1;
}

[[nodiscard]] bool isSided(int layer)
{
    const Mask& m = masks[static_cast<za::SizeT>(layer)];
    return isFine(layer) && twinLayer() >= 0 && m.sides[0] + m.sides[1] > 0.f;
}

// The layers mask `layer` is drawn in and the side each takes (-1 all of it): two for your body's, one for the rest.
int layersOf(int layer, int out[2], int side[2])
{
    if(isSided(layer))
    {
        out[0] = layer;
        side[0] = 0;
        out[1] = twinLayer();
        side[1] = 1;
        return 2;
    }
    out[0] = layer;
    side[0] = -1;
    return 1;
}

// The bones of a body's right side (names ending in _r: make_vrbody.py's), as two whole numbers of 24 bits: your body's
// mask is one a side. None for any other model.
void rightBones(const qmodel_t* model, float out[2])
{
    out[0] = out[1] = 0.f;
    if(!model || model->type != mod_alias || ZA_STRNCMP(model->name, "progs/vrbody", 12) != 0)
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_IQM || hdr->numbones <= 0 || !hdr->boneinfo)
    {
        return;
    }
    const auto* bones = reinterpret_cast<const boneinfo_t*>(reinterpret_cast<const byte*>(hdr) + hdr->boneinfo);
    za::U32 bits[2]{0u, 0u};
    for(int i = 0; i < za::min(hdr->numbones, 48); i++)
    {
        const za::SizeT n = strlen(bones[i].name);
        if(n > 2 && bones[i].name[n - 2] == '_' && bones[i].name[n - 1] == 'r')
        {
            bits[i / 24] |= 1u << (i % 24);
        }
    }
    out[0] = static_cast<float>(bits[0]);
    out[1] = static_cast<float>(bits[1]);
}

void attach(GLenum target, int layer)
{
    GL_FramebufferTextureLayerFunc(target, GL_COLOR_ATTACHMENT0, textureOf(layer), 0, layerIn(layer));
}

// A fine mask's other blood (bloodArray: not yours).
void attachBlood(GLenum target, int layer)
{
    GL_FramebufferTextureLayerFunc(target, GL_COLOR_ATTACHMENT0, bloodArray, 0, layerIn(layer));
}

void releaseTexture()
{
    if(array)
    {
        glDeleteTextures(1, &array);
        array = 0;
    }
    if(fineArray)
    {
        glDeleteTextures(1, &fineArray);
        fineArray = 0;
    }
    if(bloodArray)
    {
        glDeleteTextures(1, &bloodArray);
        bloodArray = 0;
    }
    fineSize = 0;
    if(fbo)
    {
        GL_DeleteFramebuffersFunc(1, &fbo);
        fbo = 0;
    }
    layers = 0;
    masks.clear();
    maskOf.clear();
    resetGear();
}

// The texture array and its framebuffer, `poolSize()` layers, and the fine masks (made again, empty, when either changes).
bool ensureTexture()
{
    const int want = poolSize();
    const int wantFine = ownRes();
    if(array && layers == want && fineSize == wantFine)
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
    if(wantFine > 0)
    {
        // Read smoothly (bilinear) on their own finer grid, not the skin's: soft edges, and a relief from their slopes.
        glGenTextures(1, &fineArray);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, fineArray);
        GL_TexStorage3DFunc(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, wantFine, wantFine, ownSlots);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        // The blood on you that isn't yours, one channel: healing takes your wounds' off, not it (water does).
        glGenTextures(1, &bloodArray);
        GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, bloodArray);
        GL_TexStorage3DFunc(GL_TEXTURE_2D_ARRAY, 1, GL_R8, wantFine, wantFine, ownSlots);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    GL_BindNative(GL_TEXTURE0, GL_TEXTURE_2D_ARRAY, 0);
    GL_GenFramebuffersFunc(1, &fbo);
    layers = want;
    fineSize = wantFine;
    masks.clear();
    masks.resize(static_cast<za::SizeT>(want + (wantFine > 0 ? ownSlots : 0)), Mask{});
    maskOf.clear();
    resetGear();
    Con_DPrintf("wounds: %d masks of %dx%d (%.1f MB), yours %dx%d (%.1f MB)\n", want, layerSize, layerSize,
        static_cast<double>(want) * layerSize * layerSize * 4.0 / (1024.0 * 1024.0), wantFine, wantFine,
        static_cast<double>(wantFine > 0 ? ownSlots : 0) * wantFine * wantFine * 5.0 / (1024.0 * 1024.0));
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
    attach(GL_DRAW_FRAMEBUFFER, layer);
    glViewport(0, 0, m.w, m.h);
}

// Takes `amount` (0..1 each: r g b a) off every texel of mask `layer` (all of its layer: 1 clears it; your body's: both
// sides), and its blood off the blood on it that isn't its own (a fine mask's), unless `ownOnly` (healing).
void subtract(int layer, const glm::vec4& amount, bool ownOnly = false)
{
    begin();
    const Mask& m = masks[static_cast<za::SizeT>(layer)];
    if(vr_wounds_debug.value >= 3)
    {
        Con_Printf("wounds: mask %d (%s) less %.2f %.2f %.2f %.2f\n", layer, m.model ? m.model->name : "-", amount.r, amount.g, amount.b, amount.a);
    }
    int in[2], side[2];
    const int n = layersOf(layer, in, side);
    const bool other = !ownOnly && amount.r > 0.f && isFine(layer) && bloodArray;
    for(int k = 0; k < n * (other ? 2 : 1); k++)
    {
        if(k < n)
        {
            attach(GL_DRAW_FRAMEBUFFER, in[k]);
        }
        else
        {
            attachBlood(GL_DRAW_FRAMEBUFFER, in[k - n]);
        }
        glViewport(0, 0, amount == glm::vec4{1.f} ? sideOf(layer) : m.w, amount == glm::vec4{1.f} ? sideOf(layer) : m.h);
        GL_UseProgram(glprogs.viewblend);
        GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
        glBlendFunc(GL_ONE, GL_ONE);
        GL_BlendEquationFunc(GL_FUNC_REVERSE_SUBTRACT);
        GL_Uniform4fvFunc(0, 1, &amount.x);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBlendFunc(GL_ONE, GL_ZERO);
    }
}

void paint(int layer, entity_t* e, const za::Vector<Splat>& splats)
{
    if(splats.empty())
    {
        return;
    }
    begin();
    int in[2], side[2];
    const int count = layersOf(layer, in, side);
    const bool other = foreign && isFine(layer) && bloodArray;
    for(int k = 0; k < count; k++) // (your body's: its left side and middle into its layer, its right side into the last)
    {
        target(in[k], masks[static_cast<za::SizeT>(layer)]);
        if(other) // (blood that isn't yours: its own channel, healing leaves it)
        {
            attachBlood(GL_DRAW_FRAMEBUFFER, in[k]);
        }
        for(za::SizeT i = 0; i < splats.size(); i += maxSplats)
        {
            const int n = static_cast<int>(za::min<za::SizeT>(maxSplats, splats.size() - i));
            GL_BlendEquationFunc(GL_MAX);
            R_PaintAliasWounds(e, n, &splats[i].v[0].x, side[k]);
        }
    }
    GL_BlendEquationFunc(GL_FUNC_ADD);
    masks[static_cast<za::SizeT>(layer)].painted = vr_gametime;
    paintsTotal++;
}

// ----------------------------------------------------------------------------
// The masks.

// Its skin's shape: the region of a layer its mask takes (the skin's size up to 256 on its longer side; a fine mask's:
// its longer side `fine`, finer than the skin). False for a model its mask can't be for (not an alias model, several
// surfaces: several skins over one layout).
[[nodiscard]] bool regionOf(const qmodel_t* model, int& w, int& h, int fine)
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
    const int side = fine > 0 ? fine : layerSize;
    const float k = fine > 0 ? static_cast<float>(fine) / static_cast<float>(za::max(sw, sh))
                             : za::min(1.f, static_cast<float>(layerSize) / static_cast<float>(za::max(sw, sh)));
    w = za::clamp(static_cast<int>(za::lround(sw * k)), 4, side);
    h = za::clamp(static_cast<int>(za::lround(sh * k)), 4, side);
    return true;
}

// The player's body builds share their skin's layout: a mask stays across them.
[[nodiscard]] bool sameLayout(const qmodel_t* a, const qmodel_t* b)
{
    if(a == b)
    {
        return true;
    }
    return a && b && ZA_STRNCMP(a->name, "progs/vrbody", 12) == 0 && ZA_STRNCMP(b->name, "progs/vrbody", 12) == 0;
}

void freeMask(int layer)
{
    Mask& m = masks[static_cast<za::SizeT>(layer)];
    if(m.ent)
    {
        maskOf.erase(m.ent);
        if(const int k = looseIndex(m.ent); k >= 0)
        {
            loose[k].layer = -1;
        }
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
        Mask& m = masks[static_cast<za::SizeT>(it->second)];
        if(sameLayout(m.model, e->model))
        {
            if(m.model != e->model && m.sides[0] + m.sides[1] > 0.f)
            {
                rightBones(e->model, m.sides); // another build: its bones' order may differ
            }
            m.model = e->model;
            return it->second;
        }
        freeMask(it->second); // another model now
    }
    if(!create)
    {
        return -1;
    }
    // Yours in the fine masks (vr_wounds_own_res), the rest in the pool.
    const bool fine = view && fineSize > 0;
    int w = 0, h = 0;
    if(!regionOf(e->model, w, h, fine ? fineSize : 0))
    {
        return -1;
    }

    // A free layer, else the one least worth keeping: drawn longest ago (and farther), never the player's own (in the
    // fine masks: the one drawn longest ago, a body or hand no longer drawn).
    int best = -1;
    float bestScore = -1.f;
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    for(int i = fine ? layers : 0; i < (fine ? twinLayer() : layers); i++) // (not the last fine one: your body's right side)
    {
        const Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.ent)
        {
            best = i;
            break;
        }
        if((m.view && !fine) || m.painted == vr_gametime)
        {
            continue;
        }
        const glm::vec3 at{m.ent->origin[0], m.ent->origin[1], m.ent->origin[2]};
        const float score = static_cast<float>(vr_gametime - m.lastDrawn) + (fine ? 0.f : glm::distance(at, eye) / 300.f);
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
    Mask& m = masks[static_cast<za::SizeT>(best)];
    m.ent = e;
    m.model = e->model;
    m.w = w;
    m.h = h;
    m.view = view;
    m.lastDrawn = vr_gametime;
    if(fine)
    {
        rightBones(e->model, m.sides);
    }
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
        case KindShot: s.r = za::min(1.6f + 0.1f * a, 3.f); break;
        case KindMelee:
            s.r = za::min(2.4f + 0.05f * a, 5.f);
            s.stretch = rnd(1.8f, 2.6f);
            break;
        default: s.r = za::min(2.f + 0.07f * a, 3.8f); break;
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
    za::Vector<glm::vec3> tris; // three corners each
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
    for(za::SizeT i = 0; i + 2 < s.tris.size(); i += 3)
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
        if(za::fabs(t - 0.5f) < za::fabs(bestT - 0.5f))
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
    for(za::SizeT i = 0; i + 2 < s.tris.size(); i += 3)
    {
        const glm::vec3 n = outward(s.tris[i], s.tris[i + 1], s.tris[i + 2]);
        if(glm::dot(n, dir) > -0.2f)
        {
            continue;
        }
        const glm::vec3 c = (s.tris[i] + s.tris[i + 1] + s.tris[i + 2]) / 3.f;
        const glm::vec3 rel = c - org;
        const float along = glm::dot(rel, dir);
        const float off = glm::length(rel - dir * along) + za::fabs(along) * 0.25f;
        if(off < best)
        {
            best = off;
            at = c;
            normal = n;
        }
    }
    return best < 24.f;
}

// A point on a model's surface and the surface's outward normal there.
struct SurfacePoint
{
    glm::vec3 p;
    glm::vec3 n;
};

// Up to `count` points of the model facing `from` (a blast's centre), nearer ones likelier.
void facingPoints(const Surface& s, const glm::vec3& from, int count, za::Vector<SurfacePoint>& out)
{
    const za::SizeT n = s.tris.size() / 3;
    if(!n)
    {
        return;
    }
    for(int tries = 0; tries < count * 12 && static_cast<int>(out.size()) < count; tries++)
    {
        const za::SizeT i = za::min(n - 1, static_cast<za::SizeT>(rnd() * static_cast<float>(n))) * 3;
        const glm::vec3 c = (s.tris[i] + s.tris[i + 1] + s.tris[i + 2]) / 3.f;
        const glm::vec3 nrm = outward(s.tris[i], s.tris[i + 1], s.tris[i + 2]);
        const glm::vec3 to = from - c;
        const float d = glm::length(to);
        if(d < 1e-3f || glm::dot(nrm, to / d) < 0.25f)
        {
            continue;
        }
        out.pushBack({c, nrm});
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

za::Vector<Capsule> playerCapsules(entity_t* const own[3])
{
    za::Vector<Capsule> out;
    const glm::vec3 up{0.f, 0.f, 1.f};
    if(own[0])
    {
        const glm::vec3 pelvis{own[0]->origin[0], own[0]->origin[1], own[0]->origin[2]};
        out.pushBack({pelvis - up * 4.f, pelvis + up * 24.f, 7.5f}); // the torso
        out.pushBack({pelvis - up * 30.f, pelvis - up * 4.f, 6.f});  // the legs
    }
    for(int hand = 0; hand < 2; hand++)
    {
        glm::vec3 wrist, dir;
        if(avatar::forearm(hand, wrist, dir))
        {
            avatar::ForearmFrame f;
            const float len = avatar::forearmFrame(hand, 1.f, f) && f.length > 1.f ? f.length : 10.f;
            out.pushBack({wrist - dir * len, wrist, 2.3f});
            out.pushBack({wrist + dir * 1.f, wrist + dir * 5.5f, 3.2f});
        }
        else if(own[1 + hand])
        {
            const glm::vec3 c{own[1 + hand]->origin[0], own[1 + hand]->origin[1], own[1 + hand]->origin[2]};
            out.pushBack({c, c, 3.5f});
        }
    }
    return out;
}

[[nodiscard]] glm::vec3 closestOnSegment(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 ab = b - a;
    const float l2 = glm::dot(ab, ab);
    const float t = l2 > 1e-6f ? za::clamp(glm::dot(p - a, ab) / l2, 0.f, 1.f) : 0.f;
    return a + ab * t;
}

// The first capsule the line through `org` going `dir` goes into (48 units either side): where, and its normal there.
bool strikeCapsules(const za::Vector<Capsule>& caps, const glm::vec3& org, const glm::vec3& dir, glm::vec3& at, glm::vec3& normal)
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
    const za::Vector<Capsule>* capsules{nullptr}; // the player's: where blows meet it
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
    Mask& m = masks[static_cast<za::SizeT>(layer)];
    za::Vector<Splat> splats;
    const glm::vec3 dir = glm::length(ev.dir) > 0.1f ? glm::normalize(ev.dir) : glm::vec3{0.f, 0.f, -1.f};

    Surface surf;
    const bool mesh = !t.view && surfaceOf(*t.ent, t.num, surf);

    // A blow at a point: where it meets the model (its triangles), else (the player's jointed body and hands) as it
    // goes, over what faces it within its radius of its line.
    // The player's own are seen close: smaller wounds; the hands' smaller still (a hand is a few units across).
    const float own = !t.view ? 1.f : ZA_STRNCMP(t.ent->model->name, "progs/hand", 10) == 0 ? 0.65f : 0.75f;
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
            splats.pushBack(woundSplat(at, r, n, way, ws.stretch, r * 0.9f + 1.f, -0.25f, ws.run, what));
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
            splats.pushBack(woundSplat(at, r, n, way, ws.stretch, r * 0.9f + 6.f, 0.f, ws.run, what));
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
            const int pellets = za::max(1, ev.extra);
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
            const float r = ((zap ? 3.f : 4.f) + za::min(static_cast<float>(ev.amount), 60.f) * 0.08f) * own;
            const glm::vec3 org = spread(ev.org, dir);
            glm::vec3 at, n;
            if(mesh ? !strike(surf, org, dir, at, n) : !(t.capsules && strikeCapsules(*t.capsules, org + t.shift, dir, at, n)))
            {
                break;
            }
            splats.pushBack(burnSplat(at, r, r * 0.25f, n, false, 0.1f, glm::vec4{0.f, zap ? 0.8f : 0.9f, 0.f, zap ? 0.6f : 0.9f}));
            if(!zap && ev.extra != 1) // a small wound under it (extra 1: a fire's flame, a burn alone: QC vr_burning.qc)
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
                    nearest = za::min(nearest, glm::distance(centre, p));
                }
            }
            else
            {
                nearest = za::max(0.f, glm::distance(centre, glm::vec3{t.ent->origin[0], t.ent->origin[1], t.ent->origin[2]}) - 10.f);
            }
            const float a = za::min(static_cast<float>(ev.amount), 120.f);
            const float char_ = za::clamp(a / 120.f, 0.3f, 0.75f);
            splats.pushBack(burnSplat(centre, nearest + 14.f + a * 0.12f, nearest + 3.f + a * 0.05f, glm::vec3{0.f}, true, 0.05f,
                glm::vec4{0.f, char_, 0.f, char_ * 0.8f}));
            m.hotLeft = coolTime;
            const int bleeds = za::clamp(ev.amount / 14, 1, 7);
            if(mesh)
            {
                za::Vector<SurfacePoint> pts;
                facingPoints(surf, centre, bleeds, pts);
                for(const auto& [p, n] : pts)
                {
                    const WoundSize ws = woundSize(KindNail, ev.amount / 3);
                    splats.pushBack(woundSplat(p, ws.r, n, glm::normalize(p - centre), 1.f, ws.r + 1.f, -0.25f, ws.run, paintBlood));
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
            splats.pushBack(liquidSplat(ev.org.z, 14.f, 0.4f, glm::vec4{0.f, 0.9f, 0.f, 0.9f}));
            m.hotLeft = coolTime;
            break;
        case KindSlime: splats.pushBack(liquidSplat(ev.org.z, 20.f, 0.5f, glm::vec4{0.f, 0.6f, 0.f, 0.f})); break;
        case KindLiquid:
            if(ev.extra == 2) // lava: nothing wet (its burns come with its damage)
            {
                break;
            }
            splats.pushBack(liquidSplat(ev.org.z, 30.f, 0.f, glm::vec4{0.f, 0.f, 1.f, 0.f}));
            if(vr_wounds_wet.value)
            {
                m.wetLeft = dryTime;
                onLiquid(m, ev);
            }
            break;
        default: break;
    }

    if(bloodOnly)
    {
        for(Splat& s : splats)
        {
            s.v[2] = glm::vec4{s.v[2].r, 0.f, 0.f, 0.f};
        }
    }
    splats.erase(za::removeIf(splats.begin(), splats.end(), [](const Splat& s) { return !paintsAnything(s); }), splats.end());
    paint(layer, t.ent, splats);
}

void logPlayerWound(const Event& ev); // (below: the player's wounds, to re-open after a wash)
void loosen(int layer, int hand, int ent, const glm::vec3& at); // (below: blood on you and your gear)
void spatterFrom(const Event& ev, int saw);
void hurtSpread(const Event& ev);
void armMarks(int hand, int count, float size, float legs, bool lower);

void apply(const Event& ev)
{
    if(ev.kind == KindClear)
    {
        // Removed on the server: its mask freed (a new entity in the slot, with the same model, starts clean).
        if(ev.num > 0 && ev.num < cl_max_edicts)
        {
            if(const auto it = maskOf.find(&cl_entities[ev.num]); it != maskOf.end() && !masks[static_cast<za::SizeT>(it->second)].view)
            {
                const bool gear = masks[static_cast<za::SizeT>(it->second)].gear;
                if(vr_wounds_debug.value)
                {
                    Con_Printf("wounds: entity %d removed, its mask %s\n", ev.num, gear ? "kept for a hand taking it" : "freed");
                }
                if(gear) // a bloody weapon taken into a hand: its blood goes with it (gearFrame)
                {
                    const entity_t& e = cl_entities[ev.num];
                    loosen(it->second, -1, ev.num, glm::vec3{e.origin[0], e.origin[1], e.origin[2]});
                }
                else
                {
                    freeMask(it->second);
                }
            }
        }
        return;
    }
    if(ev.kind == KindGear) // a weapon thrown from a hand: its blood onto it once it is here (gearFrame)
    {
        if(ev.num > 0 && ev.num < cl_max_edicts && drops.size() < 16)
        {
            drops.pushBack({ev.num, ev.extra ? 1 : 0, cl.time});
        }
        return;
    }
    if(vr_wounds_debug.value)
    {
        Con_Printf("wounds: event kind %d on entity %d%s, amount %d, extra %d, at %.1f %.1f %.1f going %.2f %.2f %.2f\n", ev.kind,
            ev.num, ev.num == cl.viewentity ? " (you)" : "", ev.amount, ev.extra, ev.org.x, ev.org.y, ev.org.z, ev.dir.x, ev.dir.y, ev.dir.z);
    }
    za::Vector<Capsule> caps;
    Target targets[3];
    int count = 0;
    if(ev.num == cl.viewentity)
    {
        logPlayerWound(ev);
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
    const za::U32 seed = rng;
    for(int i = 0; i < count; i++)
    {
        rng = seed;
        wound(targets[i], ev);
    }
    static_cast<void>(rnd());
    if(ev.num == cl.viewentity)
    {
        hurtSpread(ev); // blood running over your arms too
    }
    else
    {
        spatterFrom(ev, -1); // its blood thrown onto you, if you are near
    }
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
    za::Vector<Splat> splats;
    const float lowZ = surf.lo.z + (surf.hi.z - surf.lo.z) * 0.3f;
    for(int tries = 0; tries < 60 && splats.size() < 6; tries++)
    {
        const za::SizeT n = surf.tris.size() / 3;
        const za::SizeT i = za::min(n - 1, static_cast<za::SizeT>(rnd() * static_cast<float>(n))) * 3;
        const glm::vec3 c = (surf.tris[i] + surf.tris[i + 1] + surf.tris[i + 2]) / 3.f;
        if(splats.size() < 3 && c.z > lowZ)
        {
            continue; // the neck first
        }
        const glm::vec3 nrm = outward(surf.tris[i], surf.tris[i + 1], surf.tris[i + 2]);
        const WoundSize ws = woundSize(KindNail, 20);
        splats.pushBack(woundSplat(c, ws.r * 1.2f, nrm, -nrm, 1.f, ws.r + 1.f, -0.25f, ws.run, paintBlood));
    }
    paint(layer, &e, splats);
}

// ----------------------------------------------------------------------------
// Gore: bloody hands, washing, wounds re-opening (vr_gore_hands, vr_gore_wash*, vr_gore_reopen*; ROUND21.md, "Gore:
// bloody hands, washing, dying bodies, blood mist").
//
// A hand that takes a gib or a head (what it carries: STAT_QVR_CARRYMAIN/OFF, a gib's or a head's model) is smeared
// with its blood: patches over the hand (blood in its mask, as its wounds), more the longer it holds it. Water washes
// the blood (wounds' and gibs') off the body and the hands where they are under it, in vr_gore_wash_time. Hurt (wounds
// taken and not healed), the wounds re-open vr_gore_reopen_delay seconds after the last wash, one after another over
// vr_gore_reopen_time: the player's wounds are kept (where they were struck, from the player's origin) until healing
// takes them off, and painted again, their blood only, and their blood runs onto both hands (a few patches). A gib's
// blood washed off does not come back. With the wound skins (vr_wounds 0) instead: the hands' skin
// is a bloody one while a gib's blood is on it, and a part washed shows no wounds until they re-open (skinLevel).

struct LoggedWound
{
    Event ev;
    glm::vec3 rel{0.f}; // its point from the player's origin
    za::U32 seed{0};    // the draws' state it was painted with (re-opened: the same draws, the same wounds where it was)
};

constexpr za::SizeT maxLogged = 48;
constexpr float gibBlotEvery = 1.f; // seconds a held gib smears its hand again
constexpr int gibBlotsMore = 4;     // and how many more times at most, a hold
constexpr float reopenHandBlood = 1.f; // the hands' blood (as vr_gore_hands) when the wounds re-open

za::Vector<LoggedWound> playerWounds; // the player's wounds not healed yet, oldest first
int heldGib[2]{0, 0};                 // the gib each hand held at the last look (0: none)
double heldGibNext[2]{0.0, 0.0};      // when it smears the hand again
int heldGibBlots[2]{0, 0};            // the times it did, this hold
double lastWash = -1e9;               // cl.time the player was last being washed
bool reopenPending = false;           // washed while hurt: the wounds re-open
double reopenStart = -1.0;            // cl.time they began to (-1: not yet)
za::SizeT reopened = 0;               // how many of them have

// The wound skins (vr_wounds 0): per part (0 the off hand, 1 the main hand, 2 the body).
struct SkinPart
{
    double seen{-1e9};   // cl.time of the last look
    double washed{-1e9}; // cl.time it was last under water (-1e9: not washed)
    int washedLevel{0};  // the damage skin it showed then
    int gib{0};          // a gib's blood on it: the skin it shows at least
    int gibEnt{0};       // the gib it held at the last look
};

SkinPart skinParts[3];

[[nodiscard]] bool isBlood(int kind)
{
    return kind == KindShot || kind == KindNail || kind == KindMelee || kind == KindBlast || kind == KindBurn;
}

// Entity `num` (client side) is a gib or a head: by its model (progs/gib*, progs/h_*, and the mission packs' and the
// zombies' gibs).
[[nodiscard]] bool isGib(int num)
{
    if(num <= 0 || num >= cl.num_entities || !cl_entities[num].model)
    {
        return false;
    }
    const char* name = cl_entities[num].model->name;
    return ZA_STRNCMP(name, "progs/h_", 8) == 0 || strstr(name, "gib") != nullptr;
}

[[nodiscard]] bool inWater(const glm::vec3& p)
{
    if(!cl.worldmodel)
    {
        return false;
    }
    vec3_t v{p.x, p.y, p.z};
    return Mod_PointInLeaf(v, cl.worldmodel)->contents == CONTENTS_WATER;
}

// The water's surface over `p`, looked for `reach` units under and over it: false if none of that is under water. Under
// water all the way up: a surface well over it.
bool waterSurface(const glm::vec3& p, float reach, float& surface)
{
    constexpr float step = 3.f;
    float top = 0.f;
    bool any = false;
    for(float z = -reach; z <= reach; z += step)
    {
        if(inWater(p + glm::vec3{0.f, 0.f, z}))
        {
            any = true;
            top = z;
        }
    }
    if(!any)
    {
        return false;
    }
    float lo = top, hi = top + step;
    if(inWater(p + glm::vec3{0.f, 0.f, hi}))
    {
        surface = p.z + hi + 64.f;
        return true;
    }
    for(int i = 0; i < 4; i++)
    {
        const float mid = (lo + hi) * 0.5f;
        (inWater(p + glm::vec3{0.f, 0.f, mid}) ? lo : hi) = mid;
    }
    surface = p.z + (lo + hi) * 0.5f;
    return true;
}

// `amount` of the blood taken off mask `layer` (entity `e`, as drawn) under `surface`.
void washUnder(int layer, entity_t* e, float surface, float amount)
{
    const Splat s = liquidSplat(surface, 1.f, 0.f, glm::vec4{amount, 0.f, 0.f, 0.f});
    if(!paintsAnything(s))
    {
        return;
    }
    begin();
    int in[2], side[2];
    const int n = layersOf(layer, in, side);
    const bool other = isFine(layer) && bloodArray;
    for(int k = 0; k < n * (other ? 2 : 1); k++) // (a fine mask's: the blood on it that isn't its own too)
    {
        target(in[k % n], masks[static_cast<za::SizeT>(layer)]);
        if(k >= n)
        {
            attachBlood(GL_DRAW_FRAMEBUFFER, in[k - n]);
        }
        // (the state's blending set first: R_PaintAliasWounds sets the same, leaving the function as it is)
        GL_SetState(GLS_BLEND_OPAQUE | GLS_NO_ZTEST | GLS_NO_ZWRITE | GLS_CULL_NONE | GLS_ATTRIBS(0));
        glBlendFunc(GL_ONE, GL_ONE);
        GL_BlendEquationFunc(GL_FUNC_REVERSE_SUBTRACT);
        R_PaintAliasWounds(e, 1, &s.v[0].x, side[k % n]);
        GL_BlendEquationFunc(GL_FUNC_ADD);
        glBlendFunc(GL_ONE, GL_ZERO);
    }
}

// A gib's blood smeared over `hand` (0 off, 1 main): `blots` ragged patches of blood over all of it (the liquids' paint,
// in patches: where its noise is over a threshold), `amount` (vr_gore_hands) how much of the hand each covers.
void smearHand(int hand, int blots, float amount, bool theirs = true)
{
    entity_t* own[3]{};
    view::woundTargets(own);
    entity_t* e = own[1 + hand];
    if(!e || amount <= 0.f || blots <= 0)
    {
        return;
    }
    const int layer = acquire(e, true, true);
    if(layer < 0)
    {
        return;
    }
    za::Vector<Splat> splats;
    const float k = za::clamp(amount, 0.f, 3.f);
    for(int i = 0; i < blots; i++)
    {
        // a patch threshold of the noise (0..1, about 0.5 on average): 0.6 covers about a fifth, 0.5 half
        const float patch = za::clamp(0.68f - 0.06f * k + rnd(-0.03f, 0.03f), 0.4f, 0.9f);
        splats.pushBack(liquidSplat(e->origin[2] + 64.f, 1.f, patch, glm::vec4{rnd(0.85f, 1.f), 0.f, 0.f, 0.f}));
    }
    splats.erase(za::removeIf(splats.begin(), splats.end(), [](const Splat& s) { return !paintsAnything(s); }), splats.end());
    if(vr_wounds_debug.value)
    {
        Con_Printf("wounds: blood smeared on the %s hand, %d patches\n", hand ? "main" : "off", static_cast<int>(splats.size()));
    }
    foreign = theirs; // (a gib's, a blow's: not yours, healing leaves it; your wounds re-opening: yours)
    paint(layer, e, splats);
    foreign = false;
}

// Each frame: a gib taken into a hand smears it, and again now and then while it is held.
void gibHands(double now)
{
    for(int hand = 0; hand < 2; hand++)
    {
        const int ent = cl.stats[hand == 1 ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
        const bool gib = vr_gore_hands.value > 0.f && cl.stats[STAT_HEALTH] > 0 && isGib(ent);
        if(!gib)
        {
            heldGib[hand] = 0;
            continue;
        }
        if(heldGib[hand] != ent)
        {
            heldGib[hand] = ent;
            heldGibBlots[hand] = 0;
            heldGibNext[hand] = now + gibBlotEvery;
            smearHand(hand, za::max(1, static_cast<int>(vr_gore_hands.value * 4.f + 0.5f)), vr_gore_hands.value);
            armMarks(hand, static_cast<int>(vr_gore_spread.value * 2.f + 0.5f), 0.8f, 0.f, true); // and up its wrist
        }
        else if(now >= heldGibNext[hand] && heldGibBlots[hand] < gibBlotsMore)
        {
            heldGibBlots[hand]++;
            heldGibNext[hand] = now + gibBlotEvery;
            smearHand(hand, 1, vr_gore_hands.value);
        }
    }
}

void logPlayerWound(const Event& ev)
{
    if(bloodOnly || !isBlood(ev.kind))
    {
        return;
    }
    const entity_t& pl = cl_entities[cl.viewentity];
    if(playerWounds.size() >= maxLogged)
    {
        playerWounds.erase(playerWounds.begin());
    }
    playerWounds.pushBack({ev, ev.org - glm::vec3{pl.origin[0], pl.origin[1], pl.origin[2]}, rng});
}

// Healing takes a share of the wounds off (`share` of them, the oldest first): they won't re-open.
void forgetPlayerWounds(float share)
{
    const auto n = static_cast<za::SizeT>(za::ceil(share * static_cast<float>(playerWounds.size()) - 1e-4f));
    if(n >= playerWounds.size())
    {
        playerWounds.clear();
    }
    else if(n > 0)
    {
        playerWounds.erase(playerWounds.begin(), playerWounds.begin() + n);
    }
    reopened = za::min(reopened, playerWounds.size());
}

// Every tick (`n` steps of it): the player's body and hands washed where they are under water.
void washGear(float amount); // (below: blood on you and your gear)

void wash(int n)
{
    if(!vr_gore_wash.value || !vr_wounds.value || cl.stats[STAT_HEALTH] <= 0)
    {
        return;
    }
    entity_t* own[3]{};
    view::woundTargets(own);
    bool washing = false;
    const float amount = static_cast<float>(n) * tick / za::max(0.05f, vr_gore_wash_time.value);
    washGear(amount); // your weapons and props, held or lying about
    for(int k = 0; k < 3; k++)
    {
        if(!own[k])
        {
            continue;
        }
        const glm::vec3 at{own[k]->origin[0], own[k]->origin[1], own[k]->origin[2]};
        float surface;
        if(!waterSurface(at, k == 0 ? 40.f : 9.f, surface))
        {
            continue;
        }
        washing = true;
        const int layer = acquire(own[k], true, false);
        if(layer >= 0)
        {
            washUnder(layer, own[k], surface, amount);
        }
    }
    if(!washing)
    {
        return;
    }
    if(vr_wounds_debug.value && cl.time - lastWash > 1.0)
    {
        Con_Printf("wounds: washing (%d wounds to re-open)\n", static_cast<int>(playerWounds.size()));
    }
    lastWash = cl.time;
    reopenStart = -1.0;
    reopened = 0;
    if(!vr_gore_reopen.value)
    {
        playerWounds.clear(); // washed for good
    }
    reopenPending = !playerWounds.empty();
}

// Each frame: washed while hurt, the wounds re-open (their blood only) once clean long enough, one after another.
void reopen(double now)
{
    if(!reopenPending || !vr_gore_reopen.value || now - lastWash < vr_gore_reopen_delay.value)
    {
        return;
    }
    if(playerWounds.empty() || cl.stats[STAT_HEALTH] <= 0)
    {
        reopenPending = false;
        return;
    }
    if(reopenStart < 0.0)
    {
        reopenStart = now;
        reopened = 0;
        if(vr_wounds_debug.value)
        {
            Con_Printf("wounds: %d wounds re-open\n", static_cast<int>(playerWounds.size()));
        }
        // Their blood runs down the arms onto the hands (wherever the wounds land again).
        const int blots = za::clamp(static_cast<int>(playerWounds.size() / 2), 2, 4);
        smearHand(0, blots, reopenHandBlood, false);
        smearHand(1, blots, reopenHandBlood, false);
        armMarks(-1, static_cast<int>(static_cast<float>(blots) * vr_gore_spread.value + 0.5f), 1.f, 0.25f, false); // and over the arms
    }
    const float spread = vr_gore_reopen_time.value;
    const za::SizeT total = playerWounds.size();
    const float done = spread <= 0.f ? 1.f : za::min(1.f, static_cast<float>(now - reopenStart) / spread);
    const za::SizeT want = za::min(total, static_cast<za::SizeT>(za::ceil(done * static_cast<float>(total))));
    const entity_t& pl = cl_entities[cl.viewentity];
    const glm::vec3 origin{pl.origin[0], pl.origin[1], pl.origin[2]};
    const za::U32 draws = rng; // (the replays' draws are theirs: the sequence goes on after them as it was)
    bloodOnly = true;
    for(; reopened < want; reopened++)
    {
        Event ev = playerWounds[reopened].ev;
        rng = playerWounds[reopened].seed;
        ev.org = origin + playerWounds[reopened].rel;
        ev.num = cl.viewentity;
        apply(ev);
    }
    bloodOnly = false;
    rng = draws;
    if(reopened >= total)
    {
        reopenPending = false;
        reopenStart = -1.0;
    }
}

// The view masks' blood (off hand, main hand, body): texels with any (-1: no mask), and their blood summed (1 a texel
// full).
void viewBlood(int count[3], double sum[3])
{
    entity_t* own[3]{};
    view::woundTargets(own);
    const za::SizeT side = static_cast<za::SizeT>(za::max(layerSize, fineSize));
    za::Vector<byte> rgba(side * side * 4);
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    for(int part = 0; part < 3; part++)
    {
        entity_t* e = own[part < 2 ? 1 + part : 0];
        count[part] = -1;
        sum[part] = 0.0;
        const int layer = e && array ? acquire(e, true, false) : -1;
        if(layer < 0)
        {
            continue;
        }
        const Mask& m = masks[static_cast<za::SizeT>(layer)];
        count[part] = 0;
        int in[2], side[2];
        const int n = layersOf(layer, in, side);
        za::Vector<byte> other(static_cast<za::SizeT>(m.w) * static_cast<za::SizeT>(m.h), 0);
        for(int k = 0; k < n; k++) // (your body's: both sides; the blood on it not yours too)
        {
            if(isFine(layer) && bloodArray)
            {
                attachBlood(GL_READ_FRAMEBUFFER, in[k]);
                glPixelStorei(GL_PACK_ALIGNMENT, 1); // (rows of any width)
                glReadPixels(0, 0, m.w, m.h, GL_RED, GL_UNSIGNED_BYTE, other.data());
                glPixelStorei(GL_PACK_ALIGNMENT, 4);
            }
            attach(GL_READ_FRAMEBUFFER, in[k]);
            glReadPixels(0, 0, m.w, m.h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            for(int p = 0; p < m.w * m.h; p++)
            {
                const int r = za::max<int>(rgba[static_cast<za::SizeT>(p) * 4], other[static_cast<za::SizeT>(p)]);
                count[part] += r > 0 ? 1 : 0;
                sum[part] += r / 255.0;
            }
        }
    }
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
}


// ----------------------------------------------------------------------------
// Blood on you and your gear (vr_gore_spatter*, vr_gore_gear, vr_gore_spread; ROUND21.md, "Blood on you, your weapons
// and props").
//
// A hit that bleeds near you (a monster's or a corpse's wound) throws its blood onto what of you faces it: your body,
// your hands and what they hold (a weapon, a prop), as drops (a spatter: all of it covered right at the hit, fewer
// farther out): your blows near your hands some (vr_gore_spatter_melee), a chainsaw's cuts a lot (vr_gore_spatter_saw),
// shots that hit close to you a few (vr_gore_spatter_shots, within vr_gore_spatter_range). A gib flying into you
// bloodies you where it strikes. A weapon's or a prop's blood stays on it: a prop is the world's entity (its mask its
// own); a weapon in a hand is the hand's drawn weapon, whose mask goes with it: thrown, to the weapon lying in the world
// (QC's DropWeaponInHand sends QVR_WOUND_GEAR), taken again (removed in the world), back to a hand, holstered, kept for
// the weapon drawn again (Loose). Water washes it all off. Hurt, holding a gib, or as your wounds re-open, blood runs
// over your arms (and a little your legs) too: bleeding marks along them (vr_gore_spread).

[[nodiscard]] glm::vec3 originOf(const entity_t& e)
{
    return glm::vec3{e.origin[0], e.origin[1], e.origin[2]};
}

// Half a model's bounds' diagonal: about how far it reaches from its origin.
[[nodiscard]] float modelRadius(const qmodel_t* m)
{
    return m ? 0.5f * glm::length(glm::vec3{m->maxs[0] - m->mins[0], m->maxs[1] - m->mins[1], m->maxs[2] - m->mins[2]}) : 0.f;
}

// What `hand` holds that takes blood: its weapon (the drawn one), else a prop it carries (not a gib: that bleeds its
// own). Null: nothing.
[[nodiscard]] entity_t* gearOf(int hand)
{
    if(const view::ViewEntity* ve = view::heldWeapon(hand))
    {
        return const_cast<entity_t*>(&ve->ent);
    }
    const int n = cl.stats[hand == 1 ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
    if(n > 0 && n < cl.num_entities && n != cl.viewentity && cl_entities[n].model && cl_entities[n].model->type == mod_alias && !isGib(n))
    {
        return &cl_entities[n];
    }
    return nullptr;
}

// Mask `layer` let go: kept (Loose) until a hand or a weapon thrown takes it; the oldest kept gives its up.
void loosen(int layer, int hand, int ent, const glm::vec3& at)
{
    int slot = 0;
    for(int i = 0; i < maxLoose; i++)
    {
        if(loose[i].layer < 0)
        {
            slot = i;
            break;
        }
        if(loose[i].since < loose[slot].since)
        {
            slot = i;
        }
    }
    if(loose[slot].layer >= 0 && loose[slot].layer != layer)
    {
        freeMask(loose[slot].layer);
    }
    Mask& m = masks[static_cast<za::SizeT>(layer)];
    if(m.ent)
    {
        maskOf.erase(m.ent);
    }
    entity_t& key = looseKeys[slot];
    key.model = const_cast<qmodel_t*>(m.model);
    key.origin[0] = at.x;
    key.origin[1] = at.y;
    key.origin[2] = at.z;
    m.ent = &key;
    maskOf[&key] = layer;
    loose[slot] = Loose{layer, cl.time, hand, ent};
    if(vr_wounds_debug.value)
    {
        Con_Printf("wounds: %s's blood kept (%s %d)\n", m.model ? m.model->name : "?", hand >= 0 ? "let go by hand" : "removed entity",
            hand >= 0 ? hand : ent);
    }
}

// Kept mask `slot` onto `to` (a hand's weapon, a weapon thrown).
void claim(int slot, entity_t* to)
{
    const int layer = loose[slot].layer;
    if(const auto it = maskOf.find(to); it != maskOf.end() && it->second != layer)
    {
        freeMask(it->second);
    }
    Mask& m = masks[static_cast<za::SizeT>(layer)];
    maskOf.erase(m.ent);
    m.ent = to;
    m.model = to->model;
    m.view = false;
    m.gear = true;
    m.lastDrawn = vr_gametime;
    m.lastAt = originOf(*to);
    maskOf[to] = layer;
    loose[slot].layer = -1;
    if(vr_wounds_debug.value)
    {
        Con_Printf("wounds: %s's blood taken by %s\n", to->model->name,
            to >= cl_entities && to < cl_entities + cl_max_edicts ? "the weapon thrown" : "a hand");
    }
}

// The kept mask for `model` likeliest to be this one's: the one from entity `ent` (removed: taken by a hand), else
// from `hand`, else the latest; none older than `maxAge` seconds (0: any). -1: none.
[[nodiscard]] int findLoose(const qmodel_t* model, int hand, int ent, double maxAge)
{
    int best = -1;
    double bestScore = -1e30;
    for(int i = 0; i < maxLoose; i++)
    {
        const Loose& l = loose[i];
        if(l.layer < 0 || !sameLayout(masks[static_cast<za::SizeT>(l.layer)].model, model) || (maxAge > 0.0 && cl.time - l.since > maxAge))
        {
            continue;
        }
        const double score = l.since + (ent > 0 && l.ent == ent ? 1e6 : 0.0) + (hand >= 0 && l.hand == hand ? 1e5 : 0.0);
        if(score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

// Each frame: the hands' weapons let go and taken, the weapons thrown, what was removed in the world and not taken.
void gearFrame(double now)
{
    for(int hand = 0; hand < 2; hand++)
    {
        const view::ViewEntity* ve = view::heldWeapon(hand);
        entity_t* e = ve ? const_cast<entity_t*>(&ve->ent) : nullptr;
        const qmodel_t* model = e ? e->model : nullptr;
        if(heldEnt[hand] && heldModel[hand] && model != heldModel[hand])
        {
            // Let go (thrown, holstered, another drawn): its blood kept for it.
            if(const auto it = maskOf.find(heldEnt[hand]); it != maskOf.end() && masks[static_cast<za::SizeT>(it->second)].gear)
            {
                loosen(it->second, hand, 0, originOf(*heldEnt[hand]));
            }
        }
        if(model && model != heldModel[hand])
        {
            heldSince[hand] = now;
        }
        // Taken (from the world, from a holster): its blood back, if it had any (removed in the world a moment ago,
        // the removal maybe coming after).
        if(model && vr_gore_gear.value && maskOf.find(e) == maskOf.end())
        {
            const int i = findLoose(model, hand, -1, 0.0);
            if(i >= 0 && (model != heldModel[hand] || (now - heldSince[hand] < 1.5 && loose[i].hand < 0)))
            {
                claim(i, e);
            }
        }
        heldEnt[hand] = e;
        heldModel[hand] = model;
    }
    // Thrown: onto the weapon lying in the world once it is here.
    for(za::SizeT i = 0; i < drops.size();)
    {
        const Drop d = drops[i];
        entity_t& w = cl_entities[d.ent];
        bool done = now - d.at > 1.5;
        if(!done && d.ent < cl.num_entities && w.model && w.msgtime >= cl.mtime[0] - 0.001)
        {
            const int k = findLoose(w.model, d.hand, -1, 3.0);
            if(k >= 0 && maskOf.find(&w) == maskOf.end())
            {
                claim(k, &w);
            }
            done = true;
        }
        if(done)
        {
            drops.erase(drops.begin() + i);
        }
        else
        {
            i++;
        }
    }
    // Removed in the world and not taken by a hand: gone.
    for(const Loose& l : loose)
    {
        if(l.layer >= 0 && l.hand < 0 && now - l.since > 3.0)
        {
            freeMask(l.layer);
        }
    }
    // Gear lying in the world gone far at once: another entity in its slot now.
    for(int i = 0; i < maskCount(); i++)
    {
        Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.gear || !m.ent || m.ent < cl_entities || m.ent >= cl_entities + cl_max_edicts || !m.ent->model)
        {
            continue;
        }
        const glm::vec3 at = originOf(*m.ent);
        if(glm::distance(at, m.lastAt) > 256.f)
        {
            freeMask(i);
            continue;
        }
        m.lastAt = at;
    }
}

// `splats` painted on what of you is within `reach` of `at` (your body, your hands) and, with `gear`, what they hold.
void paintOnYou(const za::Vector<Splat>& splats, const glm::vec3& at, float reach, bool gear)
{
    if(splats.empty())
    {
        return;
    }
    entity_t* own[3]{};
    view::woundTargets(own);
    for(int k = 0; k < 3; k++)
    {
        if(vr_wounds_debug.value >= 2)
        {
            Con_Printf("wounds: onto you: %s %s, %.0f units (reach %.0f)\n", k == 0 ? "body" : k == 1 ? "off hand" : "main hand",
                own[k] ? "drawn" : "not drawn", own[k] ? static_cast<double>(glm::distance(originOf(*own[k]), at)) : 0.0,
                static_cast<double>(reach));
        }
        if(!own[k] || glm::distance(originOf(*own[k]), at) > reach + (k == 0 ? 64.f : 10.f))
        {
            continue;
        }
        const int layer = acquire(own[k], true, true);
        if(layer >= 0)
        {
            foreign = gear; // (a spatter's, a gib's: not your blood; your arms' marks are)
            paint(layer, own[k], splats);
            foreign = false;
        }
    }
    if(!gear || !vr_gore_gear.value)
    {
        return;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        entity_t* g = gearOf(hand);
        if(!g || glm::distance(originOf(*g), at) > reach + za::max(modelRadius(g->model), 8.f))
        {
            continue;
        }
        const int layer = acquire(g, false, true);
        if(layer < 0)
        {
            continue;
        }
        Mask& m = masks[static_cast<za::SizeT>(layer)];
        m.gear = true;
        m.lastAt = originOf(*g);
        paint(layer, g, splats);
    }
}

// Drops of blood thrown from `c` out to `radius`: all covered within `core`, `share` of the surface covered `ref` units
// out (falling off with the square of the distance), in cells `drop` units across (the drops about a third of that).
[[nodiscard]] Splat spatterSplat(const glm::vec3& c, float radius, float core, float ref, float share, float drop)
{
    Splat s;
    const float q = za::max(ref, core) / za::max(core, 0.1f);
    s.v[0] = glm::vec4{c, radius};
    s.v[1] = glm::vec4{0.f, 0.f, 0.f, 4.f};
    s.v[2] = allowed(paintBlood);
    s.v[3] = glm::vec4{0.f, 0.f, 0.f, rnd(0.f, 97.f)};
    s.v[4] = glm::vec4{core, -0.05f, 1.f / za::max(drop, 0.1f), share * q * q};
    return s;
}

// The nearest of your hands to `at` (its fist or what it holds), and how near: -1 none drawn.
int nearestHand(const glm::vec3& at, float& dist)
{
    entity_t* own[3]{};
    view::woundTargets(own);
    int best = -1;
    dist = 1e30f;
    for(int hand = 0; hand < 2; hand++)
    {
        float d = own[1 + hand] ? glm::distance(originOf(*own[1 + hand]), at) : 1e30f;
        if(const entity_t* g = gearOf(hand))
        {
            d = za::min(d, za::max(0.f, glm::distance(originOf(*g), at) - modelRadius(g->model) * 0.5f));
        }
        if(d < dist)
        {
            dist = d;
            best = hand;
        }
    }
    return best;
}

[[nodiscard]] bool holdsSaw(int hand)
{
    const view::ViewEntity* ve = view::heldWeapon(hand);
    return ve && ve->ent.model && ZA_STRCMP(ve->ent.model->name, "progs/v_chainsaw.mdl") == 0;
}

// A wound on a monster or a corpse (or a test's: `saw` 0 or 1 forces a blow's kind, -1 by what the hand holds): its
// blood thrown onto you, if near.
void spatterFrom(const Event& ev, int saw)
{
    if(!vr_gore_spatter.value || !vr_wounds.value || bloodOnly || cl.stats[STAT_HEALTH] <= 0 ||
        (ev.kind != KindShot && ev.kind != KindNail && ev.kind != KindMelee))
    {
        return;
    }
    const glm::vec3 at = ev.org;
    for(const glm::vec3& p : spatteredNow)
    {
        if(glm::distance(p, at) < 6.f)
        {
            return; // (a blast's pellets, a cut's several hits: one spatter a frame)
        }
    }
    float dHand;
    const int hand = nearestHand(at, dHand);
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const float dEye = glm::distance(eye, at);
    const float hit = za::clamp(static_cast<float>(ev.amount) / 20.f, 0.5f, 2.f);
    za::Vector<Splat> splats;
    float reach;
    if(ev.kind == KindMelee && hand >= 0 && dHand < 40.f)
    {
        // A blow by your hand, or what it holds: the blade or fist in it bloodied, drops round it; a chainsaw's spray
        // over the hands and arms.
        // (A chainsaw's cuts are many small ones, a few a second: their spray by the cut, not by its damage.)
        const bool cut = saw >= 0 ? saw == 1 : holdsSaw(hand);
        const float k = vr_gore_spatter.value * (cut ? vr_gore_spatter_saw.value : vr_gore_spatter_melee.value * hit);
        if(k <= 0.f)
        {
            return;
        }
        // Thrown out to past the hand (a blade's tip may be far from it), what it reaches there by the blow's kind.
        entity_t* own[3]{};
        view::woundTargets(own);
        const float fist = own[1 + hand] ? glm::distance(originOf(*own[1 + hand]), at) : dHand;
        reach = za::max(fist + (cut ? 24.f : 12.f), cut ? 40.f : 24.f);
        splats.pushBack(spatterSplat(at, reach, cut ? 3.f : 1.5f, za::max(fist, 6.f), za::min((cut ? 0.06f : 0.03f) * k, 0.9f), cut ? 1.2f : 1.f));
        paintOnYou(splats, at, reach, true);
        // The hands too: blood running down the blade, the fist in it (a blow's some; the saw's spray, the other hand
        // on its handle too, a lot over a few seconds' cutting).
        for(int h = 0; h < 2; h++)
        {
            const float d = own[1 + h] ? glm::distance(originOf(*own[1 + h]), at) : 1e30f;
            if(h != hand && !(cut && d < 32.f))
            {
                continue;
            }
            if(rnd() < za::min(1.f, (cut ? 0.12f : 0.35f) * k))
            {
                smearHand(h, 1, za::min(0.3f * k, 3.f));
            }
            if(cut && rnd() < za::min(1.f, 0.1f * k * vr_gore_spread.value))
            {
                armMarks(h, 1, 0.9f, 0.f, true);
            }
        }
    }
    else
    {
        // A shot (or another's blow) hitting near you: a few drops reach you.
        const float range = za::max(vr_gore_spatter_range.value, 1.f);
        const float d = za::min(dHand, za::max(0.f, dEye - 6.f));
        if(d > range)
        {
            return;
        }
        const float f = 1.f - d / range;
        const float k = vr_gore_spatter.value * vr_gore_spatter_shots.value * f * (ev.kind == KindMelee ? 2.f : 1.f) * za::sqrt(hit);
        if(k <= 0.f)
        {
            return;
        }
        reach = d + 18.f;
        splats.pushBack(spatterSplat(at, reach, 1.5f, za::max(d, 2.f), za::min(0.12f * k, 0.6f), 0.8f));
        paintOnYou(splats, at, reach, true);
    }
    spatteredNow.pushBack(at);
    if(vr_wounds_debug.value)
    {
        Con_Printf("wounds: spatter from kind %d (%d) at %.0f %.0f %.0f: hand %d %.0f units, eyes %.0f\n", ev.kind, ev.amount, at.x, at.y,
            at.z, hand, static_cast<double>(dHand), static_cast<double>(dEye));
    }
}

// A gib flying into you at `at` (`r` its size): its blood where it struck.
void gibStrike(const glm::vec3& at, float r, float amount)
{
    za::Vector<Splat> splats;
    splats.pushBack(spatterSplat(at, r + 9.f, r * 0.7f, r + 4.f, za::min(0.15f * amount, 0.9f), 1.f));
    paintOnYou(splats, at, r + 9.f, true);
}

// Each frame: the gibs flying round you that strike you (by the capsules round your body and hands, as drawn).
void gibContacts(double now)
{
    if(!vr_gore_spatter_gibs.value || !vr_wounds.value || cl.stats[STAT_HEALTH] <= 0 || cl.viewentity <= 0)
    {
        gibsSeen.clear();
        return;
    }
    entity_t* own[3]{};
    view::woundTargets(own);
    const za::Vector<Capsule> caps = playerCapsules(own);
    const glm::vec3 me = originOf(cl_entities[cl.viewentity]);
    const int carried[2]{cl.stats[protocol::STAT_QVR_CARRYOFF], cl.stats[protocol::STAT_QVR_CARRYMAIN]};
    for(int i = 1; i < cl.num_entities && !caps.empty(); i++)
    {
        entity_t& e = cl_entities[i];
        if(!e.model || e.model->type != mod_alias || e.msgtime < cl.mtime[1] - 0.2 || i == carried[0] || i == carried[1])
        {
            continue;
        }
        const glm::vec3 at = originOf(e);
        if(glm::distance(at, me) > 96.f || !isGib(i))
        {
            continue;
        }
        const auto it = gibsSeen.find(i);
        const bool first = it == gibsSeen.end() || now - it->second.seen > 0.5;
        GibSeen& g = gibsSeen[i];
        const float speed = first ? 1e9f : glm::distance(at, g.at) / static_cast<float>(za::max(now - g.seen, 1e-3));
        g.at = at;
        g.seen = now;
        if(now - g.struck < 0.75 || speed < 40.f)
        {
            continue;
        }
        const float r = za::clamp(modelRadius(e.model) * VR_EntityScale(&e) * 0.5f, 1.f, 8.f);
        for(const Capsule& c : caps)
        {
            if(glm::distance(at, closestOnSegment(at, c.a, c.b)) < c.r + r)
            {
                g.struck = now;
                gibStrike(at, r, vr_gore_spatter_gibs.value * za::clamp(r / 3.f, 0.5f, 1.5f));
                if(vr_wounds_debug.value)
                {
                    Con_Printf("wounds: gib %d (%s, %.1f across) struck you at %.0f %.0f %.0f\n", i, e.model->name, static_cast<double>(r * 2.f),
                        at.x, at.y, at.z);
                }
                break;
            }
        }
    }
    for(auto it = gibsSeen.begin(); it != gibsSeen.end();)
    {
        if(now - it->second.seen > 2.0)
        {
            it = gibsSeen.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

// `count` bleeding marks along an arm (`hand`'s, -1 either: its upper arm and forearm, `lower` the forearm's wrist
// half alone), each one a leg's instead by chance `legs`: on its top and outside mostly (what you see of your arms),
// some running down. Painted onto your body (and your hands where they reach them).
void armMarks(int hand, int count, float size, float legs, bool lower)
{
    avatar::Skeleton sk;
    if(count <= 0 || size <= 0.f || !vr_wounds.value || !avatar::skeleton(sk))
    {
        return;
    }
    const glm::vec3 up{0.f, 0.f, 1.f};
    za::Vector<Splat> splats;
    glm::vec3 mid{0.f};
    for(int i = 0; i < count; i++)
    {
        glm::vec3 a, b;
        float rad, u;
        if(sk.legs && rnd() < legs)
        {
            const int side = rnd() < 0.5f ? 0 : 1;
            const bool thigh = rnd() < 0.6f;
            a = thigh ? sk.hip[side] : sk.knee[side];
            b = thigh ? sk.knee[side] : sk.ankle[side];
            rad = (thigh ? 0.085f : 0.06f) * sk.m2w;
            u = rnd(0.15f, 0.85f);
        }
        else
        {
            const int h = hand >= 0 ? hand : (rnd() < 0.5f ? 0 : 1);
            const bool upper = !lower && rnd() < 0.4f;
            a = upper ? sk.shoulder[h] : sk.elbow[h];
            b = upper ? sk.elbow[h] : sk.wrist[h];
            rad = (upper ? 0.058f : 0.046f) * sk.m2w;
            u = lower ? rnd(0.5f, 0.92f) : rnd(0.12f, 0.92f);
        }
        glm::vec3 axis = b - a;
        const float len = glm::length(axis);
        if(len < 1.f)
        {
            continue;
        }
        axis /= len;
        const glm::vec3 p = a + axis * (len * u);
        glm::vec3 top = up - axis * glm::dot(up, axis);
        top = glm::length(top) > 0.2f ? glm::normalize(top) : glm::normalize(glm::cross(axis, glm::vec3{1.f, 0.f, 0.f}));
        const float ang = rnd(-2.f, 2.f); // round from its top, either way
        const glm::vec3 dir = top * za::cos(ang) + glm::cross(axis, top) * za::sin(ang);
        const float r = rnd(0.6f, 1.2f) * size;
        const glm::vec3 c = p + dir * rad;
        splats.pushBack(woundSplat(c, r, dir, axis, rnd(1.3f, 2.2f), rad * 0.9f + 0.5f, 0.1f, rnd() < 0.7f ? r * rnd(1.5f, 4.f) : 0.f, paintBlood));
        mid += c;
    }
    if(splats.empty())
    {
        return;
    }
    if(vr_wounds_debug.value)
    {
        Con_Printf("wounds: %d bleeding marks on your %s\n", static_cast<int>(splats.size()), hand < 0 ? "arms" : hand ? "main arm" : "off arm");
    }
    paintOnYou(splats, mid / static_cast<float>(splats.size()), 48.f, false);
}

// A wound of yours that bleeds: blood running over your arms too (and now and then a leg), more for a harder hit.
void hurtSpread(const Event& ev)
{
    if(!isBlood(ev.kind) || ev.kind == KindBurn || vr_gore_spread.value <= 0.f)
    {
        return;
    }
    const float want = vr_gore_spread.value * za::clamp(static_cast<float>(ev.amount) / 15.f, 0.3f, 3.f) * (ev.kind == KindShot ? 0.6f : 1.f);
    const int whole = static_cast<int>(want);
    const int n = za::min(4, whole + (rnd() < want - static_cast<float>(whole) ? 1 : 0));
    armMarks(-1, n, 1.f, 0.2f, false);
}

// Each tick (`amount` of it off): your gear under water washed, held or lying about (vr_gore_wash).
void washGear(float amount)
{
    for(int i = 0; i < maskCount(); i++)
    {
        Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.gear || !m.ent || looseIndex(m.ent) >= 0 || vr_gametime - m.lastDrawn > 1.0)
        {
            continue;
        }
        float surface;
        if(waterSurface(originOf(*m.ent), 16.f, surface))
        {
            washUnder(i, const_cast<entity_t*>(m.ent), surface, amount);
        }
    }
}

// Mask `layer`'s blood: texels with any, and their blood summed.
void maskBlood(int layer, int& count, double& sum)
{
    const Mask& m = masks[static_cast<za::SizeT>(layer)];
    za::Vector<byte> rgba(static_cast<za::SizeT>(m.w) * static_cast<za::SizeT>(m.h) * 4);
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    attach(GL_READ_FRAMEBUFFER, layer);
    glReadPixels(0, 0, m.w, m.h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));
    count = 0;
    sum = 0.0;
    for(za::SizeT p = 0; p < rgba.size(); p += 4)
    {
        count += rgba[p] > 0 ? 1 : 0;
        sum += rgba[p] / 255.0;
    }
}

// ----------------------------------------------------------------------------
// Over time: drying, cooling, healing; the masks of entities gone freed.

void steps(float dt)
{
    // Drying: 1/255 of the wetness a step (dryTime from soaked); embers cooling; the player's heal.
    const int n = za::max(1, static_cast<int>(dt / tick + 0.5f));
    for(int i = 0; i < maskCount(); i++)
    {
        Mask& m = masks[static_cast<za::SizeT>(i)];
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
            const float h = za::min(m.heal, healRate * static_cast<float>(n));
            take.r = take.g = h;
            m.heal -= h;
        }
        if(take != glm::vec4{0.f})
        {
            subtract(i, take, true); // (healing: your own wounds', not the blood on you that isn't yours)
        }
    }
}

void checkEntities()
{
    for(int i = 0; i < maskCount(); i++)
    {
        Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.ent || m.view)
        {
            continue;
        }
        const auto* e = const_cast<entity_t*>(m.ent);
        if(sameLayout(m.model, e->model) || (m.gear && !e->model))
        {
            continue; // (your gear not drawn this frame: out of sight, or removed and taken: its clear says)
        }
        // Another model: its head flying off (the monster is its head now), or the slot taken by something else.
        const int num = static_cast<int>(e - cl_entities);
        const bool head = e->model && ZA_STRNCMP(e->model->name, "progs/h_", 8) == 0;
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
                                     : static_cast<float>(health - playerHealth) / static_cast<float>(za::max(1, 100 - playerHealth));
    playerHealth = health;
    if(!respawned && !healed)
    {
        return;
    }
    forgetPlayerWounds(respawned ? 1.f : za::clamp(heal, 0.f, 1.f));
    for(int i = 0; i < maskCount(); i++)
    {
        Mask& m = masks[static_cast<za::SizeT>(i)];
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
            m.heal = za::min(1.f, m.heal + heal);
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
    za::copy(on, on + 3, chanOn);
    if(take == glm::vec4{0.f})
    {
        return;
    }
    for(int i = 0; i < maskCount(); i++)
    {
        if(masks[static_cast<za::SizeT>(i)].ent)
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
    for(int i = 0; i < maskCount(); i++)
    {
        Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.ent || m.wetLeft <= 0.f || now < m.dripNext || vr_gametime - m.lastDrawn > 0.5)
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
            if(ZA_STRNCMP(m.ent->model->name, "progs/vrbody", 12) == 0)
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
            const za::SizeT n = surf.tris.size() / 3;
            for(int tries = 0; tries < 10 && !found; tries++)
            {
                const za::SizeT k = za::min(n - 1, static_cast<za::SizeT>(rnd() * static_cast<float>(n))) * 3;
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
        const float h = za::max(at.z - floorZ, 1.f);
        particles::bloodDrip(at, za::sqrt(2.f * h / za::max(sv_gravity.value, 100.f)), floorZ, 0.35f, color);
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
        events.pushBack(ev);
    }
}

void parseClear()
{
    Event ev;
    ev.num = MSG_ReadShort();
    ev.kind = KindClear;
    if(vr_wounds_debug.value >= 2)
    {
        Con_Printf("wounds: received clear on %d\n", ev.num);
    }
    if(enabled() && events.size() < 512)
    {
        events.pushBack(ev);
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
    spatteredNow.clear();
    gearFrame(cl.time); // (before checkEntities: a hand's weapon let go keeps its blood)
    checkEntities();
    playerState();
    const auto t1 = Sys_DoubleTime();
    const za::SizeT received = events.size();
    for(const Event& ev : events)
    {
        apply(ev);
    }
    events.clear();
    const double now = cl.time;
    gibHands(now);
    gibContacts(now);
    reopen(now);
    const auto t2 = Sys_DoubleTime();

    if(lastTick < 0.0 || now < lastTick || now - lastTick > 5.0)
    {
        lastTick = now;
    }
    if(now - lastTick >= tick)
    {
        const float dt = static_cast<float>(now - lastTick);
        steps(dt);
        wash(za::max(1, static_cast<int>(dt / tick + 0.5f)));
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
    for(int i = 0; i < maskCount(); i++)
    {
        masks[static_cast<za::SizeT>(i)] = Mask{};
    }
    maskOf.clear();
    resetGear();
    events.clear();
    lastTick = -1.0;
    playerHealth = -1000;
    playerWounds.clear();
    heldGib[0] = heldGib[1] = 0;
    lastWash = -1e9;
    reopenPending = false;
    reopenStart = -1.0;
    reopened = 0;
    for(SkinPart& p : skinParts)
    {
        p = SkinPart{};
    }
}

int skinLevel(int part, int level, const float* origin)
{
    if(part < 0 || part > 2)
    {
        return level;
    }
    SkinPart& s = skinParts[part];
    const double now = cl.time;
    if(now < s.seen - 1.0)
    {
        s = SkinPart{}; // time went back: another map, a loaded game
    }
    s.seen = now;
    const bool alive = cl.stats[STAT_HEALTH] > 0;
    if(!alive)
    {
        s = SkinPart{};
        s.seen = now;
        return level;
    }
    if(part < 2)
    {
        const int ent = cl.stats[part == 1 ? protocol::STAT_QVR_CARRYMAIN : protocol::STAT_QVR_CARRYOFF];
        const bool gib = vr_gore_hands.value > 0.f && isGib(ent);
        if(gib && s.gibEnt != ent)
        {
            s.gib = za::max(s.gib, za::clamp(static_cast<int>(za::lround(vr_gore_hands.value * 2.f)), 1, 3));
        }
        s.gibEnt = gib ? ent : 0;
    }
    if(vr_gore_wash.value && origin && inWater(glm::vec3{origin[0], origin[1], origin[2]}))
    {
        if(s.washed < -1e8)
        {
            s.washedLevel = level;
        }
        s.washed = now;
        s.gib = 0;
    }
    int shown = level;
    if(s.washed > -1e8)
    {
        const bool reopened_ = vr_gore_reopen.value && now - s.washed >= vr_gore_reopen_delay.value;
        if(reopened_ || level > s.washedLevel) // re-opened, or hurt again since
        {
            s.washed = -1e9;
        }
        else
        {
            shown = 0;
            s.washedLevel = za::min(s.washedLevel, level); // (healed meanwhile: a later hit shows)
        }
    }
    return za::max(shown, s.gib);
}

void handsTest_f()
{
    const int hand = Cmd_Argc() > 1 && ZA_STRCMP(Cmd_Argv(1), "off") == 0 ? 0 : 1;
    const float amount = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : za::max(0.25f, vr_gore_hands.value);
    if(!vr_wounds.value)
    {
        skinParts[hand].gib = za::clamp(static_cast<int>(za::lround(amount * 2.f)), 1, 3);
        Con_Printf("vr_gore_hands_test: the %s hand's skin %d (the wound skins)\n", hand ? "main" : "off", skinParts[hand].gib);
        return;
    }
    if(!ensureTexture())
    {
        return;
    }
    smearHand(hand, za::max(1, static_cast<int>(amount * 4.f + 0.5f)), amount);
    end();
    Con_Printf("vr_gore_hands_test: a gib's blood on the %s hand\n", hand ? "main" : "off");
}

void handsInfo_f()
{
    const double now = cl.time;
    Con_Printf("gore hands: %d wounds kept, %s, last wash %.1f s ago, re-open %s", static_cast<int>(playerWounds.size()),
        lastWash > -1e8 && now - lastWash < 0.25 ? "washing" : "dry", lastWash > -1e8 ? now - lastWash : -1.0,
        !reopenPending ? "none" : reopenStart < 0.0 ? "pending" : "under way");
    if(reopenPending && reopenStart < 0.0)
    {
        Con_Printf(" (in %.1f s)", static_cast<double>(vr_gore_reopen_delay.value) - (now - lastWash));
    }
    const int carried[2]{cl.stats[protocol::STAT_QVR_CARRYOFF], cl.stats[protocol::STAT_QVR_CARRYMAIN]};
    Con_Printf(", carried %d %s, %d %s\n", carried[0], isGib(carried[0]) ? "(a gib)" : "", carried[1], isGib(carried[1]) ? "(a gib)" : "");
    if(vr_wounds.value && array)
    {
        int count[3];
        double sum[3];
        viewBlood(count, sum);
        static constexpr const char* names[3] = {"off hand", "main hand", "body"};
        for(int i = 0; i < 3; i++)
        {
            if(count[i] < 0)
            {
                Con_Printf("  %s: no mask\n", names[i]);
            }
            else
            {
                Con_Printf("  %s: blood on %d texels (%.1f)\n", names[i], count[i], sum[i]);
            }
        }
    }
    else
    {
        Con_Printf("  wound skins: off hand %d, main hand %d (gib %d %d), body washed %s\n", skinParts[0].gib, skinParts[1].gib,
            skinParts[0].gibEnt, skinParts[1].gibEnt, skinParts[2].washed > -1e8 ? "yes" : "no");
    }
    // Your gear's blood (vr_gore_gear): held, lying about, kept for a hand.
    for(int i = 0; i < maskCount() && array; i++)
    {
        const Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.gear || !m.ent)
        {
            continue;
        }
        int count;
        double sum;
        maskBlood(i, count, sum);
        const int k = looseIndex(m.ent);
        const char* where = k >= 0 ? (loose[k].hand >= 0 ? "kept (let go)" : "kept (taken from the world)")
                            : m.ent == heldEnt[0] ? "in the off hand"
                            : m.ent == heldEnt[1] ? "in the main hand"
                            : m.ent == gearOf(0) || m.ent == gearOf(1) ? "carried"
                                                  : "in the world";
        Con_Printf("  gear %s %s: blood on %d texels (%.1f)\n", m.model ? m.model->name : "?", where, count, sum);
    }
}

void spatterTest_f()
{
    const char* what = Cmd_Argc() > 1 ? Cmd_Argv(1) : "blow";
    if(!vr_wounds.value || !ensureTexture())
    {
        Con_Printf("vr_gore_spatter_test: Dynamic Wounds are off\n");
        return;
    }
    entity_t* own[3]{};
    view::woundTargets(own);
    vec3_t fwd, right, upv;
    AngleVectors(r_refdef.viewangles, fwd, right, upv);
    const glm::vec3 ahead{fwd[0], fwd[1], fwd[2]};
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    entity_t* g = gearOf(1);
    const glm::vec3 hand = g ? originOf(*g) : own[2] ? originOf(*own[2]) : eye + ahead * 16.f;
    const bool shot = ZA_STRCMP(what, "shot") == 0, gib = ZA_STRCMP(what, "gib") == 0, saw = ZA_STRCMP(what, "saw") == 0;
    const float dist = Cmd_Argc() > 2 ? static_cast<float>(atof(Cmd_Argv(2))) : shot ? 40.f : gib ? 0.f : 6.f;
    Event ev;
    ev.num = 0;
    ev.dir = ahead;
    ev.amount = 30;
    ev.kind = shot ? KindShot : KindMelee;
    ev.org = shot ? eye + ahead * dist : hand + ahead * dist;
    if(gib)
    {
        const glm::vec3 at = (own[2] ? originOf(*own[2]) : hand) + ahead * dist;
        gibStrike(at, 3.f, vr_gore_spatter_gibs.value);
        Con_Printf("vr_gore_spatter_test: a gib struck at %.0f %.0f %.0f\n", at.x, at.y, at.z);
    }
    else
    {
        spatteredNow.clear();
        spatterFrom(ev, shot ? -1 : saw ? 1 : 0);
        Con_Printf("vr_gore_spatter_test: a %s's blood from %.0f %.0f %.0f\n", shot ? "shot" : saw ? "chainsaw cut" : "blow", ev.org.x,
            ev.org.y, ev.org.z);
    }
    end();
}

void test_f()
{
    if(Cmd_Argc() < 3)
    {
        Con_Printf("vr_wounds_test <entity number | self | ahead> <kind 1 shot, 2 nail, 3 melee, 4 blast, 5 burn, 6 zap, 7 lava, 8 slime, "
                   "9 liquid> [amount] [right] [up] [extra]\n");
        return;
    }
    if(ZA_STRCMP(Cmd_Argv(1), "all") == 0) // every model drawn with an alias model (monsters, corpses, items): a stress test
    {
        char args[256];
        q_snprintf(args, sizeof(args), "%s %s %s %s %s", Cmd_Argc() > 2 ? Cmd_Argv(2) : "1", Cmd_Argc() > 3 ? Cmd_Argv(3) : "20",
            Cmd_Argc() > 4 ? Cmd_Argv(4) : "0", Cmd_Argc() > 5 ? Cmd_Argv(5) : "0", Cmd_Argc() > 6 ? Cmd_Argv(6) : "0");
        int n = 0;
        for(int i = 1; i < cl.num_entities; i++)
        {
            const entity_t& c = cl_entities[i];
            if(i != cl.viewentity && c.model && c.model->type == mod_alias && c.msgtime >= cl.mtime[1] - 0.2 &&
                ZA_STRNCMP(c.model->name, "progs/v_", 8) != 0)
            {
                Cbuf_InsertText(va("vr_wounds_test %d %s\n", i, args));
                n++;
            }
        }
        Con_Printf("vr_wounds_test: %d models\n", n);
        return;
    }
    Event ev;
    const bool self = ZA_STRCMP(Cmd_Argv(1), "self") == 0;
    ev.num = self ? cl.viewentity : atoi(Cmd_Argv(1));
    if(ZA_STRCMP(Cmd_Argv(1), "ahead") == 0) // the model nearest the view's line, ahead
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
    ev.kind = atoi(Cmd_Argv(2));
    ev.amount = Cmd_Argc() > 3 ? atoi(Cmd_Argv(3)) : 20;
    const float right = Cmd_Argc() > 4 ? static_cast<float>(atof(Cmd_Argv(4))) : 0.f;
    const float up = Cmd_Argc() > 5 ? static_cast<float>(atof(Cmd_Argv(5))) : 0.f;
    ev.extra = Cmd_Argc() > 6 ? atoi(Cmd_Argv(6)) : 0;
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
        dir = -glm::vec3{za::cos(yaw), za::sin(yaw), 0.f}; // from ahead
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
    events.pushBack(ev);
}

void dump_f()
{
    if(!array)
    {
        Con_Printf("vr_wounds_dump: no masks\n");
        return;
    }
    const za::SizeT side = static_cast<za::SizeT>(za::max(layerSize, fineSize));
    za::Vector<byte> rgba(side * side * 4);
    za::Vector<byte> rgb(side * side * 3);
    Sys_mkdir(va("%s/wounds", com_gamedir));
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, fbo);
    for(int i = 0; i < maskCount(); i++)
    {
        const Mask& m = masks[static_cast<za::SizeT>(i)];
        if(!m.ent)
        {
            continue;
        }
        int in[2], side[2];
        const int n = layersOf(i, in, side);
        for(int k = 0; k < n; k++) // (your body's: its left side and middle, then its right side)
        {
            attach(GL_READ_FRAMEBUFFER, in[k]);
            glReadPixels(0, 0, m.w, m.h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            for(int p = 0; p < m.w * m.h; p++)
            {
                // r blood, g char, b wetness; heat shown as white over them
                const int heat = rgba[static_cast<za::SizeT>(p) * 4 + 3];
                for(int c = 0; c < 3; c++)
                {
                    rgb[static_cast<za::SizeT>(p) * 3 + c] = static_cast<byte>(za::max<int>(rgba[static_cast<za::SizeT>(p) * 4 + c], heat));
                }
            }
            const char* slash = strrchr(m.model->name, '/');
            char name[MAX_OSPATH];
            q_snprintf(name, sizeof(name), "wounds/mask_%02d_%s%s.png", i, slash ? slash + 1 : m.model->name, side[k] == 1 ? "_right" : "");
            Image_WritePNG(name, rgb.data(), m.w, m.h, 24, true);
            Con_Printf("vr_wounds_dump: %s (%dx%d)\n", name, m.w, m.h);
        }
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
    Con_Printf("wounds: %d of %d masks used (%dx%d layers, %.1f MB; yours %dx%d, %.1f MB), %d paints so far, last frame's work "
               "%.3f ms (CPU)\n",
        used, maskCount(), layerSize, layerSize, static_cast<double>(layers) * layerSize * layerSize * 4.0 / (1024.0 * 1024.0), fineSize,
        fineSize, static_cast<double>(maskCount() - layers) * fineSize * fineSize * 5.0 / (1024.0 * 1024.0), paintsTotal,
        static_cast<double>(lastPaintMs));
    for(int i = 0; i < maskCount(); i++)
    {
        const Mask& m = masks[static_cast<za::SizeT>(i)];
        if(m.ent)
        {
            Con_Printf("  %2d %s%s %dx%d, drawn %.1f s ago%s%s\n", i, m.model ? m.model->name : "?",
                m.view ? (isFine(i) ? " (you, fine)" : " (you)") : "", m.w, m.h,
                vr_gametime - m.lastDrawn, m.wetLeft > 0.f ? ", wet" : "", m.hotLeft > 0.f ? ", hot" : "");
        }
    }
}

} // namespace qvr::wounds

extern "C" void VR_AliasWound(const entity_t* e, float out[4], float side[4])
{
    using namespace qvr::wounds;
    out[0] = out[1] = out[2] = out[3] = 0.f;
    side[0] = side[1] = side[2] = 0.f;
    side[3] = za::clamp(qvr::vr_wounds_blood_alpha.value, 0.f, 1.f);
    if(!array)
    {
        return;
    }
    const auto it = maskOf.find(e);
    if(it == maskOf.end())
    {
        return;
    }
    Mask& m = masks[static_cast<za::SizeT>(it->second)];
    if(!sameLayout(m.model, e->model))
    {
        return;
    }
    m.lastDrawn = vr_gametime;
    out[0] = isFine(it->second) ? -static_cast<float>(layerIn(it->second) + 1) : static_cast<float>(it->second + 1);
    out[1] = static_cast<float>(m.w);
    out[2] = static_cast<float>(m.h);
    out[3] = static_cast<float>(za::fmod(cl.time, 1000.0));
    if(isSided(it->second))
    {
        side[0] = m.sides[0];
        side[1] = m.sides[1];
    }
}

extern "C" void VR_AliasWoundPaintSide(const entity_t* e, int side, float out[4])
{
    using namespace qvr::wounds;
    out[0] = out[1] = 0.f;
    out[2] = side >= 0 ? static_cast<float>(side + 1) : 0.f;
    out[3] = 1.f;
    const auto it = maskOf.find(e);
    if(side >= 0 && it != maskOf.end() && isSided(it->second))
    {
        out[0] = masks[static_cast<za::SizeT>(it->second)].sides[0];
        out[1] = masks[static_cast<za::SizeT>(it->second)].sides[1];
    }
}

extern "C" unsigned VR_WoundTexture(void)
{
    return qvr::wounds::array;
}

extern "C" unsigned VR_WoundFineTexture(void)
{
    return qvr::wounds::fineArray;
}

extern "C" unsigned VR_WoundBloodTexture(void)
{
    return qvr::wounds::bloodArray;
}

extern "C" void VR_WoundFrameData(float out[2])
{
    out[0] = za::clamp(qvr::vr_wounds_bump_burns.value, 0.f, 4.f);
    out[1] = za::clamp(qvr::vr_wounds_bump_blood.value, 0.f, 4.f);
}
