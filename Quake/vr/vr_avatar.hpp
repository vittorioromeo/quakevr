// vr_avatar.hpp -- the player's skinned body (progs/vrbody.md5mesh, from
// Misc/quakevr/make_vrbody.py), posed each frame by inverse kinematics from the head and the
// drawn hands: the spine follows the head (crouching bends the legs and the back), the
// shoulders rise and swing when reaching, and the arms are two-bone chains to the wrists, with
// the elbows placed by heuristics after Parger et al., "Human upper-body inverse kinematics for
// increased embodiment in consumer-grade virtual reality" (VRST 2018). Legs, optionally, stand
// planted under the body, stepping round as it turns, and walk as the player moves; in water they
// wade, and swimming they trail behind and kick where the stick moves the player.
//
// The pose is drawn through the renderer's skeletal (MD5) path, with the entity's own bone
// matrices (VR_AliasBonePoses in r_alias.c) instead of the model's animation.

#pragma once

#include "vr_engine.hpp" // qmodel_t, entity_t
#include "vr_hands.hpp"

namespace qvr::avatar
{

struct Frame
{
    glm::vec3 pos{0.f};
    glm::mat3 rot{1.f}; // columns: along the bone (up the spine), side, forward
};

struct Torso
{
    Frame pelvis;
    Frame chest;
};

// The spine's pose from the head alone (usable before the hands are placed).
[[nodiscard]] Torso torso(const hands::State& s);

// `s` standing upright under the same head position, looking straight ahead at the body's yaw.
[[nodiscard]] hands::State standing(const hands::State& s);

// The model's upper arm and forearm (metres of the model, before the body's scale).
void armBones(float& upper, float& fore);

// Body Calibration (vr_bodycal.cpp): the chest as the body stands upright under a head (its eyes where `s` has them,
// turned as it has them), facing `yaw`, `eyeHeight` metres tall (the crouch and the lean left out: a seated player's
// chest is upright too). Its axes as Frame's: up the spine, the body's right, forward.
[[nodiscard]] Frame uprightChest(const hands::State& s, float yaw, float eyeHeight);

// The arm model the calibration fits, as the arms are solved (the clavicle's turn about the base of the neck), in the
// chest's frame (forward, left, up; real metres from the chest joint).
struct ShoulderModel
{
    float scale{1.f};      // the body's scale (eye height / the models')
    glm::vec3 offset{0.f}; // vr_body_shoulders_back, _up, _out (metres of the model)
    float armLength{0.55f}; // the upper arm and forearm (real metres)
    float upDegrees{25.f};  // vr_body_shoulder_up
    float forwardDegrees{20.f}; // vr_body_shoulder_forward
    bool calibrated{true};      // the calibrated arms' continuous rise
};
// The shoulder joint of `side` (0 the body's left, 1 its right) with the drawn wrist at `wrist` (the chest's frame).
[[nodiscard]] glm::vec3 shoulderInChest(int side, const glm::vec3& wrist, const ShoulderModel& m);
enum class Part
{
    Pelvis,
    Chest
};

// How a thigh has moved with the legs' animation (walking, stepping, kicking in the water) from
// where it would be with the legs standing still under the body as it now is: a turn about the hip
// joint. `down` is the still thigh's direction, from the hip towards the knee.
struct ThighMotion
{
    glm::vec3 joint{0.f};
    glm::vec3 down{0.f, 0.f, -1.f};
    glm::mat3 turn{1.f};
};

// Where points given for the standing body (see standing()) are now, carried by a part as the
// body leans and crouches. The torso is solved (as it is and standing) once, for any number of
// points.
class Follower
{
public:
    explicit Follower(const hands::State& s);

    [[nodiscard]] glm::vec3 operator()(Part part, const glm::vec3& standingPoint) const;

    // The thigh of `side` (0 the body's left, 1 its right) as the legs were last posed (the full
    // body, vr_body_mode 3), carried by the pelvis as it is now. False without posed legs.
    [[nodiscard]] bool thigh(int side, ThighMotion& out) const;

private:
    Torso now;
    Torso ref;
};

// Whether `model` is the skinned body with the expected skeleton.
[[nodiscard]] bool usable(qmodel_t* model);

// Forgets the models checked (a game directory change reuses their slots).
void reset();

// A drawn hand, in world space: its wrist, the top of the (gripping) hand, where the thumb and
// index finger are, the back of the hand (away from the palm), and the direction from the wrist
// to the fingers.
struct HandPose
{
    glm::vec3 wrist{0.f};
    glm::vec3 up{0.f, 0.f, 1.f};
    glm::vec3 back{0.f, 0.f, 1.f};
    glm::vec3 forward{0.f};
};

// Poses the body for this frame, per hand (HAND_OFF, HAND_MAIN). Returns the entity origin (the
// pelvis).
glm::vec3 pose(const hands::State& s, qmodel_t* model, const entity_t* ent, const HandPose handPoses[2], bool legs);

// The forearm of `hand` (HAND_OFF, HAND_MAIN) as last posed: its wrist and direction (from the
// elbow). False when the body is not posed.
[[nodiscard]] bool forearm(int hand, glm::vec3& wrist, glm::vec3& direction);

// The forearm of `hand` as last posed (pose) or solved (solveArms), at `along` it (0 the elbow, 1 the wrist): the
// point on its axis there, and its axes there: x along the forearm (from the elbow), y and z turned about it by as
// much of the hand's roll as the forearm turns there (the twist joints' shares, in between them interpolated: none
// at the elbow, all at the wrist). The wrist's bend (flexion, deviation) doesn't move them. `hand` gets the hand
// bone's axes (x towards the fingers), whose roll the frame at 1 has without the bend: a direction fixed in the hand,
// `h`, is carried by the forearm there as axes * transpose(hand) * h. False when neither has run this frame.
struct ForearmFrame
{
    glm::vec3 point{0.f};
    glm::mat3 axes{1.f};
    glm::mat3 hand{1.f};
    float length{0.f}; // the forearm's, elbow to wrist (world units)
};
[[nodiscard]] bool forearmFrame(int hand, float along, ForearmFrame& out);

// The forearm's girth `along` it (0 the elbow, 1 the wrist) for a body build (0 lean, 1 athletic, 2 brawny): the
// semi-axes of the bracer's ring there (make_vrbody.py's, in metres; the bracer runs from 0.13 m to the wrist of a
// 0.26 m forearm, before it the ring where it starts), `hint` along the forearm frame's z (the little finger's
// side), `other` along its y. Its rings are polygons inside this ellipse. Between them it is straight.
void forearmGirth(int build, float along, float& hint, float& other);

// The arms alone, solved as pose() does (the torso and the arm IK: no legs, no skinning), for the forearms' frames
// (forearmFrame) when the body isn't drawn. The elbow isn't swung to ease a bent wrist (vr_body_wrist_limits): with no
// arm drawn, the forearm stays put while the hand bends, as a real one does.
void solveArms(const hands::State& s, const HandPose handPoses[2]);

// A shoulder as last posed (side 0 the body's left, 1 its right), for what is strapped to it (the
// pauldrons): the shoulder joint, and the rotations that take the bind pose's body space (x forward,
// y left, z up; make_vrbody.py) to the world as the clavicle and the upper arm carry it. `m2w` is
// world units per metre of the body. False when the body is not posed.
struct Shoulder
{
    glm::vec3 joint{0.f};
    glm::quat clavicle{1.f, 0.f, 0.f, 0.f};
    glm::quat upperArm{1.f, 0.f, 0.f, 0.f};
    float m2w{1.f};
};
[[nodiscard]] bool shoulder(int side, Shoulder& out);

// The body as last posed (pose(), before the preview's move: where it is on the player), for the drawn body's collision
// proxies (vr_selfcollide.cpp): its joints in the world. Per hand (HAND_OFF, HAND_MAIN): the shoulder joint, the
// elbow and the wrist; per side (0 the body's left, 1 its right): the hip, the knee and the ankle (`legs`: the full
// body's, posed this frame). `m2w`: world units per metre of the modelled body. False when the body is not posed.
struct Skeleton
{
    float m2w{1.f};
    glm::vec3 shoulder[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 elbow[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 wrist[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    bool legs{false};
    glm::vec3 hip[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 knee[2]{glm::vec3{0.f}, glm::vec3{0.f}};
    glm::vec3 ankle[2]{glm::vec3{0.f}, glm::vec3{0.f}};
};
[[nodiscard]] bool skeleton(Skeleton& out);

// Not drawn this frame.
void hide();

// World units per model unit of the posed entity (0 for any other entity).
[[nodiscard]] float modelScale(const entity_t* e);

} // namespace qvr::avatar
