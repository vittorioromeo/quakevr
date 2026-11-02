// vr_rigstudio_strike.cpp -- the Rig Studio's tools for polishing the armed humanoids' phase-based acts (RIGSTUDIO.md,
// "The strike timeline and the ghosts"; agent/rigtools, 2026-11-02):
//
// - The strike timeline (the Pose Studio Data window, a strike or the Cast picked): while the subject is held in an act
//   the studio tells QC its time into it each frame (VR_AGK_Hold: weapondrawnpose(g, -1, 19)), and QC tells back its
//   phases' lengths and its keys' times (.vr_rs_times, .vr_rs_trec, .vr_rs_keys: VR_AGK_TellTimes). Play, pause, loop,
//   step a frame, speed 0.05x..2x, and a scrubber showing the phases and the keys. Paused, the act sits on the picked
//   phase's key (as before) until scrubbed or stepped elsewhere; a Pose Keys drag puts it back there.
// - The onion skin (vr_rigstudio_onion*): ghosts of the act before (warm) and after (cool) its time, every onion_step
//   seconds or at its phase points, fading out. Each is the subject as the game solved it at that time (its arms, hands,
//   chest and weapons: relinkSkinned's note, kept by its time as it plays); after an edit the act is played through once
//   at 1x to take them again (Auto Refresh).
// - The IK ghost (Tools > Display; vr_rigstudio_ikghost): for each hand, where its set values put it and its weapon (the
//   targets before the arms' IK and limits) beside the hand and weapon as solved, joined, with what held it off.
// - The fly camera (W A S D, Q E down and up, Shift faster): the camera and its orbit's focus moved together.
//
// All of it drawn with the studio's ImGui lists (translucent, never picked), only while the studio is open.
//
// vr_rigstudio_test_strike (open the studio with a gun: `vr_rigstudio monster_vr_grunt 4`; ends on `rigstudio strike: N
// passed, M failed`): the shove's times from QC, its keys parked and scrubbed (the hands moving), playback's speed, the
// onion's ghosts taken, the IK ghost of an out-of-reach key (undone exactly), the fly camera; screenshots of each.

#include "vr_rigstudio_internal.hpp"
#include "vr_progs.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_render.hpp"
#include "vr_geom.hpp"
#include "vr_testkit.hpp"

#include "thirdparty/imgui/imgui.h"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <string.h>

using namespace qvr;
using namespace qvr::rigstudio::detail;

namespace
{

using geom::deg;
constexpr float frameStep = 1.f / 72.f;  // s: a step (<, >, the arrow keys) of the act's time
constexpr float binDt = 1.f / 144.f;     // s: the onion's samples kept this far apart
constexpr int maxBins = 400;             // (a 2.7 s act: the longest is 1.45)
constexpr int maxOnion = 6;
constexpr double refreshSettle = 0.3;    // s (real): an edit's data unchanged this long before Auto Refresh plays it

// The subject as the game solved it at one time of its act (world).
struct GhostWeapon
{
    const qmodel_t* model{nullptr};
    int pose{0};
    glm::mat4 m{1.f}; // its packed vertices -> the world
};
struct GhostPose
{
    bool valid{false};
    bool arm[2]{false, false};
    glm::vec3 shoulder[2]{}, elbow[2]{}, hand[2]{};
    glm::vec3 waist{0.f}, chest{0.f};
    GhostWeapon weapon[2];
};

struct Ghosts
{
    GhostPose bins[maxBins];
    int pose{-1}, kind{-1}, cls{-1};
    uint32_t hash{0};
    double hashAt{0.0};        // realtime: when the data last changed
    uint32_t refreshedHash{0}; // the data Auto Refresh last played (not again for it)
    bool refreshing{false};
    float resumeT{0.f};
    bool resumeParked{true};
};
Ghosts gh;

// vr_rigstudio_test_strike's state.
struct StrikeTest
{
    bool on{false};
    int step{0};
    int waited{0};
    testkit::Tally tally;
    glm::vec3 hand[4]{};
    float t0{0.f};
    double s0{0.0};
    agpose::Key key0;
    int code{-1};
    glm::vec3 cam0{0.f};
};
StrikeTest stest;

[[nodiscard]] ImU32 col(const glm::vec4& c)
{
    return ImGui::ColorConvertFloat4ToU32(ImVec4{c.r, c.g, c.b, c.a});
}

[[nodiscard]] ImVec2 iv(const glm::vec2& v)
{
    return ImVec2{v.x, v.y};
}

[[nodiscard]] edict_t* subjectEd()
{
    if(!sv.active || st.subject <= 0 || st.subject >= sv.qcvm.num_edicts)
    {
        return nullptr;
    }
    edict_t* e = EDICT_NUM(st.subject);
    return e->free ? nullptr : e;
}

[[nodiscard]] Studio::Swing& sw()
{
    return st.swing;
}

// The act's phase points: its phases' bounds and its keys' times, sorted (s).
int phasePoints(float* out, int most)
{
    const Studio::Swing& s = sw();
    float p[8];
    int n = 0;
    float at = 0.f;
    p[n++] = 0.f;
    for(int i = 0; i < 4; i++)
    {
        at += s.times[i];
        if(s.times[i] > 0.f)
        {
            p[n++] = at;
        }
    }
    const int keys = agpose::isStrike(s.heldPose) ? agpose::numPhases : 1;
    for(int k = 0; k < keys && n < 8; k++)
    {
        p[n++] = s.keys[k];
    }
    // (sorted, the near-equal merged)
    for(int i = 1; i < n; i++)
    {
        for(int j = i; j > 0 && p[j] < p[j - 1]; j--)
        {
            const float x = p[j];
            p[j] = p[j - 1];
            p[j - 1] = x;
        }
    }
    int m = 0;
    for(int i = 0; i < n && m < most; i++)
    {
        if(m == 0 || p[i] - out[m - 1] > binDt)
        {
            out[m++] = p[i];
        }
    }
    return m;
}

// The phases' names on the scrubber (by the act: the shove's push, the Cast's raise...).
[[nodiscard]] const char* segmentName(int pose, int seg)
{
    if(pose == agpose::Shove)
    {
        const char* n[4] = {"wind-up", "push", "recover", "back"};
        return n[seg];
    }
    if(pose == agpose::Cast)
    {
        const char* n[4] = {"raise", "hold", "", "lower"};
        return n[seg];
    }
    if(pose == agpose::SawThrust)
    {
        const char* n[4] = {"wind-up", "strike", "cut", "recover"};
        return n[seg];
    }
    const char* n[4] = {"wind-up", "strike", "hold", "recover"};
    return n[seg];
}

// The data the ghosts were taken with: the held act's keys, its kind and class's stretch, the aim target, the subject.
[[nodiscard]] uint32_t dataHash()
{
    uint32_t h = 2166136261u;
    const auto mix = [&](const void* p, size_t n) {
        const auto* b = static_cast<const unsigned char*>(p);
        for(size_t i = 0; i < n; i++)
        {
            h = (h ^ b[i]) * 16777619u;
        }
    };
    const Studio::Swing& s = sw();
    const int keys = agpose::isStrike(s.heldPose) ? agpose::numPhases : 1;
    for(int k = 0; k < keys && s.heldPose >= 0; k++)
    {
        const agpose::Key key = agpose::value(agpose::code(st.subjKind, st.subjClass, s.heldPose, k));
        mix(&key, sizeof(key));
    }
    const float stretch = agpose::stretch(st.subjKind, st.subjClass);
    mix(&stretch, sizeof(stretch));
    mix(&st.aim.at, sizeof(st.aim.at));
    const int misc[5] = {st.aim.on ? 1 : 0, st.subject, s.heldPose, st.subjKind, st.subjClass};
    mix(misc, sizeof(misc));
    return h;
}

void clearGhosts()
{
    for(GhostPose& g : gh.bins)
    {
        g.valid = false;
    }
}

[[nodiscard]] int binOf(float t)
{
    return za::clamp(static_cast<int>(t / binDt + 0.5f), 0, maxBins - 1);
}

// The ghost kept nearest `t` (within two bins), or none.
[[nodiscard]] const GhostPose* ghostAt(float t)
{
    const int b = binOf(t);
    for(int d = 0; d <= 2; d++)
    {
        for(int s = -1; s <= 1; s += 2)
        {
            const int i = b + d * s;
            if(i >= 0 && i < maxBins && gh.bins[i].valid)
            {
                return &gh.bins[i];
            }
        }
    }
    return nullptr;
}

// The onion's times: before (negative k) and after `t`.
int onionTimes(float* times, int* ks, int most)
{
    const float len = strikeLength();
    const int n = za::clamp(static_cast<int>(vr_rigstudio_onion_count.value), 1, maxOnion);
    const float t = sw().t;
    int m = 0;
    if(vr_rigstudio_onion_mode.value >= 1.f)
    {
        float p[8];
        const int np = phasePoints(p, 8);
        int before = 0;
        for(int i = np - 1; i >= 0 && before < n && m < most; i--)
        {
            if(p[i] < t - binDt)
            {
                times[m] = p[i];
                ks[m++] = -(++before);
            }
        }
        int after = 0;
        for(int i = 0; i < np && after < n && m < most; i++)
        {
            if(p[i] > t + binDt)
            {
                times[m] = p[i];
                ks[m++] = ++after;
            }
        }
        return m;
    }
    const float step = za::clamp(vr_rigstudio_onion_step.value, 0.01f, 0.5f);
    for(int k = 1; k <= n && m < most; k++)
    {
        for(int s = -1; s <= 1 && m < most; s += 2)
        {
            const float at = t + static_cast<float>(s * k) * step;
            if(at >= 0.f && at <= len)
            {
                times[m] = at;
                ks[m++] = s * k;
            }
        }
    }
    return m;
}

// The subject's model space -> the world (the note's space).
[[nodiscard]] glm::mat4 spaceMatrix(const posestudio::Space& s)
{
    const float y = s.yawDeg * deg;
    glm::mat4 m{1.f};
    m[0] = glm::vec4{za::cos(y) * s.scale, za::sin(y) * s.scale, 0.f, 0.f};
    m[1] = glm::vec4{-za::sin(y) * s.scale, za::cos(y) * s.scale, 0.f, 0.f};
    m[2] = glm::vec4{0.f, 0.f, s.scale, 0.f};
    m[3] = glm::vec4{s.origin, 1.f};
    return m;
}

[[nodiscard]] glm::mat4 rigidMatrix(const rigpose::Rigid& r)
{
    glm::mat4 m = glm::mat4_cast(r.rot);
    m[3] = glm::vec4{r.pos, 1.f};
    return m;
}

// Weapon entity `num` drawn at `frame` (model space, an entity's frame) of the subject (`space`): its packed vertices ->
// the world (render::entityMatrix's at the origin unturned, then the frame).
bool weaponGhost(int num, const glm::mat4& space, const rigpose::Rigid& frame, GhostWeapon& out)
{
    if(num <= 0 || num >= cl_max_edicts || !cl_entities[num].model || cl_entities[num].model->type != mod_alias)
    {
        return false;
    }
    const entity_t& e = cl_entities[num];
    entity_t at{};
    at.model = e.model;
    at.scale = e.scale ? e.scale : ENTSCALE_DEFAULT;
    float m0[16];
    render::entityMatrix(at, false, at.scale, glm::vec3{0.f}, m0);
    glm::mat4 f0;
    for(int c = 0; c < 4; c++)
    {
        f0[c] = glm::vec4{m0[c * 4], m0[c * 4 + 1], m0[c * 4 + 2], m0[c * 4 + 3]};
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(e.model));
    out.model = e.model;
    out.pose = hdr && e.frame >= 0 && e.frame < hdr->numframes ? hdr->frames[e.frame].firstpose : 0;
    out.m = space * rigidMatrix(frame) * f0;
    return true;
}

// The subject's ghost from its note (world).
GhostPose poseFromNote(const posestudio::Note& n)
{
    GhostPose g;
    const glm::mat4 sm = spaceMatrix(n.space);
    const auto w = [&](const glm::vec3& p) { return glm::vec3{sm * glm::vec4{p, 1.f}}; };
    for(int s = 0; s < 2; s++)
    {
        g.arm[s] = n.placed.shoulder[s] != glm::vec3{0.f};
        g.shoulder[s] = w(n.placed.shoulder[s]);
        g.elbow[s] = w(n.placed.elbow[s]);
        g.hand[s] = w(n.placed.hand[s].pos);
        if(n.placed.on[s])
        {
            weaponGhost(s == 0 ? n.prop : n.propL, sm, n.placed.weapon[s], g.weapon[s]);
        }
    }
    g.waist = w(n.waist);
    g.chest = (g.shoulder[0] + g.shoulder[1]) * 0.5f;
    g.valid = true;
    return g;
}

// A weapon's triangles, its faces to the camera only, filled with `c`.
void drawWeaponGhost(ImDrawList* dl, const GhostWeapon& w, const glm::vec4& c)
{
    if(!w.model)
    {
        return;
    }
    const auto* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(const_cast<qmodel_t*>(w.model)));
    if(!hdr || w.pose < 0 || w.pose >= hdr->numposes)
    {
        return;
    }
    const auto* verts = reinterpret_cast<const trivertx_t*>(reinterpret_cast<const byte*>(hdr) + hdr->vertexes) +
                        static_cast<ptrdiff_t>(w.pose) * hdr->numverts;
    const auto* desc = reinterpret_cast<const aliasmesh_t*>(reinterpret_cast<const byte*>(hdr) + hdr->meshdesc);
    const auto* idx = reinterpret_cast<const unsigned short*>(reinterpret_cast<const byte*>(hdr) + hdr->indexes);
    const ImU32 fill = col(c);
    const bool flip = glm::determinant(glm::mat3{w.m}) < 0.f;
    for(int i = 0; i + 2 < hdr->numindexes; i += 3)
    {
        glm::vec3 p[3];
        glm::vec2 s[3];
        bool ok = true;
        for(int k = 0; k < 3; k++)
        {
            const trivertx_t& t = verts[desc[idx[i + k]].vertindex];
            p[k] = glm::vec3{w.m * glm::vec4{t.v[0], t.v[1], t.v[2], 1.f}};
            ok = ok && project(p[k], s[k]);
        }
        if(!ok)
        {
            continue;
        }
        // (its faces to the camera: either winding, the screen's: no face drawn twice over)
        const float area = (s[1].x - s[0].x) * (s[2].y - s[0].y) - (s[2].x - s[0].x) * (s[1].y - s[0].y);
        if((area > 0.f) != flip)
        {
            continue;
        }
        dl->AddTriangleFilled(iv(s[0]), iv(s[1]), iv(s[2]), fill);
    }
}

// One ghost: its arms (shoulder, elbow, hand), its chest, its weapons, tinted `c`.
void drawGhostPose(ImDrawList* dl, const GhostPose& g, const glm::vec4& c)
{
    const ImU32 line = col(c);
    const ImU32 dot = col(glm::vec4{glm::vec3{c}, za::min(1.f, c.a * 1.4f)});
    for(int s = 0; s < 2; s++)
    {
        drawWeaponGhost(dl, g.weapon[s], glm::vec4{glm::vec3{c}, c.a * 0.55f});
        glm::vec2 sh, el, ha;
        if(g.arm[s] && project(g.shoulder[s], sh) && project(g.elbow[s], el) && project(g.hand[s], ha))
        {
            dl->AddLine(iv(sh), iv(el), line, 4.f);
            dl->AddLine(iv(el), iv(ha), line, 4.f);
            dl->AddCircleFilled(iv(el), 3.f, dot, 8);
            dl->AddCircleFilled(iv(ha), 4.5f, dot, 10);
        }
    }
    glm::vec2 wa, ch, s0, s1;
    if(project(g.waist, wa) && project(g.chest, ch))
    {
        dl->AddLine(iv(wa), iv(ch), line, 3.f);
    }
    if(g.arm[0] && g.arm[1] && project(g.shoulder[0], s0) && project(g.shoulder[1], s1))
    {
        dl->AddLine(iv(s0), iv(s1), line, 3.f);
    }
}

void drawOnionSkin(ImDrawList* dl)
{
    const Studio::Swing& s = sw();
    if(vr_rigstudio_onion.value == 0.f || s.heldPose < 0 || !s.timed)
    {
        return;
    }
    float times[2 * maxOnion + 2];
    int ks[2 * maxOnion + 2];
    const int m = onionTimes(times, ks, 2 * maxOnion + 2);
    const int n = za::clamp(static_cast<int>(vr_rigstudio_onion_count.value), 1, maxOnion);
    // (the farthest first: the nearest over them)
    for(int far = n; far >= 1; far--)
    {
        for(int i = 0; i < m; i++)
        {
            if(za::abs(ks[i]) != far)
            {
                continue;
            }
            const GhostPose* g = ghostAt(times[i]);
            if(!g)
            {
                continue;
            }
            const float fade = 1.f - 0.6f * static_cast<float>(far - 1) / static_cast<float>(za::max(n, 2) - 1);
            const glm::vec4 c = ks[i] < 0 ? glm::vec4{1.f, 0.45f, 0.2f, 0.6f * fade} : glm::vec4{0.25f, 0.75f, 1.f, 0.6f * fade};
            drawGhostPose(dl, *g, c);
        }
    }
}

// The IK ghost: each hand's set values (its goal before the IK and its limits) beside the hand as solved.
void drawIkGhost(ImDrawList* dl)
{
    const int which = static_cast<int>(vr_rigstudio_ikghost.value);
    if(which <= 0 || st.noteFrame < 0 || host_framecount - st.noteFrame > 5 || st.note.ent != st.subject)
    {
        return;
    }
    const posestudio::Note& n = st.note;
    const glm::mat4 sm = spaceMatrix(n.space);
    const auto w = [&](const glm::vec3& p) { return glm::vec3{sm * glm::vec4{p, 1.f}}; };
    // (the right weapon's wanted frame carries the off hand on its second grip or cupping the right hand)
    const bool offOnGun = n.placed.held2 || n.placed.cupped;
    for(int s = 0; s < 2; s++)
    {
        if(!(which & (1 << s)))
        {
            continue;
        }
        const glm::vec4 c = s == 0 ? glm::vec4{0.35f, 0.85f, 1.f, 1.f} : glm::vec4{1.f, 0.5f, 0.95f, 1.f};
        rigpose::Rigid want = n.placed.hand[s];
        bool have = false;
        GhostWeapon gw;
        if(s == 1 && offOnGun && n.t.goal[0] == agarms::Goal::Frame && n.placed.on[0])
        {
            // (on the right gun: carried with its wanted frame)
            const glm::mat4 rel = glm::inverse(rigidMatrix(n.placed.weapon[0])) * rigidMatrix(n.placed.hand[1]);
            const glm::mat4 wm = rigidMatrix(n.t.weapon[0]) * rel;
            want.pos = glm::vec3{wm[3]};
            want.rot = glm::quat_cast(glm::mat3{wm});
            have = true;
        }
        else if(n.t.goal[s] == agarms::Goal::Frame && n.placed.on[s])
        {
            const glm::mat4 rel = glm::inverse(rigidMatrix(n.placed.weapon[s])) * rigidMatrix(n.placed.hand[s]);
            const glm::mat4 wm = rigidMatrix(n.t.weapon[s]) * rel;
            want.pos = glm::vec3{wm[3]};
            want.rot = glm::quat_cast(glm::mat3{wm});
            weaponGhost(s == 0 ? n.prop : n.propL, sm, n.t.weapon[s], gw); // (none: the hand alone)
            have = true;
        }
        else if(n.t.goal[s] == agarms::Goal::Point)
        {
            want.pos = n.t.weapon[s].pos;
            have = true;
        }
        if(!have)
        {
            continue;
        }
        drawWeaponGhost(dl, gw, glm::vec4{glm::vec3{c}, 0.3f});
        const glm::vec3 wantW = w(want.pos), handW = w(n.placed.hand[s].pos), shW = w(n.placed.shoulder[s]);
        glm::vec2 a, b, sh;
        const float gap = glm::length(want.pos - n.placed.hand[s].pos) * n.space.scale;
        const float turn = 2.f * glm::degrees(za::acos(za::min(1.f, za::abs(glm::dot(want.rot, n.placed.hand[s].rot)))));
        if(project(wantW, a) && project(handW, b))
        {
            // (the set place: a hollow diamond; to it from the solved hand, dashed)
            const ImU32 cc = col(glm::vec4{glm::vec3{c}, 0.9f});
            dl->AddQuad(ImVec2{a.x, a.y - 7.f}, ImVec2{a.x + 7.f, a.y}, ImVec2{a.x, a.y + 7.f}, ImVec2{a.x - 7.f, a.y}, cc, 2.f);
            const glm::vec2 d = a - b;
            const float len = glm::length(d);
            for(float u = 0.f; u < len; u += 8.f)
            {
                dl->AddLine(iv(b + d * (u / za::max(len, 1e-3f))), iv(b + d * (za::min(u + 4.f, len) / za::max(len, 1e-3f))), cc, 1.5f);
            }
            if(project(shW, sh))
            {
                dl->AddLine(iv(sh), iv(a), col(glm::vec4{glm::vec3{c}, 0.35f}), 3.f); // (the arm as set: straight to it)
            }
            char text[160];
            q_snprintf(text, sizeof(text), "%s set: %.1fu %.0fdeg off (reach %.1f, body %.1f, wrist %.0f, elbow %.0f)",
                s == 0 ? "R" : "L", gap, turn, n.placed.reach[s], n.placed.pushed[s], n.placed.wrist[s], n.placed.poleOff[s]);
            dl->AddText(ImVec2{a.x + 9.f, a.y + 2.f}, cc, text);
        }
        // (the elbow's pole as set: from the solved elbow)
        glm::vec2 e0, e1;
        if(glm::length(n.t.pole[s]) > 1e-3f && project(w(n.placed.elbow[s]), e0) &&
           project(w(n.placed.elbow[s] + glm::normalize(n.t.pole[s]) * 8.f), e1))
        {
            dl->AddLine(iv(e0), iv(e1), col(glm::vec4{glm::vec3{c}, 0.8f}), 2.f);
            dl->AddCircle(iv(e1), 3.f, col(glm::vec4{glm::vec3{c}, 0.8f}), 8, 1.5f);
        }
    }
}

void refreshGhosts()
{
    Studio::Swing& s = sw();
    if(s.heldPose < 0 || !s.timed)
    {
        return;
    }
    gh.refreshing = true;
    gh.resumeT = s.t;
    gh.resumeParked = s.parked;
    gh.refreshedHash = gh.hash;
    s.t = 0.f;
    s.playing = true;
    s.parked = false;
}

void stopRefresh()
{
    if(!gh.refreshing)
    {
        return;
    }
    Studio::Swing& s = sw();
    gh.refreshing = false;
    s.t = gh.resumeT;
    s.parked = gh.resumeParked;
    s.playing = false;
}

void strikeStatus()
{
    const Studio::Swing& s = sw();
    Con_Printf("strike: pose %d (%s) t %.4f / %.4f s, phase %d, %s%s%s, speed %.2fx; times %.3f %.3f %.3f %.3f, keys %.4f %.4f "
               "%.4f\n",
        s.heldPose, s.heldPose >= 0 ? agpose::poseName(st.subjClass, s.heldPose) : "none", s.t, strikeLength(), st.pdPhase,
        s.playing ? "playing" : "paused", s.parked ? " (on the key)" : "", s.loop ? ", loop" : "", s.speed, s.times[0],
        s.times[1], s.times[2], s.times[3], s.keys[0], s.keys[1], s.keys[2]);
}

// vr_rigstudio_strike [play | pause | park | t <s> | step <frames> | speed <x> | loop 0|1 | refresh]...: the strike
// timeline as its buttons work it; then its state printed.
void strike_f()
{
    Studio::Swing& s = sw();
    for(int i = 1; i < Cmd_Argc(); i++)
    {
        const char* w = Cmd_Argv(i);
        const float v = i + 1 < Cmd_Argc() ? static_cast<float>(Q_atof(Cmd_Argv(i + 1))) : 0.f;
        if(!strcmp(w, "play")) { stopRefresh(); if(!s.on && s.heldPose < 0) { swingPlay(); } else { s.playing = true; s.parked = false; } }
        else if(!strcmp(w, "pause")) { stopRefresh(); s.playing = false; }
        else if(!strcmp(w, "park")) { strikePark(); }
        else if(!strcmp(w, "t")) { strikeSetTime(v); i++; }
        else if(!strcmp(w, "step")) { strikeSetTime(s.t + v * frameStep); i++; }
        else if(!strcmp(w, "speed")) { s.speed = za::clamp(v, strikeSpeedMin, strikeSpeedMax); i++; }
        else if(!strcmp(w, "loop")) { s.loop = v != 0.f; i++; }
        else if(!strcmp(w, "refresh")) { refreshGhosts(); }
        else
        {
            Con_Printf("vr_rigstudio_strike [play | pause | park | t <s> | step <frames> | speed <x> | loop 0|1 | refresh]...\n");
            return;
        }
    }
    strikeStatus();
}

void testStrike_f()
{
    if(!st.open || st.subject <= 0)
    {
        Con_Printf("rigstudio strike: open the studio with a gun first (vr_rigstudio monster_vr_grunt 4)\n");
        testkit::verdict("rigstudio strike", 0, 1);
        return;
    }
    stest = StrikeTest{};
    stest.on = true;
    Con_Printf("rigstudio strike: begun\n");
}

} // namespace

namespace qvr::rigstudio::detail
{

bool timedPose(int pose)
{
    return agpose::isStrike(pose) || pose == agpose::Cast;
}

float strikeLength()
{
    const Studio::Swing& s = sw();
    return s.timed ? s.times[0] + s.times[1] + s.times[2] + s.times[3] : 0.f;
}

void strikeSetTime(float t)
{
    Studio::Swing& s = sw();
    stopRefresh();
    const float len = strikeLength();
    s.t = za::clamp(t, 0.f, len > 0.f ? len : t);
    s.playing = false;
    s.parked = false;
}

void strikePark()
{
    Studio::Swing& s = sw();
    stopRefresh();
    s.playing = false;
    s.parked = true;
}

glm::vec3 strikeQuery(int ent)
{
    Studio::Swing& s = sw();
    if(!st.open || ent != st.subject || st.subject <= 0)
    {
        return glm::vec3{1.f, 0.f, 0.f};
    }
    const float speed = za::clamp(s.speed, 0.1f, 1.f); // (QC's own play by count: as Swing Speed was)
    if(s.heldPose < 0)
    {
        return glm::vec3{s.seen ? speed : 1.f, 0.f, 0.f};
    }
    if(s.parked && s.timed)
    {
        s.t = s.keys[za::clamp(s.heldPhase, 0, agpose::numPhases - 1)];
    }
    s.sentT = s.t;
    return glm::vec3{speed, s.t, 1.f};
}

void strikeServerFrame()
{
    Studio::Swing& s = sw();
    const double now = sv.qcvm.time;
    const float dt = s.lastServer >= 0.0 ? static_cast<float>(za::clamp(now - s.lastServer, 0.0, 0.1)) : 0.f;
    s.lastServer = now;
    // QC's times for the held act (VR_AGK_TellTimes)
    if(edict_t* e = s.heldPose >= 0 ? subjectEd() : nullptr)
    {
        const glm::vec3 tm = progs::fieldVec(e, progs::fields().vr_rs_times);
        const float rec = progs::fieldFloatOr(e, progs::fields().vr_rs_trec, 0.f);
        const glm::vec3 keys = progs::fieldVec(e, progs::fields().vr_rs_keys);
        if(tm.x + tm.y + tm.z + rec > 0.f)
        {
            s.times[0] = tm.x;
            s.times[1] = tm.y;
            s.times[2] = tm.z;
            s.times[3] = rec;
            s.keys[0] = keys.x;
            s.keys[1] = keys.y;
            s.keys[2] = keys.z;
            s.timed = true;
        }
    }
    // A Pose Keys drag: the act on the picked phase's key (the key whole, as the handles map it)
    if(st.dragging && (st.dragKind == 11 || st.dragKind == 12) && !s.parked)
    {
        strikePark();
    }
    const float len = strikeLength();
    if(s.playing && len > 0.f)
    {
        const bool refresh = gh.refreshing;
        s.t += dt * (refresh ? 1.f : za::clamp(s.speed, strikeSpeedMin, strikeSpeedMax));
        if(s.t >= len)
        {
            if(refresh)
            {
                stopRefresh();
            }
            else if(s.loop)
            {
                s.t -= len * za::floor(s.t / len);
            }
            else
            {
                s.t = len;
                s.playing = false;
                s.parked = true; // (played once: back on the picked phase's key, as Swing held it)
            }
        }
    }
    else if(!s.playing && gh.refreshing)
    {
        stopRefresh();
    }
    // The onion's ghosts: dropped when their act or its data change; played again once the data settle (Auto Refresh)
    const uint32_t h = dataHash();
    if(s.heldPose != gh.pose || st.subjKind != gh.kind || st.subjClass != gh.cls)
    {
        clearGhosts();
        gh.pose = s.heldPose;
        gh.kind = st.subjKind;
        gh.cls = st.subjClass;
        gh.refreshedHash = 0;
    }
    if(h != gh.hash)
    {
        gh.hash = h;
        gh.hashAt = realtime;
        clearGhosts();
    }
    if(vr_rigstudio_onion.value != 0.f && s.heldPose >= 0 && s.timed && !s.playing && !gh.refreshing && !st.dragging &&
       gh.refreshedHash != gh.hash && realtime - gh.hashAt > refreshSettle)
    {
        float times[2 * maxOnion + 2];
        int ks[2 * maxOnion + 2];
        const int m = onionTimes(times, ks, 2 * maxOnion + 2);
        bool missing = false;
        for(int i = 0; i < m; i++)
        {
            missing = missing || !ghostAt(times[i]);
        }
        if(missing)
        {
            refreshGhosts();
        }
    }
}

void ghostNote()
{
    const Studio::Swing& s = sw();
    if(s.heldPose < 0 || !s.timed || s.heldPose != gh.pose)
    {
        return;
    }
    gh.bins[binOf(s.sentT)] = poseFromNote(st.note);
}

void drawGhosts(ImDrawList* dl)
{
    drawOnionSkin(dl);
    drawIkGhost(dl);
}

void flyCamera(const glm::vec3& move, bool fast, float dt)
{
    if(move == glm::vec3{0.f})
    {
        return;
    }
    const float speed = za::max(vr_rigstudio_fly_speed.value, 1.f) * (fast ? 3.f : 1.f);
    const glm::vec3 d = st.view.fwd * move.x + st.view.right * move.y + glm::vec3{0.f, 0.f, 1.f} * move.z;
    st.cam.focus += d * (speed * za::min(dt, 0.1f)); // (the focus with it: the camera as it was, moved)
}

void strikeTimelineUi()
{
    Studio::Swing& s = sw();
    if(!timedPose(st.pdPose))
    {
        return;
    }
    ImGui::SeparatorText("Timeline");
    const bool held = s.heldPose >= 0;
    const float len = strikeLength();
    // Play / pause, to the start, a frame back and on, loop, speed
    if(ImGui::Button(s.playing && !gh.refreshing ? "Pause" : "Play"))
    {
        if(s.playing)
        {
            stopRefresh();
            s.playing = false;
        }
        else if(!held || (s.parked && !s.on))
        {
            swingPlay(); // (held in the act if not yet, from its start)
        }
        else
        {
            stopRefresh();
            s.t = len > 0.f && s.t >= len - 1e-4f ? 0.f : s.t;
            s.playing = true;
            s.parked = false;
        }
    }
    ImGui::SameLine();
    if(ImGui::Button("|<"))
    {
        strikeSetTime(0.f);
    }
    ImGui::SameLine();
    if(ImGui::Button("<"))
    {
        strikeSetTime(s.t - frameStep);
    }
    ImGui::SameLine();
    if(ImGui::Button(">"))
    {
        strikeSetTime(s.t + frameStep);
    }
    ImGui::SameLine();
    if(ImGui::Button("Key"))
    {
        strikePark();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &s.loop);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.f);
    ImGui::SliderFloat("Speed", &s.speed, strikeSpeedMin, strikeSpeedMax, "%.2fx",
        ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
    if(s.on)
    {
        ImGui::SameLine();
        if(ImGui::Button("Let Go"))
        {
            stopRefresh();
            s.on = false;
            s.playing = false;
        }
    }
    helpMarker("The act as the game plays it (QC's times: VR_AGP_Times), at Speed (0.05x..2x). Play (Space): from where it "
               "is (the start when at its end); not looping, it stops back on the picked phase's key. |< the start, < > a "
               "frame (1/72 s; the Left and Right keys), Key: back on the picked phase's key (paused; where it sits, and "
               "where a Pose Keys drag puts it). Drag on the bar to scrub. Let Go: the subject no longer held in the act "
               "(outside the Pose Keys tool). The subject must be of this kind and class (Follow the Subject).");
    if(st.subject > 0 && (st.subjKind != st.pdKind || st.subjClass != st.pdClass))
    {
        ImGui::TextDisabled("Timeline: the subject holds another kind or class");
    }
    // The scrubber: the phases, the keys (the picked one bright), the onion's times, the time
    const float w = za::max(ImGui::GetContentRegionAvail().x, 120.f);
    const float h = ImGui::GetFontSize() * 2.6f;
    const ImVec2 o = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("##strikebar", ImVec2{w, h});
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(o, ImVec2{o.x + w, o.y + h}, col(glm::vec4{0.12f, 0.12f, 0.14f, 1.f}), 3.f);
    if(!held || len <= 0.f)
    {
        dl->AddText(ImVec2{o.x + 6.f, o.y + 6.f}, col(glm::vec4{0.7f, 0.7f, 0.7f, 1.f}),
            "Play (or the Pose Keys tool) holds the subject in this act: its timeline then shows here.");
        return;
    }
    if(ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        strikeSetTime((ImGui::GetIO().MousePos.x - o.x) / w * len);
    }
    const auto xOf = [&](float t) { return o.x + w * za::clamp(t / len, 0.f, 1.f); };
    const glm::vec4 segCol[4] = {{0.25f, 0.35f, 0.55f, 1.f}, {0.6f, 0.3f, 0.25f, 1.f}, {0.55f, 0.45f, 0.2f, 1.f}, {0.25f, 0.45f, 0.3f, 1.f}};
    float at = 0.f;
    const float barTop = o.y + h * 0.45f;
    for(int i = 0; i < 4; i++)
    {
        if(s.times[i] <= 0.f)
        {
            continue;
        }
        const float x0 = xOf(at), x1 = xOf(at + s.times[i]);
        dl->AddRectFilled(ImVec2{x0, barTop}, ImVec2{x1, o.y + h - 2.f}, col(segCol[i]));
        dl->AddLine(ImVec2{x0, o.y + 2.f}, ImVec2{x0, o.y + h - 2.f}, col(glm::vec4{0.8f, 0.8f, 0.8f, 0.5f}), 1.f);
        const char* name = segmentName(s.heldPose, i);
        if(x1 - x0 > ImGui::CalcTextSize(name).x + 4.f)
        {
            dl->AddText(ImVec2{x0 + 3.f, barTop + 1.f}, col(glm::vec4{0.95f, 0.95f, 0.95f, 0.9f}), name);
        }
        at += s.times[i];
    }
    const int keys = agpose::isStrike(s.heldPose) ? agpose::numPhases : 1;
    for(int k = 0; k < keys; k++)
    {
        const float x = xOf(s.keys[k]);
        const float y = o.y + h * 0.25f;
        const bool picked = k == st.pdPhase || keys == 1;
        const ImU32 kc = col(picked ? glm::vec4{1.f, 0.85f, 0.2f, 1.f} : glm::vec4{0.85f, 0.85f, 0.85f, 0.85f});
        dl->AddQuadFilled(ImVec2{x, y - 5.f}, ImVec2{x + 5.f, y}, ImVec2{x, y + 5.f}, ImVec2{x - 5.f, y}, kc);
        if(ImGui::IsMouseHoveringRect(ImVec2{x - 6.f, o.y}, ImVec2{x + 6.f, o.y + h}))
        {
            ImGui::SetTooltip("key %d: %s at %.3f s", k, keys == 1 ? "the Cast" : agpose::phaseName(s.heldPose, k), s.keys[k]);
        }
    }
    if(vr_rigstudio_onion.value != 0.f)
    {
        float times[2 * maxOnion + 2];
        int ks[2 * maxOnion + 2];
        const int m = onionTimes(times, ks, 2 * maxOnion + 2);
        for(int i = 0; i < m; i++)
        {
            const float x = xOf(times[i]);
            const glm::vec4 c = ks[i] < 0 ? glm::vec4{1.f, 0.45f, 0.2f, 0.9f} : glm::vec4{0.25f, 0.75f, 1.f, 0.9f};
            dl->AddLine(ImVec2{x, o.y + h * 0.45f}, ImVec2{x, o.y + h - 2.f}, col(c), ghostAt(times[i]) ? 2.f : 1.f);
        }
    }
    const float px = xOf(s.t);
    dl->AddLine(ImVec2{px, o.y}, ImVec2{px, o.y + h}, col(glm::vec4{1.f, 1.f, 1.f, 1.f}), 2.f);
    ImGui::Text("%.3f / %.3f s (%.0f%%)%s%s", s.t, len, 100.f * s.t / len, s.parked ? "  on the key" : "",
        gh.refreshing ? "  (taking the ghosts)" : "");
    // The onion skin
    bool onion = vr_rigstudio_onion.value != 0.f;
    if(ImGui::Checkbox("Onion Skin", &onion))
    {
        Cvar_SetValueQuick(&vr_rigstudio_onion, onion ? 1.f : 0.f);
    }
    if(onion)
    {
        ImGui::SameLine();
        int n = za::clamp(static_cast<int>(vr_rigstudio_onion_count.value), 1, maxOnion);
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.f);
        if(ImGui::SliderInt("Each Way", &n, 1, maxOnion))
        {
            Cvar_SetValueQuick(&vr_rigstudio_onion_count, static_cast<float>(n));
        }
        int mode = vr_rigstudio_onion_mode.value >= 1.f ? 1 : 0;
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.f);
        const char* modes[] = {"Every Step", "Phase Points"};
        if(ImGui::Combo("Spacing", &mode, modes, 2))
        {
            Cvar_SetValueQuick(&vr_rigstudio_onion_mode, static_cast<float>(mode));
        }
        if(mode == 0)
        {
            ImGui::SameLine();
            float step = za::clamp(vr_rigstudio_onion_step.value, 0.01f, 0.5f);
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.f);
            if(ImGui::SliderFloat("Step", &step, 0.01f, 0.5f, "%.3f s", ImGuiSliderFlags_Logarithmic))
            {
                Cvar_SetValueQuick(&vr_rigstudio_onion_step, step);
            }
        }
        ImGui::SameLine();
        if(ImGui::Button("Refresh"))
        {
            refreshGhosts();
        }
        helpMarker("Ghosts of the act before (orange) and after (blue) its time, fading with distance: every Step seconds, "
                   "or at its phase points (its phases' bounds and its keys). Each is the subject as the game solved it at "
                   "that time (its arms, hands, chest and weapons), kept as the act plays; after an edit the act plays once "
                   "at 1x to take them again (or press Refresh). Never picked.");
    }
}

void ghostsDisplayUi()
{
    const int which = static_cast<int>(vr_rigstudio_ikghost.value);
    bool r = (which & 1) != 0, l = (which & 2) != 0;
    bool changed = ImGui::Checkbox("IK Ghost: Right", &r);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Left", &l);
    if(changed)
    {
        Cvar_SetValueQuick(&vr_rigstudio_ikghost, static_cast<float>((r ? 1 : 0) | (l ? 2 : 0)));
    }
    helpMarker("Where each hand's set values put it, before the arms' IK and their limits: its weapon there (translucent; "
               "the right blue, the left pink), a hollow diamond at the hand as set, a dashed line from the hand as solved, "
               "the arm straight from its shoulder, the elbow's pole as set, and how far off it was held (units, degrees) "
               "and by what: its reach, the body, the wrist's limit, the elbow off its pole. The off hand on the gun rides "
               "on the right weapon's ghost. vr_rigstudio_ikghost.");
}

void strikeTestTick()
{
    if(!stest.on)
    {
        return;
    }
    Studio::Swing& s = sw();
    testkit::Tally& t = stest.tally;
    const char* name = "rigstudio strike";
    stest.waited++;
    const auto next = [&]() {
        stest.step++;
        stest.waited = 0;
    };
    const auto hand = [&]() {
        const glm::mat4 sm = spaceMatrix(st.note.space);
        return glm::vec3{sm * glm::vec4{st.note.placed.hand[0].pos, 1.f}};
    };
    if(stest.waited > 72 * 20)
    {
        t.check(false, name, va("timed out (step %d)", stest.step));
        stest.step = 99;
    }
    switch(stest.step)
    {
    case 0: // the shove, held by Play: its times from QC
        st.pdFollow = true;
        st.tool = Tool::PoseKeys;
        st.pdKind = st.subjKind >= 0 ? st.subjKind : st.pdKind;
        st.pdClass = st.subjClass >= 0 ? st.subjClass : st.pdClass;
        st.pdPose = agpose::Shove;
        st.pdPhase = 0;
        Cvar_SetValueQuick(&vr_rigstudio_onion, 0.f);
        Cvar_SetValueQuick(&vr_rigstudio_ikghost, 0.f);
        swingPlay();
        s.speed = 1.f;
        s.loop = false;
        next();
        break;
    case 1:
        if(s.timed && s.heldPose == agpose::Shove)
        {
            const float len = strikeLength();
            t.check(za::abs(len - 0.56f) < 0.002f, name, va("the shove's length %.4f (want 0.56)", len));
            t.check(s.keys[0] < s.keys[1] && s.keys[1] < s.keys[2] && s.keys[2] < len, name,
                va("its keys in order %.3f %.3f %.3f", s.keys[0], s.keys[1], s.keys[2]));
            Con_Printf("rigstudio strike: shove %.3f s, keys %.4f %.4f %.4f\n", len, s.keys[0], s.keys[1], s.keys[2]);
            next();
        }
        break;
    case 2: // played once, not looping: back on the key
        if(!s.playing && stest.waited > 2)
        {
            t.check(s.parked, name, "after one play: parked on the key");
            st.pdPhase = 0;
            strikePark();
            next();
        }
        break;
    case 3:
    case 4:
    case 5: // each key: the hand as the key puts it (settled)
        if(stest.waited == 12)
        {
            const int k = stest.step - 3;
            t.check(za::abs(s.t - s.keys[k]) < 1e-4f, name, va("parked on key %d: t %.4f", k, s.t));
            stest.hand[k] = hand();
            if(k < 2)
            {
                st.pdPhase = k + 1;
                strikePark();
            }
            else
            {
                t.check(glm::length(stest.hand[0] - stest.hand[1]) > 2.f, name,
                    va("the hand moves wind-up -> push: %.2f", glm::length(stest.hand[0] - stest.hand[1])));
                t.check(glm::length(stest.hand[1] - stest.hand[2]) > 0.5f, name,
                    va("the hand moves push -> recover: %.2f", glm::length(stest.hand[1] - stest.hand[2])));
                strikeSetTime((s.keys[0] + s.keys[1]) * 0.5f); // (scrubbed between)
            }
            next();
        }
        break;
    case 6:
        if(stest.waited == 12)
        {
            const glm::vec3 m = hand();
            const float a = glm::length(m - stest.hand[0]), b = glm::length(m - stest.hand[1]);
            t.check(!s.parked && a > 0.2f && b > 0.2f, name, va("scrubbed between: off both keys' hands (%.2f, %.2f)", a, b));
            Cbuf_AddText("screenshot\n");
            // playback at 0.5x: its time advanced by half the game's
            s.speed = 0.5f;
            s.loop = true;
            strikeSetTime(0.f);
            s.playing = true;
            stest.t0 = s.t;
            stest.s0 = sv.qcvm.time;
            next();
        }
        break;
    case 7:
        if(stest.waited == 30)
        {
            const float game = static_cast<float>(sv.qcvm.time - stest.s0);
            t.check(za::abs((s.t - stest.t0) - game * 0.5f) < 2.f * frameStep, name,
                va("0.5x: %.4f s of the act in %.4f s of the game", s.t - stest.t0, game));
            s.playing = false;
            // the onion skin: its ghosts taken (Auto Refresh), drawn
            Cvar_SetValueQuick(&vr_rigstudio_onion, 1.f);
            Cvar_SetValueQuick(&vr_rigstudio_onion_count, 2.f);
            Cvar_SetValueQuick(&vr_rigstudio_onion_step, 0.06f);
            Cvar_SetValueQuick(&vr_rigstudio_onion_mode, 0.f);
            st.pdPhase = 1;
            strikePark();
            gh.refreshedHash = 0;
            gh.hashAt = realtime - 10.0;
            next();
        }
        break;
    case 8:
        if(stest.waited > 4 && !gh.refreshing && !s.playing)
        {
            float times[2 * maxOnion + 2];
            int ks[2 * maxOnion + 2];
            const int m = onionTimes(times, ks, 2 * maxOnion + 2);
            int have = 0;
            for(int i = 0; i < m; i++)
            {
                have += ghostAt(times[i]) ? 1 : 0;
            }
            t.check(m == 4 && have == m, name, va("onion: %d of %d ghosts taken", have, m));
            const GhostPose* a = m > 0 ? ghostAt(times[0]) : nullptr;
            const GhostPose* b = m > 1 ? ghostAt(times[1]) : nullptr;
            t.check(a && b && glm::length(a->hand[0] - b->hand[0]) > 1.f, name, "onion: its ghosts' hands apart");
            t.check(za::abs(s.t - s.keys[1]) < 1e-4f, name, "onion: back on the key after taking them");
            if(stest.waited > 8)
            {
                Cbuf_AddText("screenshot\n");
                next();
            }
        }
        break;
    case 9: // the IK ghost: the push's key's right weapon pushed out of reach
        if(stest.waited == 3)
        {
            Cvar_SetValueQuick(&vr_rigstudio_onion, 0.f);
            Cvar_SetValueQuick(&vr_rigstudio_ikghost, 3.f);
            stest.code = agpose::code(st.pdKind, st.pdClass, agpose::Shove, 1);
            stest.key0 = agpose::value(stest.code);
            pushPoseUndo(stest.code);
            agpose::Key k = stest.key0;
            k.v[agpose::RPos].x += 30.f;
            agpose::set(stest.code, k);
            next();
        }
        break;
    case 10:
        if(stest.waited == 20)
        {
            const posestudio::Note& n = st.note;
            const float gap = glm::length(n.t.weapon[0].pos - n.placed.weapon[0].pos);
            t.check(gap > 5.f, name, va("IK ghost: the weapon held %.2f units off its set place", gap));
            Con_Printf("rigstudio strike: the right weapon set %.2f units past where the IK put it (reach %.2f)\n", gap,
                n.placed.reach[0]);
            Cbuf_AddText("screenshot\n");
            next();
        }
        break;
    case 11:
        if(stest.waited == 3)
        {
            undo();
            const agpose::Key k = agpose::value(stest.code);
            t.check(memcmp(&k, &stest.key0, sizeof(k)) == 0, name, "IK ghost: the key undone exactly");
            Cvar_SetValueQuick(&vr_rigstudio_ikghost, 0.f);
            // the fly camera: a second forward at Shift
            stest.cam0 = st.cam.focus;
            flyCamera(glm::vec3{1.f, 0.f, 0.f}, true, 0.05f);
            const float moved = glm::length(st.cam.focus - stest.cam0);
            const float want = za::max(vr_rigstudio_fly_speed.value, 1.f) * 3.f * 0.05f;
            t.check(za::abs(moved - want) < 0.01f && glm::dot(st.cam.focus - stest.cam0, st.view.fwd) > 0.f, name,
                va("fly: moved %.2f along the view (want %.2f)", moved, want));
            st.cam.focus = stest.cam0;
            next();
        }
        break;
    default:
        if(stest.step >= 12)
        {
            s.on = false;
            s.playing = false;
            st.tool = Tool::Rotate;
            stest.on = false;
            t.verdict(name);
        }
        break;
    }
}

void strikeInit()
{
    Cmd_AddCommand("vr_rigstudio_strike", strike_f);
    Cmd_AddCommand("vr_rigstudio_test_strike", testStrike_f);
}

} // namespace qvr::rigstudio::detail
