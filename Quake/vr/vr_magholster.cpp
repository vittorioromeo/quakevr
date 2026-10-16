// vr_magholster.cpp -- see vr_magholster.hpp.

#include "vr_magholster.hpp"

#include "vr_body.hpp"
#include "vr_cellcord.hpp"
#include "vr_client.hpp"
#include "vr_coil.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_mem.hpp"
#include "vr_profile.hpp"
#include "vr_protocol.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr::magholster
{
namespace
{

constexpr int numSlots = protocol::numHolsters;
constexpr float lanyardLength = 0.11f; // metres of chain hanging off the bound weapon
constexpr float stubLength = 0.045f;   // and off its holster while it is away
constexpr float tetherSlack = 0.01f;   // the tether's length over the straight line, a share of it
constexpr float tetherExtra = 0.12f;   // metres more close by (its sag in a hand), none from tetherTautAt on
constexpr float tetherTautAt = 2.f;    // metres
constexpr float tetherReach = 6.f;     // metres: the most it pays out (the return distance's when nearer)
constexpr float tetherFade = 0.5f;     // metres past that over which it fades out
const glm::vec3 keyLight = glm::normalize(glm::vec3{0.3f, 0.2f, 1.f});
const glm::vec3 down{0.f, 0.f, -1.f};

// The bind button's presses and releases taken from the game, and what is still to tell the QC.
struct Input
{
    bool stickTaken[2]{};
    bool faceTaken[2]{};
    int pending{0};
};
Input input;

enum class Where
{
    None,    // not seen (another player's hand, a holster not drawn, not sent)
    Holster, // in a holster (its own or another), drawn
    Hand,    // in a hand
    World    // lying about, flying back
};

[[nodiscard]] const char* whereName(Where w)
{
    switch(w)
    {
        case Where::Holster: return "holstered";
        case Where::Hand: return "held";
        case Where::World: return "away";
        default: return "unseen";
    }
}

// The chains of each holster's binding, and the draw's (built once a frame, uploaded once, drawn in both eyes).
enum Chain : int
{
    Lanyard,
    Stub,
    Tether,
    ChainCount
};

struct Slot
{
    int uid{0};
    Where where{Where::None};
    int holster{-1}; // where == Holster: which
    int hand{-1};    // where == Hand: which
    int entity{0};   // where == World: the client entity (kept to find it again at once)
    glm::vec3 gun{0.f};
    glm::vec3 home{0.f};  // the chains' end at the holster (view::holsterPoint)
    glm::vec3 reach{0.f}; // the holster's reach point (body::holsterPositions: where the hand goes, where it flies to)
    glm::vec3 lastRel{0.f}; // the weapon's point off its holster last frame
    coil::Cord cords[ChainCount];
    gfx::TubeBatch batch[ChainCount];
    int sides[ChainCount]{};
    float fade[ChainCount]{1.f, 1.f, 1.f}; // its opacity's share (the tether's past its reach)
};
Slot slots[numSlots];
int builtFrame = -1;
view::ViewEntity worldGun; // a weapon lying about as a view entity (its client entity's copy), for view::modelPoint

struct MagScratch
{
    za::Vector<gfx::TubeRing> rings;
    auto members() { return qvr::mem::list(rings); }
};
mem::Scratch<MagScratch> scratch{"magholster"};

// The QC's QVR_HS_* holster hotspots as body::Holster slots (the holsterweapon<slot> fields' order), -1 not one.
[[nodiscard]] int slotOfHotspot(int hs)
{
    switch(hs)
    {
        case body::HS_LEFT_SHOULDER_HOLSTER: return body::LeftShoulder;
        case body::HS_RIGHT_SHOULDER_HOLSTER: return body::RightShoulder;
        case body::HS_LEFT_HIP_HOLSTER: return body::LeftHip;
        case body::HS_RIGHT_HIP_HOLSTER: return body::RightHip;
        case body::HS_LEFT_UPPER_HOLSTER: return body::LeftUpper;
        case body::HS_RIGHT_UPPER_HOLSTER: return body::RightUpper;
        default: return -1;
    }
}

[[nodiscard]] bool inGame()
{
    return vr_magholster.value != 0.f && sv.active && cls.state == ca_connected && cls.signon == SIGNONS &&
           !cl.intermission && cl.stats[STAT_HEALTH] > 0;
}

// Whether `hand`'s press of the stick (else the lower face button) is the bind button's, at a holster that has a
// weapon in it or one bound (Weapon Mode Immersive).
[[nodiscard]] bool wants(int hand, bool stick)
{
    if(!inGame() || key_dest != key_game || vr_holster_mode.value != 0.f)
    {
        return false;
    }
    const int mode = static_cast<int>(vr_magholster_button.value);
    const bool ours = mode == 3 ? !stick : stick && (mode == 0 || (mode == 1 && hand == HAND_OFF) || (mode == 2 && hand == HAND_MAIN));
    if(!ours)
    {
        return false;
    }
    const hands::State& s = hands::current();
    const int slot = s.valid ? slotOfHotspot(s.hotspot[hand]) : -1;
    return slot >= 0 &&
           (cl.stats[protocol::STAT_QVR_HOLSTERWEAPONUID0 + slot] != 0 || cl.stats[protocol::STAT_QVR_MAGBIND0 + slot] != 0);
}

// The point of a weapon drawn as `ve` the chains hang from: its model's bottom (vr_cellcord.cpp's: where a cell cord
// leaves a gun), as drawn.
[[nodiscard]] glm::vec3 gunPoint(const view::ViewEntity& ve)
{
    return view::modelPoint(ve, cellcord::weaponBottom(ve.ent.model));
}

// Where the weapon with id `uid` is this frame, and its point the chains hang from.
void locate(Slot& c)
{
    c.where = Where::None;
    for(int j = 0; j < numSlots; j++)
    {
        if(cl.stats[protocol::STAT_QVR_HOLSTERWEAPONUID0 + j] == c.uid)
        {
            const entity_t* e = view::holsteredWeapon(j);
            const view::ViewEntity* ve = e && e->model ? view::find(e) : nullptr;
            if(ve)
            {
                c.where = Where::Holster;
                c.holster = j;
                c.gun = gunPoint(*ve);
            }
            return; // (a holster not drawn: unseen)
        }
    }
    for(int h = 0; h < 2; h++)
    {
        if(cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONUID : protocol::STAT_QVR_WEAPONUID2] == c.uid)
        {
            const view::ViewEntity* ve = view::heldWeapon(h);
            if(ve && ve->ent.model)
            {
                c.where = Where::Hand;
                c.hand = h;
                c.gun = gunPoint(*ve);
            }
            return;
        }
    }
    const auto lying = [&](int n) {
        if(n <= 0 || n >= cl.num_entities || n == cl.viewentity)
        {
            return false;
        }
        const entity_t& e = cl_entities[n];
        const client::EntityVr* d = client::entityVr(n);
        return e.model && e.msgtime == cl.mtime[0] && d && d->weaponUid == c.uid;
    };
    if(!lying(c.entity))
    {
        c.entity = 0;
        for(int n = 1; n < cl.num_entities && !c.entity; n++)
        {
            c.entity = lying(n) ? n : 0;
        }
    }
    if(c.entity)
    {
        c.where = Where::World;
        worldGun.ent = cl_entities[c.entity];
        worldGun.netEntity = c.entity;
        c.gun = gunPoint(worldGun);
    }
}

// The flashlight's chain (vr_flashlight.cpp updateCord), its links times vr_magholster_chain_size (at least
// `atLeast`: a long tether's, so that its links reach), dull steel; `length` metres relaxed.
[[nodiscard]] coil::Style chainStyle(float length, float atLeast = 0.f)
{
    const float size = za::max(za::clamp(vr_magholster_chain_size.value, 0.2f, 3.f), atLeast);
    coil::Style style;
    style.length = length;
    style.rigid = true;     // (iron: it hangs its own length, never stretched by its weight nor springing)
    style.airDamping = 3.f; // (a short heavy chain: it settles from a swing in a second or two)
    style.chain = true;
    style.wireRadius = 0.002f * size;
    style.linkLength = 0.016f * size;
    style.linkWidth = 0.008f * size;
    style.albedo = glm::vec3{0.3f, 0.3f, 0.29f};
    style.rust = glm::vec3{0.22f, 0.17f, 0.13f};
    return style;
}

// A chain hanging `length` metres off `at`, carried along by `body`'s movement (its holster's: the body's
// as drawn this frame, as the cell cords' pouch; the player's origin stepped against it, jittering the chain; fixed for a
// weapon lying about: its own movement swings it).
void hang(coil::Cord& cord, const glm::vec3& at, float length, const glm::vec3& body)
{
    const float m2u = units::metresToUnits();
    cord.update(at + down * (length * m2u), down, at, down, chainStyle(length), body, true);
}

// vr_magholster_chain_log: each chain's nodes off its anchor (its second end) the last two frames, for its movement
// and jerk (the second difference: a calm chain's is small, a jittering one's large).
constexpr int logNodes = 16;
struct ChainLog
{
    glm::vec3 rel[2][logNodes]{};
    int frames{0};
};
ChainLog chainLogs[numSlots][ChainCount];
double chainLogTime = -1.0; // (the log's last frame's vr_gametime: only frames the chains moved in)

void logChain(int slot, int k, const coil::Cord& cord, float dtMs)
{
    ChainLog& l = chainLogs[slot][k];
    if(!cord.visible() || cord.nodes().size() > static_cast<size_t>(logNodes))
    {
        l.frames = 0;
        return;
    }
    const za::Vector<glm::vec3>& p = cord.nodes();
    const int n = static_cast<int>(p.size());
    const float mm = 1000.f / units::metresToUnits();
    float move = 0.f, jerk = 0.f;
    glm::vec3 rel[logNodes];
    for(int i = 0; i < n; i++)
    {
        // (Off its anchor: a tether's off the line between its ends, a hanging one's off its second end.)
        const float t = static_cast<float>(i) / static_cast<float>(n - 1);
        rel[i] = p[i] - (k == Tether ? glm::mix(p[0], p[static_cast<size_t>(n - 1)], t) : p[static_cast<size_t>(n - 1)]);
        if(l.frames >= 1)
        {
            move = za::max(move, glm::distance(rel[i], l.rel[0][i]) * mm);
        }
        if(l.frames >= 2)
        {
            jerk = za::max(jerk, glm::length(rel[i] - 2.f * l.rel[0][i] + l.rel[1][i]) * mm);
        }
    }
    for(int i = 0; i < n; i++)
    {
        l.rel[1][i] = l.rel[0][i];
        l.rel[0][i] = rel[i];
    }
    l.frames++;
    Con_Printf("magchain %d %c err %.1f move %.1f jerk %.1f frames %d dt %.1f\n", slot, "LST"[k], 100.f * cord.springError(),
        move, jerk, l.frames, dtMs);
}

void hideSlot(Slot& c)
{
    for(coil::Cord& cord : c.cords)
    {
        cord.hide();
    }
    c.where = Where::None;
}

// vr_magholster_info: each holster's binding, where its weapon is, its chains.
void info_f()
{
    Con_Printf("magnetic holsters: %s, button %g, return %g s / %g m, flight %g s, style %g, size %g, alpha %g\n",
        vr_magholster.value != 0.f ? "on" : "off", vr_magholster_button.value, vr_magholster_return_time.value,
        vr_magholster_return_distance.value, vr_magholster_flight_time.value, vr_magholster_style.value,
        vr_magholster_chain_size.value, vr_magholster_chain_alpha.value);
    const float m2u = units::metresToUnits();
    for(int i = 0; i < numSlots; i++)
    {
        const Slot& c = slots[i];
        const int uid = cl.stats[protocol::STAT_QVR_MAGBIND0 + i];
        if(!uid)
        {
            continue;
        }
        Con_Printf("magholster %d: weapon %d %s, chain end %.1f cm off the reach point", i, uid, whereName(c.where),
            glm::distance(c.home, c.reach) / m2u * 100.f);
        if(c.where == Where::Holster && c.holster == i)
        {
            Con_Printf(", the gun's point %.1f cm off the chain end", glm::distance(c.gun, c.home) / m2u * 100.f);
        }
        if(c.where == Where::Holster)
        {
            Con_Printf(" (holster %d)", c.holster);
        }
        else if(c.where == Where::Hand)
        {
            Con_Printf(" (%s hand)", c.hand == HAND_MAIN ? "main" : "off");
        }
        else if(c.where == Where::World)
        {
            Con_Printf(" (entity %d, %.2f m from its holster)", c.entity, glm::distance(c.gun, c.home) / m2u);
        }
        Con_Printf("; lanyard %s %d links, stub %s, tether %s %d links %.2f m\n", c.cords[Lanyard].visible() ? "on" : "off",
            c.cords[Lanyard].links(), c.cords[Stub].visible() ? "on" : "off", c.cords[Tether].visible() ? "on" : "off",
            c.cords[Tether].links(), c.cords[Tether].length());
    }
}

void return_f()
{
    input.pending |= 4;
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_magholster_info", info_f);
    Cmd_AddCommand("vr_magholster_return", return_f);
}

bool button(int hand, bool HandInput::*which, bool now)
{
    const bool stick = which == &HandInput::stickClick;
    if(!stick && which != &HandInput::primary)
    {
        return false;
    }
    bool& taken = stick ? input.stickTaken[hand] : input.faceTaken[hand];
    if(!now)
    {
        const bool was = taken;
        taken = false;
        return was;
    }
    if(!wants(hand, stick))
    {
        return false;
    }
    taken = true;
    input.pending |= hand == HAND_MAIN ? 2 : 1;
    if(vr_debug_buttons.value)
    {
        Con_Printf("magnetic holsters: %s hand's bind press at holster %d\n", hand == HAND_MAIN ? "main" : "off",
            slotOfHotspot(hands::current().hotspot[hand]));
    }
    return true;
}

int poll()
{
    const int p = input.pending;
    input.pending = 0;
    return p;
}

void setupView(const hands::State& s)
{
    QVR_PROFILE("magnetic holsters");
    const int style = static_cast<int>(vr_magholster_style.value);
    const bool shown = inGame() && s.valid && style >= 0 && style <= 2;
    const bool lanyards = style == 0 || style == 2;
    const bool tethers = style == 1 || style == 2;
    const float m2u = units::metresToUnits();
    body::HolsterPositions homes{};
    bool homesKnown = false;
    for(int i = 0; i < numSlots; i++)
    {
        Slot& c = slots[i];
        const int uid = shown ? cl.stats[protocol::STAT_QVR_MAGBIND0 + i] : 0;
        if(uid != c.uid)
        {
            hideSlot(c);
            c.uid = uid;
            c.entity = 0;
        }
        if(!uid)
        {
            continue;
        }
        if(!homesKnown)
        {
            homes = body::holsterPositions(s);
            homesKnown = true;
        }
        // The chains' end at the holster: the holster as drawn (its loops, on the body), not the hand's reach point,
        // which stands out of it (a hand's length out on the hips: the tether seemed to start in the air).
        c.reach = homes[static_cast<za::SizeT>(i)];
        if(!view::holsterPoint(i, c.home))
        {
            c.home = c.reach;
        }
        const Where was = c.where;
        const int wasHolster = c.holster, wasHand = c.hand, wasEntity = c.entity;
        locate(c);
        if(c.where != was || c.holster != wasHolster || c.hand != wasHand || c.entity != wasEntity)
        {
            c.cords[Lanyard].hide(); // (moved somewhere else: the lanyard starts afresh; the tether follows it there)
            if(vr_magholster_chain_log.value != 0.f)
            {
                Con_Printf("magchain %d now %s: the lanyard afresh\n", i, whereName(c.where));
            }
        }

        // A jump of the weapon about the body (a teleported hand, the drawn gun's pose eased in): the lanyard and the
        // tether afresh, not whirled out.
        const glm::vec3 rel = c.gun - c.home;
        if(c.where != Where::World && glm::distance(rel, c.lastRel) > 0.15f * m2u)
        {
            c.cords[Lanyard].hide();
            c.cords[Tether].hide();
            if(vr_magholster_chain_log.value != 0.f)
            {
                Con_Printf("magchain %d jump %.2f m: the chains afresh\n", i, glm::distance(rel, c.lastRel) / m2u);
            }
        }
        c.lastRel = rel;

        // The lanyard, off the weapon wherever it is seen: swung as a hand moves it about the body; carried whole by a
        // weapon lying about or flying (its few springs would let a thrown one's stretch out behind it).
        if(lanyards && c.where != Where::None)
        {
            hang(c.cords[Lanyard], c.gun, lanyardLength, c.where == Where::World ? c.gun : c.home);
        }
        else
        {
            c.cords[Lanyard].hide();
        }

        // The tether, from the holster to the weapon wherever it is seen away from it (in a hand, another holster, lying
        // about, flying back): paid out of the holster, slack close by, nearly taut far off; faded out past the return
        // distance (vr_magholster_return_distance, else tetherReach) and gone beyond.
        const bool away = c.where != Where::None && !(c.where == Where::Holster && c.holster == i);
        const float metres = glm::distance(c.home, c.gun) / m2u;
        const float reach = vr_magholster_return_distance.value > 0.f ? za::min(vr_magholster_return_distance.value, tetherReach)
                                                                      : tetherReach;
        c.fade[Tether] = za::clamp(1.f - (metres - reach) / tetherFade, 0.f, 1.f);
        const bool tether = tethers && away && c.fade[Tether] > 0.f;
        if(tether)
        {
            const float length = metres * (1.f + tetherSlack) + tetherExtra * za::clamp(1.f - metres / tetherTautAt, 0.f, 1.f);
            // (Its links grow when the most links (vr_coil.cpp: 640) would not reach.)
            const float most = 600.f * 0.016f;
            glm::vec3 toHolster = c.home - c.gun;
            toHolster = glm::length(toHolster) > 1e-3f ? glm::normalize(toHolster) : glm::vec3{0.f, 0.f, 1.f};
            c.cords[Tether].update(c.home, down, c.gun, toHolster, chainStyle(length, length / most), c.home, false);
        }
        else
        {
            c.cords[Tether].hide();
        }

        // The stub at the holster while the weapon is away from it (but where the tether leaves it).
        if(lanyards && away && !tether)
        {
            hang(c.cords[Stub], c.home, stubLength, c.home);
        }
        else
        {
            c.cords[Stub].hide();
        }
    }
    if(vr_magholster_chain_log.value != 0.f && vr_gametime != chainLogTime)
    {
        const float dtMs = chainLogTime < 0.0 ? 0.f : static_cast<float>((vr_gametime - chainLogTime) * 1000.0);
        chainLogTime = vr_gametime;
        for(int i = 0; i < numSlots; i++)
        {
            for(int k = 0; k < ChainCount; k++)
            {
                logChain(i, k, slots[i].cords[k], dtMs);
            }
        }
    }
}

void drawOpaque()
{
    const float alpha = za::clamp(vr_magholster_chain_alpha.value, 0.f, 1.f);
    if(alpha <= 0.f)
    {
        return;
    }
    const bool build = builtFrame != host_framecount;
    builtFrame = host_framecount;
    for(Slot& c : slots)
    {
        for(int k = 0; k < ChainCount; k++)
        {
            coil::Cord& cord = c.cords[k];
            if(!cord.visible())
            {
                continue;
            }
            if(build)
            {
                QVR_PROFILE("magnetic holsters build");
                const hands::State& s = hands::current();
                c.batch[k] = cord.build(0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]), scratch.rings, c.sides[k])
                                 ? gfx::uploadTube(scratch.rings)
                                 : gfx::TubeBatch{};
            }
            if(c.batch[k].count)
            {
                QVR_GPU_PROFILE("magnetic holsters draw");
                gfx::drawTube(c.batch[k], c.sides[k], cord.albedo(), keyLight, cord.rust(), cord.flat(), alpha * c.fade[k]);
            }
        }
    }
}

} // namespace qvr::magholster
