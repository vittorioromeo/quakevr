// vr_cellcord.cpp -- see vr_cellcord.hpp.

#include "vr_cellcord.hpp"

#include "vr_backend.hpp"
#include "vr_coil.hpp"
#include "vr_cvars.hpp"
#include "vr_gfx.hpp"
#include "vr_mem.hpp"
#include "vr_modelmetadata.hpp"
#include "vr_profile.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"


namespace qvr::cellcord
{
namespace
{

constexpr float flyTime = 0.3f;     // seconds a plug takes back onto a cell (cells again, or another cell)
constexpr float bottomBand = 1.f;   // model units over the lowest vertex the weapon's end is the middle of
constexpr float plugLength = 0.014f; // metres, the plug's rubber boot over the cord's stub
constexpr float plugRadius = 0.0048f;
const glm::vec3 plugAlbedo{0.09f, 0.085f, 0.08f};
const glm::vec3 keyLight = glm::normalize(glm::vec3{0.3f, 0.2f, 1.f});

enum class Weapon
{
    None,
    LaserCannon,
    Hammer,
    SuperAxe
};

[[nodiscard]] const char* weaponName(Weapon w)
{
    switch(w)
    {
        case Weapon::LaserCannon: return "lasercannon";
        case Weapon::Hammer: return "mjolnir";
        case Weapon::SuperAxe: return "superaxe";
        default: return "none";
    }
}

// The corded weapon `model` is, with its cord on (None: not one, or its cord off).
[[nodiscard]] Weapon cordedWeapon(const qmodel_t* model)
{
    if(!model)
    {
        return Weapon::None;
    }
    if(modelmeta::is(model, modelmeta::Id::VLaserg))
    {
        return vr_cellcord_laser.value != 0.f ? Weapon::LaserCannon : Weapon::None;
    }
    if(modelmeta::is(model, modelmeta::Id::VHammer))
    {
        return vr_cellcord_hammer.value != 0.f ? Weapon::Hammer : Weapon::None;
    }
    if(modelmeta::is(model, modelmeta::Id::Mg3SuperAxe) || modelmeta::is(model, modelmeta::Id::Mg3SuperAxeGlow))
    {
        return vr_cellcord_superaxe.value != 0.f ? Weapon::SuperAxe : Weapon::None;
    }
    return Weapon::None;
}

[[nodiscard]] glm::vec3 offsetFor(Weapon w)
{
    switch(w)
    {
        case Weapon::LaserCannon: return {vr_cellcord_laser_x.value, vr_cellcord_laser_y.value, vr_cellcord_laser_z.value};
        case Weapon::Hammer: return {vr_cellcord_hammer_x.value, vr_cellcord_hammer_y.value, vr_cellcord_hammer_z.value};
        case Weapon::SuperAxe:
            return {vr_cellcord_superaxe_x.value, vr_cellcord_superaxe_y.value, vr_cellcord_superaxe_z.value};
        default: return glm::vec3{0.f};
    }
}

// A model's bottom (its model space), read once as it loads: the middle of its frame 0's vertices within bottomBand of
// the lowest. A few models (both hands' weapons, the one before).
struct Bottom
{
    const qmodel_t* model{nullptr};
    const void* data{nullptr};
    glm::vec3 at{0.f};
    float lowest{0.f}, highest{0.f};
};
constexpr int bottomSlots = 4;
Bottom bottoms[bottomSlots];
int nextBottom = 0;

const Bottom& bottomOf(const qmodel_t* model)
{
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(model)));
    for(const Bottom& b : bottoms)
    {
        if(b.model == model && b.data == hdr)
        {
            return b;
        }
    }
    Bottom& b = bottoms[nextBottom];
    nextBottom = (nextBottom + 1) % bottomSlots;
    b = Bottom{};
    b.model = model;
    b.data = hdr;
    if(!hdr || hdr->poseverttype != aliashdr_t::PV_QUAKE1 || hdr->numframes < 1 || hdr->numverts < 1)
    {
        return b;
    }
    const auto* base = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t* pose = base + static_cast<za::SizeT>(hdr->frames[0].firstpose) * hdr->numverts;
    const auto at = [&](const trivertx_t& t) {
        return glm::vec3{t.v[0] * hdr->scale[0] + hdr->scale_origin[0], t.v[1] * hdr->scale[1] + hdr->scale_origin[1],
            t.v[2] * hdr->scale[2] + hdr->scale_origin[2]};
    };
    float lo = 1e9f, hi = -1e9f;
    for(int v = 0; v < hdr->numverts; v++)
    {
        const float z = at(pose[v]).z;
        lo = za::min(lo, z);
        hi = za::max(hi, z);
    }
    glm::vec3 sum{0.f};
    int n = 0;
    for(int v = 0; v < hdr->numverts; v++)
    {
        const glm::vec3 p = at(pose[v]);
        if(p.z <= lo + bottomBand)
        {
            sum += p;
            n++;
        }
    }
    b.at = sum / static_cast<float>(za::max(n, 1));
    b.at.z = lo; // (on its underside, not inside it)
    b.lowest = lo;
    b.highest = hi;
    return b;
}

// A hand's cord.
struct HandCord
{
    coil::Cord cord;
    Weapon weapon{Weapon::None};
    int cell{-1};             // the cell its plug is on (or flying to), -1 loose
    bool flying{false};       // the plug on its way back onto `cell`
    double flyFrom{0.0};      // vr_gametime it set off
    glm::vec3 flyOffset{0.f}; // where it set off from, off the cell's contact (world)
    glm::vec3 gunEnd{0.f};    // the weapon's end this frame (world)
    glm::vec3 contact{0.f};   // the cell's contact this frame (world; loose: the first cell's, the body's point)
    glm::vec3 modelEnd{0.f};  // the weapon's end in its model
    const qmodel_t* model{nullptr};
    // The draw's (built once a frame, uploaded once, drawn in both eyes).
    gfx::TubeBatch rings, plug;
    int sides{0};
    int builtFrame{-1};
};
HandCord cords[2];

struct CellCordScratch
{
    za::Vector<gfx::TubeRing> rings;
    za::Vector<gfx::TubeRing> plug;
    auto members() { return qvr::mem::list(rings, plug); }
};
mem::Scratch<CellCordScratch> scratch{"cellcord"};

[[nodiscard]] float smooth(float t)
{
    return t * t * (3.f - 2.f * t);
}

// The plug's boot over the first end: a short closed tube along the way the cord leaves it.
void buildPlug(const coil::Cord& cord, za::Vector<gfx::TubeRing>& rings)
{
    rings.clear();
    const float m2u = units::metresToUnits();
    const glm::vec3 p = cord.firstEnd();
    const glm::vec3 axis = cord.firstDir();
    const glm::vec3 ref = glm::abs(axis.z) < 0.9f ? glm::vec3{0.f, 0.f, 1.f} : glm::vec3{1.f, 0.f, 0.f};
    const glm::vec3 across = glm::normalize(glm::cross(ref, axis));
    const glm::vec3 light = coil::lightAt(p);
    // (along its length in metres, its radius's share: closed at the contact, a waist and a taper into the wire)
    const float at[5][2] = {{0.f, 0.05f}, {0.0005f, 1.f}, {0.009f, 1.f}, {0.012f, 0.75f}, {plugLength, 0.45f}};
    for(const auto& a : at)
    {
        gfx::TubeRing ring{};
        ring.mid = glm::vec4{p + axis * (a[0] * m2u), plugRadius * m2u * a[1]};
        ring.across = glm::vec4{across, 0.f};
        ring.along = glm::vec4{axis, 0.f};
        ring.ambient = glm::vec4{light, 0.f};
        ring.lamp = glm::vec4{0.f};
        ring.lampDir = glm::vec4{0.f, 0.f, 1.f, 0.f};
        rings.pushBack(ring);
    }
}

void hide(HandCord& c)
{
    c.cord.hide();
    c.weapon = Weapon::None;
    c.cell = -1;
    c.flying = false;
    c.model = nullptr;
}

// vr_cellcord_info: each hand's cord: its weapon, where its ends are (the weapon's bottom, the cell's contact) and where
// the cord's line has them, loose or plugged in, its rings.
void info_f()
{
    Con_Printf("cell cords: laser %g hammer %g superaxe %g, the pouch shows %d cells\n", vr_cellcord_laser.value,
        vr_cellcord_hammer.value, vr_cellcord_superaxe.value, view::ammoPouchCells());
    for(int h = 0; h < 2; h++)
    {
        const HandCord& c = cords[h];
        const char* hand = h == HAND_MAIN ? "main" : "off";
        if(!c.cord.visible())
        {
            const view::ViewEntity* ve = view::heldWeapon(h);
            Con_Printf("cellcord %s: none (holds %s)\n", hand, ve && ve->ent.model ? ve->ent.model->name : "nothing");
            continue;
        }
        const Bottom& b = bottomOf(c.model);
        const glm::vec3 first = c.cord.firstEnd(), second = c.cord.secondEnd();
        // (gunend: the line's end off the weapon's end; plug: its plug off the cell's contact; below: the plug under
        // the weapon's end; all units)
        Con_Printf("cellcord %s: %s %s cell=%d gunend=%.2f plug=%.2f below=%.1f\n", hand, weaponName(c.weapon),
            c.cell < 0 ? "loose" : (c.flying ? "plugging" : "plugged"), c.cell, glm::distance(second, c.gunEnd),
            glm::distance(first, c.contact), c.gunEnd.z - first.z);
        Con_Printf("cellcord %s: its end in the model (%.1f %.1f %.1f), the model's z %.1f..%.1f; line %.2f m, %d rings\n",
            hand, c.modelEnd.x, c.modelEnd.y, c.modelEnd.z, b.lowest, b.highest, c.cord.length(), c.cord.rings());
    }
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_cellcord_info", info_f);
}

void setupView(const hands::State& s)
{
    QVR_PROFILE("cell cords");
    const bool alive = cl.stats[STAT_HEALTH] > 0 && !cl.intermission && s.valid;
    glm::vec3 anyContact, anyUp;
    const bool pouch = view::ammoPouchCell(0, anyContact, anyUp);
    Weapon want[2]{Weapon::None, Weapon::None};
    const view::ViewEntity* ve[2]{nullptr, nullptr};
    for(int h = 0; h < 2; h++)
    {
        ve[h] = alive && pouch ? view::heldWeapon(h) : nullptr;
        want[h] = ve[h] ? cordedWeapon(ve[h]->ent.model) : Weapon::None;
    }
    const bool both = want[0] != Weapon::None && want[1] != Weapon::None;
    const int shown = view::ammoPouchCells();
    const float m2u = units::metresToUnits();
    for(int h = 0; h < 2; h++)
    {
        HandCord& c = cords[h];
        if(want[h] == Weapon::None)
        {
            hide(c);
            continue;
        }
        const qmodel_t* model = ve[h]->ent.model;
        if(c.weapon != want[h] || c.model != model)
        {
            hide(c); // (another weapon: a fresh cord)
        }
        c.weapon = want[h];
        c.model = model;

        // The weapon's end, the cord leaving it downwards (the model's -z).
        c.modelEnd = bottomOf(model).at + offsetFor(c.weapon);
        c.gunEnd = view::modelPoint(*ve[h], c.modelEnd);
        glm::vec3 gunDir = view::modelPoint(*ve[h], c.modelEnd - glm::vec3{0.f, 0.f, 1.f}) - c.gunEnd;
        gunDir = glm::length(gunDir) > 1e-6f ? glm::normalize(gunDir) : glm::vec3{0.f, 0.f, -1.f};

        // Its cell: the first, or with two cords and two or more cells the off hand's the last.
        const int cell = shown <= 0 ? -1 : (both && h == HAND_OFF ? shown - 1 : 0);
        glm::vec3 up;
        if(!view::ammoPouchCell(za::max(cell, 0), c.contact, up))
        {
            hide(c);
            continue;
        }
        const coil::Style style = coil::coiled();
        if(!c.cord.visible())
        {
            c.cell = cell;
            c.flying = false;
            c.cord.update(c.contact, up, c.gunEnd, gunDir, style, c.contact, cell < 0);
            continue;
        }
        if(cell != c.cell)
        {
            c.cell = cell;
            if(cell >= 0)
            {
                // Cells again (or another cell): the plug flies from where it is onto this one.
                c.flying = true;
                c.flyFrom = vr_gametime;
                c.flyOffset = c.cord.firstEnd() - c.contact;
                if(glm::length(c.flyOffset) > 2.f * m2u)
                {
                    c.flyOffset = glm::vec3{0.f}; // (a jump: a teleport)
                }
            }
        }
        if(cell < 0)
        {
            c.flying = false;
            c.cord.update(c.contact, up, c.gunEnd, gunDir, style, c.contact, true);
            continue;
        }
        glm::vec3 pin = c.contact;
        if(c.flying)
        {
            const float t = za::clamp(static_cast<float>(vr_gametime - c.flyFrom) / flyTime, 0.f, 1.f);
            pin = c.contact + c.flyOffset * (1.f - smooth(t));
            c.flying = t < 1.f;
        }
        c.cord.update(pin, up, c.gunEnd, gunDir, style, c.contact, false);
    }
}

void drawOpaque()
{
    for(HandCord& c : cords)
    {
        if(!c.cord.visible())
        {
            continue;
        }
        if(c.builtFrame != host_framecount)
        {
            c.builtFrame = host_framecount;
            QVR_PROFILE("cell cords build");
            const hands::State& s = hands::current();
            c.rings = c.cord.build(0.5f * (s.eyeOrigin[0] + s.eyeOrigin[1]), scratch.rings, c.sides)
                          ? gfx::uploadTube(scratch.rings)
                          : gfx::TubeBatch{};
            buildPlug(c.cord, scratch.plug);
            c.plug = gfx::uploadTube(scratch.plug);
        }
        QVR_GPU_PROFILE("cell cords draw");
        if(c.rings.count)
        {
            gfx::drawTube(c.rings, c.sides, c.cord.albedo(), keyLight, c.cord.rust(), c.cord.flat());
        }
        gfx::drawTube(c.plug, 8, plugAlbedo, keyLight);
    }
}

} // namespace qvr::cellcord
