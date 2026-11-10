// vr_animrec.cpp -- the animation recorder (vr_animrec.hpp; docs/vr-port/ANIMATIONS.md): recording, the countdown,
// the events, saving, the smoothing command and the preview on the flat-screen hands (a ghost in the headset). The data,
// the file and the smoothing are vr_animrec_data.cpp's.

#include "vr_animrec.hpp"
#include "vr_animrec_internal.hpp"

#include "vr_body.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_flatplay.hpp"
#include "vr_geom.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_protocol.hpp"
#include "vr_recbutton.hpp"
#include "vr_testkit.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"

#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/ToString.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Clamp.hpp"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


namespace qvr::animrec
{

namespace
{

constexpr double minSeconds = 0.2; // a shorter recording: an accidental press, dropped
constexpr float spotNear = 0.4f;   // m: an event's hand this near a hotspot is "at" it (Event::spot)

enum class Rec
{
    Off,
    Countdown,
    Recording
};

// The recorder's state (one recording at a time).
struct Recorder
{
    Rec rec{Rec::Off};
    recbutton::Countdown countdown;
    bool taken[HAND_COUNT][recbutton::buttonCount]{};
    double start{0.0};
    double stopAt{-1.0};   // a timed recording's end (vr_ar_record <name> <seconds>), else -1
    za::String name;       // the name asked for ("": made from the weapons and the date)
    za::String stamp;      // the start's date, for the file name
    Anim anim;
    int wid[HAND_COUNT]{};
    int clip[HAND_COUNT]{};
    za::String lastPath;   // the last recording saved (its .anim)
    za::String lastName;   // and its name (vr_ar_play's, vr_ar_smooth's default)
    za::String feedback;   // "SAVED ..." shown until feedbackUntil
    double feedbackUntil{0.0};
    char status[160]{};
    char last[200]{};
};
Recorder recorder;

// The preview (vr_ar_play): a recording played on the hands, blended in and out of the game's own pose.
struct Preview
{
    bool active{false};
    Anim anim;
    za::String file;
    double t{0.0};          // the recording's time
    double pauseLeft{0.0};  // between loops: the game's own pose for this long
    bool once{false};       // play once whatever vr_ar_play_loop (the self-test's)
    int cycles{0};          // loops done
    za::Vector<qmodel_t*> models; // the ghost's weapon models, by WeaponChange index
};
Preview preview;

// vr_ar_selftest (tests: Misc/quakevr/animrec_test.sh): the recorder's path end to end with a definite end.
enum class TestPhase
{
    Off,
    Recording,
    Playing
};
struct SelfTest
{
    TestPhase phase{TestPhase::Off};
    testkit::Tally tally;
    int stage{0};            // the playback's variants: 0 level, 1 looking up, 2 looking down, 3 crouched
    float jumpBlend{0.f};    // units: a hand's largest step in a frame while blending
    float jumpPlay{0.f};     // and while played at full weight
    float offTarget{0.f};    // units: at full weight, the hands' largest distance from the recording's pose
    int fullFrames{0};
    glm::vec3 lastPos[HAND_COUNT]{};
    bool haveLast{false};
    double startedAt{0.0};
};
SelfTest selfTest;

void check(bool ok, const char* what)
{
    selfTest.tally.check(ok);
    Con_Printf("vr_ar_selftest: %s %s\n", ok ? "PASS" : "FAIL", what);
}

void verdict()
{
    selfTest.tally.verdict("vr_ar_selftest");
    selfTest.phase = TestPhase::Off;
}

[[nodiscard]] za::String animsDir()
{
    const za::String dir = za::String{com_gamedir} + "/anims";
    Sys_mkdir(dir.cStr());
    return dir;
}

// A recording's file from a name: <name> as given if it has a folder or ends in .anim, else anims/<name>.anim (its
// .smooth.anim with `smoothed` when there is one).
[[nodiscard]] za::String pathOf(const za::String& name, bool smoothed)
{
    if(za::StringView{name}.find('/') != za::StringView::nPos || za::StringView{name}.find('\\') != za::StringView::nPos)
    {
        return name;
    }
    const za::String dir = animsDir();
    if(name.size() > 5 && za::StringView{name}.substrByPosLen(name.size() - 5) == ".anim")
    {
        return dir + "/" + name;
    }
    if(smoothed && files::isFile((dir + "/" + name + ".smooth.anim").cStr()))
    {
        return dir + "/" + name + ".smooth.anim";
    }
    return dir + "/" + name + ".anim";
}

[[nodiscard]] za::String safeName(const za::String& s)
{
    za::String out;
    for(za::SizeT i = 0; i < s.size(); i++)
    {
        const char c = s[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                        c == '.';
        out += ok ? c : '_';
    }
    return out.empty() ? za::String{"anim"} : out;
}

// "progs/v_shot.mdl" -> "v_shot"; "" -> "empty".
[[nodiscard]] za::String shortModel(const za::String& model)
{
    if(model.empty())
    {
        return "empty";
    }
    za::StringView v = files::fileName(model);
    const za::SizeT dot = v.find('.');
    return za::String{dot == za::StringView::nPos ? v : v.substrByPosLen(0, dot)};
}

[[nodiscard]] za::String heldModel(int h)
{
    const int model = cl.stats[h == HAND_MAIN ? int{STAT_WEAPON} : int{protocol::STAT_QVR_WEAPONMODEL2}];
    return model > 0 && model < MAX_MODELS && cl.model_precache[model] ? za::String{cl.model_precache[model]->name} : za::String{};
}

[[nodiscard]] int heldWid(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2];
}

[[nodiscard]] int heldClip(int h)
{
    return cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONCLIP : protocol::STAT_QVR_WEAPONCLIP2];
}

// The two frames a pose is measured in (vr_animrec_internal.hpp): the head's and the body's.
struct Frames
{
    glm::vec3 headO{0.f};
    glm::mat3 headB{1.f};
    glm::vec3 bodyO{0.f};
    glm::mat3 bodyB{1.f};
};

[[nodiscard]] Frames framesOf(const hands::State& s)
{
    Frames f;
    f.headO = s.head;
    f.headB = hands::basisOf(s.headAngles);
    f.bodyO = hands::bodyAnchor(s, glm::vec3{0.f});
    f.bodyB = hands::basisOf(glm::vec3{0.f, s.bodyYaw, 0.f});
    return f;
}

[[nodiscard]] Pose relative(const glm::vec3& o, const glm::mat3& b, const glm::vec3& pos, const glm::vec3& angles)
{
    const float u2m = 1.f / units::metresToUnits();
    Pose p;
    p.pos = glm::transpose(b) * (pos - o) * u2m;
    p.rot = glm::normalize(glm::quat_cast(glm::transpose(b) * hands::basisOf(angles)));
    return p;
}

// The recorder's hotspots in the world now: the pouches and the holsters (vr_body.hpp).
[[nodiscard]] za::Vector<Spot> spotsNow(const hands::State& s)
{
    za::Vector<Spot> out;
    out.pushBack({"ammo_pouch", body::ammoPouchPosition(s)});
    out.pushBack({"grenade_pouch", body::pouchPosition(s)});
    constexpr const char* holsterNames[body::HolsterCount] = {
        "left_shoulder", "right_shoulder", "left_hip", "right_hip", "left_upper", "right_upper"};
    const body::HolsterPositions hp = body::holsterPositions(s);
    for(int i = 0; i < body::HolsterCount; i++)
    {
        out.pushBack({holsterNames[i], hp[i]});
    }
    return out;
}

[[nodiscard]] Frame captureFrame(const hands::State& s, double t)
{
    const Frames fr = framesOf(s);
    Frame f;
    f.t = t;
    f.head = relative(fr.bodyO, fr.bodyB, s.head, s.headAngles);
    f.crouch = s.crouchRatio;
    for(int h = 0; h < HAND_COUNT; h++)
    {
        HandFrame& hf = f.hands[h];
        hf.head = relative(fr.headO, fr.headB, s.pos[h], s.rot[h]);
        hf.body = relative(fr.bodyO, fr.bodyB, s.pos[h], s.rot[h]);
        for(int k = 0; k < fingerCount; k++)
        {
            hf.curl[k] = view::fingerCurl(h, k);
        }
        const HandInput& in = tracking().input.hands[h]; // (the frame's: the backend's, or a take's)
        hf.grip = in.gripValue;
        hf.trigger = in.triggerValue;
        hf.wid = heldWid(h);
    }
    return f;
}

void say(const char* text, double seconds)
{
    recorder.feedback = text;
    recorder.feedbackUntil = realtime + seconds;
    Con_Printf("Animation recorder: %s\n", text);
}

// ----------------------------------------------------------------------------
// Recording
// ----------------------------------------------------------------------------

void beginRecording()
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        recorder.rec = Rec::Off;
        say("not in a game with hands: not recording", 2.0);
        menuui::uiSound(menuui::UiSound::Deny);
        return;
    }
    Recorder& r = recorder;
    r.rec = Rec::Recording;
    r.start = realtime;
    r.anim = Anim{};
    const time_t now = time(nullptr);
    char date[64], stamp[64];
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now));
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
    r.stamp = stamp;
    r.anim.date = date;
    r.anim.map = cl.mapname;
    const Backend* be = backend();
    r.anim.source = be ? va("%s (%s)", be->name(), be->runtimeName()) : (flatplay::active() ? "flat screen" : "no headset");
    r.anim.note = va("vr_world_scale %g, vr_height_calibration %g", vr_world_scale.value, vr_height_calibration.value);
    const Frames fr = framesOf(s);
    const float u2m = 1.f / units::metresToUnits();
    for(const Spot& sp : spotsNow(s))
    {
        r.anim.spots.pushBack({sp.name, glm::transpose(fr.bodyB) * (sp.pos - fr.bodyO) * u2m});
    }
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        r.wid[h] = heldWid(h);
        r.clip[h] = heldClip(h);
        r.anim.weapons.pushBack({0.0, h, r.wid[h], heldModel(h)});
    }
    r.anim.name = r.name.empty() ? shortModel(heldModel(HAND_MAIN)) + "-" + shortModel(heldModel(HAND_OFF)) : r.name;
    haptics::pulse(recbutton::binding(vr_ar_button.value).hand, 0.08f, 0.9f);
    menuui::uiSound(menuui::UiSound::Talk);
    Con_Printf("Animation recorder: recording %s\n", r.anim.name.cStr());
}

// A free file name: <name>.anim, or <name>_<date>.anim made from the weapons, never over another.
[[nodiscard]] za::String freePath()
{
    const Recorder& r = recorder;
    const za::String base = animsDir() + "/" + safeName(r.name.empty() ? r.anim.name + "_" + r.stamp : r.name);
    za::String path = base + ".anim";
    for(int n = 2; files::exists(path.cStr()); n++)
    {
        path = base + "-" + za::toString(n) + ".anim";
    }
    return path;
}

void finishRecording()
{
    Recorder& r = recorder;
    r.rec = Rec::Off;
    r.stopAt = -1.0;
    const double held = r.anim.duration();
    if(r.anim.frames.size() < 2 || held < minSeconds)
    {
        say(va("too short (%.2f s): not kept", held), 2.0);
        menuui::uiSound(menuui::UiSound::Deny);
        return;
    }
    r.anim.rawFrames = static_cast<int>(r.anim.frames.size());
    const za::String path = freePath();
    // (Written at once: a minute is some 4300 frames, 2 MB of text, a few tens of milliseconds in a debug tool.)
    if(!files::writeText(path.cStr(), toText(r.anim)))
    {
        say(va("NOT SAVED: could not write %s", path.cStr()), 6.0);
        menuui::uiSound(menuui::UiSound::Deny);
        return;
    }
    r.lastPath = path;
    const za::StringView file = files::fileName(path);
    r.lastName = za::String{file.substrByPosLen(0, file.size() - 5)};
    say(va("saved anims/%s (%.2f s, %d frames, %d events)", za::String{file}.cStr(), held, static_cast<int>(r.anim.frames.size()),
            static_cast<int>(r.anim.events.size())),
        2.5);
    haptics::pulse(recbutton::binding(vr_ar_button.value).hand, 0.12f, 0.6f);
    menuui::uiSound(menuui::UiSound::Select);
}

void startCountdown(const za::String& name, double seconds)
{
    Recorder& r = recorder;
    if(r.rec != Rec::Off)
    {
        return;
    }
    r.name = name;
    r.stopAt = seconds > 0.0 ? seconds : -1.0;
    r.rec = Rec::Countdown;
    r.countdown.start(realtime, vr_ar_countdown.value);
    Con_Printf("Animation recorder: recording in %.1f s\n", vr_ar_countdown.value);
}

void stopRecording()
{
    Recorder& r = recorder;
    if(r.rec == Rec::Countdown)
    {
        r.rec = Rec::Off;
        r.countdown.cancel();
        say("countdown cancelled", 1.5);
        return;
    }
    if(r.rec == Rec::Recording)
    {
        finishRecording();
    }
}

void recordFrame()
{
    Recorder& r = recorder;
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        Con_Printf("Animation recorder: the game ended; the recording is kept as it is\n");
        finishRecording();
        return;
    }
    const double t = realtime - r.start;
    r.anim.frames.pushBack(captureFrame(s, t));
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        const int wid = heldWid(h);
        const int clip = heldClip(h);
        if(wid != r.wid[h])
        {
            r.wid[h] = wid;
            r.anim.weapons.pushBack({t, h, wid, heldModel(h)});
        }
        else if(clip > r.clip[h])
        {
            event("load", h, va("magazine %d -> %d", r.clip[h], clip));
        }
        r.clip[h] = clip;
    }
    if((r.stopAt > 0.0 && t >= r.stopAt) || t >= za::max(1.f, vr_ar_maxtime.value))
    {
        finishRecording();
    }
}

// ----------------------------------------------------------------------------
// The preview
// ----------------------------------------------------------------------------

// The recording's weight against the game's pose now: blended in over vr_ar_play_blend, out over the last as long.
[[nodiscard]] float previewWeight()
{
    if(!preview.active || preview.pauseLeft > 0.0)
    {
        return 0.f;
    }
    const float blend = za::max(0.f, vr_ar_play_blend.value);
    if(blend <= 0.f)
    {
        return 1.f;
    }
    const float t = static_cast<float>(preview.t);
    const float left = static_cast<float>(preview.anim.duration() - preview.t);
    return geom::smooth01(za::min(t / blend, left / blend, 1.f));
}

// Where the recording puts hand `h` now, in the world (the tweaks applied: mirrored, turned, scaled, moved).
void previewTarget(const hands::State& s, const Frame& f, int h, glm::vec3& pos, glm::vec3& rot)
{
    const bool mirror = vr_ar_play_mirror.value != 0.f;
    const HandFrame& hf = f.hands[mirror ? 1 - h : h];
    const bool onHead = vr_ar_play_frame.value != 0.f;
    Pose p = onHead ? hf.head : hf.body;
    if(mirror) // left to right: y negated, and the turn reflected in that plane
    {
        p.pos.y = -p.pos.y;
        p.rot = glm::quat{p.rot.w, -p.rot.x, p.rot.y, -p.rot.z};
    }
    const glm::mat3 tweak = hands::basisOf(glm::vec3{vr_ar_play_pitch.value, vr_ar_play_yaw.value, vr_ar_play_roll.value});
    const glm::vec3 local = tweak * (p.pos * vr_ar_play_scale.value) +
                            glm::vec3{vr_ar_play_x.value, vr_ar_play_y.value, vr_ar_play_z.value} * 0.01f;
    const glm::mat3 localRot = tweak * glm::mat3_cast(p.rot);
    const Frames fr = framesOf(s);
    const glm::vec3 o = onHead ? fr.headO : fr.bodyO;
    const glm::mat3 b = onHead ? fr.headB : fr.bodyB;
    pos = o + b * local * units::metresToUnits();
    const glm::mat3 world = b * localRot;
    rot = hands::anglesFromVectors(world[0], world[2]);
}

[[nodiscard]] qmodel_t* modelNamed(const za::String& name)
{
    if(name.empty())
    {
        return nullptr;
    }
    for(int i = 1; i < MAX_MODELS && cl.model_precache[i]; i++)
    {
        if(name == cl.model_precache[i]->name)
        {
            return cl.model_precache[i];
        }
    }
    return nullptr;
}

bool startPreview(const za::String& name, bool once)
{
    const za::String path = pathOf(name, vr_ar_play_smoothed.value != 0.f);
    za::String text, error;
    if(!files::readText(path.cStr(), text))
    {
        Con_Printf("vr_ar_play: can't read %s\n", path.cStr());
        return false;
    }
    Anim a;
    if(!fromText(text, a, error))
    {
        Con_Printf("vr_ar_play: %s: %s\n", path.cStr(), error.cStr());
        return false;
    }
    preview = Preview{};
    preview.anim = ZA_MOVE(a);
    preview.file = path;
    preview.active = true;
    preview.once = once;
    Con_Printf("vr_ar_play: %s (%.2f s, %d frames%s, %d events)\n", za::String{files::fileName(path)}.cStr(),
        preview.anim.duration(), static_cast<int>(preview.anim.frames.size()), preview.anim.smoothed ? ", smoothed" : "",
        static_cast<int>(preview.anim.events.size()));
    return true;
}

void advancePreview()
{
    if(!preview.active)
    {
        return;
    }
    const double dt = host_frametime * za::clamp(static_cast<double>(vr_ar_play_speed.value), 0.05, 4.0);
    if(preview.pauseLeft > 0.0)
    {
        preview.pauseLeft -= host_frametime;
        if(preview.pauseLeft <= 0.0)
        {
            preview.pauseLeft = 0.0;
            preview.t = 0.0;
        }
        return;
    }
    const double before = preview.t;
    preview.t += dt;
    for(const Event& e : preview.anim.events)
    {
        if(e.t > before && e.t <= preview.t)
        {
            Con_Printf("vr_ar_play: %.2f s %s %s at %s %s\n", e.t, e.hand == 1 ? "main" : e.hand == 0 ? "off" : "both",
                e.kind.cStr(), e.spot.cStr(), e.detail.cStr());
        }
    }
    if(preview.t >= preview.anim.duration())
    {
        preview.cycles++;
        if(preview.once || vr_ar_play_loop.value == 0.f)
        {
            preview.active = false;
            Con_Printf("vr_ar_play: done\n");
            return;
        }
        preview.t = preview.anim.duration();
        preview.pauseLeft = 0.5; // the game's own pose a moment between the loops
    }
}

// ----------------------------------------------------------------------------
// The self-test (vr_ar_selftest)
// ----------------------------------------------------------------------------

void testRecorded()
{
    // The file as saved, read back.
    za::String text, error;
    Anim back, smoothed;
    const bool read = files::readText(recorder.lastPath.cStr(), text) && fromText(text, back, error);
    check(read, va("the recording read back (%s%s)", recorder.lastName.cStr(), error.empty() ? "" : (" " + error).cStr()));
    if(!read)
    {
        return verdict();
    }
    check(back.frames.size() >= 72, va("%d frames recorded (at least 72)", static_cast<int>(back.frames.size())));
    // The motion is in it: the main hand moved (the mock swing) relative to the body.
    float travel = 0.f;
    for(za::SizeT i = 1; i < back.frames.size(); i++)
    {
        travel += glm::length(back.frames[i].hands[HAND_MAIN].body.pos - back.frames[i - 1].hands[HAND_MAIN].body.pos);
    }
    check(travel > 0.3f, va("the main hand's path %.2f m (the mock swing: over 0.3)", travel));
    // The two frames agree: the hand relative to the head, put back on the head as recorded relative to the body,
    // lands where the body frame says (one frame's).
    {
        const Frame& f = back.frames[back.frames.size() / 2];
        const glm::mat3 headB = glm::mat3_cast(f.head.rot);
        const glm::vec3 viaHead = f.head.pos + headB * f.hands[HAND_MAIN].head.pos;
        const float d = glm::length(viaHead - f.hands[HAND_MAIN].body.pos);
        check(d < 0.002f, va("head and body frames agree (%.2f mm)", d * 1000.f));
    }
    check(!back.weapons.empty() && back.spots.size() >= 2, va("weapons (%d lines) and hotspots (%d) in the header",
        static_cast<int>(back.weapons.size()), static_cast<int>(back.spots.size())));
    bool sawFire = false;
    for(const Event& e : back.events)
    {
        sawFire = sawFire || e.kind == "fire";
    }
    check(sawFire, va("a shot recorded as an event (%d events)", static_cast<int>(back.events.size())));

    // Smoothed: fewer frames, within the tolerances.
    SmoothParams p;
    p.tau = vr_ar_smooth_tau.value;
    p.posTol = vr_ar_smooth_pos.value;
    p.angTol = vr_ar_smooth_ang.value;
    p.curlTol = vr_ar_smooth_curl.value;
    const SmoothResult r = smooth(back, p, smoothed);
    check(r.after < r.before / 2, va("smoothed to %d keys of %d frames (under half)", r.after, r.before));
    check(r.maxPos <= p.posTol * 1.0001f && r.maxAng <= p.angTol * 1.0001f && r.maxCurl <= p.curlTol * 1.0001f,
        va("its spline within the tolerances: %.2f mm (%.1f), %.2f deg (%.1f), %.3f (%.2f)", r.maxPos * 1000.f,
            p.posTol * 1000.f, r.maxAng, p.angTol, r.maxCurl, p.curlTol));
    check(r.rawPos > 0.0002f, va("the jitter filter took %.2f mm of jitter out (over 0.2 with vr_mock_shake)", r.rawPos * 1000.f));
    const za::String smoothPath = animsDir() + "/" + recorder.lastName + ".smooth.anim";
    Anim smoothBack;
    const bool wrote = files::writeText(smoothPath.cStr(), toText(smoothed)) && files::readText(smoothPath.cStr(), text) &&
                       fromText(text, smoothBack, error);
    check(wrote && smoothBack.frames.size() == smoothed.frames.size() && smoothBack.smoothed,
        va("the smoothed file written and read back (%s.smooth.anim)", recorder.lastName.cStr()));
    // Its spline against the raw recording, through the file (Euler angles, rounded numbers): within the tolerance
    // plus what the filter took out.
    float worst = 0.f;
    for(const Frame& f : back.frames)
    {
        const Frame g = sample(smoothBack, f.t);
        for(int h = 0; h < HAND_COUNT; h++)
        {
            worst = za::max(worst, glm::length(g.hands[h].body.pos - f.hands[h].body.pos));
        }
    }
    check(worst <= p.posTol + r.rawPos + 0.0005f,
        va("the smoothed file's spline %.2f mm from the raw hands (tolerance + jitter: %.2f)", worst * 1000.f,
            (p.posTol + r.rawPos) * 1000.f));
    verdict();
}

void testPlayFrame()
{
    SelfTest& st = selfTest;
    const hands::State& s = hands::current();
    if(!preview.active)
    {
        // A variant done: the next, else the checks.
        Cbuf_AddText("-flatcrouch\n");
        st.stage++;
        if(st.stage <= 3)
        {
            cl.viewangles[PITCH] = st.stage == 1 ? -80.f : st.stage == 2 ? 80.f : 0.f;
            if(st.stage == 3)
            {
                Cbuf_AddText("+flatcrouch\n");
            }
            st.haveLast = false;
            startPreview(preview.file.empty() ? recorder.lastName : preview.file, true);
            return;
        }
        Con_Printf("vr_ar_selftest: blend step %.2f units/frame, played %.2f, off target %.3f units over %d frames\n",
            st.jumpBlend, st.jumpPlay, st.offTarget, st.fullFrames);
        check(st.fullFrames > 50, va("%d frames at full weight", st.fullFrames));
        check(st.offTarget < 0.05f, va("at full weight the hands are on the recording (%.3f units off)", st.offTarget));
        check(st.jumpBlend <= st.jumpPlay * 1.5f + 0.5f,
            va("blending in and out is smooth: %.2f units a frame at most (%.2f played)", st.jumpBlend, st.jumpPlay));
        cl.viewangles[PITCH] = 0.f;
        return verdict();
    }
    if(!s.valid)
    {
        return;
    }
    const float w = previewWeight();
    const Frame f = sample(preview.anim, preview.t);
    const Frames fr = framesOf(s);
    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 pos, rot;
        previewTarget(s, f, h, pos, rot);
        const glm::vec3 onBody = glm::transpose(fr.bodyB) * (s.pos[h] - fr.bodyO);
        if(st.haveLast)
        {
            const float step = glm::length(onBody - st.lastPos[h]);
            float& worst = w >= 0.999f ? st.jumpPlay : st.jumpBlend;
            worst = za::max(worst, step);
        }
        st.lastPos[h] = onBody;
        if(w >= 0.999f)
        {
            st.offTarget = za::max(st.offTarget, glm::length(s.pos[h] - pos));
        }
    }
    st.haveLast = true;
    if(w >= 0.999f)
    {
        st.fullFrames++;
    }
}

// ----------------------------------------------------------------------------
// Commands
// ----------------------------------------------------------------------------

void record_f()
{
    if(recorder.rec != Rec::Off)
    {
        Con_Printf("vr_ar_record: already %s\n", recorder.rec == Rec::Countdown ? "counting down" : "recording");
        return;
    }
    const za::String name = Cmd_Argc() > 1 ? za::String{Cmd_Argv(1)} : za::String{vr_ar_name.string};
    startCountdown(name, Cmd_Argc() > 2 ? atof(Cmd_Argv(2)) : 0.0);
}

void stop_f()
{
    stopRecording();
}

void play_f()
{
    const za::String name = Cmd_Argc() > 1 ? za::String{Cmd_Argv(1)} : recorder.lastName;
    if(name.empty())
    {
        Con_Printf("vr_ar_play <name>: plays quakevr/anims/<name>.anim (its .smooth.anim with vr_ar_play_smoothed 1)\n");
        return;
    }
    startPreview(name, false);
}

void smooth_f()
{
    const za::String name = Cmd_Argc() > 1 ? za::String{Cmd_Argv(1)} : recorder.lastName;
    if(name.empty())
    {
        Con_Printf("vr_ar_smooth <name>: writes quakevr/anims/<name>.smooth.anim\n");
        return;
    }
    const za::String path = pathOf(name, false);
    za::String text, error;
    Anim a, out;
    if(!files::readText(path.cStr(), text) || !fromText(text, a, error))
    {
        Con_Printf("vr_ar_smooth: can't read %s %s\n", path.cStr(), error.cStr());
        return;
    }
    SmoothParams p;
    p.tau = vr_ar_smooth_tau.value;
    p.posTol = vr_ar_smooth_pos.value;
    p.angTol = vr_ar_smooth_ang.value;
    p.curlTol = vr_ar_smooth_curl.value;
    const SmoothResult r = smooth(a, p, out);
    za::String outPath = path;
    if(outPath.size() > 5)
    {
        outPath = za::String{za::StringView{outPath}.substrByPosLen(0, outPath.size() - 5)};
    }
    outPath += ".smooth.anim";
    if(!files::writeText(outPath.cStr(), toText(out)))
    {
        Con_Printf("vr_ar_smooth: can't write %s\n", outPath.cStr());
        return;
    }
    Con_Printf("vr_ar_smooth: %s: %d frames -> %d keys; max error %.2f mm, %.2f deg, %.3f; jitter removed %.2f mm\n",
        za::String{files::fileName(outPath)}.cStr(), r.before, r.after, r.maxPos * 1000.f, r.maxAng, r.maxCurl,
        r.rawPos * 1000.f);
}

void list_f()
{
    const za::String dir = animsDir();
    int n = 0;
    files::forEachEntry(dir.cStr(), [&](const char* name, bool isDirectory) {
        if(!isDirectory && strstr(name, ".anim"))
        {
            Con_Printf("  %s\n", name);
            n++;
        }
    });
    Con_Printf("%d recordings in %s\n", n, dir.cStr());
}

// vr_ar_selftest record [name]: a recording of 2 s (after vr_ar_countdown) through the real path, saved, read back and
// smoothed, checked; vr_ar_selftest play <name>: played on the flat-screen hands once level, looking up, looking down
// and crouched, checked. Ends with "vr_ar_selftest: N passed, M failed".
void selftest_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_ar_selftest record [name] | play <name>\n");
        return;
    }
    selfTest = SelfTest{};
    selfTest.startedAt = realtime;
    const za::String what = Cmd_Argv(1);
    const za::String name = Cmd_Argc() > 2 ? za::String{Cmd_Argv(2)} : za::String{"selftest"};
    if(what == "record")
    {
        // (A name that is free: the last test's file stays for the play run.)
        const za::String path = animsDir() + "/" + name + ".anim";
        if(files::exists(path.cStr()))
        {
            files::rename(path.cStr(), (animsDir() + "/" + name + ".old.anim").cStr());
        }
        selfTest.phase = TestPhase::Recording;
        startCountdown(name, 2.0);
        return;
    }
    if(what == "play")
    {
        if(!flatplay::active())
        {
            check(false, "the play test needs -flatscreenmode");
            return verdict();
        }
        vr_ar_play_smoothed.value = 1.f;
        if(!startPreview(name, true))
        {
            check(false, va("%s plays", name.cStr()));
            return verdict();
        }
        recorder.lastName = name;
        selfTest.phase = TestPhase::Playing;
        return;
    }
    Con_Printf("vr_ar_selftest record [name] | play <name>\n");
}

} // namespace

// ----------------------------------------------------------------------------
// The API
// ----------------------------------------------------------------------------

void init()
{
    Cmd_AddCommand("vr_ar_record", record_f);
    Cmd_AddCommand("vr_ar_stop", stop_f);
    Cmd_AddCommand("vr_ar_play", play_f);
    Cmd_AddCommand("vr_ar_play_stop", stopPreview);
    Cmd_AddCommand("vr_ar_smooth", smooth_f);
    Cmd_AddCommand("vr_ar_list", list_f);
    Cmd_AddCommand("vr_ar_selftest", selftest_f);
}

void shutdown()
{
    if(recorder.rec == Rec::Recording)
    {
        finishRecording();
    }
}

bool recording()
{
    return recorder.rec != Rec::Off;
}

void toggleRecording()
{
    if(recorder.rec == Rec::Off)
    {
        startCountdown(za::String{vr_ar_name.string}, 0.0);
    }
    else
    {
        stopRecording();
    }
}

void playLast()
{
    if(recorder.lastName.empty())
    {
        say("nothing recorded yet (vr_ar_play <name> plays another)", 2.0);
        return;
    }
    startPreview(recorder.lastName, false);
}

void smoothLast()
{
    if(recorder.lastName.empty())
    {
        say("nothing recorded yet", 2.0);
        return;
    }
    Cbuf_AddText(va("vr_ar_smooth \"%s\"\n", recorder.lastName.cStr()));
}

void stopPreview()
{
    preview.active = false;
}

const char* statusLine()
{
    Recorder& r = recorder;
    if(r.rec == Rec::Countdown)
    {
        snprintf(r.status, sizeof(r.status), "Starting in %.1f s", r.countdown.left(realtime));
    }
    else if(r.rec == Rec::Recording)
    {
        snprintf(r.status, sizeof(r.status), "RECORDING %s: %.1f s", r.anim.name.cStr(), realtime - r.start);
    }
    else if(preview.active)
    {
        snprintf(r.status, sizeof(r.status), "Playing %s: %.2f / %.2f s", za::String{files::fileName(preview.file)}.cStr(),
            preview.t, preview.anim.duration());
    }
    else
    {
        snprintf(r.status, sizeof(r.status), "Next: %s", vr_ar_name.string[0] ? vr_ar_name.string : "named by its weapons and the date");
    }
    return r.status;
}

const char* lastLine()
{
    snprintf(recorder.last, sizeof(recorder.last), "Last: %s",
        recorder.lastName.empty() ? "none this session" : recorder.lastName.cStr());
    return recorder.last;
}

void event(const char* kind, int hand, const char* detail)
{
    Recorder& r = recorder;
    if(r.rec != Rec::Recording)
    {
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    Event e;
    e.t = realtime - r.start;
    e.hand = hand == HAND_OFF || hand == HAND_MAIN ? hand : -1;
    e.kind = kind && kind[0] ? kind : "event";
    e.detail = detail ? detail : "";
    const int h = e.hand >= 0 ? e.hand : HAND_MAIN;
    if(e.detail.empty())
    {
        e.detail = heldModel(h); // (a shot's: the gun's model)
    }
    const Frames fr = framesOf(s);
    const float u2m = 1.f / units::metresToUnits();
    const glm::mat3 toBody = glm::transpose(fr.bodyB);
    e.body = toBody * (s.pos[h] - fr.bodyO) * u2m;
    float best = spotNear;
    e.spot = "-";
    for(const Spot& sp : spotsNow(s))
    {
        const glm::vec3 from = toBody * (s.pos[h] - sp.pos) * u2m;
        if(glm::length(from) < best)
        {
            best = glm::length(from);
            e.spot = sp.name;
            e.fromSpot = from;
        }
    }
    r.anim.events.pushBack(ZA_MOVE(e));
}

bool button(int hand, bool HandInput::*which, bool pressed)
{
    Recorder& r = recorder;
    const int b = recbutton::buttonIndex(which);
    if(hand < 0 || hand >= HAND_COUNT || b < 0)
    {
        return false;
    }
    if(!pressed)
    {
        const bool was = r.taken[hand][b];
        r.taken[hand][b] = false;
        return was;
    }
    const recbutton::Binding& k = recbutton::binding(vr_ar_button.value);
    if(hand != k.hand || (r.rec == Rec::Off && (!vr_ar_armed.value || key_dest != key_game)))
    {
        return false;
    }
    if(which == k.button)
    {
        r.taken[hand][b] = true;
        if(k.withTrigger)
        {
            return true;
        }
    }
    else if(!(k.withTrigger && which == &HandInput::trigger && r.taken[hand][recbutton::buttonIndex(k.button)]))
    {
        return false;
    }
    r.taken[hand][b] = true;
    toggleRecording();
    return true;
}

void applyPreview(hands::State& s)
{
    const float w = previewWeight();
    if(w <= 0.f)
    {
        return;
    }
    const Frame f = sample(preview.anim, preview.t);
    for(int h = 0; h < HAND_COUNT; h++)
    {
        glm::vec3 pos, rot;
        previewTarget(s, f, h, pos, rot);
        s.pos[h] = glm::mix(s.pos[h], pos, w);
        s.rot[h] = anglesOf(glm::slerp(quatOf(s.rot[h]), quatOf(rot), w));
    }
}

void frame()
{
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        return;
    }
    const float m2u = units::metresToUnits();
    // The ghost (the headset: the flat screen's hands are moved instead).
    if(preview.active && !flatplay::active())
    {
        const float w = previewWeight();
        const Frame f = sample(preview.anim, preview.t);
        for(int h = 0; h < HAND_COUNT; h++)
        {
            glm::vec3 pos, rot;
            previewTarget(s, f, h, pos, rot);
            const int from = vr_ar_play_mirror.value != 0.f ? 1 - h : h;
            const WeaponChange* wc = preview.anim.weaponAt(from, preview.t);
            qmodel_t* model = wc ? modelNamed(wc->model) : nullptr;
            if(model)
            {
                view::setGhost(h, model, pos, rot, 0.25f + 0.45f * w);
            }
            const glm::vec4 colour = h == HAND_MAIN ? glm::vec4{0.3f, 0.85f, 1.f, 0.9f} : glm::vec4{0.5f, 1.f, 0.6f, 0.9f};
            lines::glowPoint(pos, 0.035f * m2u, colour * (0.4f + 0.6f * w));
            lines::glow(pos, pos + hands::forward(rot) * (0.12f * m2u), 0.01f * m2u, colour * 0.5f, colour);
        }
    }
    // The countdown and REC in view.
    za::String text;
    if(recorder.rec == Rec::Countdown)
    {
        text = va("%.0f", ceil(recorder.countdown.left(realtime)));
    }
    else if(recorder.rec == Rec::Recording)
    {
        text = va("%c%c%c %s  %.1f s", 'R' | 0x80, 'E' | 0x80, 'C' | 0x80, recorder.anim.name.cStr(), realtime - recorder.start);
    }
    else if(realtime < recorder.feedbackUntil)
    {
        text = recorder.feedback;
    }
    if(text.empty() || key_dest != key_game)
    {
        return;
    }
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
    const glm::vec3 at = s.head + fwd * (0.7f * m2u) - glm::vec3{0.f, 0.f, 0.15f * m2u};
    text3d::queueOverlay(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, recorder.rec == Rec::Countdown ? 0.16f : 0.07f);
}

void hostFrameEnd()
{
    Recorder& r = recorder;
    if(r.rec == Rec::Countdown)
    {
        const int said = r.countdown.tick(realtime);
        if(said > 0)
        {
            haptics::pulse(recbutton::binding(vr_ar_button.value).hand, 0.05f, 0.5f);
            menuui::uiSound(menuui::UiSound::Move);
            Con_Printf("Animation recorder: %d\n", said);
            if(flatplay::active() || !vrActive())
            {
                SCR_CenterPrint(va("Recording in %d", said));
            }
        }
        else if(said == 0)
        {
            beginRecording();
        }
    }
    else if(r.rec == Rec::Recording)
    {
        recordFrame();
    }

    if(selfTest.phase == TestPhase::Recording && r.rec == Rec::Off)
    {
        testRecorded();
    }
    else if(selfTest.phase == TestPhase::Playing)
    {
        testPlayFrame();
    }
    advancePreview();
    if(selfTest.phase != TestPhase::Off && realtime - selfTest.startedAt > 60.0)
    {
        check(false, "timed out");
        verdict();
    }
}

} // namespace qvr::animrec
