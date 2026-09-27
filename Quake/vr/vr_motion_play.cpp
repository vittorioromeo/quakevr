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
#include "vr_hands.hpp"
#include "vr_main.hpp"
#include "vr_progs.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace qvr::motion
{

Row captureRow(bool tick, double svDt); // vr_motion.cpp: this frame's row, as the recorder makes it
std::string resolveTake(const std::string& arg);

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
    std::vector<Event> events;

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
    std::string monClass;
    glm::vec3 monW{0.f};
    float monYawW{0.f};
    glm::vec3 monPF{0.f};
    int wid[HAND_COUNT]{};
    int wflags[HAND_COUNT]{};
};

struct Take
{
    std::string path;
    std::string name; // the file's name
    std::map<std::string, std::string> header;
    std::string label, category, detail, map;
    bool hasYaw0{false};
    float yaw0{0.f};
    bool hasTicks{false};
    bool hasPlayYaw{false};
    bool hasWeapons{false};
    std::vector<Frame> frames;
    size_t firstRec{0};
};

[[nodiscard]] std::vector<std::string> split(const std::string& s, char sep)
{
    std::vector<std::string> out;
    std::string cur;
    for(const char c : s)
    {
        if(c == sep)
        {
            out.push_back(cur);
            cur.clear();
        }
        else if(c != '\r')
        {
            cur += c;
        }
    }
    out.push_back(cur);
    return out;
}

[[nodiscard]] std::string trim(const std::string& s)
{
    const size_t a = s.find_first_not_of(" \t\r\n");
    if(a == std::string::npos)
    {
        return "";
    }
    const size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// The events of a row: kind:sub:hand:value:x:y:z:target:detail, ';' between them.
[[nodiscard]] std::vector<Event> parseEvents(const std::string& cell)
{
    std::vector<Event> out;
    if(cell.empty())
    {
        return out;
    }
    for(const std::string& e : split(cell, ';'))
    {
        const std::vector<std::string> f = split(e, ':');
        if(f.size() < 9)
        {
            continue;
        }
        Event ev;
        ev.kind = f[0];
        ev.sub = f[1];
        ev.hand = f[2] == "main" ? HAND_MAIN : f[2] == "off" ? HAND_OFF : -1;
        ev.value = std::strtof(f[3].c_str(), nullptr);
        ev.hasAt = !f[4].empty();
        if(ev.hasAt)
        {
            ev.at = {std::strtof(f[4].c_str(), nullptr), std::strtof(f[5].c_str(), nullptr), std::strtof(f[6].c_str(), nullptr)};
        }
        ev.target = f[7];
        ev.detail = f[8];
        out.push_back(std::move(ev));
    }
    return out;
}

// A take file (any columns missing take their defaults: a synthetic take needs only the tracking).
[[nodiscard]] bool loadTake(const std::string& path, Take& take, std::string& error)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
    {
        error = "can't open " + path;
        return false;
    }
    take = Take{};
    take.path = path;
    take.name = std::filesystem::path(path).filename().string();

    std::string line;
    std::vector<std::string> names;
    std::map<std::string, int> col;
    while(std::getline(in, line))
    {
        if(!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if(line.empty())
        {
            continue;
        }
        if(line[0] == '#')
        {
            const size_t colon = line.find(':');
            if(colon != std::string::npos)
            {
                take.header[trim(line.substr(1, colon - 1))] = trim(line.substr(colon + 1));
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

        const std::vector<std::string> cells = split(line, ',');
        const auto has = [&](const char* name) {
            const auto it = col.find(name);
            return it != col.end() && it->second < static_cast<int>(cells.size()) && !cells[it->second].empty();
        };
        const auto str = [&](const std::string& name) -> std::string {
            const auto it = col.find(name);
            return it != col.end() && it->second < static_cast<int>(cells.size()) ? cells[it->second] : std::string{};
        };
        const auto num = [&](const std::string& name, double def) {
            const std::string v = str(name);
            return v.empty() ? def : std::strtod(v.c_str(), nullptr);
        };
        const auto f = [&](const std::string& name, float def) { return static_cast<float>(num(name, def)); };
        const auto vec = [&](const std::string& a, const std::string& b, const std::string& c) {
            return glm::vec3{f(a, 0.f), f(b, 0.f), f(c, 0.f)};
        };

        Frame fr;
        fr.t = num("t", 0.0);
        fr.dt = num("dt", 1.0 / 90.0);
        const std::string phase = str("phase");
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
        const auto pose = [&](const std::string& p, Pose& out, bool grip) {
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
            const std::string p = h == HAND_MAIN ? "m_" : "o_";
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
            if(has((p + "wid").c_str()))
            {
                take.hasWeapons = true;
                fr.wid[h] = static_cast<int>(num(p + "wid", 0.0));
                fr.wflags[h] = static_cast<int>(num(p + "wflags", 0.0));
            }
            if(has((p + "pos_w_x").c_str()))
            {
                fr.hasW = true;
                fr.handW[h] = vec(p + "pos_w_x", p + "pos_w_y", p + "pos_w_z");
            }
            if(has((p + "pos_d_x_u").c_str()))
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
        take.frames.push_back(std::move(fr));
    }
    if(take.frames.empty())
    {
        error = "no frames in " + path;
        return false;
    }

    const auto h = [&](const char* key) {
        const auto it = take.header.find(key);
        return it == take.header.end() ? std::string{} : it->second;
    };
    take.label = h("label");
    take.category = h("category");
    take.detail = h("detail");
    take.map = h("map");
    if(take.label.empty())
    {
        take.label = take.name.substr(0, take.name.rfind('.'));
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
    if(const std::string y = h("yaw0"); !y.empty())
    {
        take.hasYaw0 = true;
        take.yaw0 = std::strtof(y.c_str(), nullptr);
    }
    else
    {
        // A synthetic take: the head's heading at the start (as vr_hands.cpp turns tracking into Quake's
        // angles), and the play space's turn.
        const Frame& fr = take.frames[take.firstRec];
        const glm::vec3 fwd = fr.tracking.head.orientation * glm::vec3{0.f, 0.f, -1.f};
        take.yaw0 = glm::degrees(std::atan2(-fwd.x, -fwd.z)) + fr.playYaw;
    }
    return true;
}

// ----------------------------------------------------------------------------
// The settings a take was recorded with
// ----------------------------------------------------------------------------

// Those that place the hands, the weapons and the body (applied for the playback, then restored); with
// `recorded`, the melee's own too.
[[nodiscard]] bool placingSetting(const std::string& name, bool melee)
{
    static const char* const placing[] = {"vr_world_scale", "vr_height_calibration", "vr_floor_offset", "vr_lefthanded",
        "vr_gunangle", "vr_gunyaw", "vr_offhandpitch", "vr_offhandyaw", "vr_gunmodel", "vr_weapon_grip_mode", "vr_2h_",
        "vr_lean_", "vr_roomscale_", "vr_body_", "vr_throw_release", "vr_throw_grab_press", "vr_wofs_",
        "vr_controller_legacy_pose", "vr_weapon_cycle_mode"};
    // (Every setting the QC's melee, damage and hit reactions read.)
    static const char* const meleeOnes[] = {"vr_melee_", "vr_bash", "vr_shove", "vr_parry", "vr_deflect", "vr_headbutt",
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
void collectSettings(const std::string& value, bool pairs, std::vector<std::pair<std::string, std::string>>& out)
{
    std::istringstream words(value);
    std::string w;
    if(pairs)
    {
        std::string v;
        while(words >> w >> v)
        {
            out.emplace_back(w, v);
        }
        return;
    }
    while(words >> w)
    {
        const size_t eq = w.find('=');
        if(eq != std::string::npos)
        {
            out.emplace_back(w.substr(0, eq), w.substr(eq + 1));
        }
    }
}

std::vector<std::pair<cvar_t*, std::string>> savedSettings; // the values before a playback

void applySettings(const Take& take, bool melee)
{
    std::vector<std::pair<std::string, std::string>> list;
    const auto h = [&](const char* key) {
        const auto it = take.header.find(key);
        return it == take.header.end() ? std::string{} : it->second;
    };
    if(const std::string all = h("settings"); !all.empty())
    {
        collectSettings(all, false, list);
    }
    else
    {
        // A take from before the settings line: its own lines.
        for(const char* key : {"vr_world_scale", "vr_height_calibration", "vr_floor_offset"})
        {
            if(const std::string v = h(key); !v.empty())
            {
                list.emplace_back(key, v);
            }
        }
        collectSettings(h("hand angles"), true, list);
        collectSettings(h("grips"), true, list);
        if(const std::string d = h("dominant hand"); !d.empty())
        {
            list.emplace_back("vr_lefthanded", d.rfind("left", 0) == 0 ? "1" : "0");
        }
    }
    collectSettings(h("weapon settings"), false, list);
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
        cvar_t* var = Cvar_FindVar(name.c_str());
        if(!var || !strcmp(var->string, value.c_str()))
        {
            continue;
        }
        savedSettings.emplace_back(var, var->string);
        Cvar_SetQuick(var, value.c_str());
        applied++;
    }
    if(applied)
    {
        Con_DPrintf("vr_motion_play: %d settings as recorded\n", applied);
    }
}

void restoreSettings()
{
    for(auto it = savedSettings.rbegin(); it != savedSettings.rend(); ++it)
    {
        Cvar_SetQuick(it->first, it->second.c_str());
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
    std::string target;   // a classname, or #<entity number>; "" the take's monster's class, else the dummy
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
    std::vector<Frame> out;
    const std::vector<Frame>& in = take.frames;
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
        const Frame& b = in[std::min(j + 1, in.size() - 1)];
        const float s = b.t > a.t ? static_cast<float>(std::clamp((t - a.t) / (b.t - a.t), 0.0, 1.0)) : 0.f;
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
        f.playYaw = a.playYaw + std::remainder(b.playYaw - a.playYaw, 360.f) * s;
        // The source frames' events up to this one.
        for(; nextEvents < in.size() && in[nextEvents].t <= t; nextEvents++)
        {
            f.events.insert(f.events.end(), in[nextEvents].events.begin(), in[nextEvents].events.end());
        }
        out.push_back(std::move(f));
    }
    take.frames = std::move(out);
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
    std::string file;
    int frames{0};
    std::vector<Event> recorded; // the take's events (from its start)
    std::vector<Event> replayed; // the replay's
    std::vector<double> recordedT, replayedT;
    double errMax[HAND_COUNT]{};
    double errSum[HAND_COUNT]{};
    int errCount{0};
    int parryFrames[HAND_COUNT]{};
    int parryAnyFrames{0}; // either hand's weapon or crossed arms
    int guardFrames{0};
    int recFrames{0};
    int wid[HAND_COUNT]{};
    std::string targetClass;
    std::string warnings;
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
std::vector<Row> replayRows;
void (*onDone)(const Report&) = nullptr; // the evaluation's

[[nodiscard]] edict_t* findTarget(const std::string& wanted, const std::string& recordedClass, const glm::vec3& from)
{
    if(!wanted.empty() && wanted[0] == '#')
    {
        const int n = std::atoi(wanted.c_str() + 1);
        if(n > 0 && n < qcvm->num_edicts && !EDICT_NUM(n)->free)
        {
            return EDICT_NUM(n);
        }
        return nullptr;
    }
    const auto nearestOf = [&](const std::string& className) -> edict_t* {
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
    edict_t* target = findTarget(opts.target, f0.hasMon ? f0.monClass : std::string{}, here);
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
            const float bearing = glm::degrees(std::atan2(monPF.y, monPF.x));
            yawP = tYaw + 180.f - bearing;
        }
        delta = std::remainder(yawP - take.yaw0, 360.f);
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
        report.targetClass.c_str(), targetEnt, tOrigin.x, tOrigin.y, tOrigin.z, placeOrigin.x, placeOrigin.y, placeOrigin.z,
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
    if(!free && report.warnings.find("overlaps") == std::string::npos)
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
        const bool holds = !take.hasWeapons || f0.wid[h] != 0 || h == HAND_MAIN && f0.wid[HAND_OFF] == 0;
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

[[nodiscard]] std::string eventText(const Event& e)
{
    std::string s = e.kind;
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
    std::vector<bool> used(r.replayed.size(), false);
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
            m.maxDamageDiff = std::max(m.maxDamageDiff,
                std::fabs(static_cast<double>(a.value - b.value)) / std::max(1.0, std::fabs(static_cast<double>(a.value))));
            m.maxTimeDiff = std::max(m.maxTimeDiff, std::fabs(r.recordedT[i] - r.replayedT[j]));
            break;
        }
    }
    return m;
}

[[nodiscard]] std::string eventsText(const std::vector<Event>& events, const std::vector<double>& times, bool hitsAndPushes)
{
    std::string s;
    for(size_t i = 0; i < events.size(); i++)
    {
        const Event& e = events[i];
        if(hitsAndPushes && !isHit(e) && e.kind != "push" && e.kind != "parry" && e.kind != "deflect")
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
    const std::string dir = motionsDir() + "/replays";
    Sys_mkdir(dir.c_str());
    const std::string stem = take.name.substr(0, take.name.rfind('.'));
    const std::string path = dir + "/" + stem + "_replay.csv";
    if(writeTake(path, info, replayRows))
    {
        Con_Printf("vr_motion_play: the replay is motions/replays/%s_replay.csv\n", stem.c_str());
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
        Con_Printf("vr_motion_play: %s: %d frames against %s\n", take.name.c_str(), report.frames,
            report.targetClass.empty() ? "nothing" : report.targetClass.c_str());
        Con_Printf("  recorded: %s\n", eventsText(report.recorded, report.recordedT, true).c_str());
        Con_Printf("  replayed: %s\n", eventsText(report.replayed, report.replayedT, true).c_str());
        if(m.recordedHits || m.replayedHits)
        {
            Con_Printf("  hits: %d recorded, %d replayed, %d matched (damage within %.1f%%, time within %.3f s)\n",
                m.recordedHits, m.replayedHits, m.matched, m.maxDamageDiff * 100.0, m.maxTimeDiff);
        }
        if(report.errCount)
        {
            Con_Printf("  hands vs the take, relative to the dummy: main max %.3f rms %.3f, off max %.3f rms %.3f units\n",
                report.errMax[HAND_MAIN], std::sqrt(report.errSum[HAND_MAIN] / report.errCount), report.errMax[HAND_OFF],
                std::sqrt(report.errSum[HAND_OFF] / report.errCount));
        }
        if(!report.warnings.empty())
        {
            Con_Printf("  note: %s\n", report.warnings.c_str());
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

[[nodiscard]] bool startPlayback(const std::string& path, const Options& o)
{
    std::string error;
    Take t;
    if(!loadTake(path, t, error))
    {
        Con_Printf("vr_motion_play: %s\n", error.c_str());
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
        resample(t, std::clamp(o.rate, 20.f, 500.f));
    }
    take = std::move(t);
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
            report.recorded.push_back(e);
            report.recordedT.push_back(f.t);
        }
    }
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        report.wid[h] = take.frames[take.firstRec].wid[h];
    }
    applySettings(take, opts.recorded);
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
    std::srand(1); // (QC's random() and the engine's rand(): the same draws every time)
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
    const std::string path = resolveTake(Cmd_Argv(1));
    if(path.empty())
    {
        Con_Printf("vr_motion_play: no take \"%s\" (in motions/, the game folder, or a path)\n", Cmd_Argv(1));
        return;
    }
    (void)startPlayback(path, o);
}

} // namespace

// A take's file: as given, in motions/, or in the game folder ("" if none).
std::string resolveTake(const std::string& arg)
{
    for(const std::string& p : {arg, motionsDir() + "/" + arg, std::string{com_gamedir} + "/" + arg})
    {
        for(const std::string& q : {p, p + ".csv"})
        {
            if(Sys_FileType(q.c_str()) == FS_ENT_FILE)
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

double hostFrameTime(double time)
{
    double dt = time;
    if(state == State::Idle && fixedLoading)
    {
        return setupDt;
    }
    if(state == State::Setup || state == State::Post)
    {
        dt = setupDt;
    }
    else if(state == State::Play)
    {
        cur = std::min(nextFrame, take.frames.size() - 1);
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
            std::this_thread::sleep_for(std::chrono::duration<double>(std::min(ahead, 0.1)));
        }
    }
    return dt;
}

int serverFrameOverride(double& frametime)
{
    if(state == State::Setup || state == State::Post || (state == State::Idle && fixedLoading))
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
    tracking = f.tracking;
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
    tracking.input.hands[HAND_MAIN].stick.x = 0.f;
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

void playFrameEnd(std::vector<Event>& events, bool tick, double svDt)
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
                    report.replayed.push_back(e);
                    report.replayedT.push_back(post ? f.t + postElapsed : f.t);
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
                    report.errMax[h] = std::max(report.errMax[h], e);
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
                replayRows.push_back(std::move(r));
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
    std::vector<std::string> required;  // kind[/sub][@point|point] (any one of them)
    std::vector<std::string> forbidden; // kind[/sub]
    bool none{false};
    std::vector<std::string> poses;     // parry, guard: held for half the take
    std::vector<std::string> notPoses;  // parry, guard: never, in the take
    std::vector<std::string> weapons;   // the weapon classes it applies to (empty: any)
    bool skip{false};                   // "-": reported only
};

std::map<std::string, Expectation> expectations;
std::string expectPath;

[[nodiscard]] std::string underscores(std::string s)
{
    std::replace(s.begin(), s.end(), ' ', '_');
    return s;
}

void loadExpectations()
{
    expectations.clear();
    expectPath = motionsDir() + "/expect.cfg";
    std::ifstream in(expectPath);
    if(!in)
    {
        Con_Printf("vr_motion_eval: no %s: every take is reported without a verdict\n", expectPath.c_str());
        return;
    }
    std::string line;
    while(std::getline(in, line))
    {
        const size_t hash = line.find('#');
        if(hash != std::string::npos)
        {
            line = line.substr(0, hash);
        }
        std::istringstream words(line);
        std::string label, w;
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
                e.notPoses.push_back(w.substr(6));
            }
            else if(w[0] == '!')
            {
                e.forbidden.push_back(w.substr(1));
            }
            else if(w.rfind("pose:", 0) == 0)
            {
                e.poses.push_back(w.substr(5));
            }
            else if(w.rfind("weapon:", 0) == 0)
            {
                for(const std::string& c : split(w.substr(7), '|'))
                {
                    e.weapons.push_back(c);
                }
            }
            else
            {
                e.required.push_back(w);
            }
        }
        expectations[label] = std::move(e);
    }
}

// A weapon id's class, as expect.cfg names them.
[[nodiscard]] std::string weaponClass(int wid)
{
    switch(wid)
    {
        case 0: return "fist";
        case 1: return "grapple";
        case 2: return "axe";
        case 3: return "mjolnir";
        case 13: return "sword";
        default: return wid >= 4 && wid <= 12 ? "gun" : "other";
    }
}

[[nodiscard]] bool matchesItem(const Event& e, const std::string& item)
{
    std::string kind = item;
    std::string points;
    if(const size_t at = kind.find('@'); at != std::string::npos)
    {
        points = kind.substr(at + 1);
        kind = kind.substr(0, at);
    }
    std::string sub;
    if(const size_t slash = kind.find('/'); slash != std::string::npos)
    {
        sub = kind.substr(slash + 1);
        kind = kind.substr(0, slash);
    }
    if(e.kind != kind || (!sub.empty() && e.sub != sub))
    {
        return false;
    }
    if(!points.empty())
    {
        const std::vector<std::string> list = split(points, '|');
        return std::find(list.begin(), list.end(), underscores(e.detail)) != list.end();
    }
    return true;
}

struct Result
{
    std::string path; // the take's file
    std::string file, label, weapons, expectation, verdict, reason, replayed, recorded, same;
    int frames{0};
    double err{0.0};
};

std::vector<Result> results;

[[nodiscard]] std::string expectationText(const Expectation& e)
{
    std::string s;
    const auto add = [&](const std::string& w) { s += (s.empty() ? "" : " ") + w; };
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
        std::string w;
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
    const std::string main = weaponClass(r.wid[HAND_MAIN]);
    const std::string off = weaponClass(r.wid[HAND_OFF]);
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
    if(!e->weapons.empty() && std::find(e->weapons.begin(), e->weapons.end(), main) == e->weapons.end() &&
       std::find(e->weapons.begin(), e->weapons.end(), off) == e->weapons.end())
    {
        out.verdict = "N/A";
        out.reason = "not for a " + main + " (expected with " + [&] {
            std::string w;
            for(const auto& c : e->weapons)
            {
                w += (w.empty() ? "" : " or ") + c;
            }
            return w;
        }() + ")";
        return;
    }

    std::vector<std::string> fails;
    // Required (any one): an event of one of them, a hit against the target.
    if(!e->required.empty())
    {
        bool any = false;
        for(const Event& ev : r.replayed)
        {
            for(const std::string& item : e->required)
            {
                if(matchesItem(ev, item) && (!isHit(ev) || ev.target == r.targetClass))
                {
                    any = true;
                }
            }
        }
        if(!any)
        {
            fails.push_back("no " + [&] {
                std::string w;
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
        for(const std::string& item : e->forbidden)
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
            for(const std::string& item : e->required)
            {
                const std::string kind = item.substr(0, item.find_first_of("/@"));
                expected = expected || kind == ev.kind;
            }
            bad = bad || !expected;
        }
        if(bad)
        {
            fails.push_back("unexpected " + eventText(ev));
        }
    }
    // Poses: held for at least half the take.
    for(const std::string& p : e->poses)
    {
        const int held = p == "parry" ? r.parryAnyFrames
                         : p == "guard" ? r.guardFrames
                                        : 0;
        if(r.recFrames == 0 || held * 2 < r.recFrames)
        {
            fails.push_back(va("%s pose held %d of %d frames", p.c_str(), held, r.recFrames));
        }
    }
    // Poses that must never be: not a frame of the take in them.
    for(const std::string& p : e->notPoses)
    {
        const int held = p == "parry" ? r.parryAnyFrames : p == "guard" ? r.guardFrames : 0;
        if(held > 0)
        {
            fails.push_back(va("%s pose in %d of %d frames", p.c_str(), held, r.recFrames));
        }
    }
    out.verdict = fails.empty() ? "PASS" : "FAIL";
    for(const std::string& f : fails)
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
std::vector<std::string> evalFiles;
size_t evalIndex = 0;
std::string evalMap = "vrfiringrange";
std::string evalOut;
Options evalOpts;
int evalWait = 0;
bool evalQuit = false; // quit the game when done (scripts)
double evalStart = 0.0;
std::string evalProgress; // a file told how far it is (the review's re-evaluation, vr_motion_review.cpp)

void writeProgress(const std::string& text)
{
    if(evalProgress.empty())
    {
        return;
    }
    std::filesystem::path p = evalProgress;
    if(p.is_relative())
    {
        p = std::filesystem::path{motionsDir()} / p;
    }
    if(FILE* f = fopen(p.string().c_str(), "wb"))
    {
        fputs(text.c_str(), f);
        fclose(f);
    }
}

void evalNext();

// developer 0 while a map loads (the user's value kept, and put back).
bool quiet = false;
std::string userDeveloper;
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
        Cvar_SetQuick(developer, userDeveloper.c_str());
    }
}

// host_maxfps raised while evaluating: the frames' game time is the takes' own (hostFrameTime), so only the
// wall clock between them changes (the server stays at 72 Hz: host_maxfps is above 72 either way).
std::string userMaxfps;
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
        Cvar_SetQuick(maxfps, userMaxfps.c_str());
        userMaxfps.clear();
    }
}

void evalDone(const Report& r)
{
    Result res;
    res.path = evalIndex < evalFiles.size() ? evalFiles[evalIndex] : r.file;
    res.file = r.file;
    std::string label = take.label;
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
    res.err = r.errCount ? std::max(r.errMax[HAND_MAIN], r.errMax[HAND_OFF]) : -1.0;
    if(!r.warnings.empty())
    {
        res.reason += (res.reason.empty() ? "" : "; ") + r.warnings;
    }
    Con_Printf("%-5s %s: %s%s%s%s\n", res.verdict.c_str(), res.file.c_str(), res.replayed.c_str(),
        res.reason.empty() ? "" : " -- ", res.reason.c_str(),
        res.same == "no" ? va(" [live: %s]", res.recorded.c_str()) : "");
    results.push_back(std::move(res));
    evalIndex++;
    writeProgress(va("%d/%d %s", static_cast<int>(evalIndex), static_cast<int>(evalFiles.size()), r.file.c_str()));
    evalState = Eval::Loading;
    evalWait = -1;
}

void writeResults()
{
    fixedLoading = false;
    quietLoad(false);
    fastFrames(false);
    std::string path = evalOut;
    if(path.empty())
    {
        const std::time_t now = std::time(nullptr);
        char stamp[64];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", std::localtime(&now));
        path = motionsDir() + "/eval_" + stamp + ".csv";
    }
    else if(std::filesystem::path(path).is_relative())
    {
        path = motionsDir() + "/" + path;
    }
    FILE* f = fopen(path.c_str(), "wb");
    int pass = 0, fail = 0, na = 0, none = 0;
    std::map<std::string, std::pair<int, int>> perLabel; // passes, judged
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
            static_cast<int>(results.size()), pass, fail, na, none, evalMap.c_str(), expectPath.c_str());
        fprintf(f, "file,label,weapons,expected,verdict,reason,events,recorded_events,same_hits_as_recorded,frames,hand_error_u\n");
        const auto cell = [](std::string s) {
            std::replace(s.begin(), s.end(), ',', ' ');
            return s;
        };
        for(const Result& r : results)
        {
            fprintf(f, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%s\n", cell(r.file).c_str(), cell(r.label).c_str(), r.weapons.c_str(),
                cell(r.expectation).c_str(), r.verdict.c_str(), cell(r.reason).c_str(), cell(r.replayed).c_str(),
                cell(r.recorded).c_str(), r.same.c_str(), r.frames, r.err >= 0.0 ? va("%.3f", r.err) : "");
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
    for(const auto& [label, c] : perLabel)
    {
        Con_Printf("  %-24s %d/%d\n", label.c_str(), c.first, c.second);
    }
    Con_Printf("vr_motion_eval: the table is %s\n", f ? path.c_str() : "(could not be written)");

    // Each take's verdict next to it (eval_status.csv: what Review Takes lists), from an evaluation as the takes are
    // judged: the current melee settings, the takes' own rate, the firing range.
    if(evalOpts.rate > 0.f || evalOpts.recorded || q_strcasecmp(evalMap.c_str(), "vrfiringrange") != 0)
    {
        Con_Printf("vr_motion_eval: (not the takes' verdicts: rate, recorded or another map; eval_status.csv unchanged)\n");
    }
    else
    {
        std::vector<review::Verdict> verdicts;
        for(const Result& r : results)
        {
            verdicts.push_back({r.path, r.label, r.weapons, r.expectation, r.verdict, r.reason, r.replayed, r.recorded,
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
            std::srand(1);
            Cbuf_AddText(va("map %s\n", evalMap.c_str()));
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
            res.file = std::filesystem::path(evalFiles[evalIndex]).filename().string();
            res.verdict = "ERROR";
            res.reason = "could not play";
            results.push_back(res);
            evalIndex++;
            evalState = Eval::Loading;
            evalWait = -1;
        }
    }
}

// A glob's (* ?) regular expression.
[[nodiscard]] std::regex globRegex(const std::string& glob)
{
    std::string re;
    for(const char c : glob)
    {
        if(c == '*')
        {
            re += ".*";
        }
        else if(c == '?')
        {
            re += '.';
        }
        else if(std::strchr(".+()[]{}^$|\\", c))
        {
            re += '\\';
            re += c;
        }
        else
        {
            re += c;
        }
    }
    return std::regex{re, std::regex::icase};
}

// The takes a vr_motion_eval argument names: a folder (its .csv takes), a pattern (in motions/, or
// in its folder), or a take; by default every take in motions/.
[[nodiscard]] std::vector<std::string> collectTakes(const std::string& arg)
{
    namespace fs = std::filesystem;
    std::vector<std::string> out;
    std::error_code ec;
    const auto isTake = [](const fs::path& p) {
        const std::string name = p.filename().string();
        return p.extension() == ".csv" && name.rfind("eval_", 0) != 0;
    };
    const auto folder = [&](const fs::path& dir, const std::regex* pattern) {
        for(const auto& entry : fs::directory_iterator(dir, ec))
        {
            if(entry.is_regular_file() && isTake(entry.path()) &&
               (!pattern || std::regex_match(entry.path().filename().string(), *pattern)))
            {
                out.push_back(entry.path().string());
            }
        }
    };
    if(arg.empty())
    {
        folder(motionsDir(), nullptr);
    }
    else
    {
        fs::path p = arg;
        std::vector<fs::path> bases{p, fs::path{motionsDir()} / p, fs::path{com_gamedir} / p};
        bool found = false;
        for(const fs::path& b : bases)
        {
            if(fs::is_directory(b, ec))
            {
                folder(b, nullptr);
                found = true;
                break;
            }
            if(fs::is_regular_file(b, ec))
            {
                out.push_back(b.string());
                found = true;
                break;
            }
        }
        if(!found)
        {
            const fs::path dir = p.has_parent_path() ? p.parent_path() : fs::path{};
            const std::regex pattern = globRegex(p.filename().string());
            for(const fs::path& d : {dir, fs::path{motionsDir()} / dir, fs::path{com_gamedir} / dir})
            {
                if(!d.empty() && fs::is_directory(d, ec))
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
    std::sort(out.begin(), out.end());
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
    if(!be || strcmp(be->name(), "mock") != 0)
    {
        Con_Printf("vr_motion_eval: plays in the mock headset only (vr_backend mock)\n");
        return;
    }
    std::string arg, list;
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
        std::filesystem::path lp = list;
        if(lp.is_relative())
        {
            lp = std::filesystem::path{motionsDir()} / lp;
        }
        std::ifstream in(lp, std::ios::binary);
        if(!in)
        {
            Con_Printf("vr_motion_eval: can't read %s\n", lp.string().c_str());
            return;
        }
        evalFiles.clear();
        std::string line;
        std::error_code ec;
        while(std::getline(in, line))
        {
            while(!line.empty() && (line.back() == '\r' || line.back() == ' '))
            {
                line.pop_back();
            }
            if(line.empty())
            {
                continue;
            }
            std::filesystem::path p = line;
            if(p.is_relative())
            {
                p = std::filesystem::path{motionsDir()} / p;
            }
            if(std::filesystem::is_regular_file(p, ec))
            {
                evalFiles.push_back(p.string());
            }
            else
            {
                Con_Printf("vr_motion_eval: no take %s (left out)\n", p.string().c_str());
            }
        }
    }
    else
    {
        evalFiles = collectTakes(arg);
    }
    if(evalFiles.empty())
    {
        Con_Printf("vr_motion_eval: no takes in \"%s\"\n", arg.empty() ? motionsDir().c_str() : arg.c_str());
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
    Con_Printf("vr_motion_eval: %d takes, each in %s\n", static_cast<int>(evalFiles.size()), evalMap.c_str());
}

} // namespace

void initPlayback()
{
    Cmd_AddCommand("vr_motion_play", play_f);
    Cmd_AddCommand("vr_motion_eval", eval_f);
}

} // namespace qvr::motion
