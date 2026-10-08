// vr_collectfx.cpp -- what a hand puts away is drawn going in (vr_collect_fx; ROUND21.md, "Put-away transition").
//
// The server takes the thing at once (its pickup, the sounds, the ammo or the key given: nothing waits for this) and
// tells that player's client (QVR_SVC_COLLECT, QC VR_CollectFx_Send): which hand, at which holster or pouch (the hand's
// hotspot), which entity, and its model and pose as the server had it. The message comes with the update that no longer
// has the entity, so the client still holds it as drawn last frame (in the hand: vr_held.cpp) and copies that, else
// builds it from the server's pose. The copy is drawn for vr_collect_fx_time (vr_gametime: slowed in bullet time, the same
// at any frame rate), its middle going from where it was into the holster's or pouch's point and its size from 1 to
// vr_collect_fx_size, both on an ease-in (u = t^2: it starts gently and drops in). The start is kept as an offset from the
// target, which is placed on the body every frame, so the copy follows the body as it moves. It is shrunk about its
// origin in vr_render.cpp (applyPre: everything the item's own transforms do, inside), and its origin is set so that its
// drawn middle is where it should be.
//
// Its "into the gun" variant (hotspot collectfx::intoGun: immersive reloading's shells, QC vr_reload.qc VR_Reload_Load):
// the hand has let go of a shell (or the super shotgun's pair) at the load point of the gun in its other hand, loaded at
// once; the copy slides from where it was to the load point and on into the gun (view::loadPath: up the shotgun's port
// into its tube, into the super shotgun's chambers) over vr_reload_insert_time, at its size, carried by the gun (its
// place and turn kept in the gun's model space: it follows the gun as it moves), and is gone inside it.

#include "vr_collectfx.hpp"
#include "vr_body.hpp"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/Clamp.hpp"


using namespace qvr;

namespace
{

struct Item
{
    bool live{false};
    entity_t ent{};             // the copy drawn
    client::EntityVr net{};     // the item's networked scale and offset
    bool hasNet{false};
    glm::mat3 axes{1.f};        // its turn, kept
    glm::vec3 centreLocal{0.f}; // its drawn middle from its origin, in its axes, at full size
    glm::vec3 from{0.f};        // its drawn middle when taken
    glm::vec3 offset{0.f};      // from - the target, on the first frame drawn
    bool placed{false};
    int hand{0};
    int hotspot{0};
    int entNum{0};
    double start{0.0};
    float shrink{1.f};
    int drawnFrame{-1}; // host_framecount it was added to the scene
    bool intoGun{false};          // the "into the gun" variant: the gun in the other hand
    int gunHand{0};
    int gunEnt{0};                // or the gun lying about as this client entity (QVR_CFX_INTO_PROP; 0: in a hand)
    glm::vec3 startLocal{0.f};    // its middle when let go of, in the gun's model space
    glm::mat3 axesLocal{1.f};     // its turn in the gun's (its axes')
};

constexpr int maxItems = collectfx::maxCopies;
constexpr int intoGunHotspot = 240; // QC's QVR_CFX_INTO_GUN: into the gun in the other hand (vr_reload.qc)
// QC's QVR_CFX_INTO_PROP: into the gun lying about that the message's entity is (VR_Reload_IntoProp; the author's note
// vrfiringrange_2026-10-08_14-18-22: shells loaded into a lying shotgun went in at once, without a held gun's slide).
constexpr int intoPropHotspot = 241;
za::Array<Item, maxItems> items;

// The gun an "into the gun" copy slides into, as drawn now: the one in its hand, or the one lying about (gunEnt).
struct GunNow
{
    view::ViewEntity prop; // (gunEnt's)
    float open{0.f};       // its barrels drawn down (a super shotgun lying open)
};

[[nodiscard]] bool gunNow(const Item& it, GunNow& g)
{
    return it.gunEnt <= 0 || view::propGun(it.gunEnt, g.prop, g.open);
}

[[nodiscard]] bool gunPoint(const Item& it, const GunNow& g, const glm::vec3& local, glm::vec3& out)
{
    if(it.gunEnt <= 0)
    {
        return view::gunToWorld(it.gunHand, local, out);
    }
    out = view::modelPoint(g.prop, local);
    return true;
}

[[nodiscard]] bool gunTurn(const Item& it, const GunNow& g, glm::mat3& out)
{
    if(it.gunEnt <= 0)
    {
        return view::gunAxes(it.gunHand, out);
    }
    out = held::axesFromAngles(g.prop.ent.angles, false);
    return true;
}

[[nodiscard]] bool gunPath(const Item& it, const GunNow& g, glm::vec3& port, glm::vec3& deep, glm::vec3& end)
{
    return it.gunEnt <= 0 ? view::loadPath(it.gunHand, port, deep, end)
                          : view::modelLoadPath(g.prop.ent.model, g.open, port, deep, end);
}

// A world point in the gun's model space.
[[nodiscard]] bool gunLocal(const Item& it, const GunNow& g, const glm::vec3& w, glm::vec3& out)
{
    if(it.gunEnt <= 0)
    {
        return view::gunFromWorld(it.gunHand, w, out);
    }
    const glm::vec3 o = view::modelPoint(g.prop, glm::vec3{0.f});
    const glm::mat3 m{view::modelPoint(g.prop, {1.f, 0.f, 0.f}) - o, view::modelPoint(g.prop, {0.f, 1.f, 0.f}) - o,
        view::modelPoint(g.prop, {0.f, 0.f, 1.f}) - o};
    if(std::abs(glm::determinant(m)) < 1e-9f)
    {
        return false;
    }
    out = glm::inverse(m) * (w - o);
    return true;
}

// Where the thing goes: the holster's or pouch's point on the body (the hand's hotspot when it let go), as drawn.
[[nodiscard]] bool targetOf(const hands::State& s, int hotspot, glm::vec3& out)
{
    if(!s.valid)
    {
        return false;
    }
    if(hotspot == body::HS_AMMO_POUCH)
    {
        out = body::ammoPouchPosition(s);
        return true;
    }
    if(hotspot == body::HS_GRENADE_POUCH)
    {
        out = body::pouchPosition(s);
        return true;
    }
    for(int h = 0; h < body::HolsterCount; ++h)
    {
        if(body::holsterHotspot(static_cast<body::Holster>(h)) == hotspot)
        {
            out = body::holsterPosition(s, static_cast<body::Holster>(h));
            return true;
        }
    }
    return false;
}

// One of the copies (the renderer asks of every entity: a range check first), else null.
[[nodiscard]] const Item* itemOf(const entity_t* e)
{
    const auto* p = reinterpret_cast<const unsigned char*>(e);
    const auto* first = reinterpret_cast<const unsigned char*>(&items[0]);
    if(p < first || p >= first + sizeof(Item) * maxItems)
    {
        return nullptr;
    }
    for(const Item& it : items)
    {
        if(&it.ent == e)
        {
            return &it;
        }
    }
    return nullptr;
}

[[nodiscard]] Item& freeSlot()
{
    Item* oldest = &items[0];
    for(Item& it : items)
    {
        if(!it.live)
        {
            return it;
        }
        if(it.start < oldest->start)
        {
            oldest = &it;
        }
    }
    return *oldest;
}

} // namespace

namespace qvr::collectfx
{

void parse()
{
    const int hand = MSG_ReadByte();
    const int hotspot = MSG_ReadByte();
    const int ent = MSG_ReadShort();
    const int modelIndex = MSG_ReadShort();
    float origin[3], angles[3];
    for(float& v : origin)
    {
        v = MSG_ReadFloat();
    }
    for(float& v : angles)
    {
        v = MSG_ReadFloat();
    }

    const bool intoProp = hotspot == intoPropHotspot;
    const bool intoGun = hotspot == intoGunHotspot || intoProp;
    if((intoGun ? vr_reload_insert_time.value <= 0.f : (!vr_collect_fx.value || vr_collect_fx_time.value <= 0.f)) ||
        cls.demoplayback || modelIndex <= 0 || modelIndex >= MAX_MODELS)
    {
        return;
    }
    qmodel_t* model = cl.model_precache[modelIndex];
    if(!model || (model->type != mod_alias && model->type != mod_brush))
    {
        return;
    }

    // As drawn last frame (still held: its removal comes with this update), else as the server had it.
    Item& it = freeSlot();
    it = Item{};
    const bool known = !intoProp && ent > 0 && ent < cl.num_entities; // (into a prop: the entity is the gun)
    const bool drawn = known && cl_entities[ent].model == model;
    if(known)
    {
        it.ent = cl_entities[ent];
        if(const client::EntityVr* net = client::entityVr(ent))
        {
            it.net = *net;
            it.hasNet = true;
        }
    }
    else
    {
        it.ent.scale = ENTSCALE_DEFAULT;
        it.ent.alpha = ENTALPHA_DEFAULT;
        it.ent.colormap = vid.colormap;
    }
    if(!drawn)
    {
        it.ent.model = model;
        for(int i = 0; i < 3; ++i)
        {
            it.ent.origin[i] = origin[i];
            it.ent.angles[i] = angles[i];
        }
    }
    it.ent.effects = 0;
    it.ent.lerpflags = LERP_RESETANIM; // (its frame as it was; its origin as set, not lerped)
    it.ent.forcelink = false;
    if(!it.ent.colormap)
    {
        it.ent.colormap = vid.colormap;
    }

    const bool brush = model->type == mod_brush;
    it.axes = held::axesFromAngles(it.ent.angles, brush);
    glm::vec3 lo, hi;
    const glm::vec3 zero{0.f};
    held::modelBox(model, it.hasNet ? it.net.scale : zero, it.hasNet ? it.net.scaleOrigin : zero,
        it.hasNet ? it.net.offset : zero, lo, hi);
    it.centreLocal = (lo + hi) * 0.5f * (it.ent.scale ? ENTSCALE_DECODE(it.ent.scale) : 1.f);
    it.from = glm::vec3{it.ent.origin[0], it.ent.origin[1], it.ent.origin[2]} + it.axes * it.centreLocal;
    it.hand = hand;
    it.hotspot = hotspot;
    it.entNum = ent;
    it.start = vr_gametime;
    it.live = true;
    if(intoGun)
    {
        it.intoGun = true;
        it.gunHand = 1 - hand;
        it.gunEnt = intoProp ? ent : 0;
        it.entNum = intoProp ? 0 : ent;
        glm::mat3 gun;
        GunNow g;
        if(!gunNow(it, g) || !gunLocal(it, g, it.from, it.startLocal) || !gunTurn(it, g, gun))
        {
            it.live = false;
            return;
        }
        it.axesLocal = glm::transpose(gun) * it.axes;
    }
    if(vr_debug_collect_fx.value)
    {
        Con_Printf("collect fx: %s (entity %d, %s pose) by hand %d into hotspot %d, from %.1f %.1f %.1f\n", model->name,
            ent, drawn ? "drawn" : "server", hand, hotspot, it.from.x, it.from.y, it.from.z);
    }
}

void frame(const hands::State& s)
{
    QVR_PROFILE("collect fx");
    const float time = vr_collect_fx_time.value;
    const float endSize = za::clamp(vr_collect_fx_size.value, 0.01f, 1.f);
    for(Item& it : items)
    {
        if(!it.live)
        {
            continue;
        }
        const float span = it.intoGun ? vr_reload_insert_time.value : time;
        const float t = span > 0.f ? static_cast<float>((vr_gametime - it.start) / span) : 1.f;
        if(t >= 1.f || t < 0.f || !it.ent.model)
        {
            if(vr_debug_collect_fx.value && it.ent.model)
            {
                Con_Printf("collect fx: %s gone in\n", it.ent.model->name);
            }
            it.live = false;
            continue;
        }

        if(it.intoGun)
        {
            // Into the gun: to its load point (the first half), then on inside it, carried by the gun; at its size.
            glm::vec3 port, deep, end, centre;
            glm::mat3 gun;
            GunNow g;
            if(!gunNow(it, g) || !gunPath(it, g, port, deep, end) || !gunTurn(it, g, gun))
            {
                it.live = false;
                continue;
            }
            const float u = t * t * (3.f - 2.f * t); // (eased in and out)
            const glm::vec3 local = u < 0.5f ? glm::mix(it.startLocal, port, u / 0.5f)
                                    : u < 0.75f ? glm::mix(port, deep, (u - 0.5f) / 0.25f)
                                                : glm::mix(deep, end, (u - 0.75f) / 0.25f);
            if(!gunPoint(it, g, local, centre))
            {
                it.live = false;
                continue;
            }
            it.axes = gun * it.axesLocal;
            held::anglesFromAxes(it.axes, it.ent.angles, it.ent.model->type == mod_brush);
            it.shrink = 1.f;
            const glm::vec3 origin = centre - it.axes * it.centreLocal;
            for(int i = 0; i < 3; ++i)
            {
                it.ent.origin[i] = origin[i];
            }
            if(vr_debug_collect_fx.value)
            {
                Con_Printf("collect fx: in gun %d%s t %.2f, %.2f off its port: %.2f %.2f %.2f, world %.1f %.1f %.1f\n",
                    it.gunEnt > 0 ? it.gunEnt : it.gunHand, it.gunEnt > 0 ? " (lying)" : "", static_cast<double>(t), static_cast<double>(glm::distance(local, port)),
                    static_cast<double>(local.x), static_cast<double>(local.y), static_cast<double>(local.z),
                    static_cast<double>(centre.x), static_cast<double>(centre.y), static_cast<double>(centre.z));
            }
            if(cl_numvisedicts < MAX_VISEDICTS)
            {
                cl_visedicts[cl_numvisedicts++] = &it.ent;
                it.drawnFrame = host_framecount;
            }
            continue;
        }

        // The target as drawn now (none: shrinks where it was); the start kept from it, so it follows the body.
        glm::vec3 target;
        const bool hasTarget = targetOf(s, it.hotspot, target);
        if(!hasTarget)
        {
            target = it.from;
        }
        if(!it.placed)
        {
            it.offset = it.from - target;
            it.placed = true;
        }

        const float u = t * t; // ease-in
        const glm::vec3 centre = target + it.offset * (1.f - u);
        it.shrink = 1.f + (endSize - 1.f) * u;
        const glm::vec3 origin = centre - it.axes * (it.centreLocal * it.shrink);
        for(int i = 0; i < 3; ++i)
        {
            it.ent.origin[i] = origin[i];
        }
        if(vr_debug_collect_fx.value >= 2)
        {
            Con_Printf("collect fx: t %.2f size %.2f to the target %.1f\n", t, it.shrink, glm::distance(centre, target));
        }
        if(cl_numvisedicts < MAX_VISEDICTS)
        {
            cl_visedicts[cl_numvisedicts++] = &it.ent;
            it.drawnFrame = host_framecount;
        }
    }
}

void clear()
{
    for(Item& it : items)
    {
        it = Item{};
    }
}

const client::EntityVr* entityVr(const entity_t* e)
{
    const Item* it = itemOf(e);
    return it && it->live && it->hasNet ? &it->net : nullptr;
}

float shrink(const entity_t* e)
{
    const Item* it = itemOf(e);
    return it && it->live ? it->shrink : 1.f;
}

entity_t* liveCopy(int i)
{
    if(i < 0 || i >= maxItems)
    {
        return nullptr;
    }
    Item& it = items[static_cast<za::SizeT>(i)];
    return it.live && it.drawnFrame == host_framecount && it.ent.model ? &it.ent : nullptr;
}

int liveCount()
{
    int n = 0;
    for(const Item& it : items)
    {
        n += it.live ? 1 : 0;
    }
    return n;
}

} // namespace qvr::collectfx
