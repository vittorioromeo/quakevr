// vr_rigstudio_animtest.cpp -- vr_rigstudio_test_rig2anim: the Timeline's keys on the authored rig the subject is drawn
// on (RIGSTUDIO.md, "Animating the authored rig"): the dope sheet's bones are its (clavicles, spine, hands...), its keys
// held within its joints' limits and saved to its own files (rigs/<model>.rig2anim.txt, each key in its bone's frame).
//
// The steps (run once the subject stands; the studio's own commands, as a script would): its `load` animation's
// override cleared; the chest turned (0 15 0) and the right upper arm (0 -30 20) at frame 3, the chest (10 0 0) and the
// upper arm (0 20 0) at frame 7 (Auto Key); the right forearm bent 170 degrees at frame 5 (held within its hinge on
// the authored rig) and undone (its track gone); the pose mirrored at frame 3 (the left upper arm keyed as the right
// mirrored) and undone; saved, read back from the files (Revert): the same keys. Ends on
// `rigstudio rig2anim: N passed, M failed`. The keys stay published: `vr_agrunt_test 82` then measures them drawn.

#include "vr_testkit.hpp"
#include "vr_rigstudio_internal.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_rig2.hpp"

#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Acos.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string.h>

using namespace qvr;
using namespace qvr::rigstudio::detail;

namespace
{

struct AnimTest
{
    bool pending{false};
    int waited{0};
    int passed{0}, failed{0};
};
AnimTest animTest;

constexpr int animTestGiveUp = 72 * 60; // frames waiting for the subject (a safety net)

void check(const char* what, bool ok)
{
    Con_Printf("rigstudio rig2anim: %s: %s\n", ok ? "ok" : "FAIL", what);
    (ok ? animTest.passed : animTest.failed)++;
}

void run(const char* cmd)
{
    Cmd_ExecuteString(cmd, src_command);
}

[[nodiscard]] float turnDeg(const glm::quat& a, const glm::quat& b)
{
    const float d = za::clamp(za::abs(glm::dot(glm::normalize(a), glm::normalize(b))), 0.f, 1.f);
    return 2.f * za::acos(d) * (180.f / 3.14159265f);
}

// The `load` override's track of bone `name` (null: none).
[[nodiscard]] const rigdata::Track* trackNamed(const char* name)
{
    const int b = boneByName(name);
    const rigdata::AnimOverride* o = curOverride(false);
    for(za::SizeT i = 0; o && b >= 0 && i < o->tracks.size(); i++)
    {
        if(o->tracks[i].bone == b && !o->tracks[i].keys.empty())
        {
            return &o->tracks[i];
        }
    }
    return nullptr;
}

// Bone b's local at `frame` as the frame has it (the fits'), and with the keys (evaluate's).
void localsAt(int b, float frame, rigpose::Rigid& frameLocal, rigpose::Rigid& keyed)
{
    rigpose::Rigid F[maxBones], P[maxBones];
    evaluate(frame, F, P, false);
    const int p = st.rig->bones[b].parent;
    frameLocal = (p >= 0 && p < b) ? rigdata::compose(rigdata::inverse(P[p]), P[b]) : P[b];
    keyed = (p >= 0 && p < b) ? rigdata::compose(rigdata::inverse(F[p]), F[b]) : F[b];
}

void runTest()
{
    const rig2::Rig* a = st.rig->authored ? rig2::rigFor(st.model) : nullptr;
    Con_Printf("rigstudio rig2anim: the subject on the %s rig, %d bones\n", a ? "authored" : "derived", st.numBones);
    check(va("the dope sheet's bones the authored rig's (%d of %d; clavicle_r %d, spine %d, hand_l %d)", st.numBones,
              a ? a->numBones : -1, boneByName("clavicle_r"), boneByName("spine"), boneByName("hand_l")),
        a && st.numBones == a->numBones && boneByName("clavicle_r") >= 0 && boneByName("spine") >= 0 && boneByName("hand_l") >= 0);
    if(!a)
    {
        return;
    }
    run("vr_rigstudio_source anim load");
    check("the load animation picked", st.source == Source::Anim);
    if(rigdata::AnimOverride* o = curOverride(false))
    {
        o->tracks.clear(); // (the test's own keys only)
    }
    st.autoKey = true;
    run("vr_rigstudio_frame 3");
    run("vr_rigstudio_select chest");
    run("vr_rigstudio_rotate 0 15 0");
    run("vr_rigstudio_select upperarm_r");
    run("vr_rigstudio_rotate 0 -30 20");
    run("vr_rigstudio_frame 7");
    run("vr_rigstudio_select chest");
    run("vr_rigstudio_rotate 10 0 0");
    run("vr_rigstudio_select upperarm_r");
    run("vr_rigstudio_rotate 0 20 0");
    const rigdata::Track* chest = trackNamed("chest");
    const rigdata::Track* arm = trackNamed("upperarm_r");
    check(va("chest and upperarm_r keyed at frames 3 and 7 (%d, %d keys)", chest ? static_cast<int>(chest->keys.size()) : 0,
              arm ? static_cast<int>(arm->keys.size()) : 0),
        chest && arm && chest->keys.size() == 2 && arm->keys.size() == 2 && chest->keys[0].frame == 3.f &&
            chest->keys[1].frame == 7.f && arm->keys[0].frame == 3.f && arm->keys[1].frame == 7.f);
    if(!chest || !arm || chest->keys.size() != 2 || arm->keys.size() != 2)
    {
        return;
    }
    // Each key within its joint's limits (the authored rig), the keyed turn drawn at its frame.
    {
        float worst = 0.f, turned = 1e9f;
        bool within = true;
        for(const char* name : {"chest", "upperarm_r"})
        {
            const int b = boneByName(name);
            for(float f : {3.f, 7.f})
            {
                rigpose::Rigid fl, kl;
                localsAt(b, f, fl, kl);
                within = within && a && rig2::withinLimits(*a, b, rig2::anglesOf(*a, b, kl.rot), 0.05f);
                turned = za::min(turned, turnDeg(fl.rot, kl.rot));
                worst = za::max(worst, turnDeg(fl.rot, kl.rot));
            }
        }
        check(va("the keys drawn at their frames: turned %.1f..%.1f degrees off the frames, within the joints' limits (%s)",
                  turned, worst, "authored"),
            turned > 5.f && within);
    }
    // A bend past the forearm's hinge: held within it (the authored rig); undone.
    {
        run("vr_rigstudio_frame 5");
        run("vr_rigstudio_select forearm_r");
        run("vr_rigstudio_rotate 0 170 0");
        const rigdata::Track* fore = trackNamed("forearm_r");
        bool held = false;
        if(fore)
        {
            const int b = boneByName("forearm_r");
            rigpose::Rigid fl, kl;
            localsAt(b, 5.f, fl, kl);
            held = rig2::withinLimits(*a, b, rig2::anglesOf(*a, b, kl.rot), 0.05f);
        }
        check("forearm_r bent 170 degrees: keyed, held within its hinge", fore && held);
        run("vr_rigstudio_undo");
        check("undone: its track gone, the others kept", !trackNamed("forearm_r") && trackNamed("chest") && trackNamed("upperarm_r"));
    }
    // Mirror pose at frame 3: the left upper arm as the right mirrored (absolute locals); undone.
    {
        run("vr_rigstudio_frame 3");
        const int r = boneByName("upperarm_r"), l = boneByName("upperarm_l");
        rigpose::Rigid flR, klR, flL, klL;
        localsAt(r, 3.f, flR, klR); // (the right as keyed: the left takes it mirrored, the right the left's)
        pushUndo("mirror");
        mirrorPose();
        localsAt(l, 3.f, flL, klL);
        const glm::vec3 rv = rigdata::toRotDeg(klR.rot);
        glm::quat mirrored = rigdata::toQuat(glm::vec3{-rv.x, rv.y, -rv.z});
        (void)rig2::clampLocal(*a, l, mirrored); // (held within the left's own limits)
        const float off = turnDeg(mirrored, klL.rot);
        check(va("mirrored: upperarm_l keyed as upperarm_r mirrored (%.2f degrees off)", off), trackNamed("upperarm_l") && off < 3.f);
        run("vr_rigstudio_undo");
        check("the mirror undone", !trackNamed("upperarm_l") && trackNamed("upperarm_r"));
    }
    // Saved and read back.
    rigdata::Track keep[2] = {*trackNamed("chest"), *trackNamed("upperarm_r")};
    save();
    char path[MAX_OSPATH];
    rigdata::rigPath(st.doc.base.cStr(), path, sizeof(path), true);
    za::String text;
    const bool read = files::readText(path, text);
    check(va("saved to %s (qvrrig2anim)", path),
        read && strstr(text.cStr(), "qvrrig2anim 1") && strstr(text.cStr(), "track upperarm_r") &&
            strstr(text.cStr(), "override load"));
    revert();
    run("vr_rigstudio_source anim load");
    float worst = 0.f;
    bool same = true;
    for(int i = 0; i < 2; i++)
    {
        const rigdata::Track* t = trackNamed(i == 0 ? "chest" : "upperarm_r");
        same = same && t && t->keys.size() == keep[i].keys.size();
        for(za::SizeT k = 0; same && k < t->keys.size(); k++)
        {
            same = same && t->keys[k].frame == keep[i].keys[k].frame && t->keys[k].interp == keep[i].keys[k].interp;
            worst = za::max(worst, glm::length(t->keys[k].value.rot - keep[i].keys[k].value.rot));
            worst = za::max(worst, glm::length(t->keys[k].value.off - keep[i].keys[k].value.off));
        }
    }
    check(va("read back from the file: the same keys (%.4f most off)", worst), same && worst < 0.01f);
    publish(); // (the game draws them: vr_agrunt_test 82)
}

} // namespace

namespace qvr::rigstudio::detail
{

void animTestTick()
{
    if(!animTest.pending)
    {
        return;
    }
    if(!st.open || !st.rig || !hdrOf() || st.subject <= 0)
    {
        if(++animTest.waited > animTestGiveUp)
        {
            animTest.pending = false;
            Con_Printf("rigstudio rig2anim: FAIL: timed out waiting for the subject\nrigstudio rig2anim: 0 passed, 1 failed\n");
        }
        return;
    }
    animTest.pending = false;
    runTest();
    qvr::testkit::verdict("rigstudio rig2anim", animTest.passed, animTest.failed);
}

void testAnim_f()
{
    animTest = AnimTest{};
    animTest.pending = true;
    Con_Printf("rigstudio rig2anim: waiting for the subject\n");
}

} // namespace qvr::rigstudio::detail
