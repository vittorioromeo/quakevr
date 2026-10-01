// vr_sightalign.cpp -- see vr_sightalign.hpp.

#include "vr_sightalign.hpp"
#include "vr_still.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menu.hpp"
#include "vr_menuui.hpp"
#include "vr_text3d.hpp"
#include "vr_trace.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Algorithm/Copy.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

#include <string.h>

namespace qvr::sightalign
{

namespace
{

using weapons::Key;

// ---------------------------------------------------------------------------------------------------------------------
// Sight lines

// The painted sights (vr_sights.cpp's models): how the rear sight is read. The front is a post (or bead): its top.
enum class Rear
{
    Notch, // two posts: the middle of the gap between them, at their tops
    Ring   // an aperture (a ghost ring): its middle
};
struct Painted
{
    const char* model;
    Rear rear;
};
constexpr Painted painted[] = {
    {"progs/v_shot.mdl", Rear::Notch},  // two rear posts, a front post
    {"progs/v_shot2.mdl", Rear::Ring},  // a ring, a front post
    {"progs/v_light.mdl", Rear::Notch}, // two rear posts, a front post (round 20)
    {"progs/v_plasma.mdl", Rear::Notch}, // the lightning gun's alternate: the same model
};

// The guns without painted sights: a line along the top of the barrel, on the model's middle (y 0), parallel to its
// barrel (the model's x axis, which Pitch and Yaw put along the shots), from above the grip (where the fist is) to the
// muzzle, just over the highest part of the gun in between (the top of the mesh along the middle, found with
// sightline_table.py: nothing stands above the line to hide it). Model units, as the frames' vertices.
struct Table
{
    const char* model;
    glm::vec3 rear;
    glm::vec3 front;
};
constexpr Table table[] = {
#include "vr_sightalign_table.inc"
};

// No sights: melee weapons, the hand, and the monsters' guns (their lights and screens are not sights).
constexpr const char* melee[] = {
    "progs/v_axe.mdl", "progs/v_hammer.mdl", "progs/v_ksword.mdl", "progs/v_hksword.mdl", "progs/v_chainsaw.mdl",
    "progs/hand.mdl", "progs/v_gruntgun.mdl", "progs/v_enfrifle.mdl", "progs/v_crowbar.mdl",
};

[[nodiscard]] bool isSightIndex(int i)
{
    return (i >= 224 && i <= 239) || i == 252 || i == 253; // as vr_sights.cpp
}

// The model file's sight texels, as points on its first frame (model space, as the frames' vertices).
[[nodiscard]] za::Vector<glm::vec3> sightTexels(const char* modelName)
{
    za::Vector<glm::vec3> pts;
    byte* data = COM_LoadMallocFile(modelName, nullptr);
    if(!data)
    {
        return pts;
    }
    const int size = static_cast<int>(com_filesize);
    const auto readInt = [&](int o) {
        int v;
        ZA_MEMCPY(&v, data + o, sizeof(v));
        return LittleLong(v);
    };
    const auto readFloat = [&](int o) {
        float v;
        ZA_MEMCPY(&v, data + o, sizeof(v));
        return LittleFloat(v);
    };
    constexpr int headerSize = 84;
    if(size < headerSize || readInt(0) != IDPOLYHEADER)
    {
        free(data);
        return pts;
    }
    const glm::vec3 scale{readFloat(8), readFloat(12), readFloat(16)};
    const glm::vec3 origin{readFloat(20), readFloat(24), readFloat(28)};
    const int numskins = readInt(48), sw = readInt(52), sh = readInt(56), numverts = readInt(60), numtris = readInt(64),
              numframes = readInt(68);
    const int skinsize = sw * sh;
    int ofs = headerSize;
    const byte* skin = nullptr;
    for(int i = 0; i < numskins && ofs < size; i++)
    {
        const int type = readInt(ofs);
        ofs += 4;
        if(type == 0)
        {
            skin = skin ? skin : data + ofs;
            ofs += skinsize;
        }
        else
        {
            const int n = readInt(ofs);
            skin = skin ? skin : data + ofs + 4 + n * 4;
            ofs += 4 + n * 4 + n * skinsize;
        }
    }
    const int stOfs = ofs;
    const int triOfs = stOfs + numverts * 12;
    int frameOfs = triOfs + numtris * 16;
    if(!skin || numframes < 1 || frameOfs + 4 > size)
    {
        free(data);
        return pts;
    }
    // The first frame (a group's first).
    if(readInt(frameOfs) == 0)
    {
        frameOfs += 4 + 24;
    }
    else
    {
        const int n = readInt(frameOfs + 4);
        frameOfs += 4 + 4 + 8 + n * 4 + 24;
    }
    if(frameOfs + numverts * 4 > size)
    {
        free(data);
        return pts;
    }
    const auto vertex = [&](int i) {
        const byte* v = data + frameOfs + i * 4;
        return origin + scale * glm::vec3{v[0], v[1], v[2]};
    };

    for(int t = 0; t < numtris; t++)
    {
        const int o = triOfs + t * 16;
        const bool front = readInt(o) != 0;
        int vi[3];
        glm::vec2 uv[3];
        for(int k = 0; k < 3; k++)
        {
            vi[k] = readInt(o + 4 + k * 4);
            if(vi[k] < 0 || vi[k] >= numverts)
            {
                free(data);
                return {};
            }
            const int so = stOfs + vi[k] * 12;
            float s = static_cast<float>(readInt(so + 4));
            if(readInt(so) && !front)
            {
                s += static_cast<float>(sw / 2); // the back half of the skin
            }
            uv[k] = {s + 0.5f, static_cast<float>(readInt(so + 8)) + 0.5f};
        }
        const float den = (uv[1].y - uv[2].y) * (uv[0].x - uv[2].x) + (uv[2].x - uv[1].x) * (uv[0].y - uv[2].y);
        if(za::fabs(den) < 1e-9f)
        {
            continue;
        }
        const glm::vec3 p0 = vertex(vi[0]), p1 = vertex(vi[1]), p2 = vertex(vi[2]);
        const int x0 = za::max(0, static_cast<int>(za::floor(qza::minOf(uv[0].x, uv[1].x, uv[2].x))) - 1);
        const int x1 = za::min(sw - 1, static_cast<int>(za::ceil(qza::maxOf(uv[0].x, uv[1].x, uv[2].x))) + 1);
        const int y0 = za::max(0, static_cast<int>(za::floor(qza::minOf(uv[0].y, uv[1].y, uv[2].y))) - 1);
        const int y1 = za::min(sh - 1, static_cast<int>(za::ceil(qza::maxOf(uv[0].y, uv[1].y, uv[2].y))) + 1);
        for(int ty = y0; ty <= y1; ty++)
        {
            for(int tx = x0; tx <= x1; tx++)
            {
                if(!isSightIndex(skin[ty * sw + tx]))
                {
                    continue;
                }
                const float px = tx + 0.5f, py = ty + 0.5f;
                const float l0 = ((uv[1].y - uv[2].y) * (px - uv[2].x) + (uv[2].x - uv[1].x) * (py - uv[2].y)) / den;
                const float l1 = ((uv[2].y - uv[0].y) * (px - uv[2].x) + (uv[0].x - uv[2].x) * (py - uv[2].y)) / den;
                const float l2 = 1.f - l0 - l1;
                if(l0 < -1e-5f || l1 < -1e-5f || l2 < -1e-5f)
                {
                    continue;
                }
                pts.pushBack(l0 * p0 + l1 * p1 + l2 * p2);
            }
        }
    }
    free(data);
    return pts;
}

// The sight line from the painted sights: the texels split along the barrel into the rear sight (the first group) and
// the front one (the last), where more than 1.5 units separate them.
[[nodiscard]] SightLine fromTexels(const char* modelName, Rear rearKind)
{
    za::Vector<glm::vec3> pts = sightTexels(modelName);
    if(pts.size() < 8)
    {
        return {};
    }
    za::quickSort(pts.begin(), pts.end(), [](const glm::vec3& a, const glm::vec3& b) { return a.x < b.x; });
    za::SizeT rearEnd = 1;
    while(rearEnd < pts.size() && pts[rearEnd].x - pts[rearEnd - 1].x <= 1.5f)
    {
        rearEnd++;
    }
    za::SizeT frontBegin = pts.size() - 1;
    while(frontBegin > 0 && pts[frontBegin].x - pts[frontBegin - 1].x <= 1.5f)
    {
        frontBegin--;
    }
    if(rearEnd > frontBegin)
    {
        return {}; // one group only
    }

    // The front post: its top, in its middle.
    SightLine line;
    glm::vec3 sum{0.f};
    float top = -1e9f;
    for(za::SizeT i = frontBegin; i < pts.size(); i++)
    {
        sum += pts[i];
        top = za::fmax(top, pts[i].z);
    }
    const glm::vec3 mid = sum / static_cast<float>(pts.size() - frontBegin);
    line.front = {mid.x, mid.y, top};

    // The rear sight, near the middle (the double shotgun's skin has marks at the sides of the same face).
    za::Vector<glm::vec3> rear;
    for(za::SizeT i = 0; i < rearEnd; i++)
    {
        if(za::fabs(pts[i].y - line.front.y) < 1.5f)
        {
            rear.pushBack(pts[i]);
        }
    }
    if(rear.size() < 4)
    {
        return {};
    }
    float x = 0.f;
    for(const glm::vec3& p : rear)
    {
        x += p.x;
    }
    x /= static_cast<float>(rear.size());

    if(rearKind == Rear::Notch)
    {
        // The widest gap across between the texels is the notch; its middle, at the posts' tops.
        za::Vector<float> ys;
        float rearTop = -1e9f;
        for(const glm::vec3& p : rear)
        {
            ys.pushBack(p.y);
            rearTop = za::fmax(rearTop, p.z);
        }
        za::quickSort(ys.begin(), ys.end());
        float gap = 0.f, gapMid = 0.f;
        for(za::SizeT i = 1; i < ys.size(); i++)
        {
            if(ys[i] - ys[i - 1] > gap)
            {
                gap = ys[i] - ys[i - 1];
                gapMid = 0.5f * (ys[i] + ys[i - 1]);
            }
        }
        if(gap < 0.15f)
        {
            return {};
        }
        line.rear = {x, gapMid, rearTop};
    }
    else
    {
        // A ring: the circle through its texels (least squares, across: y and z), its middle.
        double m[3][4]{};
        for(const glm::vec3& p : rear)
        {
            const double row[3]{p.y, p.z, 1.0}, rhs = double(p.y) * p.y + double(p.z) * p.z;
            for(int i = 0; i < 3; i++)
            {
                for(int j = 0; j < 3; j++)
                {
                    m[i][j] += row[i] * row[j];
                }
                m[i][3] += row[i] * rhs;
            }
        }
        for(int c = 0; c < 3; c++) // Gauss-Jordan
        {
            int pivot = c;
            for(int r = c + 1; r < 3; r++)
            {
                pivot = za::fabs(m[r][c]) > za::fabs(m[pivot][c]) ? r : pivot;
            }
            za::genericSwap(m[c], m[pivot]);
            if(za::fabs(m[c][c]) < 1e-12)
            {
                return {};
            }
            for(int r = 0; r < 3; r++)
            {
                if(r != c)
                {
                    const double f = m[r][c] / m[c][c];
                    for(int k = c; k < 4; k++)
                    {
                        m[r][k] -= f * m[c][k];
                    }
                }
            }
        }
        line.rear = {x, static_cast<float>(0.5 * m[0][3] / m[0][0]), static_cast<float>(0.5 * m[1][3] / m[1][1])};
    }
    line.valid = line.front.x - line.rear.x > 2.f;
    line.painted = line.valid;
    return line;
}

ankerl::unordered_dense::map<za::String, SightLine> lines;

// ---------------------------------------------------------------------------------------------------------------------
// Frames and angles

// The hands' angles as a basis (columns forward, left, up), and back.
[[nodiscard]] glm::mat3 basisOf(const glm::vec3& a)
{
    glm::vec3 f, r, u;
    hands::angleVectors(a, f, r, u);
    return glm::mat3{f, -r, u};
}
[[nodiscard]] glm::vec3 anglesOf(const glm::mat3& b)
{
    return hands::anglesFromVectors(glm::normalize(b[0]), glm::normalize(b[2]));
}

// A Hand and Weapon Together turn (pitch up, yaw left, roll: vr_hands.cpp wholeOffset) as a basis in the controller's
// aim frame, and back.
[[nodiscard]] glm::mat3 wholeTurn(const glm::vec3& a)
{
    return basisOf({-a.x, a.y, a.z});
}
[[nodiscard]] glm::vec3 wholeAngles(const glm::mat3& t)
{
    const glm::vec3 q = anglesOf(t);
    return {-q.x, q.y, q.z};
}

// The least turn taking the direction `from` onto `to` (about the axis across both).
[[nodiscard]] glm::mat3 leastTurn(const glm::vec3& from, const glm::vec3& to)
{
    const glm::vec3 a = glm::normalize(from), b = glm::normalize(to);
    const glm::vec3 axis = glm::cross(a, b);
    const float len = glm::length(axis);
    if(len < 1e-9f)
    {
        return glm::mat3{1.f}; // already along it (never the opposite way here: the captures are checked)
    }
    return glm::mat3_cast(glm::angleAxis(za::atan2(len, glm::dot(a, b)), axis / len));
}

[[nodiscard]] float degreesBetween(const glm::vec3& a, const glm::vec3& b)
{
    return glm::degrees(za::atan2(glm::length(glm::cross(a, b)), glm::dot(a, b)));
}

// Distance from p to the line through a along the unit u.
[[nodiscard]] float lineDistance(const glm::vec3& p, const glm::vec3& a, const glm::vec3& u)
{
    const glm::vec3 w = p - a;
    return glm::length(w - glm::dot(w, u) * u);
}

[[nodiscard]] float unitsToCm(float u)
{
    return u / units::metresToUnits() * 100.f;
}
[[nodiscard]] float cmToUnits(float cm)
{
    return cm * 0.01f * units::metresToUnits();
}

[[nodiscard]] int dominantEye()
{
    return vr_dominant_eye.value != 0.f ? 0 : 1; // hands::State::eyeOrigin: [0] left, [1] right
}

[[nodiscard]] const char* handName(int hand)
{
    return hand == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] za::String modelBase(const qmodel_t* m)
{
    if(!m)
    {
        return "?";
    }
    const char* s = strrchr(m->name, '/');
    return s ? s + 1 : m->name;
}

// The held weapon's sight line in the world as drawn now, and the frames the correction is worked out in: the
// controller's calibrated aim frame C (the hand before the weapon's Hand and Weapon Together offset: calibratedPos and
// calibratedRot) and the fist.
struct Pose
{
    const qmodel_t* model{nullptr};
    int slot{-1};
    glm::vec3 rear{0.f}, front{0.f}, fist{0.f}, eye{0.f};
    glm::vec3 gaze{1.f, 0.f, 0.f};
    glm::vec3 cPos{0.f};
    glm::mat3 cBasis{1.f};
    glm::vec3 handPos{0.f};
    glm::vec3 handFwd{1.f, 0.f, 0.f};
    glm::mat3 handBasis{1.f}; // the hand's aim frame (hands::State::rot): where its shots go from (weapons::shotAngles)
    bool gripValid{false};    // the foregrip (the first Grip hotspot), world
    glm::vec3 grip{0.f};
};
[[nodiscard]] bool poseOf(const hands::State& s, int hand, Pose& out)
{
    view::WeaponFrame wf;
    if(!s.valid || !view::weaponFrame(s, hand, wf))
    {
        return false;
    }
    const SightLine line = sightLine(wf.model);
    if(!line.valid)
    {
        return false;
    }
    out.model = wf.model;
    out.slot = weapons::slotForModel(wf.model);
    out.rear = glm::vec3{wf.modelToWorld * glm::vec4{line.rear, 1.f}};
    out.front = glm::vec3{wf.modelToWorld * glm::vec4{line.front, 1.f}};
    out.fist = wf.fist;
    out.eye = s.eyeOrigin[dominantEye()];
    out.gaze = hands::forward(s.eyeAngles[dominantEye()]);
    out.cPos = s.calibratedPos[hand];
    out.cBasis = basisOf(s.calibratedRot[hand]);
    out.handPos = s.pos[hand];
    out.handFwd = hands::forward(s.rot[hand]);
    out.handBasis = basisOf(s.rot[hand]);
    out.gripValid = false;
    for(int i = 0; i < weapons::maxHotspots && !out.gripValid; i++)
    {
        const view::WeaponHotspot h = view::weaponHotspot(hand, i);
        if(h.type == static_cast<int>(weapons::HotspotType::Grip))
        {
            out.gripValid = true;
            out.grip = h.pos;
        }
    }
    return true;
}

// A slot's Hand and Weapon Together offset as applied to `hand` (the off hand's mirrored), and a value for the keys.
void wholeFor(int slot, int hand, glm::vec3& p, glm::vec3& a)
{
    p = weapons::vec(slot, Key::WholeX, Key::WholeY, Key::WholeZ);
    a = weapons::vec(slot, Key::WholePitch, Key::WholeYaw, Key::WholeRoll);
    if(hand == HAND_OFF)
    {
        p.y = -p.y;
        a.y = -a.y;
        a.z = -a.z;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// The capture

// One sample: in the controller's aim frame C (x forward, y left, z up, world units, from its point).
struct Sample
{
    double time{0.0};
    glm::vec3 eye{0.f}, rear{0.f}, front{0.f}, fist{0.f};
    glm::vec3 sightInHand{1.f, 0.f, 0.f}; // the sight line's direction in the hand's aim frame (the shots' frame)
    glm::vec3 gaze{1.f, 0.f, 0.f};        // where the eye faces (the headset's forward for it)
    glm::vec3 grip{0.f};                  // the weapon's foregrip (its first Grip hotspot) in the hand's frame (f, r, u)
    bool gripValid{false};
    glm::vec3 handPos{0.f}, handFwd{1.f, 0.f, 0.f}; // world, for the stillness
};

enum class Step
{
    Countdown,
    WaitRaise, // the hand must move (lowered, then raised) before a capture
    WaitStill
};

constexpr double stillSeconds = 0.4;
constexpr float stillCm = 1.0f;     // the eye (in the controller's frame) and the hand, round their average
constexpr float stillDegrees = 1.0f; // the hand's aim
constexpr float stillWeaponCm = 0.2f; // the sights in the controller's frame: the weapon's lag behind the hand settled
constexpr float moveCm = 8.f;       // lowered: this far from the last capture (or the go beep)
constexpr float moveDegrees = 15.f;
// A capture is taken only with the sights roughly before the eye (a gun held low can have its sights on a line with the
// eye too: then the head doesn't face them).
constexpr float plausibleDegrees = 15.f;
constexpr float plausibleCm = 15.f;
constexpr float plausibleGazeDegrees = 20.f;
constexpr double giveUpSeconds = 45.0;

struct Capture
{
    Sample mean; // over the still window
};

struct Result
{
    int hand{HAND_MAIN};
    int slot{-1};
    const qmodel_t* model{nullptr};
    int used{0}, dropped{0};
    float spreadDegrees{0.f};
    float offDegrees{0.f}, offCm{0.f}; // the sights as they were: the turn onto the eye's ray, the eye off the line
    float turnDegrees{0.f}, moveCm{0.f}; // the correction: turned about the fist, the fist moved
    float shotDegrees{0.f};              // the shots turned by
    glm::vec3 eye{0.f}, rear{0.f}, front{0.f}, fist{0.f}; // in C (averaged)
    glm::mat3 q{1.f};                                     // the correction in C: x -> q (x - fist) + fist + t
    glm::vec3 t{0.f};
    float values[14]{}; // whole x y z pitch yaw roll, shot pitch yaw, two-handed aim x y z pitch yaw roll (unmirrored)
    int count{8};       // the values written: 8, or 14 with the two-handed aim
    float twoHandDegrees{0.f}; // how far holding the foregrip turned the aim before (with the aim offsets as they were)
};

struct State
{
    Phase phase{Phase::Idle};
    Step step{Step::Countdown};
    int hand{HAND_MAIN};
    int returnPage{-1};
    int world{0};
    const qmodel_t* model{nullptr};
    int slot{-1};
    double started{0.0};
    double lastEvent{0.0};
    still::Countdown countdown;
    int wanted{4};
    // The last moments' samples (vr_still.hpp), the channels as Sample's: eye, hand, hand's aim, rear, front (checked),
    // then fist, sight in hand, gaze, grip (averaged).
    still::Window window{{{still::Window::Kind::Point}, {still::Window::Kind::Point}, {still::Window::Kind::Direction, 0.f},
        {still::Window::Kind::Point}, {still::Window::Kind::Point}, {still::Window::Kind::Averaged},
        {still::Window::Kind::AveragedDirection}, {still::Window::Kind::AveragedDirection}, {still::Window::Kind::Averaged}}};
    bool lastGripValid{false};
    za::Vector<Capture> captures;
    glm::vec3 refPos{0.f}, refFwd{1.f, 0.f, 0.f};
    bool refSet{false};
    za::String message; // why it stopped
};
State st;
Result result;
bool hasResult = false; // a result shown (pending or applied)
bool applied = false;
int versionCounter = 0;

// Undo: the cvars written by the last Apply, and their strings before.
struct Written
{
    cvar_t* cvar;
    za::String before;
};
za::Vector<Written> undoList;
za::String savedTo;

za::String lineBuf[8];

void bump()
{
    versionCounter++;
}

void sound(const char* name)
{
    S_LocalSound(name);
}

void stopCapture(const char* why, bool reopen)
{
    st.phase = hasResult && !applied ? Phase::Result : Phase::Idle;
    st.message = why ? why : "";
    if(why && *why)
    {
        Con_Printf("Align Sights: %s\n", why);
    }
    bump();
    if(reopen && st.returnPage >= 0 && key_dest != key_menu)
    {
        menu::reopen(st.returnPage);
    }
}

[[nodiscard]] bool moved(const glm::vec3& pos, const glm::vec3& fwd)
{
    return !st.refSet || glm::distance(pos, st.refPos) >= cmToUnits(moveCm) || degreesBetween(fwd, st.refFwd) >= moveDegrees;
}

[[nodiscard]] bool plausible(const Sample& s)
{
    const glm::vec3 u = glm::normalize(s.front - s.rear);
    return degreesBetween(u, s.front - s.eye) <= plausibleDegrees && lineDistance(s.eye, s.rear, u) <= cmToUnits(plausibleCm) &&
           glm::dot(s.rear - s.eye, u) > 0.f &&                         // the eye behind the rear sight
           degreesBetween(s.gaze, s.front - s.eye) <= plausibleGazeDegrees; // and facing the sights, not a gun held low
}

glm::vec4 lastDev{0.f}; // the last window's largest moves (eye, hand: units; hand: degrees; the sights: units), to debug

// The window's average, if the last stillSeconds were still.
[[nodiscard]] bool stillMean(double now, Sample& mean)
{
    const float tol = cmToUnits(stillCm);
    const float settled = cmToUnits(stillWeaponCm);
    st.window.tolerance(0, tol);
    st.window.tolerance(1, tol);
    st.window.tolerance(2, stillDegrees);
    st.window.tolerance(3, settled);
    st.window.tolerance(4, settled);
    za::Vector<glm::vec3> m;
    const bool ok = st.window.still(now, stillSeconds, m);
    const za::Vector<float>& d = st.window.deviations();
    lastDev = glm::vec4{d[0], d[1], d[2], za::fmax(d[3], d[4])};
    if(!ok)
    {
        return false;
    }
    mean = Sample{};
    mean.time = now;
    mean.eye = m[0];
    mean.handPos = m[1];
    mean.handFwd = m[2];
    mean.rear = m[3];
    mean.front = m[4];
    mean.fist = m[5];
    mean.sightInHand = m[6];
    mean.gaze = m[7];
    mean.grip = m[8];
    mean.gripValid = st.lastGripValid;
    return true;
}

// The correction from the captures (vr_sightalign.hpp).
void solve()
{
    const int n = static_cast<int>(st.captures.size());
    Result r;
    r.hand = st.hand;
    r.slot = st.slot;
    r.model = st.model;

    // The weapon's points are the same in every capture (it is fixed in the controller's frame); the eye is not.
    glm::vec3 sightInHand{0.f};
    for(const Capture& c : st.captures)
    {
        r.rear += c.mean.rear;
        r.front += c.mean.front;
        r.fist += c.mean.fist;
        sightInHand += c.mean.sightInHand;
    }
    sightInHand = glm::normalize(sightInHand);
    r.rear /= static_cast<float>(n);
    r.front /= static_cast<float>(n);
    r.fist /= static_cast<float>(n);

    // Outliers: an eye further from the others' median than max(1.5 cm, 3 x the median distance).
    za::Vector<float> xs, ys, zs;
    for(const Capture& c : st.captures)
    {
        xs.pushBack(c.mean.eye.x);
        ys.pushBack(c.mean.eye.y);
        zs.pushBack(c.mean.eye.z);
    }
    const auto median = [](za::Vector<float> v) {
        za::quickSort(v.begin(), v.end());
        const za::SizeT m = v.size() / 2;
        return v.size() % 2 ? v[m] : 0.5f * (v[m - 1] + v[m]);
    };
    const glm::vec3 med{median(xs), median(ys), median(zs)};
    za::Vector<float> dist;
    for(const Capture& c : st.captures)
    {
        dist.pushBack(glm::distance(c.mean.eye, med));
    }
    const float limit = za::fmax(cmToUnits(1.5f), 3.f * median(dist));
    glm::vec3 eye{0.f};
    for(int i = 0; i < n; i++)
    {
        if(dist[i] <= limit || n < 3)
        {
            eye += st.captures[i].mean.eye;
            r.used++;
        }
    }
    r.dropped = n - r.used;
    r.eye = eye / static_cast<float>(r.used);

    const glm::vec3 u = glm::normalize(r.front - r.rear);
    const glm::vec3 d = glm::normalize(r.front - r.eye); // where the eye looked: through the front sight
    float spread = 0.f;
    for(int i = 0; i < n; i++)
    {
        if(dist[i] <= limit || n < 3)
        {
            const float a = degreesBetween(r.front - st.captures[i].mean.eye, d);
            spread += a * a;
        }
    }
    r.spreadDegrees = za::sqrt(spread / static_cast<float>(r.used));
    r.offDegrees = degreesBetween(u, d);
    r.offCm = unitsToCm(lineDistance(r.eye, r.rear, u));

    // The least turn taking the sight line's direction onto the eye's ray, about the fist; then the least move putting
    // the line through the eye (across the ray: along it changes nothing).
    r.q = leastTurn(u, d);
    const glm::vec3 turnedRear = r.fist + r.q * (r.rear - r.fist);
    const glm::vec3 w = turnedRear - r.eye;
    r.t = -(w - glm::dot(w, d) * d);
    r.turnDegrees = r.offDegrees;
    r.moveCm = unitsToCm(glm::length(r.t));

    // The new Hand and Weapon Together offset: the hand's pose in C is (p, T); the correction moves it rigidly.
    glm::vec3 p, a;
    wholeFor(r.slot, r.hand, p, a);
    const glm::mat3 turnBefore = wholeTurn(a);
    const glm::vec3 pNew = r.q * (p - r.fist) + r.fist + r.t;
    glm::vec3 aNew = wholeAngles(r.q * turnBefore);
    // The shots along the sight line: its direction in the hand's frame (the same before and after), as a pitch up and
    // a yaw left in the aim's frame (weapons::shotAngles).
    const glm::vec3 local = sightInHand;
    float shotPitch = glm::degrees(za::asin(CLAMP(-1.f, local.z, 1.f)));
    float shotYaw = glm::degrees(za::atan2(local.y, local.x));
    glm::vec3 pStore = pNew;
    if(r.hand == HAND_OFF)
    {
        pStore.y = -pStore.y;
        aNew.y = -aNew.y;
        aNew.z = -aNew.z;
        shotYaw = -shotYaw;
    }
    const float oldShot[2]{weapons::value(r.slot, Key::ShotPitch), weapons::value(r.slot, Key::ShotYaw)};
    const float values[8]{pStore.x, pStore.y, pStore.z, aNew.x, aNew.y, aNew.z, shotPitch, shotYaw};
    za::copy(values, values + 8, r.values);

    // Two-handed: the aim then runs from the hand to the other hand (on the foregrip) moved by the weapon's Aim Offset,
    // turned by its Aim Pitch/Yaw/Roll (vr_twohand.cpp). Set so that taking the foregrip where it is drawn keeps the gun
    // exactly as held in one hand (the offset puts the aim's target on the hand's own forward, the turns 0): the sights
    // stay aligned. The foregrip is fixed in the hand's frame, so this holds for any pose and after the fix.
    const int mode2h = static_cast<int>(weapons::value(r.slot, Key::TwoHMode));
    const Sample& last = st.captures.back().mean;
    if(last.gripValid && mode2h != 2 && mode2h != 3) // 2: no two-handed aim; 3: a sword
    {
        glm::vec3 g{0.f};
        for(const Capture& c : st.captures)
        {
            g += c.mean.grip;
        }
        g /= static_cast<float>(n);
        glm::vec3 off{glm::length(g) - g.x, -g.y, -g.z};
        // What holding it did before: the aim towards the foregrip moved by the old offset, then its turns.
        glm::vec3 oldOff = weapons::vec(r.slot, Key::TwoHOffsetX, Key::TwoHOffsetY, Key::TwoHOffsetZ);
        glm::vec3 oldTurn = weapons::vec(r.slot, Key::TwoHPitch, Key::TwoHYaw, Key::TwoHRoll);
        if(r.hand == HAND_OFF)
        {
            oldOff.y = -oldOff.y;
            oldTurn.y = -oldTurn.y;
            oldTurn.z = -oldTurn.z;
        }
        const glm::vec3 t2 = g + oldOff; // (forward, right, up)
        // The Euler turns are added to the aim's angles: taken at a level aim, as a turn in its frame.
        const glm::vec3 aimed = hands::anglesFromVectors(glm::normalize(glm::vec3{t2.x, -t2.y, t2.z}), glm::vec3{0.f, 0.f, 1.f}) + oldTurn;
        r.twoHandDegrees = degreesBetween(hands::forward(aimed), glm::vec3{1.f, 0.f, 0.f});
        if(r.hand == HAND_OFF)
        {
            off.y = -off.y;
        }
        const float more[6]{off.x, off.y, off.z, 0.f, 0.f, 0.f};
        za::copy(more, more + 6, r.values + 8);
        r.count = 14;
    }
    {
        // The shots' turn, in the aim's frame (the off hand's yaw as applied: mirrored).
        const auto dir = [](float pitch, float yaw) {
            const float pr = glm::radians(pitch), yr = glm::radians(yaw);
            return glm::vec3{za::cos(pr) * za::cos(yr), za::cos(pr) * za::sin(yr), za::sin(pr)};
        };
        const float mirror = r.hand == HAND_OFF ? -1.f : 1.f;
        r.shotDegrees = degreesBetween(dir(oldShot[0], oldShot[1] * mirror), dir(shotPitch, shotYaw * mirror));
    }
    result = r;
    hasResult = true;
    applied = false;
    undoList.clear();
    savedTo.clear();

    Con_Printf("Align Sights (%s, %s hand): %d captures used, %d dropped, spread %.2f deg\n", modelBase(r.model).cStr(),
        handName(r.hand), r.used, r.dropped, r.spreadDegrees);
    Con_Printf("  the sights were %.2f deg off the eye's ray, the eye %.2f cm off the sight line\n", r.offDegrees, r.offCm);
    Con_Printf("  the fix: turned %.2f deg about the fist, moved %.2f cm; shots turned %.2f deg\n", r.turnDegrees, r.moveCm,
        r.shotDegrees);
    Con_Printf("  whole %.4f %.4f %.4f / %.4f %.4f %.4f, shot %.4f %.4f\n", values[0], values[1], values[2], values[3], values[4],
        values[5], values[6], values[7]);
    if(r.count > 8)
    {
        Con_Printf("  two-handed: holding the foregrip turned the aim %.2f deg; aim offset %.4f %.4f %.4f, turns 0\n",
            r.twoHandDegrees, r.values[8], r.values[9], r.values[10]);
    }
}

constexpr Key keys[14] = {Key::WholeX, Key::WholeY, Key::WholeZ, Key::WholePitch, Key::WholeYaw, Key::WholeRoll, Key::ShotPitch,
    Key::ShotYaw, Key::TwoHOffsetX, Key::TwoHOffsetY, Key::TwoHOffsetZ, Key::TwoHPitch, Key::TwoHYaw, Key::TwoHRoll};

// Sets what `slot` uses for `key` (its own value, or the one it inherits: as the page edits), recording what it wrote.
void setEffective(int slot, Key key, float v)
{
    for(int tries = 0; tries < 3; tries++)
    {
        cvar_t* c = weapons::cvar(weapons::ownerSlot(slot, key), key);
        if(!c)
        {
            return;
        }
        undoList.pushBack({c, c->string});
        Cvar_Set(c->name, va("%.7g", v));
        // Written into a slot's own key as its default value, it may inherit again: then it is written where it inherits.
        if(za::fabs(weapons::value(slot, key) - v) <= 1e-5f * za::fmax(1.f, za::fabs(v)))
        {
            return;
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing

void drawLine(const glm::vec3& rear, const glm::vec3& front, const glm::vec4& colour, float width, bool points)
{
    const glm::vec3 dir = glm::normalize(front - rear);
    const glm::vec3 farEnd = front + dir * 4096.f;
    const trace_t tr = worldtrace::world(front, farEnd);
    const glm::vec3 end = tr.fraction < 1.f ? worldtrace::endPos(tr) : front + dir * 1024.f;
    lines::line(rear, front, width, colour, colour);
    lines::line(front, end, width * 0.7f, colour, glm::vec4{glm::vec3{colour}, colour.a * 0.4f});
    if(tr.fraction < 1.f)
    {
        lines::point(end, za::fmax(0.8f, glm::distance(front, end) * 0.005f), colour);
    }
    if(points)
    {
        lines::point(rear, 0.35f, {1.f, 0.85f, 0.2f, 1.f});  // the rear sight: yellow
        lines::point(front, 0.3f, {0.2f, 0.9f, 1.f, 1.f});   // the front sight: cyan
    }
}

// Where a point given in C lies now.
[[nodiscard]] glm::vec3 fromC(const Pose& p, const glm::vec3& c)
{
    return p.cPos + p.cBasis * c;
}
[[nodiscard]] glm::vec3 toC(const Pose& p, const glm::vec3& w)
{
    return glm::transpose(p.cBasis) * (w - p.cPos);
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands

void check_f()
{
    const int hand = Cmd_Argc() >= 2 && !q_strcasecmp(Cmd_Argv(1), "off") ? HAND_OFF : HAND_MAIN;
    const hands::State& s = hands::current();
    // In model space, for the table: the fist and the hand's point.
    view::WeaponFrame wf;
    if(view::weaponFrame(s, hand, wf))
    {
        const glm::mat4 inv = glm::inverse(wf.modelToWorld);
        const glm::vec3 fm{inv * glm::vec4{wf.fist, 1.f}}, hm{inv * glm::vec4{s.pos[hand], 1.f}};
        const glm::vec3 mm{inv * glm::vec4{s.muzzle[hand], 1.f}};
        Con_Printf("sightcheck %s model space: fist %.2f %.2f %.2f (%s), hand %.2f %.2f %.2f, muzzle %.2f %.2f %.2f\n",
            modelBase(wf.model).cStr(), fm.x, fm.y, fm.z, wf.fistFromRig ? "rig" : "palm", hm.x, hm.y, hm.z, mm.x, mm.y, mm.z);
    }
    Pose p;
    if(!poseOf(s, hand, p))
    {
        Con_Printf("vr_sight_check: no weapon with sights in the %s hand\n", handName(hand));
        return;
    }
    const glm::vec3 u = glm::normalize(p.front - p.rear);
    const float rayDeg = degreesBetween(u, p.front - p.eye);
    const float eyeLine = lineDistance(p.eye, p.rear, u);
    const float apparent = degreesBetween(p.rear - p.eye, p.front - p.eye); // the rear and front sights seen apart
    Con_Printf("sightcheck %s %s: eye %s, sight line vs eye ray %.4f deg, eye to line %.4f units (%.3f mm), rear/front seen "
               "%.4f deg apart\n",
        modelBase(p.model).cStr(), handName(hand), dominantEye() == 1 ? "right" : "left", rayDeg, eyeLine,
        unitsToCm(eyeLine) * 10.f, apparent);
    if(p.gripValid && twohand::transition(hand) <= 0.f)
    {
        // Held two-handed with the other hand's point on the foregrip as drawn now: how far the aim would turn
        // (vr_twohand.cpp applyHand, without the virtual stock).
        glm::vec3 off = weapons::vec(p.slot, Key::TwoHOffsetX, Key::TwoHOffsetY, Key::TwoHOffsetZ);
        glm::vec3 turn = weapons::vec(p.slot, Key::TwoHPitch, Key::TwoHYaw, Key::TwoHRoll);
        if(hand == HAND_OFF)
        {
            off.y = -off.y;
            turn.y = -turn.y;
            turn.z = -turn.z;
        }
        glm::vec3 f, r, up;
        hands::angleVectors(s.rot[hand], f, r, up);
        const glm::vec3 target = p.grip + hands::redirect(off, s.rot[hand]);
        const glm::vec3 aim = hands::anglesFromVectors(glm::normalize(target - s.pos[hand]), up) + turn;
        Con_Printf("sightcheck two-handed on the foregrip: the aim would turn %.4f deg\n", degreesBetween(hands::forward(aim), f));
    }
    if(twohand::transition(hand) > 0.f)
    {
        Con_Printf("sightcheck aimed two-handed (%.2f)%s", twohand::transition(hand), "\n");
    }
    Con_Printf("sightcheck rear %.3f %.3f %.3f front %.3f %.3f %.3f eye %.3f %.3f %.3f fist %.3f %.3f %.3f\n", p.rear.x, p.rear.y,
        p.rear.z, p.front.x, p.front.y, p.front.z, p.eye.x, p.eye.y, p.eye.z, p.fist.x, p.fist.y, p.fist.z);
    // Where they are seen in the dominant eye's image (the mock's field of view: tangents +-0.8), in pixels of a
    // `vr_sight_check <hand> <size>` image (default 2048): the rear and front sights, and where the sight line and the
    // shots meet the wall.
    const float size = Cmd_Argc() >= 3 ? Q_atof(Cmd_Argv(2)) : 2048.f;
    glm::vec3 ef, er, eu;
    hands::angleVectors(s.eyeAngles[dominantEye()], ef, er, eu);
    const auto pixel = [&](const glm::vec3& w) {
        const glm::vec3 d = w - p.eye;
        const float z = glm::dot(d, ef);
        return glm::vec2{(glm::dot(d, er) / z / 0.8f + 1.f) * 0.5f * size, (1.f - glm::dot(d, eu) / z / 0.8f) * 0.5f * size};
    };
    const auto wallHit = [](const glm::vec3& from, const glm::vec3& dir) {
        const trace_t tr = worldtrace::world(from, from + dir * 8192.f);
        return worldtrace::endPos(tr);
    };
    const glm::vec2 pr = pixel(p.rear), pf = pixel(p.front), pw = pixel(wallHit(p.front, u));
    Con_Printf("sightcheck pixels (%g): rear %.1f %.1f front %.1f %.1f sight line at the wall %.1f %.1f (%.0f units away)\n", size,
        pr.x, pr.y, pf.x, pf.y, pw.x, pw.y, glm::distance(p.eye, wallHit(p.front, u)));
    if(s.muzzleValid[hand])
    {
        const glm::vec3 shot = hands::forward(weapons::shotAngles(s.rot[hand], p.slot, hand == HAND_OFF));
        const glm::vec3 hit = wallHit(s.muzzle[hand], shot);
        const glm::vec2 ph = pixel(hit);
        Con_Printf("sightcheck pixels (%g): the laser at the wall %.1f %.1f: %.1f px from the sight line's, %.2f cm off it there\n",
            size, ph.x, ph.y, glm::distance(ph, pw), unitsToCm(lineDistance(hit, p.front, u)));
        za::String ranges;
        for(const float m : {2.f, 5.f, 10.f, 20.f, 50.f})
        {
            const glm::vec3 at = s.muzzle[hand] + shot * (m * units::metresToUnits());
            ranges += va(" %gm %.2fcm (%.3f deg)", m, unitsToCm(lineDistance(at, p.front, u)),
                glm::degrees(za::atan2(lineDistance(at, p.front, u), glm::distance(at, p.eye))));
        }
        Con_Printf("sightcheck laser vs sight line %.4f deg; the laser off the sight line at%s\n", degreesBetween(shot, u),
            ranges.cStr());
        Con_Printf("sightcheck muzzle %.3f %.3f %.3f shot %.5f %.5f %.5f\n", s.muzzle[hand].x, s.muzzle[hand].y, s.muzzle[hand].z,
            shot.x, shot.y, shot.z);
    }
    {
        // The shots' angles that would follow the sights (in the hand's aim frame), and through the whole offset's turn.
        const glm::vec3 inHand = glm::normalize(glm::transpose(p.handBasis) * u);
        glm::vec3 wp, wa;
        wholeFor(p.slot, hand, wp, wa);
        const glm::vec3 inC = glm::transpose(wholeTurn(wa)) * (glm::transpose(p.cBasis) * u);
        Con_Printf("sightcheck sights in the hand's frame: pitch %.4f yaw %.4f (via C and the whole turn: %.4f %.4f); shot keys %.4f %.4f\n",
            glm::degrees(za::asin(inHand.z)), glm::degrees(za::atan2(inHand.y, inHand.x)), glm::degrees(za::asin(inC.z)),
            glm::degrees(za::atan2(inC.y, inC.x)), weapons::value(p.slot, Key::ShotPitch), weapons::value(p.slot, Key::ShotYaw));
    }
}

void lines_f()
{
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        const char* id = weapons::cvar(slot, Key::ID)->string;
        if(!id[0] || !strcmp(id, "-1"))
        {
            continue;
        }
        qmodel_t* m = Mod_ForName(id, false);
        const SightLine l = sightLine(m);
        if(!l.valid)
        {
            Con_Printf("%-22s none\n", id);
            continue;
        }
        const glm::vec3 u = glm::normalize(l.front - l.rear);
        Con_Printf("%-22s %s rear %.2f %.2f %.2f front %.2f %.2f %.2f (%.2f deg up, %.2f left of the model's x)\n", id,
            l.painted ? "sights" : "table ", l.rear.x, l.rear.y, l.rear.z, l.front.x, l.front.y, l.front.z,
            glm::degrees(za::asin(u.z)), glm::degrees(za::atan2(u.y, u.x)));
    }
}

void align_f()
{
    const char* what = Cmd_Argc() >= 2 ? Cmd_Argv(1) : "start";
    const int hand = Cmd_Argc() >= 3 && !q_strcasecmp(Cmd_Argv(2), "off") ? HAND_OFF : HAND_MAIN;
    if(!q_strcasecmp(what, "start"))
    {
        start(hand, key_dest == key_menu ? menu::currentPage() : -1); // from the menu: back to it with the result
    }
    else if(!q_strcasecmp(what, "apply"))
    {
        apply();
    }
    else if(!q_strcasecmp(what, "cancel"))
    {
        cancel();
    }
    else if(!q_strcasecmp(what, "undo"))
    {
        undo();
    }
    else
    {
        Con_Printf("usage: vr_sight_align [start [main|off] | apply | cancel | undo]\n");
    }
}

} // namespace

// ---------------------------------------------------------------------------------------------------------------------

SightLine sightLine(const qmodel_t* model)
{
    if(!model || model->type != mod_alias)
    {
        return {};
    }
    const za::String name = model->name;
    if(const auto it = lines.find(name); it != lines.end())
    {
        return it->second;
    }
    SightLine line;
    bool known = false;
    for(const char* m : melee)
    {
        known = known || !q_strcasecmp(m, name.cStr());
    }
    for(const Painted& p : painted)
    {
        if(!known && !q_strcasecmp(p.model, name.cStr()))
        {
            line = fromTexels(p.model, p.rear);
            known = true;
            if(!line.valid)
            {
                Con_DPrintf("Align Sights: no sights found on %s's skin\n", p.model);
            }
        }
    }
    for(const Table& t : table)
    {
        if(!line.valid && !q_strcasecmp(t.model, name.cStr()))
        {
            line.valid = true;
            line.painted = false;
            line.rear = t.rear;
            line.front = t.front;
        }
    }
    lines[name] = line;
    return line;
}

void resetCaches()
{
    lines.clear();
}

void init()
{
    Cmd_AddCommand("vr_sight_align", align_f);
    Cmd_AddCommand("vr_sight_check", check_f);
    Cmd_AddCommand("vr_sight_lines", lines_f);
}

Phase phase()
{
    return st.phase;
}

bool alignable(int hand)
{
    const qmodel_t* m = weapons::heldModel(hand);
    return m && sightLine(m).valid;
}

bool start(int hand, int returnPage)
{
    const hands::State& s = hands::current();
    Pose p;
    if(!vrActive() || cls.state != ca_connected)
    {
        Con_Printf("Align Sights: in a game, with the headset\n");
        return false;
    }
    if(!poseOf(s, hand, p))
    {
        Con_Printf("Align Sights: hold a gun with sights in the %s hand\n", handName(hand));
        return false;
    }
    if(key_dest == key_menu)
    {
        menuui::backToGame(hand);
    }
    Cbuf_AddText("-attack\n-offhandattack\n");
    st = State{};
    st.phase = Phase::Capturing;
    st.step = Step::Countdown;
    st.hand = hand;
    st.returnPage = returnPage;
    st.world = worldGeneration();
    st.model = p.model;
    st.slot = p.slot;
    st.started = st.lastEvent = realtime;
    st.countdown.start(realtime);
    st.wanted = CLAMP(3, static_cast<int>(vr_sight_align_captures.value), 5);
    if(hasResult && !applied)
    {
        hasResult = false; // a pending result is dropped
    }
    Con_Printf("Align Sights: %s, %s hand, dominant eye %s: %d captures. Close your eyes and lower the gun; at the high "
               "beep raise it as you would your own, and hold it still.\n",
        modelBase(p.model).cStr(), handName(hand), dominantEye() == 1 ? "right" : "left", st.wanted);
    bump();
    return true;
}

void apply()
{
    if(!hasResult || applied)
    {
        return;
    }
    const Result& r = result;
    if(weapons::slotForModel(weapons::heldModel(r.hand)) != r.slot)
    {
        Con_Printf("Align Sights: the %s hand holds another weapon now\n", handName(r.hand));
        return;
    }
    undoList.clear();
    for(int i = 0; i < r.count; i++)
    {
        setEffective(r.slot, keys[i], r.values[i]);
    }
    const int owner = weapons::ownerSlot(r.slot, Key::WholePitch);
    {
        const char* id = weapons::cvar(owner, Key::ID)->string;
        const char* base = strrchr(id, '/');
        savedTo = za::String(base ? base + 1 : id) + (owner == r.slot ? "" : " (inherited)");
    }
    applied = true;
    st.phase = Phase::Idle;
    Con_Printf("Align Sights: applied to %s\n", savedTo.cStr());
    sound("misc/menu2.wav");
    bump();
}

void cancel()
{
    if(st.phase == Phase::Capturing)
    {
        stopCapture("stopped", false);
    }
    if(hasResult && !applied)
    {
        hasResult = false;
        st.phase = Phase::Idle;
        Con_Printf("Align Sights: cancelled, nothing changed\n");
        bump();
    }
}

bool canUndo()
{
    return applied && !undoList.empty();
}

void undo()
{
    if(!canUndo())
    {
        return;
    }
    for(za::SizeT k = undoList.size(); k-- > 0;) // (the last first)
    {
        Cvar_Set(undoList[k].cvar->name, undoList[k].before.cStr());
    }
    undoList.clear();
    applied = false;
    hasResult = false;
    Con_Printf("Align Sights: undone, the values before are back\n");
    sound("misc/menu3.wav");
    bump();
}

int version()
{
    return versionCounter;
}

const char* statusLine(int i)
{
    int n = 0;
    const auto add = [&](const za::String& s) {
        if(n < 8)
        {
            lineBuf[n++] = s;
        }
    };
    const int hand = st.phase == Phase::Idle && !hasResult ? -1 : st.hand;
    (void)hand;
    if(st.phase == Phase::Capturing)
    {
        add(va("Capturing: %d of %d taken", static_cast<int>(st.captures.size()), st.wanted));
    }
    else if(hasResult)
    {
        const Result& r = result;
        add(va("Sights were off %.1f deg, %.1f cm", r.offDegrees, r.offCm));
        add(r.dropped ? va("%d captures (%d dropped): %.2f deg", r.used, r.dropped, r.spreadDegrees)
                      : va("%d captures, %.2f deg apart", r.used, r.spreadDegrees));
        add(va("Fix: turn %.1f deg, move %.1f cm", r.turnDegrees, r.moveCm));
        add(va("Shots turned %.2f deg", r.shotDegrees));
        if(r.count > 8)
        {
            add(va("Two hands: %.1f deg jump now 0", r.twoHandDegrees));
        }
        if(applied)
        {
            add("Applied to " + savedTo);
        }
        else
        {
            add("Green: new line, orange: old");
            const int owner = weapons::ownerSlot(r.slot, Key::WholePitch);
            if(owner != r.slot)
            {
                // Inherit From: the page edits the settings inherited, so Apply does too (the other ammo's model shares
                // them); a weapon that should keep its own is made to Stop Inheriting first.
                const char* id = weapons::cvar(owner, Key::ID)->string;
                const char* base = strrchr(id, '/');
                add(va("Changes %s's (inherited)", base ? base + 1 : id));
            }
        }
    }
    else if(!st.message.empty())
    {
        add("Last try: " + st.message);
    }
    return i >= 0 && i < n ? lineBuf[i].cStr() : nullptr;
}

void frame()
{
    if(st.phase != Phase::Capturing)
    {
        return;
    }
    if(key_dest == key_menu || st.world != worldGeneration() || cls.state != ca_connected || cl.intermission || !vrActive() ||
        cl.stats[STAT_HEALTH] <= 0)
    {
        stopCapture("stopped (the menu, the map or the game changed)", false);
        return;
    }
    const double now = realtime;
    if(st.step == Step::Countdown)
    {
        if(st.countdown.update(now)) // (the go beep: raise it)
        {
            st.step = Step::WaitRaise;
            st.refSet = false;
            st.lastEvent = now;
        }
    }
    else if(now - st.lastEvent > giveUpSeconds)
    {
        if(static_cast<int>(st.captures.size()) >= 3)
        {
            solve();
            sound("misc/talk.wav");
            stopCapture(nullptr, true);
        }
        else
        {
            sound("misc/menu3.wav");
            stopCapture("no still aim for 45 s", true);
        }
        return;
    }

    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    za::String text = va("%c%c%c%c%c%c%c%c%c%c%c%c%c %s", 'A' | 0x80, 'L' | 0x80, 'I' | 0x80, 'G' | 0x80, 'N' | 0x80, ' ' | 0x80,
        'S' | 0x80, 'I' | 0x80, 'G' | 0x80, 'H' | 0x80, 'T' | 0x80, 'S' | 0x80, ':' | 0x80, modelBase(st.model).cStr());
    text += va("\n%d of %d taken\n", static_cast<int>(st.captures.size()), st.wanted);
    text += st.step == Step::Countdown   ? va("close your eyes, lower the gun: %d", st.countdown.remaining(now))
            : st.step == Step::WaitRaise ? za::String(st.captures.empty() ? "raise it as you would your own, hold still"
                                                                           : "lower it, then raise it again")
                                         : za::String("hold still...");
    text += "\nmenu button: stop";
    const float m2u = units::metresToUnits();
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
    const glm::vec3 at = s.head + fwd * (0.8f * m2u) + glm::vec3{0.f, 0.f, 0.25f * m2u};
    text3d::queueOverlay(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, 0.045f);
}

namespace
{
int waitDebugFrames = 0; // viewFrame's waiting frames (developer 2 prints one in 20)
} // namespace

void viewFrame(const hands::State& s)
{
    // Show Sight Line: each held gun's.
    if(vr_show_sight_line.value)
    {
        for(int hand = 0; hand < 2; hand++)
        {
            Pose p;
            if(poseOf(s, hand, p))
            {
                drawLine(p.rear, p.front, {1.f, 1.f, 1.f, 0.8f}, 0.06f, true);
            }
        }
    }

    // The result: the sight line as it is (orange) and as the fix puts it (green), and the eye captured.
    if(hasResult && !applied && st.phase != Phase::Capturing)
    {
        Pose p;
        if(poseOf(s, result.hand, p) && p.model == result.model)
        {
            const Result& r = result;
            const auto moved = [&](const glm::vec3& c) { return fromC(p, r.q * (c - r.fist) + r.fist + r.t); };
            // The weapon's points as drawn now (in C), moved by the fix.
            const glm::vec3 rearC = toC(p, p.rear), frontC = toC(p, p.front);
            drawLine(p.rear, p.front, {1.f, 0.55f, 0.1f, 0.8f}, 0.05f, false);
            drawLine(moved(rearC), moved(frontC), {0.2f, 1.f, 0.3f, 0.9f}, 0.06f, true);
            lines::point(fromC(p, r.eye), 0.25f, {1.f, 0.3f, 1.f, 0.9f});
        }
    }

    if(st.phase != Phase::Capturing || st.step == Step::Countdown)
    {
        st.window.clear();
        return;
    }
    Pose p;
    if(!poseOf(s, st.hand, p) || p.model != st.model)
    {
        sound("misc/menu3.wav");
        stopCapture("the weapon changed", true);
        return;
    }
    const double now = realtime;
    Sample smp;
    smp.time = now;
    smp.eye = toC(p, p.eye);
    smp.rear = toC(p, p.rear);
    smp.front = toC(p, p.front);
    smp.fist = toC(p, p.fist);
    smp.gaze = glm::transpose(p.cBasis) * p.gaze;
    smp.gripValid = p.gripValid;
    if(p.gripValid)
    {
        // (forward, right, up): hands::redirect's, as the two-handed aim offset is applied.
        const glm::vec3 d = p.grip - p.handPos;
        smp.grip = {glm::dot(d, p.handBasis[0]), -glm::dot(d, p.handBasis[1]), glm::dot(d, p.handBasis[2])};
    }
    smp.sightInHand = glm::normalize(glm::transpose(p.handBasis) * (p.front - p.rear));
    smp.handPos = p.cPos;
    smp.handFwd = p.cBasis[0];
    if(!st.window.empty() && st.window.lastTime() == now)
    {
        return; // the second eye's pass
    }
    const glm::vec3 channels[] = {smp.eye, smp.handPos, smp.handFwd, smp.rear, smp.front, smp.fist, smp.sightInHand, smp.gaze,
        smp.grip};
    st.window.add(now, channels, stillSeconds + 0.25);
    st.lastGripValid = smp.gripValid;

    if(st.step == Step::WaitRaise)
    {
        if(!st.refSet)
        {
            st.refPos = p.cPos;
            st.refFwd = p.cBasis[0];
            st.refSet = true;
        }
        if(moved(p.cPos, p.cBasis[0]))
        {
            st.step = Step::WaitStill;
            st.window.clear();
        }
        return;
    }

    // WaitStill.
    Sample mean;
    const bool still = stillMean(now, mean);
    if(developer.value >= 2 && ++waitDebugFrames % 20 == 0)
    {
        Con_Printf("Align Sights: waiting: still %d, plausible %d, two-handed %.2f, moves %.3f %.3f %.3f %.3f\n", still ? 1 : 0,
            still && plausible(mean) ? 1 : 0, twohand::transition(st.hand), lastDev.x, lastDev.y, lastDev.z, lastDev.w);
        const glm::vec3 u = glm::normalize(smp.front - smp.rear);
        Con_Printf("Align Sights:   now: %.2f deg off the eye's ray, the eye %.2f cm off the line, hand %.1f %.1f %.1f\n",
            degreesBetween(u, smp.front - smp.eye), unitsToCm(lineDistance(smp.eye, smp.rear, u)), p.cPos.x, p.cPos.y, p.cPos.z);
    }
    if(!still || !plausible(mean))
    {
        return;
    }
    // Two-handed aiming turns the gun from the other hand: capture it held in one hand.
    if(twohand::transition(st.hand) > 0.f)
    {
        return;
    }
    st.captures.pushBack({mean});
    st.lastEvent = now;
    st.window.clear();
    const Sample& c = mean;
    const glm::vec3 u = glm::normalize(c.front - c.rear);
    Con_Printf("Align Sights: capture %d: %.2f deg off the eye's ray, the eye %.2f cm off the line\n",
        static_cast<int>(st.captures.size()), degreesBetween(u, c.front - c.eye), unitsToCm(lineDistance(c.eye, c.rear, u)));
    bump();
    if(static_cast<int>(st.captures.size()) >= st.wanted)
    {
        solve();
        sound("misc/talk.wav");
        st.phase = Phase::Result;
        stopCapture(nullptr, true);
        return;
    }
    sound("weapons/pkup.wav");
    st.step = Step::WaitRaise;
    st.refPos = mean.handPos;
    st.refFwd = mean.handFwd;
    st.refSet = true;
}

} // namespace qvr::sightalign
