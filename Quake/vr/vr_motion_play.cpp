// vr_motion_play.cpp -- the motion recorder's playback and evaluation (vr_motion.hpp, docs/vr-port/MOTIONS.md).
//
// vr_motion_play <take> drives the mock headset from a take: the runtime's tracking and the controllers as
// they were recorded, frame by frame at the recorded frame times, with the server frames where they ran
// (host.c asks hostFrameTime and serverFrameOverride), so the melee sees the same poses at the same times.
// Before it plays, the player is put where the take has them relative to its monster, in front of the
// map's training dummy (or another target), turned the same way, with the weapons in hand and the
// two-handed grip taken again (a settling second, holding the take's first pose). As it plays, the
// replay is compared with the take (the hands' positions relative to the dummy, and the melee's events).
//
// vr_motion_eval plays many takes, each in the map loaded afresh (the same start every time: nothing
// left from the one before), and checks each one's events against quakevr/motions/expect.cfg.

#include "vr_motion.hpp"
#include "vr_motion_take.hpp"
#include "vr_motion_review.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_progs.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Replace.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Chrono/Time.hpp"
#include "Zancle/Concurrency/Thread.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "vr_zancle.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace qvr::motion
{

Row captureRow(bool tick, double svDt); // vr_motion.cpp: this frame's row, as the recorder makes it
za::String resolveTake(const za::String& arg);

namespace
{

void evalFrame();

// The evaluation's map loads run on fixed frames too (a server frame with each, at 72 Hz), from the "map"
// command to the take: every take starts at the same server time, in the same state, whatever the machine
// (the QC's 32-bit floats and its entities' think times depend on the absolute time).
bool fixedLoading = false;

// ----------------------------------------------------------------------------
// Reading a take
// ----------------------------------------------------------------------------

struct Frame
{
    double t{0.0};
    double dt{1.0 / 90.0};
    Phase phase{PhaseRec};
    bool tick{false};
    double svDt{0.0};
    TrackingState tracking; // the runtime's poses and the controllers
    float playYaw{0.f};
    bool helping[HAND_COUNT]{};

    // What the take says the game did, to compare with: the hands and head in the dummy's frame
    // (units), and the melee's events (their `at` in the player's frame).
    bool hasD{false};
    glm::vec3 handD[HAND_COUNT]{};
    bool hasW{false};                 // world, with the monster's origin
    glm::vec3 handW[HAND_COUNT]{};
    glm::vec3 headD{0.f};
    za::Vector<Event> events;

    // For placing the player (the first frame's).
    bool hasOrg{false};
    glm::vec3 org{0.f};     // world (the client's)
    bool hasSvOrg{false};
    glm::vec3 svOrg{0.f};   // world (the server's)
    bool hasVel{false};
    glm::vec3 velPF{0.f};   // the player's velocity (player frame, units/s)
    glm::vec3 leanPF{0.f};  // player frame
    bool hasMon{false};
    bool hasMonW{false};
    za::String monClass;
    glm::vec3 monW{0.f};
    float monYawW{0.f};
    glm::vec3 monPF{0.f};
    int wid[HAND_COUNT]{};
    int wflags[HAND_COUNT]{};
};

struct Take
{
    za::String path;
    za::String name; // the file's name
    ankerl::unordered_dense::map<za::String, za::String> header;
    za::String label, category, detail, map;
    bool hasYaw0{false};
    float yaw0{0.f};
    bool hasTicks{false};
    bool hasPlayYaw{false};
    bool hasWeapons{false};
    bool dummyAttacks{false}; // recorded with the training dummy striking back (the header's "dummy attacks: on")
    za::Vector<Frame> frames;
    size_t firstRec{0};
};

[[nodiscard]] za::Vector<za::String> split(za::StringView s, char sep)
{
    za::Vector<za::String> out;
    za::String cur;
    for(const char c : s)
    {
        if(c == sep)
        {
            out.pushBack(cur);
            cur.clear();
        }
        else if(c != '\r')
        {
            cur += c;
        }
    }
    out.pushBack(cur);
    return out;
}

[[nodiscard]] za::String trim(za::StringView s)
{
    const size_t a = s.findFirstNotOf(" \t\r\n");
    if(a == za::StringView::nPos)
    {
        return "";
    }
    const size_t b = s.findLastNotOf(" \t\r\n");
    return za::String{s.substrByPosLen(a, b - a + 1)};
}

// The events of a row: kind:sub:hand:value:x:y:z:target:detail, ';' between them.
[[nodiscard]] za::Vector<Event> parseEvents(const za::String& cell)
{
    za::Vector<Event> out;
    if(cell.empty())
    {
        return out;
    }
    for(const za::String& e : split(cell, ';'))
    {
        const za::Vector<za::String> f = split(e, ':');
        if(f.size() < 9)
        {
            continue;
        }
        Event ev;
        ev.kind = f[0];
        ev.sub = f[1];
        ev.hand = f[2] == "main" ? HAND_MAIN : f[2] == "off" ? HAND_OFF : -1;
        ev.value = strtof(f[3].cStr(), nullptr);
        ev.hasAt = !f[4].empty();
        if(ev.hasAt)
        {
            ev.at = {strtof(f[4].cStr(), nullptr), strtof(f[5].cStr(), nullptr), strtof(f[6].cStr(), nullptr)};
        }
        ev.target = f[7];
        ev.detail = f[8];
        out.pushBack(ZA_MOVE(ev));
    }
    return out;
}

// A take file (any columns missing take their defaults: a synthetic take needs only the tracking).
[[nodiscard]] bool loadTake(const za::String& path, Take& take, za::String& error)
{
    za::String text;
    if(!files::readText(path.cStr(), text, files::Mode::Binary))
    {
        error = "can't open " + path;
        return false;
    }
    take = Take{};
    take.path = path;
    take.name = za::String{files::fileName(path)};

    za::Vector<za::StringView> lines;
    files::forLines(text, [&](za::StringView l) { lines.pushBack(l); });
    za::Vector<za::String> names;
    ankerl::unordered_dense::map<za::String, int> col;
    for(za::StringView line : lines)
    {
        if(line.endsWith('\r'))
        {
            line.removeSuffix(1);
        }
        if(line.empty())
        {
            continue;
        }
        if(line[0] == '#')
        {
            const size_t colon = line.find(':');
            if(colon != za::StringView::nPos)
            {
                take.header[trim(line.substrByPosLen(1, colon - 1))] = trim(line.substrByPosLen(colon + 1));
            }
            continue;
        }
        if(names.empty())
        {
            names = split(line, ',');
            for(size_t i = 0; i < names.size(); i++)
            {
                col[trim(names[i])] = static_cast<int>(i);
            }
            continue;
        }

        const za::Vector<za::String> cells = split(line, ',');
        const auto has = [&](const char* name) {
            const auto it = col.find(name);
            return it != col.end() && it->second < static_cast<int>(cells.size()) && !cells[it->second].empty();
        };
        const auto str = [&](const za::String& name) -> za::String {
            const auto it = col.find(name);
            return it != col.end() && it->second < static_cast<int>(cells.size()) ? cells[it->second] : za::String{};
        };
        const auto num = [&](const za::String& name, double def) {
            const za::String v = str(name);
            return v.empty() ? def : strtod(v.cStr(), nullptr);
        };
        const auto f = [&](const za::String& name, float def) { return static_cast<float>(num(name, def)); };
        const auto vec = [&](const za::String& a, const za::String& b, const za::String& c) {
            return glm::vec3{f(a, 0.f), f(b, 0.f), f(c, 0.f)};
        };

        Frame fr;
        fr.t = num("t", 0.0);
        fr.dt = num("dt", 1.0 / 90.0);
        const za::String phase = str("phase");
        fr.phase = phase == "pre" ? PhasePre : phase == "tail" ? PhaseTail : PhaseRec;
        if(has("sv_tick"))
        {
            take.hasTicks = true;
            fr.tick = num("sv_tick", 0.0) != 0.0;
            fr.svDt = num("sv_dt", 0.0);
        }
        if(has("play_yaw"))
        {
            take.hasPlayYaw = true;
            fr.playYaw = f("play_yaw", 0.f);
        }

        // The runtime's tracking.
        const auto pose = [&](const za::String& p, Pose& out, bool grip) {
            out.position = vec(p + "px", p + "py", p + "pz");
            out.orientation = glm::quat{f(p + "qw", 1.f), f(p + "qx", 0.f), f(p + "qy", 0.f), f(p + "qz", 0.f)};
            if(glm::length(out.orientation) < 0.5f)
            {
                out.orientation = glm::quat{1.f, 0.f, 0.f, 0.f};
            }
            out.orientation = glm::normalize(out.orientation);
            out.linearVelocity = vec(p + "vx", p + "vy", p + "vz");
            out.angularVelocity = vec(p + "wx", p + "wy", p + "wz");
            out.valid = num(p + "valid", 1.0) != 0.0;
            out.velocityValid = num(p + "vvalid", 1.0) != 0.0;
            if(grip)
            {
                out.gripVelocity = vec(p + "gx", p + "gy", p + "gz");
                out.gripVelocityValid = num(p + "gvalid", 0.0) != 0.0;
            }
        };
        pose("raw_head_", fr.tracking.head, false);
        pose("raw_m_", fr.tracking.hands[HAND_MAIN], true);
        pose("raw_o_", fr.tracking.hands[HAND_OFF], true);
        for(const int h : {HAND_MAIN, HAND_OFF})
        {
            const za::String p = h == HAND_MAIN ? "m_" : "o_";
            HandInput& hi = fr.tracking.input.hands[h];
            const int buttons = static_cast<int>(num(p + "buttons", 0.0));
            hi.trigger = (buttons & 1) != 0;
            hi.grip = (buttons & 2) != 0;
            hi.primary = (buttons & 4) != 0;
            hi.secondary = (buttons & 8) != 0;
            hi.stickClick = (buttons & 16) != 0;
            hi.menu = false; // (never the menu in a replay)
            hi.triggerValue = f(p + "trigger", hi.trigger ? 1.f : 0.f);
            hi.gripValue = f(p + "grip", hi.grip ? 1.f : 0.f);
            hi.thumbTouch = num(p + "thumb", 0.0) != 0.0;
            hi.stick = {f(p + "stick_x", 0.f), f(p + "stick_y", 0.f)};
            fr.helping[h] = num(p + "helping", 0.0) != 0.0;
            if(has((p + "wid").cStr()))
            {
                take.hasWeapons = true;
                fr.wid[h] = static_cast<int>(num(p + "wid", 0.0));
                fr.wflags[h] = static_cast<int>(num(p + "wflags", 0.0));
            }
            if(has((p + "pos_w_x").cStr()))
            {
                fr.hasW = true;
                fr.handW[h] = vec(p + "pos_w_x", p + "pos_w_y", p + "pos_w_z");
            }
            if(has((p + "pos_d_x_u").cStr()))
            {
                fr.hasD = true;
                fr.handD[h] = vec(p + "pos_d_x_u", p + "pos_d_y_u", p + "pos_d_z_u");
            }
        }
        fr.headD = vec("head_d_x_u", "head_d_y_u", "head_d_z_u");
        fr.events = parseEvents(str("events"));

        if(has("org_w_x"))
        {
            fr.hasOrg = true;
            fr.org = vec("org_w_x", "org_w_y", "org_w_z");
        }
        fr.leanPF = vec("lean_x_u", "lean_y_u", "lean_z_u");
        if(has("sv_org_w_x"))
        {
            fr.hasSvOrg = true;
            fr.svOrg = vec("sv_org_w_x", "sv_org_w_y", "sv_org_w_z");
        }
        if(has("pvel_x_u"))
        {
            fr.hasVel = true;
            fr.velPF = vec("pvel_x_u", "pvel_y_u", "pvel_z_u");
        }
        if(has("mon_x_u"))
        {
            fr.hasMon = true;
            fr.monClass = str("mon_class");
            fr.monPF = vec("mon_x_u", "mon_y_u", "mon_z_u");
            if(has("mon_w_x"))
            {
                fr.hasMonW = true;
                fr.monW = vec("mon_w_x", "mon_w_y", "mon_w_z");
                fr.monYawW = f("mon_yaw_w", 0.f);
            }
        }
        take.frames.pushBack(ZA_MOVE(fr));
    }
    if(take.frames.empty())
    {
        error = "no frames in " + path;
        return false;
    }

    const auto h = [&](const char* key) {
        const auto it = take.header.find(key);
        return it == take.header.end() ? za::String{} : it->second;
    };
    take.label = h("label");
    take.category = h("category");
    take.detail = h("detail");
    take.map = h("map");
    take.dummyAttacks = h("dummy attacks").rfind("on", 0) == 0;
    if(take.label.empty())
    {
        take.label = take.name.substrByPosLen(0, take.name.rfind('.'));
    }
    if(take.category.empty())
    {
        take.category = categoryOf(take.label);
    }
    take.firstRec = 0;
    for(size_t i = 0; i < take.frames.size(); i++)
    {
        if(take.frames[i].phase == PhaseRec)
        {
            take.firstRec = i;
            break;
        }
    }
    if(const za::String y = h("yaw0"); !y.empty())
    {
        take.hasYaw0 = true;
        take.yaw0 = strtof(y.cStr(), nullptr);
    }
    else
    {
        // A synthetic take: the head's heading at the start (as vr_hands.cpp turns tracking into Quake's
        // angles), and the play space's turn.
        const Frame& fr = take.frames[take.firstRec];
        const glm::vec3 fwd = fr.tracking.head.orientation * glm::vec3{0.f, 0.f, -1.f};
        take.yaw0 = glm::degrees(za::atan2(-fwd.x, -fwd.z)) + fr.playYaw;
    }
    return true;
}

// ----------------------------------------------------------------------------
// The settings a take was recorded with
// ----------------------------------------------------------------------------

// Those that place the hands, the weapons and the body (applied for the playback, then restored); with
// `recorded`, the melee's own too.
[[nodiscard]] bool placingSetting(const za::String& name, bool melee)
{
    static constexpr const char* placing[] = {"vr_world_scale", "vr_height_calibration", "vr_floor_offset", "vr_lefthanded",
        "vr_gunangle", "vr_gunyaw", "vr_offhandpitch", "vr_offhandyaw", "vr_handcal_", "vr_gunmodel", "vr_weapon_grip_mode", "vr_2h_",
        "vr_lean_", "vr_roomscale_", "vr_body_", "vr_throw_release", "vr_throw_grab_press", "vr_wofs_",
        "vr_controller_legacy_pose", "vr_weapon_cycle_mode", "vr_hull_"};
    // (Every setting the QC's melee, damage and hit reactions read.)
    static constexpr const char* meleeOnes[] = {"vr_melee_", "vr_bash", "vr_shove", "vr_parry", "vr_deflect", "vr_headbutt",
        "vr_sword_", "vr_damage_", "vr_push", "vr_hit_push", "vr_kill_push", "vr_carry_melee_mult", "vr_positional_damage",
        "vr_headshot_mult", "vr_limbshot_mult", "vr_legshot_mult"};
    for(const char* p : placing)
    {
        if(name.rfind(p, 0) == 0)
        {
            return true;
        }
    }
    if(melee)
    {
        for(const char* p : meleeOnes)
        {
            if(name.rfind(p, 0) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

// name=value pairs of a header line (settings, weapon settings, melee settings), or "name value" pairs
// (hand angles, grips).
void collectSettings(const za::String& value, bool pairs, za::Vector<qza::Pair<za::String, za::String>>& out)
{
    files::Words words{value};
    za::String w;
    if(pairs)
    {
        za::String v;
        while(words >> w >> v)
        {
            out.emplaceBack(w, v);
        }
        return;
    }
    while(words >> w)
    {
        const size_t eq = w.find('=');
        if(eq != za::StringView::nPos)
        {
            out.pushBack({za::String{w.substrByPosLen(0, eq)}, za::String{w.substrByPosLen(eq + 1)}});
        }
    }
}

za::Vector<qza::Pair<cvar_t*, za::String>> savedSettings; // the values before a playback

void applySettings(const Take& take, bool melee)
{
    za::Vector<qza::Pair<za::String, za::String>> list;
    const auto h = [&](const char* key) {
        const auto it = take.header.find(key);
        return it == take.header.end() ? za::String{} : it->second;
    };
    if(const za::String all = h("settings"); !all.empty())
    {
        collectSettings(all, false, list);
    }
    else
    {
        // A take from before the settings line: its own lines.
        for(const char* key : {"vr_world_scale", "vr_height_calibration", "vr_floor_offset"})
        {
            if(const za::String v = h(key); !v.empty())
            {
                list.emplaceBack(key, v);
            }
        }
        collectSettings(h("hand angles"), true, list);
        collectSettings(h("grips"), true, list);
        if(const za::String d = h("dominant hand"); !d.empty())
        {
            list.emplaceBack("vr_lefthanded", d.rfind("left", 0) == 0 ? "1" : "0");
        }
    }
    collectSettings(h("weapon settings"), false, list);
    // A take from before the hand calibration: the hands as they were then (none), not as calibrated now.
    for(const cvar_t* var : {&vr_handcal_x, &vr_handcal_y, &vr_handcal_z, &vr_handcal_roll, &vr_handcal_off_mirror,
            &vr_handcal_off_x, &vr_handcal_off_y, &vr_handcal_off_z, &vr_handcal_off_roll})
    {
        if(!za::anyOf(list.begin(), list.end(), [&](const auto& kv) { return kv.first == var->name; }))
        {
            list.emplaceBack(var->name, var->default_string);
        }
    }
    // A take from before the player's narrower box (config 51, vr_hull.cpp): Quake's 32 box, as then (a narrower one lets
    // the body stand closer to the target, and the blow meets it with another part of the weapon).
    for(const char* name : {"vr_hull_width", "vr_hull_ent_width", "vr_hull_hit_width"})
    {
        if(!za::anyOf(list.begin(), list.end(), [&](const auto& kv) { return kv.first == name; }))
        {
            list.emplaceBack(name, "0");
        }
    }
    if(melee)
    {
        collectSettings(h("melee settings"), false, list);
    }
    int applied = 0;
    for(const auto& [name, value] : list)
    {
        if(!placingSetting(name, melee))
        {
            continue;
        }
        cvar_t* var = Cvar_FindVar(name.cStr());
        if(!var || !strcmp(var->string, value.cStr()))
        {
            continue;
        }
        savedSettings.emplaceBack(var, var->string);
        Cvar_SetQuick(var, value.cStr());
        applied++;
    }
    if(applied)
    {
        Con_DPrintf("vr_motion_play: %d settings as recorded\n", applied);
    }
}

void restoreSettings()
{
    for(auto it = qza::rbegin(savedSettings); it != qza::rend(savedSettings); ++it)
    {
        Cvar_SetQuick(it->first, it->second.cStr());
    }
    savedSettings.clear();
}

// ----------------------------------------------------------------------------
// Playback
// ----------------------------------------------------------------------------

enum class State
{
    Idle,
    Setup, // placing the player, the weapons, the grips, holding the first pose
    Play,  // the take's frames
    Post,  // a moment holding its last pose (late events), then the report
};

constexpr double setupDt = 1.0 / 72.0; // a server frame with every host frame: the same start every time
constexpr double setupPress = 0.2;     // the main hand's grip
constexpr double setupEquip = 0.3;     // the weapons
constexpr double setupOffGrip = 0.45;  // the off hand's grip (a two-handed grip taken again)
constexpr double setupAll = 0.9;       // every control as the take starts; the player placed again
constexpr double setupEnd = 1.3;
constexpr double postTime = 0.3;
constexpr float noTargetDistance = 40.f; // units ahead to the dummy for a take without a monster

struct Options
{
    za::String target;   // a classname, or #<entity number>; "" the take's monster's class, else the dummy
    bool yawSet{false};
    float yaw{0.f};       // the player's heading (the take's yaw0) in the world
    bool place{true};
    bool watch{false};    // at the recorded pace (else as fast as the frames go)
    bool save{false};     // the replay as a take, into motions/replays/
    bool recorded{false}; // the melee settings as recorded too
    bool quiet{false};
    float rate{0.f};      // resampled to this many frames a second (0: the take's own frames)
};

// The take resampled at `hz` frames a second (another headset's rate): poses and velocities interpolated
// (positions and velocities linearly, orientations by slerp), the controls and the rest from the frame before;
// the server frames left to the engine (72 Hz). The take's events stay at their times, for the report.
void resample(Take& take, float hz)
{
    za::Vector<Frame> out;
    const za::Vector<Frame>& in = take.frames;
    const double dt = 1.0 / hz;
    const double tEnd = in.back().t;
    size_t j = 0;
    size_t nextEvents = 0;
    for(double t = in.front().t; t <= tEnd + 1e-9; t += dt)
    {
        while(j + 1 < in.size() && in[j + 1].t <= t)
        {
            j++;
        }
        const Frame& a = in[j];
        const Frame& b = in[za::min(j + 1, in.size() - 1)];
        const float s = b.t > a.t ? static_cast<float>(za::clamp((t - a.t) / (b.t - a.t), 0.0, 1.0)) : 0.f;
        Frame f = a;
        f.t = t;
        f.dt = dt;
        f.tick = false;
        f.hasD = false;
        f.events.clear();
        const auto blend = [&](Pose& p, const Pose& pa, const Pose& pb) {
            p.position = glm::mix(pa.position, pb.position, s);
            p.orientation = glm::slerp(pa.orientation, pb.orientation, s);
            p.linearVelocity = glm::mix(pa.linearVelocity, pb.linearVelocity, s);
            p.angularVelocity = glm::mix(pa.angularVelocity, pb.angularVelocity, s);
            p.gripVelocity = glm::mix(pa.gripVelocity, pb.gripVelocity, s);
        };
        blend(f.tracking.head, a.tracking.head, b.tracking.head);
        for(int h = 0; h < HAND_COUNT; h++)
        {
            blend(f.tracking.hands[h], a.tracking.hands[h], b.tracking.hands[h]);
        }
        f.playYaw = a.playYaw + qza::remainder(b.playYaw - a.playYaw, 360.f) * s;
        // The source frames' events up to this one.
        for(; nextEvents < in.size() && in[nextEvents].t <= t; nextEvents++)
        {
            f.events.emplaceBackRange(in[nextEvents].events.data(), in[nextEvents].events.size());
        }
        out.pushBack(ZA_MOVE(f));
    }
    take.frames = ZA_MOVE(out);
    take.hasTicks = false;
    take.firstRec = 0;
    for(size_t i = 0; i < take.frames.size(); i++)
    {
        if(take.frames[i].phase == PhaseRec)
        {
            take.firstRec = i;
            break;
        }
    }
}

// A playback's outcome.
struct Report
{
    za::String file;
    int frames{0};
    za::Vector<Event> recorded; // the take's events (from its start)
    za::Vector<Event> replayed; // the replay's
    za::Vector<double> recordedT, replayedT;
    double errMax[HAND_COUNT]{};
    double errSum[HAND_COUNT]{};
    int errCount{0};
    int parryFrames[HAND_COUNT]{};
    int parryAnyFrames{0}; // either hand's weapon or crossed arms
    int guardFrames{0};
    int recFrames{0};
    int wid[HAND_COUNT]{};
    za::String targetClass;
    za::String warnings;
    bool ok{false};
};

State state = State::Idle;
Take take;
Options opts;
Report report;
double setupTime = 0.0;
double postElapsed = 0.0;
size_t nextFrame = 0;
size_t cur = 0;        // the frame this host frame plays
int targetEnt = 0;     // the target's entity number (0: none)
bool placed = false;   // the placement is worked out
float delta = 0.f;     // the take's turn in this world (degrees): world yaw = recorded + delta
glm::vec3 placeOrigin{0.f};
glm::vec3 placeLean{0.f};
glm::vec3 placeVelocity{0.f}; // the player's as the take starts (moving: a push, a step, the stick)
bool placeRequest = false;
bool launchRequest = false;
bool equipRequest = false;
double wallStart = 0.0;
double playClock = 0.0; // the take's time played so far
za::Vector<Row> replayRows;
void (*onDone)(const Report&) = nullptr; // the evaluation's

[[nodiscard]] edict_t* findTarget(const za::String& wanted, const za::String& recordedClass, const glm::vec3& from)
{
    if(!wanted.empty() && wanted[0] == '#')
    {
        const int n = atoi(wanted.cStr() + 1);
        if(n > 0 && n < qcvm->num_edicts && !EDICT_NUM(n)->free)
        {
            return EDICT_NUM(n);
        }
        return nullptr;
    }
    const auto nearestOf = [&](const za::String& className) -> edict_t* {
        edict_t* best = nullptr;
        float bestDist = 1e9f;
        for(int i = 1; i < qcvm->num_edicts; i++)
        {
            edict_t* e = EDICT_NUM(i);
            if(e->free || className != PR_GetString(e->v.classname))
            {
                continue;
            }
            const float d = glm::distance(glm::vec3{e->v.origin[0], e->v.origin[1], e->v.origin[2]}, from);
            if(d < bestDist)
            {
                bestDist = d;
                best = e;
            }
        }
        return best;
    };
    if(!wanted.empty())
    {
        return nearestOf(wanted);
    }
    if(!recordedClass.empty())
    {
        if(edict_t* e = nearestOf(recordedClass))
        {
            return e;
        }
    }
    return nearestOf("vr_dummy");
}

// Works out where the player goes (server side, the first setup frame): the take's offset from its
// monster, turned into this world.
void workOutPlacement(edict_t* player)
{
    const Frame& f0 = take.frames.front();
    const glm::vec3 here{player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    edict_t* target = findTarget(opts.target, f0.hasMon ? f0.monClass : za::String{}, here);
    targetEnt = target ? NUM_FOR_EDICT(target) : 0;
    report.targetClass = target ? PR_GetString(target->v.classname) : "";
    placed = true;
    if(!target || !opts.place)
    {
        delta = opts.yawSet ? opts.yaw - take.yaw0 : 0.f;
        placeOrigin = here;
        if(!target)
        {
            report.warnings += "no target in this map; ";
        }
        return;
    }
    const glm::vec3 tOrigin{target->v.origin[0], target->v.origin[1], target->v.origin[2]};
    const float tYaw = target->v.angles[1];

    if(f0.hasMon && f0.hasMonW && f0.hasOrg && !opts.yawSet && f0.monClass == report.targetClass)
    {
        // The same kind of target: as recorded, moved with it but not turned (its box doesn't turn: the
        // training dummy's yaw changes as it is hit, which must not turn the take about it).
        delta = 0.f;
        placeOrigin = tOrigin + hands::rotateYaw((f0.hasSvOrg ? f0.svOrg : f0.org) - f0.monW, delta);
    }
    else
    {
        // Another target, or a synthetic take: in front of the target, the take's monster (or a
        // point noTargetDistance ahead) where the target is.
        const glm::vec3 monPF = f0.hasMon ? f0.monPF : glm::vec3{noTargetDistance, 0.f, 0.f};
        float yawP;
        if(opts.yawSet)
        {
            yawP = opts.yaw;
        }
        else
        {
            const float bearing = glm::degrees(za::atan2(monPF.y, monPF.x));
            yawP = tYaw + 180.f - bearing;
        }
        delta = qza::remainder(yawP - take.yaw0, 360.f);
        placeOrigin = tOrigin - hands::rotateYaw(glm::vec3{monPF.x, monPF.y, 0.f}, yawP);
        placeOrigin.z = tOrigin.z - (f0.hasMon ? monPF.z : 0.f);
        if(!f0.hasMon)
        {
            report.warnings += "the take has no monster: the target put 40 units ahead; ";
        }
    }
    placeLean = hands::rotateYaw(f0.leanPF, take.yaw0 + delta);
    placeVelocity = f0.hasVel ? hands::rotateYaw(f0.velPF, take.yaw0 + delta) : glm::vec3{0.f};
    Con_DPrintf("vr_motion_play: %s #%d at %.1f %.1f %.1f; the player at %.2f %.2f %.2f, turned %.1f\n",
        report.targetClass.cStr(), targetEnt, tOrigin.x, tOrigin.y, tOrigin.z, placeOrigin.x, placeOrigin.y, placeOrigin.z,
        delta);
}

void placePlayer(edict_t* player)
{
    // Where its box is free: a take recorded in noclip (setpos) may have the feet a little in the floor
    // (the server would put the player back where it was), or a step may be higher here.
    glm::vec3 at = placeOrigin;
    bool free = false;
    const auto fits = [&](const glm::vec3& p) {
        vec3_t o{p.x, p.y, p.z};
        return !SV_Move(o, player->v.mins, player->v.maxs, o, MOVE_NORMAL, player).startsolid;
    };
    for(float up = 0.f; up <= 18.f && !free; up += 0.25f)
    {
        if(fits(placeOrigin + glm::vec3{0.f, 0.f, up}))
        {
            free = true;
            at.z = placeOrigin.z + up;
            if(up > 0.f)
            {
                report.warnings += va("placed %.2f units higher (the take's spot is in the floor here); ", up);
            }
        }
    }
    // Into the target (a take recorded in noclip, or a synthetic take against a box turned another way):
    // stepped back from it until the boxes are apart.
    if(!free && targetEnt > 0)
    {
        edict_t* t = EDICT_NUM(targetEnt);
        glm::vec3 away{placeOrigin.x - t->v.origin[0], placeOrigin.y - t->v.origin[1], 0.f};
        away = glm::length(away) > 0.01f ? glm::normalize(away) : glm::vec3{1.f, 0.f, 0.f};
        for(float back = 0.5f; back <= 32.f && !free; back += 0.5f)
        {
            for(float up = 0.f; up <= 2.f && !free; up += 0.25f)
            {
                const glm::vec3 p = placeOrigin + away * back + glm::vec3{0.f, 0.f, up};
                if(fits(p))
                {
                    free = true;
                    at = p;
                    report.warnings += va("placed %.1f units further from the target (the boxes overlapped); ", back);
                }
            }
        }
    }
    if(!free && report.warnings.find("overlaps") == za::StringView::nPos)
    {
        report.warnings += "the player's box overlaps something where the take has it (recorded in noclip?); ";
    }
    placeOrigin = at;
    player->v.origin[0] = placeOrigin.x;
    player->v.origin[1] = placeOrigin.y;
    player->v.origin[2] = placeOrigin.z;
    VectorCopy(player->v.origin, player->v.oldorigin); // (else a stuck check puts it back where it was)
    player->v.velocity[0] = player->v.velocity[1] = player->v.velocity[2] = 0.f;
    SV_LinkEdict(player, false);
}

void equip(edict_t* player)
{
    const func_t fn = progs::bindings().Motion_Equip;
    if(!fn || !take.hasWeapons)
    {
        return;
    }
    const Frame& f0 = take.frames.front();
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        pr_global_struct->time = qcvm->time;
        pr_global_struct->self = EDICT_TO_PROG(player);
        pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
        G_FLOAT(OFS_PARM0) = static_cast<float>(h);
        G_FLOAT(OFS_PARM1) = static_cast<float>(f0.wid[h]);
        G_FLOAT(OFS_PARM2) = static_cast<float>(f0.wflags[h]);
        PR_ExecuteProgram(fn);
    }
}

// A take recorded with the training dummy's attacks on (QC vr_dummy.qc): its target does each of the take's
// "strike" events (a wind-up, a blow and its damage, a miss; QC VR_Dummy_Replay) in the server frame that plays the
// event's frame, after the player's, as when it was recorded. The dummy's own attacks (vr_dummy_attacks) are off
// meanwhile (startPlayback): the same blows at the same moments, whatever its timer would do.
size_t strikeNext = 0; // the next frame whose strikes are to be done
int strikesDone = 0;

void doStrikes(size_t upTo)
{
    for(; strikeNext <= upTo && strikeNext < take.frames.size(); strikeNext++)
    {
        for(const Event& e : take.frames[strikeNext].events)
        {
            const float what = e.kind != "strike" ? 0.f
                               : e.sub == "windup" ? 1.f
                               : e.sub == "blow"   ? 2.f
                               : e.sub == "miss"   ? 3.f
                                                   : 0.f;
            if(what == 0.f)
            {
                continue;
            }
            const func_t fn = progs::bindings().Dummy_Replay;
            edict_t* target = targetEnt > 0 && targetEnt < qcvm->num_edicts ? EDICT_NUM(targetEnt) : nullptr;
            if(!fn || !target || target->free || strcmp(PR_GetString(target->v.classname), "vr_dummy") != 0)
            {
                if(report.warnings.find("dummy's strikes") == za::StringView::nPos)
                {
                    report.warnings += "the take's dummy's strikes aren't done again: its target isn't the training dummy; ";
                }
                continue;
            }
            pr_global_struct->time = qcvm->time;
            pr_global_struct->self = EDICT_TO_PROG(target);
            pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
            G_FLOAT(OFS_PARM0) = what;
            G_FLOAT(OFS_PARM1) = e.value;
            PR_ExecuteProgram(fn);
            strikesDone++;
        }
    }
}

// The controls of the first frame during the setup: none, then the grips in turn, then all.
[[nodiscard]] InputState setupInput()
{
    const InputState& all = take.frames.front().tracking.input;
    if(setupTime >= setupAll)
    {
        return all;
    }
    // A hand holding its own weapon grips before the weapons are given (it holds it: vr_weapon_grip_mode 0
    // drops a weapon a hand doesn't grip); an empty hand gripping (a two-handed grip on the other's weapon)
    // after, where it takes that grip.
    InputState in;
    const Frame& f0 = take.frames.front();
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        const bool holds = !take.hasWeapons || f0.wid[h] != 0 || (h == HAND_MAIN && f0.wid[HAND_OFF] == 0);
        if(all.hands[h].grip && setupTime >= (holds ? setupPress : setupOffGrip))
        {
            in.hands[h].grip = true;
            in.hands[h].gripValue = all.hands[h].gripValue;
        }
    }
    return in;
}

bool isHit(const Event& e)
{
    return e.kind == "melee" || e.kind == "bash" || e.kind == "parrybash" || e.kind == "shove" || e.kind == "headbutt";
}

[[nodiscard]] za::String eventText(const Event& e)
{
    za::String s = e.kind;
    if(!e.sub.empty())
    {
        s += "/" + e.sub;
    }
    s += e.hand == HAND_MAIN ? " main" : e.hand == HAND_OFF ? " off" : "";
    if(isHit(e) || e.kind == "parry")
    {
        s += va(" %.1f", e.value);
    }
    if(!e.detail.empty())
    {
        s += " (" + e.detail + ")";
    }
    return s;
}

// The take's hits against the replay's: each recorded hit matched by a replayed one of the same kind,
// sub, hand, target and striking point; the damage and time differences of those matched.
struct Match
{
    int recordedHits{0};
    int replayedHits{0};
    int matched{0};
    double maxDamageDiff{0.0}; // relative
    double maxTimeDiff{0.0};
};

[[nodiscard]] Match matchHits(const Report& r)
{
    Match m;
    za::Vector<bool> used(r.replayed.size(), false);
    for(size_t j = 0; j < r.replayed.size(); j++)
    {
        m.replayedHits += isHit(r.replayed[j]);
    }
    for(size_t i = 0; i < r.recorded.size(); i++)
    {
        const Event& a = r.recorded[i];
        if(!isHit(a))
        {
            continue;
        }
        m.recordedHits++;
        for(size_t j = 0; j < r.replayed.size(); j++)
        {
            const Event& b = r.replayed[j];
            if(used[j] || !isHit(b) || a.kind != b.kind || a.sub != b.sub || a.hand != b.hand || a.target != b.target ||
               a.detail != b.detail)
            {
                continue;
            }
            used[j] = true;
            m.matched++;
            m.maxDamageDiff = za::max(m.maxDamageDiff,
                za::fabs(static_cast<double>(a.value - b.value)) / za::max(1.0, za::fabs(static_cast<double>(a.value))));
            m.maxTimeDiff = za::max(m.maxTimeDiff, za::fabs(r.recordedT[i] - r.replayedT[j]));
            break;
        }
    }
    return m;
}

[[nodiscard]] za::String eventsText(const za::Vector<Event>& events, const za::Vector<double>& times, bool hitsAndPushes)
{
    za::String s;
    for(size_t i = 0; i < events.size(); i++)
    {
        const Event& e = events[i];
        if(hitsAndPushes && !isHit(e) && e.kind != "push" && e.kind != "parry" && e.kind != "deflect" && e.kind != "strike")
        {
            continue;
        }
        s += (s.empty() ? "" : "; ") + eventText(e) + va(" @%.3f", times[i]);
    }
    return s.empty() ? "-" : s;
}

void saveReplay()
{
    if(replayRows.empty())
    {
        return;
    }
    TakeInfo info;
    info.label = take.label;
    info.category = take.category;
    info.detail = take.detail;
    const auto it = take.header.find("note");
    info.note = it != take.header.end() ? it->second : "";
    info.date = take.header.count("date") ? take.header.at("date") : "";
    info.map = cl.mapname;
    info.source = "replay of " + take.name;
    const Row* first = &replayRows.front();
    for(const Row& r : replayRows)
    {
        if(r.phase == PhaseRec)
        {
            first = &r;
            break;
        }
    }
    info.yaw0 = first->headAngles.y;
    info.origin0 = first->origin;
    info.t0 = first->realtime;
    info.take = 0;
    const za::String dir = motionsDir() + "/replays";
    Sys_mkdir(dir.cStr());
    const za::String stem{take.name.substrByPosLen(0, take.name.rfind('.'))};
    const za::String path = dir + "/" + stem + "_replay.csv";
    if(writeTake(path, info, replayRows))
    {
        Con_Printf("vr_motion_play: the replay is motions/replays/%s_replay.csv\n", stem.cStr());
    }
}

void finish()
{
    state = State::Idle;
    restoreSettings();
    report.ok = true;
    report.frames = static_cast<int>(take.frames.size());
    if(opts.save)
    {
        saveReplay();
    }
    replayRows.clear();

    const Match m = matchHits(report);
    if(!opts.quiet)
    {
        Con_Printf("vr_motion_play: %s: %d frames against %s\n", take.name.cStr(), report.frames,
            report.targetClass.empty() ? "nothing" : report.targetClass.cStr());
        Con_Printf("  recorded: %s\n", eventsText(report.recorded, report.recordedT, true).cStr());
        Con_Printf("  replayed: %s\n", eventsText(report.replayed, report.replayedT, true).cStr());
        if(m.recordedHits || m.replayedHits)
        {
            Con_Printf("  hits: %d recorded, %d replayed, %d matched (damage within %.1f%%, time within %.3f s)\n",
                m.recordedHits, m.replayedHits, m.matched, m.maxDamageDiff * 100.0, m.maxTimeDiff);
        }
        if(report.errCount)
        {
            Con_Printf("  hands vs the take, relative to the dummy: main max %.3f rms %.3f, off max %.3f rms %.3f units\n",
                report.errMax[HAND_MAIN], za::sqrt(report.errSum[HAND_MAIN] / report.errCount), report.errMax[HAND_OFF],
                za::sqrt(report.errSum[HAND_OFF] / report.errCount));
        }
        if(take.dummyAttacks)
        {
            Con_Printf("  the training dummy struck back in the take: %d of its strike events done again\n", strikesDone);
        }
        if(!report.warnings.empty())
        {
            Con_Printf("  note: %s\n", report.warnings.cStr());
        }
    }
    if(onDone)
    {
        onDone(report);
    }
}

void stopPlayback(const char* why)
{
    if(state == State::Idle)
    {
        return;
    }
    Con_Printf("vr_motion_play: stopped (%s)\n", why);
    state = State::Idle;
    restoreSettings();
    replayRows.clear();
    report.ok = false;
    if(onDone)
    {
        onDone(report);
    }
}

[[nodiscard]] bool startPlayback(const za::String& path, const Options& o)
{
    za::String error;
    Take t;
    if(!loadTake(path, t, error))
    {
        Con_Printf("vr_motion_play: %s\n", error.cStr());
        return false;
    }
    const Backend* be = backend();
    if(!be || strcmp(be->name(), "mock") != 0)
    {
        Con_Printf("vr_motion_play: plays in the mock headset only (vr_backend mock)\n");
        return false;
    }
    if(!sv.active || !svs.clients[0].active || !progs::bindings().isVrProgs || !hands::current().valid)
    {
        Con_Printf("vr_motion_play: load a map first (map vrfiringrange)\n");
        return false;
    }
    // A take whose player is moving as it starts (walking into place with the stick, pushed back by the last
    // shove): starting it in motion can't be exact (the client's origin lags the server's, the move of the
    // first frame's server frame), so it starts where the player stood still for a few frames in its lead-in.
    if(t.frames.front().hasVel && glm::length(t.frames.front().velPF) > 0.f)
    {
        for(size_t i = 3; i < t.firstRec; i++)
        {
            bool still = true;
            for(size_t j = i - 3; j <= i; j++)
            {
                still = still && t.frames[j].hasVel && glm::length(t.frames[j].velPF) == 0.f;
            }
            if(still)
            {
                t.frames.erase(t.frames.begin(), t.frames.begin() + static_cast<long>(i));
                t.firstRec -= i;
                break;
            }
        }
    }
    if(o.rate > 0.f)
    {
        resample(t, za::clamp(o.rate, 20.f, 500.f));
    }
    take = ZA_MOVE(t);
    opts = o;
    report = Report{};
    report.file = take.name;
    for(const Frame& f : take.frames)
    {
        if(f.phase == PhasePre)
        {
            continue;
        }
        for(const Event& e : f.events)
        {
            report.recorded.pushBack(e);
            report.recordedT.pushBack(f.t);
        }
    }
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        report.wid[h] = take.frames[take.firstRec].wid[h];
    }
    applySettings(take, opts.recorded);
    // The training dummy's attacks off (put back afterwards): in a replay it strikes only as the take has it.
    if(vr_dummy_attacks.value != 0.f)
    {
        savedSettings.emplaceBack(&vr_dummy_attacks, vr_dummy_attacks.string);
        Cvar_SetQuick(&vr_dummy_attacks, "0");
    }
    strikeNext = 0;
    strikesDone = 0;
    state = State::Setup;
    setupTime = 0.0;
    postElapsed = 0.0;
    nextFrame = 0;
    cur = 0;
    placed = false;
    placeRequest = true;
    equipRequest = false;
    launchRequest = false;
    delta = 0.f;
    targetEnt = 0;
    replayRows.clear();
    srand(1); // (QC's random() and the engine's rand(): the same draws every time)
    return true;
}

// vr_motion_play <take> [target <classname|#entity>] [yaw <degrees>] [rate <hz>] [noplace] [watch] [save] [recorded] [quiet]
// vr_motion_play stop
void play_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("usage: vr_motion_play <take> [target <classname|#entity>] [yaw <degrees>] [rate <hz>] [noplace] [watch] "
                   "[save] [recorded] [quiet]; vr_motion_play stop\n");
        return;
    }
    if(!q_strcasecmp(Cmd_Argv(1), "stop"))
    {
        stopPlayback("vr_motion_play stop");
        return;
    }
    Options o;
    for(int i = 2; i < Cmd_Argc(); i++)
    {
        const char* a = Cmd_Argv(i);
        if(!q_strcasecmp(a, "target") && i + 1 < Cmd_Argc())
        {
            o.target = Cmd_Argv(++i);
        }
        else if(!q_strcasecmp(a, "yaw") && i + 1 < Cmd_Argc())
        {
            o.yawSet = true;
            o.yaw = Q_atof(Cmd_Argv(++i));
        }
        else if(!q_strcasecmp(a, "rate") && i + 1 < Cmd_Argc())
        {
            o.rate = Q_atof(Cmd_Argv(++i));
        }
        else if(!q_strcasecmp(a, "noplace"))
        {
            o.place = false;
        }
        else if(!q_strcasecmp(a, "watch"))
        {
            o.watch = true;
        }
        else if(!q_strcasecmp(a, "save"))
        {
            o.save = true;
        }
        else if(!q_strcasecmp(a, "recorded"))
        {
            o.recorded = true;
        }
        else if(!q_strcasecmp(a, "quiet"))
        {
            o.quiet = true;
        }
        else
        {
            Con_Printf("vr_motion_play: unknown option \"%s\"\n", a);
            return;
        }
    }
    if(state != State::Idle)
    {
        Con_Printf("vr_motion_play: a take is playing (vr_motion_play stop)\n");
        return;
    }
    onDone = nullptr;
    const za::String path = resolveTake(Cmd_Argv(1));
    if(path.empty())
    {
        Con_Printf("vr_motion_play: no take \"%s\" (in motions/, the game folder, or a path)\n", Cmd_Argv(1));
        return;
    }
    (void)startPlayback(path, o);
}

} // namespace

// A take's file: as given, in motions/, or in the game folder ("" if none).
za::String resolveTake(const za::String& arg)
{
    for(const za::String& p : {arg, motionsDir() + "/" + arg, za::String{com_gamedir} + "/" + arg})
    {
        for(const za::String& q : {p, p + ".csv"})
        {
            if(Sys_FileType(q.cStr()) == FS_ENT_FILE)
            {
                return q;
            }
        }
    }
    return "";
}

bool playing()
{
    return state != State::Idle;
}

bool playWantsSamples()
{
    return state != State::Idle;
}

bool playDummyAttacks()
{
    return state != State::Idle && take.dummyAttacks;
}

double hostFrameTime(double time)
{
    double dt = time;
    if(state == State::Idle && (fixedLoading || vr_fixed_frames.value != 0.f))
    {
        return setupDt;
    }
    if(state == State::Setup || state == State::Post)
    {
        dt = setupDt;
    }
    else if(state == State::Play)
    {
        cur = za::min(nextFrame, take.frames.size() - 1);
        nextFrame++;
        dt = CLAMP(0.0005, take.frames[cur].dt, 0.1);
    }
    else
    {
        return time;
    }
    playClock += dt;
    if(opts.watch)
    {
        // At the recorded pace: wait for the wall clock.
        const double ahead = playClock - (Sys_DoubleTime() - wallStart);
        if(ahead > 0.0)
        {
            za::ThisThread::sleepFor(za::microseconds(static_cast<za::I64>(za::min(ahead, 0.1) * 1e6)));
        }
    }
    return dt;
}

bool gameClockFixed()
{
    // hostFrameTime's cases that don't read the frame's wall-clock time (a take in watch mode waits for the wall clock).
    if(state == State::Idle)
    {
        return fixedLoading || vr_fixed_frames.value != 0.f;
    }
    return !opts.watch; // (watch mode paces the setup and the end at the wall clock too)
}

int serverFrameOverride(double& frametime)
{
    // (Before this host frame's server frame: the dummy's strikes in the take's frame it plays.)
    if(state == State::Play && take.dummyAttacks && sv.active)
    {
        qcvm_t* const old = qcvm;
        if(old != &sv.qcvm)
        {
            if(old)
            {
                PR_SwitchQCVM(nullptr);
            }
            PR_SwitchQCVM(&sv.qcvm);
        }
        doStrikes(cur);
        if(old != &sv.qcvm)
        {
            PR_SwitchQCVM(nullptr);
            if(old)
            {
                PR_SwitchQCVM(old);
            }
        }
    }

    if(state == State::Setup || state == State::Post || (state == State::Idle && (fixedLoading || vr_fixed_frames.value != 0.f)))
    {
        frametime = setupDt;
        return 1;
    }
    if(state == State::Play && take.hasTicks)
    {
        const Frame& f = take.frames[cur];
        frametime = f.svDt > 0.0 ? f.svDt : setupDt;
        return f.tick ? 1 : 0;
    }
    return -1;
}

void playAfterTracking(TrackingState& tracking, FrameState& frame)
{
    if(state == State::Idle)
    {
        return;
    }
    const Frame& f = state == State::Setup ? take.frames.front()
                     : state == State::Post ? take.frames.back()
                                            : take.frames[cur];
    GripInRaw grips[HAND_COUNT]; // the controllers' own (not in the take): where the Show Controller preview goes
    za::copy(tracking.gripInHand, tracking.gripInHand + HAND_COUNT, grips);
    tracking = f.tracking;
    za::copy(grips, grips + za::getArraySize(grips), tracking.gripInHand);
    tracking.time = realtime;
    if(state == State::Setup)
    {
        tracking.input = setupInput();
    }
    // Turning is the take's play_yaw (the main stick's turn is in it: not twice).
    for(HandInput& in : tracking.input.hands)
    {
        in.menu = false;
    }
    tracking.input.hands[1 - hands::moveHand()].stick.x = 0.f; // (the turning stick)
    hands::setPlaySpaceYaw((take.hasPlayYaw ? f.playYaw : 0.f) + delta);
    if(state == State::Play && cur == 0)
    {
        hands::setLean(placeLean);
    }
    for(int eye = 0; eye < 2; eye++)
    {
        frame.eyes[eye].pose = tracking.head;
        frame.eyes[eye].pose.position.x += eye == 0 ? -0.032f : 0.032f;
        frame.eyes[eye].fov = Fov{};
    }
    frame.shouldRender = true;
}

void playServerFrame()
{
    if(state != State::Setup || !sv.active || !svs.clients[0].active)
    {
        return;
    }
    edict_t* player = svs.clients[0].edict;
    if(!placed)
    {
        workOutPlacement(player);
    }
    if(placeRequest)
    {
        placeRequest = false;
        if(opts.place)
        {
            placePlayer(player);
        }
    }
    if(equipRequest)
    {
        equipRequest = false;
        equip(player);
    }
    if(launchRequest)
    {
        // The setup's last frame: where the take starts, moving as it was.
        launchRequest = false;
        if(opts.place)
        {
            placePlayer(player);
            player->v.velocity[0] = placeVelocity.x;
            player->v.velocity[1] = placeVelocity.y;
            player->v.velocity[2] = placeVelocity.z;
        }
    }
}

void playServerSample(edict_t*& target)
{
    if(state != State::Idle && targetEnt > 0 && targetEnt < qcvm->num_edicts && !EDICT_NUM(targetEnt)->free)
    {
        target = EDICT_NUM(targetEnt);
    }
}

void playFrameEnd(za::Vector<Event>& events, bool tick, double svDt)
{
    switch(state)
    {
        case State::Idle: evalFrame(); return;

        case State::Setup:
        {
            const double before = setupTime;
            setupTime += setupDt;
            if(before < setupEquip && setupTime >= setupEquip)
            {
                equipRequest = true;
            }
            if(before < setupAll && setupTime >= setupAll)
            {
                placeRequest = true; // again: nothing moved it since
            }
            if(setupTime < setupEnd && setupTime + setupDt >= setupEnd)
            {
                launchRequest = true; // (the next frame is the setup's last)
            }
            events.clear();
            if(setupTime >= setupEnd)
            {
                // The two-handed grip as recorded?
                const Frame& f0 = take.frames.front();
                for(const int h : {HAND_MAIN, HAND_OFF})
                {
                    if(f0.helping[h] != twohand::helping(h))
                    {
                        report.warnings += va("the %s hand %s the other's weapon in the take, %s here; ",
                            h == HAND_MAIN ? "main" : "off", f0.helping[h] ? "steadied" : "didn't steady",
                            twohand::helping(h) ? "does" : "doesn't");
                    }
                }
                state = State::Play;
                nextFrame = 0;
                wallStart = Sys_DoubleTime();
                playClock = 0.0;
            }
            return;
        }

        case State::Play:
        case State::Post:
        {
            if(!sv.active || !hands::current().valid)
            {
                stopPlayback("the game ended");
                return;
            }
            const bool post = state == State::Post;
            const Frame& f = post ? take.frames.back() : take.frames[cur];
            Row r = captureRow(tick, svDt); // (takes the frame's events)
            r.phase = post ? PhaseTail : f.phase;
            for(const Event& e : r.events)
            {
                if(r.phase != PhasePre)
                {
                    report.replayed.pushBack(e);
                    report.replayedT.pushBack(post ? f.t + postElapsed : f.t);
                }
            }
            // The hands relative to the target, against the take's: from its origin, in the world's axes turned
            // as the take was (a synthetic take's: in the dummy's frame).
            if(!post && (f.hasW ? f.hasMonW : f.hasD) && r.sv && r.sv->monster)
            {
                const ServerSample& sv = *r.sv;
                for(const int h : {HAND_MAIN, HAND_OFF})
                {
                    const double e = f.hasW
                        ? glm::distance(r.hands[h].pos - sv.monOrigin, hands::rotateYaw(f.handW[h] - f.monW, delta))
                        : glm::distance(hands::rotateYaw(r.hands[h].pos - sv.monOrigin, -sv.monAngles.y), f.handD[h]);
                    report.errMax[h] = za::max(report.errMax[h], e);
                    report.errSum[h] += e * e;
                }
                report.errCount++;
            }
            if(r.phase == PhaseRec && r.sv && r.sv->qc)
            {
                report.recFrames++;
                if(const glm::vec3* p = r.sv->value("parry"))
                {
                    report.parryFrames[HAND_MAIN] += p->x != 0.f;
                    report.parryFrames[HAND_OFF] += p->y != 0.f;
                    report.parryAnyFrames += p->x != 0.f || p->y != 0.f || p->z != 0.f;
                }
                if(const glm::vec3* g = r.sv->value("guard"))
                {
                    report.guardFrames += g->x >= 0.f;
                }
            }
            if(opts.save)
            {
                replayRows.pushBack(ZA_MOVE(r));
            }
            if(post)
            {
                postElapsed += setupDt;
                if(postElapsed >= postTime)
                {
                    finish();
                }
            }
            else if(cur + 1 >= take.frames.size())
            {
                state = State::Post;
                postElapsed = 0.0;
            }
            return;
        }
    }
}

// ----------------------------------------------------------------------------
// Evaluation
// ----------------------------------------------------------------------------

namespace
{

// An expectation (expect.cfg): see the file's comments and docs/vr-port/MOTIONS.md.
struct Expectation
{
    za::Vector<za::String> required;  // kind[/sub][@point|point] (any one of them)
    za::Vector<za::String> forbidden; // kind[/sub]
    bool none{false};
    za::Vector<za::String> poses;     // parry, guard: held for half the take
    za::Vector<za::String> notPoses;  // parry, guard: never, in the take
    za::Vector<za::String> weapons;   // the weapon classes it applies to (empty: any)
    bool skip{false};                   // "-": reported only
};

ankerl::unordered_dense::map<za::String, Expectation> expectations;
za::String expectPath;

[[nodiscard]] za::String underscores(za::String s)
{
    za::replace(s.begin(), s.end(), ' ', '_');
    return s;
}

void loadExpectations()
{
    expectations.clear();
    expectPath = motionsDir() + "/expect.cfg";
    za::String text;
    if(!files::readText(expectPath.cStr(), text))
    {
        Con_Printf("vr_motion_eval: no %s: every take is reported without a verdict\n", expectPath.cStr());
        return;
    }
    za::Vector<za::StringView> lines;
    files::forLines(text, [&](za::StringView l) { lines.pushBack(l); });
    for(za::StringView line : lines)
    {
        const size_t hash = line.find('#');
        if(hash != za::StringView::nPos)
        {
            line = line.substrByPosLen(0, hash);
        }
        files::Words words{line};
        za::String label, w;
        if(!(words >> label))
        {
            continue;
        }
        Expectation e;
        while(words >> w)
        {
            if(w == "-")
            {
                e.skip = true;
            }
            else if(w == "none")
            {
                e.none = true;
            }
            else if(w.rfind("!pose:", 0) == 0)
            {
                e.notPoses.emplaceBack(w.substrByPosLen(6));
            }
            else if(w[0] == '!')
            {
                e.forbidden.emplaceBack(w.substrByPosLen(1));
            }
            else if(w.rfind("pose:", 0) == 0)
            {
                e.poses.emplaceBack(w.substrByPosLen(5));
            }
            else if(w.rfind("weapon:", 0) == 0)
            {
                for(const za::String& c : split(w.substrByPosLen(7), '|'))
                {
                    e.weapons.pushBack(c);
                }
            }
            else
            {
                e.required.pushBack(w);
            }
        }
        expectations[label] = ZA_MOVE(e);
    }
}

// A weapon id's class, as expect.cfg names them.
[[nodiscard]] za::String weaponClass(int wid)
{
    switch(wid)
    {
        case 0: return "fist";
        case 1: return "grapple";
        case 2: return "axe";
        case 3: return "mjolnir";
        case 13: return "sword";
        case 14: return "chainsaw"; // a melee weapon (QC VR_MTHING_SAW)
        case 15:                    // a grunt's shotgun, an enforcer's laser rifle (QC vr_enemyguns.qc)
        case 16: return "gun";
        case 17: return "crowbar"; // a melee weapon (QC VR_MTHING_CROWBAR)
        default: return wid >= 4 && wid <= 12 ? "gun" : "other";
    }
}

[[nodiscard]] bool matchesItem(const Event& e, const za::String& item)
{
    za::String kind = item;
    za::String points;
    if(const size_t at = kind.find('@'); at != za::StringView::nPos)
    {
        points = kind.substrByPosLen(at + 1);
        kind = kind.substrByPosLen(0, at);
    }
    za::String sub;
    if(const size_t slash = kind.find('/'); slash != za::StringView::nPos)
    {
        sub = kind.substrByPosLen(slash + 1);
        kind = kind.substrByPosLen(0, slash);
    }
    if(e.kind != kind || (!sub.empty() && e.sub != sub))
    {
        return false;
    }
    if(!points.empty())
    {
        const za::Vector<za::String> list = split(points, '|');
        return za::find(list.begin(), list.end(), underscores(e.detail)) != list.end();
    }
    return true;
}

struct Result
{
    za::String path; // the take's file
    za::String file, label, weapons, expectation, verdict, reason, replayed, recorded, same;
    int frames{0};
    double err{0.0};
};

za::Vector<Result> results;

[[nodiscard]] za::String expectationText(const Expectation& e)
{
    za::String s;
    const auto add = [&](const za::String& w) { s += (s.empty() ? "" : " ") + w; };
    if(e.skip)
    {
        add("-");
    }
    if(e.none)
    {
        add("none");
    }
    for(const auto& r : e.required)
    {
        add(r);
    }
    for(const auto& f : e.forbidden)
    {
        add("!" + f);
    }
    for(const auto& p : e.poses)
    {
        add("pose:" + p);
    }
    for(const auto& p : e.notPoses)
    {
        add("!pose:" + p);
    }
    if(!e.weapons.empty())
    {
        za::String w;
        for(const auto& c : e.weapons)
        {
            w += (w.empty() ? "" : "|") + c;
        }
        add("weapon:" + w);
    }
    return s;
}

// The verdict on a replay: PASS, FAIL, N/A (not for this weapon), or "-" (no expectation).
void judge(const Report& r, const Expectation* e, Result& out)
{
    const za::String main = weaponClass(r.wid[HAND_MAIN]);
    const za::String off = weaponClass(r.wid[HAND_OFF]);
    out.weapons = main + (r.wid[HAND_OFF] ? "+" + off : "");
    if(!r.ok)
    {
        out.verdict = "ERROR";
        out.reason = "the playback stopped";
        return;
    }
    if(!e || e->skip)
    {
        out.verdict = "-";
        out.reason = e ? "reported only" : "no expectation for the label";
        return;
    }
    out.expectation = expectationText(*e);
    if(!e->weapons.empty() && za::find(e->weapons.begin(), e->weapons.end(), main) == e->weapons.end() &&
       za::find(e->weapons.begin(), e->weapons.end(), off) == e->weapons.end())
    {
        out.verdict = "N/A";
        out.reason = "not for a " + main + " (expected with " + [&] {
            za::String w;
            for(const auto& c : e->weapons)
            {
                w += (w.empty() ? "" : " or ") + c;
            }
            return w;
        }() + ")";
        return;
    }

    za::Vector<za::String> fails;
    // Required (any one): an event of one of them, a hit against the target.
    if(!e->required.empty())
    {
        bool any = false;
        for(const Event& ev : r.replayed)
        {
            for(const za::String& item : e->required)
            {
                if(matchesItem(ev, item) && (!isHit(ev) || ev.target == r.targetClass))
                {
                    any = true;
                }
            }
        }
        if(!any)
        {
            fails.pushBack("no " + [&] {
                za::String w;
                for(const auto& c : e->required)
                {
                    w += (w.empty() ? "" : " or ") + c;
                }
                return w;
            }());
        }
    }
    // Forbidden: listed ones, any hit whose kind isn't required, and with none every melee event.
    for(const Event& ev : r.replayed)
    {
        bool bad = false;
        for(const za::String& item : e->forbidden)
        {
            bad = bad || matchesItem(ev, item);
        }
        if(e->none && (isHit(ev) || ev.kind == "stroke" || ev.kind == "push" || ev.kind == "deflect"))
        {
            bad = true;
        }
        if(isHit(ev) && !e->none)
        {
            bool expected = false;
            for(const za::String& item : e->required)
            {
                const za::StringView kind = item.substrByPosLen(0, item.findFirstOf("/@"));
                expected = expected || kind == ev.kind;
            }
            bad = bad || !expected;
        }
        if(bad)
        {
            fails.pushBack("unexpected " + eventText(ev));
        }
    }
    // Poses: held for at least half the take.
    for(const za::String& p : e->poses)
    {
        const int held = p == "parry" ? r.parryAnyFrames
                         : p == "guard" ? r.guardFrames
                                        : 0;
        if(r.recFrames == 0 || held * 2 < r.recFrames)
        {
            fails.pushBack(va("%s pose held %d of %d frames", p.cStr(), held, r.recFrames));
        }
    }
    // Poses that must never be: not a frame of the take in them.
    for(const za::String& p : e->notPoses)
    {
        const int held = p == "parry" ? r.parryAnyFrames : p == "guard" ? r.guardFrames : 0;
        if(held > 0)
        {
            fails.pushBack(va("%s pose in %d of %d frames", p.cStr(), held, r.recFrames));
        }
    }
    out.verdict = fails.empty() ? "PASS" : "FAIL";
    for(const za::String& f : fails)
    {
        out.reason += (out.reason.empty() ? "" : "; ") + f;
    }
}

enum class Eval
{
    Idle,
    Loading, // the map (re)loading
    Playing,
};

Eval evalState = Eval::Idle;
za::Vector<za::String> evalFiles;
size_t evalIndex = 0;
za::String evalMap = "vrfiringrange";
za::String evalOut;
Options evalOpts;
int evalWait = 0;
bool evalQuit = false; // quit the game when done (scripts)
double evalStart = 0.0;
za::String evalProgress; // a file told how far it is (the review's re-evaluation, vr_motion_review.cpp)

void writeProgress(const za::String& text)
{
    if(evalProgress.empty())
    {
        return;
    }
    const za::String p = files::isRelative(evalProgress) ? files::join(motionsDir(), evalProgress) : evalProgress;
    if(FILE* f = fopen(p.cStr(), "wb"))
    {
        fputs(text.cStr(), f);
        fclose(f);
    }
}

// developer 0 while a map loads (the user's value kept, and put back).
bool quiet = false;
za::String userDeveloper;
void quietLoad(bool on)
{
    cvar_t* developer = Cvar_FindVar("developer");
    if(!developer || on == quiet)
    {
        return;
    }
    quiet = on;
    if(on)
    {
        userDeveloper = developer->string;
        Cvar_SetQuick(developer, "0");
    }
    else
    {
        Cvar_SetQuick(developer, userDeveloper.cStr());
    }
}

// host_maxfps raised while evaluating: the frames' game time is the takes' own (hostFrameTime), so only the
// wall clock between them changes (the server stays at 72 Hz: host_maxfps is above 72 either way).
za::String userMaxfps;
void fastFrames(bool on)
{
    cvar_t* maxfps = Cvar_FindVar("host_maxfps");
    if(!maxfps)
    {
        return;
    }
    if(on && userMaxfps.empty())
    {
        userMaxfps = maxfps->string;
        if(maxfps->value > 72.f && maxfps->value < 1000.f)
        {
            Cvar_SetQuick(maxfps, "1000");
        }
    }
    else if(!on && !userMaxfps.empty())
    {
        Cvar_SetQuick(maxfps, userMaxfps.cStr());
        userMaxfps.clear();
    }
}

void evalDone(const Report& r)
{
    Result res;
    res.path = evalIndex < evalFiles.size() ? evalFiles[evalIndex] : r.file;
    res.file = r.file;
    za::String label = take.label;
    res.label = label;
    const Expectation* e = nullptr;
    if(const auto it = expectations.find(label); it != expectations.end())
    {
        e = &it->second;
    }
    else if(const auto jt = expectations.find(categoryOf(label)); jt != expectations.end())
    {
        e = &jt->second;
    }
    judge(r, e, res);
    res.replayed = eventsText(r.replayed, r.replayedT, true);
    res.recorded = eventsText(r.recorded, r.recordedT, true);
    const Match m = matchHits(r);
    const auto source = take.header.find("source");
    const bool synthetic = source != take.header.end() && source->second.rfind("synthetic", 0) == 0;
    res.same = synthetic ? "-" : (m.matched == m.recordedHits && m.matched == m.replayedHits ? "yes" : "no");
    res.frames = r.frames;
    res.err = r.errCount ? za::max(r.errMax[HAND_MAIN], r.errMax[HAND_OFF]) : -1.0;
    if(!r.warnings.empty())
    {
        res.reason += (res.reason.empty() ? "" : "; ") + r.warnings;
    }
    Con_Printf("%-5s %s: %s%s%s%s\n", res.verdict.cStr(), res.file.cStr(), res.replayed.cStr(),
        res.reason.empty() ? "" : " -- ", res.reason.cStr(),
        res.same == "no" ? va(" [live: %s]", res.recorded.cStr()) : "");
    results.pushBack(ZA_MOVE(res));
    evalIndex++;
    writeProgress(va("%d/%d %s", static_cast<int>(evalIndex), static_cast<int>(evalFiles.size()), r.file.cStr()));
    evalState = Eval::Loading;
    evalWait = -1;
}

void writeResults()
{
    fixedLoading = false;
    quietLoad(false);
    fastFrames(false);
    za::String path = evalOut;
    if(path.empty())
    {
        const time_t now = time(nullptr);
        char stamp[64];
        strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
        path = motionsDir() + "/eval_" + stamp + ".csv";
    }
    else if(files::isRelative(path))
    {
        path = motionsDir() + "/" + path;
    }
    FILE* f = fopen(path.cStr(), "wb");
    int pass = 0, fail = 0, na = 0, none = 0;
    ankerl::unordered_dense::map<za::String, qza::Pair<int, int>> perLabel; // passes, judged
    for(const Result& r : results)
    {
        pass += r.verdict == "PASS";
        fail += r.verdict == "FAIL" || r.verdict == "ERROR";
        na += r.verdict == "N/A";
        none += r.verdict == "-";
        if(r.verdict == "PASS" || r.verdict == "FAIL")
        {
            auto& c = perLabel[categoryOf(r.label).empty() ? r.label : categoryOf(r.label)];
            c.first += r.verdict == "PASS";
            c.second++;
        }
    }
    if(f)
    {
        fprintf(f, "# vr_motion_eval: %d takes, %d pass, %d fail, %d n/a, %d without an expectation (%s, %s)\n",
            static_cast<int>(results.size()), pass, fail, na, none, evalMap.cStr(), expectPath.cStr());
        fprintf(f, "file,label,weapons,expected,verdict,reason,events,recorded_events,same_hits_as_recorded,frames,hand_error_u\n");
        const auto cell = [](za::String s) {
            za::replace(s.begin(), s.end(), ',', ' ');
            return s;
        };
        for(const Result& r : results)
        {
            fprintf(f, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%s\n", cell(r.file).cStr(), cell(r.label).cStr(), r.weapons.cStr(),
                cell(r.expectation).cStr(), r.verdict.cStr(), cell(r.reason).cStr(), cell(r.replayed).cStr(),
                cell(r.recorded).cStr(), r.same.cStr(), r.frames, r.err >= 0.0 ? va("%.3f", r.err) : "");
        }
        fclose(f);
    }
    Con_Printf("vr_motion_eval: %d takes: %d pass, %d fail, %d n/a, %d without an expectation (%.0f s)\n",
        static_cast<int>(results.size()), pass, fail, na, none, Sys_DoubleTime() - evalStart);
    int same = 0, compared = 0;
    for(const Result& r : results)
    {
        compared += r.same == "yes" || r.same == "no";
        same += r.same == "yes";
    }
    if(compared)
    {
        Con_Printf("vr_motion_eval: %d of %d replays hit as their takes did live (the same hits: kind, sub, hand, target, point)\n",
            same, compared);
    }
    for(const auto* entry : qza::sortedByKey(perLabel)) // (in the labels' order, as a std::map had them)
    {
        const auto& [label, c] = *entry;
        Con_Printf("  %-24s %d/%d\n", label.cStr(), c.first, c.second);
    }
    Con_Printf("vr_motion_eval: the table is %s\n", f ? path.cStr() : "(could not be written)");

    // Each take's verdict next to it (eval_status.csv: what Review Takes lists), from an evaluation as the takes are
    // judged: the current melee settings, the takes' own rate, the firing range.
    if(evalOpts.rate > 0.f || evalOpts.recorded || q_strcasecmp(evalMap.cStr(), "vrfiringrange") != 0)
    {
        Con_Printf("vr_motion_eval: (not the takes' verdicts: rate, recorded or another map; eval_status.csv unchanged)\n");
    }
    else
    {
        za::Vector<review::Verdict> verdicts;
        for(const Result& r : results)
        {
            verdicts.pushBack({r.path, r.label, r.weapons, r.expectation, r.verdict, r.reason, r.replayed, r.recorded,
                r.same, r.frames, r.err});
        }
        review::recordEval(verdicts);
    }
    writeProgress(va("done %d", static_cast<int>(results.size())));
}

void evalFrame()
{
    if(evalState == Eval::Idle || state != State::Idle)
    {
        return;
    }
    if(evalState == Eval::Loading)
    {
        if(evalWait < 0)
        {
            if(evalIndex >= evalFiles.size())
            {
                evalState = Eval::Idle;
                writeResults();
                if(evalQuit)
                {
                    // (A few frames after the disconnect: quit in a game asks first.)
                    Cbuf_AddText("disconnect\nwait\nwait\nwait\nwait\nwait\nquit\n");
                }
                return;
            }
            // Each take in the map loaded afresh: the same start every time. Quietly (developer 0: a load's
            // thousands of "can't find" lines for textures cost seconds); the take plays at the user's.
            quietLoad(true);
            fixedLoading = true;
            srand(1);
            Cbuf_AddText(va("map %s\n", evalMap.cStr()));
            evalWait = 0;
            return;
        }
        if(!sv.active || cls.signon != SIGNONS || !hands::current().valid)
        {
            evalWait = 0;
            return;
        }
        if(++evalWait < 20)
        {
            return;
        }
        evalState = Eval::Playing;
        quietLoad(false);
        fixedLoading = false;
        onDone = evalDone;
        if(!startPlayback(evalFiles[evalIndex], evalOpts))
        {
            Result res;
            res.path = evalFiles[evalIndex];
            res.file = za::String{files::fileName(evalFiles[evalIndex])};
            res.verdict = "ERROR";
            res.reason = "could not play";
            results.pushBack(res);
            evalIndex++;
            evalState = Eval::Loading;
            evalWait = -1;
        }
    }
}

// The takes a vr_motion_eval argument names: a folder (its .csv takes), a pattern (in motions/, or
// in its folder), or a take; by default every take in motions/.
[[nodiscard]] za::Vector<za::String> collectTakes(const za::String& arg)
{
    za::Vector<za::String> out;
    const auto isTake = [](za::StringView name) {
        const za::StringView dot = name.substrByPosLen(za::min(name.rfind('.'), name.size()));
        return dot == ".csv" && name.rfind('.') != 0 && !name.startsWith("eval_");
    };
    const auto folder = [&](const za::String& dir, const za::StringView* pattern) {
        files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
            if(!isDirectory && isTake(name) && (!pattern || files::globMatch(*pattern, name)))
            {
                out.pushBack(files::join(dir, name));
            }
        });
    };
    if(arg.empty())
    {
        folder(motionsDir(), nullptr);
    }
    else
    {
        const za::String bases[] = {arg, files::join(motionsDir(), arg), files::join(com_gamedir, arg)};
        bool found = false;
        for(const za::String& b : bases)
        {
            if(files::isDirectory(b.cStr()))
            {
                folder(b, nullptr);
                found = true;
                break;
            }
            if(files::isFile(b.cStr()))
            {
                out.pushBack(b);
                found = true;
                break;
            }
        }
        if(!found)
        {
            const za::String dir{files::parentPath(arg)};
            const za::StringView pattern = files::fileName(arg); // (a glob: * and ?, either case)
            for(const za::String& d : {dir, files::join(motionsDir(), dir), files::join(com_gamedir, dir)})
            {
                if(!d.empty() && files::isDirectory(d.cStr()))
                {
                    folder(d, &pattern);
                    if(!out.empty())
                    {
                        break;
                    }
                }
            }
        }
    }
    za::quickSort(out.begin(), out.end());
    return out;
}

// vr_motion_eval [<folder, pattern or take>] [list <file>] [map <name>] [out <file>] [rate <hz>] [save] [recorded] [verbose]
// [watch] [progress <file>] [quit];
// vr_motion_eval stop
void eval_f()
{
    if(Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "stop"))
    {
        if(evalState != Eval::Idle)
        {
            evalState = Eval::Idle;
            stopPlayback("vr_motion_eval stop");
            writeResults();
        }
        return;
    }
    if(evalState != Eval::Idle || state != State::Idle)
    {
        Con_Printf("vr_motion_eval: busy (vr_motion_eval stop)\n");
        return;
    }
    const Backend* be = backend();
    if((!be || strcmp(be->name(), "mock") != 0) && backendRestartPending())
    {
        // Straight from a start-up script ("vr_backend mock;vr_motion_eval ..."): the backend starts with the next
        // frame (VR_BeginFrame). The command again then, first in the buffer (before quake.rc's vr_startgame).
        Cbuf_InsertText(va("wait\n%s %s\n", Cmd_Argv(0), Cmd_Args() ? Cmd_Args() : ""));
        return;
    }
    if(!be || strcmp(be->name(), "mock") != 0)
    {
        Con_Printf("vr_motion_eval: plays in the mock headset only (vr_backend mock)\n");
        return;
    }
    za::String arg, list;
    evalOpts = Options{};
    evalProgress.clear();
    evalOpts.quiet = true;
    evalMap = "vrfiringrange";
    evalOut.clear();
    evalQuit = false;
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        const char* a = Cmd_Argv(i);
        if(!q_strcasecmp(a, "map") && i + 1 < Cmd_Argc())
        {
            evalMap = Cmd_Argv(++i);
        }
        else if(!q_strcasecmp(a, "out") && i + 1 < Cmd_Argc())
        {
            evalOut = Cmd_Argv(++i);
        }
        else if(!q_strcasecmp(a, "rate") && i + 1 < Cmd_Argc())
        {
            evalOpts.rate = Q_atof(Cmd_Argv(++i));
        }
        else if(!q_strcasecmp(a, "save"))
        {
            evalOpts.save = true;
        }
        else if(!q_strcasecmp(a, "recorded"))
        {
            evalOpts.recorded = true;
        }
        else if(!q_strcasecmp(a, "watch"))
        {
            evalOpts.watch = true;
        }
        else if(!q_strcasecmp(a, "verbose"))
        {
            evalOpts.quiet = false;
        }
        else if(!q_strcasecmp(a, "quit"))
        {
            evalQuit = true;
        }
        else if(!q_strcasecmp(a, "list") && i + 1 < Cmd_Argc())
        {
            list = Cmd_Argv(++i);
        }
        else if(!q_strcasecmp(a, "progress") && i + 1 < Cmd_Argc())
        {
            evalProgress = Cmd_Argv(++i);
        }
        else if(arg.empty())
        {
            arg = a;
        }
        else
        {
            Con_Printf("vr_motion_eval: unknown option \"%s\"\n", a);
            return;
        }
    }
    if(!list.empty())
    {
        // A file of takes, a path a line (relative: to motions/): the review's Re-evaluate.
        const za::String lp = files::isRelative(list) ? files::join(motionsDir(), list) : list;
        za::String text;
        if(!files::readText(lp.cStr(), text, files::Mode::Binary))
        {
            Con_Printf("vr_motion_eval: can't read %s\n", lp.cStr());
            return;
        }
        evalFiles.clear();
        files::forLines(text, [](za::StringView line) {
            while(line.endsWith('\r') || line.endsWith(' '))
            {
                line.removeSuffix(1);
            }
            if(line.empty())
            {
                return;
            }
            const za::String p = files::isRelative(line) ? files::join(motionsDir(), line) : za::String{line};
            if(files::isFile(p.cStr()))
            {
                evalFiles.pushBack(p);
            }
            else
            {
                Con_Printf("vr_motion_eval: no take %s (left out)\n", p.cStr());
            }
        });
    }
    else
    {
        evalFiles = collectTakes(arg);
    }
    if(evalFiles.empty())
    {
        Con_Printf("vr_motion_eval: no takes in \"%s\"\n", arg.empty() ? motionsDir().cStr() : arg.cStr());
        return;
    }
    loadExpectations();
    results.clear();
    evalIndex = 0;
    evalState = Eval::Loading;
    evalWait = -1;
    evalStart = Sys_DoubleTime();
    if(!evalOpts.watch)
    {
        fastFrames(true);
    }
    Con_Printf("vr_motion_eval: %d takes, each in %s\n", static_cast<int>(evalFiles.size()), evalMap.cStr());
}

} // namespace

bool evaluating()
{
    return evalState != Eval::Idle;
}

void initPlayback()
{
    Cmd_AddCommand("vr_motion_play", play_f);
    Cmd_AddCommand("vr_motion_eval", eval_f);
}

} // namespace qvr::motion
