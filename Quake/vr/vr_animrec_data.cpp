// vr_animrec_data.cpp -- the animation recorder's data (vr_animrec_internal.hpp): the .anim file, the spline between
// frames, the smoothing. Misc/quakevr/anim_smooth.py is the same smoothing offline: keep the two alike.

#include "vr_animrec_internal.hpp"

#include "vr_files.hpp"
#include "vr_hands.hpp"
#include "vr_strings.hpp"

#include "Zancle/Container/Array.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/StringView.hpp"

#include <math.h>
#include <stdio.h>
#include <string.h>


namespace qvr::animrec
{

const WeaponChange* Anim::weaponAt(int hand, double t) const
{
    const WeaponChange* found = nullptr;
    for(const WeaponChange& w : weapons)
    {
        if(w.hand == hand && w.t <= t + 1e-6)
        {
            found = &w;
        }
    }
    return found;
}

glm::quat quatOf(const glm::vec3& angles)
{
    return glm::normalize(glm::quat_cast(hands::basisOf(angles)));
}

glm::vec3 anglesOf(const glm::quat& q)
{
    const glm::mat3 m = glm::mat3_cast(glm::normalize(q));
    return hands::anglesFromVectors(m[0], m[2]);
}

namespace
{

// ----------------------------------------------------------------------------
// A frame as channels: the spline and the smoothing work on these
// ----------------------------------------------------------------------------

constexpr int handChans = 21; // head pos 3, head quat 4, body pos 3, body quat 4, curls 5, grip, trigger
constexpr int chanCount = 8 + handCount * handChans; // head pos 3, head quat 4, crouch, the hands
using Chans = za::Array<float, chanCount>;

constexpr int handBase(int h)
{
    return 8 + h * handChans;
}

// The channel groups the tolerances measure: positions (3), rotations (4), scalars (1).
enum class Group
{
    Pos,
    Rot,
    Scalar
};
struct GroupAt
{
    int at;
    Group group;
    int size;
};
constexpr GroupAt groups[] = {
    {0, Group::Pos, 3},
    {3, Group::Rot, 4},
    {7, Group::Scalar, 1},
    {handBase(0) + 0, Group::Pos, 3},
    {handBase(0) + 3, Group::Rot, 4},
    {handBase(0) + 7, Group::Pos, 3},
    {handBase(0) + 10, Group::Rot, 4},
    {handBase(0) + 14, Group::Scalar, 7},
    {handBase(1) + 0, Group::Pos, 3},
    {handBase(1) + 3, Group::Rot, 4},
    {handBase(1) + 7, Group::Pos, 3},
    {handBase(1) + 10, Group::Rot, 4},
    {handBase(1) + 14, Group::Scalar, 7},
};

void putPose(Chans& c, int at, const Pose& p)
{
    c[at + 0] = p.pos.x;
    c[at + 1] = p.pos.y;
    c[at + 2] = p.pos.z;
    c[at + 3] = p.rot.w;
    c[at + 4] = p.rot.x;
    c[at + 5] = p.rot.y;
    c[at + 6] = p.rot.z;
}

[[nodiscard]] Pose getPose(const Chans& c, int at)
{
    Pose p;
    p.pos = {c[at + 0], c[at + 1], c[at + 2]};
    p.rot = glm::normalize(glm::quat{c[at + 3], c[at + 4], c[at + 5], c[at + 6]});
    return p;
}

[[nodiscard]] Chans toChans(const Frame& f)
{
    Chans c{};
    putPose(c, 0, f.head);
    c[7] = f.crouch;
    for(int h = 0; h < handCount; h++)
    {
        const HandFrame& hf = f.hands[h];
        const int b = handBase(h);
        putPose(c, b + 0, hf.head);
        putPose(c, b + 7, hf.body);
        for(int k = 0; k < fingerCount; k++)
        {
            c[b + 14 + k] = hf.curl[k];
        }
        c[b + 19] = hf.grip;
        c[b + 20] = hf.trigger;
    }
    return c;
}

// The channels back into a frame; the weapons and the time from `like`.
[[nodiscard]] Frame fromChans(const Chans& c, const Frame& like, double t)
{
    Frame f = like;
    f.t = t;
    f.head = getPose(c, 0);
    f.crouch = c[7];
    for(int h = 0; h < handCount; h++)
    {
        HandFrame& hf = f.hands[h];
        const int b = handBase(h);
        hf.head = getPose(c, b + 0);
        hf.body = getPose(c, b + 7);
        for(int k = 0; k < fingerCount; k++)
        {
            hf.curl[k] = c[b + 14 + k];
        }
        hf.grip = c[b + 19];
        hf.trigger = c[b + 20];
    }
    return f;
}

// Each rotation's quaternion on the side of the frame before's (q and -q are the same turn: the spline and the filter
// would go the long way round between them).
void keepQuatSides(za::Vector<Chans>& x)
{
    for(za::SizeT i = 1; i < x.size(); i++)
    {
        for(const GroupAt& g : groups)
        {
            if(g.group != Group::Rot)
            {
                continue;
            }
            float dot = 0.f;
            for(int k = 0; k < 4; k++)
            {
                dot += x[i][g.at + k] * x[i - 1][g.at + k];
            }
            if(dot < 0.f)
            {
                for(int k = 0; k < 4; k++)
                {
                    x[i][g.at + k] = -x[i][g.at + k];
                }
            }
        }
    }
}

void normaliseQuats(Chans& c)
{
    for(const GroupAt& g : groups)
    {
        if(g.group != Group::Rot)
        {
            continue;
        }
        float len = 0.f;
        for(int k = 0; k < 4; k++)
        {
            len += c[g.at + k] * c[g.at + k];
        }
        len = sqrtf(len);
        if(len > 1e-6f)
        {
            for(int k = 0; k < 4; k++)
            {
                c[g.at + k] /= len;
            }
        }
    }
}

// The cubic Hermite spline between keys `a` and `b` (indices into x/t) at `time`, with Catmull-Rom tangents from the
// keys before and after (`prev`, `next`: -1 at an end, the tangent one-sided there).
[[nodiscard]] Chans hermite(const za::Vector<Chans>& x, const za::Vector<double>& t, int prev, int a, int b, int next,
    double time)
{
    const double h = t[b] - t[a];
    Chans out{};
    if(h <= 1e-9)
    {
        return x[a];
    }
    const double s = za::clamp((time - t[a]) / h, 0.0, 1.0);
    const double s2 = s * s;
    const double s3 = s2 * s;
    const double h00 = 2.0 * s3 - 3.0 * s2 + 1.0;
    const double h10 = s3 - 2.0 * s2 + s;
    const double h01 = -2.0 * s3 + 3.0 * s2;
    const double h11 = s3 - s2;
    const int pa = prev >= 0 ? prev : a;
    const int nb = next >= 0 ? next : b;
    const double da = t[b] - t[pa];
    const double db = t[nb] - t[a];
    for(int c = 0; c < chanCount; c++)
    {
        const double ma = da > 1e-9 ? (x[b][c] - x[pa][c]) / da : 0.0;
        const double mb = db > 1e-9 ? (x[nb][c] - x[a][c]) / db : 0.0;
        out[c] = static_cast<float>(h00 * x[a][c] + h10 * h * ma + h01 * x[b][c] + h11 * h * mb);
    }
    normaliseQuats(out);
    return out;
}

[[nodiscard]] float quatAngleDegrees(const Chans& a, const Chans& b, int at)
{
    float dot = 0.f;
    for(int k = 0; k < 4; k++)
    {
        dot += a[at + k] * b[at + k];
    }
    dot = za::min(fabsf(dot), 1.f);
    return 2.f * acosf(dot) * 57.29578f;
}

[[nodiscard]] float distanceBetween(const Chans& a, const Chans& b, int at)
{
    const float dx = a[at] - b[at], dy = a[at + 1] - b[at + 1], dz = a[at + 2] - b[at + 2];
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

// The errors of `got` against `want`, per group kind: the largest position (m), angle (degrees) and scalar.
struct Errors
{
    float pos{0.f};
    float ang{0.f};
    float scalar{0.f};
};

[[nodiscard]] Errors errorsOf(const Chans& got, const Chans& want)
{
    Errors e;
    for(const GroupAt& g : groups)
    {
        switch(g.group)
        {
            case Group::Pos: e.pos = za::max(e.pos, distanceBetween(got, want, g.at)); break;
            case Group::Rot: e.ang = za::max(e.ang, quatAngleDegrees(got, want, g.at)); break;
            case Group::Scalar:
                for(int k = 0; k < g.size; k++)
                {
                    e.scalar = za::max(e.scalar, fabsf(got[g.at + k] - want[g.at + k]));
                }
                break;
        }
    }
    return e;
}

[[nodiscard]] float normalisedError(const Errors& e, const SmoothParams& p)
{
    return za::max(e.pos / za::max(p.posTol, 1e-6f), e.ang / za::max(p.angTol, 1e-3f), e.scalar / za::max(p.curlTol, 1e-4f));
}

// ----------------------------------------------------------------------------
// The file
// ----------------------------------------------------------------------------

constexpr const char* handNames[handCount] = {"off", "main"};

[[nodiscard]] const char* handWord(int h)
{
    return h == 0 ? "off" : h == 1 ? "main" : "both";
}

[[nodiscard]] int handOfWord(za::StringView w)
{
    return w == "off" ? 0 : w == "main" ? 1 : -1;
}

void appendPose(za::String& out, const Pose& p)
{
    const glm::vec3 a = anglesOf(p.rot);
    strings::appendf(out, " %.5f %.5f %.5f %.3f %.3f %.3f", p.pos.x, p.pos.y, p.pos.z, a.x, a.y, a.z);
}

[[nodiscard]] za::String oneWord(const za::String& s)
{
    if(s.empty())
    {
        return "-";
    }
    za::String out = s;
    for(za::SizeT i = 0; i < out.size(); i++)
    {
        if(out[i] == ' ' || out[i] == '\t' || out[i] == '\n' || out[i] == '\r')
        {
            out[i] = '_';
        }
    }
    return out;
}

[[nodiscard]] za::String oneLine(const za::String& s)
{
    za::String out = s;
    for(za::SizeT i = 0; i < out.size(); i++)
    {
        if(out[i] == '\n' || out[i] == '\r')
        {
            out[i] = ' ';
        }
    }
    return out;
}

// The rest of a line after `skip` words.
[[nodiscard]] za::String restAfter(za::StringView line, int skip)
{
    za::SizeT i = 0;
    for(int w = 0; w < skip; w++)
    {
        while(i < line.size() && (line[i] == ' ' || line[i] == '\t'))
        {
            i++;
        }
        while(i < line.size() && line[i] != ' ' && line[i] != '\t')
        {
            i++;
        }
    }
    while(i < line.size() && (line[i] == ' ' || line[i] == '\t'))
    {
        i++;
    }
    za::SizeT end = line.size();
    while(end > i && (line[end - 1] == '\r' || line[end - 1] == ' '))
    {
        end--;
    }
    return za::String{line.substrByPosLen(i, end - i)};
}

bool readPose(files::Words& w, Pose& p)
{
    glm::vec3 a{0.f};
    w >> p.pos.x >> p.pos.y >> p.pos.z >> a.x >> a.y >> a.z;
    p.rot = quatOf(a);
    return static_cast<bool>(w);
}

} // namespace

za::String toText(const Anim& a)
{
    za::String out;
    out += "# Quake VR animation recording (vr_animrec.cpp; the format and its use: docs/vr-port/ANIMATIONS.md).\n";
    out += "# Units: metres, degrees, seconds. Axes: forward, left, up. Angles: Quake's pitch (down +), yaw (left +), roll.\n";
    out += "# A hand's \"head\" pose is relative to the eye (its whole turn), its \"body\" pose relative to the body anchor\n";
    out += "# (under the head, lowered with the crouch; turned by the body's yaw alone). \"head_*\": the head in the body's\n";
    out += "# frame. curl: thumb index middle ring little (0 open, 1 curled); grip, trigger: the controller's (0..1);\n";
    out += "# wid: the held weapon's id (QC WID_*; 0 none). weapon lines: a hand's weapon from that time. event lines:\n";
    out += "# t hand kind spot dx dy dz bx by bz detail (spot: the recorder's hotspot the hand was nearest, \"-\" none;\n";
    out += "# d: the hand from it; b: the hand on the body). spot lines: the recorder's hotspots on his body.\n";
    strings::appendf(out, "anim %d\n", a.version);
    out += "name " + oneWord(a.name) + "\n";
    out += "date " + oneLine(a.date) + "\n";
    out += "map " + oneWord(a.map) + "\n";
    out += "source " + oneLine(a.source) + "\n";
    out += "note " + oneLine(a.note) + "\n";
    strings::appendf(out, "frames %d %d\n", static_cast<int>(a.frames.size()), a.rawFrames > 0 ? a.rawFrames : static_cast<int>(a.frames.size()));
    strings::appendf(out, "duration %.4f\n", a.duration());
    out += a.smoothed ? "smoothed 1 " + oneLine(a.smoothInfo) + "\n" : za::String{"smoothed 0\n"};
    for(const Spot& s : a.spots)
    {
        strings::appendf(out, "spot %s %.4f %.4f %.4f\n", oneWord(s.name).cStr(), s.pos.x, s.pos.y, s.pos.z);
    }
    for(const WeaponChange& w : a.weapons)
    {
        strings::appendf(out, "weapon %.4f %s %d %s\n", w.t, handWord(w.hand), w.wid, oneWord(w.model).cStr());
    }
    for(const Event& e : a.events)
    {
        strings::appendf(out, "event %.4f %s %s %s %.4f %.4f %.4f %.4f %.4f %.4f %s\n", e.t, handWord(e.hand), oneWord(e.kind).cStr(),
            oneWord(e.spot).cStr(), e.fromSpot.x, e.fromSpot.y, e.fromSpot.z, e.body.x, e.body.y, e.body.z,
            oneLine(e.detail).cStr());
    }
    out += "columns t head_x head_y head_z head_pitch head_yaw head_roll crouch";
    for(const int h : {1, 0})
    {
        const char* n = handNames[h];
        for(const char* part : {"head", "body"})
        {
            strings::appendf(out, " %s_%s_x %s_%s_y %s_%s_z %s_%s_pitch %s_%s_yaw %s_%s_roll", n, part, n, part, n, part, n, part, n,
                part, n, part);
        }
        strings::appendf(out, " %s_curl_thumb %s_curl_index %s_curl_middle %s_curl_ring %s_curl_little %s_grip %s_trigger %s_wid", n,
            n, n, n, n, n, n, n);
    }
    out += "\n";
    for(const Frame& f : a.frames)
    {
        strings::appendf(out, "f %.4f", f.t);
        appendPose(out, f.head);
        strings::appendf(out, " %.3f", f.crouch);
        for(const int h : {1, 0})
        {
            const HandFrame& hf = f.hands[h];
            appendPose(out, hf.head);
            appendPose(out, hf.body);
            for(int k = 0; k < fingerCount; k++)
            {
                strings::appendf(out, " %.3f", hf.curl[k]);
            }
            strings::appendf(out, " %.3f %.3f %d", hf.grip, hf.trigger, hf.wid);
        }
        out += "\n";
    }
    return out;
}

bool fromText(za::StringView text, Anim& a, za::String& error)
{
    a = Anim{};
    int lineNo = 0;
    bool ok = true;
    bool sawHeader = false;
    files::forLines(text, [&](za::StringView line) {
        lineNo++;
        if(!ok || line.empty() || line[0] == '#')
        {
            return;
        }
        files::Words w{line};
        za::String key;
        if(!w.next(key))
        {
            return;
        }
        const auto fail = [&](const char* what) {
            ok = false;
            char buf[160];
            snprintf(buf, sizeof(buf), "line %d: %s", lineNo, what);
            error = buf;
        };
        if(key == "anim")
        {
            sawHeader = w.next(a.version);
        }
        else if(key == "name")
        {
            a.name = restAfter(line, 1);
        }
        else if(key == "date")
        {
            a.date = restAfter(line, 1);
        }
        else if(key == "map")
        {
            a.map = restAfter(line, 1);
        }
        else if(key == "source")
        {
            a.source = restAfter(line, 1);
        }
        else if(key == "note")
        {
            a.note = restAfter(line, 1);
        }
        else if(key == "frames")
        {
            int n = 0;
            w >> n >> a.rawFrames;
        }
        else if(key == "smoothed")
        {
            int s = 0;
            w >> s;
            a.smoothed = s != 0;
            a.smoothInfo = restAfter(line, 2);
        }
        else if(key == "spot")
        {
            Spot s;
            w >> s.name >> s.pos.x >> s.pos.y >> s.pos.z;
            if(!w)
            {
                return fail("a bad spot line");
            }
            a.spots.pushBack(ZA_MOVE(s));
        }
        else if(key == "weapon")
        {
            WeaponChange c;
            za::String hand;
            w >> c.t >> hand >> c.wid >> c.model;
            if(!w)
            {
                return fail("a bad weapon line");
            }
            c.hand = handOfWord(hand);
            if(c.model == "-")
            {
                c.model.clear();
            }
            a.weapons.pushBack(ZA_MOVE(c));
        }
        else if(key == "event")
        {
            Event e;
            za::String hand;
            w >> e.t >> hand >> e.kind >> e.spot >> e.fromSpot.x >> e.fromSpot.y >> e.fromSpot.z >> e.body.x >> e.body.y >>
                e.body.z;
            if(!w)
            {
                return fail("a bad event line");
            }
            e.hand = handOfWord(hand);
            e.detail = restAfter(line, 11);
            a.events.pushBack(ZA_MOVE(e));
        }
        else if(key == "f")
        {
            Frame f;
            w >> f.t;
            readPose(w, f.head);
            w >> f.crouch;
            for(const int h : {1, 0})
            {
                HandFrame& hf = f.hands[h];
                readPose(w, hf.head);
                readPose(w, hf.body);
                for(int k = 0; k < fingerCount; k++)
                {
                    w >> hf.curl[k];
                }
                w >> hf.grip >> hf.trigger >> hf.wid;
            }
            if(!w)
            {
                return fail("a frame with too few numbers");
            }
            a.frames.pushBack(f);
        }
        // (columns, duration and unknown keys: skipped; a later version may add lines)
    });
    if(ok && !sawHeader)
    {
        error = "no \"anim\" line: not an animation recording";
        return false;
    }
    if(ok && a.frames.empty())
    {
        error = "no frames";
        return false;
    }
    if(a.rawFrames <= 0)
    {
        a.rawFrames = static_cast<int>(a.frames.size());
    }
    return ok;
}

Frame sample(const Anim& a, double t)
{
    const za::SizeT n = a.frames.size();
    if(n == 0)
    {
        return Frame{};
    }
    if(n == 1 || t <= a.frames.front().t)
    {
        return a.frames.front();
    }
    if(t >= a.frames.back().t)
    {
        return a.frames.back();
    }
    // The frames around t (binary search).
    za::SizeT lo = 0, hi = n - 1;
    while(hi - lo > 1)
    {
        const za::SizeT mid = (lo + hi) / 2;
        (a.frames[mid].t <= t ? lo : hi) = mid;
    }
    // Four frames' channels, the rotations on one side.
    const int ia = static_cast<int>(lo), ib = static_cast<int>(hi);
    const int ip = ia > 0 ? ia - 1 : -1;
    const int in = ib + 1 < static_cast<int>(n) ? ib + 1 : -1;
    za::Vector<Chans> x;
    za::Vector<double> ts;
    for(const int i : {ip >= 0 ? ip : ia, ia, ib, in >= 0 ? in : ib})
    {
        x.pushBack(toChans(a.frames[i]));
        ts.pushBack(a.frames[i].t);
    }
    keepQuatSides(x);
    const Chans c = hermite(x, ts, ip >= 0 ? 0 : -1, 1, 2, in >= 0 ? 3 : -1, t);
    return fromChans(c, a.frames[lo], t);
}

SmoothResult smooth(const Anim& in, const SmoothParams& p, Anim& out)
{
    SmoothResult r;
    out = in;
    out.frames.clear();
    const za::SizeT n = in.frames.size();
    r.before = static_cast<int>(n);
    if(n < 3)
    {
        out.frames = in.frames;
        r.after = static_cast<int>(n);
        return r;
    }

    za::Vector<Chans> raw;
    za::Vector<double> t;
    for(const Frame& f : in.frames)
    {
        raw.pushBack(toChans(f));
        t.pushBack(f.t);
    }
    keepQuatSides(raw);

    // 1. The jitter filter: an exponential smoothing forwards then backwards (no lag), time constant tau.
    za::Vector<Chans> x = raw;
    if(p.tau > 0.f)
    {
        for(za::SizeT i = 1; i < n; i++)
        {
            const float k = 1.f - expf(-static_cast<float>(t[i] - t[i - 1]) / p.tau);
            for(int c = 0; c < chanCount; c++)
            {
                x[i][c] = x[i - 1][c] + k * (x[i][c] - x[i - 1][c]);
            }
        }
        for(za::SizeT i = n - 1; i-- > 0;)
        {
            const float k = 1.f - expf(-static_cast<float>(t[i + 1] - t[i]) / p.tau);
            for(int c = 0; c < chanCount; c++)
            {
                x[i][c] = x[i + 1][c] + k * (x[i][c] - x[i + 1][c]);
            }
        }
        for(Chans& c : x)
        {
            normaliseQuats(c);
        }
    }
    for(za::SizeT i = 0; i < n; i++)
    {
        r.rawPos = za::max(r.rawPos, errorsOf(x[i], raw[i]).pos);
    }

    // 2. The keys that must stay: the ends, each event's frame, each weapon change (the frames either side).
    za::Vector<char> must(n, char{0});
    must[0] = must[n - 1] = 1;
    for(const Event& e : in.events)
    {
        za::SizeT best = 0;
        for(za::SizeT i = 1; i < n; i++)
        {
            if(fabs(t[i] - e.t) < fabs(t[best] - e.t))
            {
                best = i;
            }
        }
        must[best] = 1;
    }
    for(za::SizeT i = 1; i < n; i++)
    {
        for(int h = 0; h < handCount; h++)
        {
            if(in.frames[i].hands[h].wid != in.frames[i - 1].hands[h].wid)
            {
                must[i - 1] = must[i] = 1;
            }
        }
    }
    za::Vector<int> keys;
    for(za::SizeT i = 0; i < n; i++)
    {
        if(must[i])
        {
            keys.pushBack(static_cast<int>(i));
        }
    }

    // 3. Keys added where the spline through them strays most, until it strays no more than the tolerances anywhere.
    // segErr[j] / segAt[j]: the worst frame strictly between keys j and j+1 (its normalised error, its index).
    za::Vector<float> segErr;
    za::Vector<int> segAt;
    const auto measure = [&](int j) {
        const int kp = j > 0 ? keys[j - 1] : -1;
        const int ka = keys[j], kb = keys[j + 1];
        const int kn = j + 2 < static_cast<int>(keys.size()) ? keys[j + 2] : -1;
        float worst = 0.f;
        int at = -1;
        for(int i = ka + 1; i < kb; i++)
        {
            const float e = normalisedError(errorsOf(hermite(x, t, kp, ka, kb, kn, t[i]), x[i]), p);
            if(e > worst)
            {
                worst = e;
                at = i;
            }
        }
        segErr[j] = worst;
        segAt[j] = at;
    };
    segErr.resize(keys.size() - 1, 0.f);
    segAt.resize(keys.size() - 1, -1);
    for(int j = 0; j + 1 < static_cast<int>(keys.size()); j++)
    {
        measure(j);
    }
    for(za::SizeT guard = 0; guard < n; guard++)
    {
        int worstSeg = -1;
        float worst = 1.f;
        for(int j = 0; j < static_cast<int>(segErr.size()); j++)
        {
            if(segErr[j] > worst && segAt[j] >= 0)
            {
                worst = segErr[j];
                worstSeg = j;
            }
        }
        if(worstSeg < 0)
        {
            break;
        }
        const int at = segAt[worstSeg];
        keys.insert(keys.begin() + worstSeg + 1, at);
        segErr.insert(segErr.begin() + worstSeg + 1, 0.f);
        segAt.insert(segAt.begin() + worstSeg + 1, -1);
        // The segments whose spline the new key changes: its own two, and the two either side (their tangents).
        for(int j = za::max(0, worstSeg - 2); j <= za::min(static_cast<int>(segErr.size()) - 1, worstSeg + 3); j++)
        {
            measure(j);
        }
    }

    // 4. The kept keys, and the errors of their spline at every frame.
    for(const int k : keys)
    {
        out.frames.pushBack(fromChans(x[k], in.frames[k], t[k]));
    }
    for(za::SizeT j = 0; j + 1 < keys.size(); j++)
    {
        const int kp = j > 0 ? keys[j - 1] : -1;
        const int kn = j + 2 < keys.size() ? keys[j + 2] : -1;
        for(int i = keys[j] + 1; i < keys[j + 1]; i++)
        {
            const Errors e = errorsOf(hermite(x, t, kp, keys[j], keys[j + 1], kn, t[i]), x[i]);
            r.maxPos = za::max(r.maxPos, e.pos);
            r.maxAng = za::max(r.maxAng, e.ang);
            r.maxCurl = za::max(r.maxCurl, e.scalar);
        }
    }
    r.after = static_cast<int>(out.frames.size());
    out.smoothed = true;
    out.rawFrames = in.rawFrames > 0 ? in.rawFrames : static_cast<int>(n);
    char info[256];
    snprintf(info, sizeof(info),
        "keys %d of %d; tau %.3f s, tolerances %.4f m %.2f deg %.3f; max error %.4f m %.2f deg %.3f; jitter removed %.4f m",
        r.after, r.before, p.tau, p.posTol, p.angTol, p.curlTol, r.maxPos, r.maxAng, r.maxCurl, r.rawPos);
    out.smoothInfo = info;
    return r;
}

} // namespace qvr::animrec
