// vr_rigstudio_aimline.cpp -- the Rig Studio's aim lines and Align to Target (RIGSTUDIO.md, "Aim lines and Align to
// Target"). For each weapon the subject holds (both for two guns), as drawn: its barrel's line (the model's butt to its
// muzzle, as the aim test measures it) from the muzzle on, the line the game's shot would take when that is another
// (vr_agrunt_aim_converge: from the muzzle to the target; else the drawn gun's line, vr_agrunt_shot_drawn, or the hidden
// gun's), a thin line from the muzzle to the aim target, and the error: yaw (+ left), pitch (+ up), total. Tools >
// Display > Aim Lines (on: drawn while the aim target is on).
//
// Align to Target (Tools > Aim Target, the Pose Studio Data window beside the key; vr_rigstudio_align): the current pose
// key's weapon turns (RTurn; LTurn for the left of two guns) changed so the drawn barrels point at the target, solved
// through the game's chain (the key previewed -> QC's pose -> the arms' IK -> the drawn weapon): each step the barrel's
// turn onto the target laid on the key's turn (its body's space), the drawn result read again once it settles, until
// every error is under 0.05 degrees (the key is then kept, its turns rounded to 0.1 as every key is). One undo step.
// Its place (RPos, LPos) as it was, unless Also Move: then moved so each muzzle stays where it was (the gun turned about
// its muzzle, not its grip). A weapon's own grip is no tool for its aim: the weapon is drawn on the pose's line and its
// grip only sets the hand on it (agarms: the hand solved onto the weapon's frame), so the key is the only one.
//
// vr_rigstudio_test_align: for the grunt and the enforcer with the shotgun (Rifle), the nail pistol (Pistol) and the
// rocket launcher (Launcher), and the grunt with two nail pistols: the key turned 10 degrees off in yaw and 5 in pitch,
// aligned (its drawn error under 0.2 degrees), undone (the key exactly as misaligned), put back. Ends on
// `rigstudio align: N passed, M failed`.

#include "vr_rigstudio_internal.hpp"
#include "vr_aggrip.hpp"
#include "vr_agpose.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_render.hpp"
#include "vr_view.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Asin.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <string.h>

using namespace qvr;
using namespace qvr::rigstudio::detail;

namespace
{

constexpr float deg = 3.14159265f / 180.f;
constexpr float alignDone = 0.05f;     // degrees: every barrel this near its target: kept
constexpr int alignMaxSteps = 16;      // steps before it gives up (keeps the best)
constexpr int alignSettleMax = 150;    // frames a step waits for the drawn weapon to settle (then reads it anyway)
constexpr float settleTurn = 0.003f;   // degrees a frame: settled
constexpr float settleMove = 0.003f;   // units a frame
constexpr int settleFrames = 3;        // ... for this many frames

struct SvVm
{
    qcvm_t* old{nullptr};
    SvVm() { PR_PushQCVM(&sv.qcvm, &old); }
    ~SvVm() { PR_PopQCVM(old); }
};

[[nodiscard]] ImU32 colU(const glm::vec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4{c.x, c.y, c.z, c.w});
}

// A text on a dark backing (readable over a bright room), kept inside the view (`left`: ending at x instead).
void textBox(ImDrawList* dl, float x, float y, const glm::vec4& c, const char* text, bool left = false)
{
    const ImVec2 sz = ImGui::CalcTextSize(text);
    if(left || x + sz.x > st.view.x + st.view.w - 4.f)
    {
        x = za::max(st.view.x + 4.f, (left ? x : st.view.x + st.view.w - 4.f) - sz.x);
    }
    dl->AddRectFilled(ImVec2{x - 3.f, y - 1.f}, ImVec2{x + sz.x + 3.f, y + sz.y + 1.f}, colU(glm::vec4{0.f, 0.f, 0.f, 0.65f}), 3.f);
    dl->AddText(ImVec2{x, y}, colU(c), text);
}

// (atan2 of the cross and the dot: acos near 1 is no good under a tenth of a degree in floats, 0.02 at the least)
[[nodiscard]] float angleBetween(const glm::vec3& a, const glm::vec3& b)
{
    return za::atan2(glm::length(glm::cross(a, b)), glm::dot(a, b)) / deg;
}

[[nodiscard]] float wrap180(float a)
{
    return a - 360.f * za::floor((a + 180.f) / 360.f);
}

// A key's turn (a rotation vector: its axis, its length in degrees) and back.
[[nodiscard]] glm::quat fromTurn(const glm::vec3& v)
{
    const float a = glm::length(v);
    return a < 1e-6f ? glm::quat{1.f, 0.f, 0.f, 0.f} : glm::angleAxis(a * deg, v / a);
}

[[nodiscard]] glm::vec3 toTurn(glm::quat q)
{
    q = glm::normalize(q);
    if(q.w < 0.f)
    {
        q = -q;
    }
    const float s = za::sqrt(za::max(0.f, 1.f - q.w * q.w));
    if(s < 1e-7f)
    {
        return glm::vec3{0.f};
    }
    const float a = 2.f * za::acos(za::clamp(q.w, -1.f, 1.f));
    return glm::vec3{q.x, q.y, q.z} / s * (a / deg);
}

// The rotation taking unit `from` onto unit `to` (the shortest).
[[nodiscard]] glm::quat between(const glm::vec3& from, const glm::vec3& to)
{
    const glm::vec3 c = glm::cross(from, to);
    const float s = glm::length(c);
    if(s < 1e-7f)
    {
        return glm::quat{1.f, 0.f, 0.f, 0.f};
    }
    return glm::angleAxis(za::atan2(s, glm::dot(from, to)), c / s);
}

[[nodiscard]] bool fresh()
{
    return st.subject > 0 && st.noteFrame >= 0 && host_framecount - st.noteFrame <= 5 && st.note.ent == st.subject;
}

// The subject's body space (QC VR_AGP_Body: its yaw) in the world.
[[nodiscard]] glm::quat bodyYaw()
{
    return glm::angleAxis(st.note.space.yawDeg * deg, glm::vec3{0.f, 0.f, 1.f});
}

// The Align's run (one at a time).
struct Align
{
    int phase{0};       // 0 idle, 1 a step settling, 2 the kept key settling (the last reading)
    int code{-1};
    bool hand[2]{false, false};
    bool move{false};
    agpose::Key key;    // the key being solved (previewed)
    agpose::Key best;
    float bestErr{1e9f}; // (its error; Also Move: plus the muzzles' drift)
    float gain{1.f};     // a step's share of the correction (halved each time a step made it worse)
    bool reread{false};  // the best key put back: read again before the next step
    // Also Move's place steps: one under way (`trial`), the aligned key and drift before it; still trying
    bool trial{false}, placing{true};
    agpose::Key preKey;
    float preDrift{0.f};
    float drift0{0.f}, drift{0.f}; // the muzzles' drift once first aligned, and now (units)
    glm::vec3 muzzle0[2]{glm::vec3{0.f}, glm::vec3{0.f}}; // (Also Move: where each muzzle stays)
    int steps{0}, waited{0}, still{0};
    bool have{false};
    glm::vec3 lastBarrel[2]{glm::vec3{0.f}, glm::vec3{0.f}}, lastMuzzle[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    float err[2]{0.f, 0.f};
    bool ok{false};     // the last run's result: under 0.1 degrees after keeping
    char status[200]{};
};
Align al;

// The subject's weapons as last drawn (rigstudio::drawnMatrix): each one's entity, frame and matrix (its model's
// vertices -> the world).
struct DrawnWeapon
{
    int num{0};
    int frame{-1};
    glm::mat4 m{1.f};
};
DrawnWeapon drawnWeapons[2];

// Weapon entity `num`'s drawn matrix from its model's own frame (x forward, y left, z up, units: weaponLocalGeometry's)
// to the world: the drawn matrix without the model's vertex scale and origin (the .mdl's packing). False: not drawn in
// the last two frames.
[[nodiscard]] bool drawnWeapon(int num, glm::mat4& out)
{
    for(const DrawnWeapon& d : drawnWeapons)
    {
        if(d.num == num && d.frame >= 0 && host_framecount - d.frame <= 2 && cl_entities[num].model)
        {
            // (the model's own frame: its drawing at the origin unturned, every model transform in it, as
            // weaponLocalGeometry measures it; the drawn matrix after it)
            entity_t at{};
            at.model = cl_entities[num].model;
            at.scale = ENTSCALE_DEFAULT;
            float m0[16];
            render::entityMatrix(at, false, ENTSCALE_DEFAULT, glm::vec3{0.f}, m0);
            glm::mat4 f0;
            for(int c = 0; c < 4; c++)
            {
                f0[c] = glm::vec4{m0[c * 4], m0[c * 4 + 1], m0[c * 4 + 2], m0[c * 4 + 3]};
            }
            out = d.m * glm::inverse(f0);
            return true;
        }
    }
    return false;
}

} // namespace

void qvr::rigstudio::drawnMatrix(const entity_t* e, const float* matrix)
{
    if(!st.open || !cl_entities || e < cl_entities || e >= cl_entities + cl_max_edicts)
    {
        return;
    }
    const int num = static_cast<int>(e - cl_entities);
    for(int h = 0; h < 2; h++)
    {
        if(num > 0 && num == (h == 0 ? st.note.prop : st.note.propL))
        {
            DrawnWeapon& d = drawnWeapons[h];
            d.num = num;
            d.frame = host_framecount;
            for(int c = 0; c < 4; c++)
            {
                d.m[c] = glm::vec4{matrix[c * 4], matrix[c * 4 + 1], matrix[c * 4 + 2], matrix[c * 4 + 3]};
            }
        }
    }
}

namespace qvr::rigstudio::detail
{

bool readAimLine(int hand, AimLineReading& r)
{
    r = AimLineReading{};
    if(hand < 0 || hand > 1 || !fresh())
    {
        return false;
    }
    const posestudio::Note& n = st.note;
    const int prop = hand == 0 ? n.prop : n.propL;
    if(prop <= 0 || prop >= cl.num_entities || !cl_entities[prop].model)
    {
        return false;
    }
    glm::vec3 muzzle, butt, second;
    int secondType = 0;
    glm::mat4 w;
    if(!view::weaponLocalGeometry(cl_entities[prop].model, muzzle, butt, second, secondType) || !drawnWeapon(prop, w))
    {
        return false;
    }
    // The weapon as drawn last (its model matrix, the renderer's own: every layer, lerp, mirror and rig included): its
    // muzzle, and its barrel along its model's forward (x)
    r.muzzle = glm::vec3{w * glm::vec4{muzzle, 1.f}};
    r.origin = glm::vec3{w * glm::vec4{0.f, 0.f, 0.f, 1.f}};
    r.barrel = glm::normalize(glm::vec3{w * glm::vec4{1.f, 0.f, 0.f, 0.f}});
    r.length = glm::length(glm::vec3{w * glm::vec4{muzzle.x, 0.f, 0.f, 0.f}});
    r.dist = glm::length(st.aim.at - r.muzzle);
    if(r.dist < 1.f)
    {
        return false;
    }
    r.toTarget = (st.aim.at - r.muzzle) / r.dist;
    r.total = angleBetween(r.barrel, r.toTarget);
    r.yaw = wrap180((za::atan2(r.barrel.y, r.barrel.x) - za::atan2(r.toTarget.y, r.toTarget.x)) / deg);
    r.pitch = (za::asin(za::clamp(r.barrel.z, -1.f, 1.f)) - za::asin(za::clamp(r.toTarget.z, -1.f, 1.f))) / deg;
    // The game's shot (QC VR_AG_SetHand): converging, from the muzzle to its target; else along the drawn gun's line
    // (vr_agrunt_shot_drawn) or the hidden gun's (its entity's angles, the server's)
    if(vr_agrunt_aim_converge.value != 0.f)
    {
        r.shot = r.toTarget;
        r.shotWhy = "converge: muzzle to target";
        r.shotOk = true;
    }
    else if(vr_agrunt_shot_drawn.value != 0.f)
    {
        r.shot = r.barrel;
        r.shotWhy = "the drawn gun's line";
        r.shotOk = true;
    }
    else if(sv.active && prop < sv.qcvm.num_edicts)
    {
        const SvVm vm;
        const edict_t* p = EDICT_NUM(prop);
        if(!p->free)
        {
            const float pp = p->v.angles[0] * deg, py = p->v.angles[1] * deg; // (a model's pitch: up)
            r.shot = glm::vec3{za::cos(pp) * za::cos(py), za::cos(pp) * za::sin(py), za::sin(pp)};
            r.shotWhy = "the hidden gun's line";
            r.shotOk = true;
        }
    }
    r.shotOff = r.shotOk ? angleBetween(r.shot, r.barrel) : 0.f;
    r.ok = true;
    return true;
}

int alignKeyCode()
{
    if(st.subjKind < 0 || st.subjClass < 0 || !agpose::isGunClass(st.subjClass))
    {
        return -1; // (only guns aim)
    }
    if(st.tool == Tool::PoseKeys && st.pdKind == st.subjKind && st.pdClass == st.subjClass)
    {
        return agpose::code(st.pdKind, st.pdClass, st.pdPose, agpose::isStrike(st.pdPose) ? st.pdPhase : 0);
    }
    return st.aim.on ? agpose::code(st.subjKind, st.subjClass, agpose::Aim, 0) : -1;
}

void sharedWith(int code, char* out, int size)
{
    out[0] = '\0';
    if(code < 0)
    {
        return;
    }
    const int kind = agpose::kindOf(code);
    const int cls = agpose::classOf(code);
    if(cls == agpose::TwoGuns)
    {
        q_strlcpy(out, "this key aims every pair of guns", static_cast<size_t>(size));
        return;
    }
    int n = q_snprintf(out, static_cast<size_t>(size), "this key also aims:");
    int count = 0;
    for(int r = 0; r < agholds::count() && n < size - 40; r++)
    {
        const int c = agholds::code(r);
        if(!agholds::isGun(r) || c == st.made.weapon || agholds::poseClass(kind, c, nullptr) != cls)
        {
            continue;
        }
        n += q_snprintf(out + n, static_cast<size_t>(size - n), "%s %s", count ? "," : "", agholds::label(r));
        count++;
    }
    if(count == 0)
    {
        q_snprintf(out, static_cast<size_t>(size), "this key aims no other weapon of the %s's", agpose::kindName(kind));
    }
}

bool alignBusy()
{
    return al.phase != 0;
}

const char* alignStatus()
{
    return al.status;
}

bool alignStart(bool alsoMove)
{
    const int code = alignKeyCode();
    if(al.phase != 0)
    {
        q_strlcpy(al.status, "align: already under way", sizeof(al.status));
        return false;
    }
    if(code < 0)
    {
        q_strlcpy(al.status, "align: no gun key to align (a gun held, the aim target on)", sizeof(al.status));
        return false;
    }
    if(st.dragging)
    {
        q_strlcpy(al.status, "align: a drag under way", sizeof(al.status));
        return false;
    }
    AimLineReading r[2];
    const bool h0 = readAimLine(0, r[0]);
    const bool h1 = readAimLine(1, r[1]);
    if(!h0)
    {
        q_strlcpy(al.status, "align: the subject's weapon not drawn (its arms not solved)", sizeof(al.status));
        return false;
    }
    al = Align{};
    al.code = code;
    al.hand[0] = true;
    al.hand[1] = h1 && agpose::classOf(code) == agpose::TwoGuns; // (the left gun's turn: two guns)
    al.move = alsoMove;
    al.key = agpose::value(code);
    al.best = al.key;
    for(int h = 0; h < 2; h++)
    {
        al.muzzle0[h] = r[h].muzzle;
    }
    pushPoseUndo(code); // (one undo step: the key as it was)
    agpose::setPreview(code, al.key);
    al.phase = 1;
    q_snprintf(al.status, sizeof(al.status), "align: key %d, solving", code);
    return true;
}

void alignTick()
{
    if(al.phase == 0)
    {
        return;
    }
    if(!st.open || !fresh())
    {
        agpose::clearPreview();
        al.phase = 0;
        q_strlcpy(al.status, "align: stopped (the subject gone); the key as it was", sizeof(al.status));
        return;
    }
    // Settled: each barrel and muzzle still for a few frames
    AimLineReading r[2];
    bool still = al.have;
    for(int h = 0; h < 2; h++)
    {
        if(!al.hand[h])
        {
            continue;
        }
        if(!readAimLine(h, r[h]))
        {
            still = false;
            continue;
        }
        if(al.have)
        {
            still = still && angleBetween(r[h].barrel, al.lastBarrel[h]) < settleTurn && glm::length(r[h].muzzle - al.lastMuzzle[h]) < settleMove;
        }
        al.lastBarrel[h] = r[h].barrel;
        al.lastMuzzle[h] = r[h].muzzle;
    }
    al.have = true;
    al.still = still ? al.still + 1 : 0;
    al.waited++;
    if(!(al.still >= settleFrames && al.waited >= settleFrames + 2) && al.waited < alignSettleMax)
    {
        return;
    }
    float worst = 0.f;
    for(int h = 0; h < 2; h++)
    {
        al.err[h] = al.hand[h] && r[h].ok ? r[h].total : 0.f;
        worst = za::max(worst, al.err[h]);
    }
    if(al.phase == 2)
    {
        // The kept key (rounded) as drawn: the result
        al.phase = 0;
        al.ok = worst < 0.1f;
        q_snprintf(al.status, sizeof(al.status), "align: key %d kept in %d steps%s: right %.3f deg, left %s%.3f deg%s", al.code,
            al.steps, al.move ? " (moved too)" : "", al.err[0], al.hand[1] ? "" : "(none) ", al.err[1], al.ok ? "" : " (NOT under 0.1)");
        Con_Printf("rigstudio align: done: key %d, %d steps, right %.3f deg, left %.3f deg%s\n", al.code, al.steps, al.err[0],
            al.err[1], al.ok ? "" : " (not under 0.1)");
        return;
    }
    // (Also Move: how far the muzzles are from where they were)
    float drift = 0.f;
    for(int h = 0; h < 2 && al.move; h++)
    {
        drift = za::max(drift, al.hand[h] && r[h].ok ? glm::length(al.muzzle0[h] - r[h].muzzle) : 0.f);
    }
    al.drift = drift;
    if(al.move && worst < alignDone)
    {
        // Aligned. Also Move tries a place step (the muzzles put back), re-aims, and keeps it only if the muzzles came
        // nearer: a shouldered gun's place turns it about its stock far more than it moves it (the grunt's shotgun: 3.5
        // units of place, 12.8 degrees of aim and 0.45 units of muzzle), so it may not
        if(al.trial && drift > al.preDrift - 0.05f)
        {
            al.key = al.preKey; // (the place step did not help: the aligned key before it)
            al.drift = al.preDrift;
            al.placing = false;
        }
        else if(al.placing && drift >= 0.1f && al.steps < alignMaxSteps * 2)
        {
            if(!al.trial)
            {
                al.drift0 = drift;
            }
            al.trial = true;
            al.preKey = al.key;
            al.preDrift = drift;
            const glm::quat yi = glm::inverse(bodyYaw());
            for(int h = 0; h < 2; h++)
            {
                if(al.hand[h] && r[h].ok)
                {
                    al.key.v[h == 0 ? agpose::RPos : agpose::LPos] += yi * (al.muzzle0[h] - r[h].muzzle);
                }
            }
            agpose::setPreview(al.code, al.key);
            al.steps++;
            al.waited = 0;
            al.still = 0;
            al.have = false;
            return;
        }
        if(!al.trial)
        {
            al.drift0 = drift;
        }
        al.best = al.key;
        al.bestErr = worst;
    }
    else if(worst < al.bestErr && !al.trial)
    {
        al.bestErr = worst;
        al.best = al.key;
    }
    else if(!al.move && !al.reread && al.steps > 0)
    {
        // Worse than the best: back to it and read it again, the next steps half as long (an arm at its reach, a
        // wrist at its most: the chain is not the straight rotation the step assumes)
        al.key = al.best;
        al.gain *= 0.5f;
        al.reread = true;
        agpose::setPreview(al.code, al.key);
        al.steps++;
        al.waited = 0;
        al.still = 0;
        al.have = false;
        return;
    }
    al.reread = false;
    if(worst < alignDone || al.steps >= (al.move ? alignMaxSteps * 3 : alignMaxSteps))
    {
        if(al.trial && worst >= alignDone)
        {
            al.best = al.preKey; // (a place step's re-aim never got there: the aligned key before it)
        }
        agpose::set(al.code, al.best); // (rounded as every key is)
        agpose::clearPreview();
        al.phase = 2;
        al.waited = 0;
        al.still = 0;
        al.have = false;
        return;
    }
    // A step: each barrel's turn onto its target laid on its turn (the key's turn is QC's rotation of the procedural
    // gun in its body's space: world = Y * key * Y^-1, so key' = Y^-1 * C * Y * key)
    const glm::quat y = bodyYaw();
    const glm::quat yi = glm::inverse(y);
    for(int h = 0; h < 2; h++)
    {
        if(al.hand[h] && r[h].ok)
        {
            const int turn = h == 0 ? agpose::RTurn : agpose::LTurn;
            const glm::quat c = glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, between(r[h].barrel, r[h].toTarget), al.gain);
            al.key.v[turn] = toTurn(yi * c * y * fromTurn(al.key.v[turn]));
        }
    }
    agpose::setPreview(al.code, al.key);
    al.steps++;
    al.waited = 0;
    al.still = 0;
    al.have = false;
    q_snprintf(al.status, sizeof(al.status), "align: key %d, step %d: right %.3f deg, left %.3f deg", al.code, al.steps, al.err[0], al.err[1]);
}

void alignCancel()
{
    if(al.phase == 1)
    {
        agpose::clearPreview();
    }
    al.phase = 0;
}

bool alignResult(float* errR, float* errL)
{
    *errR = al.err[0];
    *errL = al.err[1];
    return al.ok;
}

// Also Move's muzzle drift: once first aligned (turn alone) and as kept (units).
void alignDrift(float* first, float* kept)
{
    *first = al.drift0;
    *kept = al.drift;
}

// The lines: each weapon's barrel (solid), the shot's line where it is another (dashed), the way to the target (thin);
// each barrel's error beside it, the readout at the view's foot.
void drawAimLines(ImDrawList* dl)
{
    if(!st.aimLines || !st.aim.on)
    {
        return;
    }
    const glm::vec4 cBarrel{0.25f, 0.9f, 1.f, 1.f}, cShot{1.f, 0.35f, 0.9f, 0.95f}, cTarget{1.f, 0.6f, 0.15f, 0.75f};
    float footY = st.view.y + st.view.h - 22.f;
    int lines = 0;
    for(int h = 1; h >= 0; h--)
    {
        AimLineReading r;
        if(!readAimLine(h, r))
        {
            continue;
        }
        glm::vec2 m, b, e, t;
        const bool pm = project(r.muzzle, m);
        if(pm && project(r.muzzle - r.barrel * r.length, b))
        {
            dl->AddLine(ImVec2{b.x, b.y}, ImVec2{m.x, m.y}, colU(cBarrel), 1.5f); // (along the drawn barrel itself)
        }
        if(pm && project(st.aim.at, t))
        {
            dl->AddLine(ImVec2{m.x, m.y}, ImVec2{t.x, t.y}, colU(cTarget), 1.f);
        }
        if(pm && project(r.muzzle + r.barrel * r.dist, e))
        {
            dl->AddLine(ImVec2{m.x, m.y}, ImVec2{e.x, e.y}, colU(cBarrel), 2.f);
            dl->AddCircle(ImVec2{e.x, e.y}, 5.f, colU(cBarrel), 12, 2.f);
            // (a short name only: the numbers at the view's foot; the target's own label stays clear)
            textBox(dl, e.x - 10.f, e.y - (h ? 40.f : 24.f), cBarrel, h ? "L barrel" : "R barrel", true);
        }
        glm::vec2 s;
        if(pm && r.shotOk && r.shotOff > 0.05f && project(r.muzzle + r.shot * r.dist, s))
        {
            // (dashed, its end a diamond: not the target's cross, never dragged)
            constexpr int dashes = 24;
            for(int i = 0; i < dashes; i += 2)
            {
                const glm::vec2 a = m + (s - m) * (static_cast<float>(i) / dashes);
                const glm::vec2 z = m + (s - m) * (static_cast<float>(i + 1) / dashes);
                dl->AddLine(ImVec2{a.x, a.y}, ImVec2{z.x, z.y}, colU(cShot), 1.5f);
            }
            constexpr float d = 5.f;
            dl->AddQuad(ImVec2{s.x, s.y - d}, ImVec2{s.x + d, s.y}, ImVec2{s.x, s.y + d}, ImVec2{s.x - d, s.y}, colU(cShot), 2.f);
            textBox(dl, s.x - 10.f, s.y + 8.f, cShot, "shot", true);
        }
        char line[200];
        q_snprintf(line, sizeof(line), "%s weapon: aim error: yaw %+.2f deg, pitch %+.2f deg (total %.2f deg); shot: %s%s", h ? "left" : "right",
            r.yaw, r.pitch, r.total, r.shotOk ? r.shotWhy : "none", r.shotOk && r.shotOff > 0.05f ? va(" (%.2f deg off the barrel)", r.shotOff) : "");
        textBox(dl, st.view.x + 12.f, footY, glm::vec4{0.8f, 0.97f, 1.f, 1.f}, line);
        footY -= 16.f;
        lines++;
    }
    if(lines > 0 || al.status[0])
    {
        char shared[256];
        sharedWith(alignKeyCode(), shared, sizeof(shared));
        if(al.status[0])
        {
            textBox(dl, st.view.x + 12.f, footY, glm::vec4{1.f, 0.9f, 0.5f, 1.f}, al.status);
            footY -= 16.f;
        }
        if(shared[0])
        {
            textBox(dl, st.view.x + 12.f, footY, glm::vec4{0.85f, 0.85f, 0.85f, 1.f}, shared);
        }
    }
}

// The Align row (Tools > Aim Target; the Pose Studio Data window beside key `shownCode`, -1 Tools'): its button, Also
// Move, the readout and the shared-key note.
void alignRow(int shownCode)
{
    const int code = alignKeyCode();
    if(shownCode >= 0 && shownCode != code)
    {
        return; // (the window shows another key than the subject's now)
    }
    ImGui::BeginDisabled(code < 0 || alignBusy());
    if(ImGui::Button(alignBusy() ? "Aligning..." : "Align to Target"))
    {
        (void)alignStart(st.alignMove);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Also Move", &st.alignMove);
    if(shownCode < 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if(ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", "Turns the current pose key's weapon turn (Right Turn; Left Turn for the left of two guns) until "
                                    "the drawn barrel points exactly at the aim target (solved through the game's pose and the arms' "
                                    "IK; one undo). Also Move: the key's place moved too, so the muzzle stays where it was. The key "
                                    "is shared by every weapon of its pose category (the note under it); a weapon's own grip does not "
                                    "change its aim (the weapon is drawn on the pose's line, the grip only sets the hand on it): to aim "
                                    "one weapon apart, give it its own Pose Category (Weapon Holds).");
        }
    }
    if(code >= 0)
    {
        ImGui::TextDisabled("key %d: %s %s %s", code, agpose::kindTitle(agpose::kindOf(code)), agpose::classTitle(agpose::classOf(code)),
            agpose::poseName(agpose::classOf(code), agpose::poseOf(code)));
    }
    for(int h = 0; h < 2; h++)
    {
        AimLineReading r;
        if(readAimLine(h, r))
        {
            ImGui::Text("%s: yaw %+.2f, pitch %+.2f (total %.2f deg)", h ? "Left" : "Right", r.yaw, r.pitch, r.total);
        }
    }
    char shared[256];
    sharedWith(code, shared, sizeof(shared));
    if(shared[0])
    {
        ImGui::TextWrapped("%s", shared);
    }
    if(al.status[0])
    {
        ImGui::TextWrapped("%s", al.status);
    }
}

} // namespace qvr::rigstudio::detail

namespace
{

// vr_rigstudio_align [move 0|1]: Align to Target on the current key (as the button); vr_rigstudio_aimline [0|1]: the
// lines shown (no argument: the readout printed).
void align_f()
{
    bool move = st.alignMove;
    if(Cmd_Argc() > 2 && !strcmp(Cmd_Argv(1), "move"))
    {
        move = Q_atoi(Cmd_Argv(2)) != 0;
    }
    if(!alignStart(move))
    {
        Con_Printf("rigstudio %s\n", al.status);
    }
}

void aimline_f()
{
    if(Cmd_Argc() > 1)
    {
        st.aimLines = Q_atoi(Cmd_Argv(1)) != 0;
    }
    for(int h = 0; h < 2; h++)
    {
        AimLineReading r;
        if(readAimLine(h, r))
        {
            Con_Printf("rigstudio aimline %s: yaw %+.3f pitch %+.3f total %.3f deg; shot: %s, %.3f deg off the barrel\n", h ? "left" : "right",
                r.yaw, r.pitch, r.total, r.shotOk ? r.shotWhy : "none", r.shotOff);
        }
    }
    char shared[256];
    sharedWith(alignKeyCode(), shared, sizeof(shared));
    Con_Printf("rigstudio aimline: lines %s, key %d; %s\n", st.aimLines ? "on" : "off", alignKeyCode(), shared);
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_rigstudio_test_align
struct AlignCase
{
    const char* spawn;
    int weapon, weapon2;
    const char* name;
    bool move; // Also Move: the muzzle kept where it was (its place moved)
    int mode;  // 0 as the studio opens (Rotate, Game display, paused), 1 the Pose Keys tool, 2 Shaded display, 3 the
               // animation playing (the line checked only), 4 the aim target clicked and dragged (no align)
    bool views; // close side and top screenshots of the aligned gun
};
constexpr AlignCase alignCases[] = {
    {"monster_vr_grunt", 4, 0, "grunt, shotgun (Rifle)", false, 0, true},
    {"monster_vr_grunt", 29, 0, "grunt, nail pistol (Pistol), Pose Keys tool", false, 1, true},
    {"monster_vr_grunt", 10, 0, "grunt, rocket launcher (Launcher), Shaded", false, 2, false},
    {"monster_vr_enforcer", 4, 0, "enforcer, shotgun (Rifle), Pose Keys tool", false, 1, true},
    {"monster_vr_enforcer", 29, 0, "enforcer, nail pistol (Pistol)", false, 0, false},
    {"monster_vr_enforcer", 10, 0, "enforcer, rocket launcher (Launcher)", false, 0, false},
    {"monster_vr_grunt", 29, 29, "grunt, two nail pistols (two guns)", false, 0, true},
    {"monster_vr_grunt", 4, 0, "grunt, shotgun (Rifle), Also Move", true, 0, false},
    {"monster_vr_grunt", 4, 0, "grunt, shotgun (Rifle), the shoot animation playing", false, 3, false},
    {"monster_vr_enforcer", 29, 29, "enforcer, two nail pistols, playing", false, 3, false},
    {"monster_vr_grunt", 4, 0, "the aim target clicked and dragged", false, 4, false},
};
constexpr int alignCaseCount = static_cast<int>(sizeof(alignCases) / sizeof(alignCases[0]));

struct AlignTest
{
    int step{-1};
    int phase{0};
    int waited{0};
    int still{0};
    int spawnFrame{0};
    bool posed{false};
    bool have{false};
    float change{-1.f};
    glm::vec3 last[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    int code{-1};
    agpose::Key original, misaligned;
    float before{0.f};
    glm::vec3 muzzle{0.f}; // (misaligned, before the align)
    int passed{0}, failed{0};
    int shotWait{0};
    int endCase{0};         // the case after the last run
    int view{0};            // the close views' step (case.views)
    int lineFrames{0};      // mode 3: frames of the line checked while playing, the worst
    float lineWorst{0.f}, muzzleWorst{0.f};
    glm::vec3 aimAt{0.f};   // mode 4: the target before the drag
    Camera cam;             // (put back after the close views)
};
AlignTest atl;

// The aim line against the drawn weapon entity itself (its angles and origin through the renderer's own matrix, not
// the captured one): the barrel's angle off the model's forward (degrees) and the muzzle's distance (units). False: none.
bool lineVsModel(int hand, float& turn, float& off)
{
    AimLineReading r;
    const int prop = hand == 0 ? st.note.prop : st.note.propL;
    glm::vec3 muzzle, butt, second;
    int secondType = 0;
    if(!readAimLine(hand, r) || prop <= 0 || !view::weaponLocalGeometry(cl_entities[prop].model, muzzle, butt, second, secondType))
    {
        return false;
    }
    const entity_t& e = cl_entities[prop];
    float m[16];
    vec3_t origin, angles;
    VectorCopy(e.origin, origin);
    VectorCopy(e.angles, angles);
    R_EntityMatrix(m, origin, angles, e.scale);
    glm::mat4 w;
    for(int c = 0; c < 4; c++)
    {
        w[c] = glm::vec4{m[c * 4], m[c * 4 + 1], m[c * 4 + 2], m[c * 4 + 3]};
    }
    const glm::vec3 fwd = glm::normalize(glm::vec3{w * glm::vec4{1.f, 0.f, 0.f, 0.f}});
    turn = angleBetween(r.barrel, fwd);
    // (a left weapon is drawn mirrored: its muzzle's y the other way)
    const glm::vec3 lm = hand == 1 && VR_AliasMirrored(&e) ? glm::vec3{muzzle.x, -muzzle.y, muzzle.z} : muzzle;
    off = glm::length(glm::vec3{w * glm::vec4{lm, 1.f}} - r.muzzle);
    return true;
}

void alignCheck(const char* what, bool ok);

void checkLine(int hands, const char* when)
{
    for(int h = 0; h < hands; h++)
    {
        float turn = 0.f, off = 0.f;
        const bool ok = lineVsModel(h, turn, off);
        alignCheck(va("%s: the %s barrel line on the drawn model: %.3f deg off its forward, its muzzle %.3f units off", when,
                       h ? "left" : "right", turn, off),
            ok && turn < 0.5f && off < 0.5f);
    }
}

constexpr int alignTestGiveUp = 72 * 30; // frames a case waits for its subject or its align (a safety net)

void alignCheck(const char* what, bool ok)
{
    Con_Printf("rigstudio align %d: %s: %s\n", atl.step + 1, ok ? "ok" : "FAIL", what);
    (ok ? atl.passed : atl.failed)++;
}

void alignCaseStart()
{
    const AlignCase& c = alignCases[atl.step];
    q_strlcpy(st.choice.spawn, c.spawn, sizeof(st.choice.spawn));
    st.choice.weapon = c.weapon;
    st.choice.weapon2 = c.weapon2;
    respawn();
    atl.phase = 0;
    atl.waited = 0;
    atl.still = 0;
    atl.have = false;
    atl.posed = false;
    atl.spawnFrame = host_framecount;
}

void alignCaseEnd()
{
    if(atl.code >= 0)
    {
        agpose::set(atl.code, atl.original); // (the key as it was before the case)
    }
    atl.code = -1;
    st.tool = Tool::Rotate;
    st.mesh = MeshMode::Game;
    st.playing = false;
    atl.step++;
    if(atl.step >= alignCaseCount || atl.step >= atl.endCase)
    {
        Con_Printf("rigstudio align: %d passed, %d failed\n", atl.passed, atl.failed);
        atl.step = -1;
        return;
    }
    alignCaseStart();
}

// The subject's drawn weapons still (each barrel under 0.003 degrees a frame) for 10 frames.
[[nodiscard]] bool subjectStill(int hands)
{
    bool still = atl.have;
    for(int h = 0; h < hands; h++)
    {
        AimLineReading r;
        if(!readAimLine(h, r))
        {
            atl.have = false;
            atl.still = 0;
            return false;
        }
        atl.change = atl.have ? angleBetween(r.barrel, atl.last[h]) : -1.f;
        still = still && atl.change < settleTurn;
        atl.last[h] = r.barrel;
    }
    atl.have = true;
    atl.still = still ? atl.still + 1 : 0;
    return atl.still >= 10;
}

void alignTestTick()
{
    if(atl.step < 0 || !st.open)
    {
        return;
    }
    const AlignCase& c = alignCases[atl.step];
    const int hands = c.weapon2 ? 2 : 1;
    atl.waited++;
    if(atl.waited > alignTestGiveUp && atl.phase != 3)
    {
        AimLineReading r;
        alignCheck(va("timed out (phase %d; drawn %d, its barrel's last change %.4f deg, subject %d %s %d, fresh %d)", atl.phase,
                       readAimLine(0, r) ? 1 : 0, atl.change, st.subject, st.made.spawn, st.made.weapon, fresh() ? 1 : 0),
            false);
        alignCancel();
        alignCaseEnd();
        return;
    }
    if(atl.phase == 0)
    {
        // The new subject standing, its weapons drawn and still
        if(host_framecount - atl.spawnFrame < 30 || !fresh() || st.noteFrame <= atl.spawnFrame + 2 ||
            strcmp(st.made.spawn, c.spawn) != 0 || st.made.weapon != c.weapon)
        {
            return;
        }
        if(!atl.posed)
        {
            // (its shoot frame held, as the aim test holds it: the arms on the weapons as the game poses them)
            for(int i = 0; i < groupCount(); i++)
            {
                if(!q_strcasecmp(groupAt(i).name, "shoot") || (st.source != Source::Anim && !q_strcasecmp(groupAt(i).name, "attack")))
                {
                    st.source = Source::Anim;
                    st.group = i;
                }
            }
            st.playing = c.mode == 3;
            setFrame(1.f);
            st.tool = c.mode == 1 ? Tool::PoseKeys : Tool::Rotate;
            st.pdFollow = true;
            st.mesh = c.mode == 2 ? MeshMode::Shaded : MeshMode::Game;
            st.loop = true;
            atl.posed = true;
            atl.lineFrames = 0;
            atl.lineWorst = atl.muzzleWorst = 0.f;
            return;
        }
        if(c.mode == 3)
        {
            // Playing: the line on the drawn model every frame for 72 frames (after 36 to start)
            if(++atl.lineFrames < 36)
            {
                return;
            }
            for(int h = 0; h < hands; h++)
            {
                float turn = 0.f, off = 0.f;
                if(lineVsModel(h, turn, off))
                {
                    atl.lineWorst = za::max(atl.lineWorst, turn);
                    atl.muzzleWorst = za::max(atl.muzzleWorst, off);
                }
                else
                {
                    atl.lineWorst = 99.f;
                }
            }
            if(atl.lineFrames == 60)
            {
                Cbuf_AddText("screenshot\n");
            }
            if(atl.lineFrames < 36 + 72)
            {
                return;
            }
            alignCheck(va("playing, 72 frames: the barrel line at worst %.3f deg off the drawn model's forward, its muzzle %.3f "
                          "units off",
                           atl.lineWorst, atl.muzzleWorst),
                atl.lineWorst < 0.5f && atl.muzzleWorst < 0.5f);
            alignCaseEnd();
            return;
        }
        if(!subjectStill(hands))
        {
            return;
        }
        checkLine(hands, "held");
        if(c.mode == 4)
        {
            // The aim target picked by a click on it (not the lines' markers), then dragged 20 along x, undone
            st.aim.selected = false;
            atl.aimAt = st.aim.at;
            alignCheck("the aim target in view to click", mockClick(st.aim.at));
            atl.phase = 10;
            return;
        }
        atl.code = alignKeyCode();
        if(atl.code < 0)
        {
            alignCheck("no gun key", false);
            alignCaseEnd();
            return;
        }
        // The key misaligned: its turn(s) 10 degrees left (yaw) and 5 up (pitch) more, in its body's space
        atl.original = agpose::value(atl.code);
        atl.misaligned = atl.original;
        const glm::quat off = glm::angleAxis(10.f * deg, glm::vec3{0.f, 0.f, 1.f}) * glm::angleAxis(-5.f * deg, glm::vec3{0.f, 1.f, 0.f});
        for(int h = 0; h < hands; h++)
        {
            const int turn = h == 0 ? agpose::RTurn : agpose::LTurn;
            atl.misaligned.v[turn] = toTurn(off * fromTurn(atl.original.v[turn]));
        }
        agpose::set(atl.code, atl.misaligned);
        atl.misaligned = agpose::value(atl.code); // (as kept: rounded)
        atl.phase = 1;
        atl.have = false;
        atl.still = 0;
        return;
    }
    if(atl.phase == 10 || atl.phase == 11)
    {
        if(mockBusy())
        {
            return;
        }
        if(atl.phase == 10)
        {
            alignCheck("clicked: the aim target selected", st.aim.selected);
            alignCheck("its handle dragged 20 up (z)", mockDrag(1, 0, 2, 20.f));
            atl.phase = 11;
            return;
        }
        const glm::vec3 moved = st.aim.at - atl.aimAt;
        alignCheck(va("dragged: the aim target moved %.2f %.2f %.2f", moved.x, moved.y, moved.z),
            za::abs(moved.z - 20.f) < 1.f && za::abs(moved.x) < 0.5f && za::abs(moved.y) < 0.5f);
        Cbuf_AddText("screenshot\n");
        undo();
        alignCheck("undone: the aim target back", glm::length(st.aim.at - atl.aimAt) < 0.01f);
        st.aim.selected = false;
        alignCaseEnd();
        return;
    }
    if(atl.phase == 1)
    {
        if(!subjectStill(hands))
        {
            return;
        }
        AimLineReading r;
        (void)readAimLine(0, r);
        atl.before = r.total;
        atl.muzzle = r.muzzle;
        Con_Printf("rigstudio align %d: %s, key %d misaligned: right yaw %+.2f pitch %+.2f (%.2f deg)\n", atl.step + 1, c.name, atl.code,
            r.yaw, r.pitch, r.total);
        alignCheck("misaligned by more than 5 degrees", r.total > 5.f);
        Cbuf_AddText("screenshot\n");
        if(!alignStart(c.move))
        {
            alignCheck(alignStatus(), false);
            alignCaseEnd();
            return;
        }
        atl.phase = 2;
        return;
    }
    if(atl.phase == 2)
    {
        if(alignBusy())
        {
            return;
        }
        float eR = 0.f, eL = 0.f;
        (void)alignResult(&eR, &eL);
        AimLineReading r[2];
        bool all = readAimLine(0, r[0]);
        float worst = r[0].total;
        if(hands > 1)
        {
            all = all && readAimLine(1, r[1]);
            worst = za::max(worst, r[1].total);
        }
        const agpose::Key k = agpose::value(atl.code);
        alignCheck(va("aligned: right %.3f deg (yaw %+.3f pitch %+.3f)%s, from %.2f; key turn %.2f %.2f %.2f", r[0].total, r[0].yaw,
                       r[0].pitch, hands > 1 ? va(", left %.3f deg", r[1].total) : "", atl.before, k.v[agpose::RTurn].x,
                       k.v[agpose::RTurn].y, k.v[agpose::RTurn].z),
            all && worst < 0.2f);
        if(c.move)
        {
            const float moved = glm::length(r[0].muzzle - atl.muzzle);
            float first = 0.f, kept = 0.f;
            alignDrift(&first, &kept);
            alignCheck(va("Also Move: the muzzle %.3f units from where it was (turned alone: %.3f; never further), the place "
                          "moved %.2f %.2f %.2f",
                           moved, first, k.v[agpose::RPos].x - atl.misaligned.v[agpose::RPos].x,
                           k.v[agpose::RPos].y - atl.misaligned.v[agpose::RPos].y, k.v[agpose::RPos].z - atl.misaligned.v[agpose::RPos].z),
                moved <= first + 0.1f);
        }
        else
        {
            alignCheck("place left alone (RPos, LPos)", k.v[agpose::RPos] == atl.misaligned.v[agpose::RPos] &&
                                                           k.v[agpose::LPos] == atl.misaligned.v[agpose::LPos]);
        }
        checkLine(hands, "aligned");
        Cbuf_AddText("screenshot\n");
        atl.shotWait = 0;
        atl.view = 0;
        atl.phase = 3;
        return;
    }
    if(atl.phase == 3 && ++atl.shotWait < 3)
    {
        return; // (the screenshot taken first)
    }
    if(atl.phase == 3 && c.views && atl.view < 4)
    {
        // Close views of the aligned gun: from its side, then from above (the line along the barrel in both)
        AimLineReading r;
        (void)readAimLine(0, r);
        if(atl.view == 0)
        {
            atl.cam = st.cam;
        }
        if(atl.view == 0 || atl.view == 2)
        {
            st.cam.focus = r.muzzle - r.barrel * (r.length * 0.5f);
            st.cam.yaw = st.stageYaw + 90.f;
            st.cam.pitch = atl.view == 0 ? 0.f : 89.f;
            st.cam.dist = 45.f;
        }
        else
        {
            Cbuf_AddText("screenshot\n");
        }
        atl.view++;
        atl.shotWait = 0;
        return;
    }
    if(atl.phase == 3)
    {
        if(c.views)
        {
            st.cam = atl.cam;
        }
        undo();
        const agpose::Key k = agpose::value(atl.code);
        bool same = true;
        for(int p = 0; p < agpose::NumParts; p++)
        {
            same = same && k.v[p] == atl.misaligned.v[p];
        }
        alignCheck("undone: the key exactly as before the align", same);
        alignCaseEnd();
    }
}

// vr_rigstudio_test_align: run once the studio is open.
void testAlign_f()
{
    if(!st.open)
    {
        Con_Printf("rigstudio align: the studio is not open\n");
        return;
    }
    atl = AlignTest{};
    atl.step = Cmd_Argc() > 1 ? za::clamp(Q_atoi(Cmd_Argv(1)) - 1, 0, alignCaseCount - 1) : 0; // ([first case] [count])
    atl.endCase = Cmd_Argc() > 2 ? atl.step + za::max(1, Q_atoi(Cmd_Argv(2))) : alignCaseCount;
    st.aim.on = true;
    st.aim.show = true;
    st.aimLines = true;
    resetAim();
    st.showTools = st.showOutliner = st.showProps = st.showTimeline = st.showSequence = st.showPoseData = false;
    // (the camera at its left side, a little in front, looking across at it and the way it aims: the aim test's)
    const float y = st.stageYaw * deg;
    st.cam.focus = st.stage + glm::vec3{za::cos(y), za::sin(y), 0.f} * 80.f + glm::vec3{0.f, 0.f, 40.f};
    st.cam.yaw = st.stageYaw + 118.f;
    st.cam.pitch = 0.f;
    st.cam.dist = 210.f;
    alignCaseStart();
}

} // namespace

namespace qvr::rigstudio::detail
{

void aimLineFrame()
{
    alignTick();
    alignTestTick();
}

void aimLineInit()
{
    Cmd_AddCommand("vr_rigstudio_align", align_f);
    Cmd_AddCommand("vr_rigstudio_aimline", aimline_f);
    Cmd_AddCommand("vr_rigstudio_test_align", testAlign_f);
}

} // namespace qvr::rigstudio::detail
