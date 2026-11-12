// vr_takes.cpp -- takes and clips for trailers (vr_takes.hpp; docs/vr-port/TRAILER.md, "Takes and clips").
//
// OBS's recording state comes as events (RecordStateChanged, ReplayBufferSaved: vr_obs.cpp passes them on), so a take
// is whatever OBS records while vr_takes is on, however it was started. A take's times are the game's realtime (the
// wall clock's in play; a fixed 1/72 s a frame in the kit's test runs: the same numbers every run), from OBS's
// "started" event. Files are moved only on the main thread, by rename (never copied, never deleted); a move that fails
// (OBS still closing the file) is tried again every half second for vr_takes_move_wait seconds.

#include "vr_takes.hpp"
#include "vr_obs.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_gadget.hpp"
#include "vr_gfx.hpp"
#include "vr_hands.hpp"
#include "vr_testkit.hpp"
#include "vr_touch.hpp"
#include "vr_units.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"

extern "C"
{
#include "json.h"
int Key_StringToKeynum(const char* str); // keys.c
}

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

namespace qvr::takes
{
namespace
{

constexpr double moveRetry = 0.5;  // seconds between tries of a move that failed
constexpr int moveTries = 40;      // tries (20 s) before it is given up (the file stays where it is, its take says so)
constexpr double ratedShown = 2.0; // seconds the prompt shows a rating just given

enum class Rating : int
{
    Unrated,
    Keep,
    Maybe,
    Reject,
    Clip,
};
constexpr const char* ratingNames[] = {"unrated", "keep", "maybe", "reject", "clip"};

enum class MarkKind : int
{
    In,
    Out,
    Note,
};

struct Mark
{
    MarkKind kind{MarkKind::In};
    double t{0.0};   // seconds from the take's start
    za::String text; // a voice note's files (their path without the extension)
};

struct Take
{
    int number{0}; // per scene and date (0: OBS has not named its file yet)
    bool clip{false};
    bool running{false};
    Rating rating{Rating::Unrated};
    za::String scene, date, map;
    za::String dir;  // OBS's folder (where its file was made)
    za::String path; // where its file is now ("" until OBS says)
    za::String obsPath; // where OBS made it (its answer and its event both say: taken once)
    za::String ext;  // ".mkv"
    double started{0.0}; // realtime
    double length{0.0};  // seconds (a clip's: OBS's replay buffer's)
    za::Vector<Mark> marks;
    bool movePending{false}; // a move to its rating's name failed: tried again
    double moveAt{0.0};
    int moveTriesLeft{0};
};

struct Session
{
    bool open{false};
    za::String stamp;     // when it opened (2026-11-10_21-04-05)
    za::String recordDir; // OBS's (GetRecordDirectory)
    int fpsNum{0}, fpsDen{1};
    int width{0}, height{0};
    double clipSeconds{0.0};   // OBS's replay buffer's length (0: not known)
    bool replayActive{false};
    bool replayOurs{false};    // started by the session (stopped at its end)
    double replaySince{-1.0};  // realtime it started (a clip holds no more than it has run)
    za::Vector<Take> takes;
    int current{-1}; // the take or clip a rating goes to
    int running{-1};
    double promptUntil{-1.0};
    double ratedAt{-10.0};
    MarkKind nextChord{MarkKind::In};
};
Session session;

enum class Pending
{
    None,
    Take,
    Clip,
};
Pending pending = Pending::None;
double pendingUntil = 0.0;
bool wasReady = false;
bool replayHintShown = false;
bool pageHidden = false; // the gadget's page hidden (its HIDE button) until the next take ends
touch::Ui ui;
glm::vec2 pagePixels{240.f, 150.f};
bool chordHeld[HAND_COUNT][5]{};
bool chordSwallow[HAND_COUNT][5]{};
char statusText[192];
char flatText[128];
int chaptersAsked = 0; // OBS chapters asked for (vr_takes_test)
za::Vector<gfx::Vertex> textVerts;  // the headset's light and prompt, built each eye (reused): the characters,
za::Vector<gfx::Vertex> shapeVerts; // their dark backings,
za::Vector<gfx::Vertex> discVerts;  // the light

// ---------------------------------------------------------------- names

// The scene's name for the files: vr_take_scene, else the map's; letters, digits and '-' only ('_' separates the
// name's parts).
[[nodiscard]] za::String sceneName()
{
    const char* s = vr_take_scene.string && vr_take_scene.string[0] ? vr_take_scene.string : cl.mapname[0] ? cl.mapname : "menu";
    za::String out;
    for(const char* p = s; *p && out.size() < 40; p++)
    {
        const char c = *p;
        out += (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ? c : '-';
    }
    return out.empty() ? za::String{"scene"} : out;
}

[[nodiscard]] za::String today()
{
    return files::timestamp(time(nullptr), "%Y-%m-%d");
}

[[nodiscard]] za::String fileName(const Take& t, Rating r)
{
    char name[160];
    q_snprintf(name, sizeof(name), "%s_%s_t%02d_%s%s", t.date.cStr(), t.scene.cStr(), t.number,
        ratingNames[static_cast<int>(r)], t.ext.cStr());
    return za::String{name};
}

// A take number in a file's name: <prefix><NN>_..., else 0.
[[nodiscard]] int numberIn(const char* name, const za::String& prefix)
{
    if(strncmp(name, prefix.cStr(), prefix.size()))
    {
        return 0;
    }
    const char* p = name + prefix.size();
    char* end = nullptr;
    const long n = strtol(p, &end, 10);
    return end != p && *end == '_' && n > 0 && n < 100000 ? static_cast<int>(n) : 0;
}

// The next number for a scene on a date: past this session's and the files already in OBS's folder, its rejected/ and
// its session_*/ folders (an earlier session's of the day).
[[nodiscard]] int nextNumber(const za::String& date, const za::String& scene, const za::String& dir)
{
    const za::String prefix = date + "_" + scene + "_t";
    int most = 0;
    for(const Take& t : session.takes)
    {
        if(t.number > 0 && t.date.toStringView() == date.toStringView() && t.scene.toStringView() == scene.toStringView())
        {
            most = za::max(most, t.number);
        }
    }
    const auto scan = [&](const za::String& folder) {
        files::forEachEntry(folder.cStr(), [&](const char* name, bool isDirectory) {
            if(!isDirectory)
            {
                most = za::max(most, numberIn(name, prefix));
            }
        });
    };
    if(!dir.empty())
    {
        scan(dir);
        scan(files::join(dir.toStringView(), "rejected"));
        files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
            if(isDirectory && !strncmp(name, "session_", 8))
            {
                scan(files::join(dir.toStringView(), name));
            }
        });
    }
    return most + 1;
}

// The take's file moved to its rating's name (reject: into rejected/). False: not now (tried again).
bool moveFile(Take& t, Rating r)
{
    t.rating = r;
    if(t.path.empty() || t.number <= 0)
    {
        return true; // (named when OBS says where the file is)
    }
    const za::String folder = r == Rating::Reject ? files::join(t.dir.toStringView(), "rejected") : t.dir;
    files::createDirectories(folder.cStr());
    za::String target = files::join(folder.toStringView(), fileName(t, r).toStringView());
    if(target.toStringView() == t.path.toStringView())
    {
        t.movePending = false;
        return true;
    }
    for(int k = 2; files::exists(target.cStr()) && k < 100; k++) // (never over another file)
    {
        char suffix[16];
        q_snprintf(suffix, sizeof(suffix), "-%d", k);
        za::String name = fileName(t, r);
        name.insert(name.size() - t.ext.size(), suffix);
        target = files::join(folder.toStringView(), name.toStringView());
    }
    if(!files::rename(t.path.cStr(), target.cStr()))
    {
        if(!t.movePending)
        {
            t.movePending = true;
            t.moveTriesLeft = moveTries;
            Con_DPrintf("takes: %s not moved yet (OBS may still hold it): trying again\n", t.path.cStr());
        }
        t.moveAt = realtime + moveRetry;
        return false;
    }
    t.movePending = false;
    t.path = target;
    Con_Printf("takes: %s\n", files::fileName(t.path.toStringView()).data());
    return true;
}

// OBS said where a take's (or a clip's) file is: numbered and named.
void gotFile(Take& t, const char* path)
{
    if(!path || !path[0] || !t.path.empty())
    {
        return;
    }
    t.path = files::generic(path);
    t.obsPath = t.path;
    t.dir =za::String{files::parentPath(t.path.toStringView())};
    const char* dot = strrchr(path, '.');
    const char* slash = za::max(strrchr(path, '/'), strrchr(path, '\\'));
    t.ext = dot && dot > slash ? za::String{dot} : za::String{};
    t.number = nextNumber(t.date, t.scene, t.dir);
    (void)moveFile(t, t.rating);
}

[[nodiscard]] Take* runningTake()
{
    return session.running >= 0 ? &session.takes[static_cast<za::SizeT>(session.running)] : nullptr;
}

[[nodiscard]] Take* currentTake()
{
    return session.current >= 0 && session.current < static_cast<int>(session.takes.size())
               ? &session.takes[static_cast<za::SizeT>(session.current)]
               : nullptr;
}

[[nodiscard]] const char* label(const Take& t)
{
    char* out = va("%s%02d", t.clip ? "c" : "t", t.number);
    if(t.number <= 0)
    {
        q_strlcpy(out, t.clip ? "clip" : "take", 16);
    }
    return out;
}

// ---------------------------------------------------------------- the session

void openSession()
{
    if(session.open)
    {
        return;
    }
    session.open = true;
    session.stamp = files::timestamp(time(nullptr));
    pageHidden = false;
    Con_Printf("takes: session open (scene %s)\n", sceneName().cStr());
    if(obs::ready() && vr_takes_replay.value)
    {
        (void)obs::request("StartReplayBuffer");
        session.replayOurs = true;
    }
}

void closeSession()
{
    if(!session.open)
    {
        return;
    }
    int counts[5]{};
    for(const Take& t : session.takes)
    {
        counts[static_cast<int>(t.rating)]++;
    }
    Con_Printf("takes: session closed: %d takes and clips (%d keep, %d maybe, %d unrated, %d clips, %d rejected)\n",
        static_cast<int>(session.takes.size()), counts[1], counts[2], counts[0], counts[4], counts[3]);
    if(session.replayOurs && session.replayActive && obs::ready())
    {
        (void)obs::request("StopReplayBuffer");
    }
    session = Session{};
}

void prompt(int index)
{
    session.current = index;
    session.promptUntil = realtime + vr_takes_prompt_time.value;
    session.ratedAt = -10.0;
    pageHidden = false;
}

// ---------------------------------------------------------------- OBS's messages

void recordStarted()
{
    if(runningTake())
    {
        return;
    }
    openSession();
    Take t;
    t.running = true;
    t.scene = sceneName();
    t.date = today();
    t.map = za::String{cl.mapname[0] ? cl.mapname : "menu"};
    t.started = realtime;
    session.takes.emplaceBack(static_cast<Take&&>(t));
    session.running = static_cast<int>(session.takes.size()) - 1;
    session.nextChord = MarkKind::In;
    Con_Printf("takes: take started (scene %s)\n", sceneName().cStr());
}

void recordStopped(const char* path)
{
    Take* t = runningTake();
    int index = session.running;
    if(!t)
    {
        // Not seen starting (vr_takes turned on meanwhile, or the game started after OBS): a take of unknown length,
        // unless it is the last one's file again (StopRecord's answer after the event).
        if(!path || !path[0])
        {
            return;
        }
        const za::String made = files::generic(path);
        for(const Take& o : session.takes)
        {
            if(o.obsPath.toStringView() == made.toStringView())
            {
                return;
            }
        }
        if(!session.takes.empty() && session.takes.back().path.empty() && !session.takes.back().clip)
        {
            index = static_cast<int>(session.takes.size()) - 1;
            t = &session.takes.back();
        }
        else
        {
            openSession();
            Take n;
            n.scene = sceneName();
            n.date = today();
            n.map = za::String{cl.mapname[0] ? cl.mapname : "menu"};
            n.started = realtime;
            session.takes.emplaceBack(static_cast<Take&&>(n));
            index = static_cast<int>(session.takes.size()) - 1;
            t = &session.takes.back();
        }
    }
    const bool stopped = t->running || t->number <= 0;
    if(t->running)
    {
        t->running = false;
        t->length = realtime - t->started;
        session.running = -1;
    }
    gotFile(*t, path);
    if(stopped && (t->number > 0 || !path))
    {
        prompt(index);
        Con_Printf("takes: %s stopped, %.1f s, %d marks: rate it (vr_take_keep, vr_take_maybe, vr_take_reject)\n",
            label(*t), t->length, static_cast<int>(t->marks.size()));
    }
}

void replaySaved(const char* path)
{
    if(!path || !path[0])
    {
        return;
    }
    openSession();
    Take t;
    t.clip = true;
    t.rating = Rating::Clip;
    t.scene = sceneName();
    t.date = today();
    t.map = za::String{cl.mapname[0] ? cl.mapname : "menu"};
    t.length = session.clipSeconds > 0.0 ? session.clipSeconds : static_cast<double>(vr_takes_clip_seconds.value);
    if(session.replaySince >= 0.0)
    {
        t.length = za::min(t.length, za::max(0.1, realtime - session.replaySince));
    }
    t.started = realtime - t.length;
    session.takes.emplaceBack(static_cast<Take&&>(t));
    const int index = static_cast<int>(session.takes.size()) - 1;
    gotFile(session.takes.back(), path);
    if(session.running < 0)
    {
        prompt(index); // (during a take, the take keeps the prompt: rated after it)
    }
    Con_Printf("takes: clip saved (%s, the last %.0f s)\n", label(session.takes.back()), t.length);
}

void message(const za::String& text)
{
    json_t* json = JSON_Parse(text.cStr());
    const jsonentry_t* root = json ? json->root : nullptr;
    const double* op = root ? JSON_FindNumber(root, "op") : nullptr;
    const jsonentry_t* d = root ? JSON_Find(root, "d", JSON_OBJECT) : nullptr;
    if(!op || !d)
    {
        JSON_Free(json);
        return;
    }
    if(*op == 5)
    {
        const char* type = JSON_FindString(d, "eventType");
        const jsonentry_t* data = JSON_Find(d, "eventData", JSON_OBJECT);
        const char* state = data ? JSON_FindString(data, "outputState") : nullptr;
        if(type && state && !strcmp(type, "RecordStateChanged"))
        {
            if(!strcmp(state, "OBS_WEBSOCKET_OUTPUT_STARTED"))
            {
                recordStarted();
            }
            else if(!strcmp(state, "OBS_WEBSOCKET_OUTPUT_STOPPED"))
            {
                recordStopped(JSON_FindString(data, "outputPath"));
            }
        }
        else if(type && state && !strcmp(type, "ReplayBufferStateChanged"))
        {
            if(!strcmp(state, "OBS_WEBSOCKET_OUTPUT_STARTED"))
            {
                session.replaySince = realtime;
            }
            session.replayActive = !strcmp(state, "OBS_WEBSOCKET_OUTPUT_STARTED") ||
                                   (session.replayActive && strcmp(state, "OBS_WEBSOCKET_OUTPUT_STOPPED"));
        }
        else if(type && data && !strcmp(type, "ReplayBufferSaved"))
        {
            replaySaved(JSON_FindString(data, "savedReplayPath"));
        }
    }
    else if(*op == 7)
    {
        const char* type = JSON_FindString(d, "requestType");
        const jsonentry_t* rs = JSON_Find(d, "requestStatus", JSON_OBJECT);
        const qboolean* ok = rs ? JSON_FindBoolean(rs, "result") : nullptr;
        const char* comment = rs ? JSON_FindString(rs, "comment") : nullptr;
        const jsonentry_t* data = JSON_Find(d, "responseData", JSON_OBJECT);
        const bool good = ok && *ok;
        if(!type)
        {
        }
        else if(!strcmp(type, "GetVideoSettings") && good && data)
        {
            const double* num = JSON_FindNumber(data, "fpsNumerator");
            const double* den = JSON_FindNumber(data, "fpsDenominator");
            const double* w = JSON_FindNumber(data, "outputWidth");
            const double* h = JSON_FindNumber(data, "outputHeight");
            session.fpsNum = num ? static_cast<int>(*num) : 0;
            session.fpsDen = den && *den >= 1.0 ? static_cast<int>(*den) : 1;
            session.width = w ? static_cast<int>(*w) : 0;
            session.height = h ? static_cast<int>(*h) : 0;
        }
        else if(!strcmp(type, "GetRecordDirectory") && good && data)
        {
            const char* dir = JSON_FindString(data, "recordDirectory");
            session.recordDir = files::generic(dir ? dir : "");
        }
        else if(!strcmp(type, "GetOutputSettings") && good && data)
        {
            const jsonentry_t* settings = JSON_Find(data, "outputSettings", JSON_OBJECT);
            const double* seconds = settings ? JSON_FindNumber(settings, "max_time_sec") : nullptr;
            session.clipSeconds = seconds && *seconds > 0.0 ? *seconds : 0.0;
        }
        else if(!strcmp(type, "StopRecord") && good && data)
        {
            recordStopped(JSON_FindString(data, "outputPath")); // (the file, if the event had none)
        }
        else if(!strcmp(type, "StartReplayBuffer"))
        {
            if(good || (comment && strstr(comment, "already")))
            {
                session.replayActive = true;
            }
            else if(!replayHintShown)
            {
                replayHintShown = true;
                Con_Printf("takes: OBS's replay buffer did not start (%s): OBS Settings > Output > Replay Buffer: Enable "
                           "Replay Buffer (Maximum Replay Time: a clip's length)\n",
                    comment ? comment : "?");
            }
        }
        else if(!strcmp(type, "CreateRecordChapter"))
        {
            if(!good)
            {
                Con_DPrintf("takes: OBS made no chapter (%s): the marks are kept in the take alone\n", comment ? comment : "?");
            }
        }
        else if(!good && (!strcmp(type, "StartRecord") || !strcmp(type, "StopRecord") || !strcmp(type, "SaveReplayBuffer")))
        {
            Con_Printf("takes: OBS's %s failed: %s\n", type, comment ? comment : "?");
        }
    }
    JSON_Free(json);
}

// Once a connection is there: OBS's frame rate and size (the timeline's), its folder, the replay buffer's length.
void connected()
{
    (void)obs::request("GetVideoSettings");
    (void)obs::request("GetRecordDirectory");
    (void)obs::request("GetOutputSettings", "{\"outputName\":\"Replay Buffer\"}");
    (void)obs::request("GetReplayBufferStatus");
    if(session.open && vr_takes_replay.value)
    {
        (void)obs::request("StartReplayBuffer");
        session.replayOurs = true;
    }
}

// ---------------------------------------------------------------- actions

void doTake(int want) // -1 toggle, 0 stop, 1 start
{
    const bool on = obs::recording();
    if(want == 1 && on)
    {
        Con_Printf("vr_take: a take runs already\n");
        return;
    }
    if(want == 0 && !on)
    {
        Con_Printf("vr_take: no take runs\n");
        return;
    }
    openSession();
    (void)obs::request(on ? "StopRecord" : "StartRecord");
}

void doClip()
{
    openSession();
    if(!session.replayActive)
    {
        (void)obs::request("StartReplayBuffer");
        session.replayOurs = true;
        Con_Printf("vr_clip: OBS's replay buffer was off: started now (a clip holds what comes from here)\n");
        return;
    }
    (void)obs::request("SaveReplayBuffer");
}

// An action wanting OBS: now, or once connected (within vr_takes_connect_wait seconds).
void act(Pending what)
{
    if(!vr_takes.value)
    {
        Con_Printf("takes: vr_takes is 0\n");
        return;
    }
    if(!vr_obs.value)
    {
        Con_Printf("takes: vr_obs is 0 (OBS Recording Control off)\n");
        return;
    }
    if(!obs::ready())
    {
        pending = what;
        pendingUntil = realtime + vr_takes_connect_wait.value;
        obs::connectNow();
        Con_Printf("takes: connecting to OBS first\n");
        return;
    }
    what == Pending::Take ? doTake(-1) : doClip();
}

void mark(MarkKind kind)
{
    Take* t = runningTake();
    if(!t)
    {
        Con_Printf("takes: no take runs (an in or out mark is a take's)\n");
        return;
    }
    Mark m;
    m.kind = kind;
    m.t = realtime - t->started;
    int n = 1;
    for(const Mark& o : t->marks)
    {
        n += o.kind == kind ? 1 : 0;
    }
    t->marks.emplaceBack(static_cast<Mark&&>(m));
    session.nextChord = kind == MarkKind::In ? MarkKind::Out : MarkKind::In;
    char name[32];
    q_snprintf(name, sizeof(name), "%s %d", kind == MarkKind::In ? "In" : "Out", n);
    chaptersAsked += obs::request("CreateRecordChapter", (za::String{"{\"chapterName\":"} + obs::jsonString(name) + "}").cStr()) > 0;
    Con_Printf("takes: %s at %.2f s\n", name, t->marks.back().t);
}

void rate(Rating r, int number)
{
    Take* t = currentTake();
    if(number > 0)
    {
        t = nullptr;
        for(int i = static_cast<int>(session.takes.size()) - 1; i >= 0; i--)
        {
            Take& o = session.takes[static_cast<za::SizeT>(i)];
            if(o.number == number && !o.running && o.scene.toStringView() == sceneName().toStringView())
            {
                t = &o;
                session.current = i;
                break;
            }
        }
    }
    if(!t || t->running)
    {
        Con_Printf("takes: no finished take or clip to rate%s\n", number > 0 ? " by that number" : "");
        return;
    }
    if(r == Rating::Clip && !t->clip)
    {
        r = Rating::Unrated;
    }
    (void)moveFile(*t, r);
    session.ratedAt = realtime;
    session.promptUntil = za::min(session.promptUntil, realtime + ratedShown);
    Con_Printf("takes: %s rated %s\n", label(*t), ratingNames[static_cast<int>(r)]);
}

// ---------------------------------------------------------------- commands

void endSession();
void setup_f();
void retake_f();

void take_f()
{
    if(Cmd_Argc() > 1 && obs::ready())
    {
        doTake(!strcmp(Cmd_Argv(1), "start") ? 1 : !strcmp(Cmd_Argv(1), "stop") ? 0 : -1);
        return;
    }
    act(Pending::Take);
}

void clip_f()
{
    act(Pending::Clip);
}

void keep_f()
{
    rate(Rating::Keep, Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 0);
}

void maybe_f()
{
    rate(Rating::Maybe, Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 0);
}

void reject_f()
{
    rate(Rating::Reject, Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 0);
}

void rate_f()
{
    for(int r = 0; r < 5 && Cmd_Argc() > 1; r++)
    {
        if(!q_strcasecmp(Cmd_Argv(1), ratingNames[r]))
        {
            rate(static_cast<Rating>(r), Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : 0);
            return;
        }
    }
    Con_Printf("vr_take_rate keep|maybe|reject|unrated|clip [take number]: the last take or clip, or that number's\n");
}

void in_f()
{
    mark(MarkKind::In);
}

void out_f()
{
    mark(MarkKind::Out);
}

void list_f()
{
    Con_Printf("takes: session %s, scene %s, OBS %s, %d/%d fps, replay buffer %s (%.0f s)\n",
        session.open ? session.stamp.cStr() : "closed", sceneName().cStr(), obs::ready() ? (obs::mocked() ? "mock" : "connected") : "not connected",
        session.fpsNum, session.fpsDen, session.replayActive ? "on" : "off", session.clipSeconds);
    for(const Take& t : session.takes)
    {
        Con_Printf("  %s %-7s %6.1f s %d marks%s %s\n", label(t), t.running ? "running" : ratingNames[static_cast<int>(t.rating)],
            t.running ? realtime - t.started : t.length, static_cast<int>(t.marks.size()), t.movePending ? " (move pending)" : "",
            t.path.empty() ? "" : files::fileName(t.path.toStringView()).data());
        for(const Mark& m : t.marks)
        {
            Con_Printf("      %s %.2f s %s\n", m.kind == MarkKind::In ? "in" : m.kind == MarkKind::Out ? "out" : "note", m.t,
                m.text.cStr());
        }
    }
}

void session_f()
{
    const char* a = Cmd_Argc() > 1 ? Cmd_Argv(1) : "status";
    if(!strcmp(a, "start"))
    {
        if(!obs::ready())
        {
            obs::connectNow();
        }
        openSession();
    }
    else if(!strcmp(a, "end"))
    {
        if(runningTake())
        {
            Con_Printf("vr_takes_session end: a take runs (stop it first)\n");
            return;
        }
        endSession();
    }
    else
    {
        list_f();
    }
}

void hide_f()
{
    pageHidden = !pageHidden;
}

// ---------------------------------------------------------------- drawing

void glyphQuad(za::Vector<gfx::Vertex>& out, const glm::vec3& at, const glm::vec3& right, const glm::vec3& up,
    unsigned char c, const glm::vec4& color)
{
    const glm::vec4 g = gfx::fontGlyph(c);
    const glm::vec3 p00 = at, p10 = at + right, p01 = at + up, p11 = at + right + up;
    const glm::vec2 t00{g.x, g.w}, t10{g.z, g.w}, t01{g.x, g.y}, t11{g.z, g.y};
    out.emplaceBack(gfx::Vertex{p00, t00, color});
    out.emplaceBack(gfx::Vertex{p10, t10, color});
    out.emplaceBack(gfx::Vertex{p11, t11, color});
    out.emplaceBack(gfx::Vertex{p00, t00, color});
    out.emplaceBack(gfx::Vertex{p11, t11, color});
    out.emplaceBack(gfx::Vertex{p01, t01, color});
}

void quad(za::Vector<gfx::Vertex>& out, const glm::vec3& at, const glm::vec3& right, const glm::vec3& up, const glm::vec4& color,
    bool disc)
{
    const glm::vec2 lo = disc ? glm::vec2{-1.f} : glm::vec2{0.f};
    const glm::vec3 p00 = at, p10 = at + right, p01 = at + up, p11 = at + right + up;
    out.emplaceBack(gfx::Vertex{p00, lo, color});
    out.emplaceBack(gfx::Vertex{p10, {1.f, lo.y}, color});
    out.emplaceBack(gfx::Vertex{p11, {1.f, 1.f}, color});
    out.emplaceBack(gfx::Vertex{p00, lo, color});
    out.emplaceBack(gfx::Vertex{p11, {1.f, 1.f}, color});
    out.emplaceBack(gfx::Vertex{p01, {lo.x, 1.f}, color});
}

// What the light and the prompt say now: line 1 (a take running), line 2 (the prompt). False: nothing to show.
bool indicatorLines(char (&line1)[64], char (&line2)[96], bool flat)
{
    line1[0] = line2[0] = '\0';
    if(!vr_takes.value || !vr_takes_light.value || (flat && !vr_takes_light_flat.value))
    {
        return false;
    }
    if(const Take* t = runningTake())
    {
        const int s = static_cast<int>(realtime - t->started);
        int marks = 0;
        for(const Mark& m : t->marks)
        {
            marks += m.kind != MarkKind::Note ? 1 : 0;
        }
        q_snprintf(line1, sizeof(line1), "REC %s %d:%02d%s", t->scene.cStr(), s / 60, s % 60,
            marks ? va(" [%d]", marks) : "");
    }
    if(const Take* t = currentTake(); t && realtime < session.promptUntil && !t->running)
    {
        if(session.ratedAt >= 0.0)
        {
            q_snprintf(line2, sizeof(line2), "%s %s", label(*t), ratingNames[static_cast<int>(t->rating)]);
        }
        else
        {
            q_snprintf(line2, sizeof(line2), flat ? "%s: KEEP kp1  MAYBE kp2  REJECT kp3" : "%s: KEEP / MAYBE / REJECT?",
                label(*t));
        }
    }
    return line1[0] || line2[0];
}

// ---------------------------------------------------------------- the session's end: its folder, session.json, FCPXML

// A voice note's transcript (transcribe_notes.py's <note>.transcript.txt), its lines joined; "" until there is one.
[[nodiscard]] za::String transcript(const za::String& base)
{
    za::String text;
    if(!files::readText((base + ".transcript.txt").cStr(), text))
    {
        return {};
    }
    za::String out;
    for(const char c : text.toStringView())
    {
        const bool space = c == '\n' || c == '\r' || c == '\t' || c == ' ';
        if(!(space && (out.empty() || out[out.size() - 1] == ' ')))
        {
            out += space ? ' ' : c;
        }
    }
    while(!out.empty() && out[out.size() - 1] == ' ')
    {
        out.popBack();
    }
    return out;
}

[[nodiscard]] za::String xmlText(za::StringView text)
{
    za::String out;
    for(const char c : text)
    {
        switch(c)
        {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += static_cast<unsigned char>(c) < 0x20 ? ' ' : c;
        }
    }
    return out;
}

// A file's URL (file:///C:/Videos/a%20b.mp4): '/' between the parts, all but the unreserved characters escaped.
[[nodiscard]] za::String fileUrl(const za::String& path)
{
    za::String out{"file://"};
    if(!path.empty() && path[0] != '/')
    {
        out += "/";
    }
    for(const char c : path.toStringView())
    {
        const unsigned char u = static_cast<unsigned char>(c);
        const bool plain = (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || c == '-' || c == '_' ||
                           c == '.' || c == '~' || c == '/' || c == ':';
        if(plain)
        {
            out += c;
        }
        else
        {
            char hex[4];
            q_snprintf(hex, sizeof(hex), "%%%02X", u);
            out += hex;
        }
    }
    return out;
}

// Seconds as FCPXML's rational time on the frame grid: frames * den / num seconds ("0s" for none).
struct Rate
{
    za::I64 num{60}, den{1};
    [[nodiscard]] za::I64 frames(double seconds) const { return static_cast<za::I64>(llround(seconds * static_cast<double>(num) / static_cast<double>(den))); }
    [[nodiscard]] const char* time(za::I64 frames) const
    {
        return frames == 0 ? "0s" : va("%lld/%llds", static_cast<long long>(frames * den), static_cast<long long>(num));
    }
};

// A take's in and out on its file: the first in mark and the first out mark after it (else its start and end).
void inOut(const Take& t, double& in, double& out)
{
    in = 0.0;
    out = t.length;
    for(const Mark& m : t.marks)
    {
        if(m.kind == MarkKind::In)
        {
            in = m.t;
            break;
        }
    }
    for(const Mark& m : t.marks)
    {
        if(m.kind == MarkKind::Out && m.t > in)
        {
            out = m.t;
            break;
        }
    }
}

[[nodiscard]] const char* markName(const Take& t, const Mark& m)
{
    int n = 0;
    for(const Mark& o : t.marks)
    {
        n += o.kind == m.kind ? 1 : 0;
        if(&o == &m)
        {
            break;
        }
    }
    return va("%s %d", m.kind == MarkKind::In ? "In" : m.kind == MarkKind::Out ? "Out" : "Voice note", n);
}

[[nodiscard]] za::String sessionJson(const za::String& folder, bool fpsGuessed)
{
    za::String j{"{\n  \"session\": "};
    j += obs::jsonString(session.stamp.toStringView());
    j += ",\n  \"folder\": ";
    j += obs::jsonString(folder.toStringView());
    j += va(",\n  \"fps\": [%d, %d],\n  \"fps_from_obs\": %s,\n  \"width\": %d,\n  \"height\": %d,\n  \"timeline\": \"timeline.fcpxml\",\n  \"takes\": [",
        session.fpsNum > 0 ? session.fpsNum : 60, session.fpsDen, fpsGuessed ? "false" : "true", session.width, session.height);
    for(za::SizeT i = 0; i < session.takes.size(); i++)
    {
        const Take& t = session.takes[i];
        double in, out;
        inOut(t, in, out);
        j += i ? ",\n    {" : "\n    {";
        j += va("\"label\": \"%s\", \"number\": %d, \"clip\": %s, \"rating\": \"%s\", \"scene\": ", label(t), t.number,
            t.clip ? "true" : "false", ratingNames[static_cast<int>(t.rating)]);
        j += obs::jsonString(t.scene.toStringView());
        j += ", \"map\": ";
        j += obs::jsonString(t.map.toStringView());
        j += ", \"file\": ";
        j += obs::jsonString(t.path.toStringView());
        j += va(", \"length\": %.3f, \"in\": %.3f, \"out\": %.3f, \"marks\": [", t.length, in, out);
        for(za::SizeT k = 0; k < t.marks.size(); k++)
        {
            const Mark& m = t.marks[k];
            j += va("%s{\"kind\": \"%s\", \"t\": %.3f, \"name\": \"%s\"", k ? ", " : "",
                m.kind == MarkKind::In ? "in" : m.kind == MarkKind::Out ? "out" : "note", m.t, markName(t, m));
            if(m.kind == MarkKind::Note)
            {
                const za::String said = transcript(m.text);
                j += ", \"note\": ";
                j += obs::jsonString(m.text.toStringView());
                j += ", \"text\": ";
                j += said.empty() ? za::String{"null"} : obs::jsonString(said.toStringView());
            }
            j += "}";
        }
        j += "]}";
    }
    j += "\n  ]\n}\n";
    return j;
}

// The timeline (FCPXML 1.10, as DaVinci Resolve imports: File > Import > Timeline): the keep takes in order, then the
// maybe takes, each trimmed to its in and out marks (the whole take without), its marks and voice notes as markers.
[[nodiscard]] za::String timelineXml(const za::String& name)
{
    const Rate rate{session.fpsNum > 0 ? session.fpsNum : 60, session.fpsDen > 0 ? session.fpsDen : 1};
    const int width = session.width > 0 ? session.width : 1920, height = session.height > 0 ? session.height : 1080;
    za::Vector<const Take*> order;
    for(const Rating r : {Rating::Keep, Rating::Maybe})
    {
        for(const Take& t : session.takes)
        {
            if(t.rating == r && !t.running && !t.path.empty())
            {
                order.emplaceBack(&t);
            }
        }
    }
    za::String x{"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE fcpxml>\n<fcpxml version=\"1.10\">\n  <resources>\n"};
    x += va("    <format id=\"r1\" frameDuration=\"%s\" width=\"%d\" height=\"%d\"/>\n", rate.time(1), width, height);
    for(za::SizeT i = 0; i < order.size(); i++)
    {
        const Take& t = *order[i];
        x += va("    <asset id=\"r%d\" name=\"", static_cast<int>(i) + 2);
        x += xmlText(files::fileName(t.path.toStringView()));
        x += va("\" start=\"0s\" duration=\"%s\" hasVideo=\"1\" format=\"r1\" hasAudio=\"1\" audioSources=\"1\" "
                "audioChannels=\"2\" audioRate=\"48000\">\n      <media-rep kind=\"original-media\" src=\"",
            rate.time(za::max<za::I64>(1, rate.frames(t.length))));
        x += xmlText(fileUrl(t.path).toStringView());
        x += "\"/>\n    </asset>\n";
    }
    za::String spine;
    za::I64 offset = 0;
    for(za::SizeT i = 0; i < order.size(); i++)
    {
        const Take& t = *order[i];
        double in, out;
        inOut(t, in, out);
        const za::I64 f0 = rate.frames(in), f1 = za::max(f0 + 1, rate.frames(out));
        spine += va("            <asset-clip ref=\"r%d\" offset=\"%s\" name=\"", static_cast<int>(i) + 2, rate.time(offset));
        spine += xmlText(files::fileName(t.path.toStringView()));
        spine += va("\" start=\"%s\"", rate.time(f0));
        spine += va(" duration=\"%s\" format=\"r1\">\n", rate.time(f1 - f0));
        for(const Mark& m : t.marks)
        {
            const za::I64 f = rate.frames(m.t);
            if(f < f0 || f >= f1)
            {
                continue; // (outside the trimmed clip: in session.json only)
            }
            const za::String said = m.kind == MarkKind::Note ? transcript(m.text) : za::String{};
            spine += va("              <marker start=\"%s\" duration=\"%s\" value=\"", rate.time(f), rate.time(1));
            spine += xmlText(said.empty() ? za::StringView{markName(t, m)} : said.toStringView());
            if(m.kind == MarkKind::Note)
            {
                spine += "\" note=\"";
                spine += xmlText(files::fileName(m.text.toStringView()));
            }
            spine += "\"/>\n";
        }
        spine += "            </asset-clip>\n";
        offset += f1 - f0;
    }
    x += "  </resources>\n  <library>\n    <event name=\"";
    x += xmlText(name.toStringView());
    x += "\">\n      <project name=\"";
    x += xmlText(name.toStringView());
    x += va("\">\n        <sequence format=\"r1\" duration=\"%s\" tcStart=\"0s\" tcFormat=\"NDF\" audioLayout=\"stereo\" "
            "audioRate=\"48k\">\n          <spine>\n",
        rate.time(offset));
    x += spine;
    x += "          </spine>\n        </sequence>\n      </project>\n    </event>\n  </library>\n</fcpxml>\n";
    return x;
}

// vr_takes_session end: the keep and maybe files moved to <folder>/session_<when it opened>/ with session.json and
// timeline.fcpxml; rejected/ and the unrated takes and clips stay where they are (listed in session.json).
void endSession()
{
    if(!session.open)
    {
        Con_Printf("takes: no session open\n");
        return;
    }
    za::String base = session.recordDir;
    for(const Take& t : session.takes)
    {
        if(!t.dir.empty())
        {
            base = t.dir;
            break;
        }
    }
    if(session.takes.empty() || base.empty())
    {
        closeSession();
        return;
    }
    const za::String folder = files::join(base.toStringView(), (za::String{"session_"} + session.stamp).toStringView());
    files::createDirectories(folder.cStr());
    int moved = 0;
    for(Take& t : session.takes)
    {
        if((t.rating != Rating::Keep && t.rating != Rating::Maybe) || t.path.empty() || t.running)
        {
            continue;
        }
        const za::String to = files::join(folder.toStringView(), files::fileName(t.path.toStringView()));
        if(!files::exists(to.cStr()) && files::rename(t.path.cStr(), to.cStr()))
        {
            t.path = to;
            moved++;
        }
        else
        {
            Con_Printf("takes: could not move %s into the session's folder (left where it is)\n", t.path.cStr());
        }
    }
    const bool fpsGuessed = session.fpsNum <= 0;
    const bool jsonOk = files::writeText(files::join(folder.toStringView(), "session.json").cStr(), sessionJson(folder, fpsGuessed).toStringView());
    const za::String name = za::String{"Quake VR takes "} + session.stamp;
    const bool xmlOk = files::writeText(files::join(folder.toStringView(), "timeline.fcpxml").cStr(), timelineXml(name).toStringView());
    Con_Printf("takes: session written: %s (%d files moved; session.json %s, timeline.fcpxml %s%s)\n", folder.cStr(), moved,
        jsonOk ? "written" : "NOT written", xmlOk ? "written" : "NOT written", fpsGuessed ? "; OBS's frame rate unknown: 60 assumed" : "");
    closeSession();
}

// ---------------------------------------------------------------- scene snapshots and retakes

// The snapshot's save: <game dir>/take_<scene>.sav (a savegame: the map, the toolgun's props and every entity, the
// player's place and angles).
[[nodiscard]] za::String snapshotName()
{
    return za::String{"take_"} + sceneName();
}

void setup_f()
{
    if(!sv.active || cl.intermission)
    {
        Con_Printf("vr_take_setup: not in a game (a snapshot is a savegame)\n");
        return;
    }
    Cbuf_InsertText(va("save %s\n", snapshotName().cStr()));
    Con_Printf("takes: scene %s set up (vr_take_retake loads it and starts a take)\n", sceneName().cStr());
}

// The retake: the snapshot loaded, then a take once the game runs again (its load seen, the view settled for
// vr_takes_retake_delay seconds).
struct Retake
{
    int stage{0}; // 0 none, 1 load asked (waiting to see it start), 2 loading, 3 settling
    double at{0.0};
};
Retake retake;
int retakesStarted = 0; // (vr_takes_test)

void retake_f()
{
    const za::String name = snapshotName();
    if(!files::exists(files::join(com_gamedir, (name + ".sav").toStringView()).cStr()))
    {
        Con_Printf("vr_take_retake: scene %s has no snapshot (vr_take_setup first)\n", sceneName().cStr());
        return;
    }
    if(obs::recording())
    {
        (void)obs::request("StopRecord"); // (the take before ends: the retake is a new one)
    }
    Cbuf_InsertText(va("load %s\n", name.cStr()));
    retake.stage = 1;
    retake.at = realtime;
    if(!obs::ready())
    {
        obs::connectNow();
    }
    Con_Printf("takes: retake of scene %s\n", sceneName().cStr());
}

void retakeFrame()
{
    const bool inGame = cls.state == ca_connected && cls.signon == SIGNONS && sv.active;
    switch(retake.stage)
    {
        case 1:
            if(!inGame)
            {
                retake.stage = 2;
            }
            else if(realtime - retake.at > 10.0)
            {
                retake.stage = 0;
                Con_Printf("takes: the retake's load did not start\n");
            }
            break;
        case 2:
            if(inGame)
            {
                retake.stage = 3;
                retake.at = realtime;
            }
            break;
        case 3:
            if(realtime - retake.at >= vr_takes_retake_delay.value && !obs::recording())
            {
                retake.stage = 0;
                retakesStarted++;
                act(Pending::Take);
            }
            break;
        default: break;
    }
}

// ---------------------------------------------------------------- vr_takes_test

// vr_takes_test <folder>: takes, marks, ratings, a clip and the numbering against the OBS mock (vr_obs_mock), its files
// in a new folder under <folder>; one step a frame (or on a condition), at the fixed clock's 1/72 s a frame. Ends with
// "vr_takes_test: N passed, M failed" (a give-up after 2000 frames: FAIL timed out).
struct Test
{
    bool on{false};
    int step{0};
    int wait{0};
    int frames{0};
    int retakes{0};
    testkit::Tally tally;
    za::String dir;
};
Test test;

bool check(bool ok, const char* what)
{
    if(test.tally.check(ok, "vr_takes_test", what))
    {
        Con_Printf("vr_takes_test: ok %s\n", what);
    }
    return ok;
}

[[nodiscard]] bool there(int number, const char* rating, bool rejected = false)
{
    char name[128];
    q_snprintf(name, sizeof(name), "%s%s_testscene_t%02d_%s.mkv", rejected ? "rejected/" : "", today().cStr(), number, rating);
    return files::exists(files::join(test.dir.toStringView(), name).cStr());
}

[[nodiscard]] bool near(double a, double b)
{
    return a > b - 0.03 && a < b + 0.03;
}

void testEnd()
{
    test.on = false;
    test.tally.verdict("vr_takes_test");
}

void testStep()
{
    if(!test.on)
    {
        return;
    }
    if(++test.frames > 2000)
    {
        check(false, va("timed out at step %d", test.step));
        testEnd();
        return;
    }
    if(test.wait > 0)
    {
        test.wait--;
        return;
    }
    const Take* last = session.takes.empty() ? nullptr : &session.takes.back();
    const bool named = last && !last->running && last->number > 0;
    switch(test.step++)
    {
        case 0:
            Cvar_Set("vr_obs", "1");
            Cvar_Set("vr_obs_mock", "1");
            Cvar_Set("vr_obs_mock_dir", test.dir.cStr());
            Cvar_Set("vr_take_scene", "testscene");
            Cvar_Set("vr_takes", "1");
            Cvar_Set("vr_takes_replay", "1");
            test.wait = 1;
            break;
        case 1: act(Pending::Take); break;
        case 2:
            if(!runningTake())
            {
                test.step--;
                break;
            }
            {
                bool lit = false;
                const float saved = vr_takes_light_flat.value;
                Cvar_SetValueQuick(&vr_takes_light_flat, 0.f);
                check(flatIndicator(lit) == nullptr && !lit, "flat: no corner indicator by default (vr_takes_light_flat 0)");
                Cvar_SetValueQuick(&vr_takes_light_flat, 1.f);
                check(flatIndicator(lit) != nullptr && lit, "flat: the corner indicator shows with vr_takes_light_flat 1");
                Cvar_SetValueQuick(&vr_takes_light_flat, saved);
            }
            test.wait = 35; // (the in mark 36 frames on: 0.5 s)
            break;
        case 3:
        {
            mark(MarkKind::In);
            const za::String note = files::join(test.dir.toStringView(), "note1");
            (void)files::writeText((note + ".transcript.txt").cStr(), " Nice\nshot \n");
            voiceNote(note.cStr()); // (as a voice note started now: its transcript as transcribe_notes.py leaves it)
            test.wait = 35;
            break;
        }
        case 4:
            mark(MarkKind::Out);
            test.wait = 35;
            break;
        case 5: act(Pending::Take); break;
        case 6:
            if(!named)
            {
                test.step--;
                break;
            }
            check(there(1, "unrated"), "take 1 named <date>_testscene_t01_unrated.mkv");
            check(last->marks.size() == 3 && last->marks[0].kind == MarkKind::In && near(last->marks[0].t, 0.5) &&
                      last->marks[1].kind == MarkKind::Note && near(last->marks[1].t, 0.5) &&
                      last->marks[2].kind == MarkKind::Out && near(last->marks[2].t, 1.0),
                va("in mark, voice note and out mark at 0.5, 0.5 and 1.0 s (%d marks)", static_cast<int>(last->marks.size())));
            check(last->length > 1.49 && last->length < 1.55, va("take 1 is 1.5 s long (%.3f)", last->length));
            check(chaptersAsked == 2, va("two OBS chapters asked for (%d)", chaptersAsked));
            check(currentTake() == last && realtime < session.promptUntil, "the rating prompt shows for take 1");
            rate(Rating::Maybe, 0);
            break;
        case 7:
            check(there(1, "maybe") && !there(1, "unrated"), "rated maybe: renamed t01_maybe");
            act(Pending::Take);
            break;
        case 8:
            if(!runningTake())
            {
                test.step--;
                break;
            }
            test.wait = 10;
            break;
        case 9: act(Pending::Take); break;
        case 10:
            if(!named)
            {
                test.step--;
                break;
            }
            check(there(2, "unrated"), "take 2 named t02_unrated");
            rate(Rating::Reject, 0);
            break;
        case 11:
            check(there(2, "reject", true) && !there(2, "unrated") && !there(2, "reject"), "rejected: moved to rejected/t02_reject");
            check(session.replayActive, "the session started OBS's replay buffer");
            act(Pending::Clip);
            break;
        case 12:
            if(session.takes.size() < 3 || !named)
            {
                test.step--;
                break;
            }
            check(last->clip && there(3, "clip"), "a clip saved and named t03_clip");
            check(last->length > 1.0 && last->length < 20.0,
                va("the clip holds what the buffer ran, under OBS's 20 s (%.2f)", last->length));
            rate(Rating::Maybe, 0);
            break;
        case 13:
            check(there(3, "maybe") && !there(3, "clip"), "the clip rated maybe: t03_maybe");
            rate(Rating::Keep, 1);
            break;
        case 14:
        {
            check(there(1, "keep") && !there(1, "maybe"), "take 1 rated again by number: t01_keep");
            const za::String folder = files::join(test.dir.toStringView(), (za::String{"session_"} + session.stamp).toStringView());
            endSession();
            const auto in = [&](int n, const char* rating) {
                return files::exists(files::join(folder.toStringView(), va("%s_testscene_t%02d_%s.mkv", today().cStr(), n, rating)).cStr());
            };
            check(in(1, "keep") && in(3, "maybe") && !there(1, "keep") && !there(3, "maybe") && there(2, "reject", true),
                "the session's end moved keep and maybe into session_<when>/, rejected/ left");
            za::String json, xml;
            const bool haveJson = files::readText(files::join(folder.toStringView(), "session.json").cStr(), json);
            const bool haveXml = files::readText(files::join(folder.toStringView(), "timeline.fcpxml").cStr(), xml);
            check(haveJson && json.toStringView().find("\"text\": \"Nice shot\"") != za::StringView::nPos &&
                      json.toStringView().find("\"rating\": \"reject\"") != za::StringView::nPos,
                "session.json: every take, the voice note's transcript");
            const za::SizeT keepAt = xml.toStringView().find("t01_keep.mkv\" start=\"30/60s\" duration=\"30/60s\"");
            const za::SizeT maybeAt = keepAt == za::StringView::nPos ? keepAt : xml.toStringView().find("t03_maybe.mkv\" start=\"0s\"", keepAt); // (in the spine: after the keep clip)
            check(haveXml && keepAt != za::StringView::nPos && maybeAt != za::StringView::nPos && keepAt < maybeAt,
                va("timeline.fcpxml: the keep take trimmed to its in and out (0.5-1.0 s), then the maybe clip whole (%d %d %d)",
                    haveXml ? 1 : 0, static_cast<int>(keepAt), static_cast<int>(maybeAt)));
            check(xml.toStringView().find("value=\"Nice shot\" note=\"note1\"") != za::StringView::nPos &&
                      xml.toStringView().find("value=\"In 1\"") != za::StringView::nPos && xml.toStringView().find("value=\"Out 1\"") == za::StringView::nPos,
                "timeline.fcpxml: the in mark and the voice note as markers (the out mark is the clip's end)");
            Con_Printf("vr_takes_test: timeline %s/timeline.fcpxml\n", folder.cStr());
            act(Pending::Take);
            break;
        }
        case 15:
            if(!runningTake())
            {
                test.step--;
                break;
            }
            act(Pending::Take);
            break;
        case 16:
            if(!named)
            {
                test.step--;
                break;
            }
            check(last->number == 4 && there(4, "unrated"), va("a new session's take goes on from the files: t04 (t%02d)", last->number));
            if(!check(sv.active, "a map runs (the retake's snapshot is a savegame)"))
            {
                testEnd();
                break;
            }
            setup_f();
            break;
        case 17:
            if(!files::exists(files::join(com_gamedir, (snapshotName() + ".sav").toStringView()).cStr()))
            {
                test.step--;
                break;
            }
            test.retakes = retakesStarted;
            retake_f();
            break;
        case 18:
            if(retake.stage != 0 || !runningTake())
            {
                test.step--;
                break;
            }
            check(retakesStarted == test.retakes + 1 && sv.active && cls.signon == SIGNONS,
                "retake: the snapshot loaded, then a take started by itself");
            act(Pending::Take);
            break;
        case 19:
            if(!named)
            {
                test.step--;
                break;
            }
            check(last->number == 5, va("the retake is take 5 (t%02d)", last->number));
            closeSession();
            testEnd();
            break;
        default: testEnd();
    }
}

void test_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_takes_test <folder>: the takes' test against the OBS mock, its files in a new folder in there\n");
        return;
    }
    closeSession();
    test = Test{};
    test.on = true;
    test.dir = files::join(files::generic(Cmd_Argv(1)).toStringView(), files::timestamp(time(nullptr)).toStringView());
    chaptersAsked = 0;
    Con_Printf("vr_takes_test: files in %s\n", test.dir.cStr());
}

} // namespace

// ---------------------------------------------------------------- the API

void registerCommands()
{
    Cmd_AddCommand("vr_take", take_f);
    Cmd_AddCommand("vr_clip", clip_f);
    Cmd_AddCommand("vr_take_keep", keep_f);
    Cmd_AddCommand("vr_take_maybe", maybe_f);
    Cmd_AddCommand("vr_take_reject", reject_f);
    Cmd_AddCommand("vr_take_rate", rate_f);
    Cmd_AddCommand("vr_take_in", in_f);
    Cmd_AddCommand("vr_take_out", out_f);
    Cmd_AddCommand("vr_take_list", list_f);
    Cmd_AddCommand("vr_takes_session", session_f);
    Cmd_AddCommand("vr_takes_page_hide", hide_f);
    Cmd_AddCommand("vr_takes_test", test_f);
    Cmd_AddCommand("vr_take_setup", setup_f);
    Cmd_AddCommand("vr_take_retake", retake_f);
}

void frame()
{
    za::String m;
    while(obs::takeMessage(m))
    {
        if(vr_takes.value)
        {
            message(m);
        }
    }
    const bool ready = obs::ready();
    if(ready && !wasReady && vr_takes.value)
    {
        connected();
    }
    if(!ready && wasReady)
    {
        session.replayActive = false;
    }
    wasReady = ready;
    obs::stayConnected(vr_takes.value != 0.f && (session.open || pending != Pending::None));
    if(pending != Pending::None)
    {
        if(ready)
        {
            const Pending p = pending;
            pending = Pending::None;
            p == Pending::Take ? doTake(-1) : doClip();
        }
        else if(realtime > pendingUntil)
        {
            pending = Pending::None;
            Con_Printf("takes: OBS did not connect (vr_obs_status says why)\n");
        }
    }
    for(Take& t : session.takes)
    {
        if(t.movePending && realtime >= t.moveAt)
        {
            if(!moveFile(t, t.rating) && --t.moveTriesLeft <= 0)
            {
                t.movePending = false;
                Con_Printf("takes: could not move %s (left as it is)\n", t.path.cStr());
            }
        }
    }
    retakeFrame();
    testStep();
    if(gadgetPage())
    {
        touch::Request req;
        req.gadget = true;
        req.pixels = pagePixels;
        touch::request(req);
    }
}

void drawInEye(const hands::State& s)
{
    char line1[64], line2[96];
    if(!indicatorLines(line1, line2, false))
    {
        return;
    }
    glm::vec3 f, r, u;
    hands::angleVectors(s.headAngles, f, r, u);
    const float m = units::metresToUnits();
    const float dist = za::max(0.2f, vr_takes_light_dist.value) * m;
    const float ch = dist * 0.02f * za::max(0.2f, vr_takes_light_size.value); // a character's height (units): ~1.1 degrees
    const glm::vec3 origin = s.head + f * dist + r * (vr_takes_light_right.value * dist) + u * (vr_takes_light_up.value * dist);
    textVerts.clear();
    shapeVerts.clear();
    discVerts.clear();
    const auto line = [&](const char* text, const glm::vec3& at, const glm::vec4& color) {
        const float len = static_cast<float>(strlen(text));
        quad(shapeVerts, at - r * (ch * 0.3f) - u * (ch * 0.25f), r * (ch * (len + 0.6f)), u * (ch * 1.5f), {0.f, 0.f, 0.f, 0.5f}, false);
        for(int i = 0; text[i]; i++)
        {
            glyphQuad(textVerts, at + r * (ch * static_cast<float>(i)), r * ch, u * ch, static_cast<unsigned char>(text[i]), color);
        }
    };
    glm::vec3 at = origin;
    if(line1[0])
    {
        const float blink = 0.6f + 0.4f * cosf(static_cast<float>(realtime) * 4.f);
        quad(discVerts, at - r * (ch * 1.6f) - u * (ch * 0.1f), r * (ch * 1.2f), u * (ch * 1.2f), {1.f, 0.1f, 0.1f, blink}, true);
        line(line1, at, {1.f, 0.85f, 0.85f, 1.f});
        at -= u * (ch * 1.8f);
    }
    if(line2[0])
    {
        line(line2, at, {1.f, 1.f, 0.7f, 1.f});
    }
    const glm::mat4 vp = gfx::sceneViewProjection();
    gfx::draw(shapeVerts, vp, {.shade = gfx::Shade::Color, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
    gfx::draw(discVerts, vp, {.shade = gfx::Shade::SoftEdge, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
    gfx::draw(textVerts, vp, {.shade = gfx::Shade::Texture, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false},
        gfx::fontTexture());
}

const char* flatIndicator(bool& light)
{
    char line1[64], line2[96];
    light = false;
    if(!indicatorLines(line1, line2, true))
    {
        return nullptr;
    }
    light = line1[0] != '\0';
    q_snprintf(flatText, sizeof(flatText), "%s%s%s", line1, line1[0] && line2[0] ? "  " : "", line2);
    return flatText;
}

bool gadgetPage()
{
    return vr_takes.value && vr_takes_gadget.value && session.open && !pageHidden && gadget::active();
}

void drawGadgetPage(float width, float height)
{
    using namespace gfx::draw2D;
    pagePixels = {width, height};
    const Take* run = runningTake();
    const Take* cur = currentTake();
    fill(2.f, 2.f, width - 4.f, height - 4.f, glm::vec3{0.02f, 0.03f, 0.04f});
    color({1.f, 0.3f, 0.3f, 1.f});
    char line[64];
    if(run)
    {
        const int s = static_cast<int>(realtime - run->started);
        q_snprintf(line, sizeof(line), "REC %d:%02d %s", s / 60, s % 60, run->scene.cStr());
    }
    else
    {
        color({0.7f, 0.9f, 1.f, 1.f});
        q_snprintf(line, sizeof(line), "TAKES %s", sceneName().cStr());
    }
    text(6.f, 6.f, 8.f, line);
    color({0.9f, 0.9f, 0.7f, 1.f});
    if(cur)
    {
        q_snprintf(line, sizeof(line), "%s %s %.0fs", label(*cur), ratingNames[static_cast<int>(cur->rating)], cur->length);
        text(6.f, 17.f, 8.f, line);
    }
    color({1.f, 1.f, 1.f, 1.f});

    ui.begin(touch::contact(), {width, height}, 1.f);
    const float top = 30.f, gap = 3.f;
    const float rowH = (height - top - 4.f - gap * 2.f) / 3.f;
    const auto btn = [&](int id, int col, int cols, int row, const char* label, touch::Look look) {
        const float w = (width - 8.f - gap * static_cast<float>(cols - 1)) / static_cast<float>(cols);
        return ui.button(id, {4.f + (w + gap) * static_cast<float>(col), top + (rowH + gap) * static_cast<float>(row), w, rowH},
            label, look);
    };
    if(btn(1, 0, 4, 0, run ? "STOP" : "TAKE", run ? touch::Look::On : touch::Look::Accent).pressed)
    {
        act(Pending::Take);
    }
    if(btn(2, 1, 4, 0, "CLIP", touch::Look::Normal).pressed)
    {
        act(Pending::Clip);
    }
    if(btn(3, 2, 4, 0, "IN", run ? touch::Look::Normal : touch::Look::Quiet).pressed)
    {
        mark(MarkKind::In);
    }
    if(btn(4, 3, 4, 0, "OUT", run ? touch::Look::Normal : touch::Look::Quiet).pressed)
    {
        mark(MarkKind::Out);
    }
    const bool rateable = cur && !cur->running;
    const auto look = [&](Rating r) {
        return !rateable ? touch::Look::Quiet : cur->rating == r ? touch::Look::On : touch::Look::Normal;
    };
    if(btn(5, 0, 3, 1, "KEEP", look(Rating::Keep)).pressed)
    {
        rate(Rating::Keep, 0);
    }
    if(btn(6, 1, 3, 1, "MAYBE", look(Rating::Maybe)).pressed)
    {
        rate(Rating::Maybe, 0);
    }
    if(btn(7, 2, 3, 1, "REJECT", look(Rating::Reject)).pressed)
    {
        rate(Rating::Reject, 0);
    }
    if(btn(8, 0, 4, 2, "SET UP", touch::Look::Normal).pressed)
    {
        setup_f();
    }
    if(btn(9, 1, 4, 2, "RETAKE", touch::Look::Normal).pressed)
    {
        retake_f();
    }
    if(btn(10, 2, 4, 2, "END", run ? touch::Look::Quiet : touch::Look::Normal).released && !run)
    {
        endSession(); // (on release: the page goes with the session)
    }
    if(btn(11, 3, 4, 2, "HIDE", touch::Look::Quiet).pressed)
    {
        pageHidden = true;
    }
    ui.draw();
}

bool controllerButton(int hand, int button, bool down)
{
    if(hand < 0 || hand >= HAND_COUNT || button < 0 || button >= 5)
    {
        return false;
    }
    chordHeld[hand][button] = down;
    if(!down && chordSwallow[hand][button])
    {
        chordSwallow[hand][button] = false;
        return true; // (the press was the chord's: so is its release)
    }
    const int mode = static_cast<int>(vr_takes_chord.value);
    const int chordHand = mode == 1 ? HAND_OFF : mode == 2 ? HAND_MAIN : -1;
    if(!down || hand != chordHand || !vr_takes.value || !runningTake() || !chordHeld[hand][1] || (button != 2 && button != 3))
    {
        return false;
    }
    chordSwallow[hand][button] = true;
    mark(button == 2 ? MarkKind::In : MarkKind::Out);
    return true;
}

void voiceNote(const char* base)
{
    Take* t = runningTake();
    if(!t || !base)
    {
        return;
    }
    Mark m;
    m.kind = MarkKind::Note;
    m.t = realtime - t->started;
    m.text = za::String{base};
    t->marks.emplaceBack(static_cast<Mark&&>(m));
    Con_Printf("takes: voice note at %.2f s\n", t->marks.back().t);
}

void defaultKeys()
{
    if(vr_takes_keys.value >= 1.f)
    {
        return;
    }
    struct Key
    {
        const char* key;
        const char* command;
    };
    static constexpr Key keys[] = {{"F7", "vr_take"}, {"F8", "vr_clip"}, {"HOME", "vr_take_in"}, {"PGUP", "vr_take_out"},
        {"KP_END", "vr_take_keep"}, {"KP_DOWNARROW", "vr_take_maybe"}, {"KP_PGDN", "vr_take_reject"}};
    for(const Key& k : keys)
    {
        const int n = Key_StringToKeynum(k.key);
        if(n >= 0 && (!keybindings[n] || !keybindings[n][0]))
        {
            Key_SetBinding(n, k.command);
        }
    }
    Cvar_SetValueQuick(&vr_takes_keys, 1.f);
}

const char* statusLine()
{
    if(!vr_takes.value)
    {
        q_strlcpy(statusText, "Takes: off", sizeof(statusText));
        return statusText;
    }
    int kept = 0;
    for(const Take& t : session.takes)
    {
        kept += t.rating == Rating::Keep ? 1 : 0;
    }
    const Take* run = runningTake();
    const Take* cur = currentTake();
    q_snprintf(statusText, sizeof(statusText), "Takes: scene %s; %s%s%s", sceneName().cStr(),
        !session.open ? "no session" : va("%d takes and clips, %d keep", static_cast<int>(session.takes.size()), kept),
        run ? "; a take runs" : "", cur && !run ? va("; last %s %s", label(*cur), ratingNames[static_cast<int>(cur->rating)]) : "");
    return statusText;
}

} // namespace qvr::takes

extern "C" const char* VR_TakesIndicator(int* light, float* scale)
{
    bool lit = false;
    const char* text = VR_IsActive() ? nullptr : qvr::takes::flatIndicator(lit); // (the headset's: in it alone)
    *light = lit ? 1 : 0;
    *scale = CLAMP(0.5f, qvr::vr_takes_light_size.value, 4.f) * 0.75f;
    return text;
}
