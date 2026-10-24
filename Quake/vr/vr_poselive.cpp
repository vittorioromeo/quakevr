// vr_poselive.cpp -- the Pose Studio's Live mode: see vr_poselive.hpp.

#include "vr_poselive.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <math.h>
#include <string.h>

namespace qvr::poselive
{
namespace
{

constexpr int tapeFrames = 1200;
constexpr double recordEvery = 1.0 / 120.0; // game seconds: a frame drawn sooner after the last kept is not kept
constexpr float pauseScale = 0.0001f;       // host_timescale paused: the game's clock all but stopped
constexpr float speeds[] = {1.f, 0.5f, 0.25f, 0.1f};
constexpr int numSpeeds = 4;
constexpr const char* loopNames[NumLoops] = {"All", "Aim+Fire", "Reload", "Run", "Strikes", "Pickup"};
constexpr float stickDead = 0.25f;
constexpr float stickRate = 1.5f;    // game seconds of tape a second at the stick's full deflection
constexpr double glitchMerge = 0.3;  // bad frames this close are one moment (the finder's)
constexpr int maxMoments = 64;
constexpr int numShown = 5;          // the measures on the panel
constexpr int shownMetric[numShown] = {2, 0, 3, 4, 8}; // pen, flip, reach, grip2, jump (agglitch's order)
constexpr int toggleMetric = 5;
constexpr int lineWidth = 36;        // the readout's lines (padded: the bars line up)

struct Tape
{
    Frame frames[tapeFrames];
    int next{0};  // where the next goes
    int count{0};
};
Tape tape;

struct Moment
{
    int index{0};
    float sev{0.f};
    double last{0.0};
};

struct Live
{
    bool on{false}, paused{false};
    int ent{0};
    int restarts{0};
    float timescaleWas{0.f};
    double lastRec{-1.0};
    Frame* open{nullptr};
    bool warming{false};
    int head{-1};       // the playhead (paused): a tape index
    // the server's keys on the subject this frame (qcKey)
    double keysTime{-1.0};
    int keysFrame{-1000};
    int nKeys{0};
    KeyUse keys[maxUses];
    // the playhead's measures as solved now (paused: edits laid on)
    bool shownOk{false};
    agglitch::Sample warm[2]; // a window's last two frames before the playhead as solved again (its measures' history)
    int warmN{0};
    float shown[agglitch::numMetrics]{};
    signed char shownSide[agglitch::numMetrics]{};
    float shownSev{0.f};
    int shownWorst{0};
    Moment moments[maxMoments];
    int nMoments{0};
    int glitch{-1};     // the moment Next Glitch went to last
    double lastReal{-1.0};
    glm::vec2 stick{0.f};
    za::String feedback;
    double feedbackUntil{0.0};
};
Live live;

void say(const char* text)
{
    Con_Printf("Pose Studio Live: %s\n", text);
    live.feedback = za::String{text};
    live.feedbackUntil = realtime + 5.0;
}

[[nodiscard]] int ringIndex(int i)
{
    return ((i % tapeFrames) + tapeFrames) % tapeFrames;
}

[[nodiscard]] int oldest()
{
    return ringIndex(tape.next - tape.count);
}

[[nodiscard]] int newest()
{
    return ringIndex(tape.next - 1);
}

// The tape's index `i` frames after the oldest (0 the oldest).
[[nodiscard]] int nth(int i)
{
    return ringIndex(oldest() + i);
}

// Its place on the tape (0 the oldest).
[[nodiscard]] int ordinal(int index)
{
    return ringIndex(index - oldest());
}

[[nodiscard]] float speedNow()
{
    return za::clamp(vr_posestudio_live_speed.value, 0.1f, 1.f);
}

[[nodiscard]] int loopNow()
{
    return za::clamp(static_cast<int>(vr_posestudio_live_loop.value), 0, NumLoops - 1);
}

void severity(const float* m, float& sev, int& worst)
{
    sev = 0.f;
    worst = 0;
    for(int k = 0; k < agglitch::numMetrics; k++)
    {
        const float v = k == toggleMetric ? 0.f : m[k] / agglitch::metricBad(k);
        if(v > sev)
        {
            sev = v;
            worst = k;
        }
    }
}

// The frame before tape index `index` (null: none, not measured).
[[nodiscard]] const Frame* before(int index, int by)
{
    const int o = ordinal(index);
    if(o - by < 0)
    {
        return nullptr;
    }
    const Frame& f = tape.frames[nth(o - by)];
    return f.measured ? &f : nullptr;
}

void pauseAt(int index)
{
    live.paused = true;
    live.head = index;
    live.shownOk = false;
}

void moveHead(int ord)
{
    if(tape.count <= 0)
    {
        return;
    }
    if(!live.paused)
    {
        pauseAt(newest());
    }
    live.head = nth(za::clamp(ord, 0, tape.count - 1));
    live.shownOk = false;
}

[[nodiscard]] glm::vec3 mirrorPlace(glm::vec3 v)
{
    v.y = -v.y;
    return v;
}

[[nodiscard]] glm::vec3 mirrorTurn(const glm::vec3& v)
{
    return glm::vec3{-v.x, v.y, -v.z};
}

void findMoments()
{
    live.nMoments = 0;
    int open = -1;
    for(int i = 0; i < tape.count; i++)
    {
        const int index = nth(i);
        const Frame& f = tape.frames[index];
        if(!f.measured || f.sev < 1.f)
        {
            continue;
        }
        if(open >= 0 && f.time - live.moments[open].last <= glitchMerge)
        {
            Moment& m = live.moments[open];
            m.last = f.time;
            if(f.sev > m.sev)
            {
                m.sev = f.sev;
                m.index = index;
            }
            continue;
        }
        if(live.nMoments < maxMoments)
        {
            open = live.nMoments++;
        }
        else
        {
            // (full: the mildest dropped for it)
            int least = 0;
            for(int k = 1; k < live.nMoments; k++)
            {
                least = live.moments[k].sev < live.moments[least].sev ? k : least;
            }
            open = least;
        }
        live.moments[open] = Moment{index, f.sev, f.time};
    }
    // (worst first)
    for(int a = 1; a < live.nMoments; a++)
    {
        for(int b = a; b > 0 && live.moments[b].sev > live.moments[b - 1].sev; b--)
        {
            const Moment t = live.moments[b];
            live.moments[b] = live.moments[b - 1];
            live.moments[b - 1] = t;
        }
    }
}

[[nodiscard]] const char* keyName(int code)
{
    const int cls = (code / 1000) % 10, pose = (code / 10) % 100, phase = code % 10;
    return va("%s%s%s", agpose::poseName(cls, pose), agpose::isStrike(pose) ? ", " : "",
        agpose::isStrike(pose) ? agpose::phaseName(phase) : "");
}

[[nodiscard]] const Frame* shownFrame()
{
    if(tape.count <= 0)
    {
        return nullptr;
    }
    return &tape.frames[live.paused && live.head >= 0 ? live.head : newest()];
}

void pad(za::String& line)
{
    while(static_cast<int>(line.size()) < lineWidth)
    {
        line += " ";
    }
}

// --- commands ------------------------------------------------------------------------------------------------------

void pause_f()
{
    togglePause();
}

void scrub_f()
{
    scrub(Cmd_Argc() >= 2 ? static_cast<float>(Q_atof(Cmd_Argv(1))) : -1.f);
}

void step_f()
{
    step(Cmd_Argc() >= 2 ? Q_atoi(Cmd_Argv(1)) : 1);
}

void glitch_f()
{
    nextGlitch();
}

void loop_f()
{
    cycleLoop();
}

void speed_f()
{
    cycleSpeed();
}

// vr_posestudio_live_report: the mode, the tape, the playhead's frame: its key weighing most, its measures.
void report_f()
{
    Con_Printf("poselive: on %d paused %d ent %d loop %s speed %.2f tape %d frames", live.on ? 1 : 0, live.paused ? 1 : 0,
        live.ent, loopNames[loopNow()], static_cast<double>(speedNow()), tape.count);
    const Frame* f = shownFrame();
    if(!f)
    {
        Con_Printf("\n");
        return;
    }
    const Frame& n = tape.frames[newest()];
    Con_Printf(" (%.2f s), head %.3f s (frame %d of %d)\n", tape.count > 0 ? n.time - tape.frames[oldest()].time : 0.0,
        f->time - n.time, ordinal(static_cast<int>(f - tape.frames)) + 1, tape.count);
    for(int i = 0; i < f->uses; i++)
    {
        Con_Printf("poselive: key %d %s w %.3f%s\n", f->use[i].code, keyName(f->use[i].code), static_cast<double>(f->use[i].w),
            f->use[i].mirror ? " mirrored" : "");
    }
    const bool solved = live.paused && live.shownOk;
    const float* m = solved ? live.shown : f->m;
    Con_Printf("poselive: %s sev %.2f (%s)", solved ? "solved now" : (f->measured ? "recorded" : "unmeasured"),
        static_cast<double>(solved ? live.shownSev : f->sev), agglitch::metricName(solved ? live.shownWorst : f->worst));
    for(int k = 0; k < agglitch::numMetrics; k++)
    {
        Con_Printf(" %s %.2f", agglitch::metricName(k), static_cast<double>(m[k]));
    }
    Con_Printf("\n");
    Con_Printf("poselive: sample R elbow %.2f %.2f %.2f grip %.2f %.2f %.2f, L grip %.2f %.2f %.2f\n",
        static_cast<double>(f->sample.arm[0].E.x), static_cast<double>(f->sample.arm[0].E.y), static_cast<double>(f->sample.arm[0].E.z),
        static_cast<double>(f->sample.arm[0].grip.x), static_cast<double>(f->sample.arm[0].grip.y),
        static_cast<double>(f->sample.arm[0].grip.z), static_cast<double>(f->sample.arm[1].grip.x),
        static_cast<double>(f->sample.arm[1].grip.y), static_cast<double>(f->sample.arm[1].grip.z));
}

} // namespace

void init()
{
    Cmd_AddCommand("vr_posestudio_live_pause", pause_f);
    Cmd_AddCommand("vr_posestudio_live_scrub", scrub_f);
    Cmd_AddCommand("vr_posestudio_live_step", step_f);
    Cmd_AddCommand("vr_posestudio_live_glitch", glitch_f);
    Cmd_AddCommand("vr_posestudio_live_loop_next", loop_f);
    Cmd_AddCommand("vr_posestudio_live_speed_next", speed_f);
    Cmd_AddCommand("vr_posestudio_live_report", report_f);
}

bool on()
{
    return live.on;
}

bool paused()
{
    return live.on && live.paused;
}

int subject()
{
    return live.on ? live.ent : 0;
}

int loop()
{
    return loopNow();
}

float speed()
{
    return speedNow();
}

int restarts()
{
    return live.restarts;
}

bool start(int ent)
{
    if(ent <= 0)
    {
        say("walk up to an armed grunt or enforcer first");
        return false;
    }
    const int restartsWas = live.restarts;
    stop();
    live = Live{};
    live.restarts = restartsWas + 1;
    live.on = true;
    live.ent = ent;
    live.timescaleWas = Cvar_VariableValue("host_timescale");
    live.lastReal = realtime;
    tape.next = 0;
    tape.count = 0;
    say(va("on: Loop %s at %gx; Pause to stop it there, then scrub and drag", loopNames[loopNow()], static_cast<double>(speedNow())));
    return true;
}

void stop()
{
    if(!live.on)
    {
        return;
    }
    if(Cvar_VariableValue("host_timescale") != live.timescaleWas)
    {
        Cvar_SetValue("host_timescale", live.timescaleWas);
    }
    live.on = false;
    live.paused = false;
    live.open = nullptr;
    live.warming = false;
}

void togglePause()
{
    if(!live.on)
    {
        return;
    }
    if(live.paused)
    {
        live.paused = false;
        live.head = -1;
        live.glitch = -1;
        say("playing");
        return;
    }
    if(tape.count <= 0)
    {
        say("nothing recorded yet: let it play a moment");
        return;
    }
    pauseAt(newest());
    say("paused: scrub with a stick (left/right) or < 0.5 s >; grip a part to edit the key shown");
}

void cycleLoop()
{
    Cvar_SetValueQuick(&vr_posestudio_live_loop, static_cast<float>((loopNow() + 1) % NumLoops));
    live.restarts++;
    if(live.paused)
    {
        togglePause();
    }
    say(va("Loop: %s (from its start)", loopNames[loopNow()]));
}

void cycleSpeed()
{
    int i = 0;
    for(int k = 0; k < numSpeeds; k++)
    {
        i = za::fabs(speeds[k] - speedNow()) < 0.01f ? k : i;
    }
    Cvar_SetValueQuick(&vr_posestudio_live_speed, speeds[(i + 1) % numSpeeds]);
    say(va("Speed: %gx", static_cast<double>(speedNow())));
}

void scrub(float seconds)
{
    if(!live.on || tape.count <= 0)
    {
        return;
    }
    if(!live.paused)
    {
        pauseAt(newest());
    }
    const double want = tape.frames[live.head].time + static_cast<double>(seconds);
    // (the frame nearest that time)
    int best = ordinal(live.head);
    double bestD = 1e30;
    for(int i = 0; i < tape.count; i++)
    {
        const double d = fabs(tape.frames[nth(i)].time - want);
        if(d < bestD)
        {
            bestD = d;
            best = i;
        }
    }
    moveHead(best);
}

void step(int frames)
{
    if(!live.on || tape.count <= 0)
    {
        return;
    }
    if(!live.paused)
    {
        pauseAt(newest());
    }
    moveHead(ordinal(live.head) + frames);
}

void nextGlitch()
{
    if(!live.on)
    {
        return;
    }
    findMoments();
    if(live.nMoments <= 0)
    {
        say(va("no glitch on the tape (%.1f s): let it play on (Loop: All)",
            tape.count > 0 ? tape.frames[newest()].time - tape.frames[oldest()].time : 0.0));
        return;
    }
    if(!live.paused)
    {
        pauseAt(newest());
        live.glitch = -1;
    }
    live.glitch = (live.glitch + 1) % live.nMoments;
    const Moment& m = live.moments[live.glitch];
    moveHead(ordinal(m.index));
    const Frame& f = tape.frames[m.index];
    float w = 0.f;
    bool mirror = false;
    const int code = dominant(w, mirror);
    say(va("glitch %d of %d: %s %.2f %s (%s), %.1f s before the pause; drag %s (%.2f)", live.glitch + 1, live.nMoments,
        agglitch::metricName(f.worst), static_cast<double>(f.m[f.worst]), agglitch::metricUnit(f.worst),
        f.side[f.worst] == 0 ? "right" : "left", tape.frames[newest()].time - f.time, code >= 0 ? keyName(code) : "no key",
        static_cast<double>(w)));
}

void describe(za::String& out, int& measureLine)
{
    out.clear();
    measureLine = -1;
    if(!live.on)
    {
        return;
    }
    const Frame* f = shownFrame();
    const double span = tape.count > 0 ? tape.frames[newest()].time - tape.frames[oldest()].time : 0.0;
    za::String line;
    const auto put = [&](const char* text) {
        line = za::String{text};
        pad(line);
        out += out.empty() ? "" : "\n";
        out += line;
    };
    put(va("LIVE  %s  %gx  %s", loopNames[loopNow()], static_cast<double>(speedNow()), live.paused ? "PAUSED" : "playing"));
    if(live.paused && f)
    {
        put(va("at %.2f s of %.1f s (frame %d)", f->time - tape.frames[newest()].time, span, ordinal(live.head) + 1));
    }
    else
    {
        put(va("recording: %.1f s on the tape", span));
    }
    if(f)
    {
        // (the keys laid on now, the most first)
        int order[maxUses];
        for(int i = 0; i < f->uses; i++)
        {
            order[i] = i;
        }
        for(int a = 1; a < f->uses; a++)
        {
            for(int b = a; b > 0 && f->use[order[b]].w > f->use[order[b - 1]].w; b--)
            {
                const int t = order[b];
                order[b] = order[b - 1];
                order[b - 1] = t;
            }
        }
        if(f->uses <= 0)
        {
            put("key: none weighs here");
        }
        for(int i = 0; i < za::min(2, f->uses); i++)
        {
            const KeyUse& u = f->use[order[i]];
            put(va("%s %s %.2f%s", i == 0 ? "key:" : "    ", keyName(u.code), static_cast<double>(u.w), u.mirror ? " (mirrored)" : ""));
        }
        const bool solved = live.paused && live.shownOk;
        const float* m = solved ? live.shown : f->m;
        measureLine = 0;
        for(const char* c = out.cStr(); *c; c++)
        {
            measureLine += *c == '\n' ? 1 : 0;
        }
        measureLine++;
        for(int i = 0; i < numShown; i++)
        {
            const int k = shownMetric[i];
            put(va("%-6s %6.2f %-5s", agglitch::metricName(k), static_cast<double>(m[k]), agglitch::metricUnit(k)));
        }
    }
    if(realtime < live.feedbackUntil)
    {
        // (the last word, cut to the lines' width)
        const char* t = live.feedback.cStr();
        while(*t)
        {
            char chunk[lineWidth + 1];
            int n = 0;
            while(t[n] && n < lineWidth)
            {
                chunk[n] = t[n];
                n++;
            }
            chunk[n] = 0;
            put(chunk);
            t += n;
        }
    }
}

float measureShare(int i, bool& bad)
{
    bad = false;
    const Frame* f = shownFrame();
    if(!f || i < 0 || i >= numShown)
    {
        return 0.f;
    }
    const int k = shownMetric[i];
    const float v = (live.paused && live.shownOk ? live.shown[k] : f->m[k]) / agglitch::metricBad(k);
    bad = v >= 1.f;
    return v;
}

int dominant(float& w, bool& mirror)
{
    w = 0.f;
    mirror = false;
    const Frame* f = shownFrame();
    int code = -1;
    for(int i = 0; f && i < f->uses; i++)
    {
        if(f->use[i].w > w)
        {
            w = f->use[i].w;
            mirror = f->use[i].mirror;
            code = f->use[i].code;
        }
    }
    return code;
}

void frame()
{
    if(!live.on)
    {
        return;
    }
    const float rdt = static_cast<float>(za::clamp(realtime - live.lastReal, 0.0, 0.1));
    live.lastReal = realtime;
    // The time scale: paused all but stopped, else the speed (1: as it was).
    const float speedSet = speedNow();
    const float want = live.paused ? pauseScale : (speedSet >= 0.999f ? live.timescaleWas : speedSet);
    if(Cvar_VariableValue("host_timescale") != want)
    {
        Cvar_SetValue("host_timescale", want);
    }
    // The subject gone (dead, removed): Live off.
    if(live.ent <= 0 || live.ent >= cl.num_entities || !cl_entities[live.ent].model)
    {
        say("its subject is gone: Live off");
        stop();
        return;
    }
    // A stick held left or right (paused): the playhead scrubbed, faster the further.
    const float x = live.stick.x;
    if(live.paused && za::fabs(x) > stickDead)
    {
        const float s = (za::fabs(x) - stickDead) / (1.f - stickDead);
        scrub((x > 0.f ? 1.f : -1.f) * s * s * stickRate * rdt);
    }
}

bool sticksTaken()
{
    return live.on && live.paused;
}

void sticks(const glm::vec2& off, const glm::vec2& main)
{
    live.stick = za::fabs(off.x) > za::fabs(main.x) ? off : main;
}

int qcLoop(int ent)
{
    return live.on && ent == live.ent ? loopNow() : -1;
}

void qcKey(int ent, float arg)
{
    if(!live.on || ent != live.ent)
    {
        return;
    }
    live.keysFrame = host_framecount;
    if(qcvm->time != live.keysTime)
    {
        live.keysTime = qcvm->time;
        live.nKeys = 0;
    }
    if(live.nKeys >= maxUses)
    {
        return;
    }
    const float a = za::fabs(arg);
    KeyUse& u = live.keys[live.nKeys++];
    u.code = static_cast<int>(floorf(a));
    u.w = a - static_cast<float>(u.code);
    u.w = u.w >= 0.998f ? 1.f : u.w;
    u.mirror = arg < 0.f;
    u.key = agpose::value(u.code);
}

bool recording(int ent)
{
    return live.on && !live.paused && !live.warming && ent == live.ent && cl.time >= live.lastRec + recordEvery - 1e-6;
}

Frame* record()
{
    Frame& f = tape.frames[tape.next];
    f.time = cl.time;
    f.armsLen = f.legsLen = f.bodyLen = 0;
    f.measured = false;
    f.sev = 0.f;
    f.worst = 0;
    // (the keys of the server's last frame: its message's)
    f.uses = live.keysFrame >= host_framecount - 30 ? live.nKeys : 0;
    for(int i = 0; i < f.uses; i++)
    {
        f.use[i] = live.keys[i];
    }
    live.open = &f;
    return &f;
}

void recorded()
{
    if(!live.open)
    {
        return;
    }
    live.lastRec = live.open->time;
    live.open = nullptr;
    tape.next = ringIndex(tape.next + 1);
    tape.count = za::min(tape.count + 1, tapeFrames);
}

bool replaying(int ent)
{
    return live.on && live.paused && ent == live.ent && live.head >= 0 && tape.count > 0;
}

bool window(int& first, int& count)
{
    if(!replaying(live.ent))
    {
        return false;
    }
    first = live.head;
    count = 1;
    const double warm = static_cast<double>(za::clamp(vr_posestudio_live_warm.value, 0.f, 2.f));
    const double t0 = tape.frames[live.head].time;
    agpose::Key d;
    // (back to the first frame within the warm-up an edit is laid on: from there again, the eases seeing it)
    for(int o = ordinal(live.head); o >= 0; o--)
    {
        const int index = nth(o);
        if(t0 - tape.frames[index].time > warm)
        {
            break;
        }
        if(delta(tape.frames[index], d))
        {
            first = index;
            count = ordinal(live.head) - o + 1;
        }
    }
    return true;
}

const Frame& at(int first, int k)
{
    return tape.frames[ringIndex(first + k)];
}

bool delta(const Frame& f, agpose::Key& out)
{
    out = agpose::Key{};
    float most = 0.f;
    for(int i = 0; i < f.uses; i++)
    {
        const KeyUse& u = f.use[i];
        const agpose::Key now = agpose::value(u.code);
        agpose::Key d;
        for(int p = 0; p < agpose::NumParts; p++)
        {
            d.v[p] = now.v[p] - u.key.v[p];
        }
        if(u.mirror)
        {
            // (as QC lays a mirrored key on: the hands swapped, places' y and turns' x and z the other way)
            const agpose::Key m = d;
            d.v[agpose::RPos] = mirrorPlace(m.v[agpose::LPos]);
            d.v[agpose::LPos] = mirrorPlace(m.v[agpose::RPos]);
            d.v[agpose::RTurn] = mirrorTurn(m.v[agpose::LTurn]);
            d.v[agpose::LTurn] = mirrorTurn(m.v[agpose::RTurn]);
            d.v[agpose::ElbowR] = mirrorPlace(m.v[agpose::ElbowL]);
            d.v[agpose::ElbowL] = mirrorPlace(m.v[agpose::ElbowR]);
            d.v[agpose::Torso] = mirrorTurn(m.v[agpose::Torso]);
        }
        for(int p = 0; p < agpose::NumParts; p++)
        {
            out.v[p] += d.v[p] * u.w;
            most = za::max(most, glm::length(d.v[p]));
        }
    }
    return most > 1e-4f;
}

void setWarming(bool w)
{
    if(w && !live.warming)
    {
        live.warmN = 0;
    }
    live.warming = w;
}

bool warming()
{
    return live.warming;
}

bool measuring(int ent)
{
    return live.on && ent == live.ent && (live.open || live.paused);
}

void sample(const agglitch::Sample& s)
{
    if(live.open)
    {
        Frame& f = *live.open;
        // (the frames before it: the tape's newest two, as it is not on the tape yet)
        const Frame* p1 = tape.count >= 1 && tape.frames[newest()].measured ? &tape.frames[newest()] : nullptr;
        const Frame* p2 = tape.count >= 2 && tape.frames[ringIndex(newest() - 1)].measured ? &tape.frames[ringIndex(newest() - 1)] : nullptr;
        const float dt = p1 ? static_cast<float>(f.time - p1->time) : 1.f / 90.f;
        agglitch::measure(s, p1 ? &p1->sample : nullptr, p1 && p2 ? &p2->sample : nullptr, dt, f.m, f.side);
        f.sample = s;
        severity(f.m, f.sev, f.worst);
        f.measured = true;
        return;
    }
    if(live.paused && live.warming)
    {
        live.warm[1] = live.warm[0];
        live.warm[0] = s;
        live.warmN++;
        return;
    }
    if(live.paused && live.head >= 0)
    {
        const Frame& f = tape.frames[live.head];
        const Frame* p1 = before(live.head, 1);
        const Frame* p2 = before(live.head, 2);
        const float dt = p1 ? static_cast<float>(f.time - p1->time) : 1.f / 90.f;
        // (the frames before it as solved again with the edits, if they were: else as recorded)
        const agglitch::Sample* s1 = live.warmN >= 1 ? &live.warm[0] : (p1 ? &p1->sample : nullptr);
        const agglitch::Sample* s2 = live.warmN >= 2 ? &live.warm[1] : (p1 && p2 ? &p2->sample : nullptr);
        live.warmN = 0;
        agglitch::measure(s, s1, s1 ? s2 : nullptr, dt, live.shown, live.shownSide);
        live.shown[toggleMetric] = f.m[toggleMetric];
        severity(live.shown, live.shownSev, live.shownWorst);
        live.shownOk = true;
    }
}

} // namespace qvr::poselive
