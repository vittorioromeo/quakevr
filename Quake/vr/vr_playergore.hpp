#pragma once

// vr_playergore.hpp -- your own gore as the enemies' (vr_player_gore; ROUND24.md, "Player gore parity"; experimental):
// your body, arms and hands bleed from where you are hit, through the enemies' own code paths. The spot is the one the
// wounds paint (vr_wounds.cpp's strike on your capsules, selfshot::build: the torso, the legs, each forearm, each hand;
// your own shot's exact point), bound to its capsule as drawn (Spot) and carried with it:
//  - a blood stream from it (vr_player_gore_streams: bloodstream::startOwn, the enemies' streams; a shot, a nail, a blade
//    at vr_player_gore_stream_chance, vr_player_gore_stream_time of their time, vr_player_gore_stream_max at once);
//  - drips from it while you are hurt (vr_player_gore_drips: the gore's drops, as a wounded enemy's: vr_drips_blood_rate
//    * vr_player_gore_drip_rate * (vr_drips_blood_min + the rest times the health lost); your last four wounds);
//  - a splash of drops out of it at the hit (vr_player_gore_splash: vr_player_gore_splash_amount).
// The painted wounds themselves (vr_wounds) and your arms' own drips (vr_body_blood) are theirs, as before. Nothing with
// vr_gore 0; nothing pours, splashes or drips close to the eyes or over them (vr_player_gore_eye_clear). No head: the
// wounds' capsules have none (a hit there paints nothing, so nothing bleeds there either).

#include <glm/glm.hpp>

namespace qvr::playergore
{

enum class Effect : int
{
    Streams,
    Drips,
    Splash
};

// Whether `effect` is on: vr_player_gore and its own toggle, and the gore on.
[[nodiscard]] bool on(Effect effect);

// A point on one of your capsules (selfshot::Part), carried with it as drawn: how far along its axis, and its offset
// from the axis in the axis' frame (turning with your view's yaw), as a share of its radius.
// Also, where the drawn body is posed (vr_avatar.cpp), the body's own skinned surface nearest it: three vertices and the
// point's barycentrics on them, so its blood pours from the drawn skin as it moves (his 2026-11-10: the capsule's surface
// floated off the flatter body), not from the capsule.
struct Spot
{
    int part{-1};
    float along{0.f};
    glm::vec3 offset{0.f};
    bool onSkin{false};
    unsigned vert[3]{0u, 0u, 0u};
    glm::vec3 bary{1.f, 0.f, 0.f};
};

// `at` on your capsule `part` as drawn now: its spot. False if that part is not drawn.
[[nodiscard]] bool bind(const glm::vec3& at, int part, Spot& out);

// Where `spot` is drawn now and the surface's outward normal there; false if its part is not drawn.
[[nodiscard]] bool where(const Spot& spot, glm::vec3& at, glm::vec3& normal);

// Too close to the eyes for blood to pour, splash or drip from (vr_player_gore_eye_clear): close in front, or over them.
[[nodiscard]] bool nearEyes(const glm::vec3& p);
// ... by a clearance of `clearM` metres (0: nothing is; the test's).
[[nodiscard]] bool nearEyesBy(const glm::vec3& p, float clearM);

// A wound on you struck (vr_wounds.cpp, as it is painted): at `at` (normal `n`) on capsule `part`, the blow going `way`,
// wound kind `kind` (QVR_WOUND_*), `amount` its damage. Its stream, drips and splash.
void onWound(const glm::vec3& at, const glm::vec3& n, const glm::vec3& way, int part, int kind, int amount);

// Once a frame (vr_view.cpp, after the wounds): the drips.
void frame();

// All gone (a new map).
void clear();

// vr_player_gore_info, vr_player_gore_test.
void registerCommands();

} // namespace qvr::playergore
