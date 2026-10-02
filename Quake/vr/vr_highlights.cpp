// vr_highlights.cpp -- see vr_highlights.hpp.

#include "vr_highlights.hpp"
#include "vr_audio.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"

#include <stdio.h>
#include <string.h>
#include <time.h>

namespace qvr::highlights
{
namespace
{

// A moment logged (the main thread: the server's QC, bullet time, the frame).
struct Event
{
    double t = 0.0;        // real seconds after the sync mark
    double gameTime = 0.0; // the server's clock (slowed in slow motion)
    float duration = 0.f;  // real seconds (0: an instant)
    float score = 0.f;
    int count = 1;         // a multi-kill's kills
    za::String kind, map, subject, detail;
};

// Each kind's own score (a QC call's score 0) and marker colour (Resolve's names). Unknown kinds: 1, blue.
struct KindInfo
{
    const char* kind;
    float score;
    const char* colour;
};
static constexpr KindInfo kinds[] = {
    {"sync", 0.f, "Cream"},          {"mark", 5.f, "Fuchsia"},        {"kill", 1.f, "Blue"},
    {"gib", 3.f, "Red"},             {"corpsegib", 1.5f, "Rose"},     {"multikill", 4.f, "Rose"},
    {"parry", 3.f, "Cyan"},          {"counter", 4.f, "Green"},       {"shoveparry", 4.f, "Mint"},
    {"bullettime", 4.f, "Purple"},   {"explosion", 3.f, "Yellow"},    {"barrel", 2.f, "Sand"},
    {"chain", 4.f, "Lemon"},         {"shotgrenade", 4.f, "Pink"},    {"axestick", 3.f, "Cocoa"},
    {"swing", 3.f, "Sky"},           {"grapplepull", 3.f, "Lavender"},
};

[[nodiscard]] const KindInfo* kindInfo(const char* kind)
{
    for(const KindInfo& k : kinds)
    {
        if(!strcmp(k.kind, kind))
        {
            return &k;
        }
    }
    return nullptr;
}

// The log (main thread).
struct Session
{
    bool active = false;
    double syncRealtime = 0.0; // realtime at the sync mark: t 0
    double syncGameTime = 0.0;
    char started[32] = {};     // the wall clock at the sync mark, "YYYY-MM-DD HH:MM:SS"
    za::String base;           // <game dir>/highlights/<date>_<time> (no extension)
    FILE* csv = nullptr;
    za::Vector<Event> events;

    // A multi-kill: kills no more than vr_highlights_multikill game seconds apart.
    int streak = 0;
    double streakFirstT = 0.0, streakLastT = 0.0, streakFirstGame = 0.0, streakLastGame = 0.0;
    za::String streakVictims;

    // Bullet time running since (t), and its scale.
    bool bulletTime = false;
    double bulletTimeT = 0.0, bulletTimeGame = 0.0;
    float bulletTimeScale = 1.f;

    double flashUntil = -1.0; // realtime: the sync mark's flash is drawn until then
    int syncs = 0;            // sync marks made (vr_timescale_wav: a game-time WAV from each)
};
Session session;

// The game's clock against realtime, once a frame: a moment that began `since` (game time) is placed in real time
// by them (main thread; the last few seconds).
struct ClockSample
{
    double game = 0.0;
    double real = 0.0;
};
struct ClockHistory
{
    za::Array<ClockSample, 1024> samples;
    int next = 0;
    int count = 0;
};
ClockHistory clockHistory;

[[nodiscard]] double gameTime()
{
    return sv.active ? sv.qcvm.time : cl.time;
}

[[nodiscard]] const char* mapName()
{
    return sv.active ? sv.name : (cl.mapname[0] ? cl.mapname : "");
}

// Realtime when the game's clock read `game` (now's when it is later than the history's last, or none fits).
[[nodiscard]] double realtimeAt(double game)
{
    if(game <= 0.0 || game >= gameTime() || clockHistory.count == 0)
    {
        return realtime;
    }
    const int n = static_cast<int>(clockHistory.samples.size());
    ClockSample later{gameTime(), realtime};
    for(int i = 0; i < clockHistory.count; ++i)
    {
        const ClockSample& s = clockHistory.samples[static_cast<za::SizeT>((clockHistory.next - 1 - i + n) % n)];
        if(s.game <= game)
        {
            const double span = later.game - s.game;
            return span > 0.0 ? s.real + (later.real - s.real) * (game - s.game) / span : s.real;
        }
        later = s;
    }
    return later.real; // older than the history: its oldest
}

// The text as one CSV field: quoted when it holds a comma, a quote or a line break (quotes doubled).
void csvField(za::String& out, const za::String& s)
{
    const bool quote = strpbrk(s.cStr(), ",\"\r\n") != nullptr;
    if(!quote)
    {
        out += s;
        return;
    }
    out += '"';
    for(const char c : s)
    {
        if(c == '"')
        {
            out += '"';
        }
        out += c;
    }
    out += '"';
}

void jsonString(za::String& out, const za::String& s)
{
    out += '"';
    for(const char c : s)
    {
        if(c == '"' || c == '\\')
        {
            out += '\\';
            out += c;
        }
        else if(static_cast<unsigned char>(c) < 0x20)
        {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\u%04x", static_cast<unsigned char>(c));
            out += esc;
        }
        else
        {
            out += c;
        }
    }
    out += '"';
}

void csvRow(const Event& e)
{
    if(!session.csv)
    {
        return;
    }
    char head[160];
    snprintf(head, sizeof(head), "%.3f,%.3f,", e.t, e.gameTime);
    za::String row{head};
    csvField(row, e.kind);
    snprintf(head, sizeof(head), ",%.2f,%.3f,%d,", e.score, e.duration, e.count);
    row += head;
    csvField(row, e.map);
    row += ',';
    csvField(row, e.subject);
    row += ',';
    csvField(row, e.detail);
    row += '\n';
    fputs(row.cStr(), session.csv);
    fflush(session.csv); // (a crash keeps every moment so far)
}

void add(Event&& e)
{
    if(vr_debug_highlights.value)
    {
        Con_Printf("highlight: %.2f s %s (%.1f)%s%s%s%s, %.2f s long\n", e.t, e.kind.cStr(), e.score,
                   e.subject.empty() ? "" : " ", e.subject.cStr(), e.detail.empty() ? "" : ": ", e.detail.cStr(),
                   e.duration);
    }
    csvRow(e);
    session.events.pushBack(ZA_MOVE(e));
}

[[nodiscard]] Event makeEvent(const char* kind, float score, double realStart)
{
    Event e;
    e.t = realStart - session.syncRealtime;
    e.gameTime = gameTime();
    e.kind = kind;
    const KindInfo* info = kindInfo(kind);
    e.score = score > 0.f ? score : (info ? info->score : 1.f);
    e.map = mapName();
    return e;
}

// The multi-kill so far, as one moment when it had two kills or more; then none.
void closeStreak()
{
    if(session.streak >= 2)
    {
        Event e = makeEvent("multikill", 2.f * static_cast<float>(session.streak), session.syncRealtime + session.streakFirstT);
        e.gameTime = session.streakFirstGame;
        e.duration = static_cast<float>(session.streakLastT - session.streakFirstT);
        e.count = session.streak;
        char what[48];
        snprintf(what, sizeof(what), "%d kills", session.streak);
        e.detail = what;
        e.subject = session.streakVictims;
        add(ZA_MOVE(e));
    }
    session.streak = 0;
    session.streakVictims = za::String{};
}

void closeBulletTime()
{
    if(!session.bulletTime)
    {
        return;
    }
    session.bulletTime = false;
    Event e = makeEvent("bullettime", 0.f, session.syncRealtime + session.bulletTimeT);
    e.gameTime = session.bulletTimeGame;
    e.duration = static_cast<float>(realtime - session.syncRealtime - session.bulletTimeT);
    char what[48];
    snprintf(what, sizeof(what), "%.2fx", session.bulletTimeScale);
    e.detail = what;
    add(ZA_MOVE(e));
}

// "HH:MM:SS:FF" of `seconds` at `fps` (whole frames, non-drop).
void timecode(char* out, za::SizeT size, double seconds, int fps)
{
    const long long frames = static_cast<long long>(seconds * fps + 0.5);
    const long long f = frames % fps;
    const long long s = frames / fps;
    snprintf(out, size, "%02lld:%02lld:%02lld:%02lld", s / 3600, (s / 60) % 60, s % 60, f);
}

// A marker's name as Resolve takes it: letters, digits and a few signs (no '|', which ends the field).
[[nodiscard]] za::String markerName(const Event& e)
{
    char text[320];
    snprintf(text, sizeof(text), "%s %.1f%s%s%s%s", e.kind.cStr(), e.score, e.subject.empty() ? "" : " ",
             e.subject.cStr(), e.detail.empty() ? "" : " - ", e.detail.cStr());
    za::String out;
    for(const char* c = text; *c; ++c)
    {
        const bool keep = (*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') ||
                          strchr(" _-.+:()x", *c) != nullptr;
        out += keep ? *c : ' ';
    }
    return out;
}

// Resolve's marker EDL (what Timelines > Export > Timeline Markers to EDL writes, and Import reads): an event a
// marker, one frame of the timeline at its time (the sync mark at 01:00:00:00), its colour, name and length in frames.
void writeEdl(const za::Vector<Event>& events)
{
    const int fps = za::max(1, static_cast<int>(vr_highlights_fps.value + 0.5f));
    za::String text{"TITLE: Quake VR highlights "};
    text += session.started;
    text += "\nFCM: NON-DROP FRAME\n\n";
    int n = 0;
    for(const Event& e : events)
    {
        if(e.score < vr_highlights_min_score.value && strcmp(e.kind.cStr(), "sync") != 0)
        {
            continue;
        }
        const double at = 3600.0 + za::max(0.0, e.t);
        char in[16], out[16], line[256];
        timecode(in, sizeof(in), at, fps);
        timecode(out, sizeof(out), at + 1.0 / fps, fps);
        snprintf(line, sizeof(line), "%03d  001      V     C        %s %s %s %s  \n", ++n, in, out, in, out);
        text += line;
        const KindInfo* info = kindInfo(e.kind.cStr());
        const int frames = za::max(1, static_cast<int>(e.duration * static_cast<float>(fps) + 0.5f));
        snprintf(line, sizeof(line), " |C:ResolveColor%s |M:%s |D:%d\n\n", info ? info->colour : "Blue",
                 markerName(e).cStr(), frames);
        text += line;
    }
    const za::String path = session.base + ".edl";
    if(!files::writeText(path.cStr(), text))
    {
        Con_Printf("highlights: can't write %s\n", path.cStr());
    }
}

void writeJson(const za::Vector<Event>& events)
{
    char line[256];
    za::String text{"{\n  \"format\": \"quakevr-highlights\",\n  \"version\": 1,\n  \"started\": "};
    jsonString(text, za::String{session.started});
    snprintf(line, sizeof(line), ",\n  \"sync\": {\"realtime\": %.3f, \"game_time\": %.3f},\n  \"fps\": %g,\n",
             session.syncRealtime, session.syncGameTime, static_cast<double>(vr_highlights_fps.value));
    text += line;
    text += "  \"events\": [\n";
    for(za::SizeT i = 0; i < events.size(); ++i)
    {
        const Event& e = events[i];
        snprintf(line, sizeof(line), "    {\"t\": %.3f, \"game_time\": %.3f, \"kind\": ", e.t, e.gameTime);
        text += line;
        jsonString(text, e.kind);
        snprintf(line, sizeof(line), ", \"score\": %.2f, \"duration\": %.3f, \"count\": %d, \"map\": ", e.score,
                 e.duration, e.count);
        text += line;
        jsonString(text, e.map);
        text += ", \"subject\": ";
        jsonString(text, e.subject);
        text += ", \"detail\": ";
        jsonString(text, e.detail);
        text += i + 1 < events.size() ? "},\n" : "}\n";
    }
    text += "  ]\n}\n";
    const za::String path = session.base + ".json";
    if(!files::writeText(path.cStr(), text))
    {
        Con_Printf("highlights: can't write %s\n", path.cStr());
    }
}

void syncMark(const char* why)
{
    if(vr_highlights_flash.value > 0.f)
    {
        session.flashUntil = realtime + vr_highlights_flash.value;
    }
    if(vr_highlights_beep.value > 0.f)
    {
        if(sfx_t* sfx = S_PrecacheSound("vr/sync_beep.wav")) // (none without sound)
        {
            S_StartSound(cl.viewentity, -1, sfx, vec3_origin, za::min(1.f, vr_highlights_beep.value), 1.f);
        }
    }
    // The game-time mix (as at normal speed: slow motion sped up in editing) from the beep on, a file a sync mark.
    session.syncs++;
    if(vr_timescale_wav.value != 0.f)
    {
        za::String path = session.base + "_gametime";
        if(session.syncs > 1)
        {
            char n[16];
            snprintf(n, sizeof(n), "_%d", session.syncs);
            path += n;
        }
        path += ".wav";
        audio::startGameWav(path.cStr());
    }
    Event e = makeEvent("sync", 0.f, realtime);
    e.detail = why;
    add(ZA_MOVE(e));
}

void start()
{
    if(session.active)
    {
        return;
    }
    const time_t now = time(nullptr);
    char stamp[32];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
    strftime(session.started, sizeof(session.started), "%Y-%m-%d %H:%M:%S", localtime(&now));
    const za::String dir = za::String{com_gamedir} + "/highlights";
    files::createDirectories(dir.cStr());
    session.base = dir + "/" + stamp;
    const za::String csvPath = session.base + ".csv";
    session.csv = fopen(csvPath.cStr(), "w");
    if(!session.csv)
    {
        Con_Printf("highlights: can't write %s\n", csvPath.cStr());
        Cvar_SetValueQuick(&vr_highlights, 0.f);
        return;
    }
    fputs("t,game_time,kind,score,duration,count,map,subject,detail\n", session.csv);
    session.active = true;
    session.events.clear();
    session.streak = 0;
    session.bulletTime = false;
    session.syncRealtime = realtime;
    session.syncGameTime = gameTime();
    session.syncs = 0;
    syncMark("start");
    Con_Printf("highlights: logging to %s.csv\n", session.base.cStr());
}

void stop()
{
    if(!session.active)
    {
        return;
    }
    closeStreak();
    closeBulletTime();
    za::stableSort(session.events.begin(), session.events.end(),
                   [](const Event& l, const Event& r) { return l.t < r.t; });
    writeJson(session.events);
    writeEdl(session.events);
    fclose(session.csv);
    session.csv = nullptr;
    audio::stopGameWav();
    session.active = false;
    session.flashUntil = -1.0;
    int markers = 0;
    for(const Event& e : session.events)
    {
        markers += e.score >= vr_highlights_min_score.value ? 1 : 0;
    }
    Con_Printf("highlights: %d moments (%d markers) in %s.csv, .json, .edl\n", static_cast<int>(session.events.size()),
               markers, session.base.cStr());
    session.events = za::Vector<Event>{};
}

void onCvar(cvar_t* var)
{
    if(var->value != 0.f)
    {
        start();
    }
    else
    {
        stop();
    }
}

void start_f()
{
    Cvar_SetValueQuick(&vr_highlights, 1.f);
}

void stop_f()
{
    Cvar_SetValueQuick(&vr_highlights, 0.f);
}

// vr_highlights_sync: another sync mark in a running log (a recording started after it).
void sync_f()
{
    if(!session.active)
    {
        Con_Printf("vr_highlights_sync: no log running (vr_highlights 1)\n");
        return;
    }
    syncMark("sync");
}

// vr_highlight_mark [score] [words...]: a moment by hand (a bound key: "that was good").
void mark_f()
{
    if(!session.active)
    {
        Con_Printf("vr_highlight_mark: no log running (vr_highlights 1)\n");
        return;
    }
    const float score = Cmd_Argc() >= 2 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : 0.f;
    za::String words;
    for(int i = 2; i < Cmd_Argc(); ++i)
    {
        words += i > 2 ? " " : "";
        words += Cmd_Argv(i);
    }
    event("mark", score, "", words.cStr());
}

} // namespace

void init()
{
    Cvar_SetCallback(&vr_highlights, onCvar);
    Cmd_AddCommand("vr_highlights_start", start_f);
    Cmd_AddCommand("vr_highlights_stop", stop_f);
    Cmd_AddCommand("vr_highlights_sync", sync_f);
    Cmd_AddCommand("vr_highlight_mark", mark_f);
}

bool active()
{
    return session.active;
}

void frame()
{
    const double game = gameTime();
    if(clockHistory.count > 0)
    {
        const za::SizeT n = clockHistory.samples.size();
        const ClockSample& last = clockHistory.samples[(static_cast<za::SizeT>(clockHistory.next) + n - 1) % n];
        if(game < last.game)
        {
            clockHistory.count = 0; // a new map: its clock starts again
        }
    }
    clockHistory.samples[static_cast<za::SizeT>(clockHistory.next)] = {game, realtime};
    clockHistory.next = (clockHistory.next + 1) % static_cast<int>(clockHistory.samples.size());
    clockHistory.count = za::min(clockHistory.count + 1, static_cast<int>(clockHistory.samples.size()));

    if(!session.active)
    {
        return;
    }
    if(session.streak > 0 &&
       (game - session.streakLastGame > static_cast<double>(vr_highlights_multikill.value) || game < session.streakLastGame))
    {
        closeStreak();
    }
}

void drawFlash()
{
    if(realtime >= session.flashUntil)
    {
        return;
    }
    static constexpr float white[3] = {1.f, 1.f, 1.f};
    GL_SetCanvas(CANVAS_DEFAULT);
    Draw_FillEx(0.f, 0.f, static_cast<float>(vid.width), static_cast<float>(vid.height), white, 1.f);
}

void shutdown()
{
    if(session.active)
    {
        stop();
    }
}

void event(const char* kind, float score, const char* subject, const char* detail, double since)
{
    if(!session.active)
    {
        return;
    }
    const double realStart = since > 0.0 ? realtimeAt(since) : realtime;
    Event e = makeEvent(kind, score, realStart);
    e.subject = subject ? subject : "";
    e.detail = detail ? detail : "";

    const bool kill = !strcmp(kind, "kill") || !strcmp(kind, "gib");
    if(kill)
    {
        const double game = gameTime();
        if(session.streak > 0 && game - session.streakLastGame > static_cast<double>(vr_highlights_multikill.value))
        {
            closeStreak();
        }
        if(session.streak == 0)
        {
            session.streakFirstT = e.t;
            session.streakFirstGame = game;
        }
        ++session.streak;
        session.streakLastT = e.t;
        session.streakLastGame = game;
        if(!e.subject.empty())
        {
            session.streakVictims += session.streakVictims.empty() ? "" : " ";
            session.streakVictims += e.subject;
        }
    }
    add(ZA_MOVE(e));
}

void bulletTime(bool on, float scale)
{
    if(!session.active)
    {
        return;
    }
    if(on)
    {
        closeBulletTime();
        session.bulletTime = true;
        session.bulletTimeT = realtime - session.syncRealtime;
        session.bulletTimeGame = gameTime();
        session.bulletTimeScale = scale;
        return;
    }
    closeBulletTime();
}

} // namespace qvr::highlights
