// vr_grip.cpp -- see vr_grip.hpp.

#include "vr_grip.hpp"
#include "vr_cvars.hpp"
#include "vr_held.hpp"
#include "vr_physics.hpp"
#include "vr_props.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>

using namespace qvr;

namespace qvr::grip
{
namespace
{

HandFrame frames[2];

// The default hand's frame (the right hand; the left mirrored), for a hand not measured: the jointed hand at the shipped
// settings (vr_world_scale 1.25, vr_gunmodelscale 0.7, the shipped hand calibration), measured with vr_grip_frame
// (2026-09-29; the same at any hand pose: it is in the hand's frame).
HandFrame defaultFrame(bool left)
{
    HandFrame f;
    f.palm = {-3.54f, -0.05f, -2.08f};
    f.palmNormal = {0.f, 1.f, 0.f};
    f.channelPoint = {-1.17f, 0.03f, -1.98f};
    f.channelDir = glm::normalize(glm::vec3{0.06f, 0.1f, 0.99f});
    f.channelRadius = 0.28f;
    if(left)
    {
        f.palm.y = -f.palm.y;
        f.palmNormal.y = -f.palmNormal.y;
        f.channelPoint.y = -f.channelPoint.y;
        f.channelDir.y = -f.channelDir.y;
    }
    return f;
}

// The settings of a slot that place it.
struct Settings
{
    Mode mode{Mode::WhereTaken};
    glm::vec3 move{0.f}; // the offset (Fixed: the place), the hand's frame (x forward, y left, z up), mirrored for the left
    glm::vec3 turn{0.f}; // pitch, yaw, roll, degrees, mirrored for the left
    glm::vec3 com{0.f};  // its centre of mass off its drawn middle (its axes)
    glm::vec3 tip{0.f};
    float handleFrom{0.f}, handleTo{0.f}, handleTilt{25.f};
};

Settings settingsOf(int slot, bool left)
{
    using props::Key;
    Settings s;
    const int mode = static_cast<int>(std::lround(props::value(slot, Key::GripMode)));
    s.mode = mode >= 0 && mode <= 3 ? static_cast<Mode>(mode) : Mode::WhereTaken;
    s.move = {props::value(slot, Key::GripX), props::value(slot, Key::GripY), props::value(slot, Key::GripZ)};
    s.turn = {props::value(slot, Key::GripPitch), props::value(slot, Key::GripYaw), props::value(slot, Key::GripRoll)};
    if(left)
    {
        s.move.y = -s.move.y;
        s.turn.y = -s.turn.y;
        s.turn.z = -s.turn.z;
    }
    s.com = {props::value(slot, Key::ComX), props::value(slot, Key::ComY), props::value(slot, Key::ComZ)};
    s.tip = {props::value(slot, Key::TipX), props::value(slot, Key::TipY), props::value(slot, Key::TipZ)};
    s.handleFrom = props::value(slot, Key::HandleFrom);
    s.handleTo = props::value(slot, Key::HandleTo);
    s.handleTilt = std::clamp(props::value(slot, Key::HandleTilt), 0.f, 90.f);
    return s;
}

// The least turn taking the unit vector `a` to the unit vector `b` (a half turn about any axis across them if opposite).
glm::mat3 turnBetween(const glm::vec3& a, const glm::vec3& b)
{
    const float c = std::clamp(glm::dot(a, b), -1.f, 1.f);
    glm::vec3 axis = glm::cross(a, b);
    const float s = glm::length(axis);
    if(s < 1e-6f)
    {
        if(c > 0.f)
        {
            return glm::mat3{1.f};
        }
        axis = glm::cross(a, std::fabs(a.x) < 0.9f ? glm::vec3{1.f, 0.f, 0.f} : glm::vec3{0.f, 1.f, 0.f});
    }
    return glm::mat3_cast(glm::angleAxis(std::atan2(s, c), glm::normalize(axis)));
}

glm::mat3 orthonormal(const glm::mat3& m)
{
    const glm::vec3 x = glm::normalize(m[0]);
    const glm::vec3 y = glm::normalize(m[1] - x * glm::dot(x, m[1]));
    return glm::mat3{x, y, glm::cross(x, y)};
}

glm::vec3 middleOf(const Prop& prop)
{
    return (prop.lo + prop.hi) * 0.5f;
}

// The farthest the prop (placed at `rot`, its middle at `centre`) reaches from its middle along `dir`.
float reach(const Prop& prop, const glm::mat3& rot, const glm::vec3& centre, const glm::vec3& dir)
{
    const glm::vec3 d = glm::transpose(rot) * dir; // in its axes
    float most = 0.f;
    if(!prop.vertices.empty())
    {
        for(const glm::vec3& v : prop.vertices)
        {
            most = std::max(most, glm::dot(v - centre, d));
        }
        return most;
    }
    for(int i = 0; i < 8; i++)
    {
        const glm::vec3 v{(i & 1) ? prop.hi.x : prop.lo.x, (i & 2) ? prop.hi.y : prop.lo.y, (i & 4) ? prop.hi.z : prop.lo.z};
        most = std::max(most, glm::dot(v - centre, d));
    }
    return most;
}

// Along the Handle (vr_grip.hpp): the handle through the fist's grip channel.
Place handlePlace(const Prop& prop, const Settings& s, const HandFrame& frame, const Place& taken)
{
    const glm::vec3 size = prop.hi - prop.lo;
    const int k = size.x >= size.y && size.x >= size.z ? 0 : size.y >= size.z ? 1 : 2; // its long axis
    const glm::vec3 mid = middleOf(prop);
    // Its head: the end its weight is towards (a torch's), else its melee tip's, else +.
    float head = 1.f;
    if(std::fabs(s.com[k]) > 1e-3f)
    {
        head = s.com[k] > 0.f ? 1.f : -1.f;
    }
    else if(s.tip != glm::vec3{0.f} && std::fabs(s.tip[k] - mid[k]) > 1e-3f)
    {
        head = s.tip[k] > mid[k] ? 1.f : -1.f;
    }
    // The handle: Handle From .. To along the long axis (its model's units from its origin); both 0: the butt's half,
    // short of its end (12% to 45% of its length from the butt).
    float from = std::min(s.handleFrom, s.handleTo), to = std::max(s.handleFrom, s.handleTo);
    if(s.handleFrom == 0.f && s.handleTo == 0.f)
    {
        const float len = size[k];
        const float butt = head > 0.f ? prop.lo[k] : prop.hi[k];
        from = butt + head * 0.12f * len;
        to = butt + head * 0.45f * len;
        if(from > to)
        {
            std::swap(from, to);
        }
    }

    // The point of the handle the fist is at: where the grip channel's point is along the long axis as taken, kept on
    // the handle.
    const glm::vec3 cp = frame.channelPoint;
    const glm::vec3 local = glm::transpose(taken.rot) * (cp - taken.pos);
    glm::vec3 held = mid;
    held[k] = std::clamp(local[k], from, to);

    // Its long axis along the channel: its head towards the index finger unless it was taken clearly the other way
    // round (within 60 degrees of the little finger's side), leaning off the channel as taken, at most Handle Tilt.
    glm::vec3 axis{0.f};
    axis[k] = head;
    const glm::vec3 a0 = glm::normalize(taken.rot * axis);
    const glm::vec3 cd = glm::normalize(frame.channelDir);
    const glm::vec3 u = glm::dot(a0, cd) >= -0.5f ? cd : -cd;
    glm::vec3 a1 = a0;
    const float lean = std::acos(std::clamp(glm::dot(a0, u), -1.f, 1.f));
    const float most = glm::radians(s.handleTilt);
    if(lean > most)
    {
        const glm::vec3 across = glm::cross(u, a0);
        a1 = glm::length(across) > 1e-5f ? glm::mat3_cast(glm::angleAxis(most, glm::normalize(across))) * u : u;
    }
    Place p;
    p.rot = orthonormal(turnBetween(a0, glm::normalize(a1)) * taken.rot);
    p.pos = cp - p.rot * held;
    return p;
}

} // namespace

void setHandFrame(int hand, const HandFrame& f)
{
    if(hand == 0 || hand == 1)
    {
        frames[hand] = f;
    }
}

HandFrame handFrame(int hand, bool left, bool local)
{
    if(local && (hand == 0 || hand == 1) && frames[hand].measured)
    {
        return frames[hand];
    }
    return defaultFrame(left);
}

Place palmPlace(const Prop& prop, const glm::vec3& centre, const HandFrame& frame, const Place& taken)
{
    const glm::vec3 n = glm::normalize(frame.palmNormal);
    // The face nearest the palm flat on it: of its axes, the one nearest the palm's normal (either way) turned onto it,
    // the least (so its turn about the normal, how it was taken, is kept). Never its longest (a long stone lies in the
    // palm, it doesn't stand out of it past the fingers).
    const glm::vec3 size = prop.hi - prop.lo;
    const int longest = size.x >= size.y && size.x >= size.z ? 0 : size.y >= size.z ? 1 : 2;
    int best = longest == 0 ? 1 : 0;
    float bestDot = -1.f;
    for(int i = 0; i < 3; i++)
    {
        if(i == longest)
        {
            continue;
        }
        const float d = std::fabs(glm::dot(glm::normalize(taken.rot[i]), n));
        if(d > bestDot)
        {
            bestDot = d;
            best = i;
        }
    }
    const glm::vec3 a = glm::normalize(taken.rot[best]);
    Place p;
    p.rot = orthonormal(turnBetween(a, glm::dot(a, n) >= 0.f ? n : -n) * taken.rot);
    // Its middle over the grip channel (where the fingers curl round what they hold: the palm's middle is nearer the
    // wrist, and a stone there is out of the fingers' reach), on the palm's skin, as far out as it reaches back towards
    // the palm.
    const glm::vec3 over = frame.channelPoint - n * glm::dot(frame.channelPoint - frame.palm, n);
    const glm::vec3 c = over + n * reach(prop, p.rot, centre, -n);
    p.pos = c - p.rot * centre;
    return p;
}

Place place(const Prop& prop, const HandFrame& frame, bool left, const Place& taken, Mode* modeOut, glm::vec3* pivotOut)
{
    const Settings s = settingsOf(prop.slot, left);
    if(modeOut)
    {
        *modeOut = s.mode;
    }
    if(s.mode == Mode::Fixed)
    {
        // Its origin at Grip X/Y/Z, turned by Grip Pitch/Yaw/Roll about it (pitch up, yaw left, roll right, whatever the
        // model: a brush model's pitch was the other way round before vr_props_version 40).
        const float angles[3]{-s.turn.x, s.turn.y, s.turn.z};
        if(pivotOut)
        {
            *pivotOut = s.move;
        }
        return {s.move, held::axesFromAngles(angles, true)};
    }
    const glm::vec3 centre = middleOf(prop) + s.com;
    Place base = taken;
    glm::vec3 pivot = frame.palm;
    if(s.mode == Mode::Palm)
    {
        base = palmPlace(prop, centre, frame, taken);
        pivot = base.pos + base.rot * centre;
    }
    else if(s.mode == Mode::Handle)
    {
        base = handlePlace(prop, s, frame, taken);
        pivot = frame.channelPoint;
    }
    if(pivotOut)
    {
        *pivotOut = pivot;
    }
    // The offset: turned about the pivot (pitch up, yaw left, roll right: as view angles, the pitch the other way), then
    // moved.
    const float angles[3]{-s.turn.x, s.turn.y, s.turn.z};
    const glm::mat3 m = held::axesFromAngles(angles, true);
    return {pivot + m * (base.pos - pivot) + s.move, orthonormal(m * base.rot)};
}

namespace
{

// Server side: what each held prop was placed from (by entity).
struct Held
{
    const qmodel_t* model{nullptr};
    int hand{0};
    bool left{false};
    bool local{false};
    HandFrame frame;
    Prop prop;
    Place taken;
    bool kept{false};         // taken where it was at a regrip (serverKeep): placed as taken while its mode is still:
    Mode keptMode{Mode::WhereTaken};
    unsigned generation{0};
    bool turnStale{false};    // placed again by the client (serverPlace): its turn to be set at the next frame
    Place now;
    glm::vec3 lastOffset{0.f}; // .carry_offset as last given to QC (forward, right, up)
    bool pouch{false};         // a hand grenade taken from the pouch (serverFromPouch): turned by vr_grenade_pouch_hold_*
    glm::vec3 pouchTurn{0.f};  // the turn it was placed with (pitch, yaw, roll; mirrored for the left hand)
};

std::unordered_map<int, Held> heldProps;

// The grenade pouch's turn in the hand (vr_grenade_pouch_hold_*: pitch up, yaw left, roll right, degrees), mirrored for
// the left hand as the grip's offsets are.
[[nodiscard]] glm::vec3 pouchTurnNow(bool left)
{
    glm::vec3 t{vr_grenade_pouch_hold_pitch.value, vr_grenade_pouch_hold_yaw.value, vr_grenade_pouch_hold_roll.value};
    if(left)
    {
        t.y = -t.y;
        t.z = -t.z;
    }
    return t;
}

// Whether `h` is to be placed again: its prop's settings changed, or (from the pouch) the pouch's turn in the hand.
[[nodiscard]] bool stale(const Held& h)
{
    return h.generation != props::settingsGeneration() || (h.pouch && h.pouchTurn != pouchTurnNow(h.left));
}

[[nodiscard]] const qmodel_t* modelOf(edict_t* e)
{
    const int index = static_cast<int>(e->v.modelindex);
    return index > 0 && index < MAX_MODELS ? sv.models[index] : nullptr;
}

[[nodiscard]] glm::vec3 toFRU(const glm::vec3& p)
{
    return {p.x, -p.y, p.z};
}

void placeNow(Held& h)
{
    const int mode = static_cast<int>(std::lround(props::value(h.prop.slot, props::Key::GripMode)));
    if(h.kept && mode == static_cast<int>(h.keptMode) && h.keptMode != Mode::Fixed)
    {
        // Held where a regrip left it: its offset on top of that, about the same pivot.
        Place pretend = h.taken;
        const Settings s = settingsOf(h.prop.slot, h.left);
        glm::vec3 pivot = h.frame.palm;
        if(s.mode == Mode::Palm)
        {
            pivot = h.taken.pos + h.taken.rot * (middleOf(h.prop) + s.com);
        }
        else if(s.mode == Mode::Handle)
        {
            pivot = h.frame.channelPoint;
        }
        const float angles[3]{-s.turn.x, s.turn.y, s.turn.z};
        const glm::mat3 m = held::axesFromAngles(angles, true);
        h.now = {pivot + m * (pretend.pos - pivot) + s.move, orthonormal(m * pretend.rot)};
    }
    else
    {
        h.kept = false;
        h.now = place(h.prop, h.frame, h.left, h.taken);
        if(h.pouch)
        {
            // From the pouch: turned on top of all that, about its middle (vr_grenade_pouch_hold_*).
            h.pouchTurn = pouchTurnNow(h.left);
            const float angles[3]{-h.pouchTurn.x, h.pouchTurn.y, h.pouchTurn.z};
            const glm::mat3 m = held::axesFromAngles(angles, true);
            const glm::vec3 mid = h.now.pos + h.now.rot * (middleOf(h.prop) + settingsOf(h.prop.slot, h.left).com);
            h.now = {mid + m * (h.now.pos - mid), orthonormal(m * h.now.rot)};
        }
    }
    h.generation = props::settingsGeneration();
}

void logPlace(const char* what, int num, const Held& h)
{
    if(!developer.value && !vr_debug_carry.value)
    {
        return;
    }
    const int mode = static_cast<int>(std::lround(props::value(h.prop.slot, props::Key::GripMode)));
    const glm::vec3 a = h.now.rot[0], u = h.now.rot[2];
    Con_Printf("grip: %s %d %s (mode %d, %s hand%s): taken at %.2f %.2f %.2f, held at %.2f %.2f %.2f, its x %.2f %.2f %.2f, "
               "its z %.2f %.2f %.2f (hand frame: forward, left, up; generation %u, frame %d)\n",
        what, num, h.model ? h.model->name : "?", mode, h.left ? "left" : "right", h.local ? ", measured" : "", h.taken.pos.x,
        h.taken.pos.y, h.taken.pos.z, h.now.pos.x, h.now.pos.y, h.now.pos.z, a.x, a.y, a.z, u.x, u.y, u.z, h.generation,
        host_framecount);
}

} // namespace

glm::vec3 serverTake(edict_t* e, edict_t* player, int hand, bool left, const float* handAngles, const glm::vec3& offset)
{
    const qmodel_t* model = modelOf(e);
    const int num = NUM_FOR_EDICT(e);
    Held h;
    h.model = model;
    h.hand = hand;
    h.left = left;
    h.local = cls.state != ca_dedicated && player && NUM_FOR_EDICT(player) == 1;
    h.frame = handFrame(hand, left, h.local);
    h.prop.brush = model && model->type == mod_brush;
    h.prop.slot = model ? props::slotForModel(model) : -1;
    if(held::drawnVertices(e, h.prop.vertices) && !h.prop.vertices.empty())
    {
        h.prop.lo = glm::vec3{1e9f};
        h.prop.hi = glm::vec3{-1e9f};
        for(const glm::vec3& v : h.prop.vertices)
        {
            h.prop.lo = glm::min(h.prop.lo, v);
            h.prop.hi = glm::max(h.prop.hi, v);
        }
    }
    else
    {
        h.prop.vertices.clear();
        h.prop.lo = glm::vec3{e->v.mins[0], e->v.mins[1], e->v.mins[2]};
        h.prop.hi = glm::vec3{e->v.maxs[0], e->v.maxs[1], e->v.maxs[2]};
    }
    const glm::mat3 handAxes = held::axesFromAngles(handAngles, true);
    h.taken.pos = {offset.x, -offset.y, offset.z};
    h.taken.rot = glm::transpose(handAxes) * held::axesFromAngles(e->v.angles, h.prop.brush);
    placeNow(h);
    physics::setCarryTurn(e, handAngles, h.now.rot);
    h.lastOffset = toFRU(h.now.pos);
    logPlace("taken", num, h);
    const glm::vec3 out = h.lastOffset;
    heldProps[num] = std::move(h);
    return out;
}

glm::vec3 serverFrame(edict_t* e, const float* handAngles, const glm::vec3& offset)
{
    const int num = NUM_FOR_EDICT(e);
    const auto it = heldProps.find(num);
    // None, another model, or its place in the hand set by something else since (QC): as QC has it.
    if(it == heldProps.end() || it->second.model != modelOf(e) || glm::any(glm::greaterThan(glm::abs(offset - it->second.lastOffset), glm::vec3{1e-3f})))
    {
        if(it != heldProps.end())
        {
            heldProps.erase(it);
        }
        physics::carryAngles(e, handAngles, false, e->v.angles);
        return offset;
    }
    Held& h = it->second;
    if(stale(h))
    {
        placeNow(h);
        h.turnStale = true;
        logPlace("placed again", num, h);
    }
    if(h.turnStale)
    {
        physics::setCarryTurn(e, handAngles, h.now.rot);
        h.turnStale = false;
    }
    else
    {
        physics::carryAngles(e, handAngles, false, e->v.angles);
    }
    h.lastOffset = toFRU(h.now.pos);
    return h.lastOffset;
}

void serverKeep(edict_t* e, const float* handAngles, const glm::vec3& offset)
{
    const int num = NUM_FOR_EDICT(e);
    const auto it = heldProps.find(num);
    if(it == heldProps.end() || it->second.model != modelOf(e))
    {
        return;
    }
    Held& h = it->second;
    h.pouch = false; // (regripped: held where it is, the pouch's turn in it)
    const Settings s = settingsOf(h.prop.slot, h.left);
    const Place current{{offset.x, -offset.y, offset.z},
        glm::transpose(held::axesFromAngles(handAngles, true)) * held::axesFromAngles(e->v.angles, h.prop.brush)};
    // What it was taken at: where it is, its offset taken off (placeNow puts it back on).
    glm::vec3 pivot = h.frame.palm;
    if(s.mode == Mode::Palm)
    {
        pivot = current.pos + current.rot * (middleOf(h.prop) + s.com) - s.move;
    }
    else if(s.mode == Mode::Handle)
    {
        pivot = h.frame.channelPoint;
    }
    const float angles[3]{-s.turn.x, s.turn.y, s.turn.z};
    const glm::mat3 mt = glm::transpose(held::axesFromAngles(angles, true));
    h.taken = {pivot + mt * (current.pos - s.move - pivot), orthonormal(mt * current.rot)};
    h.kept = true;
    h.keptMode = s.mode;
    if(s.mode == Mode::Fixed)
    {
        h.now = current; // until a setting changes
        h.generation = props::settingsGeneration();
    }
    else
    {
        placeNow(h);
    }
    h.turnStale = false;
    h.lastOffset = offset;
    logPlace("kept", num, h);
}

glm::vec3 serverFromPouch(edict_t* e, const float* handAngles, const glm::vec3& offset)
{
    const int num = NUM_FOR_EDICT(e);
    const auto it = heldProps.find(num);
    if(it == heldProps.end() || it->second.model != modelOf(e))
    {
        return offset;
    }
    Held& h = it->second;
    h.pouch = true;
    placeNow(h);
    physics::setCarryTurn(e, handAngles, h.now.rot);
    h.turnStale = false;
    h.lastOffset = toFRU(h.now.pos);
    logPlace("from the pouch", num, h);
    return h.lastOffset;
}

void forget(int num)
{
    heldProps.erase(num);
}

void resetServer()
{
    heldProps.clear();
}

bool serverPlace(int num, Place& out)
{
    if(!sv.active)
    {
        return false;
    }
    const auto it = heldProps.find(num);
    if(it == heldProps.end())
    {
        return false;
    }
    Held& h = it->second;
    if(stale(h))
    {
        placeNow(h);
        h.turnStale = true;
        logPlace("placed again (client)", num, h);
    }
    out = h.now;
    return true;
}

} // namespace qvr::grip
