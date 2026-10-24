// vr_agglitch.cpp -- the armed humanoids' glitch finder (vr_agglitch.hpp; ROUND22.md, "Armed humanoids in motion: the
// glitch finder and a robust solve").

#include "vr_agglitch.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_tonemap.hpp"

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <string.h>

using namespace qvr;

namespace
{

constexpr float pi = 3.14159265f;
constexpr float refFrame = 1.f / 90.f; // the rates and jumps are given per frame at 90 Hz

enum Metric : int
{
    Flip = 0,  // the elbow's swing about its reach line (degrees a frame)
    Wrong,     // the elbow off its outward and downward way (degrees)
    Pen,       // an elbow, forearm, wrist or hand inside the torso, pelvis or head (units)
    Reach,     // a wrist brought in, short of its goal (units)
    Grip2,     // the off hand off its second grip, held (units)
    Toggle,    // the second grip taken or let go (times over the last second)
    Gap,       // a joint's gap (units)
    Aim,       // the right weapon off its wanted line (degrees)
    Jump,      // a hand's or an elbow's jump (its second difference: units at 90 Hz)
    Shoulder,  // a shoulder's jump (the body under the arms: units at 90 Hz)
    Wrist,     // a wrist bent past its most, its hand (and weapon) turned back (degrees)
    numMetrics
};
constexpr const char* metricNames[numMetrics] = {"flip", "wrong", "pen", "reach", "grip2", "toggle", "gap", "aim", "jump", "shoulder", "wrist"};
constexpr const char* metricUnits[numMetrics] = {"deg/f", "deg", "u", "u", "u", "/s", "u", "deg", "u", "u", "deg"};
// (a moment's severity: its value over this; 1 a glitch)
constexpr float metricBadValue[numMetrics] = {20.f, 135.f, 1.f, 2.f, 1.5f, 2.f, 0.05f, 10.f, 1.5f, 1.f, 10.f};
constexpr double settle = 0.4;  // seconds after a subject starts (spawned, moved) before it is measured
constexpr double merge = 0.3;   // a glitch this close after the subject's last is the same moment
constexpr int maxMoments = 8192;
constexpr int maxRows = 400000;
constexpr float camDist = 50.f;   // the eye cameras: this far from the subject,
constexpr float camHeight = 12.f; // ... this high over its origin, looking down so much (degrees)
constexpr float camPitch = 6.f;

struct Row
{
    float t{0.f};
    short subject{0}, state{0};
    float m[numMetrics]{};
    signed char side[numMetrics]{};
};

struct Moment
{
    int subject{0}, state{0};
    float t{0.f};     // since its subject's start (seconds)
    double at{0.0};   // cl.time of its peak
    double last{0.0}; // ... of its last bad frame
    float sev{0.f};
    int metric{0}, side{0};
    float value{0.f};
    int shot{-1};     // its eyeshot's number (eyeshots/<map>_<n>_L.png front, _R side), -1 none
    float shotSev{0.f};
    za::String map;
};

struct Track
{
    int ent{0};
    int n{0};          // frames of history
    double time{0.0};  // its last frame's cl.time
    agglitch::Sample prev[2]; // its last frame, the one before
    bool held2{false};
    double toggles[16]{};
    int nToggles{0};
};

struct State
{
    bool on{false};
    za::Vector<za::String> subjects, states;
    za::Vector<double> starts;  // each subject's start (cl.time)
    za::Vector<Row> rows;
    za::Vector<Moment> moments;
    int subject{-1}, state{-1};
    int open{-1};               // the moment still open (its subject's last)
    Track track;
    bool cams{false};
    za::Vector<float> scratch;
};
State state;

[[nodiscard]] int intern(za::Vector<za::String>& names, const char* s)
{
    for(za::SizeT i = 0; i < names.size(); i++)
    {
        if(!strcmp(names[i].cStr(), s))
        {
            return static_cast<int>(i);
        }
    }
    names.pushBack(za::String{s});
    return static_cast<int>(names.size()) - 1;
}

[[nodiscard]] glm::vec3 perp(const glm::vec3& v, const glm::vec3& axis)
{
    return v - axis * glm::dot(v, axis);
}

[[nodiscard]] float angleBetween(const glm::vec3& a, const glm::vec3& b)
{
    const float la = glm::length(a), lb = glm::length(b);
    if(la < 1e-5f || lb < 1e-5f)
    {
        return 0.f;
    }
    return za::acos(za::clamp(glm::dot(a, b) / (la * lb), -1.f, 1.f)) * (180.f / pi);
}

void cameras(int ent)
{
    if(ent <= 0 || ent >= cl.num_entities || vr_agglitch_shots.value <= 0.f)
    {
        if(state.cams)
        {
            setMockEyeCameras(false);
            state.cams = false;
        }
        return;
    }
    const entity_t& e = cl_entities[ent];
    const float yaw = e.angles[1] * (pi / 180.f);
    const glm::vec3 o{e.origin[0], e.origin[1], e.origin[2] + camHeight};
    const glm::vec3 fw{za::cos(yaw), za::sin(yaw), 0.f}, left{-za::sin(yaw), za::cos(yaw), 0.f};
    // (the left eye from its front, the right eye from its right side)
    const glm::vec3 at[2] = {o + fw * camDist, o - left * camDist};
    const float pitch[2] = {camPitch, camPitch};
    const float yaws[2] = {e.angles[1] + 180.f, e.angles[1] + 90.f};
    setMockEyeCameras(true, at, pitch, yaws);
    state.cams = true;
}

void closeOpen()
{
    state.open = -1;
}

// The `k`-th worst severity of `subject`'s moments but `self` (0: fewer than k).
[[nodiscard]] float kthWorst(int subject, const Moment* self, int k)
{
    state.scratch.clear();
    for(const Moment& m : state.moments)
    {
        if(m.subject == subject && &m != self)
        {
            state.scratch.pushBack(-m.sev);
        }
    }
    if(static_cast<int>(state.scratch.size()) < k)
    {
        return 0.f;
    }
    za::quickSort(state.scratch.begin(), state.scratch.end());
    return -state.scratch[static_cast<za::SizeT>(k - 1)];
}

void noteMoment(const Row& r, float sev, int metric)
{
    Moment* m = state.open >= 0 ? &state.moments[static_cast<za::SizeT>(state.open)] : nullptr;
    if(m && (m->subject != r.subject || cl.time - m->last > merge))
    {
        m = nullptr;
    }
    if(!m)
    {
        if(state.moments.size() >= static_cast<za::SizeT>(maxMoments))
        {
            return;
        }
        state.moments.pushBack(Moment{});
        state.open = static_cast<int>(state.moments.size()) - 1;
        m = &state.moments.back();
        m->subject = r.subject;
        m->map = za::String{cl.mapname};
    }
    m->last = cl.time;
    if(sev > m->sev)
    {
        m->sev = sev;
        m->state = r.state;
        m->t = r.t;
        m->at = cl.time;
        m->metric = metric;
        m->side = r.side[metric];
        m->value = r.m[metric];
    }
    // Its eye images: while it is among its subject's worst (vr_agglitch_shots), at its worst so far.
    const int k = static_cast<int>(vr_agglitch_shots.value);
    if(k <= 0 || !state.cams || vr_eyeshot.value > 0.f || sev < m->sev)
    {
        return;
    }
    const bool want = sev > kthWorst(m->subject, m, k) && (m->shot < 0 || sev > m->shotSev * 1.3f);
    if(want)
    {
        m->shot = tonemap::nextEyeshot();
        m->shotSev = sev;
        Cvar_SetQuick(&vr_eyeshot, "1");
    }
}

void start_f()
{
    state = State{};
    state.on = true;
    Con_Printf("agglitch: recording\n");
}

void stop()
{
    state.on = false;
    if(state.cams)
    {
        setMockEyeCameras(false);
        state.cams = false;
    }
}

[[nodiscard]] float percentile(za::Vector<float>& v, float share)
{
    if(v.empty())
    {
        return 0.f;
    }
    za::quickSort(v.begin(), v.end());
    return v[static_cast<za::SizeT>(share * static_cast<float>(v.size() - 1))];
}

// Each metric's worst and 95th percentile over the rows of `subject` (-1 all) in `st` (-1 all).
void stats(int subject, int st, float* worst, float* p95, int* n)
{
    *n = 0;
    for(int k = 0; k < numMetrics; k++)
    {
        state.scratch.clear();
        for(const Row& r : state.rows)
        {
            if((subject < 0 || r.subject == subject) && (st < 0 || r.state == st))
            {
                state.scratch.pushBack(r.m[k]);
            }
        }
        *n = static_cast<int>(state.scratch.size());
        worst[k] = percentile(state.scratch, 1.f);
        p95[k] = percentile(state.scratch, 0.95f);
    }
}

void report_f()
{
    const bool was = state.on;
    stop();
    const char* label = Cmd_Argc() >= 2 ? Cmd_Argv(1) : "agglitch";
    const za::String dir = za::String{com_gamedir} + "/agglitch";
    files::createDirectories(dir.cStr());
    // The summary: each subject's (and each of its states') worst and 95th percentile of each metric.
    za::String csv = "subject,state,frames";
    for(int k = 0; k < numMetrics; k++)
    {
        csv += va(",%s_worst,%s_p95", metricNames[k], metricNames[k]);
    }
    csv += "\n";
    float worst[numMetrics], p95[numMetrics];
    int n = 0;
    Con_Printf("agglitch: %d frames, %d subjects, %d moments over 1 (recording %s); worst/p95 per subject:\n",
        static_cast<int>(state.rows.size()), static_cast<int>(state.subjects.size()), static_cast<int>(state.moments.size()),
        was ? "stopped" : "was off");
    for(int s = -1; s < static_cast<int>(state.subjects.size()); s++)
    {
        for(int st = -1; st < static_cast<int>(state.states.size()); st++)
        {
            stats(s, st, worst, p95, &n);
            if(n == 0)
            {
                continue;
            }
            csv += va("%s,%s,%d", s < 0 ? "all" : state.subjects[static_cast<za::SizeT>(s)].cStr(),
                st < 0 ? "all" : state.states[static_cast<za::SizeT>(st)].cStr(), n);
            for(int k = 0; k < numMetrics; k++)
            {
                csv += va(",%.2f,%.2f", worst[k], p95[k]);
            }
            csv += "\n";
            if(st < 0)
            {
                za::String line = va("agg %s:", s < 0 ? "ALL" : state.subjects[static_cast<za::SizeT>(s)].cStr());
                for(int k = 0; k < numMetrics; k++)
                {
                    line += va(" %s %.1f/%.1f", metricNames[k], worst[k], p95[k]);
                }
                Con_Printf("%s\n", line.cStr());
            }
        }
    }
    (void)files::writeText((dir + "/" + label + "_summary.csv").cStr(), csv.toStringView());
    // The worst moments.
    za::Vector<int> order;
    for(za::SizeT i = 0; i < state.moments.size(); i++)
    {
        order.pushBack(static_cast<int>(i));
    }
    za::quickSort(order.begin(), order.end(), [](int a, int b) {
        return state.moments[static_cast<za::SizeT>(a)].sev > state.moments[static_cast<za::SizeT>(b)].sev;
    });
    za::String top = "rank,severity,subject,state,t,metric,value,unit,side,front,side_image\n";
    const int most = za::max(1, static_cast<int>(vr_agglitch_top.value));
    int printed = 0;
    for(int i = 0; i < static_cast<int>(order.size()); i++)
    {
        const Moment& m = state.moments[static_cast<za::SizeT>(order[static_cast<za::SizeT>(i)])];
        const char* subj = state.subjects[static_cast<za::SizeT>(m.subject)].cStr();
        const char* st = m.state >= 0 && m.state < static_cast<int>(state.states.size()) ? state.states[static_cast<za::SizeT>(m.state)].cStr() : "?";
        const za::String front = m.shot >= 0 ? za::String{va("eyeshots/%s_%03d_L.png", m.map.cStr(), m.shot)} : za::String{"-"};
        const za::String side = m.shot >= 0 ? za::String{va("eyeshots/%s_%03d_R.png", m.map.cStr(), m.shot)} : za::String{"-"};
        top += va("%d,%.2f,%s,%s,%.2f,%s,%.2f,%s,%s,%s,%s\n", i + 1, m.sev, subj, st, m.t, metricNames[m.metric], m.value,
            metricUnits[m.metric], m.side == 0 ? "right" : "left", front.cStr(), side.cStr());
        // (the console's list: the worst moment of each subject and metric)
        bool again = false;
        for(int j = 0; j < i && !again; j++)
        {
            const Moment& o = state.moments[static_cast<za::SizeT>(order[static_cast<za::SizeT>(j)])];
            again = o.subject == m.subject && o.metric == m.metric;
        }
        if(!again && printed++ < most)
        {
            Con_Printf("aggtop %d sev %.2f %s | %s t %.2f: %s %.2f %s (%s) shot %d\n", i + 1, m.sev, subj, st, m.t,
                metricNames[m.metric], m.value, metricUnits[m.metric], m.side == 0 ? "right" : "left", m.shot);
        }
    }
    (void)files::writeText((dir + "/" + label + "_top.csv").cStr(), top.toStringView());
    // Every frame (the Pose Studio's jump to a glitch, later; plots).
    za::String rows = "t,subject,state";
    for(int k = 0; k < numMetrics; k++)
    {
        rows += va(",%s", metricNames[k]);
    }
    rows += "\n";
    for(const Row& r : state.rows)
    {
        rows += va("%.3f,%s,%s", r.t, state.subjects[static_cast<za::SizeT>(r.subject)].cStr(), state.states[static_cast<za::SizeT>(r.state)].cStr());
        for(int k = 0; k < numMetrics; k++)
        {
            rows += va(",%.2f", r.m[k]);
        }
        rows += "\n";
    }
    (void)files::writeText((dir + "/" + label + "_frames.csv").cStr(), rows.toStringView());
    Con_Printf("agglitch: written %s/%s_summary.csv, _top.csv, _frames.csv\n", dir.cStr(), label);
}

void stop_f()
{
    stop();
    Con_Printf("agglitch: stopped (%d frames)\n", static_cast<int>(state.rows.size()));
}

} // namespace

namespace qvr::agglitch
{

bool watching(int ent)
{
    const int want = static_cast<int>(vr_agglitch_ent.value);
    if(state.cams && (want <= 0 || !state.on))
    {
        cameras(0);
    }
    if(!state.on && want > 0)
    {
        start_f(); // (a sweep's first subject named: recording afresh until vr_agglitch_report)
    }
    return state.on && ent > 0 && ent == want;
}

void note(const Sample& s)
{
    if(!state.on)
    {
        return;
    }
    cameras(s.ent);
    const int subject = intern(state.subjects, vr_agglitch_subject.string ? vr_agglitch_subject.string : "");
    const int st = intern(state.states, vr_agglitch_state.string ? vr_agglitch_state.string : "");
    while(state.starts.size() < state.subjects.size())
    {
        state.starts.pushBack(cl.time);
    }
    Track& tr = state.track;
    if(subject != state.subject || tr.ent != s.ent)
    {
        state.subject = subject;
        state.starts[static_cast<za::SizeT>(subject)] = cl.time;
        tr = Track{};
        tr.ent = s.ent;
        closeOpen();
    }
    state.state = st;
    const double dtd = cl.time - tr.time;
    if(tr.n > 0 && dtd <= 0.0)
    {
        return; // (the same instant: drawn twice)
    }
    const float dt = static_cast<float>(za::clamp(dtd, 1e-4, 0.25));
    Row r;
    r.t = static_cast<float>(cl.time - state.starts[static_cast<za::SizeT>(subject)]);
    r.subject = static_cast<short>(subject);
    r.state = static_cast<short>(st);
    measure(s, tr.n >= 1 ? &tr.prev[0] : nullptr, tr.n >= 2 ? &tr.prev[1] : nullptr, dt, r.m, r.side);
    const auto put = [&](int k, float v, int side) {
        if(v > r.m[k])
        {
            r.m[k] = v;
            r.side[k] = static_cast<signed char>(side);
        }
    };
    if(tr.n >= 1 && s.held2 != tr.held2 && (s.want2 || tr.held2))
    {
        tr.toggles[tr.nToggles % 16] = cl.time;
        tr.nToggles++;
    }
    int recent = 0;
    for(int i = 0; i < za::min(tr.nToggles, 16); i++)
    {
        recent += cl.time - tr.toggles[i] <= 1.0 ? 1 : 0;
    }
    put(Toggle, static_cast<float>(recent), 1);
    // History.
    tr.prev[1] = tr.prev[0];
    tr.prev[0] = s;
    tr.held2 = s.held2;
    tr.time = cl.time;
    tr.n++;
    if(cl.time - state.starts[static_cast<za::SizeT>(subject)] < settle || tr.n < 3)
    {
        return;
    }
    if(state.rows.size() < static_cast<za::SizeT>(maxRows))
    {
        state.rows.pushBack(r);
    }
    float sev = 0.f;
    int worst = 0;
    const char* focus = vr_agglitch_metric.string ? vr_agglitch_metric.string : "";
    for(int k = 0; k < numMetrics; k++)
    {
        const float v = r.m[k] / metricBadValue[k];
        if(v > sev && (!focus[0] || strstr(focus, metricNames[k])))
        {
            sev = v;
            worst = k;
        }
    }
    if(sev >= 1.f)
    {
        noteMoment(r, sev, worst);
    }
}

const char* metricName(int k)
{
    return k >= 0 && k < numMetrics ? metricNames[k] : "?";
}

const char* metricUnit(int k)
{
    return k >= 0 && k < numMetrics ? metricUnits[k] : "";
}

float metricBad(int k)
{
    return k >= 0 && k < numMetrics ? metricBadValue[k] : 1.f;
}

void measure(const Sample& s, const Sample* p1, const Sample* p2, float dt, float* out, signed char* sideOut)
{
    const float rate = refFrame / za::max(dt, 1e-4f);
    for(int k = 0; k < numMetrics; k++)
    {
        out[k] = 0.f;
        sideOut[k] = 0;
    }
    const auto put = [&](int k, float v, int side) {
        if(v > out[k])
        {
            out[k] = v;
            sideOut[k] = static_cast<signed char>(side);
        }
    };
    for(int side = 0; side < 2; side++)
    {
        const Arm& a = s.arm[side];
        const glm::vec3 dir = glm::normalize(a.W - a.S + glm::vec3{0.f, 0.f, 1e-5f});
        const glm::vec3 u = perp(a.E - a.S, dir);
        const float lu = glm::length(a.E - a.S);
        if(p1)
        {
            const glm::vec3 u0 = perp(p1->arm[side].E - p1->arm[side].S, dir);
            if(glm::length(u) > 0.25f * lu && glm::length(u0) > 0.25f * lu)
            {
                put(Flip, angleBetween(u, u0) * rate, side);
            }
        }
        const glm::vec3 pp = perp(a.pole, dir);
        if(glm::length(u) > 0.25f * lu && glm::length(pp) > 0.2f)
        {
            put(Wrong, angleBetween(u, pp), side);
        }
        put(Pen, za::max(0.f, a.pen), side);
        put(Reach, a.miss, side);
        put(Wrist, a.turned, side);
        if(p1 && p2)
        {
            const float k2 = rate * rate;
            const Arm& b = p1->arm[side];
            const Arm& c = p2->arm[side];
            put(Jump, glm::length(a.grip - 2.f * b.grip + c.grip) * k2, side);
            put(Jump, glm::length(a.E - 2.f * b.E + c.E) * k2, side);
            put(Shoulder, glm::length(a.S - 2.f * b.S + c.S) * k2, side);
        }
    }
    if(s.held2)
    {
        put(Grip2, s.second, 1);
    }
    put(Gap, s.gap, 0);
    put(Aim, s.aim, 0);
}

void registerCommands()
{
    Cmd_AddCommand("vr_agglitch_start", start_f);
    Cmd_AddCommand("vr_agglitch_stop", stop_f);
    Cmd_AddCommand("vr_agglitch_report", report_f);
}

} // namespace qvr::agglitch
