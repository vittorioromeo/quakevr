// vr_weaponstudio.cpp -- the Weapon Studio: see vr_weaponstudio.hpp. Here: the key table (every weapons::Key, its page,
// how it is shown and its range), reading and writing the store the way the Weapon Offsets / Weapon Weights menus do
// (a slot's stand-in cvars), the measurement that lines the sights up with where the gun actually shoots, the frozen
// armed humanoid preview, the markers drawn over the game's own view, the console commands, and the tests.
// The window itself is vr_weaponstudio_ui.cpp.

#include "vr_weaponstudio_internal.hpp"

#include "vr_box3d.hpp"
#include "vr_cvars.hpp"
#include "vr_geom.hpp"
#include "vr_imgui.hpp"
#include "vr_progs.hpp"
#include "vr_rigstudio.hpp"
#include "vr_sightalign.hpp"
#include "vr_strings.hpp" // (its startsWith / endsWith: the key-name rules below use them, not a copy of them)
#include "vr_testkit.hpp"
#include "vr_toolgun.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_waveeditor.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <SDL.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

extern "C" int VR_IsActive(void);

using namespace qvr;

namespace qvr::weaponstudio
{

State st;

namespace detail
{
using strings::startsWith; // (the canonical helpers, named once for this whole namespace: vr_strings.hpp)
using strings::endsWith;

namespace
{

constexpr int numKeys = static_cast<int>(weapons::Key::Count);

// A key's page, from its data name (the vr_wofs_<name>_NN name): the first rule whose prefix it starts with wins, so
// the longer prefixes come first ("hand_only_" before "hand_"). `skip` is what the page's own heading already says.
struct Rule
{
    const char* prefix;
    Group group;
    int skip;
};

const Rule rules[] = {
    {"w_", Group::Weights, 2},
    {"reload_", Group::Reload, 7},
    {"em_", Group::Embedded, 3},
    {"embed", Group::Embedded, 0},
    {"bl_", Group::Blade, 3},
    {"hol_", Group::Holstered, 4},
    {"torch_", Group::Flashlight, 6},
    {"wpnbtn", Group::Ammo, 0},
    {"wpntxt", Group::Ammo, 0},
    {"stock_", Group::Stock, 6},
    {"hs", Group::Hotspots, 4},
    {"2h_", Group::TwoHanded, 3},
    {"fgr_", Group::Fingers, 4},
    {"muzzle_", Group::Muzzle, 7},
    {"shot_", Group::Muzzle, 5},
    {"flat_ads_", Group::Muzzle, 9},
    {"ch_", Group::Muzzle, 3},
    {"fx_", Group::Effects, 3},
    {"whole_", Group::Placement, 6},
    {"hand_only_", Group::Placement, 10},
    {"hand_", Group::Placement, 5},
    {"gunoff_", Group::Placement, 7},
};

// The keys with no prefix of their own.
struct Exact
{
    const char* name;
    Group group;
};

const Exact exactNames[] = {
    {"x", Group::Placement},
    {"y", Group::Placement},
    {"z", Group::Placement},
    {"yaw", Group::Placement},
    {"pitch", Group::Placement},
    {"roll", Group::Placement},
    {"scale", Group::Placement},
    {"length", Group::Placement},
    {"overlap", Group::Placement},
    {"hide_hand", Group::Placement},
    {"anygrip_mode", Group::Placement},
    {"inherit", Group::Identity},
    {"id", Group::Identity},
};

[[nodiscard]] bool hasWord(const char* name, const char* word)
{
    return strstr(name, word) != nullptr;
}

const char* const groupNames[] = {
    "Placement", "Fingers", "Muzzle and Sights", "Two-Handed", "Hotspots", "Stock", "Ammo Screen", "Holstered",
    "Effects", "Flashlight", "Weapon Weights", "Blade", "Reloading", "Embedded", "Model and Inheritance", "Other",
};

const char* const kindNames[] = {"units", "degrees", "0..1", "multiplier", "x the default", "units/s", "seconds",
                                 "a choice", "anchor text"};

// A mode/switch key's values, where the game's own menus name them (nullptr: the number is shown, which is the same
// number the console and the config hold).
const char* const hideNames[] = {"no", "yes", nullptr};
const char* const hotspotTypeNames[] = {"none", "grip", "blade", "cup", nullptr};
const char* const onOffNames[] = {"off", "on", nullptr};

[[nodiscard]] const char* const* choiceNames(const char* name)
{
    if(hasWord(name, "hide"))
        return hideNames;
    if(endsWith(name, "_type"))
        return hotspotTypeNames;
    if(endsWith(name, "_aim2h") || endsWith(name, "_sticky") || endsWith(name, "_manual"))
        return onOffNames;
    return nullptr;
}

// How a key is best shown, and its comfortable range: from the name's shape and, for the weights, from the key itself
// (the Weapon Weights page's sliders: so many times the default).
[[nodiscard]] Kind kindOf(const weapons::Key key, const char* name)
{
    if(weapons::anchorKey(key))
        return Kind::Anchor;
    if(endsWith(name, "_pitch") || endsWith(name, "_yaw") || endsWith(name, "_roll") || !strcmp(name, "pitch") ||
       !strcmp(name, "yaw") || !strcmp(name, "roll") || endsWith(name, "_deg"))
        return Kind::Angle;
    if(hasWord(name, "mode") || hasWord(name, "owncol") || endsWith(name, "_type") || endsWith(name, "_style") ||
       endsWith(name, "_manual") || hasWord(name, "hide") || hasWord(name, "aim2h") || !strcmp(name, "2h_blade") ||
       !strcmp(name, "embed") || !strcmp(name, "fx_flash") || !strcmp(name, "fx_tracers"))
        return Kind::Choice;
    if(hasWord(name, "curl") || endsWith(name, "_open") || endsWith(name, "_across") || endsWith(name, "overlap") ||
       endsWith(name, "_sticky") || endsWith(name, "_chance") || !strcmp(name, "w_com") || !strcmp(name, "w_spinalign"))
        return Kind::Share;
    if(!strcmp(name, "scale") || endsWith(name, "_size"))
        return Kind::Scale;
    if(endsWith(name, "_time"))
        return Kind::Seconds;
    if(endsWith(name, "_speed") || endsWith(name, "_min"))
        return Kind::Speed;
    if(weapons::weightKey(key))
    {
        if(!strcmp(name, "w_mass"))
            return Kind::Scale;
        if(endsWith(name, "_dmg") || endsWith(name, "_len"))
            return Kind::Length;
        return Kind::Spring;
    }
    return Kind::Length;
}

void rangeOf(const Kind kind, const char* name, float& lo, float& hi, float& step)
{
    switch(kind)
    {
    case Kind::Angle:
        lo = -180.f;
        hi = 180.f;
        step = 0.25f;
        if(endsWith(name, "_deg"))
        {
            lo = -90.f;
            hi = 90.f;
        }
        break;
    case Kind::Share:
        lo = !strcmp(name, "w_com") || !strcmp(name, "w_spinalign") ? -1.f : 0.f;
        hi = 1.f;
        step = 0.01f;
        break;
    case Kind::Scale:
        lo = 0.02f;
        hi = 10.f;
        step = 0.01f;
        break;
    case Kind::Spring:
        lo = 0.f;
        hi = 20.f;
        step = 0.05f;
        break;
    case Kind::Speed:
        lo = 0.f;
        hi = 1200.f;
        step = 1.f;
        break;
    case Kind::Seconds:
        lo = 0.f;
        hi = 5.f;
        step = 0.005f;
        break;
    case Kind::Choice:
        lo = 0.f;
        hi = 9.f;
        step = 1.f;
        break;
    case Kind::Anchor:
        lo = 0.f;
        hi = 0.f;
        step = 0.f;
        break;
    case Kind::Length:
        lo = -150.f;
        hi = 150.f;
        step = 0.05f;
        if(!strcmp(name, "length"))
        {
            lo = 0.f;
            hi = 200.f;
        }
        else if(endsWith(name, "_bias"))
        {
            lo = -10.f;
            hi = 10.f;
        }
        else if(endsWith(name, "_cm"))
        {
            lo = 0.f;
            hi = 30.f;
        }
        else if(endsWith(name, "_fwd") || endsWith(name, "_up") || endsWith(name, "_out"))
        {
            lo = -40.f;
            hi = 40.f;
        }
        else if(endsWith(name, "_len") || endsWith(name, "_width") || endsWith(name, "_dmg"))
        {
            lo = 0.f;
            hi = 100.f;
        }
        break;
    default:
        break; // (Kind::Count ends the enum: no key has it as its kind)
    }
}

// The page's label: the data name spaced out, shorted of what the page already says.
void makeLabel(Entry& e, const int skip)
{
    const char* n = e.name + static_cast<int>(za::min(static_cast<size_t>(skip), strlen(e.name)));
    int i = 0;
    for(; i < static_cast<int>(sizeof(e.label)) - 1 && n[i]; i++)
        e.label[i] = n[i] == '_' ? ' ' : n[i];
    e.label[i] = 0;
    if(i > 0 && e.label[0] >= 'a' && e.label[0] <= 'z')
        e.label[0] = static_cast<char>(e.label[0] - 'a' + 'A');
}
} // namespace

Entry tableEntries[numKeys];
int tableSize = 0;
bool touchedMap[weapons::numSlots * numKeys];
float savedMap[weapons::numSlots * numKeys]; // what a key held before this window first changed it

void buildTable()
{
    tableSize = 0;
    for(int i = 0; i < numKeys; i++)
    {
        const weapons::Key key = static_cast<weapons::Key>(i);
        const char* name = weapons::keyName(key);
        if(!name || !name[0])
            continue;
        Entry& e = tableEntries[i];
        e = Entry{};
        e.key = key;
        e.holster = -1;
        q_strlcpy(e.name, name, sizeof(e.name));
        e.group = Group::Other;
        int skip = 0;
        for(const Rule& r : rules)
        {
            if(startsWith(name, r.prefix))
            {
                e.group = r.group;
                skip = r.skip;
                break;
            }
        }
        if(e.group == Group::Other)
        {
            for(const Exact& x : exactNames)
            {
                if(!strcmp(name, x.name))
                {
                    e.group = x.group;
                    break;
                }
            }
        }
        if(e.group == Group::Hotspots && name[0] == 'h' && name[1] == 's' && name[2] >= '1' && name[2] <= '4')
            e.hotspot = name[2] - '0';
        if(e.group == Group::Holstered)
        {
            if(startsWith(name, "hol_hip_"))
                e.holster = static_cast<int>(weapons::HolsterKind::Hip);
            else if(startsWith(name, "hol_upper_"))
                e.holster = static_cast<int>(weapons::HolsterKind::Upper);
            else if(startsWith(name, "hol_shoulder_"))
                e.holster = static_cast<int>(weapons::HolsterKind::Shoulder);
        }
        e.kind = kindOf(key, name);
        rangeOf(e.kind, name, e.lo, e.hi, e.step);
        makeLabel(e, skip);
        tableSize++;
    }
}

const Entry* entryOf(const weapons::Key key)
{
    const int i = static_cast<int>(key);
    return i >= 0 && i < numKeys ? &tableEntries[i] : nullptr;
}

int countInGroup(const Group g)
{
    int n = 0;
    for(int i = 0; i < tableSize; i++)
        if(tableEntries[i].group == g)
            n++;
    return n;
}

int uncovered()
{
    return countInGroup(Group::Other);
}

const char* groupLabel(const Group g)
{
    const int i = static_cast<int>(g);
    return i >= 0 && i < static_cast<int>(Group::Count) ? groupNames[i] : "?";
}

const char* kindLabel(const Kind k)
{
    const int i = static_cast<int>(k);
    return i >= 0 && i < static_cast<int>(Kind::Count) ? kindNames[i] : "?";
}

const char* const* choiceNamesFor(const Entry& e)
{
    return choiceNames(e.name);
}

bool matches(const Entry& e, const char* filter)
{
    if(!filter || !filter[0])
        return true;
    const size_t fl = strlen(filter);
    for(size_t at = 0; at + fl <= strlen(e.name); at++)
    {
        size_t i = 0;
        for(; i < fl; i++)
        {
            const char a = static_cast<char>(filter[i] >= 'A' && filter[i] <= 'Z' ? filter[i] - 'A' + 'a' : filter[i]);
            const char b = static_cast<char>(e.name[at + i] >= 'A' && e.name[at + i] <= 'Z' ? e.name[at + i] - 'A' + 'a'
                                                                                            : e.name[at + i]);
            if(a != b)
                break;
        }
        if(i == fl)
            return true;
    }
    return false;
}

bool writeValue(const Entry& e, const int slot, const float value)
{
    cvar_t* c = weapons::cvar(slot, e.key);
    if(!c)
        return false;
    const float was = c->value;
    Cvar_SetValueQuick(c, value);
    markTouched(slot, e.key, was);
    return true;
}

bool writeText(const Entry& e, const int slot, const char* text)
{
    cvar_t* c = weapons::cvar(slot, e.key);
    if(!c || !text)
        return false;
    Cvar_SetQuick(c, text);
    markTouched(slot, e.key, 0.f);
    return true;
}

void markTouched(const int slot, const weapons::Key key, const float before)
{
    if(slot < 0 || slot >= weapons::numSlots)
        return;
    const int at = slot * numKeys + static_cast<int>(key);
    if(touchedMap[at])
        return; // (its first change keeps the value it had then: the window's "it was ..." readout)
    touchedMap[at] = true;
    savedMap[at] = before;
}

bool touched(const int slot, const weapons::Key key)
{
    if(slot < 0 || slot >= weapons::numSlots || static_cast<int>(key) < 0 || static_cast<int>(key) >= numKeys)
        return false;
    return touchedMap[slot * numKeys + static_cast<int>(key)];
}

float savedValue(const int slot, const weapons::Key key)
{
    if(slot < 0 || slot >= weapons::numSlots || static_cast<int>(key) < 0 || static_cast<int>(key) >= numKeys)
        return 0.f;
    return savedMap[slot * numKeys + static_cast<int>(key)];
}

int touchedCount(const int slot)
{
    if(slot < 0 || slot >= weapons::numSlots)
        return 0;
    int n = 0;
    for(int i = 0; i < numKeys; i++)
        if(touchedMap[slot * numKeys + i])
            n++;
    return n;
}

void clearTouched()
{
    for(int i = 0; i < weapons::numSlots * numKeys; i++)
        touchedMap[i] = false;
}
} // namespace detail

namespace
{

// A Quake turn (AngleVectors: the engine's own frame for it) as its forward.
[[nodiscard]] glm::vec3 forwardOf(const glm::vec3& angles)
{
    vec3_t a = {angles.x, angles.y, angles.z}; // (AngleVectors takes the engine's vec3_t, unmodified)
    vec3_t f, r, u;
    AngleVectors(a, f, r, u);
    return geom::vec(f);
}

void basisOf(const glm::vec3& angles, glm::vec3& fwd, glm::vec3& right, glm::vec3& up)
{
    vec3_t a = {angles.x, angles.y, angles.z}; // (AngleVectors takes the engine's vec3_t, unmodified)
    vec3_t f, r, u;
    AngleVectors(a, f, r, u);
    fwd = geom::vec(f);
    right = geom::vec(r);
    up = geom::vec(u);
}

[[nodiscard]] float angleBetweenDeg(const glm::vec3& a, const glm::vec3& b)
{
    const glm::vec3 na = glm::normalize(a), nb = glm::normalize(b);
    const float d = za::clamp(glm::dot(na, nb), -1.f, 1.f);
    return za::atan2(sqrtf(za::max(0.f, 1.f - d * d)), d) * radToDeg; // asin, without asin
}

// Quake units to centimetres (the units the weapon pages work in).
[[nodiscard]] float toCm(const float units)
{
    return units * 100.f / units::perMetre;
}

[[nodiscard]] const char* modelStem(const char* path)
{
    const char* s = path;
    for(const char* c = path; c && *c; c++)
        if(*c == '/')
            s = c + 1;
    return s;
}

[[nodiscard]] bool validSlot(const int slot)
{
    return slot >= 0 && slot < weapons::slotsUsed();
}

[[nodiscard]] const cvar_t* idOf(const int slot)
{
    return weapons::cvar(slot, weapons::Key::ID);
}

void addMarker(const glm::vec3& a, const glm::vec3& b, const float r, const float g, const float bl, const bool point,
               const char* what)
{
    if(st.markers.size() >= st.markers.capacity())
        return;
    Marker m;
    m.a = a;
    m.b = b;
    m.r = r;
    m.g = g;
    m.bl = bl;
    m.point = point;
    q_strlcpy(m.what, what, sizeof(m.what));
    st.markers.pushBack(m);
}

// One hand's held weapon, as the view draws it: the muzzle it shoots from, the model's own muzzle and butt, its sight
// line, its shot direction and its grip spots (all of them the shipped APIs: view::heldWeapon, view::weaponMount,
// view::weaponLocalGeometry, view::ModelSpace, sightalign::sightLine, weapons::shotAngles, weapons::hotspot). Its
// numbers go in st.hand[] and, when the markers are shown, its lines in st.markers.
void measureHand(const hands::State& s, const int hand)
{
    Readout& r = st.hand[hand];
    const bool ran = r.alignRan; // (the Align button's own mark: not this frame's work)
    r = Readout{};
    r.alignRan = ran;
    const view::ViewEntity* ve = view::heldWeapon(hand);
    view::WeaponMount mount;
    if(!ve || !view::weaponMount(hand, mount) || !mount.model)
        return;
    r.held = true;
    r.slot = mount.slot;
    if(const cvar_t* id = idOf(r.slot))
        q_strlcpy(r.model, modelStem(id->string ? id->string : ""), sizeof(r.model));
    const view::ModelSpace ms{*ve};
    const glm::vec3 muzzle = mount.muzzle; // the muzzle the game shoots from
    glm::vec3 modelMuzzle{0.f}, barrel{0.f}, ownMuzzle{0.f};
    bool ownMuzzleOk = false;
    if(qmodel_t* model = weapons::heldModel(hand))
    {
        glm::vec3 butt{0.f}, second{0.f};
        int secondType = 0;
        if(view::weaponLocalGeometry(model, modelMuzzle, butt, second, secondType))
        {
            r.geometry = true;
            // Only its DIRECTION is sound here: a difference of two points cancels ModelSpace's shift, and the
            // per-axis header scale that survives is ~1% (a .mdl's own scale). The POSITION is not:
            // weaponLocalGeometry measures the model drawn at the origin unturned (its header scale and
            // scale_origin already applied), while view::ModelSpace wants a point in the model's own vertices and
            // applies that packing itself -- give it the other one and it re-applies it (the shotgun: a phantom
            // 10 units / 39.8 cm). Muzzle distances therefore use view::muzzleWithOffset, a world anchor like
            // mount.muzzle: with no offset it is the model's own muzzle, before Muzzle X/Y/Z moved it.
            const glm::vec3 wm = ms.point(modelMuzzle), wb = ms.point(butt);
            barrel = wm - wb;
            if(view::muzzleWithOffset(hand, glm::vec3{0.f}, ownMuzzle))
                ownMuzzleOk = true;
            else
                r.geometry = false;
        }
        if(ownMuzzleOk)
            r.realMuzzleCm = toCm(glm::length(ownMuzzle - muzzle));
        if(st.showMarkers && r.slot >= 0)
        {
            for(int i = 1; i <= weapons::maxHotspots; i++) // the grip spots as their sliders place them
            {
                const weapons::Hotspot h = weapons::hotspot(r.slot, i);
                if(h.type == weapons::HotspotType::None)
                    continue;
                addMarker(ms.point(h.pos), glm::vec3{0.f}, 0.3f, 1.f, 0.4f, true, "grip");
            }
        }
    }
    const sightalign::SightLine sl = sightalign::sightLine(mount.model);
    glm::vec3 sights{0.f};
    if(sl.valid)
    {
        r.sights = true;
        sights = ms.point(sl.front) - ms.point(sl.rear);
    }
    const glm::vec3 shot = forwardOf(weapons::shotAngles(mount.rot, r.slot, mount.mirrored));
    r.sightToShot = r.sights ? angleBetweenDeg(sights, shot) : 0.f;
    r.barrelToShot = r.geometry ? angleBetweenDeg(barrel, shot) : 0.f;
    if(r.sights)
    {
        const glm::vec3 d = muzzle - ms.point(sl.rear);
        const glm::vec3 axis = glm::normalize(sights);
        r.muzzleGapCm = toCm(glm::length(d - axis * glm::dot(d, axis)));
    }
    view::WeaponFrame wf;
    if(view::weaponFrame(s, hand, wf) && wf.fistFromRig)
        r.fist = true; // (your hand's fist sits on this weapon through the shipped rig)
    if(!st.showMarkers)
        return;
    addMarker(muzzle, muzzle, 0.2f, 0.9f, 1.f, true, "muzzle");
    addMarker(muzzle, muzzle + glm::normalize(shot) * 60.f, 1.f, 0.15f, 0.15f, false, "shot");
    if(r.geometry && ownMuzzleOk)
    {
        addMarker(ownMuzzle - barrel, ownMuzzle + barrel, 1.f, 1.f, 1.f, false, "barrel");
    }
    if(r.sights)
    {
        const glm::vec3 rear = ms.point(sl.rear);
        addMarker(rear - sights, rear + sights * 2.f, 1.f, 0.9f, 0.1f, false, "sights");
    }
    if(r.fist)
        addMarker(wf.fist, wf.fist, 1.f, 0.5f, 0.2f, true, "fist");
}

// The game's own view (the flatscreen's camera) as a projection, which is what the markers are drawn with. (No second
// camera: this tool shows the weapon exactly as the game draws it.)
struct ViewBasis
{
    glm::vec3 org{0.f};
    glm::vec3 fwd{1.f, 0.f, 0.f};
    glm::vec3 right{0.f, 1.f, 0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    float tanX{1.f};
    float tanY{1.f};
    float w{1.f};
    float h{1.f};
};

ViewBasis viewBasis()
{
    ViewBasis v;
    v.org = {r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    basisOf({r_refdef.viewangles[0], r_refdef.viewangles[1], 0.f}, v.fwd, v.right, v.up);
    v.tanX = tanf(r_refdef.fov_x * 0.5f * degToRad);
    v.tanY = tanf(r_refdef.fov_y * 0.5f * degToRad);
    v.w = ImGui::GetIO().DisplaySize.x;
    v.h = ImGui::GetIO().DisplaySize.y;
    return v;
}

[[nodiscard]] bool projectPoint(const ViewBasis& v, const glm::vec3& p, ImVec2& out)
{
    const glm::vec3 d = p - v.org;
    const float depth = glm::dot(d, v.fwd);
    if(depth < 0.5f || v.tanX <= 0.f || v.tanY <= 0.f)
        return false;
    out.x = (glm::dot(d, v.right) / (depth * v.tanX) * 0.5f + 0.5f) * v.w;
    out.y = (0.5f - glm::dot(d, v.up) / (depth * v.tanY) * 0.5f) * v.h;
    return out.x > -1e4f && out.x < 1e4f && out.y > -1e4f && out.y < 1e4f;
}

// The armed humanoid preview stands where the player can see it, facing them (the Rig Studio's stage, nearer).
constexpr float previewAhead = 70.f; // units

[[nodiscard]] edict_t* previewEdict()
{
    if(st.preview.ent <= 0 || st.preview.ent >= sv.qcvm.num_edicts)
        return nullptr;
    edict_t* e = EDICT_NUM(st.preview.ent);
    return e->free ? nullptr : e;
}
} // namespace

// ---- What the window does with the store ------------------------------------------------------------------------

void resetToDefaults(const int slot)
{
    if(!validSlot(slot))
        return;
    const unsigned before = weapons::settingsGeneration();
    const int changed = detail::touchedCount(slot);
    weapons::resetSlotToDefaults(slot);
    Con_Printf("weaponstudio: slot %d reset to its defaults (%d key(s) this window had changed): %s\n", slot, changed,
               before != weapons::settingsGeneration() ? "the game re-read them" : "nothing had changed");
    detail::clearTouched();
}

void printChanges(const int slot)
{
    if(!validSlot(slot))
        return;
    Con_Printf("weaponstudio: slot %d, %d key(s) changed in this window:\n", slot, detail::touchedCount(slot));
    for(int i = 0; i < detail::tableSize; i++)
    {
        const detail::Entry& e = detail::tableEntries[i];
        if(!detail::touched(slot, e.key))
            continue;
        const cvar_t* c = weapons::cvar(slot, e.key);
        if(!c)
            continue;
        if(e.kind == detail::Kind::Anchor)
            Con_Printf("  %-22s %s\n", e.name, weapons::anchor(slot, e.key));
        else
            Con_Printf("  %-22s %.4f\n", e.name, c->value);
    }
    weapons::printSlot(slot); // (the menu's own print: everything the slot holds, default or not)
}

void copyInto(const int from, const int to)
{
    if(!validSlot(from) || !validSlot(to) || from == to)
    {
        Con_Printf("weaponstudio: copy needs two different slots in use\n");
        return;
    }
    weapons::stopInheriting(to); // its own values, or what it inherits would hide them
    int n = 0;
    for(int i = 0; i < detail::tableSize; i++)
    {
        const detail::Entry& e = detail::tableEntries[i];
        const cvar_t* src = weapons::cvar(from, e.key);
        if(!src)
            continue;
        if(e.kind == detail::Kind::Anchor)
        {
            if(detail::writeText(e, to, weapons::anchor(from, e.key)))
                n++;
        }
        else if(detail::writeValue(e, to, src->value))
            n++;
    }
    Con_Printf("weaponstudio: slot %d copied into slot %d (%d keys; slot %d no longer inherits from anywhere)\n", from,
               to, n, to);
}

// ---- Lining the sights up with where the gun actually shoots -----------------------------------------------------

void alignRun(const int hand, const bool apply)
{
    const int first = hand == 2 ? 0 : za::clamp(hand, 0, 1);
    const int last = hand == 2 ? 1 : first;
    for(int h = first; h <= last; h++)
    {
        const view::ViewEntity* ve = view::heldWeapon(h);
        view::WeaponMount mount;
        if(!ve || !view::weaponMount(h, mount) || !mount.model || mount.slot < 0)
        {
            Con_Printf("weaponstudio align: the %s hand holds no weapon to measure\n", h ? "main" : "off");
            continue;
        }
        st.hand[h].alignRan = true;
        const view::ModelSpace ms{*ve};
        const glm::vec3 shot = forwardOf(weapons::shotAngles(mount.rot, mount.slot, mount.mirrored));
        const sightalign::SightLine sl = sightalign::sightLine(mount.model);
        // The model's own muzzle (view::muzzleWithOffset with no Muzzle X/Y/Z on it) against the muzzle the game
        // shoots from: both world anchors, so their distance is the real one (measureHand: the two frames).
        glm::vec3 own{0.f};
        const float gap = view::muzzleWithOffset(h, glm::vec3{0.f}, own) ? toCm(glm::length(own - mount.muzzle)) : 0.f;
        if(!sl.valid)
        {
            Con_Printf("weaponstudio align: %s hand, slot %d: this model has no sights to line up; its own muzzle is "
                       "%.1f cm from the muzzle the game shoots from (Muzzle X/Y/Z move that one)\n",
                       h ? "main" : "off", mount.slot, gap);
            continue;
        }
        const glm::vec3 rear = ms.point(sl.rear), front = ms.point(sl.front);
        const glm::vec3 axis = glm::normalize(front - rear);
        glm::vec3 aimFwd, aimRight, aimUp;
        basisOf(mount.rot, aimFwd, aimRight, aimUp);
        // The sight axis as the two angles the store holds (the shot's, which the menu's Shot Pitch / Shot Yaw are).
        const float yawLeft = -za::atan2(glm::dot(axis, aimRight), glm::dot(axis, aimFwd)) * radToDeg;
        const float uz = za::clamp(glm::dot(axis, aimUp), -1.f, 1.f);
        const float upDeg = za::atan2(uz, sqrtf(za::max(0.f, 1.f - uz * uz))) * radToDeg;
        const float storePitch = upDeg; // the store's own sign: weapons::shotAngles pitches the shot UP by Shot Pitch
        const float storeYaw = mount.mirrored ? -yawLeft : yawLeft; // as weapons::shotAngles mirrors the off hand's
        const glm::vec3 d = mount.muzzle - rear; // how far the muzzle sits off the line your sights draw
        const float offLine = toCm(glm::length(d - axis * glm::dot(d, axis)));
        Con_Printf("weaponstudio align: %s hand, slot %d: the sight line is %.2f deg from where the gun shoots (shot "
                   "pitch %+.2f, shot yaw %+.2f would line them up); the muzzle it shoots from is %.1f cm off that "
                   "line, %.1f cm from the model's own muzzle (Muzzle X/Y/Z move that one)\n",
                   h ? "main" : "off", mount.slot, angleBetweenDeg(axis, shot), storePitch, storeYaw, offLine, gap);
        if(!apply)
            continue;
        const detail::Entry* pe = detail::entryOf(weapons::Key::ShotPitch);
        const detail::Entry* ye = detail::entryOf(weapons::Key::ShotYaw);
        if(!pe || !ye || !detail::writeValue(*pe, mount.slot, storePitch) || !detail::writeValue(*ye, mount.slot, storeYaw))
        {
            Con_Printf("weaponstudio align: slot %d has no Shot Pitch / Shot Yaw to write\n", mount.slot);
            continue;
        }
        const glm::vec3 again = forwardOf(weapons::shotAngles(mount.rot, mount.slot, mount.mirrored));
        const float left = angleBetweenDeg(axis, again);
        Con_Printf("weaponstudio align: slot %d written: the sight line is now %.2f deg from where it shoots%s\n",
                   mount.slot, left,
                   left < 0.25f ? " (lined up)" : " (still off: check the signs, or move the muzzle instead)");
    }
}

// ---- The preview humanoid: an armed monster spawned frozen, like the Rig Studio's subject ------------------------

void makePreview()
{
    st.wantMake = false;
    if(!sv.active)
    {
        Con_Printf("weaponstudio preview: no map running (a humanoid needs one)\n");
        return;
    }
    removePreview();
    toolgun::armed::refresh();
    const int kinds = toolgun::armed::kindCount();
    if(kinds <= 0)
    {
        Con_Printf("weaponstudio preview: the armed humanoid list is not loaded yet (it comes from QC once a map runs)\n");
        return;
    }
    st.previewKind = za::clamp(st.previewKind, 0, kinds - 1);
    const char* spawn = toolgun::armed::kindSpawn(st.previewKind);
    if(!spawn || !spawn[0])
    {
        Con_Printf("weaponstudio preview: the armed humanoid list has no kind %d to place\n", st.previewKind);
        return;
    }
    int code = st.previewCode;
    int code2 = st.previewCode2;
    if(code2 && !toolgun::armed::pairs(st.previewKind, code, code2))
    {
        Con_Printf("weaponstudio preview: that humanoid does not hold those two weapons (its off hand is left empty)\n");
        code2 = 0;
        st.previewCode2 = 0;
    }
    edict_t* p = progs::playerEdict(); // (the canonical way to client 0's edict: vr_progs.hpp)
    if(!p)
    {
        Con_Printf("weaponstudio preview: there is no player here to stand one in front of\n");
        return;
    }
    const glm::vec3 home{p->v.origin[0], p->v.origin[1], p->v.origin[2]};
    const float yaw = p->v.angles[1];
    const glm::vec3 at = home + glm::vec3{za::cos(yaw * degToRad), za::sin(yaw * degToRad), 0.f} * previewAhead +
                         glm::vec3{0.f, 0.f, 24.f};
    edict_t* e = box3d::spawnClass(spawn, at, glm::vec3{0.f, yaw + 180.f, 0.f}, nullptr, "weapon",
                                   static_cast<float>(code), false, 0.f, "weapon2", static_cast<float>(code2));
    if(!e)
    {
        Con_Printf("weaponstudio preview: %s could not be placed here\n", spawn);
        return;
    }
    const progs::ServerVm vm; // (the client frame runs with no VM current: NUM_FOR_EDICT and the edict's own fields
                              // need the server's pushed, the shipped way: vr_rigstudio.cpp's removeSubject)
    e->v.takedamage = 0; // (the Rig Studio's subject: it is looked at, not fought)
    st.preview.made = true;
    st.preview.ent = NUM_FOR_EDICT(e);
    st.preview.kind = st.previewKind;
    st.preview.code = code;
    st.preview.code2 = code2;
    st.preview.slot = -1;
    st.preview.slotL = -1;
    st.preview.pose[0] = 0;
    st.preview.noteFrame = -1;
    Con_Printf("weaponstudio preview: %s placed at %.0f %.0f %.0f facing you (weapon %d%s)\n", spawn, at.x, at.y, at.z,
               code, code2 ? ", its off hand too" : "");
    if(st.hold)
        Con_Printf("weaponstudio preview: the game's time is held, so its arms solve never runs and its pose stays "
                   "unseen: switch off \"hold the game\" (vr_weaponstudio hold 0) to see how it is held\n");
}

void removePreview()
{
    st.wantRemove = false;
    if(!st.preview.made)
        return;
    if(sv.active)
    {
        const progs::ServerVm vm;
        edict_t* e = previewEdict();
        const func_t fn = progs::findFunction("VR_RigStudio_Remove");
        if(e && fn && st.preview.ent > svs.maxclients)
        {
            // The shipped way a studio's subject goes away (QC/vr_rigseq.qc): its own guns and props first, then it.
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(progs::playerEdict());
            G_INT(OFS_PARM0) = EDICT_TO_PROG(e);
            PR_ExecuteProgram(fn);
            Con_Printf("weaponstudio preview: subject %d removed\n", st.preview.ent);
        }
    }
    st.preview = Preview{};
}

// ---- The markers, over the game's own view ------------------------------------------------------------------------

void drawOverlays()
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const ViewBasis v = viewBasis();
    if(st.showMarkers)
    {
        for(const Marker& m : st.markers)
        {
            const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4{m.r, m.g, m.bl, 1.f});
            if(m.point)
            {
                ImVec2 p;
                if(projectPoint(v, m.a, p))
                {
                    dl->AddCircleFilled(p, 4.f, col);
                    dl->AddText(ImVec2{p.x + 6.f, p.y - 7.f}, col, m.what);
                }
                continue;
            }
            ImVec2 a, b;
            const bool okA = projectPoint(v, m.a, a), okB = projectPoint(v, m.b, b);
            if(!okA && !okB)
                continue;
            if(okA && !okB)
                b = a; // it leaves the screen: drawn as far as it goes
            if(!okA && okB)
                a = b;
            dl->AddLine(a, b, col, 2.f);
            dl->AddText(ImVec2{(a.x + b.x) * 0.5f + 4.f, (a.y + b.y) * 0.5f - 7.f}, col, m.what);
        }
    }
    if(st.showCrosshair)
    {
        const ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4{1.f, 1.f, 1.f, 1.f});
        const float cx = v.w * 0.5f, cy = v.h * 0.5f;
        dl->AddLine(ImVec2{cx - 10.f, cy}, ImVec2{cx + 10.f, cy}, col, 1.f);
        dl->AddLine(ImVec2{cx, cy - 10.f}, ImVec2{cx, cy + 10.f}, col, 1.f);
        dl->AddCircle(ImVec2{cx, cy}, 7.f, col, 12, 1.f);
    }
}

// ---- The tests' mouse, and the test -------------------------------------------------------------------------------

void noteSpot(const char* name)
{
    if(!st.testing)
        return;
    if(st.spots.size() >= st.spots.capacity())
        return;
    Spot sp;
    q_strlcpy(sp.name, name, sizeof(sp.name));
    const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    sp.x = (a.x + b.x) * 0.5f;
    sp.y = (a.y + b.y) * 0.5f;
    st.spots.pushBack(sp);
}

void noteSpotAt(const char* name, const float x, const float y)
{
    // (A spot that is not a widget: the window's own frame -- the middle of its title bar, its X, its corner.)
    if(!st.testing)
        return;
    if(st.spots.size() >= st.spots.capacity())
        return;
    Spot sp;
    q_strlcpy(sp.name, name, sizeof(sp.name));
    sp.x = x;
    sp.y = y;
    st.spots.pushBack(sp);
}

const Spot* spotNamed(const char* name)
{
    for(const Spot& s : st.spots)
        if(!strcmp(s.name, name))
            return &s;
    return nullptr;
}

void check(const bool ok, const char* what)
{
    if(ok)
        st.passed++;
    else
        st.failed++;
    Con_Printf("weaponstudio test: %s: %s\n", what, ok ? "ok" : "FAIL");
}

void checkSched(const bool ok, const char* what)
{
    // (The window's own frame -- pos, size, its X -- counted apart: they get their own verdict line.)
    if(ok)
        st.schedPassed++;
    else
        st.schedFailed++;
    Con_Printf("wsched test: %s: %s\n", what, ok ? "ok" : "FAIL");
}

// (The window has to fit the screen it is drawn on: a widget outside it is drawn past its clip, and no click -- the
// tests' synthetic mouse or a real one -- ever reaches it. Every click the test makes counts here.)
void checkFits()
{
    check(st.clickOffscreen == 0, "the window fits the screen: every widget the test clicked was on it");
}

void testVerdict()
{
    st.testing = false;
    st.testClosing = false;
    st.clickPhase = 0;
    st.showMarkers = false; // (the test's debug markers are not left over the view once it is done)
    testkit::verdict("weaponstudio", st.passed, st.failed);
}

void schedVerdict()
{
    testkit::verdict("wsched", st.schedPassed, st.schedFailed);
}

void click(const char* name)
{
    q_strlcpy(st.clickName, name, sizeof(st.clickName));
    st.dragging = false;
    st.clickDX = 0.f;
    st.clickDY = 0.f;
    st.clickPhase = 1;
}

// (Held, and walked to (dx,dy) before letting go there: what moving a window or resizing it from a corner takes.)
void dragSpot(const char* name, const float dx, const float dy)
{
    q_strlcpy(st.clickName, name, sizeof(st.clickName));
    st.dragging = true;
    st.clickDX = dx;
    st.clickDY = dy;
    st.clickPhase = 1;
}

// (A key, to ImGui: down one frame, up the next. Its window closes on the press.)
void pressEscape()
{
    st.keyPhase = 1;
}

void feedMock()
{
    if(st.keyPhase > 0)
    {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, st.keyPhase == 1);
        st.keyPhase = st.keyPhase == 1 ? 2 : 0;
    }
    if(st.clickPhase <= 0)
        return;
    if(st.clickPhase == 1)
    {
        const Spot* s = spotNamed(st.clickName);
        if(!s)
        {
            check(false, st.clickName);
            Con_Printf("weaponstudio test: no widget named \"%s\" (step %d, page %s, %d widgets this frame): ",
                       st.clickName, st.testStep, detail::groupLabel(st.group), static_cast<int>(st.spots.size()));
            for(int i = 0; i < static_cast<int>(st.spots.size()); i++)
                Con_Printf("%s%s", i ? " " : "", st.spots[i].name);
            Con_Printf("\n");
            st.clickPhase = 0;
            return;
        }
        st.clickX = s->x;
        st.clickY = s->y;
        const ImVec2 ds = ImGui::GetIO().DisplaySize;
        const bool onScreen = s->x >= 0.f && s->x < ds.x && s->y >= 0.f && s->y < ds.y;
        if(!onScreen)
            st.clickOffscreen++;
        Con_Printf("weaponstudio test: click %s at %.0f,%.0f (screen %.0fx%.0f)%s\n", st.clickName, s->x, s->y, ds.x,
                   ds.y, onScreen ? "" : "  OFF SCREEN: the window does not fit this screen");
        imgui::feedMouse(st.clickX, st.clickY, false, 0.f);
        st.clickPhase = 2;
        return;
    }
    // This frame's pointer: a click presses on the spot and lets go there; a drag holds the button down and walks to
    // its delta in two steps, and lets go there.
    float x = st.clickX;
    float y = st.clickY;
    bool down = false;
    int next = 0;
    switch(st.clickPhase)
    {
    case 2:
        down = true;
        next = 3; // (a press is only a click once it lets go: phase 3 is the release)
        break;
    case 3:
        if(!st.dragging)
            break; // (the click: let go on the spot itself)
        x += st.clickDX * 0.5f;
        y += st.clickDY * 0.5f;
        down = true;
        next = 4;
        break;
    case 4:
        x += st.clickDX;
        y += st.clickDY;
        down = true;
        next = 5;
        break;
    default:
        x += st.clickDX;
        y += st.clickDY;
        break; // (5: let go at the end of the drag)
    }
    imgui::feedMouse(x, y, down, 0.f);
    st.clickPhase = next;
}

constexpr int testFrames = 4; // frames a click is given to land and its widget to answer
constexpr int poseWaitMax = 400; // and the most the hold preview waits for the arms solve's note, before it says FAIL

void testFrame()
{
    if(!st.testing || st.clickPhase > 0 || st.keyPhase > 0)
        return;
    if(st.testWait > 0)
    {
        st.testWait--;
        if(st.testWait > 0)
            return;
    }
    using detail::Group;
    switch(st.testStep)
    {
    case 0:
        st.testStep = 1;
        st.testWait = testFrames;
        click("held_main");
        break;
    case 1:
    {
        check(weapons::heldSlot(1) < 0 || st.slot == weapons::heldSlot(1), "the held weapon's slot chosen");
        check(detail::tableSize == static_cast<int>(weapons::Key::Count), "every weapon key has a page");
        check(detail::uncovered() == 0, "none of them left ungrouped");
        int empty = 0;
        for(int g = 0; g < static_cast<int>(Group::Other); g++)
            if(detail::countInGroup(static_cast<Group>(g)) == 0)
                empty++;
        check(empty == 0, "every page has keys in it");
        st.testStep = 2;
        st.testWait = testFrames;
        click("page_muzzle");
        break;
    }
    case 2:
        check(st.group == Group::Muzzle, "the Muzzle and Sights page opens");
        st.testStep = 3;
        st.testWait = testFrames;
        click("page_hotspots");
        break;
    case 3:
        check(st.group == Group::Hotspots, "the Hotspots page opens");
        st.testStep = 4;
        st.testWait = testFrames;
        click("hotspot_3"); // the page's own grip-spot picker
        break;
    case 4:
        check(st.hotspot == 3, "grip spot 3 selected");
        check(st.rowsShown >= 5 && st.rowsShown < detail::countInGroup(Group::Hotspots),
              "only that grip spot's keys and the shared ones are shown");
        q_strlcpy(st.filter, "shot", sizeof(st.filter)); // as the window's own filter box holds it
        st.testStep = 5;
        st.testWait = testFrames;
        click("page_muzzle"); // (the filter's own page: shot_pitch and shot_yaw live on the Muzzle page)
        break;
    case 5:
        check(st.group == Group::Muzzle, "the page chosen by the filter's test is the one it was set on");
        check(st.hotspot == 3, "the grip spot chosen on the Hotspots page is kept");
        check(st.rowsShown == 2, "the filter shows only the keys it matches (shot_pitch, shot_yaw)");
        q_strlcpy(st.filter, "", sizeof(st.filter));
        st.testStep = 6;
        st.testWait = 2;
        break;
    case 6:
    {
        const int slot = validSlot(st.slot) ? st.slot : 0;
        const detail::Entry* e = detail::entryOf(weapons::Key::MuzzleOffsetX);
        const cvar_t* c = weapons::cvar(slot, weapons::Key::MuzzleOffsetX);
        const float was = c ? c->value : 0.f;
        check(e && c && detail::writeValue(*e, slot, was + 1.5f), "a key written through its stand-in cvar");
        check(c && za::fabs(c->value - (was + 1.5f)) < 0.001f, "the store holds what was written");
        check(e && detail::writeValue(*e, slot, was), "and the test puts it back");
        check(detail::touchedCount(slot) >= 1, "the change is marked as changed");
        st.testStep = 7;
        st.testWait = testFrames;
        click("align");
        break;
    }
    case 7:
        check(st.hand[1].held && st.hand[1].slot >= 0, "the main hand's weapon measured");
        check(st.hand[1].alignRan, "the align helper ran on it");
        st.testStep = 8;
        st.testWait = testFrames;
        click("preview_spawn");
        break;
    case 8:
        check(st.preview.made, "an armed humanoid preview placed");
        // Its pose comes from QC's own frame (vr_armedgrunt_pose.qc sends it, QVR_SVC_ARMPOSE): while the window holds
        // the game's time the server does not frame and its arms solve never reports. The test lets them run, and puts
        // the hold back with the preview.
        st.hold = false;
        Con_Printf("weaponstudio test: the game's time let go (a placed humanoid's pose needs the server's frames)\n");
        st.testStep = 9;
        st.poseFrames = 0;
        st.testWait = testFrames;
        break;
    case 9: // the shipped arms solve reports the pose when it next poses this monster (vr_hitreact_skin.cpp's note):
             // the test waits for that note, not for a guessed number of frames
        if(st.preview.noteFrame >= 0)
        {
            Con_Printf("weaponstudio test: its pose \"%s\" (slots %d and %d) after %d frames\n", st.preview.pose,
                       st.preview.slot, st.preview.slotL, st.poseFrames);
            check(true, "the shipped arms solve reported its pose");
            st.testStep = 10;
            st.testWait = testFrames;
            click("preview_remove");
            break;
        }
        if(st.poseFrames >= poseWaitMax)
        {
            Con_Printf("weaponstudio test: no pose note for preview ent %d (kind %d, weapon %d) in %d frames\n",
                       st.preview.ent, st.preview.kind, st.preview.code, st.poseFrames);
            check(false, "the shipped arms solve reported its pose");
            st.testStep = 10;
            st.testWait = testFrames;
            click("preview_remove");
            break;
        }
        st.poseFrames += testFrames;
        st.testWait = testFrames;
        break;
    case 10:
        check(!st.preview.made, "the preview humanoid removed");
        st.hold = true; // (back to the window's own default)
        st.testStep = 11;
        st.testWait = 2;
        break;
    case 11:
        st.testClosing = true; // (the window closes only after its own End, never from inside it)
        st.closeTries = 1;
        if(const Spot* s = spotNamed("close"))
            Con_Printf("weaponstudio test: clicking Close at %.0f,%.0f\n", s->x, s->y);
        click("close");
        st.testStep = 12;
        st.testWait = 4 * testFrames;
        break;
    case 12: // the Close button's watchdog: a window that stays open is the tool's problem, and it is reported rather
             // than waited on (a test ends by its own verdict, never by the harness's timeout)
        if(!st.open)
            break; // (testClosedFrame prints it and gives the verdict)
        if(st.closeTries < 4)
        {
            st.closeTries++;
            const Spot* s = spotNamed("close");
            Con_Printf("weaponstudio test: still open, clicking Close again (try %d, %s, screen %.0fx%.0f)\n",
                       st.closeTries, s ? va("at %.0f,%.0f", s->x, s->y) : "no such widget",
                       ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);
            click("close");
            st.testWait = 4 * testFrames;
            break;
        }
        check(false, "Close gave the game back (its button never took the click)");
        checkFits();
        testVerdict();
        schedVerdict();
        break;
    case 13:
        // Its own frame next, on the same 960x540 screen. Drag its lower-right corner, and drag it well past the
        // window's minimum: one drag, and it shows both that the corner took the drag and that it will not shrink
        // past the size that keeps the top row and the close button reachable.
        Con_Printf("wsched test: dragging its lower-right corner (at %.0f,%.0f) by -500,-320\n", st.winX + st.winW,
                   st.winY + st.winH);
        dragSpot("win_corner", -500.f, -320.f);
        st.testStep = 14;
        st.testWait = 3 * testFrames;
        break;
    case 14:
        Con_Printf("wsched test: its size went %.0fx%.0f -> %.0fx%.0f\n", st.schedW, st.schedH, st.winW, st.winH);
        checkSched(st.winW < st.schedW - 100.f && st.winH < st.schedH - 60.f, "a drag from its corner resizes it");
        checkSched(st.winW >= winMinW - 4.f && st.winH >= winMinH - 4.f,
                   "it will not go smaller than its minimum: its top row and its close stay on it");
        st.schedW = st.winW;
        st.schedH = st.winH;
        Con_Printf("wsched test: dragging its title bar (from %.0f,%.0f) by +150,+120\n", st.winX + st.winW * 0.5f,
                   st.winY);
        dragSpot("title_bar", 150.f, 120.f);
        st.testStep = 15;
        st.testWait = 3 * testFrames;
        break;
    case 15:
        Con_Printf("wsched test: its pos went %.0f,%.0f -> %.0f,%.0f\n", st.schedX, st.schedY, st.winX, st.winY);
        checkSched(st.winX > st.schedX + 60.f && st.winY > st.schedY + 60.f, "a drag on its title bar moves it");
        st.schedX = st.winX;
        st.schedY = st.winY;
        st.testClosing = true; // (Escape closes it the way its own X does: after its End, never from inside it)
        pressEscape();
        st.testStep = 16;
        st.testWait = 4 * testFrames;
        break;
    case 16: // Escape's watchdog, the same shape as Close's
        if(!st.open)
            break; // (testClosedFrame prints it and carries on)
        if(st.closeTries < 3)
        {
            st.closeTries++;
            Con_Printf("wsched test: still open after Escape (try %d; a text box has the keyboard: %s)\n",
                       st.closeTries, ImGui::GetIO().WantTextInput ? "yes" : "no");
            pressEscape();
            st.testWait = 4 * testFrames;
            break;
        }
        checkSched(false, "Escape closes it, with no text box holding the keyboard");
        checkSched(false, "a reopen puts it back where it was left");
        checkSched(false, "its title bar's X closes it");
        testVerdict();
        schedVerdict();
        break;
    case 17:
        // Opened again after Escape: where he left it, at the size he made it.
        Con_Printf("wsched test: reopened at %.0f,%.0f %.0fx%.0f (it was left at %.0f,%.0f %.0fx%.0f)\n", st.winX,
                   st.winY, st.winW, st.winH, st.schedX, st.schedY, st.schedW, st.schedH);
        checkSched(st.winX > st.schedX - 10.f && st.winX < st.schedX + 10.f && st.winY > st.schedY - 10.f &&
                       st.winY < st.schedY + 10.f,
                   "a reopen puts it back where it was left");
        checkSched(st.winW > st.schedW - 10.f && st.winH > st.schedH - 10.f, "and at the size it was left at");
        st.testClosing = true;
        st.closeTries = 1;
        if(const Spot* s = spotNamed("title_x"))
            Con_Printf("wsched test: clicking its title bar's X at %.0f,%.0f\n", s->x, s->y);
        click("title_x");
        st.testStep = 18;
        st.testWait = 4 * testFrames;
        break;
    case 18: // the title bar X's watchdog
        if(!st.open)
            break;
        if(st.closeTries < 4)
        {
            st.closeTries++;
            const Spot* s = spotNamed("title_x");
            Con_Printf("wsched test: still open after its title bar X (try %d, %s, its pos %.0f,%.0f size %.0fx%.0f)\n",
                       st.closeTries, s ? va("at %.0f,%.0f", s->x, s->y) : "no such spot", st.winX, st.winY, st.winW,
                       st.winH);
            click("title_x");
            st.testWait = 4 * testFrames;
            break;
        }
        checkSched(false, "its title bar's X closes it");
        checkSched(false, "its title bar, its corner and its X were all on the screen");
        testVerdict();
        schedVerdict();
        break;
    default:
        testVerdict();
        schedVerdict();
        break;
    }
}

void testClosedFrame()
{
    if(!st.testClosing)
        return;
    if(st.clickPhase > 0)
        return; // (its release is still on its way)
    if(st.testStep == 12)
    {
        check(!st.open && !holds(), "Close gave the game back");
        checkFits();
        // The window's own frame is what the rest of the run is about, on this same 960x540 screen: open it again the
        // way a player does, and carry this test over the reopen.
        st.clickOffscreen = 0;
        openStudio();
        st.testing = true;
        st.testClosing = false;
        st.closeTries = 0;
        st.schedX = st.winX;
        st.schedY = st.winY;
        st.schedW = st.winW;
        st.schedH = st.winH;
        Con_Printf("wsched test: vr_weaponstudio _open brought it back at %.0f,%.0f %.0fx%.0f\n", st.winX, st.winY,
                   st.winW, st.winH);
        st.testStep = 13;
        st.testWait = 2 * testFrames;
        return;
    }
    if(st.testStep == 16)
    {
        checkSched(!st.open && !holds(), "Escape closes it, with no text box holding the keyboard");
        st.clickOffscreen = 0; // (its own clicks count toward its own reachability check, at the end)
        openStudio();
        st.testing = true;
        st.testClosing = false;
        st.closeTries = 0;
        st.testStep = 17;
        st.testWait = 2 * testFrames;
        return;
    }
    if(st.testStep == 18)
    {
        checkSched(!st.open && !holds(), "its title bar's X closes it");
        checkSched(st.clickOffscreen == 0, "its title bar, its corner and its X were all on the screen");
        testVerdict();
        schedVerdict();
        return;
    }
    check(!st.open && !holds(), "Close gave the game back");
    checkFits();
    testVerdict();
    schedVerdict();
}

// ---- The drawn camera: while the window is open, its keys and its mouse ----------------------------------------
// The view flown out of his head, so the weapon he is holding and both hand poses can be seen from outside. The keys
// are the Rig Studio's -- W A S D forward, left, back and right, Q E down and up, Shift 3x -- and so is the rest of
// its camera: a middle-drag or an Alt+left drag orbits, the wheel zooms, and the orbiting and zooming speeds are
// vr_rigstudio_orbit_speed and vr_rigstudio_zoom_speed. Only the flying speed is the Studio's own
// (vr_weaponstudio_fly_speed, in its preferences row). While the window is open these are his move keys' job -- his
// own walking is paused for those frames (VR_AdjustMove, vr_input.cpp) and comes back with the window, nothing kept.
//
// `off` is the default, and what `camera reset` and the window's Close leave behind: while it is off
// VR_WeaponStudioView below never touches the view, so the drawn eye stays the engine's own -- his eyes, his crouch's
// drop, the mode's own nudges included -- and every number the game measures stays exactly as it was.
//
// Out past flatplay's keepNear (vr_flat_gun_near, 8..60: 24 at the defaults) the drawn view stops counting as his own
// eyes, which is the case the flatscreen already handles: the drawn hands, the weapon and the crosshair stay with the
// body (flatplay::viewIsHisOwn / drawnOffset, vr_flatplay.cpp) instead of being dragged along with the camera. The
// Studio's markers and its line-up read-outs are measured from the body's pose (viewFrame, measureHand), never from
// this camera, so they do not move with it either -- only where they land on the screen does.
namespace
{

constexpr float camPitchMax = 89.f;   // the orbit's pitch limit (the Rig Studio's)
constexpr float camDistMax = 4000.f;  // how far the eye may be taken from his head (the Rig Studio's zoom range)
constexpr float camZoomStep = 16.f;   // the wheel's step at the near end, so a zoom from his own eyes still moves it
constexpr float camFast = 3.f;        // Shift: three times the flying speed

// Where his own eyes are and where he looks: what the camera is put on top of, and what "back to my eyes" means.
[[nodiscard]] glm::vec3 hisEyes()
{
    const hands::State& s = hands::current();
    if(s.valid)
        return s.head;
    return {r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
}

[[nodiscard]] glm::vec3 hisTurn()
{
    const hands::State& s = hands::current();
    if(s.valid)
        return {s.headAngles[0], s.headAngles[1], 0.f};
    return {r_refdef.viewangles[0], r_refdef.viewangles[1], 0.f};
}

[[nodiscard]] glm::vec3 viewTurn()
{
    return {r_refdef.viewangles[0], r_refdef.viewangles[1], r_refdef.viewangles[2]};
}

// The eye the view is drawn from this frame: off, the engine's own; on, the one flown out.
[[nodiscard]] glm::vec3 camEye()
{
    if(!st.camOn)
        return {r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    return st.camPos;
}

// Put the eye on top of his own, looking where he looks: where the camera starts.
void camBegin()
{
    st.camOn = true;
    st.camPos = hisEyes();
    st.camTurn = hisTurn();
}

void camBack(const bool announce)
{
    if(!st.camOn)
    {
        if(announce)
            Con_Printf("weaponstudio: the camera is already at his eyes\n");
        return;
    }
    st.camOn = false;
    st.camPos = glm::vec3{0.f};
    st.camTurn = glm::vec3{0.f};
    if(announce)
        Con_Printf("weaponstudio: camera back to his eyes (vr_weaponstudio camera reset)\n");
}

// Not further from his head than the Rig Studio's camera ever goes.
void camKeep()
{
    const glm::vec3 head = hisEyes();
    const glm::vec3 off = st.camPos - head;
    const float d = glm::length(off);
    if(d > camDistMax)
        st.camPos = head + off * (camDistMax / d);
}

// How far the eye stands from his head: 0.00 while the camera is off, and how far it was flown once it is on.
[[nodiscard]] float camOff()
{
    if(!st.camOn)
        return 0.f;
    return glm::length(st.camPos - hisEyes());
}

void camFly(const glm::vec3& move, const bool fast, const float dt)
{
    if(move.x == 0.f && move.y == 0.f && move.z == 0.f)
        return;
    if(!st.camOn)
        camBegin();
    glm::vec3 fwd, right, up;
    basisOf(st.camTurn, fwd, right, up);
    const float speed = za::max(vr_weaponstudio_fly_speed.value, 1.f) * (fast ? camFast : 1.f);
    st.camPos += (fwd * move.x + right * move.y + up * move.z) * (speed * za::min(dt, 0.1f));
    camKeep();
}

// A drag orbits the view: the eye walks the ring it stands on round his head and keeps looking at him, so the weapon
// and both hands stay the thing being looked at. (At his own eyes there is no ring to walk, and there the drag turns
// the view the way the Rig Studio's look does.)
void camOrbit(const glm::vec2& d)
{
    if(!st.camOn)
        camBegin();
    const glm::vec3 head = hisEyes();
    const glm::vec3 r = st.camPos - head;
    const float dist = glm::length(r);
    const float o = vr_rigstudio_orbit_speed.value;
    if(dist < 1.f)
    {
        st.camTurn[1] -= d.x * o;
        st.camTurn[0] = za::clamp(st.camTurn[0] + d.y * o, -camPitchMax, camPitchMax);
        return;
    }
    // The turn it aims with, read off the line from the eye to him (pitch down is positive, as everywhere here; and
    // its asin taken with atan2, as angleBetweenDeg above does).
    const glm::vec3 look = r * (-1.f / dist);
    float yaw = za::atan2(look.y, look.x) * radToDeg;
    float pitch = -za::atan2(look.z, sqrtf(look.x * look.x + look.y * look.y)) * radToDeg;
    yaw -= d.x * o;
    pitch = za::clamp(pitch + d.y * o, -camPitchMax, camPitchMax);
    const glm::vec3 aimed = forwardOf({pitch, yaw, 0.f});
    st.camPos = head - aimed * dist;
    st.camTurn = {pitch, yaw, 0.f};
    camKeep();
}

// The wheel walks the eye along its own line of sight: up brings it nearer what it looks at, down takes it back out.
// The step is a share of the distance, as the Rig Studio's zoom is, with a floor so a zoom from his own eyes moves it.
void camZoom(const float wheel)
{
    if(!st.camOn)
        camBegin();
    glm::vec3 fwd, right, up;
    basisOf(st.camTurn, fwd, right, up);
    const float step = (camOff() + camZoomStep) * za::clamp(vr_rigstudio_zoom_speed.value, 0.01f, 0.9f) * wheel;
    st.camPos -= fwd * step;
    camKeep();
}

void camPrint(const char* state)
{
    const glm::vec3 head = hisEyes();
    const glm::vec3 eye = camEye();
    const glm::vec3 turn = st.camOn ? st.camTurn : viewTurn();
    Con_Printf("weaponstudio camera %s: %s eye %.2f %.2f %.2f turn %.2f %.2f  off his head %.2f  his head %.2f %.2f "
               "%.2f  the game's eye %.2f %.2f %.2f apart %.2f  fly %.0f u/s\n",
               state, st.camOn ? "flown" : "off his eyes", eye.x, eye.y, eye.z, turn.x, turn.y, camOff(),
               head.x, head.y, head.z, r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2],
               glm::length(glm::vec3{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]} - head),
               za::max(vr_weaponstudio_fly_speed.value, 1.f));
}

// The keys and the mouse, while the window is open and no text entry has the keyboard (neither an ImGui input box
// nor the game's own console: key_dest). The keys are read from ImGui and from the game's move buttons both, so a
// hand and a script drive the same camera.
void cameraInput()
{
    ImGuiIO& io = ImGui::GetIO();
    if(st.testing)
        return; // (its own fed mouse is the self-test's business; the camera is not part of it)
    if(key_dest != key_game || io.WantTextInput || io.KeyCtrl)
        return;

    const auto down = [](const ImGuiKey key, const kbutton_t& button) {
        return ImGui::IsKeyDown(key) || (button.state & 1) != 0;
    };
    const glm::vec3 move{(down(ImGuiKey_W, in_forward) ? 1.f : 0.f) - (down(ImGuiKey_S, in_back) ? 1.f : 0.f),
                         (down(ImGuiKey_D, in_moveright) ? 1.f : 0.f) - (down(ImGuiKey_A, in_moveleft) ? 1.f : 0.f),
                         (down(ImGuiKey_E, in_up) ? 1.f : 0.f) - (down(ImGuiKey_Q, in_down) ? 1.f : 0.f)};
    const bool fast = io.KeyShift || (in_speed.state & 1) != 0;
    camFly(move, fast, io.DeltaTime);

    if(io.WantCaptureMouse)
        return; // (its own widgets own the mouse that frame)
    const glm::vec2 d{io.MouseDelta.x, io.MouseDelta.y};
    if(d.x != 0.f || d.y != 0.f)
    {
        if(ImGui::IsMouseDown(ImGuiMouseButton_Middle) || (io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left)))
            camOrbit(d);
    }
    if(io.MouseWheel != 0.f)
        camZoom(io.MouseWheel);
}

// vr_weaponstudio camera [reset | <x> <y> <z> | <pitch> <yaw> | drag <right> <down> | print]: the same camera without
// a hand on the mouse. <x> <y> <z> stands the eye that many units off his own eyes -- ahead, his view's own right,
// and up, the frame vr_flat_cam_fwd, vr_flat_cam_side and vr_flat_cam_up use -- measured where he stands and looks
// now. <pitch> <yaw> is the flown eye's turn, measured off his own look. `drag <right> <down>` is a middle-drag with
// nobody's hand on the mouse: it calls the very orbit a middle-drag calls, with the same pixels. `reset` puts it back
// at his eyes, and `print` -- with no numbers -- says where it is.
void cameraCmd()
{
    if(!st.open)
    {
        Con_Printf("weaponstudio: its camera belongs to the open window -- vr_weaponstudio open first\n");
        return;
    }
    const int args = Cmd_Argc() - 2;
    if(args <= 0 || !strcmp(Cmd_Argv(2), "print"))
    {
        camPrint("is");
        return;
    }
    if(args == 1)
    {
        if(!strcmp(Cmd_Argv(2), "reset"))
        {
            camBack(true);
            return;
        }
        Con_Printf("weaponstudio: camera reset, or three numbers off his eyes (ahead right up), or two (pitch yaw), "
                   "or drag <right> <down>, or print\n");
        return;
    }
    if(!st.camOn)
        camBegin();
    // `drag <right> <down>` is a middle-drag with nobody's hand on the mouse: it calls the very orbit a middle-drag
    // calls, with the same pixels, and prints the turn it came to (a test reads its two prints).
    if(args == 3 && !strcmp(Cmd_Argv(2), "drag"))
    {
        camOrbit(glm::vec2{static_cast<float>(Q_atof(Cmd_Argv(3))), static_cast<float>(Q_atof(Cmd_Argv(4)))});
        camPrint("turned");
        return;
    }
    if(args == 3)
    {
        glm::vec3 fwd, right, up;
        basisOf(hisTurn(), fwd, right, up);
        st.camPos = hisEyes() + fwd * Q_atof(Cmd_Argv(2)) + right * Q_atof(Cmd_Argv(3)) + up * Q_atof(Cmd_Argv(4));
        camKeep();
        camPrint("placed");
        return;
    }
    if(args == 2)
    {
        const glm::vec3 base = hisTurn();
        st.camTurn[0] = za::clamp(base[0] + Q_atof(Cmd_Argv(2)), -camPitchMax, camPitchMax);
        st.camTurn[1] = base[1] + Q_atof(Cmd_Argv(3));
        camPrint("turned");
        return;
    }
    Con_Printf("weaponstudio: camera reset, or three numbers off his eyes (ahead right up), or two (pitch yaw), "
               "or drag <right> <down>, or print\n");
}

} // namespace

// V_CalcRefdef (Quake/view.c): the flown camera over the drawn view, after the mode put its own view there. Off by
// default, and while it is off nothing here runs: the view stays where the game put it.
extern "C" int VR_WeaponStudioView(float* origin, float* angles)
{
    if(!st.open || !st.camOn)
        return 0;
    origin[0] = st.camPos.x;
    origin[1] = st.camPos.y;
    origin[2] = st.camPos.z;
    angles[0] = st.camTurn.x;
    angles[1] = st.camTurn.y;
    angles[2] = 0.f;
    return 1;
}

// ---- Open, close, the console commands ----------------------------------------------------------------------------

void openStudio()
{
    if(!imgui::ensure())
    {
        Con_Printf("weaponstudio: ImGui could not be started\n");
        return;
    }
    st.open = true;
    st.askClose = false;
    st.testing = false;
    st.testClosing = false;
    st.clickPhase = 0;
    st.spots.clear();
    st.markers.clear();
    st.camOn = false; // (`off`: the view stays where the game put it until a key, a drag or the wheel takes it out)
    st.wantCamReset = false;
    detail::clearTouched();
    if(!validSlot(st.slot))
        st.slot = validSlot(weapons::heldSlot(1)) ? weapons::heldSlot(1) : 0;
    IN_Activate();
    key_dest = key_game;
    m_state = m_none;
    Con_Printf("weaponstudio: open (slot %d, %d keys, %d slots in use) -- vr_weaponstudio_close gives the game back\n",
               st.slot, detail::tableSize, weapons::slotsUsed());
    Con_Printf("weaponstudio: its camera is off (his own eyes). W A S D and Q E fly it out, Shift 3x, a middle-drag or "
               "an Alt+left drag orbits it round him, the wheel zooms; his own walking is paused while the window is "
               "open, and vr_weaponstudio camera reset brings the view back without closing it\n");
}

void closeStudio()
{
    if(!st.open)
        return;
    st.open = false;
    st.askClose = false;
    st.clickPhase = 0;
    camBack(false); // (the view goes back to his eyes with the window: nothing of the camera is kept)
    st.wantCamReset = false;
    if(st.keyPhase > 0)
    {
        // (The Escape press we fed and never released: let it go, so ImGui does not keep holding the key down.)
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape, false);
        st.keyPhase = 0;
    }
    st.dragging = false;
    st.markers.clear();
    removePreview();
    imgui::want(false);
    IN_Activate();
    Con_Printf("weaponstudio: closed\n");
}

namespace
{
void startTest()
{
    if(VR_IsActive())
    {
        Con_Printf("weaponstudio: the ImGui test runs on the screen (-flatscreenmode)\n");
        return;
    }
    if(!st.open)
        openStudio();
    st.testing = true;
    st.testStep = 0;
    st.testWait = 2;
    st.passed = 0;
    st.failed = 0;
    st.schedPassed = 0;
    st.schedFailed = 0;
    st.clickPhase = 0;
    st.keyPhase = 0;
    st.dragging = false;
    st.testClosing = false;
    st.clickOffscreen = 0;
    st.showMarkers = true;
    Con_Printf("weaponstudio test: begin (window open, slot %d, %d keys, page %s)\n", st.slot, detail::tableSize,
               detail::groupLabel(st.group));
}

void chooseSlot(const int slot)
{
    if(!validSlot(slot))
    {
        Con_Printf("weaponstudio: slot %d is not in use (0..%d)\n", slot, weapons::slotsUsed() - 1);
        return;
    }
    st.slot = slot;
    Con_Printf("weaponstudio: slot %d\n", slot);
}

void listSlots()
{
    Con_Printf("weaponstudio: %d slots in use, %d keys each\n", weapons::slotsUsed(), detail::tableSize);
    for(int s = 0; s < weapons::slotsUsed(); s++)
    {
        const cvar_t* id = idOf(s);
        const int from = weapons::inheritsFrom(s);
        const bool heldMain = s == weapons::heldSlot(1), heldOff = s == weapons::heldSlot(0);
        Con_Printf("  slot %2d %-24s%s%s%s%s\n", s, id && id->string ? modelStem(id->string) : "?",
                   s == st.slot ? " <- edited" : "", from >= 0 ? " <- from " : "",
                   from >= 0 ? va("%d", from) : "",
                   heldMain ? " [held main]" : heldOff ? " [held off]" : "");
    }
}

void slot_f()
{
    if(Cmd_Argc() > 1)
        chooseSlot(Q_atoi(Cmd_Argv(1)));
    else
        listSlots();
}

// The align line's arguments start at `first`: vr_weaponstudio_align main apply has them at 1, and
// vr_weaponstudio align both apply at 2.
void alignArgs(const int first)
{
    const int argc = Cmd_Argc();
    const char* a = first < argc ? Cmd_Argv(first) : "";
    int hand = 2; // (both hands)
    if(!q_strcasecmp(a, "main") || !q_strcasecmp(a, "right"))
        hand = 1;
    else if(!q_strcasecmp(a, "off") || !q_strcasecmp(a, "left"))
        hand = 0;
    else if(!q_strcasecmp(a, "both"))
        hand = 2;
    const char* b = first + 1 < argc ? Cmd_Argv(first + 1) : "";
    alignRun(hand, !q_strcasecmp(b, "apply"));
}

void align_f()
{
    alignArgs(1);
}

void open_f()
{
    if(VR_IsActive())
    {
        Con_Printf("weaponstudio: it is flat-screen only: start the game with -flatscreenmode\n");
        return;
    }
    if(rigstudio::active())
    {
        Con_Printf("weaponstudio: close the Rig Studio first (vr_rigstudio_close)\n");
        return;
    }
    if(waveeditor::shown() || waveeditor::holds())
    {
        Con_Printf("weaponstudio: close the Arena Wave Editor first (vr_waveeditor_close)\n");
        return;
    }
    if(!st.open)
        openStudio();
}

// vr_weaponstudio <subcommand>: the scripting path, which needs no window.
void command_f()
{
    const int argc = Cmd_Argc();
    const char* sub = argc > 1 ? Cmd_Argv(1) : "";
    if(!sub[0] || !strcmp(sub, "open"))
    {
        open_f();
        return;
    }
    if(!strcmp(sub, "close"))
    {
        closeStudio();
        return;
    }
    if(!strcmp(sub, "test"))
    {
        startTest();
        return;
    }
    if(!strcmp(sub, "align"))
    {
        alignArgs(2);
        return;
    }
    if(!strcmp(sub, "slot"))
    {
        if(argc > 2)
            chooseSlot(Q_atoi(Cmd_Argv(2)));
        else
            listSlots();
        return;
    }
    if(!strcmp(sub, "preview"))
    {
        if(argc > 2)
            st.previewKind = Q_atoi(Cmd_Argv(2));
        if(argc > 3)
            st.previewCode = Q_atoi(Cmd_Argv(3));
        if(argc > 4)
            st.previewCode2 = Q_atoi(Cmd_Argv(4));
        if(argc > 2 || !st.preview.made)
            makePreview();
        else
            removePreview();
        return;
    }
    if(!strcmp(sub, "hold"))
    {
        st.hold = argc > 2 ? Q_atoi(Cmd_Argv(2)) != 0 : !st.hold;
        Con_Printf("weaponstudio: while the window is open the game's physics is %s\n",
                   st.hold ? "held (monsters and props stand still; his own walking is paused too, its keys flying the "
                            "Studio's camera -- vr_weaponstudio camera)"
                           : "running (nothing is held: the preview humanoid moves)");
        return;
    }
    if(!strcmp(sub, "camera"))
    {
        cameraCmd();
        return;
    }
    if(!strcmp(sub, "reset"))
    {
        resetToDefaults(argc > 2 ? Q_atoi(Cmd_Argv(2)) : st.slot);
        return;
    }
    if(!strcmp(sub, "print"))
    {
        printChanges(argc > 2 ? Q_atoi(Cmd_Argv(2)) : st.slot);
        return;
    }
    if(!strcmp(sub, "copy"))
    {
        copyInto(argc > 2 ? Q_atoi(Cmd_Argv(2)) : st.slot, argc > 3 ? Q_atoi(Cmd_Argv(3)) : st.slot);
        return;
    }
    Con_Printf("weaponstudio [open|close|test|slot [n]|preview [kind weapon weapon2]|align [hand 0|1|2 [apply]]|");
    Con_Printf("hold [0|1]|camera [reset|<ahead right up off his eyes>|<pitch yaw off his look>|print]|");
    Con_Printf("reset [slot]|print [slot]|copy [from to]]\n");
    Con_Printf("  the window: Debug > Tools > Weapon Studio, or vr_weaponstudio open (flat screen only)\n");
    Con_Printf("  its camera, while its window is open: W A S D and Q E fly the drawn view out of his eyes (Shift 3x, "
               "vr_weaponstudio_fly_speed), a middle-drag or an Alt+left drag orbits it round him, the wheel zooms "
               "(vr_rigstudio_orbit_speed, vr_rigstudio_zoom_speed)\n");
    Con_Printf("  every key is vr_wofs_<key>_<slot+1> -- the same cvars the Weapon Offsets / Weapon Weights menus use\n");
}

void preview_f()
{
    st.previewKind = Cmd_Argc() > 1 ? Q_atoi(Cmd_Argv(1)) : st.previewKind;
    st.previewCode = Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : st.previewCode;
    st.previewCode2 = Cmd_Argc() > 3 ? Q_atoi(Cmd_Argv(3)) : st.previewCode2;
    makePreview();
}
} // namespace

void afterWindow(const bool keep)
{
    if(!keep || st.askClose)
        closeStudio(); // (after ImGui::End, never from inside the window)
    if(st.wantCamReset)
    {
        st.wantCamReset = false;
        camBack(true); // (the window's "camera back to my eyes", done the same way as its other buttons)
    }
    if(st.wantMake)
        makePreview();
    if(st.wantRemove)
        removePreview();
    if(st.wantAlign)
    {
        st.wantAlign = false;
        alignRun(st.alignHand, st.alignApply);
    }
    if(st.wantReset)
    {
        st.wantReset = false;
        resetToDefaults(st.slot);
    }
    if(st.wantPrint)
    {
        st.wantPrint = false;
        printChanges(st.slot);
    }
    if(st.wantCopy)
    {
        st.wantCopy = false;
        copyInto(st.copyFromSlot, st.slot);
    }
    if(st.wantStopCopy)
    {
        st.wantStopCopy = false;
        const int from = weapons::inheritsFrom(st.slot);
        if(from >= 0)
        {
            weapons::stopInheriting(st.slot);
            Con_Printf("weaponstudio: slot %d holds its own settings now (it copied slot %d)\n", st.slot, from);
        }
    }
}

void clientFrame()
{
    if(!st.open)
    {
        testClosedFrame();
        return;
    }
    imgui::setPanel(0, 0, 0, 1.f); // (the screen only: this tool has no headset panel)
    imgui::want(true);
    if(st.clickPhase > 0 || st.keyPhase > 0)
        feedMock();
    if(!imgui::beginFrame())
        return;
    if(SDL_GetRelativeMouseMode())
        SDL_SetRelativeMouseMode(SDL_FALSE);
    if(SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
        SDL_ShowCursor(SDL_ENABLE);
    cameraInput(); // (its keys and its mouse: the drawn camera, while no text entry has the keyboard)
    testFrame();
    st.spots.clear(); // (the window's widgets, for the tests' mouse: what the last frame offered is already used)
    drawOverlays();
    const bool keep = drawWindow();
    afterWindow(keep);
}

void viewFrame(const hands::State& s)
{
    st.markers.clear(); // (measured again below, from what the view is drawing this frame)
    if(!st.open)
        return;
    measureHand(s, 0);
    measureHand(s, 1);
}

bool wantsNotes()
{
    return st.open && st.preview.made;
}

void note(const posestudio::Note& n)
{
    if(!st.preview.made || n.ent != st.preview.ent)
        return;
    st.preview.slot = n.slot;
    st.preview.slotL = n.slotL;
    st.preview.noteFrame = host_framecount;
    if(n.pose && n.pose[0])
        q_strlcpy(st.preview.pose, n.pose, sizeof(st.preview.pose));
}

bool active()
{
    return st.open;
}

bool holds()
{
    return st.open && st.hold;
}

void init()
{
    detail::buildTable();
    Cmd_AddCommand("vr_weaponstudio", command_f);
    Cmd_AddCommand("vr_weaponstudio_open", open_f);
    Cmd_AddCommand("vr_weaponstudio_close", closeStudio);
    Cmd_AddCommand("vr_weaponstudio_slot", slot_f);
    Cmd_AddCommand("vr_weaponstudio_align", align_f);
    Cmd_AddCommand("vr_weaponstudio_preview", preview_f);
    Cmd_AddCommand("vr_weaponstudio_test", startTest);
}

void shutdown()
{
    if(st.open)
        closeStudio(); // (its preview subject let go, the input grab released; nothing else is ours to free)
    detail::clearTouched();
    st.markers.clear();
    st.spots.clear();
}

} // namespace qvr::weaponstudio
