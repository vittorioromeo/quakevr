// vr_tips.cpp -- see vr_tips.hpp.

#include "vr_tips.hpp"
#include "vr_cvars.hpp"
#include "vr_debris.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gadget.hpp"
#include "vr_hands.hpp"
#include "vr_mem.hpp"
#include "vr_protocol.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"
#include "vr_walltorch.hpp"

#include "Zancle/Algorithm/LowerBound.hpp"
#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include <string.h>

using namespace qvr::protocol;

namespace qvr::tips
{
namespace
{

struct Tip
{
    const char* name; // its key in tips_seen.txt
    const char* text; // its lines (the panel's and the hologram's)
    bool (*about)(const entity_t& e);
};

[[nodiscard]] bool wallTorch(const entity_t& e)
{
    return walltorch::onWall(e);
}

constexpr Tip tips[] = {
    {"walltorch", "Wall torches can be taken:\ngrip one and pull it\noff its wall. Hit enemies\nwith it or throw it to\nset them on fire.",
        wallTorch},
};
constexpr int tipCount = static_cast<int>(sizeof(tips) / sizeof(tips[0]));

constexpr int maxMapTips = 64; // func_vr_tip's in one map

// The server's list, made by the vr_tip_* builtins while the map spawns (the main thread, the QC VM), and the
// client's mirror of it (the client thread): the map's tips, after the built-in ones.
za::Vector<MapTip> serverTips;
za::Vector<MapTip> mapTips;

struct TipsScratch
{
    za::String seenKey; // a map tip's key in tips_seen.txt (seenKeyOf)
    auto members() { return mem::list(seenKey); }
};
mem::Scratch<TipsScratch> scratch{"tips"};

// The tip waiting to show: what it is about has been near and seen since `since`.
struct Candidate
{
    int tip{-1};
    int ent{0};
    double since{0.0};
};
Candidate candidate;

// The tip showing as a floating panel.
struct Showing
{
    int tip{-1};
    int ent{0};
    double start{0.0};
    double until{0.0};
    glm::vec3 panel{0.f}; // its place, following what it is about smoothly
    glm::vec3 lastTarget{0.f}; // retained while its entity disappears and the panel fades
    bool placed{false};
    double lastFrame{0.0};
};
Showing showing;
int pendingTest = -1; // vr_tips_test's tip, shown once back in the game (the menu or the console closed)

// The tips, in the order they are considered: the built-in ones, then the map's. `t` indexes both.
[[nodiscard]] int tipTotal()
{
    return tipCount + static_cast<int>(mapTips.size());
}

[[nodiscard]] MapTip* mapTipOf(int t)
{
    return t >= tipCount && t < tipTotal() ? &mapTips[static_cast<size_t>(t - tipCount)] : nullptr;
}

[[nodiscard]] const char* tipText(int t)
{
    const MapTip* mt = mapTipOf(t);
    return mt ? mt->text.cStr() : tips[t].text;
}

[[nodiscard]] const char* tipName(int t)
{
    const MapTip* mt = mapTipOf(t);
    return mt ? (mt->name.empty() ? "<unnamed>" : mt->name.cStr()) : tips[t].name;
}

// A tip's key in tips_seen.txt: a built-in tip's is its name; a map tip's is its map and name, so the same name in
// two maps is two tips, and an unnamed one is keyed by its place in the map's list.
[[nodiscard]] const char* seenKeyOf(int t)
{
    const MapTip* mt = mapTipOf(t);
    if(!mt)
    {
        return tips[t].name;
    }
    za::String& key = scratch.seenKey;
    key = za::String{cl.mapname[0] ? cl.mapname : "map"};
    if(!mt->name.empty())
    {
        key += ':';
        // A key is a word (tips_seen.txt is read split at spaces and line ends; it was vr_tips_seen, in the config in
        // quotes): a name's spaces, quotes and semicolons become '_' (else its key never matches, and the tip shows
        // again and again).
        for(const char c : za::StringView{mt->name.cStr()})
        {
            key += static_cast<unsigned char>(c) <= ' ' || c == '"' || c == ';' ? '_' : c;
        }
    }
    else
    {
        key += va("#%d", t - tipCount);
    }
    return key.cStr();
}

// The tips shown already: their keys (seenKeyOf), sorted, as in <gamedir>/tips_seen.txt (one a line). Read from the
// file the first time they are asked for in a game folder; written whole through a temporary file as one is added.
struct SeenList
{
    za::String dir; // the game folder they were read from (another one: read again)
    bool loaded{false};
    za::Vector<za::String> keys;
};
SeenList seenList;

[[nodiscard]] za::String seenPath()
{
    return za::String{com_gamedir} + "/tips_seen.txt";
}

[[nodiscard]] bool keyLess(const za::String& a, const za::StringView& b)
{
    return za::StringView{a.cStr()} < b;
}

[[nodiscard]] bool hasKey(za::StringView key)
{
    const za::String* it = za::lowerBound(seenList.keys.begin(), seenList.keys.end(), key, keyLess);
    return it != seenList.keys.end() && za::StringView{it->cStr()} == key;
}

// False: it was there already.
bool addKey(za::StringView key)
{
    if(key.empty())
    {
        return false;
    }
    const za::String* it = za::lowerBound(seenList.keys.begin(), seenList.keys.end(), key, keyLess);
    if(it != seenList.keys.end() && za::StringView{it->cStr()} == key)
    {
        return false;
    }
    seenList.keys.insert(it, za::String{key});
    return true;
}

// Each key of a list split at spaces, tabs and line ends; the number new.
[[nodiscard]] int addKeys(za::StringView list)
{
    int added = 0;
    for(size_t i = 0; i < list.size();)
    {
        while(i < list.size() && static_cast<unsigned char>(list[i]) <= ' ')
        {
            i++;
        }
        size_t j = i;
        while(j < list.size() && static_cast<unsigned char>(list[j]) > ' ')
        {
            j++;
        }
        added += addKey(list.substrByPosLen(i, j - i)) ? 1 : 0;
        i = j;
    }
    return added;
}

void saveSeen()
{
    za::String text;
    for(const za::String& key : seenList.keys)
    {
        text += key;
        text += '\n';
    }
    const za::String path = seenPath();
    const za::String tmp = path + ".tmp";
    if(!files::writeBytes(tmp.cStr(), text.data(), text.size()) || !files::rename(tmp.cStr(), path.cStr()))
    {
        files::remove(tmp.cStr());
        Con_Printf("VR: could not write %s\n", path.cStr());
    }
}

// vr_tips_seen's value as the config's line has it, whole: a config value longer than 1023 characters is read back
// cut (com_token), so a long list's last keys are only there. Empty: no such line.
[[nodiscard]] za::String configSeenLine()
{
    za::String cfg;
    if(!files::readText(va("%s/%s", com_gamedir, CONFIG_NAME), cfg))
    {
        return {};
    }
    const za::StringView text{cfg.cStr()};
    const za::StringView prefix{"vr_tips_seen \""};
    for(size_t start = 0; start < text.size();)
    {
        size_t end = text.find('\n', start);
        end = end == za::StringView::nPos ? text.size() : end;
        const za::StringView line = text.substrByPosLen(start, end - start);
        if(line.size() > prefix.size() && line.substrByPosLen(0, prefix.size()) == prefix)
        {
            const za::StringView rest = line.substrByPosLen(prefix.size(), line.size() - prefix.size());
            const size_t quote = rest.find('"');
            return za::String{rest.substrByPosLen(0, quote == za::StringView::nPos ? rest.size() : quote)};
        }
        start = end + 1;
    }
    return {};
}

// The list as it is in this game folder: read from its file, with the old vr_tips_seen cvar's (set by an older
// config) moved into it once.
void loadSeen()
{
    if(!seenList.loaded || seenList.dir != za::StringView{com_gamedir})
    {
        seenList.keys.clear();
        seenList.dir = za::String{com_gamedir};
        seenList.loaded = true;
        za::String text;
        if(files::readText(seenPath().cStr(), text))
        {
            (void)addKeys(za::StringView{text.cStr()});
        }
    }
    if(vr_tips_seen.string[0])
    {
        int added = addKeys(za::StringView{vr_tips_seen.string});
        added += addKeys(za::StringView{configSeenLine().cStr()});
        Cvar_Set(vr_tips_seen.name, "");
        saveSeen();
        Con_DPrintf("VR: vr_tips_seen moved to tips_seen.txt (%d keys new, %d in all)\n", added,
            static_cast<int>(seenList.keys.size()));
    }
}

[[nodiscard]] bool seen(const char* key)
{
    loadSeen();
    return hasKey(za::StringView{key});
}

void markSeen(const char* key)
{
    loadSeen();
    if(addKey(za::StringView{key}))
    {
        saveSeen();
    }
}

// Where the line from the panel points: the middle of what the tip is about (its model's box, turned with it: a wall
// torch's origin is in its wall).
[[nodiscard]] glm::vec3 targetPoint(const entity_t& e)
{
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    if(!e.model)
    {
        return origin;
    }
    const glm::vec3 c = (glm::vec3{e.model->mins[0], e.model->mins[1], e.model->mins[2]} +
                            glm::vec3{e.model->maxs[0], e.model->maxs[1], e.model->maxs[2]}) *
                        0.5f;
    glm::vec3 f, r, u;
    hands::angleVectors(glm::vec3{e.angles[0], e.angles[1], e.angles[2]}, f, r, u);
    return origin + f * c.x - r * c.y + u * c.z; // (a model's y is its left)
}

[[nodiscard]] bool connected()
{
    return cls.state == ca_connected && cls.signon == SIGNONS && cl.worldmodel;
}

// Playing: no menu or console open, no intermission (a tip showing waits meanwhile).
[[nodiscard]] bool inGame()
{
    return connected() && !cl.intermission && key_dest == key_game && hands::current().valid;
}

// Whether entity `i` is there this frame (in the last message).
[[nodiscard]] const entity_t* liveEntity(int i)
{
    if(i <= 0 || i >= cl.num_entities || i == cl.viewentity)
    {
        return nullptr;
    }
    const entity_t& e = cl_entities[i];
    return e.model && e.msgtime == cl.mtime[0] ? &e : nullptr;
}

// Whether entity `i` is there this frame (in the last message) and what `tip` is about.
[[nodiscard]] const entity_t* entityFor(const Tip& tip, int i)
{
    const entity_t* e = liveEntity(i);
    return e && tip.about(*e) ? e : nullptr;
}

// Nothing of the world between the eyes and `at`, pulled a few units towards them (what is on a wall is partly in it).
[[nodiscard]] bool clearTo(const glm::vec3& eye, const glm::vec3& at)
{
    const glm::vec3 toEye = eye - at;
    const float d = glm::length(toEye);
    if(d < 1.f)
    {
        return true;
    }
    const glm::vec3 endpoint = at + toEye / d * za::min(8.f, d);
    vec3_t start{eye.x, eye.y, eye.z}, end{endpoint.x, endpoint.y, endpoint.z};
    trace_t trace;
    memset(&trace, 0, sizeof trace);
    trace.fraction = 1.f;
    SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0.f, 1.f, start, end, &trace);
    return trace.fraction >= 1.f && !trace.allsolid;
}

// Whether the eyes see `at`: its middle, or the top of what it is on (a torch's flame over a ledge).
[[nodiscard]] bool inSight(const glm::vec3& eye, const glm::vec3& at, const entity_t* e)
{
    if(clearTo(eye, at))
    {
        return true;
    }
    return e && e->model && clearTo(eye, glm::vec3{e->origin[0], e->origin[1], e->origin[2] + e->model->maxs[2] + 8.f});
}

// How well `at` is placed for a tip now: near enough, in view and in sight (else -1); the nearer the better. `e` (may
// be null: a point in the map) is what the tip is about, for the test to its top. `distance` 0: vr_tips_distance;
// AnyAngle: the view angle and the line of sight are not tested; `anyDistance`: neither is nearness (vr_tips_test).
[[nodiscard]] float rangeOf(float distance)
{
    return distance > 0.f ? distance : za::max(vr_tips_distance.value, 1.f);
}

[[nodiscard]] float placing(const glm::vec3& at, const entity_t* e, float distance, int flags, bool anyDistance)
{
    const hands::State& s = hands::current();
    const glm::vec3 to = at - s.head;
    const float d = glm::length(to);
    if(d < 1e-3f || (!anyDistance && d > rangeOf(distance)))
    {
        return -1.f;
    }
    const bool anyAngle = (flags & AnyAngle) != 0;
    const float maxAngle = vr_tips_view_angle.value;
    if(!anyAngle && maxAngle > 0.f && maxAngle < 180.f)
    {
        const float cosine = glm::dot(to / d, hands::forward(s.headAngles));
        if(cosine < glm::cos(glm::radians(maxAngle)))
        {
            return -1.f;
        }
    }
    if(!anyAngle && vr_tips_line_of_sight.value != 0.f && !inSight(s.head, at, e))
    {
        return -1.f;
    }
    return d;
}

// What a tip is about, found now: the entity it follows, or the point it is at.
struct Subject
{
    const entity_t* e{nullptr}; // what it is on (null: a point in the map, or its entity is gone)
    int ent{0};                 // its index (0: none)
    glm::vec3 point{0.f};       // where the panel points
};

// The subject `tip` is best shown on now (false: none). `anyDistance`: nearness is not required (vr_tips_test).
[[nodiscard]] bool bestFor(int tip, bool anyDistance, Subject& out)
{
    if(MapTip* mt = mapTipOf(tip))
    {
        if(mt->ent == goneEntity)
        {
            return false; // its entity is gone for good (the server said so)
        }
        if(mt->ent >= 0)
        {
            const entity_t* e = liveEntity(mt->ent);
            if(!e)
            {
                return false; // its entity is not in this frame's message
            }
            const glm::vec3 at = targetPoint(*e);
            const float d = placing(at, e, mt->distance, mt->flags, anyDistance);
            if(d < 0.f)
            {
                return false;
            }
            out = {e, mt->ent, at};
            return true;
        }
        const float d = placing(mt->pos, nullptr, mt->distance, mt->flags, anyDistance);
        if(d < 0.f)
        {
            return false;
        }
        out = {nullptr, 0, mt->pos};
        return true;
    }

    // A built-in tip: the nearest of what it is about.
    int best = 0;
    float bestDistance = 1e9f;
    glm::vec3 bestPoint{0.f};
    for(int i = 1; i < cl.num_entities; i++)
    {
        if(const entity_t* e = entityFor(tips[tip], i))
        {
            const glm::vec3 at = targetPoint(*e);
            const float d = placing(at, e, 0.f, 0, anyDistance);
            if(d >= 0.f && d < bestDistance)
            {
                best = i;
                bestDistance = d;
                bestPoint = at;
            }
        }
    }
    if(best == 0)
    {
        return false;
    }
    out = {liveEntity(best), best, bestPoint};
    return true;
}

// A Repeat tip shows again once the player has gone out of its range, by this much more than the range (so standing at
// its edge does not show it again and again).
constexpr float repeatRearm = 1.25f;

// Whether the player has gone out of a Repeat tip's range since it showed (its entity out of the message counts).
[[nodiscard]] bool outOfRange(const MapTip& mt)
{
    const entity_t* e = mt.ent >= 0 ? liveEntity(mt.ent) : nullptr;
    if(mt.ent >= 0 && !e)
    {
        return true;
    }
    const glm::vec3 at = e ? targetPoint(*e) : mt.pos;
    return glm::distance(at, hands::current().head) > rangeOf(mt.distance) * repeatRearm;
}

// How long a tip must have been near and seen before it shows, and how big its screen's text is.
[[nodiscard]] float tipDelay(const MapTip* mt)
{
    return mt && mt->delay >= 0.f ? mt->delay : za::max(vr_tips_delay.value, 0.f);
}

[[nodiscard]] float tipSize(const MapTip* mt)
{
    return mt && mt->size > 0.f ? mt->size : vr_tips_size.value;
}

// Shows tip `t` on `s`: in the gadget's hologram (a map tip's Hologram flag; a built-in tip's vr_tips 2, if there is
// one), else as the floating panel.
void show(int t, const Subject& s, bool count)
{
    MapTip* mt = mapTipOf(t);
    const char* text = tipText(t);
    const float time = za::max(vr_tips_time.value, 1.f);
    if(count && mt && (mt->flags & Repeat) != 0)
    {
        mt->shownNear = true; // not remembered: it shows again once he has gone away and come back
    }
    else if(count)
    {
        markSeen(seenKeyOf(t));
    }
    const bool hologram = mt ? (mt->flags & Hologram) != 0 : vr_tips.value >= 2.f;
    if(hologram && gadget::tip(text, time))
    {
        Con_DPrintf("tips: \"%s\" in the gadget's hologram\n", tipName(t));
        showing.tip = -1;
        return;
    }
    Con_DPrintf("tips: \"%s\" %s\n", tipName(t),
        s.ent ? va("by entity %d", s.ent) : va("at %.0f %.0f %.0f", s.point.x, s.point.y, s.point.z));
    showing.tip = t;
    showing.ent = s.ent;
    showing.start = realtime;
    showing.until = realtime + time;
    showing.placed = false;
    showing.lastTarget = s.point;
    showing.lastFrame = realtime;
}

// The floating panel this frame: by what it is about, a little below and to the right of it as seen, facing the eyes,
// with a line to it; fading in and out.
void drawShowing()
{
    const MapTip* mt = mapTipOf(showing.tip);
    const entity_t* e = showing.ent ? liveEntity(showing.ent) : nullptr;
    if(!e && showing.ent && showing.until > realtime + 0.4)
    {
        showing.until = realtime + 0.4; // taken (or gone): it fades now
    }
    if(realtime >= showing.until)
    {
        showing.tip = -1;
        return;
    }
    const hands::State& s = hands::current();
    const glm::vec3 target = e ? targetPoint(*e)
                               : (mt && !showing.ent ? mt->pos : showing.lastTarget); // (a point in the map stays put)
    showing.lastTarget = target;

    glm::vec3 fwd, right, up;
    hands::angleVectors(s.headAngles, fwd, right, up);
    const float m2u = units::metresToUnits();
    const glm::vec3 to = target - s.head;
    const float d = za::max(glm::length(to), 1.f);
    const float panelDistance = CLAMP(0.5f * m2u, d * 0.75f, 1.2f * m2u);
    const glm::vec3 side = glm::normalize(glm::cross(to / d, glm::vec3{0.f, 0.f, 1.f})); // the eyes' right, level
    const glm::vec3 dir = glm::normalize(to / d + side * 0.32f - glm::vec3{0.f, 0.f, 0.2f});
    const glm::vec3 wanted = s.head + dir * panelDistance;
    const float dt = static_cast<float>(za::min(realtime - showing.lastFrame, 0.1));
    showing.lastFrame = realtime;
    showing.panel = showing.placed ? glm::mix(showing.panel, wanted, 1.f - glm::exp(-8.f * dt)) : wanted;
    showing.placed = true;

    const float fadeIn = CLAMP(0.f, static_cast<float>(realtime - showing.start) / 0.3f, 1.f);
    const float fadeOut = CLAMP(0.f, static_cast<float>(showing.until - realtime) / 0.6f, 1.f);
    const float alpha = fadeIn * fadeOut;

    const float charSize = glm::distance(s.head, showing.panel) * 0.026f * CLAMP(0.25f, tipSize(mt), 4.f);
    // Its plane: turned towards the eyes, level (vr_tips_facing 0), or square to the view, as the head is turned
    // (1: flat in front of you, however you tilt your head).
    glm::vec3 panelRight, panelUp;
    if(vr_tips_facing.value != 0.f)
    {
        glm::vec3 f;
        hands::angleVectors(s.headAngles, f, panelRight, panelUp);
    }
    else
    {
        const glm::vec3 look = glm::normalize(showing.panel - s.head);
        const glm::vec3 level = glm::cross(look, glm::vec3{0.f, 0.f, 1.f});
        panelRight = glm::length(level) > 1e-4f ? glm::normalize(level) : right;
        panelUp = glm::cross(panelRight, look);
    }
    // A CRT screen as the map boards', with a cable of the same screen to what it is about.
    text3d::queueOverlayScreen(tipText(showing.tip), showing.panel, panelRight, panelUp, charSize, alpha, s.head,
        &target, 0.0022f);
}

// ----------------------------------------------------------------------------
// The server's list, and the client's mirror (vr_worldtext.cpp's pattern)

[[nodiscard]] MapTip& serverTip(int handle)
{
    if(handle < 0 || handle >= static_cast<int>(serverTips.size()))
    {
        PR_RunError("invalid tip handle %d", handle);
    }

    return serverTips[static_cast<size_t>(handle)];
}

// Changes made while the map is loading are not broadcast: spawning clients get the full list from serverWriteAll.
[[nodiscard]] sizebuf_t* broadcast()
{
    return sv.state == ss_active ? &sv.reliable_datagram : nullptr;
}

void beginMessage(sizebuf_t* msg, int subcmd, int handle)
{
    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, subcmd);
    MSG_WriteShort(msg, handle);
}

void writeTip(sizebuf_t* msg, int handle, const MapTip& mt, unsigned int protocolflags)
{
    beginMessage(msg, QVR_SVC_TIP_NAME, handle);
    MSG_WriteString(msg, mt.name.cStr());
    beginMessage(msg, QVR_SVC_TIP_TEXT, handle);
    MSG_WriteString(msg, mt.text.cStr());
    beginMessage(msg, QVR_SVC_TIP_POS, handle);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(msg, mt.pos[i], protocolflags);
    }
    beginMessage(msg, QVR_SVC_TIP_ENT, handle);
    MSG_WriteShort(msg, mt.ent);
    beginMessage(msg, QVR_SVC_TIP_DISTANCE, handle);
    MSG_WriteFloat(msg, mt.distance);
    beginMessage(msg, QVR_SVC_TIP_SIZE, handle);
    MSG_WriteFloat(msg, mt.size);
    beginMessage(msg, QVR_SVC_TIP_DELAY, handle);
    MSG_WriteFloat(msg, mt.delay);
    beginMessage(msg, QVR_SVC_TIP_FLAGS, handle);
    MSG_WriteByte(msg, mt.flags);
}

// The whole list: for a spawning client (the server's), or a demo recorded in the middle of a map (the client's mirror).
void writeAll(sizebuf_t* msg, const za::Vector<MapTip>& list, unsigned int protocolflags)
{
    for(int handle = 0; handle < static_cast<int>(list.size()); handle++)
    {
        beginMessage(msg, QVR_SVC_TIP_MAKE, handle);
        writeTip(msg, handle, list[static_cast<size_t>(handle)], protocolflags);
    }
}

[[nodiscard]] MapTip& clientTip(int handle)
{
    if(handle < 0 || handle >= maxMapTips)
    {
        Host_Error("svc_quakevr: bad tip handle %d", handle);
    }

    if(handle >= static_cast<int>(mapTips.size()))
    {
        mapTips.resize(static_cast<size_t>(handle) + 1);
    }

    return mapTips[static_cast<size_t>(handle)];
}

// The map's worldspawn "_vr_tips_repeat": every tip in it repeats (a tutorial map). Read here, as QC never sees a key
// that starts with '_'.
[[nodiscard]] int mapFlags()
{
    return debris::worldspawnValue("_vr_tips_repeat", 0.f) > 0.f ? Repeat : 0;
}

} // namespace

void serverReset()
{
    serverTips.clear();
}

int serverMake()
{
    if(serverTips.size() >= static_cast<za::SizeT>(maxMapTips))
    {
        PR_RunError("too many map tips (max %d)", maxMapTips);
    }

    serverTips.emplaceBack();
    const int handle = static_cast<int>(serverTips.size() - 1);
    serverTips.back().flags = mapFlags();
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_MAKE, handle);
        beginMessage(msg, QVR_SVC_TIP_FLAGS, handle);
        MSG_WriteByte(msg, serverTips.back().flags);
    }
    return handle;
}

void serverSetName(int handle, const char* name)
{
    MapTip& mt = serverTip(handle);
    mt.name = name;
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_NAME, handle);
        MSG_WriteString(msg, mt.name.cStr());
    }
}

void serverSetText(int handle, const char* text)
{
    MapTip& mt = serverTip(handle);
    mt.text = text;
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_TEXT, handle);
        MSG_WriteString(msg, mt.text.cStr());
    }
}

void serverSetPos(int handle, const glm::vec3& pos)
{
    MapTip& mt = serverTip(handle);
    mt.pos = pos;
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_POS, handle);
        for(int i = 0; i < 3; i++)
        {
            MSG_WriteCoord(msg, mt.pos[i], sv.protocolflags);
        }
    }
}

void serverSetEntity(int handle, int ent, const char* classname)
{
    MapTip& mt = serverTip(handle);
    mt.ent = ent;
    mt.followClass = ent >= 0 && classname ? classname : "";
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_ENT, handle);
        MSG_WriteShort(msg, mt.ent);
    }
}

void serverSetDistance(int handle, float distance)
{
    MapTip& mt = serverTip(handle);
    mt.distance = distance;
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_DISTANCE, handle);
        MSG_WriteFloat(msg, mt.distance);
    }
}

void serverSetSize(int handle, float size)
{
    MapTip& mt = serverTip(handle);
    mt.size = size;
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_SIZE, handle);
        MSG_WriteFloat(msg, mt.size);
    }
}

void serverSetDelay(int handle, float delay)
{
    MapTip& mt = serverTip(handle);
    mt.delay = delay;
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_DELAY, handle);
        MSG_WriteFloat(msg, mt.delay);
    }
}

void serverSetFlags(int handle, int flags)
{
    MapTip& mt = serverTip(handle);
    mt.flags = flags | mapFlags();
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_TIP_FLAGS, handle);
        MSG_WriteByte(msg, mt.flags);
    }
}

void serverFrame()
{
    for(int handle = 0; handle < static_cast<int>(serverTips.size()); handle++)
    {
        const MapTip& mt = serverTips[static_cast<size_t>(handle)];
        if(mt.ent < 0)
        {
            continue;
        }
        // Freed, or another entity in its slot (a loaded game; a slot freed in the map's first seconds is taken at
        // once): the tip would follow whatever takes it next.
        const edict_t* ed = mt.ent < qcvm->num_edicts ? EDICT_NUM(mt.ent) : nullptr;
        if(ed && !ed->free && mt.followClass == PR_GetString(ed->v.classname))
        {
            continue;
        }
        serverSetEntity(handle, goneEntity, nullptr);
    }
}

void serverWriteAll(sizebuf_t* msg)
{
    writeAll(msg, serverTips, sv.protocolflags);
}

void clientWriteAll(sizebuf_t* msg)
{
    writeAll(msg, mapTips, cl.protocolflags);
}

void clientReset()
{
    mapTips.clear();
    showing.tip = -1;
    candidate.tip = -1;
    pendingTest = -1;
}

void clientParse(int subcmd)
{
    MapTip& mt = clientTip(MSG_ReadShort());

    switch(subcmd)
    {
        case QVR_SVC_TIP_MAKE: mt = MapTip{}; break;
        case QVR_SVC_TIP_NAME: mt.name = MSG_ReadString(); break;
        case QVR_SVC_TIP_TEXT: mt.text = MSG_ReadString(); break;
        case QVR_SVC_TIP_POS:
            for(int i = 0; i < 3; i++)
            {
                mt.pos[i] = MSG_ReadCoord(cl.protocolflags);
            }
            break;
        case QVR_SVC_TIP_ENT: mt.ent = MSG_ReadShort(); break;
        case QVR_SVC_TIP_DISTANCE: mt.distance = MSG_ReadFloat(); break;
        case QVR_SVC_TIP_SIZE: mt.size = MSG_ReadFloat(); break;
        case QVR_SVC_TIP_DELAY: mt.delay = MSG_ReadFloat(); break;
        case QVR_SVC_TIP_FLAGS: mt.flags = MSG_ReadByte(); break;
        default: Host_Error("svc_quakevr: bad tip command %d", subcmd);
    }
}

void frame()
{
    if(vr_tips.value == 0.f || !connected())
    {
        showing.tip = -1;
        candidate.tip = -1;
        return;
    }
    if(!inGame())
    {
        candidate.tip = -1;
        showing.lastFrame = realtime;
        return;
    }
    if(pendingTest >= 0)
    {
        const int t = pendingTest;
        pendingTest = -1;
        Subject subject;
        if(t >= tipTotal())
        {
            return; // its map changed
        }
        if(bestFor(t, true, subject))
        {
            show(t, subject, false);
        }
        else
        {
            Con_Printf("vr_tips_test: nothing for \"%s\" in view\n", tipName(t));
        }
    }
    if(showing.tip >= 0)
    {
        drawShowing();
        return;
    }
    // The first tip not shown yet whose subject is near and seen; it shows once it has been so for its delay.
    for(int t = 0; t < tipTotal(); t++)
    {
        MapTip* mt = mapTipOf(t);
        if(mt && (mt->flags & Repeat) != 0)
        {
            if(mt->shownNear)
            {
                mt->shownNear = !outOfRange(*mt);
                continue;
            }
        }
        else if(seen(seenKeyOf(t)))
        {
            continue;
        }
        Subject subject;
        if(!bestFor(t, false, subject))
        {
            continue;
        }
        if(candidate.tip != t || candidate.ent != subject.ent)
        {
            candidate = {t, subject.ent, realtime};
        }
        if(realtime - candidate.since >= static_cast<double>(tipDelay(mt)))
        {
            candidate.tip = -1;
            show(t, subject, true);
        }
        return;
    }
    candidate.tip = -1;
}

void reset_f()
{
    loadSeen(); // (an older config's vr_tips_seen moved first, to be cleared with the rest)
    seenList.keys.clear();
    saveSeen();
    showing.tip = -1;
    candidate.tip = -1;
    Con_Printf("VR: every tip will show again\n");
}

// The tips named on the command line: a built-in one, or a map tip's tipname.
[[nodiscard]] int findTip(const char* name)
{
    for(int i = 0; i < tipCount; i++)
    {
        if(!q_strcasecmp(tips[i].name, name))
        {
            return i;
        }
    }
    for(int i = 0; i < tipTotal(); i++)
    {
        const MapTip* mt = mapTipOf(i);
        if(mt && !mt->name.empty() && !q_strcasecmp(mt->name.cStr(), name))
        {
            return i;
        }
    }
    if(name[0] == '#' && name[1] >= '0' && name[1] <= '9') // an unnamed map tip, by its place in the map's list
    {
        const int i = tipCount + Q_atoi(name + 1);
        return i < tipTotal() ? i : -1;
    }
    return -1;
}

// vr_tips_test list (VR Settings > Tips > List This Map's Tips): every tip here, what it is about, and how it shows.
void listTips()
{
    Con_Printf("vr_tips_test: the tips here (vr_tips_test <name> shows one now):\n");
    for(int i = 0; i < tipCount; i++)
    {
        Con_Printf("  %s: built-in%s\n", tips[i].name, seen(seenKeyOf(i)) ? ", seen" : "");
    }
    for(int i = tipCount; i < tipTotal(); i++)
    {
        const MapTip& mt = *mapTipOf(i);
        const char* about = mt.ent == goneEntity ? "its entity is gone"
                            : mt.ent >= 0        ? va("follows entity %d", mt.ent)
                                                 : va("at %.0f %.0f %.0f", mt.pos.x, mt.pos.y, mt.pos.z);
        const char* shown = (mt.flags & Repeat) != 0 ? ", repeats" : seen(seenKeyOf(i)) ? ", seen" : "";
        const char* delay = mt.delay >= 0.f ? va(", delay %g s", static_cast<double>(mt.delay)) : ""; // (its tip_delay)
        Con_Printf("  %s: %s, range %.0f%s%s%s%s\n", mt.name.empty() ? va("#%d", i - tipCount) : mt.name.cStr(), about,
            rangeOf(mt.distance), delay, (mt.flags & Hologram) != 0 ? ", hologram" : "",
            (mt.flags & AnyAngle) != 0 ? ", any angle" : "", shown);
    }
}

void test_f()
{
    int t = 0;
    if(Cmd_Argc() > 1)
    {
        t = findTip(Cmd_Argv(1));
        if(t < 0)
        {
            if(q_strcasecmp(Cmd_Argv(1), "list") != 0)
            {
                Con_Printf("vr_tips_test: no tip \"%s\"\n", Cmd_Argv(1));
            }
            listTips();
            return;
        }
    }
    if(!connected())
    {
        Con_Printf("vr_tips_test: in a game\n");
        return;
    }
    // What it is about, and how it stands against the settings now.
    const hands::State& head = hands::current();
    struct Near
    {
        int ent;
        float d;
        glm::vec3 point;
    };
    za::Vector<Near> nearby;
    if(MapTip* mt = mapTipOf(t))
    {
        if(mt->ent == goneEntity)
        {
            Con_Printf("vr_tips_test: %s followed an entity that is gone\n", tipName(t));
            return;
        }
        if(mt->ent >= 0)
        {
            const entity_t* e = liveEntity(mt->ent);
            if(!e)
            {
                Con_Printf("vr_tips_test: %s follows entity %d, which is not there\n", tipName(t), mt->ent);
                return;
            }
            nearby.pushBack({mt->ent, glm::distance(targetPoint(*e), head.head), targetPoint(*e)});
        }
        else
        {
            nearby.pushBack({0, glm::distance(mt->pos, head.head), mt->pos});
        }
    }
    else
    {
        for(int i = 1; i < cl.num_entities; i++)
        {
            if(const entity_t* e = entityFor(tips[t], i))
            {
                nearby.pushBack({i, glm::distance(targetPoint(*e), head.head), targetPoint(*e)});
            }
        }
    }
    za::stableSort(nearby.begin(), nearby.end(), [](const Near& a, const Near& b) { return a.d < b.d; });
    const float limit = rangeOf(mapTipOf(t) ? mapTipOf(t)->distance : 0.f);
    for(size_t i = 0; i < nearby.size() && i < 3; i++)
    {
        const entity_t* e = nearby[i].ent ? liveEntity(nearby[i].ent) : nullptr;
        const glm::vec3 to = glm::normalize(nearby[i].point - head.head);
        const float angle = glm::degrees(glm::acos(CLAMP(-1.f, glm::dot(to, hands::forward(head.headAngles)), 1.f)));
        Con_Printf("vr_tips_test: %s %s: %.0f units (%s), %.0f degrees from the view (%s), %s\n", tipName(t),
            nearby[i].ent ? va("entity %d", nearby[i].ent) : "point", nearby[i].d,
            nearby[i].d <= limit ? "near enough" : "too far", angle,
            vr_tips_view_angle.value <= 0.f || angle <= vr_tips_view_angle.value ? "in view" : "out of view",
            inSight(head.head, nearby[i].point, e) ? "in sight" : "hidden");
    }
    pendingTest = t; // (as the menu or the console closes)
    showing.tip = -1;
}

} // namespace qvr::tips
