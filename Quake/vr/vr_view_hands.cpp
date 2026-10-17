// vr_view_hands.cpp -- the hands' set-up: fingers and grasps, pushing against walls and props, the rigged hand
// (setupRigHand), ground and free spots, limb holds, setupHand and drawHand.
// Split from vr_view.cpp (a pure move); what they share is in vr_view_internal.hpp.

#include "vr_view_internal.hpp"

namespace view_detail
{
namespace
{
// ----------------------------------------------------------------------------
// Hands

[[nodiscard]] glm::vec3 fingerOffset(int finger, int hand)
{
    glm::vec3 result{vr_fingers_and_base_x.value, vr_fingers_and_base_y.value,
        vr_fingers_and_base_z.value};

    if(hand == HAND_OFF)
    {
        result += glm::vec3{vr_fingers_and_base_offhand_x.value,
            vr_fingers_and_base_offhand_y.value, vr_fingers_and_base_offhand_z.value};
    }

    if(finger == FingerBase)
    {
        return result + glm::vec3{vr_finger_base_x.value, vr_finger_base_y.value,
                            vr_finger_base_z.value};
    }

    result += glm::vec3{vr_fingers_x.value, vr_fingers_y.value, vr_fingers_z.value};

    // On a held weapon, its thumb placement (fgr_thumb_x/y/z; round 21: the fingers wrap the grip on their own).
    const int slot = weapons::heldSlot(hand);
    const bool holding = slot >= 0 && slot != weapons::fistSlot() && !twohand::helping(hand);
    // On the held torch, its grip's (vr_flashlight_low_thumb_*, _high_thumb_*).
    flashlight::Fingers torch;
    const bool torchHeld = !holding && flashlight::fingers(hand, torch);

    switch(finger)
    {
        case FingerThumb:
            return result + glm::vec3{vr_finger_thumb_x.value, vr_finger_thumb_y.value,
                                vr_finger_thumb_z.value} +
                   (holding     ? weapons::vec(slot, Key::FingerThumbX, Key::FingerThumbY, Key::FingerThumbZ)
                       : torchHeld ? torch.thumb
                                   : glm::vec3{0.f});
        case FingerIndex:
            return result + glm::vec3{vr_finger_index_x.value, vr_finger_index_y.value,
                                vr_finger_index_z.value};
        case FingerMiddle:
            return result + glm::vec3{vr_finger_middle_x.value, vr_finger_middle_y.value,
                                vr_finger_middle_z.value};
        case FingerRing:
            return result + glm::vec3{vr_finger_ring_x.value, vr_finger_ring_y.value,
                                vr_finger_ring_z.value};
        default:
            return result + glm::vec3{vr_finger_pinky_x.value, vr_finger_pinky_y.value,
                                vr_finger_pinky_z.value};
    }
}
} // namespace

// How hurt the player is, 0..3 (vr_body_state): the body's and the hands' damage skins
// (make_vrbody.py, make_bloody_hands.py).
[[nodiscard]] int damageLevel()
{
    if(!vr_body_state.value)
    {
        return 0;
    }
    const int health = cl.stats[STAT_HEALTH];
    return health > 75 ? 0 : health > 50 ? 1 : health > 25 ? 2 : 3;
}

// What `held`'s fingers wrap: its own shape, else its entity's model's pose (`frame`, or its own if < 0).
[[nodiscard]] const grasp::Shape* heldShape(const Held& held, int frame)
{
    return held.shape ? held.shape : grasp::shapeOf(*held.ent, frame);
}

// Where `held`'s shape is drawn: its shape's coordinates to the world.
[[nodiscard]] glm::mat4 heldToWorld(const Held& held)
{
    return held.shape ? held.toWorld : grasp::shapeToWorld(*held.ent, held.mirrored);
}

// The weapon's (Key::FingerManual, FingerCurl*) or a hotspot's fingers set by hand, into `held`.
void setManualFingers(Held& held, bool manual, const float curl[handrig::FingerCount], float thumbAcross)
{
    held.manual = manual;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        held.manualCurl[f] = CLAMP(0.f, curl[f], 1.f);
    }
    held.manualThumbAcross = CLAMP(0.f, thumbAcross, 1.f);
}

// Weapon slot `slot`'s grip overlap and fingers (Key::GripOverlap, FingerManual, FingerCurl*, FingerThumbAcross), into
// `held` (the gun in the hand, set or drawn).
void weaponFingers(Held& held, int slot)
{
    held.overlap = CLAMP(0.f, weapons::value(slot, Key::GripOverlap), 1.f) * weapons::maxOverlapCm;
    const float curl[handrig::FingerCount] = {weapons::value(slot, Key::FingerCurlThumb), weapons::value(slot, Key::FingerCurlIndex),
        weapons::value(slot, Key::FingerCurlMiddle), weapons::value(slot, Key::FingerCurlRing), weapons::value(slot, Key::FingerCurlPinky)};
    setManualFingers(held, weapons::value(slot, Key::FingerManual) >= 0.5f, curl, weapons::value(slot, Key::FingerThumbAcross));
}

namespace
{
constexpr int rigFinger[handrig::FingerCount] = {FingerThumb, FingerIndex, FingerMiddle, FingerRing, FingerPinky};
} // namespace

[[nodiscard]] glm::vec3 vec3Of(const float* v)
{
    return {v[0], v[1], v[2]};
}

namespace
{
// A finger offset (fingerOffset's hand units, y to the hand's right) in model space, at the offsets' scale.
[[nodiscard]] glm::vec3 offsetInModel(const glm::vec3& v)
{
    return glm::vec3{v.x, -v.y, v.z} * weapons::offsetScale();
}
} // namespace

// A finger's curl as drawn without a grasp: the controller's (whole frames or between them: the joints turn smoothly),
// with a held weapon's finger tweak.
[[nodiscard]] float rigCurl(int hand, int finger)
{
    return CLAMP(0.f, fingerFrames[hand][finger] + 5.f * fingerBias[hand][finger], 5.f);
}
[[nodiscard]] glm::mat4 toMat4(const float m[16])
{
    glm::mat4 r;
    for(int c = 0; c < 4; c++)
    {
        for(int row = 0; row < 4; row++)
        {
            r[c][row] = m[c * 4 + row];
        }
    }
    return r;
}

namespace
{
// Whether `g` was solved for `held` where it is in the hand now: turned no more than `degrees`, moved no more than
// `units` (hand units) from it, the fingers where they were. By default, vr_hand_fit_resolve's (cm, and twice as many
// degrees; `rigUnit` world units a hand unit).
[[nodiscard]] bool sameGrasp(const Grasp& g, const qmodel_t* model, int frame, const glm::mat4& inRig, const handrig::Pose& pose,
    float rigUnit)
{
    const float cm = za::fmax(vr_hand_fit_resolve.value, 0.f);
    const float degrees = 2.f * cm;
    const float units = rigUnit > 0.f ? cm * 0.01f * units::metresToUnits() / rigUnit : 0.f;
    if(!g.valid || g.model != model || g.frame != frame)
    {
        return false;
    }
    if(glm::distance(glm::vec3{inRig[3]}, glm::vec3{g.inRig[3]}) > units)
    {
        return false;
    }
    const float cosine = za::cos(glm::radians(degrees));
    for(int c = 0; c < 3; c++)
    {
        const glm::vec3 a{inRig[c]}, b{g.inRig[c]};
        if(glm::dot(a, b) < cosine * glm::length(a) * glm::length(b))
        {
            return false;
        }
    }
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        if(glm::distance(pose.shift[f], g.shift[f]) > 0.01f)
        {
            return false;
        }
    }
    return true;
}
} // namespace

// A rig point as drawn, moved and turned by the palm's fit, in the rig's space before them.
[[nodiscard]] glm::vec3 drawnInRig(const RigHand& rh, const glm::vec3& p)
{
    const glm::vec3 c = grasp::palmCentre();
    return c + rh.palm + glm::mat3_cast(rh.turn) * (p - c);
}

namespace
{
int graspSolves[2]{}; // solves, per hand (vr_debug_grasp_trace)
int brushSolves[2]{}; // brushing the other hand's weapon: solves, per hand

void printGrasp(int hand, const Grasp& g, float rigUnit)
{
    Con_Printf("grasp (%.2f s): %s hand, %s: %d triangles within reach; the palm moved %.2f cm (%.1f %.1f %.1f), the thumb "
               "turned %.0f (choice %d), the palm %.0f; %.1f us%s; held at (%.3f %.3f %.3f) in the hand\n",
        vr_gametime, hand == HAND_MAIN ? "main" : "off", g.model ? g.model->name : "-", g.solution.triangles,
        glm::length(g.solution.palm) * rigUnit / units::metresToUnits() * 100.f, g.solution.palm.x, g.solution.palm.y,
        g.solution.palm.z, glm::degrees(glm::angle(g.solution.thumbTurn)), g.solution.thumbChoice,
        glm::degrees(glm::angle(g.solution.palmTurn)), g.solution.seconds * 1e6, g.rested ? ", afresh at rest" : "",
        g.inRig[3][0], g.inRig[3][1], g.inRig[3][2]);
    if(vr_debug_grasp.value >= 2.f)
    {
        constexpr const char* names[handrig::FingerCount] = {"thumb", "index", "middle", "ring", "pinky"};
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            const grasp::FingerStop& st = g.solution.finger[f];
            Con_Printf("  %-6s %s stops %.3f %.3f %.3f\n", names[f],
                st.startsInside ? "inside" : st.fromClosed ? "met (from closed)" : st.met ? "met" : "free", st.stop[0], st.stop[1],
                st.stop[2]);
        }
    }
}
} // namespace

// The solve's settings for what `held` is: the palm's move (vr_hand_fit_palm or _weapon, cm) and turn, the overlap
// (cm), in hand units (`rigUnit` world units each).
[[nodiscard]] grasp::Settings graspSettings(const Held& held, float rigUnit)
{
    const float toRig = rigUnit > 0.f ? 0.01f * units::metresToUnits() / rigUnit : 0.f;
    grasp::Settings s;
    s.palmLimit = za::fmax(held.weapon ? vr_hand_fit_palm_weapon.value : vr_hand_fit_palm.value, 0.f) * toRig;
    s.palmTurnLimit = za::fmax(vr_hand_fit_palm_turn.value, 0.f);
    // Round 21, third pass: a hand on a weapon (its own, or a hotspot of the other hand's) has its palm's place searched
    // near where the weapon's placement or the hotspot puts it (within vr_hand_fit_palm_weapon: along the grip and the
    // fingers, flush on it) for where the fingers hold best, never turned. The turn (up to 20 degrees towards the
    // surface the palm's normal met first) took the super nailgun's frame or its handle by a hair's difference in the
    // weapon's place at the grab: two grips, by turns. (Solved afresh once the weapon rests in the hand, updateGrasp:
    // the same grip however it was taken.) A cup keeps the fit it had (flush on the other hand and the weapon, turned
    // towards them): its place in the hand is worked out from the settings now (Held::canonical), so it is the same
    // every time, and a config's cups moved to the new definition look as they did.
    if(held.weapon && !held.cup)
    {
        s.palmTurnLimit = 0.f;
        s.searchPlace = true;
    }
    s.overlap = za::fmax(held.overlap >= 0.f ? held.overlap : vr_hand_fit_overlap.value, 0.f) * toRig;
    s.thenar = !held.weapon;
    s.thumbTop = held.thumbTop;
    s.thumbOutside = held.inPalm && vr_hand_fit_thumb_outside.value > 0.f;
    s.thumbSink = s.thumbOutside ? vr_hand_fit_thumb_outside.value * toRig : 0.f;
    s.thumbWide = s.thumbOutside && vr_hand_fit_thumb_wide.value > 0.f;
    return s;
}

namespace
{
// The hand's grasp of `held` (vr_grasp.cpp): solved on the spot, in the frame it is needed, when what it holds, its
// place in the hand (beyond vr_hand_fit_resolve) or the settings changed; else the last one stands. Tens of
// microseconds: no thread, the same inputs the same pose.
void updateGrasp(int hand, const Held& held, const glm::mat4& rigMatrix, float rigUnit)
{
    RigHand& rh = rigHands[hand];
    Grasp& g = rh.grasp;
    if(g.rig != handrig::generation()) // the hand was edited (vr_hand_reload): solved afresh
    {
        g.valid = false;
        g.rig = handrig::generation();
    }
    if(!held.ent || !held.ent->model || !vr_hand_fit.value)
    {
        g.valid = false;
        return;
    }
    const int frame = held.frame >= 0 ? held.frame : held.ent->frame;
    const grasp::Shape* shape = heldShape(held, frame);
    if(!shape)
    {
        g.valid = false;
        return;
    }
    const glm::mat4 inRig =
        held.canonical ? held.canonicalInRig : glm::inverse(rigMatrix) * heldToWorld(held);
    // Whether two places in the hand are the same, to a hundredth of a hand unit and a tenth of a degree; whether it
    // rests (where it was last frame).
    const auto samePlace = [](const glm::mat4& p, const glm::mat4& q) {
        if(glm::distance(glm::vec3{p[3]}, glm::vec3{q[3]}) >= 0.01f)
        {
            return false;
        }
        for(int c = 0; c < 3; c++)
        {
            const glm::vec3 a{p[c]}, b{q[c]};
            if(glm::dot(a, b) < za::cos(glm::radians(0.1f)) * glm::length(a) * glm::length(b))
            {
                return false;
            }
        }
        return true;
    };
    const bool steady = samePlace(inRig, rh.inRig);
    rh.inRig = inRig;
    rh.solveRig = rigMatrix;
    rh.rigUnit = rigUnit;

    // Cupping the other hand (a two-handed pistol grip): that hand is in the way too, as drawn (its fingers move with
    // its trigger and grip: solved again when they, or it, move).
    const RigHand& otherHand = rigHands[1 - hand];
    const bool cup = held.cup && otherHand.drawn;
    float other[handrig::FingerCount * handrig::jointsPerFinger]{};
    glm::mat4 otherInRig{1.f};
    bool otherMoved = false;
    if(cup)
    {
        otherInRig = glm::inverse(rigMatrix) * otherHand.rigToWorld;
        float change = 0.f;
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            for(int j = 0; j < handrig::jointsPerFinger; j++)
            {
                other[f * handrig::jointsPerFinger + j] = otherHand.pose.curl[f][j];
                change += za::fabs(other[f * handrig::jointsPerFinger + j] - g.other[f * handrig::jointsPerFinger + j]);
            }
        }
        otherMoved = change > 0.1f || glm::distance(glm::vec3{otherInRig[3]}, glm::vec3{g.otherInRig[3]}) > 0.1f;
    }

    // The settings changed: solved again.
    grasp::Settings settings = graspSettings(held, rigUnit);
    if(held.canonical && held.offset)
    {
        // The palm fitted without the tuning offset (solved again when that place or the settings change), kept as the
        // offset moves the hand.
        const bool fitSame = rh.fitValid && rh.fitModel == held.ent->model && rh.fitInRig == held.fitInRig &&
                             rh.fitSettings.palmLimit == settings.palmLimit && rh.fitSettings.palmTurnLimit == settings.palmTurnLimit &&
                             rh.fitSettings.overlap == settings.overlap && rh.fitSettings.searchPlace == settings.searchPlace &&
                             rh.fitSettings.thenar == settings.thenar && rh.fitSettings.thumbOutside == settings.thumbOutside &&
                             rh.fitSettings.thumbSink == settings.thumbSink && rh.fitSettings.thumbWide == settings.thumbWide;
        if(!fitSame)
        {
            QVR_PROFILE("grasp solve");
            grasp::Solution fit;
            grasp::solve(rh.pose, *shape, held.fitInRig, settings, nullptr, fit);
            rh.fitValid = true;
            rh.fitModel = held.ent->model;
            rh.fitInRig = held.fitInRig;
            rh.fitSettings = settings;
            rh.fitPalm = fit.palm;
            rh.fitTurn = fit.palmTurn;
        }
        settings.fixedPalm = true;
        settings.palmMove = rh.fitPalm;
        settings.palmTurnMove = rh.fitTurn;
    }
    const bool settingsChanged = settings.palmLimit != g.settings.palmLimit || settings.palmTurnLimit != g.settings.palmTurnLimit ||
                                 settings.overlap != g.settings.overlap || settings.thenar != g.settings.thenar ||
                                 settings.thumbTop != g.settings.thumbTop || settings.thumbOutside != g.settings.thumbOutside ||
                                 settings.thumbSink != g.settings.thumbSink || settings.thumbWide != g.settings.thumbWide ||
                                 settings.searchPlace != g.settings.searchPlace ||
                                 settings.fixedPalm != g.settings.fixedPalm || settings.palmMove != g.settings.palmMove ||
                                 settings.palmTurnMove != g.settings.palmTurnMove;
    bool fresh = false;
    if(!settingsChanged && !otherMoved && sameGrasp(g, held.ent->model, frame, inRig, rh.pose, rigUnit))
    {
        // Resting in the hand somewhere it wasn't solved afresh at (it moved in the hand, as a hand takes a grip, and
        // the solves then started from the one before): solved once more, afresh (round 21, third pass). A warm solve
        // keeps what it can of the one before, so where it rests after depended on how it came there (a foregrip
        // taken ten times: the palm 1.3 units apart); afresh, it is the same grip however it was taken.
        if(!steady || (g.rested && samePlace(inRig, g.restInRig)))
        {
            return;
        }
        fresh = true;
    }

    QVR_PROFILE("grasp solve");
    const bool same = !fresh && g.valid && g.model == held.ent->model && g.frame == frame;
    grasp::Solution solution;
    if(cup)
    {
        grasp::Shape& otherShape = scratch.otherHand;
        za::Vector<grasp::Triangle>& tris = scratch.otherHandTris;
        tris.clear();
        const auto at = [&](const glm::vec3& p) { return glm::vec3{otherInRig * glm::vec4{drawnInRig(otherHand, p), 1.f}}; };
        za::Vector<glm::vec3>& posed = scratch.otherHandVerts;
        handrig::vertices(otherHand.posed, posed);
        for(const auto& tri : handrig::rig().triangles)
        {
            tris.pushBack({{at(posed[tri[0]]), at(posed[tri[1]]), at(posed[tri[2]])}});
        }
        grasp::makeShape(tris, otherShape);
        const float toRig = rigUnit > 0.f ? 0.01f * units::metresToUnits() / rigUnit : 0.f;
        grasp::solve(rh.pose, *shape, inRig, settings, same ? &g.solution : nullptr, solution, &otherShape, glm::mat4{1.f},
            za::fmax(vr_hand_fit_overlap_hands.value, 0.f) * toRig);
    }
    else
    {
        grasp::solve(rh.pose, *shape, inRig, settings, same ? &g.solution : nullptr, solution);
    }
    za::copy(other, other + handrig::FingerCount * handrig::jointsPerFinger, g.other);
    g.otherInRig = otherInRig;
    g.valid = true;
    g.rested = !same && steady;
    g.restInRig = inRig;
    g.model = held.ent->model;
    g.frame = frame;
    g.inRig = inRig;
    g.settings = settings;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        g.shift[f] = rh.pose.shift[f];
    }
    g.solution = solution;
    graspSolves[hand]++;
    if(vr_debug_grasp.value)
    {
        printGrasp(hand, g, rigUnit);
    }
}

// How far past its stop on the weapon the trigger finger curls at a full pull (curl frames per joint: the knuckle
// little, the two distal joints more, a hook round the trigger).
constexpr float triggerPull[handrig::jointsPerFinger] = {0.6f, 1.4f, 1.2f};

// How much a finger grips (0..1) at the controller's curl: from half closed to nearly the full press.
[[nodiscard]] float engagement(float curl)
{
    const float t = za::fmin(za::fmax((curl - 2.5f) / 2.f, 0.f), 1.f);
    return t * t * (3.f - 2.f * t);
}
} // namespace

// The jointed hand's rig to the world for the hand at `pos` turned `handRot` (setupRigHand's placement: the entity where
// the palm model's origin is drawn), placing `ve` there if given.
glm::mat4 rigPlacement(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored, view::ViewEntity* ve)
{
    qmodel_t* const model = viewModel(handrig::modelName);
    const weapons::ModelTransform t = weapons::modelTransform(model);
    const float k = t.active ? t.k : 1.f;
    const glm::vec3 ts = t.active ? t.scale : glm::vec3{1.f};
    const glm::vec3 mBase = offsetInModel(fingerOffset(FingerBase, hand));
    const glm::vec3 e = mBase + k * (glm::vec3{1.f} - ts) * vec3Of(handrig::data::baseScaleOrigin);
    view::ViewEntity scratch;
    view::ViewEntity& v = ve ? *ve : scratch;
    place(v, model, pos + hands::redirect({e.x, mirrored ? e.y : -e.y, e.z}, handRot), {-handRot.x, handRot.y, handRot.z}, 0,
        mirrored);
    float m[16];
    render::entityMatrix(v.ent, mirrored, ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
    return toMat4(m);
}

namespace
{
// The hand at (`pos`, `handRot`) moved and turned, the least, so that its grip channel (grasp::gripChannel: where a
// handle lies in the curled fingers) is on the line through `on` along `axis`: turned about the channel's middle to
// lie along it (either way), then moved onto it (to its nearest point). A blade, a cupped hand: they are then in the
// fingers' closing reach, not beside them. `legacy`: the hand before "Hands remodelled"'s channel (migrating settings).
void alignChannel(int hand, bool mirrored, const glm::vec3& on, const glm::vec3& axis, glm::vec3& pos, glm::vec3& handRot,
    bool legacy = false, float halfLength = -1.f)
{
    glm::vec3 cp, cd;
    float radius;
    if(!handrig::usable(viewModel(handrig::modelName)))
    {
        return;
    }
    if(legacy)
    {
        grasp::legacyGripChannel(rigHands[hand].pose, cp, cd, radius);
    }
    else if(!grasp::gripChannel(rigHands[hand].pose, cp, cd, radius))
    {
        return;
    }
    const glm::mat4 m = rigPlacement(hand, pos, handRot, mirrored, nullptr);
    const glm::vec3 c{m * glm::vec4{cp, 1.f}};
    glm::vec3 d = glm::normalize(glm::mat3{m} * cd);
    const glm::vec3 a = glm::normalize(axis);
    if(glm::dot(d, a) < 0.f)
    {
        d = -d;
    }
    // The least turn taking d to a (they are within 90 degrees).
    const glm::vec3 x = glm::cross(d, a);
    const glm::mat3 r = glm::mat3_cast(glm::normalize(glm::quat{1.f + glm::dot(d, a), x.x, x.y, x.z}));
    handRot = basisAngles(r * anglesBasis(handRot));
    pos = c + r * (pos - c);
    // (`halfLength`: no further along the line from `on` than that, either way: a magazine's length.)
    const float along = glm::dot(c - on, a);
    const glm::vec3 target = on + a * (halfLength >= 0.f ? za::clamp(along, -halfLength, halfLength) : along);
    pos += target - c;
}

// Whether a finger at `curls` is clear of the world: the lines from its knuckle through its joints to its tip.
[[nodiscard]] bool fingerClear(const RigHand& rh, int finger, const glm::mat4& rigToWorld, const float curls[handrig::jointsPerFinger])
{
    glm::vec3 p[4];
    grasp::fingerPoints(rh.pose, finger, curls, p);
    for(int i = 0; i < 3; i++)
    {
        const glm::vec3 a{rigToWorld * glm::vec4{drawnInRig(rh, p[i]), 1.f}};
        const glm::vec3 b{rigToWorld * glm::vec4{drawnInRig(rh, p[i + 1]), 1.f}};
        if(worldtrace::line(a, b) < 1.f)
        {
            return false;
        }
    }
    return true;
}

// A finger going into the world's geometry curled (its joints alike) as little as keeps it out (six halvings: a
// sixty-fourth of the way to the fist); left as it is if it is in it even closed (its knuckle is: the hand is).
void bendOutOfWalls(const RigHand& rh, int finger, const glm::mat4& rigToWorld, float target[handrig::jointsPerFinger])
{
    QVR_PROFILE("hand walls");
    if(fingerClear(rh, finger, rigToWorld, target))
    {
        return;
    }
    float lo = 0.f, hi = 4.f;
    float trial[handrig::jointsPerFinger];
    const auto bent = [&](float by) {
        for(int j = 0; j < handrig::jointsPerFinger; j++)
        {
            trial[j] = za::fmin(target[j] + by, 4.f);
        }
    };
    bent(hi);
    if(!fingerClear(rh, finger, rigToWorld, trial))
    {
        return;
    }
    for(int h = 0; h < 6; h++)
    {
        const float mid = 0.5f * (lo + hi);
        bent(mid);
        (fingerClear(rh, finger, rigToWorld, trial) ? hi : lo) = mid;
    }
    bent(hi);
    za::copy(trial, trial + handrig::jointsPerFinger, target);
    if(vr_debug_grasp.value >= 3.f)
    {
        Con_Printf("finger %d bent %.2f out of the world\n", finger, hi);
    }
}

// Hand units from the palm's middle within which the other hand's weapon makes the fingers react (a finger's reach).
constexpr float brushReach = 24.f;

// The free `hand` (if `free`) against what the other hand holds: its weapon, or the prop it holds alone (vr_hand_collide,
// both the same way: NOTES.md vrfiringrange_2026-10-01_12-12-28, "the hand itself should collide ... with the same
// behavior that it's already there when holding a prop", and 17-15-34, "they could work in the same way"); `most` that
// setting in world units. Its palm's middle and knuckles (and the tips of the fingers stuck in it last frame) stop at
// the thing's own surface, vr_hand_collide_props_margin cm off it. As a prop in this hand and that thing meet (vr_held.cpp
// meetFrame), the hand and the thing are each drawn moved back by half how deep the hand would be, at once (not eased
// in, which let it sink in first), sliding along the surface, at most `most` each; pressed further, the hand sinks in by
// the rest (never let through all at once), held out by the face it went in by however deep, until it is out of it. A
// point in it is held out by the face it went in by (from where it was drawn last frame, in the thing's own frame, to
// where it is now), so a hand deeper than half the thing is not pushed out through the far side; a point just outside,
// off its nearest point. The winding is the model's own (grasp::signedDistance: Quake's models are wound inwards; read
// as outwards, the hand was pushed while still outside and out through the far side). The thing's part: weaponPressed
// (its hand, and a prop with it, moved by the model collision next frame); the hand is tested against the thing where it
// would be without it. A buzz in both hands as they meet. The fingers rest on it (setupRigHand). `part`: a part drawn
// on it as its own model, solid with it (a weapon's seated magazine: vr_reload_mag_collide), moving with it: each point
// held out of whichever of the two it is in (or nearer), its plane kept in the thing's frame all the same. `keepOut` (a
// prop held alone, vr_hand_collide_props_keep_out): the hand never sinks in; past the push it is drawn held out by the
// rest, resting on the surface however deep the real hand is (a point deeper than the search reaches keeps the face it
// went in by), and its grab there takes the prop at the surface (heldOutOf, vr_carry2h.cpp).
void pushAgainst(RigHand& rh, int hand, bool free, const entity_t& against, bool againstMirrored, float most,
    glm::vec3& pos, const glm::vec3& handRot, bool mirrored, float dt, const entity_t* part = nullptr, bool partMirrored = false,
    bool keepOut = false)
{
    const float u = 0.01f * units::metresToUnits();
    const float margin = za::clamp(vr_hand_collide_props_margin.value, 0.f, 10.f) * u;
    const grasp::Shape* shape =
        free && most > 0.f && handrig::usable(viewModel(handrig::modelName)) ? grasp::shapeOf(against, against.frame) : nullptr;
    if(rh.propAgainst != &against || !shape)
    {
        for(bool& b : rh.propDrawnValid) { b = false; }
        for(bool& b : rh.propTouching) { b = false; }
        for(bool& b : rh.propPlaneValid) { b = false; }
        rh.propAgainst = shape ? &against : nullptr;
    }
    glm::vec3 target{0.f};
    glm::vec3 press{0.f}; // the thing's part
    float deepest = 0.f; // how far the real hand is in (its point the furthest past its plane)
    glm::mat4 toWorld{1.f};
    // The points, each in its own place (RigHand::propDrawn): the palm's middle, the knuckles, the stuck tips.
    glm::vec3 points[RigHand::propPoints];
    bool tested[RigHand::propPoints]{};
    if(shape)
    {
        toWorld = grasp::shapeToWorld(against, againstMirrored);
        toWorld = glm::translate(glm::mat4{1.f}, -weaponPressed[1 - hand]) * toWorld; // where it is without the press
        const glm::mat4 toShape = glm::inverse(toWorld);
        const grasp::Shape* partShape = part ? grasp::shapeOf(*part, part->frame) : nullptr;
        const glm::mat4 partToWorld =
            partShape ? glm::translate(glm::mat4{1.f}, -weaponPressed[1 - hand]) * grasp::shapeToWorld(*part, partMirrored)
                      : glm::mat4{1.f};
        const glm::mat3 normalToWorld = glm::transpose(glm::inverse(glm::mat3{toWorld}));
        const glm::mat3 normalToShape = glm::transpose(glm::mat3{toWorld});
        const glm::mat4 rig = rigPlacement(hand, pos, handRot, mirrored, nullptr);
        const float unit = glm::length(glm::vec3{rig[0]});
        points[0] = glm::vec3{rig * glm::vec4{grasp::palmCentre(), 1.f}};
        tested[0] = true;
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            glm::vec3 p[4];
            grasp::fingerPoints(rh.pose, f, rh.pose.curl[f], p);
            points[1 + f] = glm::vec3{rig * glm::vec4{p[0], 1.f}};
            tested[1 + f] = true;
            points[1 + handrig::FingerCount + f] = glm::vec3{rig * glm::vec4{p[3], 1.f}};
            tested[1 + handrig::FingerCount + f] =
                rh.brushStuck[f] || rh.propTouching[1 + handrig::FingerCount + f] || !vr_hand_collide_fingers.value;
        }
        // (The hand may be in it deeper than the push, by the rest.)
        const float reach = 6.f * most + margin;
        // Each point's plane (n . x >= rest).
        glm::vec3 normals[RigHand::propPoints];
        float rests[RigHand::propPoints];
        glm::vec3 planePoints[RigHand::propPoints];
        int planes = 0;
        bool planeMade[RigHand::propPoints]{};
        for(bool& b : rh.propTouching) { b = false; }
        for(int i = 0; i < RigHand::propPoints; i++)
        {
            float d;
            glm::vec3 at, n;
            bool in = false;
            // In it: it went in by a face from where it was drawn last frame (held out, so outside); else (no way in
            // found: the first frame near it, passing through, or the way in missed between two triangles) by the
            // nearest face's side.
            const auto probe = [&](const grasp::Shape& sh, const glm::mat4& shToWorld, float& pd, glm::vec3& pat,
                                   glm::vec3& pn, bool& pin) {
                const bool within = grasp::signedDistance(sh, shToWorld, points[i], reach, pd, pat, pn);
                // (Kept out: a point gone in further than the search reaches since last frame, from where it was
                // drawn, outside, is in by the face the way in meets.)
                const bool farIn = !within && keepOut && rh.propDrawnValid[i] &&
                                   glm::distance(glm::vec3{toWorld * glm::vec4{rh.propDrawn[i], 1.f}}, points[i]) > reach;
                if(!within && !farIn)
                {
                    return false;
                }
                pin = within && pd < 0.f;
                if(!within)
                {
                    pd = -reach;
                }
                if(rh.propDrawnValid[i])
                {
                    const glm::vec3 from{toWorld * glm::vec4{rh.propDrawn[i], 1.f}};
                    glm::vec3 hit, hn;
                    bool entering = false;
                    if(glm::distance(from, points[i]) > 1e-4f && grasp::rayHit(sh, shToWorld, from, points[i], hit, hn, entering) &&
                        entering)
                    {
                        pin = true;
                        pat = hit;
                        pn = hn;
                    }
                }
                return within || pin;
            };
            if(!tested[i])
            {
                continue;
            }
            bool found = probe(*shape, toWorld, d, at, n, in);
            if(partShape)
            {
                // The part: taken where the point is in it and not in the thing, or nearer it (deeper in it).
                float pd;
                glm::vec3 pat, pn;
                bool pin = false;
                if(probe(*partShape, partToWorld, pd, pat, pn, pin) && (!found || (pin && !in) || (pin == in && pd < d)))
                {
                    found = true;
                    d = pd;
                    at = pat;
                    n = pn;
                    in = pin;
                }
            }
            if(!found && keepOut && rh.propPlaneValid[i])
            {
                // Kept out, held out last frame and now deeper than the search reaches: still by that face.
                const glm::vec3 keptAt{toWorld * glm::vec4{rh.propPlaneAt[i], 1.f}};
                const glm::vec3 keptN = glm::normalize(normalToWorld * rh.propPlaneN[i]);
                if(glm::dot(keptN, points[i] - keptAt) < margin)
                {
                    found = true;
                    in = true;
                    at = keptAt;
                    n = keptN;
                    d = glm::dot(keptN, points[i] - keptAt);
                }
            }
            if(!found)
            {
                continue;
            }
            if(in && rh.propPlaneValid[i])
            {
                // In it and still behind the plane it was held out by last frame: held out by that face however deep
                // (sunk in by the rest), not the nearest (past the middle of a barrel, the far side's) nor one a way in
                // from inside it meets (the model's other parts).
                const glm::vec3 keptAt{toWorld * glm::vec4{rh.propPlaneAt[i], 1.f}};
                const glm::vec3 keptN = glm::normalize(normalToWorld * rh.propPlaneN[i]);
                if(glm::dot(keptN, points[i] - keptAt) < margin)
                {
                    at = keptAt;
                    n = keptN;
                }
            }
            if(!in)
            {
                const float off = glm::length(points[i] - at);
                if(off >= margin)
                {
                    continue; // clear of it
                }
                if(off > 0.01f)
                {
                    n = (points[i] - at) / off; // just outside: off its nearest point (an edge's, a corner's)
                }
            }
            if(i > handrig::FingerCount && glm::dot(n, points[0] - at) < 0.f)
            {
                continue; // a fingertip past the far side (hooked round it): it bends, the hand is never pulled in
            }
            if(vr_debug_hand_collide.value >= 2.f)
            {
                Con_Printf("  point %d d %.2f n %.2f %.2f %.2f in %.2f%s\n", i, d / u, n.x, n.y, n.z,
                    (glm::dot(n, at) + margin - glm::dot(n, points[i])) / u, in ? " (in)" : "");
            }
            normals[planes] = n;
            rests[planes] = glm::dot(n, at) + margin;
            planePoints[planes] = points[i];
            rh.propTouching[i] = true;
            rh.propPlaneAt[i] = glm::vec3{toShape * glm::vec4{at, 1.f}};
            rh.propPlaneN[i] = glm::normalize(normalToShape * n);
            planeMade[i] = true;
            deepest = za::fmax(deepest, rests[planes] - glm::dot(n, points[i]));
            planes++;
        }
        // The least move of the real hand that takes every point out past its plane (a few passes: a corner's two faces).
        glm::vec3 move{0.f};
        for(int pass = 0; pass < 4; pass++)
        {
            for(int i = 0; i < planes; i++)
            {
                const float in = rests[i] - glm::dot(normals[i], planePoints[i] + move);
                if(in > 0.f)
                {
                    move += normals[i] * in;
                }
            }
        }
        for(int i = 0; i < RigHand::propPoints; i++)
        {
            rh.propPlaneValid[i] = planeMade[i];
        }
        // Each moved back by half, at most `most`; deeper, the hand in it by the rest. Kept out: the hand alone held out
        // by all of it, the prop not pressed back (a pressed prop moves the hand holding it, vr_modelcollide.cpp, and that
        // hand sprang back as the free hand took hold too, turning the two-handed hold off both grips).
        const float depth = glm::length(move);
        if(depth > 1e-4f)
        {
            const float each = za::fmin(depth * 0.5f, most);
            press = keepOut ? glm::vec3{0.f} : -move * (each / depth);
            target = keepOut ? move : -press;
        }
        // Where each point is drawn (held out, or where it is), in the thing's frame: next frame's way in (off the thing
        // as drawn, pressed back, as off it unpressed).
        for(int i = 0; i < RigHand::propPoints; i++)
        {
            rh.propDrawn[i] = glm::vec3{toShape * glm::vec4{points[i] + target - press, 1.f}};
            rh.propDrawnValid[i] = tested[i];
        }
        // Near it (within a hand's length of the palm's middle): the fingers react.
        glm::vec3 closest;
        bool in = false;
        if(vr_hand_collide_fingers.value &&
            grasp::surfaceDistance(*shape, toWorld, points[0], brushReach * unit, closest, in) >= 0.f)
        {
            rh.brushEnt = &against;
            rh.brushMirrored = againstMirrored;
        }
    }
    // Held back at once; let go of (drawn out of it) eased over 0.08 s, and a sudden drop (part of the hand through to
    // the far side) eased too.
    const float ease = za::fmin(1.f, dt / 0.08f);
    const bool holding = glm::length(target) > 0.f && !(glm::length(target) < glm::length(rh.pushed) - u);
    rh.pushed = holding ? target : rh.pushed + (target - rh.pushed) * ease;
    glm::vec3& w = weaponPressed[1 - hand];
    w = holding ? press : w + (press - w) * ease;
    if(holding && !rh.weaponTouching && !vr_disablehaptics.value)
    {
        if(Backend* be = backend())
        {
            be->haptic(0, 0.04f, 120.f, 0.35f); // (meetFrame's)
            be->haptic(1, 0.04f, 120.f, 0.35f);
        }
    }
    rh.weaponTouching = glm::length(target) > 0.f;
    if(vr_debug_hand_collide.value && shape)
    {
        // The real hand's and the drawn hand's nearest point to the surface (cm, negative in it): where it touched, and
        // how far the drawn hand is in (its palm and knuckles: the fingertips bend out of it, setupRigHand).
        // (And its part's, the seated magazine's: "mag".)
        const grasp::Shape* partShape = part ? grasp::shapeOf(*part, part->frame) : nullptr;
        const glm::mat4 partToWorld = partShape ? glm::translate(glm::mat4{1.f}, -weaponPressed[1 - hand]) *
                                                      grasp::shapeToWorld(*part, partMirrored)
                                                : glm::mat4{1.f};
        float real = 1e9f, drawn = 1e9f, partReal = 1e9f, partDrawn = 1e9f;
        for(int i = 0; i <= handrig::FingerCount; i++)
        {
            float d;
            glm::vec3 at, n;
            if(tested[i] && grasp::signedDistance(*shape, toWorld, points[i], 4.f * most + margin, d, at, n))
            {
                real = za::fmin(real, d);
            }
            const glm::vec3 apart = rh.pushed - weaponPressed[1 - hand];
            if(tested[i] && grasp::signedDistance(*shape, toWorld, points[i] + apart, 4.f * most + margin, d, at, n))
            {
                drawn = za::fmin(drawn, d);
            }
            if(partShape && tested[i] && grasp::signedDistance(*partShape, partToWorld, points[i], 4.f * most + margin, d, at, n))
            {
                partReal = za::fmin(partReal, d);
            }
            if(partShape && tested[i] &&
                grasp::signedDistance(*partShape, partToWorld, points[i] + apart, 4.f * most + margin, d, at, n))
            {
                partDrawn = za::fmin(partDrawn, d);
            }
        }
        if(real < 1e8f || partReal < 1e8f || glm::length(rh.pushed) > 0.01f)
        {
            Con_Printf("handcollide: hand %d real %.2f drawn %.2f depth %.2f target %.2f pushed %.2f weapon %.2f%s%s\n", hand,
                real / u, drawn / u, deepest / u, glm::length(target) / u, glm::length(rh.pushed) / u,
                glm::length(weaponPressed[1 - hand]) / u, held::heldAlone(1 - hand) ? " (prop)" : "",
                partShape ? va(" mag real %.2f drawn %.2f", partReal / u, partDrawn / u) : "");
        }
    }
    pos += rh.pushed;
}

// vr_hand_collide: the free `hand` (if `free`) held off the other hand's weapon or the prop it holds alone, the same way
// (pushAgainst). The fingers react to its shape (setupRigHand, the thing noted here while the hand is near it): they
// rest on it, or bend out of it, rather than the whole hand moving.
void pushOut(int hand, bool free, glm::vec3& pos, const glm::vec3& handRot, bool mirrored)
{
    QVR_PROFILE("hand collide");
    RigHand& rh = rigHands[hand];
    const double now = cl.time;
    const float dt = rh.pushedTime >= 0.0 ? static_cast<float>(CLAMP(0.0, now - rh.pushedTime, 0.1)) : 0.f;
    rh.pushedTime = now;
    rh.brushEnt = nullptr;
    // The other hand's weapon, or the prop it holds alone (as drawn there this frame: held.cpp placed it, moved with
    // that hand by what stops it). A prop held in both hands is theirs; the prop this hand holds is never tested (not
    // free).
    const float u = 0.01f * units::metresToUnits();
    const view::ViewEntity& other = entities.weapon[1 - hand];
    const int slot = weapons::slotForModel(other.ent.model);
    if(other.visible && slot >= 0 && slot != weapons::fistSlot())
    {
        // (Its seated magazine solid with it: vr_reload_mag_collide.)
        const view::ViewEntity& mag = entities.mag[1 - hand];
        const bool magSolid = vr_reload_mag_collide.value && mag.visible && mag.ent.model;
        pushAgainst(rh, hand, free, other.ent, other.mirrored, za::fmax(vr_hand_collide.value, 0.f) * u, pos, handRot,
            mirrored, dt, magSolid ? &mag.ent : nullptr, mag.mirrored);
        return;
    }
    if(const int prop = held::heldAlone(1 - hand))
    {
        // (Its hand pressed back as a weapon's is: the model collision moves that hand, and the prop with it.)
        pushAgainst(rh, hand, free, cl_entities[prop], false, za::fmax(vr_hand_collide.value, 0.f) * u, pos, handRot,
            mirrored, dt, nullptr, false, vr_hand_collide_props_keep_out.value != 0.f);
    }
    else
    {
        // (Nothing there: the press let go of, eased.)
        weaponPressed[1 - hand] -= weaponPressed[1 - hand] * za::fmin(1.f, dt / 0.08f);
        rh.weaponTouching = false;
        rh.propAgainst = nullptr;
        rh.pushed -= rh.pushed * za::fmin(1.f, dt / 0.08f); // let go of, eased
        pos += rh.pushed;
    }
}

// `motion`: the held weapon's firing animation, moving the drawn hand after its grasp (solved without it).
// Where the six models are drawn: each at the hand plus its offset (fingerOffset, at offsetScale), scaled by the fist
// slot's Scale about its own scale origin (weapons::modelTransform). The rig is hand_base.mdl's space: the entity goes
// where the palm's origin is drawn, each finger moves from its bind place (the defaults) by what its current offset
// changes: `pose`'s shifts.
void rigShifts(qmodel_t* model, int hand, handrig::Pose& pose)
{
    const weapons::ModelTransform t = weapons::modelTransform(model);
    const float k = t.active ? t.k : 1.f;
    const glm::vec3 ts = t.active ? t.scale : glm::vec3{1.f};
    const glm::vec3 baseOrigin = vec3Of(handrig::data::baseScaleOrigin);
    const glm::vec3 mBase = offsetInModel(fingerOffset(FingerBase, hand));
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        const glm::vec3 m = offsetInModel(fingerOffset(rigFinger[f], hand));
        const glm::vec3 origin = vec3Of(handrig::data::fingerScaleOrigin[f]);
        pose.shift[f] = ((m - mBase) / k + (glm::vec3{1.f} - ts) * (origin - baseOrigin)) / ts -
                        vec3Of(handrig::data::fingerBindShift[f]);
    }
}

// emptyHandSpheres' posed spheres, per hand, open and closed (the pose they were posed at, the rig's generation).
struct PosedSpheresMemo
{
    bool valid = false;
    unsigned generation = 0;
    handrig::Pose pose;
    za::Vector<glm::vec4> spheres;
};
struct PosedSpheresCache
{
    za::Array<PosedSpheresMemo, 4> memos; // [hand * 2 + fist] (released whole: not valid)
    auto members() { return mem::list(memos); }
};
mem::Cache<PosedSpheresCache> posedSpheresMemo{"view posed spheres", mem::GameDirChange | mem::ModelReload};
} // namespace

// The empty hand at the controller's pose (setupHand's: turned by the fist's angle offsets), open (`fist` false) or closed
// as a full press of the grip, trigger and thumb draws it (every finger curled), as the grasp's spheres, in the hand's
// frame: its place and the axes of its angles as the move sends them (held::setFist). False without the jointed hand.
bool emptyHandSpheres(const hands::State& s, int hand, bool fist, za::Vector<glm::vec4>& out)
{
    out.clear();
    qmodel_t* const model = viewModel(handrig::modelName);
    if(!handrig::usable(model))
    {
        return false;
    }
    const bool mirrored = hand == HAND_OFF;
    handrig::Pose pose;
    rigShifts(model, hand, pose);
    HandInput full;
    full.gripValue = full.triggerValue = 1.f;
    full.grip = full.trigger = full.thumbTouch = full.triggerTouch = true;
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        const float curl = fist ? grasp::pathCurl(CLAMP(0.f, targetCurl(full, rigFinger[f], false) * 5.f, 5.f)) : 0.f;
        for(float& j : pose.curl[f])
        {
            j = curl;
        }
    }
    const glm::vec3 handRot = basisAngles(anglesBasis(s.rot[hand]) * anglesBasis(weaponAngleOffsets(weapons::fistSlot(), mirrored)));
    const glm::mat4 rig = rigPlacement(hand, s.pos[hand], handRot, mirrored, nullptr);
    const float unit = glm::length(glm::vec3{rig[0]});
    const glm::mat3 toHand = glm::transpose(held::axesFromAngles(&s.rot[hand][0], true));
    // The posed spheres (rig space) are the pose's and the rig's alone: the open and the closed hand's kept from the last
    // frame while both are the same (the settings unchanged), not posed again four times a frame.
    PosedSpheresMemo& memo = posedSpheresMemo.memos[static_cast<za::SizeT>(hand * 2 + (fist ? 1 : 0))];
    if(!memo.valid || memo.generation != handrig::generation() || memcmp(&memo.pose, &pose, sizeof(pose)) != 0)
    {
        grasp::posedSpheres(pose, memo.spheres);
        memo.valid = true;
        memo.generation = handrig::generation();
        memo.pose = pose;
    }
    const za::Vector<glm::vec4>& spheres = memo.spheres;
    for(const glm::vec4& sp : spheres)
    {
        const glm::vec3 w{rig * glm::vec4{glm::vec3{sp}, 1.f}};
        out.pushBack(glm::vec4{toHand * (w - s.pos[hand]), sp.w * unit});
    }
    return true;
}

// Grab reach from the fist (held::setFist): the closed empty hand, every frame; none without the jointed hand model (the
// server's old test).
void updateFist(const hands::State& s, int hand)
{
    za::Vector<glm::vec4>& local = scratch.fistSpheres;
    emptyHandSpheres(s, hand, true, local);
    held::setFist(hand, local);
}

namespace
{
bool gripFrameCommandRegistered = false; // (vr_grip_frame, vr_gear_status, vr_body_wrist_info: registered on the first call)
void gearStatus_f();

// Held props' grips (grip::setHandFrame): where the empty hand's palm and grip channel are in its frame (the move's place
// and angles), every frame; not measured without the jointed hand (the default hand's are used).
void gripFrame_f();
} // namespace

void updateGripFrame(const hands::State& s, int hand)
{
    if(!gripFrameCommandRegistered) // a test command (no init hook here)
    {
        gripFrameCommandRegistered = true;
        Cmd_AddCommand("vr_grip_frame", gripFrame_f);
        Cmd_AddCommand("vr_gear_status", gearStatus_f);
        Cmd_AddCommand("vr_body_wrist_info", wristInfo_f);
        Cmd_AddCommand("vr_powerup_info", powerupInfo_f);
    }
    grip::HandFrame f;
    qmodel_t* const model = viewModel(handrig::modelName);
    glm::vec3 cp, cd;
    float radius = 0.f;
    handrig::Pose pose;
    if(s.valid && handrig::usable(model))
    {
        rigShifts(model, hand, pose);
        f.measured = grasp::gripChannel(pose, cp, cd, radius);
    }
    if(!f.measured)
    {
        gripfit::setHandRig(hand, nullptr, pose);
    }
    if(f.measured)
    {
        const bool mirrored = hand == HAND_OFF;
        const glm::vec3 handRot =
            basisAngles(anglesBasis(s.rot[hand]) * anglesBasis(weaponAngleOffsets(weapons::fistSlot(), mirrored)));
        const glm::mat4 rig = rigPlacement(hand, s.pos[hand], handRot, mirrored, nullptr);
        const glm::mat3 toHand = glm::transpose(held::axesFromAngles(&s.rot[hand][0], true));
        const auto point = [&](const glm::vec3& p) { return toHand * (glm::vec3{rig * glm::vec4{p, 1.f}} - s.pos[hand]); };
        const auto dir = [&](const glm::vec3& d) { return glm::normalize(toHand * glm::vec3{rig * glm::vec4{d, 0.f}}); };
        f.palm = point(grasp::palmCentre());
        f.palmNormal = dir({0.f, 1.f, 0.f}); // the rig's palm faces +y (grasp::solve)
        f.channelPoint = point(cp);
        f.channelDir = dir(cd);
        f.channelRadius = radius * glm::length(glm::vec3{rig[0]});
        // The rig to the hand's frame, kept for the grip fit at a grab (vr_gripfit.hpp: the drawn hand posed there).
        glm::mat4 fromPos{1.f};
        fromPos[3] = glm::vec4{-s.pos[hand], 1.f};
        const glm::mat4 rigToHand = glm::mat4{toHand} * fromPos * rig;
        gripfit::setHandRig(hand, &rigToHand, pose);
        // The palm's middle is inside the hand: out along its normal to its skin, where the open hand's spheres (the
        // grasp's) the normal passes through end, so that what rests on it rests on the skin, not in the palm.
        za::Vector<glm::vec4>& open = scratch.openSpheres;
        if(emptyHandSpheres(s, hand, false, open))
        {
            float skin = 0.f;
            for(const glm::vec4& sp : open)
            {
                const glm::vec3 d = glm::vec3{sp} - f.palm;
                const float along = glm::dot(d, f.palmNormal);
                const float across2 = glm::dot(d, d) - along * along;
                if(across2 < sp.w * sp.w)
                {
                    skin = za::max(skin, along + za::sqrt(sp.w * sp.w - across2));
                }
            }
            f.palm += f.palmNormal * skin;
        }
    }
    grip::setHandFrame(hand, f);
}

namespace
{
// vr_gear_status: what of the gear the view drew last (a test of vr_dead_hide_gear: dead, all hidden but the status bar;
// Debug > Reports > Gear). The worn gear: the holstered guns (with their magazines, wells, open super shotguns' parts and
// buttons), the holster sleeves, the ammo pouch and its grenades, the grenade pouch at the back, the flashlight and its
// cord, the wrist gadget and its straps, the body and its pauldrons.
void gearStatus_f()
{
    int guns = 0, sleeves = 0, gunParts = 0;
    for(int h = 0; h < HolsterCount; h++)
    {
        guns += entities.holster[h].visible ? 1 : 0;
        sleeves += entities.holsterSlot[h].visible ? 1 : 0;
        for(const view::ViewEntity* e : {&entities.holsterMag[h], &entities.holsterWell[h], &entities.holsterSsgFrame[h],
                &entities.holsterSsgBarrels[h], &entities.holsterButton[h], &entities.holsterFrontButton[h]})
        {
            gunParts += e->visible ? 1 : 0;
        }
    }
    int pouchGrenades = 0, straps = 0, pauldrons = 0;
    for(const view::ViewEntity& e : entities.ammoPouchGrenade)
    {
        pouchGrenades += e.visible ? 1 : 0;
    }
    for(int side = 0; side < 2; side++)
    {
        straps += entities.gadgetStrap[side].visible ? 1 : 0;
        pauldrons += (entities.pauldron[side].visible ? 1 : 0) + (entities.pauldronArm[side].visible ? 1 : 0);
    }
    Con_Printf("gear: health %d, hidden for death %d; holstered guns %d (their parts %d), holster sleeves %d, ammo pouch %d "
               "(grenades %d), grenade pouch %d, flashlight %d (cord %d), wrist gadget %d (drawn %d, straps %d), body %d "
               "(pauldrons %d), status bar on a hand %d\n",
        cl.stats[STAT_HEALTH], body::gearHiddenForDeath() ? 1 : 0, guns, gunParts, sleeves, entities.ammoPouch.visible ? 1 : 0,
        pouchGrenades, entities.pouch.visible ? 1 : 0, entities.flashlight.visible ? 1 : 0, flashlight::cordDrawn() ? 1 : 0,
        gadget::active() ? 1 : 0, entities.gadget.visible ? 1 : 0, straps, entities.body.visible ? 1 : 0, pauldrons,
        panel::statusBarOnHand() ? 1 : 0);
    // Each pair's guns drawn, of those it holds (vr_holster_weapons_draw_*).
    const auto pair = [](int left, int bodyLeft, char* out, int size) {
        const int drawn = (entities.holster[left].visible ? 1 : 0) + (entities.holster[left + 1].visible ? 1 : 0);
        int held = 0;
        for(int stat = bodyLeft; stat < bodyLeft + 2; stat++)
        {
            const qmodel_t* model = precachedModel(cl.stats[STAT_QVR_HOLSTERWEAPONMODEL0 + stat]);
            held += model && !isHandModel(model) ? 1 : 0;
        }
        q_snprintf(out, size, "%d of %d", drawn, held);
    };
    char hips[16], chest[16], back[16];
    pair(LeftHip, body::LeftHip, hips, sizeof(hips));
    pair(LeftUpper, body::LeftUpper, chest, sizeof(chest));
    pair(LeftShoulder, body::LeftShoulder, back, sizeof(back));
    Con_Printf("gear: holstered guns drawn: hips %s, chest %s, shoulders %s (vr_holster_weapons_draw_hip %g, _upper %g, "
               "_shoulder %g)\n",
        hips, chest, back, vr_holster_weapons_draw_hip.value, vr_holster_weapons_draw_upper.value,
        vr_holster_weapons_draw_shoulder.value);
    // Each holster's gun as the view reads its stats (holsterStat): its model, clip and flags, and what of it is drawn
    // (the gun, its magazine, an open super shotgun's parts).
    constexpr const char* names[HolsterCount] = {"lhip", "rhip", "lupper", "rupper", "lshoulder", "rshoulder"};
    for(int h = 0; h < HolsterCount; h++)
    {
        const qmodel_t* model = precachedModel(holsterStat(h, STAT_QVR_HOLSTERWEAPONMODEL0));
        Con_Printf("gear: holster %s: %s clip %d flags %d; drawn %d, magazine %d, open parts %d\n", names[h],
            model ? model->name : "-", holsterStat(h, STAT_QVR_HOLSTERWEAPONCLIP0),
            holsterStat(h, STAT_QVR_HOLSTERWEAPONFLAGS0), entities.holster[h].visible ? 1 : 0,
            entities.holsterMag[h].visible ? 1 : 0, entities.holsterSsgFrame[h].visible ? 1 : 0);
    }
}

// vr_grip_frame: the hands' grip frames (grip::HandFrame), for the default hand's (vr_grip.cpp defaultFrame).
void gripFrame_f()
{
    for(int hand = 1; hand >= 0; hand--)
    {
        const grip::HandFrame f = grip::handFrame(hand, hand == HAND_OFF, true);
        Con_Printf("grip frame %s%s: palm %.2f %.2f %.2f normal %.2f %.2f %.2f channel %.2f %.2f %.2f dir %.2f %.2f %.2f radius %.2f\n",
            hand == HAND_MAIN ? "main" : "off", f.measured ? "" : " (not measured: the default)", f.palm.x, f.palm.y, f.palm.z,
            f.palmNormal.x, f.palmNormal.y, f.palmNormal.z, f.channelPoint.x, f.channelPoint.y, f.channelPoint.z, f.channelDir.x,
            f.channelDir.y, f.channelDir.z, f.channelRadius);
    }
}

FILE* graspTrace = nullptr; // vr_debug_grasp_trace's file (setupRigHand)

void noteIndexTip(int hand, const RigHand& rh)
{
    const hands::State& s = hands::current();
    if(!rh.drawn || !s.valid)
    {
        return;
    }
    glm::vec3 p[4];
    grasp::fingerPoints(rh.pose, handrig::Index, rh.pose.curl[handrig::Index], p);
    const glm::vec3 tip{rh.rigToWorld * glm::vec4{drawnInRig(rh, p[3]), 1.f}};
    glm::vec3 fwd, right, up;
    hands::angleVectors(s.rot[hand], fwd, right, up);
    const glm::vec3 d = tip - s.pos[hand];
    indexTips[hand] = {{glm::dot(d, fwd), glm::dot(d, right), glm::dot(d, up)}, host_framecount};
}

bool setupRigHand(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored, bool hide, const Held& held,
    const glm::mat4& motion)
{
    QVR_PROFILE("rig hand");
    RigHand& rh = rigHands[hand];
    rh.drawn = false;
    if(!vr_hand_rig.value)
    {
        return false;
    }
    qmodel_t* const model = viewModel(handrig::modelName);
    if(!handrig::usable(model))
    {
        return false;
    }

    const weapons::ModelTransform t = weapons::modelTransform(model);
    const float k = t.active ? t.k : 1.f;
    const glm::vec3 ts = t.active ? t.scale : glm::vec3{1.f};
    rigShifts(model, hand, rh.pose);
    rh.pose.metacarpal = glm::quat{1.f, 0.f, 0.f, 0.f};

    view::ViewEntity& ve = entities.hand[hand][FingerBase];
    const glm::mat4 rigToWorld = rigPlacement(hand, pos, handRot, mirrored, &ve);

    // The fingers: the controller's curls (as the six models'), or wrapping what the hand holds, or set by hand (the
    // weapon's or the hotspot's Fingers: Manual: no solve).
    if(held.manual)
    {
        rh.grasp.valid = false;
        rh.inRig = held.ent ? glm::inverse(rigToWorld) * heldToWorld(held) : glm::mat4{1.f};
    }
    else
    {
        updateGrasp(hand, held, rigToWorld, k * ts.x);
    }
    rh.held = held;
    rh.rigToWorld = rigToWorld;

    // A free hand brushing the other hand's weapon (pushOut noted it): its fingers closed onto it from open (the grasp's
    // solve, the palm where it is), to rest on it or bend out of it (below).
    const bool brushing = rh.brushEnt && !held.ent && !held.manual;
    if(brushing)
    {
        QVR_PROFILE("hand brush");
        if(const grasp::Shape* shape = grasp::shapeOf(*rh.brushEnt, rh.brushEnt->frame))
        {
            // Solved again only once the hand has moved on it by Refit Threshold (vr_hand_fit_resolve), as a held thing's
            // grasp: steady, and cheap while the hand rests against it.
            const glm::mat4 inRig = glm::inverse(rigToWorld) * grasp::shapeToWorld(*rh.brushEnt, rh.brushMirrored);
            Grasp& last = rh.brushSolve;
            if(!rh.brushValid || !sameGrasp(last, rh.brushEnt->model, rh.brushEnt->frame, inRig, rh.pose, k * ts.x))
            {
                grasp::Settings settings;
                grasp::solve(rh.pose, *shape, inRig, settings, rh.brushValid ? &rh.brush : nullptr, last.solution);
                rh.brush = last.solution;
                rh.brushValid = true;
                last.valid = true;
                last.model = rh.brushEnt->model;
                last.frame = rh.brushEnt->frame;
                last.inRig = inRig;
                for(int f = 0; f < handrig::FingerCount; f++)
                {
                    last.shift[f] = rh.pose.shift[f];
                }
                brushSolves[hand]++;
            }
            const grasp::Solution& solution = rh.brush;
            if(vr_debug_grasp.value)
            {
                Con_Printf("brush (%.2f s): %s hand on %s: solve %d, %.1f us, %d triangles; fingers", vr_gametime,
                    hand == HAND_MAIN ? "main" : "off", rh.brushEnt->model ? rh.brushEnt->model->name : "-", brushSolves[hand],
                    solution.seconds * 1e6, solution.triangles);
                for(const grasp::FingerStop& st : solution.finger)
                {
                    Con_Printf(" %s%.1f/%.1f/%.1f", st.startsInside ? "in " : st.fromClosed ? "out " : st.met ? "on " : "- ",
                        st.stop[0], st.stop[1], st.stop[2]);
                }
                Con_Printf("\n");
            }
        }
        else
        {
            rh.brushValid = false;
        }
    }
    else
    {
        rh.brushValid = false;
    }
    for(bool& stuck : rh.brushStuck)
    {
        stuck = false;
    }
    const double now = cl.time;
    const float dt = rh.jointsTime >= 0.0 ? static_cast<float>(CLAMP(0.0, now - rh.jointsTime, 0.1)) : 1.f;
    rh.jointsTime = now;

    // The palm flush on what it holds: the drawn hand eased there (and back) over about a tenth of a second.
    // The palm turned to face the surface it holds (round its middle) and moved flush on it: the rig drawn as
    //   rigToWorld * T(centre + move) * turn * T(-centre),
    // as an entity: its angles R' = R * turn (mirrored as the hand is), its origin to match.
    // Every change of the grasp (a new solve, the controller's curls) is eased in over vr_hand_fit_blend seconds
    // (95% of the way: an exponential approach, a third of it the time constant): no jumps between poses.
    const float blend = za::fmax(vr_hand_fit_blend.value, 0.f);
    const float follow = blend > 0.f ? 1.f - za::exp(-dt * 3.f / blend) : 1.f;
    const glm::vec3 palmTarget = rh.grasp.valid ? rh.grasp.solution.palm : glm::vec3{0.f};
    const glm::quat turnTarget = rh.grasp.valid ? rh.grasp.solution.palmTurn : glm::quat{1.f, 0.f, 0.f, 0.f};
    rh.palm += (palmTarget - rh.palm) * follow;
    rh.turn = glm::normalize(glm::slerp(rh.turn, turnTarget, follow));
    if(glm::length(rh.palm) > 1e-4f || glm::angle(rh.turn) > 1e-4f)
    {
        const glm::mat3 q = glm::mat3_cast(rh.turn);
        const glm::vec3 c = grasp::palmCentre();
        const glm::mat3 l = glm::mat3{rigToWorld} / ts.x; // R * [mirror] * k
        const glm::vec3 offset = t.active ? t.offset : glm::vec3{0.f};
        const glm::vec3 o{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
        const glm::vec3 moved = o + l * offset + l * (ts.x * (c + rh.palm - q * c)) - l * (q * offset);
        glm::vec3 f, r, u;
        hands::angleVectors(handRot, f, r, u);
        const glm::mat3 mirror{1.f, 0.f, 0.f, 0.f, mirrored ? -1.f : 1.f, 0.f, 0.f, 0.f, 1.f};
        const glm::mat3 turned = glm::mat3{f, -r, u} * mirror * q * mirror;
        const glm::vec3 a = hands::anglesFromVectors(glm::normalize(turned[0]), glm::normalize(turned[2]));
        for(int i = 0; i < 3; i++)
        {
            ve.ent.origin[i] = moved[i];
        }
        ve.ent.angles[0] = -a.x;
        ve.ent.angles[1] = a.y;
        ve.ent.angles[2] = a.z;
    }
    for(int f = 0; f < handrig::FingerCount; f++)
    {
        const float curl = fingerFrames[hand][rigFinger[f]];
        const float bias = fingerBias[hand][rigFinger[f]];
        float target[handrig::jointsPerFinger];
        // Set by hand: the finger stops at its curl, as a solved finger stops on what it holds.
        grasp::FingerStop manualStop;
        if(held.manual)
        {
            manualStop.met = true;
            for(float& j : manualStop.stop)
            {
                j = 4.f * held.manualCurl[f];
            }
        }
        const grasp::FingerStop* stop = held.manual ? &manualStop : rh.grasp.valid ? &rh.grasp.solution.finger[f] : nullptr;
        if(stop)
        {
            grasp::curls(*stop, curl, engagement(curl), target);
            if(f == handrig::Index && held.trigger)
            {
                // The trigger finger pulls as the trigger is pulled, past where it met the weapon (the trigger
                // gives): from its stop (or the controller's curl, if less) towards a trigger pull's hook.
                const float pull = CLAMP(0.f, curl / 5.f, 1.f);
                for(int j = 0; j < handrig::jointsPerFinger; j++)
                {
                    target[j] = za::fmax(target[j], za::fmin(stop->stop[j], curl) + pull * triggerPull[j]);
                }
            }
            for(float& t : target)
            {
                t = CLAMP(0.f, t + 4.f * bias, 4.f); // the weapon's finger tweak, on top of the wrap
            }
        }
        else
        {
            for(float& t : target)
            {
                t = rigCurl(hand, rigFinger[f]);
            }
            // Brushing the other hand's weapon: a finger closing onto it stops on its surface (it rests there, as far as
            // the controller closes it); one in it open bends out of it, closing to where it is clear; one in it at every
            // curl is left, and pushes the hand out instead (pushOut). Eased in and out as any change of the fingers.
            if(brushing && rh.brushValid)
            {
                const grasp::FingerStop& st = rh.brush.finger[f];
                rh.brushStuck[f] = st.startsInside;
                if(st.startsInside && rh.brushHeld[f])
                {
                    // In it at every curl (the hand gone deeper): kept as it was last (not back to the controller's
                    // curl, into the weapon), while the hand is pushed out by its tip.
                    for(int j = 0; j < handrig::jointsPerFinger; j++)
                    {
                        target[j] = rh.brushHold[f][j];
                    }
                }
                else if(!st.startsInside && (st.met || st.fromClosed))
                {
                    for(int j = 0; j < handrig::jointsPerFinger; j++)
                    {
                        target[j] = st.fromClosed ? st.stop[j] : za::fmin(grasp::pathCurl(target[j]), st.stop[j]);
                    }
                }
                if(!st.startsInside)
                {
                    za::copy(target, target + handrig::jointsPerFinger, rh.brushHold[f]);
                    rh.brushHeld[f] = true;
                }
            }
            else
            {
                rh.brushHeld[f] = false;
            }
        }
        // Into a wall (vr_hand_walls): bent (curled, all its joints alike) as little as keeps it out: the lines from
        // the knuckle through its joints to its tip clear of the world's geometry.
        if(vr_hand_walls.value)
        {
            bendOutOfWalls(rh, f, rigToWorld, target);
        }
        for(int j = 0; j < handrig::jointsPerFinger; j++)
        {
            float& c = rh.joints[f][j];
            c += (target[j] - c) * follow;
            rh.pose.curl[f][j] = c;
        }
    }
    // The thumb turns across the palm (opposition) as the grasp found, as far as it closes.
    const float w = CLAMP(0.f, fingerFrames[hand][FingerThumb] / 3.f, 1.f);
    // Set by hand: across the palm by its share of 45 degrees (the solver's widest opposition), about the same axis.
    const glm::quat manualThumb = glm::angleAxis(glm::radians(45.f * held.manualThumbAcross), glm::vec3{-1.f, 0.f, 0.f});
    const bool brushThumb = brushing && rh.brushValid && !rh.brush.finger[handrig::Thumb].startsInside &&
                            (rh.brush.finger[handrig::Thumb].met || rh.brush.finger[handrig::Thumb].fromClosed);
    const glm::quat thumbTarget = held.manual   ? glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, manualThumb, w)
                                  : rh.grasp.valid ? glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, rh.grasp.solution.thumbTurn, w)
                                  : brushThumb     ? rh.brush.thumbTurn
                                                   : glm::quat{1.f, 0.f, 0.f, 0.f};
    rh.thumb = glm::normalize(glm::slerp(rh.thumb, thumbTarget, follow));
    rh.pose.metacarpal = rh.thumb;
    handrig::pose(rh.pose, rh.posed);
    handrig::skin(rh.posed, rh.skin.data());

    // The weapon's firing animation moves the hand (and so the arm) with it.
    if(motion != glm::mat4{1.f})
    {
        glm::vec3 o{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
        glm::vec3 a{-ve.ent.angles[0], ve.ent.angles[1], ve.ent.angles[2]};
        movePose(motion, o, a);
        for(int i = 0; i < 3; i++)
        {
            ve.ent.origin[i] = o[i];
        }
        ve.ent.angles[0] = -a.x;
        ve.ent.angles[1] = a.y;
        ve.ent.angles[2] = a.z;
        rh.rigToWorld = motion * rh.rigToWorld;
    }
    if(vr_debug_hand_bones.value)
    {
        float m[16];
        render::entityMatrix(ve.ent, mirrored, ENTSCALE_DEFAULT, glm::vec3{0.f}, m);
        rh.drawnToWorld = toMat4(m);
    }
    FILE*& trace = graspTrace;
    if(!vr_debug_grasp_trace.value && trace)
    {
        fclose(trace);
        trace = nullptr;
    }
    if(static_cast<int>(vr_debug_grasp_trace.value) & (hand == HAND_MAIN ? 1 : 2))
    {
        // One line a frame: the time, the solves so far, whether a grasp holds, the 15 joints, the thumb's
        // metacarpal turn (degrees), the palm's move (rig units) and turn (degrees), and where the hand is.
        za::String line = va("gt %d %.4f %d %d", hand, cl.time, graspSolves[hand], rh.grasp.valid ? 1 : 0);
        for(int f = 0; f < handrig::FingerCount; f++)
        {
            for(int j = 0; j < handrig::jointsPerFinger; j++)
            {
                line += va(" %.3f", rh.pose.curl[f][j]);
            }
        }
        line += va(" %.2f %.3f %.3f %.3f %.2f %.2f %.2f %.2f", glm::degrees(glm::angle(rh.pose.metacarpal)), rh.palm.x,
            rh.palm.y, rh.palm.z, glm::degrees(glm::angle(rh.turn)), ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]);
        // What it holds, relative to the grasp's: turned (degrees; the largest of its axes') and moved (hand units).
        float turned = 0.f, moved = 0.f;
        if(rh.grasp.valid)
        {
            moved = glm::distance(glm::vec3{rh.inRig[3]}, glm::vec3{rh.grasp.inRig[3]});
            for(int c = 0; c < 3; c++)
            {
                const glm::vec3 a = glm::normalize(glm::vec3{rh.inRig[c]}), b = glm::normalize(glm::vec3{rh.grasp.inRig[c]});
                turned = za::fmax(turned, glm::degrees(za::acos(CLAMP(-1.f, glm::dot(a, b), 1.f))));
            }
        }
        // The firing animation's motion at the hand: moved (world units), turned (degrees).
        const glm::vec3 at{ve.ent.origin[0], ve.ent.origin[1], ve.ent.origin[2]};
        line += va(" %.3f %.4f %d %.3f %.2f %d %.3f\n", turned, moved, twohand::helping(hand) ? 1 : 0,
            glm::distance(glm::vec3{motion * glm::vec4{at, 1.f}}, at), glm::degrees(glm::angle(glm::quat_cast(glm::mat3{motion}))),
            entities.weapon[hand].ent.frame, glm::length(rh.pushed));
        if(!trace)
        {
            trace = fopen(va("%s/grasp_trace.txt", com_gamedir), "w");
        }
        if(trace)
        {
            fputs(line.cStr(), trace);
            fflush(trace);
        }
    }
    ve.zeroBlend = 0.f;
    // wounds painted instead (vr_wounds.cpp); the wound skins washed and a gib's blood (wounds::skinLevel)
    ve.ent.skinnum = wounds::replacesSkins() ? 0 : wounds::skinLevel(hand, damageLevel(), ve.ent.origin);
    ve.visible = !hide;
    for(int finger = FingerBase + 1; finger < FingerCount; finger++)
    {
        entities.hand[hand][finger].visible = false;
    }
    rh.drawn = ve.visible;
    noteIndexTip(hand, rh);
    return true;
}
} // namespace

// The middle of the palm of the jointed hand drawn at (`pos`, `handRot`), before the palm's fit; `pos` if the jointed
// hand isn't there.
[[nodiscard]] glm::vec3 palmAt(int hand, const glm::vec3& pos, const glm::vec3& handRot, bool mirrored)
{
    if(!handrig::usable(viewModel(handrig::modelName)))
    {
        return pos;
    }
    return glm::vec3{rigPlacement(hand, pos, handRot, mirrored, nullptr) * glm::vec4{grasp::palmCentre(), 1.f}};
}

// Round 21, third pass: a drawn hand (at `pos`, `handRot`) moved by `p` and turned by `a` (pitch up, yaw left, roll;
// degrees) in the frame of the angles `frameRot` (x forward, y left, z up), turned about its palm's middle. The values
// are as for the main hand's frame; `mirror`: as the off hand's (y, yaw and roll the other way).
void moveDrawnHand(int hand, bool mirrored, const glm::vec3& frameRot, glm::vec3 p, glm::vec3 a, bool mirror, glm::vec3& pos,
    glm::vec3& handRot)
{
    if(p == glm::vec3{0.f} && a == glm::vec3{0.f})
    {
        return;
    }
    if(mirror)
    {
        p.y = -p.y;
        a.y = -a.y;
        a.z = -a.z;
    }
    const glm::mat3 frame = anglesBasis(frameRot);
    const glm::vec3 pivot = palmAt(hand, pos, handRot, mirrored);
    const glm::mat3 turn = frame * anglesBasis({-a.x, a.y, a.z}) * glm::transpose(frame);
    handRot = basisAngles(turn * anglesBasis(handRot));
    pos = pivot + turn * (pos - pivot) + frame * p;
}

namespace
{
// Round 21, third pass: where a weapon is in the rig of the hand holding it (`hand`: its own), or of the hand holding it
// by the hotspot `spot` (the other), as the settings put them, worked out with the holding hand at the origin,
// unturned (Held::canonical). The same steps as the drawn hands', at that pose.
[[nodiscard]] glm::mat4 canonicalOwnGrip(int hand, int slot, qmodel_t* model, bool offset = true);
[[nodiscard]] glm::mat4 canonicalHotspotGrip(int hand, int otherSlot, qmodel_t* model, const weapons::Hotspot& spot,
    bool offset = true);
} // namespace

// The turn of a helping hand on the weapon in the other hand (`other`), from a hotspot's angles (as for the off hand
// helping): the weapon's fixed-hand angles for a grip; a cup, the holding hand's own turn. Carried rigidly by the
// weapon (attachedTurn).
[[nodiscard]] glm::vec3 helpingTurn(const glm::vec3& holderRot, int holder, int hand, int otherSlot, bool cup,
    const glm::vec3& spotAngles)
{
    const int other = 1 - hand;
    const bool mirrored = hand == HAND_OFF;
    glm::vec3 offsets = cup ? glm::vec3{0.f}
                            : weapons::vec(otherSlot, Key::TwoHFixedHandPitch, Key::TwoHFixedHandYaw, Key::TwoHFixedHandRoll);
    offsets += spotAngles;
    if(!mirrored)
    {
        offsets.y = -offsets.y;
        offsets.z = -offsets.z;
    }
    if(cup)
    {
        offsets += weaponAngleOffsets(weapons::fistSlot(), mirrored); // the hand's own, as the holding hand's
    }
    return attachedTurn(holderRot, otherSlot, other == HAND_OFF, offsets, holder);
}

namespace
{
glm::mat4 canonicalOwnGrip(int hand, int slot, qmodel_t* model, bool offset)
{
    const bool mirrored = hand == HAND_OFF;
    const glm::vec3 zero{0.f};
    view::ViewEntity weapon;
    const glm::vec3 wt = weaponTurn(zero, slot, mirrored);
    place(weapon, model, zero, {-wt.x, wt.y, wt.z}, 0, mirrored);
    glm::vec3 pos{0.f};
    glm::vec3 handRot = attachedTurn(zero, slot, mirrored, weaponAngleOffsets(weapons::fistSlot(), mirrored));
    if(offset)
    {
        moveDrawnHand(hand, mirrored, attachedTurn(zero, slot, mirrored, zero), weapons::vec(slot, Key::HandOnlyX, Key::HandOnlyY, Key::HandOnlyZ),
            weapons::vec(slot, Key::HandOnlyPitch, Key::HandOnlyYaw, Key::HandOnlyRoll), mirrored, pos, handRot);
    }
    return glm::inverse(rigPlacement(hand, pos, handRot, mirrored, nullptr)) * grasp::shapeToWorld(weapon.ent, mirrored);
}

glm::mat4 canonicalHotspotGrip(int hand, int otherSlot, qmodel_t* model, const weapons::Hotspot& spot, bool offset)
{
    const int other = 1 - hand;
    const bool mirrored = hand == HAND_OFF, otherMirrored = other == HAND_OFF;
    const glm::vec3 zero{0.f};
    view::ViewEntity weapon;
    const glm::vec3 wt = weaponTurn(zero, otherSlot, otherMirrored);
    place(weapon, model, zero, {-wt.x, wt.y, wt.z}, 0, otherMirrored);
    const bool cup = spot.type == weapons::HotspotType::Cup;
    glm::vec3 handRot = helpingTurn(zero, -1, hand, otherSlot, cup, spot.angles);
    const glm::vec3 point{hotspotFrame(weapon.ent, otherMirrored) * glm::vec4{spot.pos, 1.f}};
    glm::vec3 pos = cup ? point - palmAt(hand, zero, handRot, mirrored) : point;
    if(offset)
    {
        moveDrawnHand(hand, mirrored, attachedTurn(zero, otherSlot, otherMirrored, zero), spot.visualPos, spot.visualAngles,
            otherMirrored, pos, handRot);
    }
    return glm::inverse(rigPlacement(hand, pos, handRot, mirrored, nullptr)) * grasp::shapeToWorld(weapon.ent, otherMirrored);
}

// ----------------------------------------------------------------------------
// A weapon lying about taken by a hotspot (vr_weapon_grab_hotspots, NOTES.md vrfiringrange_2026-10-01_11-30-28): an
// empty hand gripping a weapon on the floor nearer one of its grip or blade hotspots than its handle (a chainsaw by its
// front handle, a crowbar or a sword by its bar or blade) carries it by that hotspot, as the hand-off leaves a weapon
// (vr_twohand.cpp): hanging from the hand as if the other hand had held its handle and let go, until a hand takes its
// handle. Not a cup (the two-handed pistol grip's: the hand round the other's), and not a force grab's catch (QC: the
// handle, to use it).

constexpr float groundSpotReachCm = 15.f; // how far from the hand a hotspot may be, nearer than the handle, to be taken
// A hotspot this near the handle is the handle (a sword's or a crowbar's second hand on its grip): taken by the handle.
constexpr float groundSpotFromHandleCm = 10.f;
} // namespace

// Where hotspot `h` of a weapon of `slot` lying as `e` is: a grip's point; a blade's zone (from `a` to `b`, shares of the
// way from its handle to its tip; `a` == `b` for a grip). False for none or a cup.
[[nodiscard]] bool groundHotspotZone(const entity_t& e, int slot, const weapons::Hotspot& h, glm::vec3& a, glm::vec3& b)
{
    if(h.type == weapons::HotspotType::Grip)
    {
        a = b = glm::vec3{hotspotFrame(e, false) * glm::vec4{h.pos, 1.f}};
        return true;
    }
    if(h.type != weapons::HotspotType::Blade)
    {
        return false; // none, or a cup (the two-handed pistol grip's)
    }
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    const glm::vec3 tip = view::anchorPosition(e, false, 0.f, weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    a = origin + (tip - origin) * weapons::bladeFrom(h);
    b = origin + (tip - origin) * weapons::bladeTo(h);
    return true;
}

namespace
{
// The poses of a weapon of `slot` (lying as `e`) carried by hotspot `h` in `hand`, in the frame of the other hand holding
// its handle at the origin, unturned (as canonicalHotspotGrip): the hand as drawn on the hotspot (`drawnPos`, `drawnRot`).
// A grip's are the helping hand's on it; a blade's, the hand as it lies on the weapon now (`handPos`, `handRot`: drawn),
// turned onto the blade and slid along it into the blade grip's zone, as the half-sword grip takes it.
[[nodiscard]] bool groundSpotPose(const entity_t& e, int slot, const weapons::Hotspot& h, int hand, const glm::vec3& handPos,
    const glm::vec3& handRot, glm::vec3& drawnPos, glm::vec3& drawnRot)
{
    const int other = 1 - hand;
    const bool mirrored = hand == HAND_OFF, otherMirrored = other == HAND_OFF;
    const glm::vec3 zero{0.f};
    view::ViewEntity weapon;
    const glm::vec3 wt = weaponTurn(zero, slot, otherMirrored);
    place(weapon, e.model, zero, {-wt.x, wt.y, wt.z}, 0, otherMirrored);
    if(weapons::isGripType(h.type))
    {
        drawnRot = helpingTurn(zero, -1, hand, slot, false, h.angles);
        drawnPos = glm::vec3{hotspotFrame(weapon.ent, otherMirrored) * glm::vec4{h.pos, 1.f}};
        moveDrawnHand(hand, mirrored, attachedTurn(zero, slot, otherMirrored, zero), h.visualPos, h.visualAngles,
            otherMirrored, drawnPos, drawnRot);
        return true;
    }
    if(h.type != weapons::HotspotType::Blade)
    {
        return false;
    }
    // The hand on the weapon as it lies, in the frame of it held (both the models' frames: mirrored for the off hand's).
    const glm::mat4 toHeld = hotspotFrame(weapon.ent, otherMirrored) * glm::inverse(hotspotFrame(e, false));
    const glm::mat3 turn{toHeld};
    glm::vec3 f, r, u;
    hands::angleVectors(handRot, f, r, u);
    drawnPos = glm::vec3{toHeld * glm::vec4{handPos, 1.f}};
    drawnRot = hands::anglesFromVectors(glm::normalize(turn * f), glm::normalize(turn * u));
    const glm::vec3 tip = view::anchorPosition(weapon, weapons::anchor(slot, Key::MuzzleAnchor),
        weapons::vec(slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    twohand::bladeGripOn(slot, other, tip, zero, zero, weaponAngleOffsets(weapons::fistSlot(), otherMirrored), drawnPos,
        drawnRot);
    alignChannel(hand, mirrored, zero, tip, drawnPos, drawnRot);
    return true;
}

double groundPrintAt = -1.0; // vr_debug_2h_grip 2: the next print of where an empty hand is on a weapon lying about

// Weapons held anywhere (vr_weapon_grab_anywhere): a hand whose closed fist (held::fist: the palm and the curled fingers,
// as a prop is taken) is within vr_carry_grab_bias of a weapon's drawn surface (or in it) is on it; one lying about, with
// vr_weapon_grab_slack more (as the server takes it: a gun lying flat is thinner than the lowest the fist gets over it).
// It was the hand's point within 6 cm: the open hand's fingertips' reach (the author's note hip1m1_2026-10-07_22-33-15).
// Without the jointed hand (no fist known): the point within this (cm), as before.
constexpr float anywhereReachCm = 6.f;
} // namespace

// How far (units) the world point `p` is from the drawn surface of `e` (its model's shape as drawn, mirrored or not):
// 0 inside it; `reach` or more if not within it.
[[nodiscard]] float surfaceDistance(const entity_t& e, bool mirrored, const glm::vec3& p, float reach)
{
    const grasp::Shape* shape = grasp::shapeOf(e, -1);
    float d = 0.f;
    glm::vec3 on, normal;
    if(!shape || !grasp::signedDistance(*shape, grasp::shapeToWorld(e, mirrored), p, reach, d, on, normal))
    {
        return reach;
    }
    return za::max(d, 0.f);
}

namespace
{
// The gap (units; negative: sunk in) between `hand`'s closed fist at its tracked pose and the drawn surface of `e`;
// `reach` or more if none of it is within `reach`. Without a fist (no jointed hand model): the hand's point's distance
// less anywhereReachCm (as before: within that is touching).
[[nodiscard]] float fistSurfaceGap(const entity_t& e, bool mirrored, const hands::State& s, int hand, float reach)
{
    const float pointReach = anywhereReachCm * 0.01f * units::metresToUnits();
    za::Vector<glm::vec4>& spheres = scratch.anywhereFist;
    held::fistInWorld(hand, s.pos[hand], s.rot[hand], spheres);
    if(spheres.empty())
    {
        return surfaceDistance(e, mirrored, s.pos[hand], reach + pointReach) - pointReach;
    }
    const grasp::Shape* shape = grasp::shapeOf(e, -1);
    if(!shape)
    {
        return reach;
    }
    const glm::mat4 toWorld = grasp::shapeToWorld(e, mirrored);
    float gap = reach;
    for(const glm::vec4& sp : spheres)
    {
        float d = 0.f;
        glm::vec3 on, normal;
        if(grasp::signedDistance(*shape, toWorld, glm::vec3{sp}, reach + sp.w, d, on, normal))
        {
            gap = za::fmin(gap, d - sp.w);
        }
    }
    return gap;
}
} // namespace

// Each frame, before the weapons are placed: for each empty hand, the weapon lying nearest it, and the hotspot it would
// take it by (twohand::recordGroundSpot), if any.
void updateGroundSpots(const hands::State& s)
{
    const float reach = groundSpotReachCm * 0.01f * units::metresToUnits();
    const float fromHandle = groundSpotFromHandleCm * 0.01f * units::metresToUnits();
    const float anywhereMin = za::max(vr_weapon_grab_anywhere_min.value, 0.f) * 0.01f * units::metresToUnits();
    const float anywhereAllowed =
        (vr_carry_grab_bias.value + za::max(vr_weapon_grab_slack.value, 0.f)) * 0.01f * units::metresToUnits();
    const float anywhereReach = za::fmax(anywhereAllowed, 0.f) + 1.f; // (how far the fist's gap is measured)
    bool printedGround = false; // (vr_debug_2h_grip 2)
    for(int hand = 0; hand < 2; hand++)
    {
        twohand::clearGroundSpot(hand);
        if((!vr_weapon_grab_hotspots.value && !vr_weapon_grab_anywhere.value) || !s.valid || !held::handEmpty(hand) ||
            twohand::helping(hand) ||
            twohand::carrying(hand))
        {
            continue;
        }
        const glm::vec3 at = s.pos[hand];
        const entity_t* best = nullptr;
        int bestSpot = -1;
        float bestDist = 1e30f;
        for(int i = 0; i < cl_numvisedicts; i++)
        {
            const entity_t* e = cl_visedicts[i];
            if(!e || e < cl_entities || e >= cl_entities + cl_max_edicts || !e->model || e->model->type != mod_alias ||
                view::find(e) || e == &cl_entities[cl.viewentity] || !modelmeta::has(e->model, modelmeta::Trait::ViewWeapon))
            {
                continue;
            }
            const int slot = weapons::slotForModel(e->model);
            const glm::vec3 origin{e->origin[0], e->origin[1], e->origin[2]};
            const float handle = glm::distance(at, origin);
            if(slot < 0 || slot == weapons::fistSlot() || handle > 64.f)
            {
                continue;
            }
            // Its hotspots as it lies: a grip's point, the nearest point of a blade's zone.
            int spot = -1;
            float closest = handle;
            float nearestSpot = 1e30f; // (unbiased: for anywhere)
            for(int k = 0; k < weapons::maxHotspots; k++)
            {
                const weapons::Hotspot h = weapons::hotspot(slot, k);
                glm::vec3 a, b;
                if(!groundHotspotZone(*e, slot, h, a, b) || glm::distance(a, origin) < fromHandle)
                {
                    continue;
                }
                const float len2 = glm::dot(b - a, b - a);
                const glm::vec3 p = len2 > 0.f ? a + (b - a) * za::clamp(glm::dot(at - a, b - a) / len2, 0.f, 1.f) : a;
                nearestSpot = za::min(nearestSpot, glm::distance(at, p));
                const float d = glm::distance(at, p) - h.bias;
                if(vr_weapon_grab_hotspots.value && d < closest && d < reach)
                {
                    closest = d;
                    spot = k;
                }
            }
            // Anywhere on it (vr_weapon_grab_anywhere): no hotspot taken, the hand on its drawn shape, away from its handle
            // and its hotspots (vr_weapon_grab_anywhere_min).
            if(spot < 0 && vr_weapon_grab_anywhere.value && handle >= anywhereMin && nearestSpot >= anywhereMin)
            {
                const float surface = fistSurfaceGap(*e, false, s, hand, anywhereReach);
                if(surface <= anywhereAllowed && za::fmax(surface, 0.f) < closest)
                {
                    closest = za::fmax(surface, 0.f);
                    spot = twohand::anywhereSpot;
                }
            }
            if(vr_debug_2h_grip.value >= 2.f && vr_gametime >= groundPrintAt)
            {
                Con_Printf("anywhere: %s hand's fist %.1f units off the lying %s's surface, %.1f from its handle, %.1f from its "
                           "nearest hotspot: %s (it lies at %.1f %.1f %.1f, angles %.1f %.1f %.1f)\n", hand == HAND_MAIN ? "main" : "off",
                    fistSurfaceGap(*e, false, s, hand, anywhereReach), e->model->name, handle, nearestSpot < 1e29f ? nearestSpot : -1.f,
                    spot == twohand::anywhereSpot ? "anywhere" : spot >= 0 ? "a hotspot" : "the handle", e->origin[0],
                    e->origin[1], e->origin[2], e->angles[0], e->angles[1], e->angles[2]);
                printedGround = true;
            }
            const float d = za::min(closest, handle);
            if(d < bestDist)
            {
                bestDist = d;
                best = e;
                bestSpot = spot;
            }
        }
        if(!best || bestSpot < 0)
        {
            continue;
        }
        const int slot = weapons::slotForModel(best->model);
        const bool mirrored = hand == HAND_OFF;
        // The hand as an empty hand is drawn (the fist's angle offsets), and as it would be drawn on the hotspot.
        const glm::mat3 fistTurn = anglesBasis(weaponAngleOffsets(weapons::fistSlot(), mirrored));
        const glm::vec3 handRot = basisAngles(anglesBasis(s.rot[hand]) * fistTurn);
        if(bestSpot == twohand::anywhereSpot)
        {
            // Carried as it lies, where the hand holds it: from the pose that draws it there (its handle's, unmirrored:
            // setupWeapon's place and weaponTurn undone), the hand drawn where it is.
            const glm::vec3 o = weaponAngleOffsets(slot, false);
            const glm::vec3 holderRot{o.x - best->angles[0], best->angles[1] - o.y, best->angles[2] - o.z};
            const glm::vec3 holderPos{best->origin[0], best->origin[1], best->origin[2]};
            twohand::recordGroundSpot(hand, static_cast<int>(best - cl_entities), bestSpot, at, s.rot[hand], holderPos,
                holderRot, false, at, handRot);
            continue;
        }
        glm::vec3 drawnPos, drawnRot;
        if(!groundSpotPose(*best, slot, weapons::hotspot(slot, bestSpot), hand, at, handRot, drawnPos, drawnRot))
        {
            continue;
        }
        // The hand stays where it is tracked, drawn as there (the weapon moves to it): tracked as the drawn hand on the
        // hotspot, less the fist's offsets.
        const glm::vec3 trackedRot = basisAngles(anglesBasis(drawnRot) * glm::transpose(fistTurn));
        twohand::recordGroundSpot(hand, static_cast<int>(best - cl_entities), bestSpot, drawnPos, trackedRot, glm::vec3{0.f},
            glm::vec3{0.f}, hand == HAND_MAIN, drawnPos, drawnRot);
    }
    if(printedGround)
    {
        groundPrintAt = vr_gametime + 0.25;
    }
}

namespace
{
// The distance from `p` to the segment from `a` to `b`.
[[nodiscard]] float segmentPointDistance(const glm::vec3& p, const glm::vec3& a, const glm::vec3& b)
{
    const float len2 = glm::dot(b - a, b - a);
    return glm::distance(p, len2 > 0.f ? a + (b - a) * za::clamp(glm::dot(p - a, b - a) / len2, 0.f, 1.f) : a);
}

double freeSpotPrintAt = -1.0; // vr_debug_2h_grip 2: the next print of where an empty hand is on the other's weapon
} // namespace

// Each frame, after the weapons are placed: whether each empty hand is on the other hand's weapon (held or carried) where
// gripping holds it anywhere (vr_weapon_grab_anywhere; twohand::setFreeCandidate): its fist within vr_carry_grab_bias of
// its drawn surface (fistSurfaceGap), at least vr_weapon_grab_anywhere_min from its handle and (held by its handle) from its grip and blade hotspots,
// which take priority.
void updateFreeSpots(const hands::State& s)
{
    const float minAway = za::max(vr_weapon_grab_anywhere_min.value, 0.f) * 0.01f * units::metresToUnits();
    const float allowed = vr_carry_grab_bias.value * 0.01f * units::metresToUnits();
    const float reach = za::fmax(allowed, 0.f) + 1.f; // (how far the fist's gap is measured)
    const bool print = vr_debug_2h_grip.value >= 2.f && vr_gametime >= freeSpotPrintAt;
    for(int hand = 0; hand < 2; hand++)
    {
        const int other = 1 - hand;
        const view::ViewEntity& w = entities.weapon[other];
        const int slot = weapons::slotForModel(w.ent.model);
        bool on = false;
        if(vr_weapon_grab_anywhere.value && s.valid && held::handEmpty(hand) && !twohand::carrying(hand) &&
            !flashlight::holds(hand) && w.visible &&
            w.ent.model && w.ent.model->type == mod_alias && slot >= 0 && slot != weapons::fistSlot() &&
            !isHandModel(w.ent.model))
        {
            const glm::vec3 at = s.pos[hand];
            const float handle = glm::distance(at, drawnAs[other].pos);
            float nearestSpot = 1e30f;
            if(!weaponCarried[other])
            {
                for(int i = 0; i < weapons::maxHotspots; i++)
                {
                    const WorldHotspot& h = worldHotspots[other][i];
                    if(i == magSpot[other] && s.magBoxValid[other])
                    {
                        nearestSpot = za::min(nearestSpot, boxDistance(s.magBox[other], at)); // (its attached magazine)
                    }
                    else if(weapons::isGripType(h.type) || h.type == weapons::HotspotType::Blade)
                    {
                        nearestSpot = za::min(nearestSpot, segmentPointDistance(at, h.pos, h.end));
                    }
                }
            }
            const float surface = handle < 64.f ? fistSurfaceGap(w.ent, w.mirrored, s, hand, reach) : reach;
            // (A carried weapon's handle: where gripping takes it back, HS_CARRIED_GRIP.)
            const float fromHandle = weaponCarried[other] ? za::max(minAway, twohand::carriedGripRadius) : minAway;
            // (On its attached magazine: gripping there holds the magazine, hotspot HS_MAGAZINE.)
            on = surface <= allowed && handle >= fromHandle && nearestSpot >= minAway && !s.onMagazine[hand];
            if(print)
            {
                Con_Printf("anywhere: %s hand's fist %.1f units off the %s weapon's surface, %.1f from its handle, %.1f from its "
                           "nearest hotspot: %s\n", hand == HAND_MAIN ? "main" : "off", surface,
                    weaponCarried[other] ? "carried" : "held", handle, nearestSpot < 1e29f ? nearestSpot : -1.f,
                    on ? "on it" : "no");
            }
        }
        twohand::setFreeCandidate(hand, on);
    }
    if(print)
    {
        freeSpotPrintAt = vr_gametime + 0.25;
    }
}

namespace
{
entity_t placedTorch[2]; // (setupHand: the torch as it is placed this frame, per hand)
} // namespace

// The limb the local player's `hand` holds (a listen server's ragdoll: box3d::ragdollHold), as drawn now. False: none.
[[nodiscard]] bool limbHold(int hand, LimbHold& out)
{
    out = LimbHold{};
    box3d::RagdollHold h;
    if(!sv.active || cls.state != ca_connected || !box3d::ragdollHold(cl.viewentity, hand, h))
    {
        return false;
    }
    glm::quat rot;
    glm::vec3 pos;
    float scale = 1.f;
    const ragdoll::Rig* rig = nullptr;
    if(!ragdoll::drawnPart(h.num, h.part, rot, pos, scale, &rig) || !rig)
    {
        return false;
    }
    out.num = h.num;
    out.part = h.part;
    out.pos = rot * h.at + pos;
    out.turn = glm::normalize(rot * h.turn);
    out.toWorld = glm::translate(glm::mat4{1.f}, pos) * glm::mat4_cast(rot) * glm::scale(glm::mat4{1.f}, glm::vec3{scale});
    const int id = -2 - h.part; // (grasp::keptShape's ids: negative)
    out.shape = grasp::keptShape(rig->model, id);
    if(!out.shape)
    {
        ragdoll::boneTriangles(*rig, h.part, scratch.limbPoints);
        za::Vector<grasp::Triangle>& tris = scratch.limbTris;
        tris.clear();
        for(za::SizeT i = 0; i + 2 < scratch.limbPoints.size(); i += 3)
        {
            tris.pushBack({{scratch.limbPoints[i], scratch.limbPoints[i + 1], scratch.limbPoints[i + 2]}});
        }
        out.shape = grasp::keptShape(rig->model, id, &tris);
    }
    return true;
}

namespace
{
constexpr float limbEaseTime = 0.1f; // s

// `pos`, `angles` (the controller's pose of `hand`) moved onto the limb it holds, as if its controller were there.
void limbDrawnHand(int hand, glm::vec3& pos, glm::vec3& angles)
{
    LimbEase& e = limbEase[hand];
    const float dt = e.time >= 0.0 ? static_cast<float>(CLAMP(0.0, cl.time - e.time, 0.1)) : 0.f;
    e.time = cl.time;
    const float stick = za::fmax(vr_ragdoll_hand_stick.value, 0.f) * 0.01f * units::metresToUnits();
    LimbHold h;
    const bool on = stick > 0.f && limbHold(hand, h);
    if(on)
    {
        // Turned with the limb (at most vr_ragdoll_hand_turn), about its grip channel where the limb has it: the fingers
        // stay on the limb as the hand turns less than it.
        const glm::quat controller = glm::normalize(glm::quat_cast(anglesBasis(angles)));
        glm::quat turn = glm::normalize(h.turn * glm::inverse(controller));
        if(turn.w < 0.f)
        {
            turn = -turn;
        }
        const float most = za::fmax(vr_ragdoll_hand_turn.value, 0.f);
        const float degrees = glm::degrees(glm::angle(turn));
        if(degrees > most)
        {
            turn = glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, turn, most / degrees);
        }
        const glm::vec3 channel = grip::handFrame(hand, hand == HAND_OFF, true).channelPoint; // (the hand's frame)
        const glm::vec3 contact = h.pos + glm::mat3_cast(h.turn) * channel;
        glm::vec3 offset = contact - glm::mat3_cast(turn * controller) * channel - pos;
        const float length = glm::length(offset);
        if(length > stick)
        {
            offset *= stick / length;
        }
        e.offset = offset;
        e.turn = turn;
        e.unclamped = degrees;
    }
    e.blend = on ? za::fmin(1.f, e.blend + dt / limbEaseTime) : za::fmax(0.f, e.blend - dt / limbEaseTime);
    if(e.blend <= 0.f)
    {
        return;
    }
    pos += e.offset * e.blend;
    const glm::quat turn = glm::slerp(glm::quat{1.f, 0.f, 0.f, 0.f}, e.turn, e.blend);
    angles = basisAngles(glm::mat3_cast(turn) * anglesBasis(angles));
}
} // namespace

void setupHand(const hands::State& s, int hand)
{
    QVR_PROFILE("hand");
    const view::ViewEntity& weapon = entities.weapon[hand];
    const bool mirrored = hand == HAND_OFF;
    const int fist = weapons::fistSlot();
    const int slot = weapons::slotForModel(weapon.ent.model);

    if(!weapon.ent.model)
    {
        for(view::ViewEntity& ve : entities.hand[hand])
        {
            ve.visible = false;
        }
        rigHands[hand].drawn = false;
        rigHands[hand].held = Held{};
        return;
    }

    // A prop held in both hands (vr_held.cpp): the hand is drawn on its grip on it, as if its controller were there.
    glm::vec3 controllerPos = s.pos[hand], controllerRot = s.rot[hand];
    held::drawnHand(hand, controllerPos, controllerRot);
    // A ragdoll's limb: the hand drawn on it, where it holds it (limbDrawnHand).
    limbDrawnHand(hand, controllerPos, controllerRot);
    // Holding the other hand's weapon anywhere on it (vr_twohand.cpp's free grips): drawn where it holds it, as if its
    // controller were there (the weapon drawn from drawnAs: as held by its handle, or carried), its fingers wrapping it.
    const bool freeHelp = twohand::freeHand(hand, s.pos[hand], s.rot[hand], drawnAs[1 - hand].pos, drawnAs[1 - hand].rot,
        controllerPos, controllerRot);
    glm::vec3 lightShift{0.f}; // a hand holding a ledge or a rung: drawn on it, facing it (vr_climb.cpp), lit as without the looks' offset
    const float onHold = climb::drawnHand(s, hand, anglesBasis(weaponAngleOffsets(fist, mirrored)), controllerPos, controllerRot, lightShift);
    // A hand holding an enemy: drawn on the spot it holds, as the model moves (vr_foegrab.cpp).
    (void)foegrab::drawnHand(s, hand, controllerPos, controllerRot);
    if(onHold > 0.f)
    {
        // Drawn on its hold, it shakes there too (tired arms: vr_fatigue.cpp; the shake is in s's hands).
        glm::vec3 shakePos, shakeAngles;
        fatigue::shake(hand, shakePos, shakeAngles);
        controllerPos += shakePos * onHold;
        controllerRot += shakeAngles * onHold;
    }
    for(view::ViewEntity& ve : entities.hand[hand])
    {
        ve.lightShift = lightShift;
    }

    // The hand turned from the controller by the fist's angle offsets, rigidly (round 21, second pass: added as Euler
    // angles, the hand slid round what it held as the wrist turned, and its grasp was solved again and again).
    glm::vec3 handRot = basisAngles(anglesBasis(controllerRot) * anglesBasis(weaponAngleOffsets(fist, mirrored)));

    // The hand is where the controller is, holding a weapon or not (round 21: the weapon is placed in the hand, and the
    // fingers wrap it; it was drawn at an anchor of the weapon, which the settings kept within 0.8 of a
    // centimetre of the controller).
    glm::vec3 pos = controllerPos;
    bool hide = false;
    glm::mat4 motion{1.f}; // the weapon's firing animation where the hand holds it (animationMotion)
    if(slot >= 0 && slot != fist)
    {
        hide = weapons::value(slot, Key::HideHand) != 0.f;
        handRot = attachedTurn(s.rot[hand], slot, mirrored, weaponAngleOffsets(fist, mirrored), hand);
        // Round 21, third pass: the Hand Only offset (vr_wofs_hand_only_*): the drawn hand moved and turned on the
        // weapon, in the aim frame, about its palm; the weapon, its muzzle and its aim stay, the fingers wrap it again.
        moveDrawnHand(hand, mirrored, attachedTurn(s.rot[hand], slot, mirrored, glm::vec3{0.f}, hand),
            weapons::vec(slot, Key::HandOnlyX, Key::HandOnlyY, Key::HandOnlyZ),
            weapons::vec(slot, Key::HandOnlyPitch, Key::HandOnlyYaw, Key::HandOnlyRoll), mirrored, pos, handRot);
        motion = animationMotion(weapon, pos);
    }

    // Steadying the other hand's weapon in the "fixed" two-handed display mode: the hand moves
    // onto the weapon's foregrip (blending in and out with the grip), turned like the holding
    // hand plus the weapon's fixed-hand angles (old engine's V_SetupFixedHelpingHandViewEnt).
    const int other = 1 - hand;
    const float gripBlend = s.grip2HValid[other] ? twohand::transition(other) : 0.f;
    const int otherSlot = weapons::slotForModel(entities.weapon[other].ent.model);
    // The hotspot it holds the other hand's weapon by (the blade's, for the half-sword grip).
    const WorldHotspot* heldSpot = nullptr;
    if(gripBlend > 0.f && twohand::bladeGrip(other))
    {
        for(const WorldHotspot& w : worldHotspots[other])
        {
            heldSpot = w.type == weapons::HotspotType::Blade ? &w : heldSpot;
        }
    }
    else if(gripBlend > 0.f && chosenGrip[other] >= 0)
    {
        heldSpot = &worldHotspots[other][chosenGrip[other]];
    }
    glm::vec3 bladePos = pos, bladeRot = handRot;
    if(gripBlend > 0.f && twohand::bladeGrip(other) && otherSlot >= 0 &&
        twohand::bladeGripHand(s, hand,
            glm::vec3{entities.weapon[other].ent.origin[0], entities.weapon[other].ent.origin[1], entities.weapon[other].ent.origin[2]},
            s.rot[other] + weaponAngleOffsets(fist, other == HAND_OFF), bladePos, bladeRot))
    {
        // Holding the other hand's sword by its blade (round 18): on the blade where the hand is,
        // turned (the least turn) to close round it; its fingers' channel on the blade (round 21, second pass: the
        // blade lay beside the fingers, the author's "very far away from the blade").
        if(s.muzzleValid[other])
        {
            const glm::vec3 hilt{entities.weapon[other].ent.origin[0], entities.weapon[other].ent.origin[1],
                entities.weapon[other].ent.origin[2]};
            alignChannel(hand, mirrored, hilt, s.muzzle[other] - hilt, bladePos, bladeRot);
        }
        pos = glm::mix(pos, bladePos, gripBlend);
        handRot = bladeRot;
        hide = false;
        motion = partMotion(animationMotion(entities.weapon[other], bladePos), gripBlend, bladePos);
    }
    else if(gripBlend > 0.f && heldSpot && magSpot[other] >= 0 && chosenGrip[other] == magSpot[other] &&
            s.magBoxValid[other])
    {
        // On the gun's attached magazine (immersive reloading): the hand's grip channel along it where the hand holds it,
        // turned from the tracked hand the least (any way round it), then its offset (vr_reload_hold_<kind>_*, below).
        glm::vec3 at = pos, turn = handRot;
        const glm::vec3& along = s.magBox[other][1];
        alignChannel(hand, mirrored, s.magBox[other][0], along, at, turn, false,
            za::max(glm::length(along) - magHandMargin, 0.f));
        pos = glm::mix(pos, at, gripBlend);
        const glm::quat from = glm::quat_cast(anglesBasis(handRot)), to = glm::quat_cast(anglesBasis(turn));
        handRot = basisAngles(glm::mat3_cast(glm::slerp(from, to, gripBlend)));
        hide = false;
        motion = partMotion(animationMotion(entities.weapon[other], at), gripBlend, at);
    }
    else if(gripBlend > 0.f)
    {
        pos = glm::mix(pos, s.grip2H[other], gripBlend);
        hide = false;
        motion = partMotion(animationMotion(entities.weapon[other], s.grip2H[other]), gripBlend, s.grip2H[other]);

        if(twohand::helping(hand) && otherSlot >= 0)
        {
            // A grip: the weapon's fixed-hand angles; a cup: turned as the holding hand is. Then the hotspot's own.
            // (The weapon's and the hotspot's are for the off hand helping: mirrored for the main hand.) Carried
            // rigidly by the weapon (attachedTurn), turned onto it as the hand takes the grip.
            const bool cup = heldSpot && heldSpot->type == weapons::HotspotType::Cup;
            const glm::vec3 attached =
                helpingTurn(s.rot[other], other, hand, otherSlot, cup, heldSpot ? heldSpot->angles : glm::vec3{0.f});
            // A cup is where the palm's middle goes (round 21, third pass: it was a point the hand's grip channel was
            // aligned round, often far from where the hand ended up): the hand placed so, as it is turned there.
            if(cup)
            {
                const glm::vec3 at = s.grip2H[other] - palmAt(hand, glm::vec3{0.f}, attached, mirrored);
                pos = glm::mix(s.pos[hand], at, gripBlend);
            }
            const glm::quat from = glm::quat_cast(anglesBasis(handRot)), to = glm::quat_cast(anglesBasis(attached));
            handRot = basisAngles(glm::mat3_cast(glm::slerp(from, to, gripBlend)));
            // The super shotgun open: the hand on its fore-end turned down with the barrels (the author: it kept the
            // shut gun's turn, as if it held nothing).
            if(heldSpot && heldSpot->ssgTurned)
            {
                handRot = basisAngles(ssgWorldTurn(entities.weapon[other], ssgHands[other].angle) * anglesBasis(handRot));
                if(vr_reload_debug.value >= 2 && developer.value)
                {
                    Con_Printf("ssg: the hand on the open barrels turned %.1f deg with them (pitch %.1f)\n",
                        static_cast<double>(ssgHands[other].angle), static_cast<double>(handRot.x));
                }
            }
        }
    }
    // Round 21, third pass: the held hotspot's hand offset (vr_wofs_hsN_v*): its drawn pose moved and turned once it
    // holds it, as it takes it (visual only: the grab and the aim are the tracked hand's), in the holding hand's aim
    // frame, about its palm; as for the off hand helping, mirrored for the main hand.
    if(heldSpot && twohand::helping(hand))
    {
        moveDrawnHand(hand, mirrored, attachedTurn(s.rot[other], otherSlot, other == HAND_OFF, glm::vec3{0.f}, other),
            heldSpot->visualPos * gripBlend, heldSpot->visualAngles * gripBlend, other == HAND_OFF, pos, handRot);
    }

    // A free hand pushed into the other hand's weapon stops at its surface (drawn only), held back a few centimetres at
    // most: past that it passes into it until it is out again (vr_hand_collide).
    pushOut(hand,
        gripBlend <= 0.f && !freeHelp && (slot < 0 || slot == fist) && !held::heldEntity(hand) && !flashlight::holds(hand) &&
            limbEase[hand].blend <= 0.f,
        pos, handRot, mirrored);

    // The hand-off (vr_twohand.cpp): a helping hand's drawn pose is what it carries a gun by, and a
    // hand carrying one is drawn so.
    if(twohand::helping(hand))
    {
        twohand::recordHelp(s, hand, pos, handRot, drawnAs[other].pos, drawnAs[other].rot, drawnAs[other].mirrored);
    }
    else if(twohand::carryingHand(s, hand, pos, handRot))
    {
        hide = false;
    }

    // What the fingers wrap (vr_hand_fit): the gun in the hand, the other hand's gun it helps hold (its foregrip, a
    // sword's blade), a carried box or gib, the flashlight.
    Held held;
    if(gripBlend > 0.f && entities.weapon[other].ent.model)
    {
        held = {&entities.weapon[other].ent, other == HAND_OFF, 0, true};
        if(heldSpot && magSpot[other] >= 0 && heldSpot == &worldHotspots[other][magSpot[other]] &&
            entities.mag[other].visible && entities.mag[other].ent.model)
        {
            // Its attached magazine: wrapped round the magazine (or the fingers its page sets).
            held = {&entities.mag[other].ent, entities.mag[other].mirrored, 0, true};
            held.overlap = CLAMP(0.f, heldSpot->overlap, 1.f) * weapons::maxOverlapCm;
            setManualFingers(held, heldSpot->def.manual, heldSpot->def.curl, heldSpot->def.thumbAcross);
        }
        // Its hotspot's kind, style and overlap.
        else if(heldSpot)
        {
            held.cup = heldSpot->type == weapons::HotspotType::Cup;
            held.thumbTop = heldSpot->style == weapons::HotspotStyle::ThumbTop;
            held.overlap = CLAMP(0.f, heldSpot->overlap, 1.f) * weapons::maxOverlapCm;
            setManualFingers(held, heldSpot->def.manual, heldSpot->def.curl, heldSpot->def.thumbAcross);
            // Held fully by a grip or a cup (not while taking it, nor a blade, where the hand slides): its settled place.
            if(gripBlend >= 1.f && weapons::isGripType(heldSpot->type) && twohand::helping(hand) && otherSlot >= 0 &&
                handrig::usable(viewModel(handrig::modelName)))
            {
                held.canonical = true;
                held.canonicalInRig = canonicalHotspotGrip(hand, otherSlot, entities.weapon[other].ent.model, heldSpot->def);
                held.offset = heldSpot->def.visualPos != glm::vec3{0.f} || heldSpot->def.visualAngles != glm::vec3{0.f};
                if(held.offset)
                {
                    held.fitInRig = canonicalHotspotGrip(hand, otherSlot, entities.weapon[other].ent.model, heldSpot->def, false);
                }
            }
        }
    }
    else if(freeHelp && entities.weapon[other].ent.model)
    {
        // The other hand's weapon, held anywhere: wrapped where the hand is on it.
        held = {&entities.weapon[other].ent, entities.weapon[other].mirrored, 0, true};
        held.overlap = otherSlot >= 0 ? CLAMP(0.f, weapons::value(otherSlot, Key::GripOverlap), 1.f) * weapons::maxOverlapCm : -1.f;
    }
    else if(slot >= 0 && slot != fist)
    {
        held = {&weapon.ent, mirrored, 0, true, true};
        weaponFingers(held, slot);
        if(!weaponCarried[hand] && handrig::usable(viewModel(handrig::modelName)))
        {
            held.canonical = true;
            held.canonicalInRig = canonicalOwnGrip(hand, slot, weapon.ent.model);
            held.offset = weapons::vec(slot, Key::HandOnlyX, Key::HandOnlyY, Key::HandOnlyZ) != glm::vec3{0.f} ||
                          weapons::vec(slot, Key::HandOnlyPitch, Key::HandOnlyYaw, Key::HandOnlyRoll) != glm::vec3{0.f};
            if(held.offset)
            {
                held.fitInRig = canonicalOwnGrip(hand, slot, weapon.ent.model, false);
            }
        }
    }
    else if(const int ent = held::heldEntity(hand))
    {
        held = {&cl_entities[ent], false, -1};
        // Its fingers and overlap (Held Object Offsets, vr_props.inc; the flashlight's are its own page's).
        if(const qmodel_t* model = cl_entities[ent].model)
        {
            const int slot = props::slotForModel(model);
            using props::Key;
            held.overlap = props::value(slot, Key::Overlap);
            held.inPalm = static_cast<int>(props::value(slot, Key::GripMode)) == 2; // (vr_grip.hpp's Palm)
            const float curl[handrig::FingerCount]{props::value(slot, Key::FingerCurlThumb), props::value(slot, Key::FingerCurlIndex),
                props::value(slot, Key::FingerCurlMiddle), props::value(slot, Key::FingerCurlRing), props::value(slot, Key::FingerCurlPinky)};
            setManualFingers(held, props::value(slot, Key::FingerManual) >= 0.5f, curl, props::value(slot, Key::FingerThumbAcross));
        }
    }
    else if(LimbHold limb; limbHold(hand, limb) && limb.shape)
    {
        // A ragdoll's limb (limbHold): its mesh, as drawn (its part: what keeps its grasp apart from the others').
        held = {&cl_entities[limb.num], false, limb.part};
        held.shape = limb.shape;
        held.toWorld = limb.toWorld;
    }
    else if(flashlight::holds(hand) && entities.flashlight.ent.model)
    {
        // The torch as it is placed this frame (it is set up after the hands).
        entity_t(&torch)[2] = placedTorch;
        torch[hand] = entities.flashlight.ent;
        glm::vec3 origin, angles;
        if(flashlight::heldPlace(s, hand, origin, angles))
        {
            for(int i = 0; i < 3; i++)
            {
                torch[hand].origin[i] = origin[i];
                torch[hand].angles[i] = angles[i];
            }
            held = {&torch[hand], false, 0};
            // Its grip's fingers (vr_flashlight_low_* or _high_*: Fingers, the curls, Thumb Across, Overlap).
            if(flashlight::Fingers f; flashlight::fingers(hand, f))
            {
                held.overlap = f.overlap * weapons::maxOverlapCm;
                setManualFingers(held, f.manual, f.curl, f.thumbAcross);
            }
        }
    }
    drawHand(hand, pos, handRot, mirrored, hide, held, motion);
}

// A hand drawn at (`pos`, `handRot`), holding `held` (the jointed hand; else the six models).
void drawHand(int hand, glm::vec3 pos, glm::vec3 handRot, bool mirrored, bool hide, const Held& held, const glm::mat4& motion)
{
    if(setupRigHand(hand, pos, handRot, mirrored, hide, held, motion))
    {
        return;
    }
    movePose(motion, pos, handRot);

    const float offsetScale = weapons::offsetScale();
    const int skin = wounds::skinLevel(hand, damageLevel(), &pos.x); // (washed, a gib's blood: vr_wounds.cpp)
    for(int finger = 0; finger < FingerCount; finger++)
    {
        view::ViewEntity& ve = entities.hand[hand][finger];

        glm::vec3 foff = fingerOffset(finger, hand) * offsetScale;
        if(mirrored)
        {
            foff.y = -foff.y;
        }

        const FingerPose curl = fingerPose(hand, finger);
        place(ve, viewModel(fingerModels[finger]), pos + hands::redirect(foff, handRot),
            {-handRot.x, handRot.y, handRot.z}, curl.frame, mirrored);
        ve.zeroBlend = curl.open;
        ve.ent.skinnum = skin;

        if(hide)
        {
            ve.visible = false;
        }
    }
}

// ----------------------------------------------------------------------------
// Weapon posing mode (vr_posing.cpp): the weapon floating still in front of the player, drawn as the weapon hand holds
// it (by setupWeapon, from a hand put where its settings, whatever they are now, leave the weapon where it floats); the
// posing hand drawn at its controller, passing through it (unsolved; for a moment after a set, wrapping it as play will);
// and what confirming would write (posing::Candidate), worked out from the placement's own steps undone.

// The turn of an entity as drawn (R_EntityMatrix's, without its origin).
[[nodiscard]] glm::mat3 entityTurn(const entity_t& e)
{
    float m[16];
    vec3_t origin{0.f, 0.f, 0.f}, angles;
    VectorCopy(e.angles, angles);
    R_EntityMatrix(m, origin, angles, ENTSCALE_DEFAULT);
    return glm::mat3{toMat4(m)};
}

// Where the floating weapon goes at first: level, facing where the head does, its grip (the holding hand's point: the
// weapon's origin) 40 cm ahead of the head and 35 cm below it (the chest); the sticks turn it about the middle of its
// grip and muzzle.
void placeFloating(const hands::State& s, posing::Session& ps)
{
    const float m2u = units::metresToUnits();
    const glm::vec3 level{0.f, s.headAngles.y, 0.f};
    const glm::vec3 grip = s.head + hands::forward(level) * (0.4f * m2u) - glm::vec3{0.f, 0.f, 0.35f * m2u};
    const bool mirrored = ps.weaponHand == HAND_OFF;
    view::ViewEntity ve;
    place(ve, ps.model, grip, level, 0, mirrored);
    ps.turn = anglesBasis(level);
    ps.modelOrigin = glm::vec3{hotspotFrame(ve.ent, mirrored)[3]};
    const glm::vec3 muzzle = view::anchorPosition(ve, weapons::anchor(ps.slot, Key::MuzzleAnchor),
        weapons::vec(ps.slot, Key::MuzzleOffsetX, Key::MuzzleOffsetY, Key::MuzzleOffsetZ));
    ps.pivot = glm::mix(grip, muzzle, 0.5f);
    ps.tiltAxis = ps.turn[1]; // across the view (to the left)
    ps.startOrigin = ps.modelOrigin;
    ps.startTurn = ps.turn;
    ps.placed = true;
}

// The weapon hand put where its weapon, placed by its settings now, is drawn where it floats: the weapon's turn is the
// hand's plus its angle offsets (weaponTurn), its model's origin the hand's point plus its offset in its own frame.
void holdFloating(hands::State& s, const posing::Session& ps)
{
    const int wh = ps.weaponHand;
    const bool mirrored = wh == HAND_OFF;
    const glm::vec3 wt = basisAngles(ps.turn);
    const glm::vec3 o = weaponAngleOffsets(ps.slot, mirrored);
    view::ViewEntity ve;
    place(ve, ps.model, glm::vec3{0.f}, {-wt.x, wt.y, wt.z}, 0, mirrored);
    s.pos[wh] = ps.modelOrigin - glm::vec3{hotspotFrame(ve.ent, mirrored)[3]};
    s.rot[wh] = s.visualRot[wh] = {wt.x + o.x, wt.y - o.y, wt.z - o.z};
    wholeTurn[wh] = glm::mat3{1.f};
}
} // namespace view_detail
