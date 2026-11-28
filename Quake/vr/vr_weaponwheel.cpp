// vr_weaponwheel.cpp -- the headset's weapon wheel (vr_weaponwheel.hpp).
//
// The image is the flat holster menu's (gadget::renderHolsterMenu, the same render target: the two never show together,
// one is flatscreen play's, this a headset's): its slots laid out on a ring, the hovered one lit with the drop target's
// double frame, those a pick would not change greyed, a stroke from the ring's middle to the cursor. It is shown on a quad
// facing the eyes by the holster menu's own composite (flatholster::drawImage: its face, the CRT screen).
//
// A pick is the provider's; the holsters' sends the client command "vr_wheel_pick <hand> <slot>" (a remote server's
// too: sv_user.c lets it through), which runs QC VR_Wheel_Pick (vr_flatholster.qc) with `self` the player who sent it:
// the holster menu's and the number keys' own steps (VR_FlatHolster_DrawInto, _Put), the headset's weapon records,
// sounds, refusals and messages, and every player sees the weapons' stats and models change as ever.

#include "vr_client.hpp"
#include "vr_weaponwheel.hpp"

#include "vr_api.h"
#include "vr_backend.hpp"
#include "vr_bindpanel.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatholster.hpp"
#include "vr_flatplay.hpp"
#include "vr_gadget.hpp"
#include "vr_gfx.hpp"
#include "vr_main.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_renderdoc.hpp"
#include "vr_seat.hpp"
#include "vr_testkit.hpp"
#include "vr_timescale.hpp"
#include "vr_toolgun.hpp"
#include "vr_weapon_ids.h"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Constants.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sin.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <stdio.h>
#include <stdlib.h>

namespace qvr::weaponwheel
{

namespace
{

// The image's virtual pixels (the holster menu's renderer: its header on top, its footer at the bottom), the ring's
// middle, its radius (the slots' middles), a slot's size, and where the hand counts as in the middle (nothing lit) or
// outside (let go: cancelled). Ten slots fit (the holsters' provider: the empty hand, six holsters, DROP, FLIP GRIP,
// UNLOAD; 36 degrees apart): side by side 68 wide (the longest title, "RIGHT CHEST", 11 characters of 6 pixels), the steep
// pairs (36 and 72 degrees) clear above one another, and the top and bottom actions beside their neighbours (a ring of 109:
// 2 pixels between EMPTY HAND and the shoulders' slots, 8 between FLIP GRIP and DROP / UNLOAD) inside the header and footer.
// Eleven with the back pouch's grenade (wheelgrenade, 32.7 degrees apart): the ring an oval (119 across, 134 up and down:
// the steep pairs stacked need the height), the image 308 x 318, at least 1.5 pixels between neighbours and 2 inside the
// edges and the footer (a search over radii and sizes, ROUND26 "wheelgrenade"). Its pixels keep the ten-slot wheel's size:
// vr_weaponwheel_size and vr_flat_wheel_size are the size of its first 280 x 282 (refW, refH).
constexpr float panelW = 308.f;
constexpr float panelH = 318.f;
constexpr float refW = 280.f;
constexpr float refH = 282.f;
constexpr glm::vec2 ringMid{154.f, 155.f};
constexpr glm::vec2 ringR{119.f, 134.f}; // (across, up and down)
constexpr float slotW = 68.f;
constexpr float slotH = 46.f;
constexpr float halfGap = 2.f;  // between a split slot's halves
constexpr float actionW = 56.f; // an action's (EMPTY HAND, DROP, FLIP GRIP, UNLOAD: the holster menu's buttons, smaller than a slot)
constexpr float actionH = 32.f;
constexpr float innerR = 26.f;
constexpr glm::vec2 outerR{ringR.x + 46.f, ringR.y + 46.f}; // (the oval past which the hand is off the wheel)
constexpr float fadeSeconds = 0.1f;
constexpr float panelGap = 6.f; // a headset: between the bindings panel and the wheel's image above which it shows
constexpr float flatCursorMargin = 60.f; // flatscreen play: how far past the image's edge the mouse's cursor goes (off the wheel)
constexpr int giveUp = 400; // the test: frames a step may wait

struct WheelState
{
    const Provider* provider{nullptr}; // setProvider's (nullptr: the holsters')
    int hand{-1};                      // open for this hand (-1 closed)
    int shownHand{0};                  // the last hand it opened for (its fade out)
    bool flat{false};                  // opened in flatscreen play (the mouse its cursor, drawn on the window)
    glm::vec2 screenAt{0.f};           // flatscreen play: where the hand was on the window as it opened (pixels, y down)
    float scale{1.f};                  // flatscreen play: window pixels an image pixel, as last drawn
    glm::vec3 centre{0.f}, right{0.f, -1.f, 0.f}, up{0.f, 0.f, 1.f}, normal{-1.f, 0.f, 0.f}; // the ring's middle; towards the eyes
    // ... the same in the play space (as it opened; follow: the world's each frame from the space's origin and turn)
    glm::vec3 roomCentre{0.f}, roomRight{0.f, -1.f, 0.f}, roomUp{0.f, 0.f, 1.f}, roomNormal{-1.f, 0.f, 0.f};
    float followed{0.f}; // how far the play space carried it since it opened (units; vr_weaponwheel_dump, the test)
    Choice choices[maxChoices];
    int count{0};
    int hovered{-1};      // the lit choice (-1 none)
    bool outside{false};  // the hand off the wheel: letting go cancels
    glm::vec2 cursor{ringMid};
    float fade{0.f};
    double lastTime{-1.0};
    unsigned texture{0}; // the image, rendered this frame (render)
    gadget::VisorImage panel; // the bindings panel above it (a headset's; render)
    bool panelShown{false};   // the panel shown since the test began (the test)
    int ticks{0}, opens{0}, picks{0}, cancels{0}; // (vr_weaponwheel_dump, the test)
};
// split15: each seat's (seat::Part::WeaponWheel)
[[nodiscard]] WheelState& ws()
{
    return seat::part<WheelState>(seat::current(), seat::Part::WeaponWheel);
}

[[nodiscard]] const Provider& provider()
{
    return ws().provider ? *ws().provider : holsterProvider();
}

// ---------------------------------------------------------------------------------------------------------------------
// The holsters' provider: the holster menu's model (flatholster::holsterSlot), clockwise from the top: the empty hand,
// then the right shoulder, chest and hip, DROP, FLIP GRIP (at the bottom) and UNLOAD, then the left hip, chest and
// shoulder (the body's own places round the hand).

constexpr int holsterOrder[] = {1, 5, 3, keyDrop, keyFlip, keyUnload, keyGrenade, 2, 4, 0};

void copyText(char* to, za::SizeT size, const char* from)
{
    q_strlcpy(to, from ? from : "", size);
}

// The back pouch's grenade entry (wheelgrenade): a grenade (rockets, STAT_ROCKETS) or the multi-grenade (multi-rockets,
// STAT_QVR_AMMOMULTI), its thumbnail the pouch's own model (QC VR_HandGrenade_Model) and the count left as an ammo line;
// both carried: two halves, the grenade on top; neither: one greyed grenade. None without the pouch (vr_handgrenade 0).
int listGrenades(Choice* out, int max)
{
    if(vr_handgrenade.value == 0.f || max <= 0)
    {
        return 0;
    }
    const int rockets = cl.stats[STAT_ROCKETS], multi = cl.stats[protocol::STAT_QVR_AMMOMULTI];
    const bool split = rockets > 0 && multi > 0 && max >= 2;
    const auto make = [](Choice& c, bool isMulti, int left, int half) {
        c = Choice{};
        c.key = isMulti ? keyMultiGrenade : keyGrenade;
        copyText(c.title, sizeof(c.title), "BACK POUCH");
        copyText(c.name, sizeof(c.name), isMulti ? "cluster" : "grenade");
        copyText(c.model, sizeof(c.model), isMulti ? "progs/mervup.mdl" : "progs/grenade.mdl");
        c.ammoType = isMulti ? 6 : 3; // (AID_MULTI_ROCKETS, AID_ROCKETS)
        c.reserve = za::max(left, 0);
        c.usable = left > 0;
        c.half = half;
    };
    if(split)
    {
        make(out[0], false, rockets, 1);
        make(out[1], true, multi, 2);
        return 2;
    }
    make(out[0], multi > 0, multi > 0 ? multi : rockets, 0);
    return 1;
}

int listHolsters(int hand, Choice* out, int max)
{
    const flatholster::HolsterSlot held = flatholster::handSlot(hand);
    const bool handFull = held.name[0] != 0;
    const bool immersive = vr_holster_mode.value == 0.f;
    int n = 0;
    if(n < max)
    {
        Choice& c = out[n++];
        c = Choice{};
        c.key = -1;
        copyText(c.title, sizeof(c.title), "EMPTY HAND");
        copyText(c.name, sizeof(c.name), handFull ? "put away" : "");
        c.usable = handFull;
        c.icon = hand == HAND_MAIN ? gadget::MenuIcon::HandRight : gadget::MenuIcon::HandLeft;
    }
    for(const int i : holsterOrder)
    {
        if(n >= max)
        {
            break;
        }
        if(i == keyGrenade)
        {
            n += listGrenades(out + n, max - n);
            continue;
        }
        Choice& c = out[n++];
        c = Choice{};
        c.key = i;
        if(i == keyDrop || i == keyUnload || i == keyFlip)
        {
            // The hand's actions (the holster menu's DROP, UNLOAD and FLIP GRIP, its own tests): what the hand holds,
            // greyed when nothing would happen.
            const bool drop = i == keyDrop, flip = i == keyFlip;
            copyText(c.title, sizeof(c.title), drop ? "DROP" : flip ? "FLIP GRIP" : "UNLOAD");
            c.usable = drop ? flatholster::canDrop(hand) : flip ? flatholster::canFlip(hand) : flatholster::canUnload(hand);
            copyText(c.name, sizeof(c.name), handFull ? held.name : drop && c.usable ? "carried" : "");
            c.icon = drop ? gadget::MenuIcon::Drop : flip ? gadget::MenuIcon::FlipGrip : gadget::MenuIcon::Unload;
            continue;
        }
        const flatholster::HolsterSlot s = flatholster::holsterSlot(i);
        copyText(c.title, sizeof(c.title), s.title);
        copyText(c.name, sizeof(c.name), s.name);
        copyText(c.model, sizeof(c.model), s.model);
        c.clip = s.clip;
        c.clipSize = s.clipSize;
        c.ammoType = s.ammoType;
        c.reserve = s.reserve;
        c.attach = s.attach;
        // An empty holster takes the hand's weapon (a quick slot's copy); a full one gives its own (immersive: the same
        // weapon already in the hand is a quick slot's copy only).
        c.usable = s.name[0] ? (immersive || q_strcasecmp(s.model, held.model) != 0) : handFull;
    }
    return n;
}

void pickHolster(int hand, const Choice& c)
{
    if(cls.state != ca_connected || cls.signon != SIGNONS)
    {
        return;
    }
    qvr::client::forwardCommand(va("vr_wheel_pick %d %d", hand, c.key)); // (mpoverflow: held while full)
}

const Provider holsters{"holsters", listHolsters, pickHolster};

// ---------------------------------------------------------------------------------------------------------------------
// The layout.

// The places on the ring: one a choice, a split slot's two halves one (wheelgrenade).
[[nodiscard]] int placeCount()
{
    int n = 0;
    for(int i = 0; i < ws().count; i++)
    {
        n += ws().choices[i].half == 2 ? 0 : 1;
    }
    return n;
}

// Choice `i`'s place on the ring (0 .. placeCount() - 1).
[[nodiscard]] int placeOf(int i)
{
    int place = -1;
    for(int j = 0; j <= i && j < ws().count; j++)
    {
        place += ws().choices[j].half == 2 ? 0 : 1;
    }
    return za::max(place, 0);
}

// Place `place`'s middle (of `n`), clockwise from the top round the oval.
[[nodiscard]] glm::vec2 placeMid(int place, int n)
{
    const float a = static_cast<float>(place) * za::tau / static_cast<float>(za::max(n, 1));
    return ringMid + glm::vec2{za::sin(a), -za::cos(a)} * ringR;
}

// Choice `i`'s slot's middle: its place's, a half's own half of it.
[[nodiscard]] glm::vec2 choiceMid(int i)
{
    glm::vec2 m = placeMid(placeOf(i), placeCount());
    const int half = i >= 0 && i < ws().count ? ws().choices[i].half : 0;
    m.y += half == 1 ? -(slotH + halfGap) * 0.25f : half == 2 ? (slotH + halfGap) * 0.25f : 0.f;
    return m;
}

// The choice under panel point `p` (-1 none: the middle, or outside: `outside`). A split slot: its half above or below its
// place's middle.
[[nodiscard]] int choiceAt(const glm::vec2& p, bool& outside)
{
    const glm::vec2 d = p - ringMid;
    const glm::vec2 u = d / outerR; // (the outer oval a unit circle)
    outside = glm::length(u) > 1.f || p.x < 0.f || p.y < 0.f || p.x > panelW || p.y > panelH;
    const int n = placeCount();
    if(outside || glm::length(d) < innerR || n <= 0)
    {
        return -1;
    }
    float a = za::atan2(d.x / ringR.x, -d.y / ringR.y); // (clockwise from the top, round the oval)
    if(a < 0.f)
    {
        a += za::tau;
    }
    const float step = za::tau / static_cast<float>(n);
    const int place = static_cast<int>(za::floor(a / step + 0.5f)) % n;
    for(int i = 0; i < ws().count; i++)
    {
        if(placeOf(i) == place)
        {
            const bool lower = ws().choices[i].half == 1 && i + 1 < ws().count && ws().choices[i + 1].half == 2 &&
                               p.y > placeMid(place, n).y;
            return lower ? i + 1 : i;
        }
    }
    return -1;
}

[[nodiscard]] float unitsPerPixel()
{
    return za::clamp(vr_weaponwheel_size.value, 4.f, 80.f) / refW;
}

// Panel point `p` in the world (on the ring's plane; `lift` units towards the eyes).
[[nodiscard]] glm::vec3 worldAt(const glm::vec2& p, float lift = 0.f)
{
    const float k = unitsPerPixel();
    return ws().centre + ws().right * ((p.x - ringMid.x) * k) - ws().up * ((p.y - ringMid.y) * k) + ws().normal * lift;
}

// The hand's point projected onto the ring's plane, in panel pixels.
[[nodiscard]] glm::vec2 cursorOf(const glm::vec3& hand)
{
    const glm::vec3 d = hand - ws().centre;
    const float k = unitsPerPixel();
    return ringMid + glm::vec2{glm::dot(d, ws().right), -glm::dot(d, ws().up)} / k;
}

// The headset's wheel carried by the play space (the stick's walk and turn, a push, the stairs), as the hands are: its
// place in the room as it opened, put in the world again from the space's origin and turn.
void follow(const hands::State& s)
{
    const glm::vec3 was = ws().centre;
    ws().centre = s.spaceOrigin + hands::rotateYaw(ws().roomCentre, s.turnYaw);
    ws().normal = hands::rotateYaw(ws().roomNormal, s.turnYaw);
    ws().right = hands::rotateYaw(ws().roomRight, s.turnYaw);
    ws().up = hands::rotateYaw(ws().roomUp, s.turnYaw);
    ws().followed += glm::length(ws().centre - was);
}

void tick(int hand)
{
    ws().ticks++;
    if(!ws().flat)
    {
        haptics::pulse(hand, 0.012f, 0.35f, 180.f);
    }
}

// Flatscreen play: where hand `hand` is on the window (pixels, y down) through the last view drawn; off the view or behind
// the eye: low on that hand's side.
[[nodiscard]] glm::vec2 flatHandOnWindow(int hand, const hands::State& s)
{
    const float W = static_cast<float>(za::max(vid.width, 1)), H = static_cast<float>(za::max(vid.height, 1));
    const glm::vec2 fallback{W * (hand == HAND_MAIN ? 0.72f : 0.28f), H * 0.68f};
    if(!s.valid)
    {
        return fallback;
    }
    const glm::vec4 c = glm::make_mat4(r_matviewproj) * glm::vec4{s.pos[hand], 1.f};
    if(c.w <= 1e-3f)
    {
        return fallback;
    }
    return {(c.x / c.w * 0.5f + 0.5f) * W, (0.5f - c.y / c.w * 0.5f) * H};
}

[[nodiscard]] int handWeapon(int hand)
{
    return cl.stats[hand == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2];
}

void close(bool picked)
{
    (picked ? ws().picks : ws().cancels)++;
    ws().hand = -1;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_weaponwheel_test (Misc/quakevr/weaponwheel_test.sh; a headset's game, the mock's): the wheel driven by the mock's
// buttons and hands (vr_mock_button, vr_mock_hand_to onto a slot's place in the world), each step waiting on what it
// changes (at most giveUp frames: FAIL "timed out"). Single player: a nailgun in the right hand (gripped) and a shotgun
// in the right shoulder holster; B held opens it round the right hand, time slows; the hand onto the shoulder's slot
// (lit, a tick), let go: the shotgun in the right hand, the nailgun in a holster; the left hand's Y, the hand moved off
// the wheel, let go: nothing changes; the left hand (its grip not held) takes the nailgun: still held a while later; the
// right hand's EMPTY HAND: the shotgun back in its shoulder; time back at 1. "mp": only the first pick (the MP test's
// other player checks what he sees). Ends "weaponwheel_test: N passed, M failed".
struct Test
{
    bool on{false};
    bool mp{false};
    int step{0};
    int waited{0};
    int mark{0}; // a step's own count
    int nailgunAt{-1};
    int clip{0}, ammo{0}; // the rounds in the gun and the reserve before UNLOAD
    int nailClip{0};      // the nailgun's rounds before the wheel's hold
    int flags0{0};        // the dagger's flags before the wheel's hold (FLIP GRIP: its reverse grip's bit turned over)
    bool flat{false};     // "flat": flatscreen play's (flatTestStep)
    bool gren{false};     // "grenade" / "flatgrenade": the grenade entry's (grenadeTestStep)
    int shotgunAt{-1};    // flat: the shotgun's holster
    int firstWid{0};      // flat: the weapon 2's first tap drew (the first holstered)
    int pickAt{-1};       // flat: its holster, the wheel's pick
    int opens{0};         // flat: the wheel's opens before a step
    int walk{0};          // the left stick's walk with the wheel open: frames walked
    glm::vec2 cursor0{0.f}; // ... the cursor before it
    float wheelWas{-1.f}; // vr_weaponwheel before the test (set on for it: off by default; back at the end)
    testkit::Tally t;
};
Test test; // seat-shared: the test's own
constexpr int walkFrames = 36; // the walk's length (counted frames: about 0.5 s of slowed time)
constexpr int revGripFlag = 8192; // QC QVR_WPNFLAG_REVGRIP: the weapon held reversed (STAT_QVR_WEAPONFLAGS)
// The dagger into the right hand (the tests' impulse 149: id 13 with its kind's flags).
constexpr const char* giveDagger = "vr_test_give_wid 13; vr_test_give_flags 3072; impulse 149\n";

[[nodiscard]] int mainFlags()
{
    return cl.stats[protocol::STAT_QVR_WEAPONFLAGS];
}

void result(bool ok, const char* what)
{
    test.t.check(ok);
    Con_Printf("weaponwheel_test: %s %s\n", ok ? "PASS" : "FAIL", what);
}

void finish()
{
    test.on = false;
    Cbuf_AddText(test.flat ? va("vr_mock_key %s up; vr_mock_key %s up; vr_flat_wheel 1\n", vr_flat_key_cycle_left.string,
                                 vr_flat_key_cycle_right.string)
                           : "vr_mock_button main secondary 0; vr_mock_button off secondary 0; vr_mock_stick off 0 0\n");
    if(test.wheelWas >= 0.f)
    {
        Cvar_SetValueQuick(&vr_weaponwheel, test.wheelWas);
    }
    test.t.verdict("weaponwheel_test");
}

[[nodiscard]] const char* handName(int hand)
{
    return hand == HAND_MAIN ? "main" : "off";
}

[[nodiscard]] int holsterStat(int i)
{
    return cl.stats[protocol::STAT_QVR_HOLSTERWEAPON0 + i];
}

[[nodiscard]] int choiceOfKey(int key)
{
    for(int i = 0; i < ws().count; i++)
    {
        if(ws().choices[i].key == key)
        {
            return i;
        }
    }
    return -1;
}

// The mock's hand `hand` onto choice `i`'s slot (-1: off the wheel, to its left), at the hand's own depth.
void aimAt(int hand, int i)
{
    const glm::vec2 p = i >= 0 ? choiceMid(i) : ringMid - glm::vec2{outerR.x + 30.f, 0.f};
    const glm::vec3 w = worldAt(p, za::max(vr_weaponwheel_distance.value, 0.f));
    Cbuf_AddText(va("vr_mock_hand_to %s %.2f %.2f %.2f\n", handName(hand), w.x, w.y, w.z));
}

void press(int hand, bool down)
{
    const char* button = buttonKind() == 1 ? "stickclick" : buttonKind() == 2 ? "primary" : "secondary";
    Cbuf_AddText(va("vr_mock_button %s %s %d\n", handName(hand), button, down ? 1 : 0));
}

// The step waits until `ok`; then the next step (true). Past giveUp frames: FAIL "timed out" and the end.
[[nodiscard]] bool waitFor(bool ok, const char* what)
{
    if(ok)
    {
        test.step++;
        test.waited = 0;
        test.mark = 0;
        return true;
    }
    if(++test.waited > giveUp)
    {
        result(false, va("timed out: %s (wheel hand %d, cursor %.0f %.0f, lit %d; hands %d %d; holsters %d %d %d %d %d %d)",
                          what, ws().hand, ws().cursor.x, ws().cursor.y, ws().hovered,
                          handWeapon(HAND_OFF), handWeapon(HAND_MAIN), holsterStat(0), holsterStat(1), holsterStat(2),
                          holsterStat(3), holsterStat(4), holsterStat(5)));
        finish();
    }
    return false;
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_weaponwheel_test flat (Misc/quakevr/weaponwheel_test.sh flat; flatscreen play, the holsters on, the script's own
// weapons: the shotgun in the right hand, the nailgun in the left): the keys by vr_mock_key, the cursor by
// vr_flat_mouse_move. The shotgun put away (impulse 89); 2 tapped twice: the right hand cycles to the first holstered
// weapon, then the next, no wheel; 2 held: the wheel opens for the right hand (nine slots), time slows; the mouse onto the
// first weapon's slot, 2 let go: the right hand takes it; 1 held, the mouse off the wheel, let go: nothing; 1 held, UNLOAD:
// the nailgun's rounds back in the reserve (its FLIP GRIP greyed); 2 held, DROP: the right hand's weapon dropped; 1 held, an
// empty holster: the nailgun put in it; the dagger in the right hand, 2 held, FLIP GRIP: its grip turned over; vr_flat_wheel
// 0: 1 cycles on the press and a long hold opens nothing; time back at 1.

void flatKey(int hand, bool down)
{
    const cvar_t& key = hand == HAND_MAIN ? vr_flat_key_cycle_right : vr_flat_key_cycle_left;
    Cbuf_AddText(va("vr_mock_key %s %s\n", key.string, down ? "down" : "up"));
}

// The mouse's cursor onto choice `i`'s slot (-1: off the wheel, to its left), by the counts vr_flat_mouse_move hands on.
void flatPoint(int i)
{
    const glm::vec2 p = i >= 0 ? choiceMid(i) : ringMid - glm::vec2{outerR.x + 40.f, 0.f};
    const float k = za::max(ws().scale, 0.1f) / za::max(vr_flat_holster_menu_cursor_speed.value, 0.01f);
    const int dx = static_cast<int>(za::floor((p.x - ws().cursor.x) * k + 0.5f));
    const int dy = static_cast<int>(za::floor((p.y - ws().cursor.y) * k + 0.5f));
    Cbuf_AddText(va("vr_flat_mouse_move %d %d\n", dx, dy));
}

// A step pointing the cursor at choice `want` (once, two frames in: the image drawn, its scale known), waiting until lit.
[[nodiscard]] bool flatPointed(int want, const char* what)
{
    if(++test.mark == 2)
    {
        flatPoint(want);
    }
    return waitFor(test.mark > 2 && (want < 0 ? ws().outside : ws().hovered == want), what);
}

[[nodiscard]] int freeHolster()
{
    for(int i = 0; i < flatholster::holsterSlots; i++)
    {
        if(holsterStat(i) == WID_FIST)
        {
            return i;
        }
    }
    return -1;
}

void flatTestStep()
{
    const int main = HAND_MAIN, off = HAND_OFF;
    switch(test.step)
    {
    case 0:
        if(waitFor(cls.signon == SIGNONS && cl.stats[STAT_HEALTH] > 0 && handWeapon(main) == WID_SHOTGUN &&
                       handWeapon(off) == WID_NAILGUN, "the game up, both hands full"))
        {
            Cbuf_AddText("impulse 89\n"); // (5's: the right hand's shotgun into a free holster)
        }
        break;
    case 1:
        for(int i = 0; i < flatholster::holsterSlots; i++)
        {
            test.shotgunAt = holsterStat(i) == WID_SHOTGUN ? i : test.shotgunAt;
        }
        if(waitFor(handWeapon(main) == WID_FIST && test.shotgunAt >= 0, "the shotgun holstered"))
        {
            test.opens = ws().opens;
            flatKey(main, true);
        }
        break;
    case 2:
    case 4:
        if(waitFor(++test.mark >= 4, "a tap"))
        {
            flatKey(main, false);
        }
        break;
    case 3:
        if(waitFor(handWeapon(main) != WID_FIST, "the tap's cycle"))
        {
            test.firstWid = handWeapon(main);
            result(ws().opens == test.opens, va("2 tapped: the right hand cycles to the first holstered weapon (%d), no wheel", test.firstWid));
            flatKey(main, true);
        }
        break;
    case 5:
        if(waitFor(handWeapon(main) != WID_FIST && handWeapon(main) != test.firstWid, "the second tap's cycle"))
        {
            result(ws().opens == test.opens, va("2 tapped again: the next holstered one (%d), no wheel", handWeapon(main)));
            for(int i = 0; i < flatholster::holsterSlots; i++)
            {
                test.pickAt = holsterStat(i) == test.firstWid ? i : test.pickAt;
            }
            flatKey(main, true);
        }
        break;
    case 6:
    {
        const int frames = ++test.mark;
        if(waitFor(ws().hand == main, "2 held: the wheel open for the right hand"))
        {
            result(ws().flat && frames >= 10, va("2 held: the wheel opens for the right hand (after %d frames), on the window", frames));
            result(placeCount() == 11, va("eleven places (%d)", placeCount()));
        }
        break;
    }
    case 7:
        if(waitFor(timescale::current() <= za::clamp(vr_weaponwheel_timescale.value, 0.05f, 1.f) + 0.02f, "time slowed"))
        {
            result(true, va("time slowed while open (x%.2f)", timescale::current()));
        }
        break;
    case 8:
        if(flatPointed(choiceOfKey(test.pickAt), "the cursor onto the first weapon's slot"))
        {
            result(ws().ticks > 1, va("the mouse lights that holster's slot (%s)", ws().choices[ws().hovered].title));
        }
        break;
    case 9:
    case 13:
    {
        const int hand = test.step == 9 ? main : off; // (before waitFor moves the step on)
        // (the screenshot a few frames after the cursor settled: the window's image is drawn after the move)
        if(++test.mark == 4)
        {
            Cbuf_AddText("screenshot\n");
        }
        if(waitFor(test.mark >= 9, "the screenshot"))
        {
            flatKey(hand, false);
        }
        break;
    }
    case 10:
        if(waitFor(ws().hand < 0 && test.pickAt >= 0 && handWeapon(main) == test.firstWid && holsterStat(test.pickAt) == WID_FIST, "the pick"))
        {
            result(true, "2 let go on that slot: the right hand takes its weapon (what it held back to its holster)");
            test.opens = ws().cancels;
            flatKey(off, true);
        }
        break;
    case 11:
    case 15:
    case 21:
        (void)waitFor(ws().hand == off, "1 held: the wheel open for the left hand");
        break;
    case 12:
        if(flatPointed(-1, "the cursor off the wheel"))
        {
            result(ws().hovered < 0, "off the wheel: nothing lit");
        }
        break;
    case 14:
        if(waitFor(++test.mark >= 30, "a pick's time"))
        {
            result(ws().hand < 0 && handWeapon(off) == WID_NAILGUN && ws().cancels > test.opens, "1 let go off the wheel: nothing moved");
            flatKey(off, true);
        }
        break;
    case 16:
    {
        const int want = choiceOfKey(keyUnload);
        if(flatPointed(want, "the cursor onto UNLOAD"))
        {
            test.clip = cl.stats[protocol::STAT_QVR_WEAPONCLIP2];
            test.ammo = cl.stats[STAT_NAILS];
            result(ws().choices[want].usable && test.clip > 0, va("UNLOAD lit (%d rounds in the nailgun, %d nails)", test.clip, test.ammo));
            const int flip = choiceOfKey(keyFlip);
            result(flip >= 0 && !ws().choices[flip].usable, "FLIP GRIP greyed: the nailgun's grip does not flip");
            flatKey(off, false);
        }
        break;
    }
    case 17:
        if(waitFor(cl.stats[protocol::STAT_QVR_WEAPONCLIP2] == 0, "the nailgun unloaded"))
        {
            result(cl.stats[STAT_NAILS] > test.ammo, va("UNLOAD: the nailgun empty, nails %d -> %d", test.ammo, cl.stats[STAT_NAILS]));
            flatKey(main, true);
        }
        break;
    case 18:
        (void)waitFor(ws().hand == main, "2 held: the wheel open for the right hand");
        break;
    case 19:
    {
        const int want = choiceOfKey(keyDrop);
        if(flatPointed(want, "the cursor onto DROP"))
        {
            result(ws().choices[want].usable, "DROP lit: the right hand holds a weapon");
            flatKey(main, false);
        }
        break;
    }
    case 20:
        if(waitFor(handWeapon(main) == WID_FIST, "the right hand's weapon dropped"))
        {
            result(true, "DROP: the right hand's weapon dropped");
            flatKey(off, true);
        }
        break;
    case 22:
        test.nailgunAt = freeHolster();
        if(flatPointed(choiceOfKey(test.nailgunAt), "the cursor onto an empty holster"))
        {
            flatKey(off, false);
        }
        break;
    case 23:
        if(waitFor(handWeapon(off) == WID_FIST && test.nailgunAt >= 0 && holsterStat(test.nailgunAt) == WID_NAILGUN, "the nailgun holstered"))
        {
            result(true, "1 let go on an empty holster: the left hand's nailgun put in it");
            Cbuf_AddText(giveDagger); // (FLIP GRIP: steps 50 .. 54, then back to 24)
            test.step = 50;
        }
        break;
    case 50:
        if(waitFor(handWeapon(main) == WID_SWORD, "the dagger in the right hand"))
        {
            test.flags0 = mainFlags();
            flatKey(main, true);
        }
        break;
    case 51:
        if(waitFor(ws().hand == main, "2 held with the dagger: the wheel open for the right hand"))
        {
            result((mainFlags() & revGripFlag) == (test.flags0 & revGripFlag), "2 held: the hold flipped nothing");
        }
        break;
    case 52:
    {
        const int want = choiceOfKey(keyFlip);
        if(flatPointed(want, "the cursor onto FLIP GRIP"))
        {
            result(ws().choices[want].usable, "FLIP GRIP lit: the right hand holds the dagger");
        }
        break;
    }
    case 53:
        // (the screenshot of FLIP GRIP lit, a few frames after the cursor settled)
        if(++test.mark == 4)
        {
            Cbuf_AddText("screenshot\n");
        }
        if(waitFor(test.mark >= 9, "the screenshot"))
        {
            flatKey(main, false);
        }
        break;
    case 54:
        if(waitFor((mainFlags() & revGripFlag) != (test.flags0 & revGripFlag), "the dagger's grip flipped"))
        {
            result(handWeapon(main) == WID_SWORD, va("2 let go on FLIP GRIP: the dagger's grip turned over (%s)",
                                                      (mainFlags() & revGripFlag) ? "reversed" : "upright"));
            Cbuf_AddText("vr_flat_wheel 0\n");
            test.opens = ws().opens;
            test.step = 24;
        }
        break;
    case 24:
        if(waitFor(++test.mark >= 3 && vr_flat_wheel.value == 0.f, "vr_flat_wheel 0"))
        {
            flatKey(off, true);
        }
        break;
    case 25:
    {
        const int frames = ++test.mark;
        if(waitFor(handWeapon(off) != WID_FIST, "vr_flat_wheel 0: 1's cycle"))
        {
            result(frames < 10, va("vr_flat_wheel 0: 1 cycles the left hand on the press (%d frames)", frames));
        }
        break;
    }
    case 26:
        if(waitFor(++test.mark >= 50, "a long hold"))
        {
            result(ws().opens == test.opens && ws().hand < 0, "vr_flat_wheel 0: a long hold of 1 opens no wheel");
            flatKey(off, false);
            Cbuf_AddText("vr_flat_wheel 1\n");
        }
        break;
    case 27:
        if(waitFor(timescale::current() >= 0.99f, "time back at 1"))
        {
            result(true, "time back at 1");
            finish();
        }
        break;
    default:
        finish();
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// vr_weaponwheel_test grenade | flatgrenade (wheelgrenade; weaponwheel_test.sh grenade / flatgrenade): the back pouch's
// grenade entry. The pouch emptied (give r 0, give m 0), the first hand's wheel (flat: the right
// hand, holding the shotgun; headset: the left, empty, its grip not held) shows one greyed grenade; 5 rockets: a whole
// usable GRENADE, 5 left on it; 3 multi-rockets too: split, GRENADE on top and CLUSTER below, one place; the cursor on
// each half lights it; let go on CLUSTER: that hand holds a multi-grenade (its gun holstered), 2 multi-rockets left; still
// held a while later (the headset's: without its grip); its fire button (flat: LMB; headset: the trigger) arms it, then
// the throw (flat: LMB held, let go; headset: the grip pressed and let go): gone from the hand, still 2 left; the other
// hand (holding a gun) on GRENADE: a grenade in that hand, its gun holstered, 4 rockets left; flat: G puts it back (5).
// Ends "weaponwheel_test: N passed, M failed".

// What the first player's hand `hand` holds from the pouch (QC VR_Wheel_TestHeld: 0 nothing, 1 a grenade, 2 a
// multi-grenade; + 10 armed; + 100 x the quick grenade's phase). Single player.
[[nodiscard]] int heldCode(int hand)
{
    if(!sv.active)
    {
        return -1;
    }
    const progs::ServerVm vm;
    return static_cast<int>(progs::qcFloat(progs::findFunction("VR_Wheel_TestHeld"), static_cast<float>(hand)));
}

// The pouch's ammo: the player's rockets and multi-rockets, by "give r" and "give m" (the scene's way; Host_Give_f's m
// outside the mission pack too: wheelgrenade).
void setPouch(int rockets, int multi)
{
    Cbuf_AddText(va("give r %d; give m %d\n", rockets, multi));
}

void wheelKey(int hand, bool down)
{
    if(test.flat)
    {
        flatKey(hand, down);
    }
    else
    {
        press(hand, down);
    }
}

// A step pointing at choice `want` (the hand or the mouse, two frames in), waiting until it is lit.
[[nodiscard]] bool pointed(int want, const char* what)
{
    if(++test.mark == 2)
    {
        if(test.flat)
        {
            flatPoint(want);
        }
        else
        {
            aimAt(ws().hand, want);
        }
    }
    return waitFor(test.mark > 2 && want >= 0 && ws().hovered == want, what);
}

[[nodiscard]] int rocketsLeft()
{
    return cl.stats[STAT_ROCKETS];
}

[[nodiscard]] int multiLeft()
{
    return cl.stats[protocol::STAT_QVR_AMMOMULTI];
}

void grenadeTestStep()
{
    const int first = test.flat ? HAND_MAIN : HAND_OFF, second = test.flat ? HAND_OFF : HAND_MAIN;
    const char* fire = test.flat ? (first == HAND_MAIN ? "attack" : "offhandattack") : nullptr;
    switch(test.step)
    {
    case 0:
        if(waitFor(cls.signon == SIGNONS && cl.stats[STAT_HEALTH] > 0 && !cl.intermission &&
                       (!test.flat || (handWeapon(HAND_MAIN) == WID_SHOTGUN && handWeapon(HAND_OFF) == WID_NAILGUN)),
                "the game up"))
        {
            if(!test.flat)
            {
                // (the VR test's: the hands out in front, the right one gripping a nailgun, the left empty, its grip open)
                Cbuf_AddText("vr_mock_hand main 0.15 1.35 -0.4 0 0 0; vr_mock_hand off -0.15 1.35 -0.4 0 0 0; vr_mock_look 35 0\n");
                Cbuf_AddText("vr_mock_button main grip 1; vr_mock_button off grip 0; impulse 156\n");
            }
            setPouch(0, 0);
        }
        break;
    case 1:
        if(waitFor(rocketsLeft() == 0 && multiLeft() == 0 && handWeapon(second) != WID_FIST && ++test.mark > 10,
               "the pouch empty, the second hand's gun"))
        {
            wheelKey(first, true);
        }
        break;
    case 2:
        if(waitFor(ws().hand == first, "the wheel open for the first hand"))
        {
            const int g = choiceOfKey(keyGrenade);
            result(placeCount() == 11 && g >= 0 && choiceOfKey(keyMultiGrenade) < 0 && !ws().choices[g].usable &&
                       ws().choices[g].half == 0,
                va("an empty pouch: one greyed grenade entry (places %d, choice %d)", placeCount(), g));
            setPouch(5, 0);
        }
        break;
    case 3:
    {
        const int g = choiceOfKey(keyGrenade);
        if(waitFor(rocketsLeft() == 5 && g >= 0 && ws().choices[g].usable, "5 rockets: the entry usable"))
        {
            result(choiceOfKey(keyMultiGrenade) < 0 && ws().choices[g].half == 0 && ws().choices[g].reserve == 5,
                va("5 rockets: one whole GRENADE entry, %d left on it", ws().choices[g].reserve));
            setPouch(5, 3);
        }
        break;
    }
    case 4:
    {
        const int g = choiceOfKey(keyGrenade), m = choiceOfKey(keyMultiGrenade);
        if(waitFor(multiLeft() == 3 && m >= 0, "3 multi-rockets: the CLUSTER half"))
        {
            result(g >= 0 && ws().choices[g].half == 1 && ws().choices[m].half == 2 && m == g + 1 && placeCount() == 11 &&
                       choiceMid(g).y < choiceMid(m).y && ws().choices[m].reserve == 3,
                va("both kinds: the entry split, GRENADE on top, CLUSTER below (%d left), one place (%d places)",
                    ws().choices[m].reserve, placeCount()));
        }
        break;
    }
    case 5:
        if(pointed(choiceOfKey(keyGrenade), "the top half lit"))
        {
            result(true, "the cursor on the top half lights GRENADE");
        }
        break;
    case 6:
        // (the screenshot a few frames after the cursor settled: his review of the split entry)
        if(++test.mark == 4)
        {
            Cbuf_AddText("screenshot\n");
        }
        (void)waitFor(test.mark >= 9, "the screenshot");
        break;
    case 7:
        if(pointed(choiceOfKey(keyMultiGrenade), "the bottom half lit"))
        {
            result(true, "the cursor on the bottom half lights CLUSTER");
            wheelKey(first, false);
        }
        break;
    case 8:
        if(waitFor(ws().hand < 0 && heldCode(first) % 100 == 2 && handWeapon(first) == WID_FIST, "the multi-grenade in the hand"))
        {
            result(multiLeft() == 2 && rocketsLeft() == 5,
                va("let go on CLUSTER: the hand holds a multi-grenade%s; %d multi-rockets left, %d rockets",
                    test.flat ? ", its shotgun holstered" : "", multiLeft(), rocketsLeft()));
        }
        break;
    case 9:
        if(waitFor(++test.mark >= 40, "a while"))
        {
            result(heldCode(first) % 100 == 2, test.flat ? "still held a while later" : "still held a while later, its grip not pressed");
            Cbuf_AddText(test.flat ? va("+%s\n", fire) : "vr_mock_button off trigger 1\n");
        }
        break;
    case 10:
        if(++test.mark == 4)
        {
            Cbuf_AddText(test.flat ? va("-%s\n", fire) : "vr_mock_button off trigger 0\n");
        }
        if(waitFor(test.mark > 4 && heldCode(first) % 100 >= 10, "armed"))
        {
            result(true, va("the fire button arms it (%d)", heldCode(first)));
            // (flat: held, the wind-up; the headset: the grip takes it from the wheel's hold)
            Cbuf_AddText(test.flat ? va("+%s\n", fire) : "vr_mock_button off grip 1\n");
        }
        break;
    case 11:
        if(waitFor(++test.mark >= 30 && (!test.flat || heldCode(first) / 100 == 4), "the wind-up / the grip"))
        {
            Cbuf_AddText(test.flat ? va("-%s\n", fire) : "vr_mock_button off grip 0\n");
        }
        break;
    case 12:
    {
        // (out of the hand soon after the release, long before its fuse: thrown, not gone off in the hand)
        const int frames = ++test.mark;
        if(waitFor(heldCode(first) % 100 == 0, "thrown"))
        {
            result(frames <= 40 && multiLeft() == 2,
                va("let go: thrown (out of the hand %d frames after), still %d multi-rockets left", frames, multiLeft()));
        }
        break;
    }
    case 13:
        if(waitFor(++test.mark >= 30 && heldCode(first) == 0, "the throw over"))
        {
            wheelKey(second, true);
        }
        break;
    case 14:
        (void)waitFor(ws().hand == second, "the wheel open for the second hand");
        break;
    case 15:
        if(pointed(choiceOfKey(keyGrenade), "GRENADE lit"))
        {
            wheelKey(second, false);
        }
        break;
    case 16:
        if(waitFor(ws().hand < 0 && heldCode(second) % 100 == 1 && handWeapon(second) == WID_FIST, "the grenade in the second hand"))
        {
            result(rocketsLeft() == 4 && multiLeft() == 2,
                va("the other hand on GRENADE: a grenade in it, its gun holstered; %d rockets left", rocketsLeft()));
            if(!test.flat)
            {
                test.step = 99; // (the headset's ends here)
                break;
            }
            Cbuf_AddText("impulse 57\n"); // (G: the unarmed one back in the pouch)
        }
        break;
    case 17:
        if(waitFor(heldCode(second) == 0 && rocketsLeft() == 5, "G puts it back"))
        {
            result(true, "G puts the unarmed grenade back: 5 rockets");
            test.step = 99;
        }
        break;
    default:
        Cbuf_AddText("-attack; -offhandattack; vr_mock_button main trigger 0; vr_mock_button off trigger 0\n");
        finish();
        break;
    }
}

void testStep()
{
    if(!test.on)
    {
        return;
    }
    if(test.gren)
    {
        grenadeTestStep();
        return;
    }
    if(test.flat)
    {
        flatTestStep();
        return;
    }
    const int main = HAND_MAIN, off = HAND_OFF;
    switch(test.step)
    {
    case 0:
        if(waitFor(cls.signon == SIGNONS && cl.stats[STAT_HEALTH] > 0 && !cl.intermission, "the game up"))
        {
            // (one impulse a frame: a move carries one)
            // (the hands held out in front at the chest, where a player opens it: in view)
            Cbuf_AddText("vr_mock_hand main 0.15 1.35 -0.4 0 0 0; vr_mock_hand off -0.15 1.35 -0.4 0 0 0; vr_mock_look 35 0\n");
            Cbuf_AddText("vr_mock_button main grip 1; vr_mock_button off grip 0; impulse 156\n");
        }
        break;
    case 1:
        if(test.mark == 0 && handWeapon(main) == WID_NAILGUN)
        {
            test.mark = 1;
            Cbuf_AddText("impulse 91\n");
        }
        if(waitFor(holsterStat(1) == WID_SHOTGUN && handWeapon(main) == WID_NAILGUN, "the shotgun holstered"))
        {
            test.nailClip = cl.stats[protocol::STAT_QVR_WEAPONCLIP];
            press(main, true);
        }
        break;
    case 2:
    {
        const int frames = ++test.mark;
        if(waitFor(ws().hand == main, "the wheel open round the right hand"))
        {
            // (B is the magazine's eject button too: the hold that opens the wheel must not reach the server's press:
            // vr_input.cpp WheelHold. ROUND25 "holsterammo".)
            result(test.nailClip > 0 && cl.stats[protocol::STAT_QVR_WEAPONCLIP] == test.nailClip,
                va("the hold kept the nailgun's magazine in (%d rounds, %d before)", cl.stats[protocol::STAT_QVR_WEAPONCLIP],
                    test.nailClip));
            result(frames > 1, va("held, the wheel opens round that hand (after %d frames)", frames));
            result(placeCount() == 11,
                va("eleven places: the empty hand, the six holsters, DROP, FLIP GRIP, UNLOAD and the grenade (%d)", placeCount()));
        }
        break;
    }
    case 3:
    {
        // (single player only: a server keeps its own time)
        const bool slowed = timescale::current() <= za::clamp(vr_weaponwheel_timescale.value, 0.05f, 1.f) + 0.02f;
        // Then the left stick walks forward a while (his note 2026-11-23: he could walk away from it): the wheel comes
        // along, the hand still on the same place on it.
        if(!test.mp && slowed && test.walk < walkFrames)
        {
            if(test.walk == 0)
            {
                test.cursor0 = ws().cursor;
                Cbuf_AddText("vr_mock_stick off 0 1\n");
            }
            if(++test.walk == walkFrames)
            {
                Cbuf_AddText("vr_mock_stick off 0 0\n");
                const float moved = glm::distance(ws().cursor, test.cursor0);
                result(ws().hand == main && ws().followed > 8.f && moved < 2.f,
                    va("walked with it open: the wheel carried %.1f units, the hand's cursor moved %.1f pixels", ws().followed, moved));
            }
            break;
        }
        if(waitFor(test.mp || (slowed && test.walk >= walkFrames), "time slowed") && !test.mp)
        {
            result(true, va("time slowed while open (x%.2f)", timescale::current()));
        }
        break;
    }
    case 4:
    {
        const int want = choiceOfKey(1);
        aimAt(main, want);
        if(waitFor(want >= 0 && ws().hovered == want, "the right hand onto the right shoulder's slot"))
        {
            result(ws().ticks > 1, va("the slot lit (%s), a tick (%d so far)", ws().choices[want].title, ws().ticks));
            Cbuf_AddText("screenshot\n");
        }
        break;
    }
    case 5:
        // (a few counted frames: the screenshot's image drawn first)
        aimAt(main, choiceOfKey(1));
        if(waitFor(++test.mark >= 3, "the screenshot"))
        {
            result(vr_wheel_bind_panel.value == 0.f || ws().panelShown,
                va("the bindings panel above the wheel (vr_wheel_bind_panel %g: the controllers' pictures, labelled)",
                    vr_wheel_bind_panel.value));
            press(main, false);
        }
        break;
    case 6:
        if(waitFor(ws().hand < 0 && handWeapon(main) == WID_SHOTGUN && holsterStat(1) == WID_FIST, "the shotgun in the right hand"))
        {
            result(true, "let go on the right shoulder: the shotgun in the right hand, its holster empty");
            if(test.mp)
            {
                finish();
                return;
            }
            for(int i = 0; i < flatholster::holsterSlots; i++)
            {
                test.nailgunAt = holsterStat(i) == WID_NAILGUN ? i : test.nailgunAt;
            }
            if(!test.t.check(test.nailgunAt >= 0, "weaponwheel_test", "the nailgun put away in a holster"))
            {
                finish();
                return;
            }
            Con_Printf("weaponwheel_test: PASS the nailgun put away in a holster (%d)\n", test.nailgunAt);
            press(main, true); // (the right hand's UNLOAD: steps 30 .. 32, then back to 7)
            test.step = 30;
        }
        break;
    case 30:
        (void)waitFor(ws().hand == main, "the wheel open round the right hand for UNLOAD");
        break;
    case 31:
    {
        const int want = choiceOfKey(keyUnload);
        aimAt(main, want);
        if(waitFor(want >= 0 && ws().hovered == want, "the right hand onto UNLOAD"))
        {
            test.clip = cl.stats[protocol::STAT_QVR_WEAPONCLIP];
            test.ammo = cl.stats[STAT_SHELLS];
            result(ws().choices[want].usable && test.clip > 0,
                va("UNLOAD lit for the right hand's shotgun (%d shells in it, %d in the reserve)", test.clip, test.ammo));
            const int flip = choiceOfKey(keyFlip);
            result(flip >= 0 && !ws().choices[flip].usable, "FLIP GRIP greyed: the shotgun's grip does not flip");
            press(main, false);
        }
        break;
    }
    case 32:
        if(waitFor(cl.stats[protocol::STAT_QVR_WEAPONCLIP] == 0, "the shotgun unloaded"))
        {
            result(handWeapon(main) == WID_SHOTGUN && cl.stats[STAT_SHELLS] == test.ammo + test.clip,
                va("UNLOAD: the shotgun empty, its shells in the reserve (%d -> %d)", test.ammo, cl.stats[STAT_SHELLS]));
            press(off, true);
            test.step = 7;
        }
        break;
    case 7:
        if(waitFor(ws().hand == off, "the wheel open round the left hand"))
        {
            aimAt(off, -1);
        }
        break;
    case 8:
        aimAt(off, -1);
        if(waitFor(ws().outside, "the left hand off the wheel"))
        {
            result(ws().hovered < 0, "off the wheel: nothing lit");
            Cbuf_AddText("screenshot\n");
        }
        break;
    case 9:
        if(waitFor(++test.mark >= 3, "the screenshot"))
        {
            press(off, false);
        }
        break;
    case 10:
        // (counted frames for a pick's round trip: nothing must change)
        if(waitFor(++test.mark >= 30, "a pick's time"))
        {
            result(ws().hand < 0 && handWeapon(off) == WID_FIST && holsterStat(test.nailgunAt) == WID_NAILGUN && ws().cancels >= 1,
                "let go off the wheel: cancelled, nothing moved");
            press(off, true);
        }
        break;
    case 11:
        if(waitFor(ws().hand == off, "the wheel open round the left hand again"))
        {
            aimAt(off, choiceOfKey(test.nailgunAt));
        }
        break;
    case 12:
    {
        const int want = choiceOfKey(test.nailgunAt);
        aimAt(off, want);
        if(waitFor(want >= 0 && ws().hovered == want, "the left hand onto the nailgun's slot"))
        {
            press(off, false);
        }
        break;
    }
    case 13:
        if(waitFor(handWeapon(off) == WID_NAILGUN && holsterStat(test.nailgunAt) == WID_FIST, "the nailgun in the left hand"))
        {
            result(true, "the left hand (either hand) takes the nailgun from its holster");
            result(cl.stats[protocol::STAT_QVR_WEAPONCLIP2] == test.nailClip,
                va("holstered and drawn, the nailgun kept its rounds (%d, %d before)", cl.stats[protocol::STAT_QVR_WEAPONCLIP2],
                    test.nailClip));
        }
        break;
    case 14:
        // (counted frames: a weapon its grip does not hold falls at once; the wheel's stays until the grip is pressed)
        if(waitFor(++test.mark >= 40, "a while"))
        {
            result(handWeapon(off) == WID_NAILGUN, "the left hand keeps it, its grip not held");
            press(main, true);
        }
        break;
    case 15:
        (void)waitFor(ws().hand == main, "the wheel open round the right hand again");
        break;
    case 16:
    {
        const int want = choiceOfKey(-1);
        aimAt(main, want);
        if(waitFor(want >= 0 && ws().hovered == want, "the right hand onto EMPTY HAND"))
        {
            press(main, false);
        }
        break;
    }
    case 17:
        if(waitFor(ws().hand < 0 && handWeapon(main) == WID_FIST && holsterStat(1) == WID_SHOTGUN, "the shotgun put away"))
        {
            result(true, "EMPTY HAND: the shotgun back in the holster it came from");
        }
        break;
    case 18:
        // The left hand's nailgun: Y tapped (shorter than the wheel's hold) still ejects its magazine (its bit sent as it is
        // let go: vr_input.cpp WheelHold), steps 40 .. 41; then DROP (its UNLOAD greyed: the clip empty; the right hand's
        // UNLOAD was steps 30 .. 32).
        if(waitFor(++test.mark >= 10, "a pick's time"))
        {
            press(off, true);
        }
        test.step = test.step == 19 ? 40 : test.step;
        break;
    case 40:
        // (two counted frames: well short of vr_weaponwheel_hold)
        if(waitFor(++test.mark >= 2, "a tap"))
        {
            press(off, false);
        }
        break;
    case 41:
        if(waitFor(cl.stats[protocol::STAT_QVR_WEAPONCLIP2] == 0, "the tap's eject"))
        {
            result(ws().hand < 0 && handWeapon(off) == WID_NAILGUN, "Y tapped: no wheel, its magazine ejected");
            press(off, true);
            test.step = 22;
        }
        break;
    case 22:
        (void)waitFor(ws().hand == off, "the wheel open round the left hand for DROP");
        break;
    case 23:
    {
        const int un = choiceOfKey(keyUnload), want = choiceOfKey(keyDrop);
        // (aimed once: vr_mock_hand_to moves from the hand's place last frame, so sent every frame it overshoots and
        // swings about the slot; at the eleven-place wheel's DROP it never settled on it)
        if(pointed(want, "the left hand onto DROP"))
        {
            result(un >= 0 && !ws().choices[un].usable && cl.stats[protocol::STAT_QVR_WEAPONCLIP2] == 0,
                "UNLOAD greyed: nothing in the left hand's nailgun");
            result(ws().choices[want].usable, "DROP lit: the left hand holds the nailgun");
            press(off, false);
        }
        break;
    }
    case 24:
        if(waitFor(handWeapon(off) == WID_FIST, "the nailgun dropped"))
        {
            result(true, "DROP: the left hand's nailgun dropped");
            Cbuf_AddText(giveDagger); // (FLIP GRIP: steps 50 .. 53, then on to 25)
            test.step = 50;
        }
        break;
    case 50:
        if(waitFor(handWeapon(main) == WID_SWORD, "the dagger in the right hand"))
        {
            test.flags0 = mainFlags();
            press(main, true);
        }
        break;
    case 51:
        // (B is the dagger's flip too (the eject button's, vr_revgrip.qc): the hold that opens the wheel must not flip it)
        if(waitFor(ws().hand == main, "the wheel open round the right hand with the dagger"))
        {
            result((mainFlags() & revGripFlag) == (test.flags0 & revGripFlag), "the hold flipped nothing");
        }
        break;
    case 52:
    {
        const int want = choiceOfKey(keyFlip);
        aimAt(main, want);
        if(waitFor(want >= 0 && ws().hovered == want, "the right hand onto FLIP GRIP"))
        {
            result(ws().choices[want].usable, "FLIP GRIP lit: the right hand holds the dagger");
            press(main, false);
        }
        break;
    }
    case 53:
        if(waitFor((mainFlags() & revGripFlag) != (test.flags0 & revGripFlag), "the dagger's grip flipped"))
        {
            result(ws().hand < 0 && handWeapon(main) == WID_SWORD, va("let go on FLIP GRIP: the dagger's grip turned over (%s)",
                                                                   (mainFlags() & revGripFlag) ? "reversed" : "upright"));
            test.step = 25;
        }
        break;
    case 25:
        if(waitFor(timescale::current() >= 0.99f, "time back at 1"))
        {
            result(true, "closed: time back at 1");
            finish();
        }
        break;
    default:
        finish();
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Commands.

// "vr_wheel_pick <hand> <slot>": a client's pick (the holsters' provider), on the server: QC VR_Wheel_Pick with `self`
// his player.
void serverPick_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || !host_client->edict || Cmd_Argc() != 3)
    {
        return;
    }
    const int hand = atoi(Cmd_Argv(1)), slot = atoi(Cmd_Argv(2));
    if((hand != 0 && hand != 1) || slot < keyMultiGrenade || slot >= flatholster::holsterSlots)
    {
        return;
    }
    const progs::ServerVm vm;
    const func_t fn = progs::findFunction("VR_Wheel_Pick");
    if(!fn)
    {
        return;
    }
    const int self = pr_global_struct->self;
    const float parm[2]{G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1)};
    pr_global_struct->time = qcvm->time;
    pr_global_struct->self = EDICT_TO_PROG(host_client->edict);
    G_FLOAT(OFS_PARM0) = static_cast<float>(hand);
    G_FLOAT(OFS_PARM1) = static_cast<float>(slot);
    PR_ExecuteProgram(fn);
    pr_global_struct->self = self;
    G_FLOAT(OFS_PARM0) = parm[0], G_FLOAT(OFS_PARM1) = parm[1];
}

void dump_f()
{
    Con_Printf("weapon wheel: %s (hand %d), provider %s, %d choices, hovered %d%s, cursor %.0f %.0f, fade %.2f, time x%.2f; "
               "opens %d, picks %d, cancels %d, ticks %d\n",
        ws().hand >= 0 ? "open" : "closed", ws().hand, provider().name, ws().count, ws().hovered, ws().outside ? " (outside)" : "",
        ws().cursor.x, ws().cursor.y, ws().fade, timeScale(), ws().opens, ws().picks, ws().cancels, ws().ticks);
    Con_Printf("  hands: left weapon %d clip %d, right weapon %d clip %d; unload/drop bits %d (STAT_QVR_FLATUNLOADFULL)\n",
        handWeapon(HAND_OFF), cl.stats[protocol::STAT_QVR_WEAPONCLIP2], handWeapon(HAND_MAIN), cl.stats[protocol::STAT_QVR_WEAPONCLIP],
        cl.stats[protocol::STAT_QVR_FLATUNLOADFULL]);
    Con_Printf("  placed: centre %.1f %.1f %.1f, carried %.1f units since it opened (the play space's walk and turn)\n",
        ws().centre.x, ws().centre.y, ws().centre.z, ws().followed);
    for(int i = 0; i < ws().count; i++)
    {
        const Choice& c = ws().choices[i];
        Con_Printf("  %d: key %d, %s: %s%s\n", i, c.key, c.title, c.name[0] ? c.name : "(empty)", c.usable ? "" : " (greyed)");
    }
}

void test_f()
{
    test = Test{};
    test.on = true;
    test.mp = Cmd_Argc() > 1 && !q_strcasecmp(Cmd_Argv(1), "mp");
    test.gren = Cmd_Argc() > 1 && (!q_strcasecmp(Cmd_Argv(1), "grenade") || !q_strcasecmp(Cmd_Argv(1), "flatgrenade"));
    test.flat = Cmd_Argc() > 1 && (!q_strcasecmp(Cmd_Argv(1), "flat") || !q_strcasecmp(Cmd_Argv(1), "flatgrenade"));
    if(!test.flat)
    {
        test.wheelWas = vr_weaponwheel.value; // (off by default since 2026-11-24: on for the test)
        Cvar_SetValueQuick(&vr_weaponwheel, 1.f);
    }
    Con_Printf("weaponwheel_test: start%s\n", test.mp ? " (mp)" : test.flat ? " (flat)" : "");
}

} // namespace

const Provider& holsterProvider()
{
    return holsters;
}

void setProvider(const Provider* p)
{
    ws().provider = p;
}

int buttonKind()
{
    return za::clamp(static_cast<int>(vr_weaponwheel_button.value), 0, 2);
}

double holdSeconds()
{
    return za::clamp(static_cast<double>(vr_weaponwheel_hold.value), 0.0, 2.0);
}

double flatHoldSeconds()
{
    return za::clamp(static_cast<double>(vr_flat_wheel_hold.value), 0.0, 2.0);
}

bool canOpen(int hand)
{
    if(key_dest != key_game || cls.signon != SIGNONS || cl.stats[STAT_HEALTH] <= 0 || cl.intermission || ws().hand >= 0 ||
        toolgun::heldHand() == hand)
    {
        return false;
    }
    if(flatplay::active())
    {
        // (The holster menu draws the same image: one at a time. The toolgun's panel has the mouse.)
        return vr_flat_wheel.value != 0.f && vr_flat_holsters.value != 0.f && !flatholster::shown() &&
               !toolgun::panel::flatShown();
    }
    if(vr_weaponwheel.value == 0.f || !vrActive())
    {
        return false;
    }
    if(buttonKind() == 0)
    {
        // B and Y: the grappling hook's reel while it is in that hand; vr_menu_button 2 (Y) and 3 (B) hold them for the menu.
        const int menuHold = static_cast<int>(vr_menu_button.value);
        if(handWeapon(hand) == WID_GRAPPLE || (menuHold == 2 && hand == HAND_OFF) || (menuHold == 3 && hand == HAND_MAIN))
        {
            return false;
        }
    }
    return true;
}

bool holdsButtonBack(int hand)
{
    return !flatplay::active() && handWeapon(hand) != WID_FIST && canOpen(hand);
}

void open(int hand)
{
    const hands::State& s = hands::current();
    const bool flat = flatplay::active();
    if(!canOpen(hand) || (!flat && !s.valid))
    {
        return;
    }
    ws().hand = ws().shownHand = hand;
    ws().flat = flat;
    ws().count = provider().list(hand, ws().choices, maxChoices);
    ws().hovered = -1;
    ws().outside = false;
    ws().cursor = ringMid;
    ws().opens++;
    ws().followed = 0.f;
    if(flat)
    {
        // Where that hand is on the screen; the mouse starts in the middle (drawFlat keeps it all on the window).
        ws().screenAt = flatHandOnWindow(hand, s);
        tick(hand); // (no menu sound on opening, flat or VR: his ask 2026-11-23, as the pick's click)
        Con_DPrintf("weapon wheel: opens at the %s hand on the screen (%.0f %.0f, %d choices)\n",
            hand == HAND_MAIN ? "right" : "left", ws().screenAt.x, ws().screenAt.y, ws().count);
        return;
    }
    // Beyond the hand from the eyes, facing them (upright: its up the world's, leaned to face them).
    const glm::vec3 at = s.pos[hand];
    glm::vec3 away = at - s.head;
    away = glm::length(away) > 0.01f ? glm::normalize(away) : glm::vec3{1.f, 0.f, 0.f};
    ws().centre = at + away * za::max(vr_weaponwheel_distance.value, 0.f);
    ws().normal = -away;
    const glm::vec3 side = glm::cross(glm::vec3{0.f, 0.f, 1.f}, ws().normal);
    ws().right = glm::length(side) > 0.01f ? glm::normalize(side) : glm::vec3{0.f, -1.f, 0.f};
    ws().up = glm::cross(ws().normal, ws().right);
    // Kept in the play space (his note 2026-11-23: walking with the stick left it behind): the stick's walk and turn carry
    // it as they carry the hands (follow), so the hand stays on its slot.
    ws().roomCentre = hands::rotateYaw(ws().centre - s.spaceOrigin, -s.turnYaw);
    ws().roomNormal = hands::rotateYaw(ws().normal, -s.turnYaw);
    ws().roomRight = hands::rotateYaw(ws().right, -s.turnYaw);
    ws().roomUp = hands::rotateYaw(ws().up, -s.turnYaw);
    tick(hand); // (the haptic tick only: no menu sound on opening)
    Con_DPrintf("weapon wheel: opens round the %s hand (%d choices)\n", hand == HAND_MAIN ? "right" : "left", ws().count);
}

void release(int hand)
{
    if(ws().hand != hand)
    {
        return;
    }
    if(ws().hovered >= 0 && ws().hovered < ws().count && ws().choices[ws().hovered].usable)
    {
        const Choice& c = ws().choices[ws().hovered];
        Con_DPrintf("weapon wheel: the %s hand takes %s (%s)\n", hand == HAND_MAIN ? "right" : "left", c.title,
            c.name[0] ? c.name : "empty");
        provider().pick(hand, c); // (no menu click: the pick's own sounds, a holster's, are the feedback; his ask 2026-11-23)
        close(true);
        return;
    }
    Con_DPrintf("weapon wheel: let go %s (cursor %.0f %.0f): nothing\n",
        ws().outside ? "off the wheel" : ws().hovered >= 0 ? "on a greyed slot" : "in the middle", ws().cursor.x, ws().cursor.y);
    close(false);
}

bool isOpen()
{
    return ws().hand >= 0;
}

bool openInHeadset()
{
    return ws().hand >= 0 && !ws().flat;
}

bool flatMouse(int dx, int dy)
{
    if(ws().hand < 0 || !ws().flat)
    {
        return false;
    }
    // The holster menu's cursor speed (vr_flat_holster_menu_cursor_speed 1: as the desktop's pointer), a little past the
    // image's edge (off the wheel: letting go cancels).
    const float k = za::max(vr_flat_holster_menu_cursor_speed.value, 0.f) / za::max(ws().scale, 0.1f);
    ws().cursor.x = za::clamp(ws().cursor.x + static_cast<float>(dx) * k, -flatCursorMargin, panelW + flatCursorMargin);
    ws().cursor.y = za::clamp(ws().cursor.y + static_cast<float>(dy) * k, -flatCursorMargin, panelH + flatCursorMargin);
    return true;
}

bool escape()
{
    if(ws().hand < 0 || !ws().flat)
    {
        return false;
    }
    Con_DPrintf("weapon wheel: closed (Esc)\n");
    close(false);
    return true;
}

float crosshairAlpha()
{
    return ws().flat ? 1.f - ws().fade * za::clamp(vr_flat_holster_menu_xhair_fade.value, 0.f, 1.f) : 1.f;
}

float blurRadius()
{
    return ws().flat ? ws().fade * za::clamp(vr_flat_holster_menu_blur.value, 0.f, 40.f) : 0.f;
}

float timeScale()
{
    return ws().hand >= 0 ? za::clamp(vr_weaponwheel_timescale.value, 0.05f, 1.f) : 1.f;
}

void frame(const hands::State& s)
{
    const double now = realtime;
    const float dt = ws().lastTime >= 0.0 ? za::clamp(static_cast<float>(now - ws().lastTime), 0.f, 0.1f) : 0.f;
    ws().lastTime = now;
    const bool modeGone = ws().flat ? !flatplay::active() || vr_flat_wheel.value == 0.f : vr_weaponwheel.value == 0.f || !s.valid;
    if(ws().hand >= 0 && (modeGone || key_dest != key_game || cls.signon != SIGNONS || cl.stats[STAT_HEALTH] <= 0 || cl.intermission))
    {
        close(false); // (a menu, death, the level's end: closed, its button's letting go does nothing)
    }
    if(ws().hand >= 0)
    {
        ws().count = provider().list(ws().hand, ws().choices, maxChoices); // (what the holsters hold now)
        if(!ws().flat)
        {
            follow(s);
            ws().cursor = cursorOf(s.pos[ws().hand]); // (flatscreen play: the mouse's, flatMouse)
        }
        const int was = ws().hovered;
        ws().hovered = choiceAt(ws().cursor, ws().outside);
        if(ws().hovered >= 0 && ws().hovered != was)
        {
            tick(ws().hand);
        }
    }
    ws().fade = za::clamp(ws().fade + (ws().hand >= 0 ? dt : -dt) / fadeSeconds, 0.f, 1.f);
    testStep();
}

void render()
{
    ws().texture = 0;
    if(ws().fade <= 0.f || ws().count <= 0)
    {
        return;
    }
    gadget::HolsterMenuSlot slots[maxChoices]{};
    gadget::HolsterMenuButton buttons[maxChoices]{};
    int slotCount = 0, buttonCount = 0;
    using Drop = gadget::HolsterMenuSlot::Drop;
    for(int i = 0; i < ws().count; i++)
    {
        const Choice& c = ws().choices[i];
        const glm::vec2 mid = choiceMid(i);
        if(c.icon != gadget::MenuIcon::None)
        {
            // An action: smaller, the holster menu's button (its icon above its title; lit with a second ring).
            gadget::HolsterMenuButton& b = buttons[buttonCount++];
            b.rect = {mid.x - actionW * 0.5f, mid.y - actionH * 0.5f, actionW, actionH};
            b.icon = c.icon;
            b.label = c.title;
            b.hovered = i == ws().hovered;
            b.enabled = c.usable;
            continue;
        }
        gadget::HolsterMenuSlot& s = slots[slotCount++];
        const float h = c.half ? (slotH - halfGap) * 0.5f : slotH; // (a split slot's half: the slot's own outline in two)
        s.rect = {mid.x - slotW * 0.5f, mid.y - h * 0.5f, slotW, h};
        s.compact = c.half != 0;
        s.title = c.title;
        s.name = c.name;
        s.model = c.model;
        s.clip = c.clip;
        s.clipSize = c.clipSize;
        s.ammoType = c.ammoType;
        s.reserve = c.reserve;
        s.attach = c.attach;
        s.hovered = i == ws().hovered;
        s.drop = s.hovered && c.usable ? Drop::Valid : c.usable ? Drop::None : Drop::Invalid;
    }
    // The middle's cross, and a stroke from it to the hand (outside: to the wheel's edge that way).
    glm::vec2 to = ws().cursor - ringMid;
    const float len = glm::length(to);
    const float past = glm::length(to / outerR); // (the outer oval at 1)
    if(past > 1.f)
    {
        to /= past;
    }
    const glm::vec4 lines[3] = {{ringMid.x - 6.f, ringMid.y, ringMid.x + 6.f, ringMid.y},
        {ringMid.x, ringMid.y - 6.f, ringMid.x, ringMid.y + 6.f}, {ringMid.x, ringMid.y, ringMid.x + to.x, ringMid.y + to.y}};
    gadget::HolsterMenuView v;
    v.size = {panelW, panelH};
    v.slots = slots;
    v.slotCount = slotCount;
    v.buttons = buttons;
    v.buttonCount = buttonCount;
    v.lines = lines;
    v.lineCount = len > innerR * 0.5f ? 3 : 2;
    const bool lit = ws().hovered >= 0 && ws().choices[ws().hovered].usable;
    const int litKey = lit ? ws().choices[ws().hovered].key : 0;
    v.footer = !lit                 ? (ws().outside ? "Let go here: cancel" : ws().flat ? "Point at a slot" : "Move the hand to a slot")
               : litKey == keyDrop   ? "Let go: drop it"
               : litKey == keyUnload ? "Let go: unload it"
               : litKey == keyFlip   ? "Let go: flip its grip"
               : litKey == keyGrenade ? "Let go: take a grenade"
               : litKey == keyMultiGrenade ? "Let go: take a cluster grenade"
                                     : "Let go: take it";
    const gadget::VisorImage img = gadget::renderHolsterMenu(v);
    ws().texture = img.shown ? img.texture : 0;
    // The bindings panel above it in a headset (bindpanel2; vr_wheel_bind_panel): the controllers' pictures, labelled.
    ws().panel = ws().flat ? gadget::VisorImage{} : bindpanel::headsetImage();
    ws().panelShown = ws().panelShown || ws().panel.shown;
}

void drawInEye(const hands::State& s)
{
    if(!ws().texture || ws().fade <= 0.f || ws().flat || !s.valid || renderdoc::hidingPanel())
    {
        return;
    }
    const glm::vec3 corner[4] = {worldAt({0.f, panelH}), worldAt({panelW, panelH}), worldAt({panelW, 0.f}), worldAt({0.f, 0.f})};
    flatholster::drawImage(ws().texture, {panelW, panelH}, corner, gfx::sceneViewProjection(), ws().fade);
    if(ws().panel.shown)
    {
        // Above the wheel, centred on it, at its scale (a wheel pixel a panel pixel), a little gap between.
        const glm::vec2 p = ws().panel.size;
        const float x0 = ringMid.x - p.x * 0.5f, y1 = -panelGap;
        const glm::vec3 above[4] = {worldAt({x0, y1}), worldAt({x0 + p.x, y1}), worldAt({x0 + p.x, y1 - p.y}), worldAt({x0, y1 - p.y})};
        flatholster::drawImage(ws().panel.texture, p, above, gfx::sceneViewProjection(), ws().fade);
    }
}

void drawFlat()
{
    if(!ws().flat || ws().fade <= 0.f || !flatplay::active() || vid.width <= 0 || vid.height <= 0)
    {
        return;
    }
    render();
    if(!ws().texture)
    {
        return;
    }
    // vr_flat_wheel_size of the window's height, its middle where the hand was (all of it kept on the window), framed as the
    // holster menu is on the window (its composite: flatholster::drawOnWindow), the mouse's arrow on it while open.
    const float W = static_cast<float>(vid.width), H = static_cast<float>(vid.height);
    float h = za::min(za::clamp(vr_flat_wheel_size.value, 0.2f, 1.f) * H * (panelH / refH), H * 0.96f); // (the ten-slot wheel's pixels kept)
    float w = h * panelW / panelH;
    if(w > W * 0.96f)
    {
        w = W * 0.96f;
        h = w * panelH / panelW;
    }
    ws().scale = h / panelH;
    // The bindings panel over it (wheelui; vr_flat_bind_panel), at its scale, the two kept on the window together (the
    // panel under the wheel when the wheel is too high for it above).
    const glm::vec2 keys = bindpanel::shown() ? bindpanel::size() : glm::vec2{0.f};
    const float ks = za::min(ws().scale, W * 0.96f / za::max(keys.x, 1.f));
    const float above = keys.y > 0.f ? (keys.y + 4.f) * ks : 0.f;
    const float x0 = za::clamp(ws().screenAt.x - w * 0.5f, W * 0.02f, za::max(W * 0.98f - w, W * 0.02f));
    const float y0 = za::clamp(ws().screenAt.y - h * 0.5f, H * 0.02f + above, za::max(H * 0.98f - h, H * 0.02f + above));
    if(keys.y > 0.f)
    {
        const float kx = za::clamp(x0 + (w - keys.x * ks) * 0.5f, W * 0.02f, za::max(W * 0.98f - keys.x * ks, W * 0.02f));
        bindpanel::drawOnWindow(kx, y0 - above, ks, ws().fade);
    }
    flatholster::drawOnWindow(ws().texture, {panelW, panelH}, {x0, y0, w, h}, ws().fade);
    if(ws().hand >= 0)
    {
        flatholster::drawCursorOnWindow({x0 + ws().cursor.x * ws().scale, y0 + ws().cursor.y * ws().scale}, ws().fade);
    }
}

void registerCommands()
{
    Cmd_AddCommand_ClientCommand("vr_wheel_pick", serverPick_f);
    Cmd_AddCommand("vr_weaponwheel_dump", dump_f);
    Cmd_AddCommand("vr_weaponwheel_test", test_f);
}

} // namespace qvr::weaponwheel
