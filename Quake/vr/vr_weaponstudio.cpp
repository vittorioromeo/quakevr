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
    glm::vec3 modelMuzzle{0.f}, barrel{0.f};
    if(qmodel_t* model = weapons::heldModel(hand))
    {
        glm::vec3 butt{0.f}, second{0.f};
        int secondType = 0;
        if(view::weaponLocalGeometry(model, modelMuzzle, butt, second, secondType))
        {
            r.geometry = true;
            const glm::vec3 wm = ms.point(modelMuzzle), wb = ms.point(butt);
            barrel = wm - wb;
            r.realMuzzleCm = toCm(glm::length(wm - muzzle));
        }
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
    if(r.geometry)
    {
        const glm::vec3 wm = ms.point(modelMuzzle);
        addMarker(wm - barrel, wm + barrel, 1.f, 1.f, 1.f, false, "barrel");
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
        float gap = 0.f;
        if(qmodel_t* model = weapons::heldModel(h))
        {
            glm::vec3 mm{0.f}, butt{0.f}, second{0.f};
            int secondType = 0;
            if(view::weaponLocalGeometry(model, mm, butt, second, secondType))
                gap = toCm(glm::length(ms.point(mm) - mount.muzzle));
        }
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
        const float storePitch = -upDeg;                    // Quake's pitch: down is positive
        const float storeYaw = mount.mirrored ? -yawLeft : yawLeft; // as weapons::shotAngles mirrors the off hand's
        Con_Printf("weaponstudio align: %s hand, slot %d: the sight line is %.2f deg from where the gun shoots (shot "
                   "pitch %+.2f, shot yaw %+.2f would line them up); the muzzle it shoots from is %.1f cm off the "
                   "sight line, %.1f cm from the model's own muzzle\n",
                   h ? "main" : "off", mount.slot, angleBetweenDeg(axis, shot), storePitch, storeYaw, gap,
                   st.hand[h].realMuzzleCm);
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

void testVerdict()
{
    st.testing = false;
    st.testClosing = false;
    st.clickPhase = 0;
    testkit::verdict("weaponstudio", st.passed, st.failed);
}

void click(const char* name)
{
    q_strlcpy(st.clickName, name, sizeof(st.clickName));
    st.clickPhase = 1;
}

void feedMock()
{
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
        imgui::feedMouse(st.clickX, st.clickY, false, 0.f);
        st.clickPhase = 2;
        return;
    }
    if(st.clickPhase == 2)
    {
        imgui::feedMouse(st.clickX, st.clickY, true, 0.f);
        st.clickPhase = 3;
        return;
    }
    imgui::feedMouse(st.clickX, st.clickY, false, 0.f);
    st.clickPhase = 0;
}

constexpr int testFrames = 4; // frames a click is given to land and its widget to answer
constexpr int testPoseFrames = 45; // and the arms solve's pose note

void testFrame()
{
    if(!st.testing || st.clickPhase > 0)
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
        st.testStep = 9;
        st.testWait = testPoseFrames;
        break;
    case 9:
        check(st.preview.noteFrame >= 0, "the shipped arms solve reported its pose");
        st.testStep = 10;
        st.testWait = testFrames;
        click("preview_remove");
        break;
    case 10:
        check(!st.preview.made, "the preview humanoid removed");
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
        testVerdict();
        break;
    default:
        testVerdict();
        break;
    }
}

void testClosedFrame()
{
    if(!st.testClosing)
        return;
    if(st.clickPhase > 0)
        return; // (its release is still on its way)
    check(!st.open && !holds(), "Close gave the game back");
    testVerdict();
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
    detail::clearTouched();
    if(!validSlot(st.slot))
        st.slot = validSlot(weapons::heldSlot(1)) ? weapons::heldSlot(1) : 0;
    IN_Activate();
    key_dest = key_game;
    m_state = m_none;
    Con_Printf("weaponstudio: open (slot %d, %d keys, %d slots in use) -- vr_weaponstudio_close gives the game back\n",
               st.slot, detail::tableSize, weapons::slotsUsed());
}

void closeStudio()
{
    if(!st.open)
        return;
    st.open = false;
    st.askClose = false;
    st.clickPhase = 0;
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
    st.clickPhase = 0;
    st.testClosing = false;
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
                   st.hold ? "held (monsters and props stand still; the player still walks and looks)"
                           : "running (nothing is held: the preview humanoid moves)");
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
    Con_Printf("hold [0|1]|reset [slot]|print [slot]|copy [from to]]\n");
    Con_Printf("  the window: Debug > Tools > Weapon Studio, or vr_weaponstudio open (flat screen only)\n");
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
    if(st.clickPhase > 0)
        feedMock();
    if(!imgui::beginFrame())
        return;
    if(SDL_GetRelativeMouseMode())
        SDL_SetRelativeMouseMode(SDL_FALSE);
    if(SDL_ShowCursor(SDL_QUERY) != SDL_ENABLE)
        SDL_ShowCursor(SDL_ENABLE);
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
