// vr_agpose.cpp -- armed humanoids' pose keys (vr_agpose.hpp).

#include "vr_agpose.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "vr_zancle.hpp"

#include <glm/gtc/quaternion.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace qvr;

namespace
{

// The shipped keys (vr_agrunt_pose_dump prints rows to paste here): the key's code (agpose::code: kind * 100000 +
// class * 1000 + pose * 10 + phase), then its parts as agpose::Part orders them; a code with no row: QC's pose as it
// is, but a gun category's (Pistol .. Widowmaker) with no row of its own: the rifle's (Gun's) row of its kind, pose and
// phase (shippedOf: each category starts as his rifle keys until it is tuned and shipped). An archived key (vr_agrunt_poses) replaces its code's row (agpose::get). Vittorio's, set in the Pose Studio (his
// vr_agrunt_poses of 2026-10-24 14:24, then of 2026-10-25 17:50: 71 keys, approved 2026-10-25; then of 2026-10-27: 20
// keys changed or new, 75 in all), written by Misc/quakevr/pose_defaults.py --cfg <his ironwail.cfg> --write.
// configVersion 202, and again 210, drop the archived keys equal to these (dropShippedEqual: his text then empty).
struct ShippedRow
{
    int code;
    float v[agpose::NumParts * 3];
};
constexpr ShippedRow shipped[] = {
    // grunt, the pickup's legs (any weapon)
    {31, {0.f, 0.f, 6.23f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -1.2f, -0.39f, 0.97f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a gun, Idle / Carry
    {1000, {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.25f, -0.92f, -0.31f, 0.67f, 0.74f, -0.05f, 0.f, 0.f, 0.f}},
    // grunt, a gun, Aim
    {1010, {-3.5f, 7.04f, 6.75f, 0.1f, 3.5f, -4.7f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.01f, -0.98f, -0.25f, -0.5f, 0.85f, -0.16f, 0.f, 0.f, 0.f}},
    // grunt, a gun, Run
    {1020, {4.06f, 0.96f, 0.87f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a gun, Pickup
    {1030, {-1.29f, 5.65f, 15.86f, -6.f, 20.f, 18.2f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.83f, -0.26f, 0.49f, -0.13f, 0.99f, 0.07f, 8.4f, -48.6f, 37.2f}},
    // grunt, a one-handed melee weapon, Idle / Carry
    {2000, {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 4.16f, -2.61f, -0.17f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.96f, 0.12f, -0.27f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Ready
    {2010, {2.06f, -2.85f, 5.51f, 24.9f, 0.f, 0.f, 3.45f, -2.35f, -1.45f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.73f, 0.28f, -0.62f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Run
    {2020, {3.65f, 0.f, 0.f, 0.f, 0.f, 0.f, -2.7f, 2.47f, 0.77f, 0.f, 0.f, 0.f, -0.89f, 0.19f, -0.41f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Pickup
    {2030, {-4.82f, -8.26f, 14.69f, -13.3f, 6.3f, -9.3f, -4.31f, -6.9f, 14.4f, 0.f, 0.f, 0.f, -0.9f, -0.35f, 0.27f, -0.62f, 0.66f, -0.42f, -4.5f, -50.f, 22.4f}},
    // grunt, a one-handed melee weapon, Overhead, wind-up
    {2140, {3.43f, -2.98f, 5.21f, 38.4f, 6.9f, -21.7f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.24f, -0.46f, -0.86f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Overhead, blow
    {2141, {4.4f, -3.21f, 1.07f, 13.2f, -24.2f, -0.5f, 2.78f, 0.f, 3.25f, 0.f, 0.f, 0.f, -0.17f, -0.94f, 0.29f, 0.09f, 0.14f, -0.02f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Overhead, follow-through
    {2142, {2.43f, -0.54f, -10.26f, 28.9f, -18.7f, -25.7f, 5.74f, 0.f, 5.54f, 0.f, 0.f, 0.f, -0.26f, -0.86f, -0.43f, 0.63f, 0.44f, -0.06f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Diagonal, wind-up
    {2150, {2.33f, -7.4f, 3.37f, 4.8f, 2.1f, 24.8f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.29f, -0.02f, -0.91f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Diagonal, blow
    {2151, {1.88f, 1.76f, -2.75f, -5.f, -3.8f, 6.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.09f, -0.98f, -0.17f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Diagonal, follow-through
    {2152, {5.58f, 4.01f, -11.66f, -0.4f, -4.2f, -4.1f, 0.3f, -1.68f, -0.29f, 0.f, 0.f, 0.f, 0.81f, -0.5f, 0.31f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Horizontal, wind-up
    {2160, {4.7f, -8.59f, 0.63f, 85.1f, 82.5f, -12.9f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.5f, 0.34f, -0.29f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Horizontal, blow
    {2161, {3.4f, -1.14f, -7.8f, -7.3f, 4.6f, -33.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Horizontal, follow-through
    {2162, {14.44f, 1.84f, -15.43f, -49.5f, -50.5f, -67.4f, -0.42f, 5.91f, -0.14f, 0.f, 0.f, 0.f, 0.55f, -0.83f, 0.09f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Backhand, wind-up
    {2170, {9.45f, 4.93f, -3.29f, 3.8f, 9.f, -17.5f, -9.79f, 3.67f, -4.63f, 0.f, 0.f, 0.f, 0.36f, -0.18f, -0.35f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Backhand, blow
    {2171, {1.54f, -7.77f, -13.5f, 5.f, 35.3f, 11.f, 1.35f, 3.85f, -2.93f, 0.f, 0.f, 0.f, 0.21f, -0.97f, 0.16f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a one-handed melee weapon, Backhand, follow-through
    {2172, {1.8f, -8.95f, -24.82f, 25.1f, 45.4f, 22.2f, 0.f, -4.47f, 3.19f, 0.f, 0.f, 0.f, -0.62f, -0.77f, -0.16f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Ready
    {5010, {4.62f, -2.16f, 7.14f, -3.5f, 11.4f, 14.9f, 3.02f, 5.07f, 8.85f, 11.4f, 11.9f, 12.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Run
    {5020, {6.53f, 0.83f, 6.98f, -40.4f, -29.f, 20.2f, 7.1f, -0.45f, 7.57f, 33.f, -43.1f, -11.4f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Pickup
    {5030, {1.43f, 0.24f, 5.07f, -12.4f, -9.6f, -10.9f, -2.66f, 12.27f, 6.06f, 3.6f, -1.5f, -5.9f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 22.4f, -50.f, 14.f}},
    // grunt, two melee weapons, Overhead, blow
    {5141, {-0.39f, 2.89f, -0.31f, 19.2f, -20.5f, 9.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.34f, -0.58f, -0.74f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Overhead, follow-through
    {5142, {-0.67f, 3.33f, -2.51f, 64.5f, -60.7f, -26.4f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.07f, -0.98f, -0.22f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Diagonal, wind-up
    {5150, {4.36f, -4.91f, -2.61f, 16.5f, 33.2f, 30.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Diagonal, blow
    {5151, {0.81f, 2.77f, -1.32f, 13.2f, -9.3f, 9.9f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Diagonal, follow-through
    {5152, {3.23f, 0.03f, -4.27f, 11.2f, -34.f, -32.7f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.42f, -0.88f, 0.22f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Horizontal, wind-up
    {5160, {4.48f, -5.52f, -3.81f, 27.5f, 80.8f, 7.7f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Horizontal, blow
    {5161, {-0.88f, -4.f, -4.56f, 3.4f, -5.5f, -18.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Horizontal, follow-through
    {5162, {5.28f, -3.83f, -12.73f, -37.8f, 22.f, -19.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.25f, -0.97f, -0.07f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Backhand, wind-up
    {5170, {4.89f, -0.02f, -5.7f, 27.2f, 10.7f, -20.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Backhand, blow
    {5171, {-1.02f, -1.67f, -8.3f, 10.5f, 3.2f, 22.4f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.11f, -0.95f, 0.29f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Backhand, follow-through
    {5172, {-0.2f, -2.48f, -12.77f, 16.5f, -2.3f, 16.3f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.53f, -0.82f, -0.23f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Dual Strike, wind-up
    {5190, {3.91f, 8.28f, 6.53f, -37.f, 53.4f, -54.f, 0.58f, -6.83f, 7.13f, 15.9f, 51.8f, 50.1f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Dual Strike, blow
    {5191, {2.51f, -0.94f, -1.12f, -56.f, -47.4f, -33.8f, 0.79f, 3.38f, -3.68f, 52.3f, -18.3f, 7.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two melee weapons, Dual Strike, follow-through
    {5192, {16.06f, -13.43f, -25.45f, 2.9f, 15.2f, -78.7f, 4.79f, 11.33f, -18.14f, 8.3f, 23.4f, 49.5f, -0.26f, -0.92f, -0.31f, -0.54f, 0.32f, -0.78f, 0.f, 0.f, 0.f}},
    // grunt, two guns, Idle / Carry
    {6000, {2.51f, 0.27f, -0.98f, 5.4f, 3.9f, 14.3f, 6.15f, -4.52f, -1.37f, 1.4f, 1.2f, -21.6f, -0.97f, -0.18f, -0.14f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, two guns, Aim
    {6010, {0.94f, 1.83f, 1.78f, 0.5f, 3.9f, -2.4f, -0.22f, -2.8f, 1.75f, 2.2f, 3.1f, -4.2f, 0.f, 0.f, 0.f, -0.33f, 0.19f, -0.92f, 0.f, 0.f, 0.f}},
    // grunt, two guns, Run
    {6020, {3.46f, 3.87f, 0.17f, 0.f, 0.f, 0.f, 0.f, -2.95f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // grunt, a pistol, Aim
    {17010, {3.88f, 7.97f, 5.5f, 0.4f, 9.2f, -4.2f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.01f, -0.98f, -0.25f, -0.5f, 0.85f, -0.16f, 0.f, 0.f, 0.f}},
    // enforcer, a gun, Idle / Carry
    {101000, {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.15f, 0.89f, -0.42f, 0.f, 0.f, 0.f}},
    // enforcer, a gun, Aim
    {101010, {12.1f, 6.33f, 6.41f, 2.5f, 3.9f, 13.9f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.45f, -0.7f, -0.55f, -0.83f, 0.1f, -0.55f, 0.f, 0.f, 0.f}},
    // enforcer, a gun, Run
    {101020, {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.2f, 0.11f, 0.39f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Idle / Carry
    {102000, {5.62f, 2.02f, -1.7f, 5.7f, -51.1f, 9.7f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Ready
    {102010, {2.83f, -0.89f, 0.62f, -6.1f, 14.9f, 19.2f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Overhead, wind-up
    {102140, {-5.7f, -7.34f, 7.78f, 47.4f, -6.3f, -36.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Overhead, blow
    {102141, {1.41f, -2.18f, -4.53f, 25.9f, -27.f, 0.7f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.09f, -0.8f, 0.59f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Overhead, follow-through
    {102142, {-0.41f, -0.75f, -9.4f, 66.3f, -50.8f, -18.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.18f, -0.94f, -0.3f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Diagonal, wind-up
    {102150, {10.12f, -8.2f, -0.48f, 29.7f, 28.8f, 21.4f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Diagonal, blow
    {102151, {-0.74f, 2.41f, -5.93f, 20.7f, -10.2f, -6.8f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Diagonal, follow-through
    {102152, {7.11f, -4.65f, -11.28f, -14.9f, -0.1f, -31.5f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.16f, -0.91f, -0.37f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Horizontal, wind-up
    {102160, {5.12f, -8.26f, -6.03f, 75.7f, 83.3f, -13.2f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Horizontal, blow
    {102161, {-2.53f, -2.72f, -5.31f, 4.5f, 25.8f, -5.4f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.13f, -0.87f, -0.48f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Horizontal, follow-through
    {102162, {6.12f, 2.54f, -5.37f, -79.1f, 95.2f, -5.4f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.35f, -0.67f, -0.66f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Backhand, wind-up
    {102170, {2.08f, -2.01f, -9.96f, 7.8f, -17.f, -18.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.02f, -0.29f, -0.91f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Backhand, blow
    {102171, {1.55f, -11.14f, -4.03f, -0.9f, 9.5f, 16.3f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.23f, -0.97f, -0.06f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, a one-handed melee weapon, Backhand, follow-through
    {102172, {-8.8f, -9.21f, -2.33f, 3.1f, -2.2f, -20.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, -0.62f, -0.58f, 0.53f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Idle / Carry
    {105000, {3.45f, 0.37f, 1.73f, -10.6f, -13.4f, 0.6f, 5.58f, -0.13f, 1.08f, 4.3f, -9.1f, -3.4f, 0.f, 0.f, 0.f, -0.63f, 0.67f, -0.38f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Ready
    {105010, {4.17f, -0.09f, 6.8f, 6.4f, 26.3f, 34.5f, 2.16f, 1.04f, 6.36f, -8.8f, 18.1f, -34.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Overhead, wind-up
    {105140, {5.27f, -2.19f, 0.31f, 4.f, 30.3f, -22.1f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.58f, -0.45f, -0.73f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Overhead, blow
    {105141, {-1.45f, 4.14f, -9.45f, -3.7f, -16.7f, -10.9f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Overhead, follow-through
    {105142, {-1.77f, 4.05f, -10.01f, 26.2f, -33.3f, -39.8f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.1f, -0.92f, -0.38f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Diagonal, wind-up
    {105150, {7.32f, -6.86f, -5.67f, -4.8f, 36.5f, 21.4f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.59f, -0.35f, -0.74f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Diagonal, blow
    {105151, {-1.52f, 4.46f, -10.23f, -11.7f, -14.4f, -7.4f, -8.81f, 10.03f, -3.08f, -24.f, 12.4f, 13.9f, 0.22f, -0.87f, -0.43f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Diagonal, follow-through
    {105152, {2.7f, -2.63f, -14.03f, -19.2f, -19.4f, -17.8f, -3.06f, 6.47f, 6.79f, -11.8f, -11.f, -2.f, 0.39f, -0.92f, 0.05f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Horizontal, wind-up
    {105160, {9.4f, -5.27f, -4.9f, 11.3f, 88.8f, -6.2f, -0.13f, 11.03f, 3.44f, 1.3f, 2.f, 8.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Horizontal, blow
    {105161, {0.36f, -1.17f, -11.69f, -7.7f, -6.8f, -18.1f, -3.25f, 7.78f, 1.3f, -8.7f, -0.8f, 2.5f, 0.34f, -0.86f, -0.38f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Horizontal, follow-through
    {105162, {8.76f, -1.15f, -17.03f, -39.f, 52.2f, -57.8f, -3.81f, 3.49f, 9.77f, 36.1f, -3.8f, -17.2f, 0.03f, -0.82f, -0.57f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Backhand, wind-up
    {105170, {5.19f, 3.08f, -2.95f, 18.9f, 10.4f, -34.5f, -8.37f, 8.03f, 4.03f, -0.1f, 8.2f, 13.6f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Backhand, blow
    {105171, {1.93f, -3.79f, -13.39f, 16.3f, 1.4f, -5.6f, -0.17f, 5.12f, 0.25f, 11.4f, 7.5f, -1.3f, -0.01f, -0.84f, -0.55f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Backhand, follow-through
    {105172, {-7.53f, -7.57f, -18.87f, 9.3f, -22.7f, -13.2f, -0.35f, -8.41f, -3.42f, 20.4f, 4.4f, -24.4f, -0.86f, -0.49f, -0.12f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Dual Strike, wind-up
    {105190, {5.45f, 8.66f, 10.11f, -76.9f, 46.7f, -35.7f, 1.14f, -5.21f, 11.06f, 70.7f, 65.8f, 46.1f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Dual Strike, blow
    {105191, {-0.16f, -1.13f, -17.36f, -36.4f, -76.2f, -28.2f, -0.54f, 0.81f, -15.84f, 51.1f, -74.1f, 55.1f, -0.02f, -0.8f, -0.6f, -0.08f, 1.f, -0.03f, 0.f, 0.f, 0.f}},
    // enforcer, two melee weapons, Dual Strike, follow-through
    {105192, {12.89f, -20.68f, -30.38f, -33.6f, -0.2f, -90.3f, 3.35f, 20.39f, -21.96f, 30.f, 41.4f, 73.f, -0.38f, -0.9f, -0.22f, -0.29f, 0.96f, -0.05f, 0.f, 0.f, 0.f}},
    {-1, {}}, // (the end)
};

// QC's grunt-made fit of each kind (vr_armedmon.qc VR_AM_Row: vr_am_fit_scale): a place on the grunt times this is the
// enforcer's.
constexpr glm::vec3 fitScale[agpose::numKinds] = {glm::vec3{1.f, 1.f, 1.f}, glm::vec3{1.f, 1.35f, 1.1f}};

struct Entry
{
    int code{-1};
    agpose::Key k;
};

constexpr int maxEntries = 512;

struct State
{
    za::Vector<Entry> over; // vr_agrunt_poses parsed
    za::String parsed;
    bool everParsed{false};
    int previewCode{-1};
    agpose::Key previewKey;
};
State state;

[[nodiscard]] bool validCode(int c)
{
    return c >= 0 && agpose::kindOf(c) < agpose::numKinds && agpose::isClass(agpose::classOf(c)) &&
           agpose::poseOf(c) < agpose::NumPoses && agpose::phaseOf(c) < agpose::numPhases;
}

// (an old-format code read as the new: agpose::codeFromOld; written back new by the next write)
void parse()
{
    const char* text = vr_agrunt_poses.string ? vr_agrunt_poses.string : "";
    if(state.everParsed && strcmp(state.parsed.cStr(), text) == 0)
    {
        return;
    }
    state.everParsed = true;
    state.parsed = za::String{text};
    state.over.clear();
    const char* p = text;
    while(*p && static_cast<int>(state.over.size()) < maxEntries)
    {
        Entry e;
        char* end = nullptr;
        e.code = agpose::codeFromOld(static_cast<int>(strtol(p, &end, 10)));
        bool ok = end != p;
        p = end;
        for(int i = 0; i < agpose::NumParts * 3 && ok; i++)
        {
            const float f = strtof(p, &end);
            if(end == p && i >= agpose::numOldParts * 3)
            {
                break; // (an entry from before the off-hand parts: the rest zero, no change and Auto)
            }
            ok = end != p;
            e.k.v[i / 3][i % 3] = f;
            p = end;
        }
        if(ok && validCode(e.code))
        {
            state.over.pushBack(e);
        }
        const char* semi = strchr(p, ';');
        if(!semi)
        {
            break;
        }
        p = semi + 1;
    }
}

// How many of key `k`'s parts its text and dump rows carry: the first seven, the off hand's too when any is set (older
// builds read the first seven and skip the rest of an entry).
[[nodiscard]] int partsOf(const agpose::Key& k)
{
    for(int i = agpose::numOldParts; i < agpose::NumParts; i++)
    {
        if(k.v[i] != glm::vec3{0.f})
        {
            return agpose::NumParts;
        }
    }
    return agpose::numOldParts;
}

void write()
{
    za::String text;
    for(const Entry& e : state.over)
    {
        text += va("%d", e.code);
        for(int i = 0; i < partsOf(e.k) * 3; i++)
        {
            text += va(" %g", static_cast<double>(e.k.v[i / 3][i % 3]));
        }
        text += ";";
    }
    Cvar_SetQuick(&vr_agrunt_poses, text.cStr());
    parse();
}

[[nodiscard]] bool shippedRow(int code, agpose::Key& out)
{
    for(const ShippedRow& r : shipped)
    {
        if(r.code == code && r.code >= 0)
        {
            for(int i = 0; i < agpose::NumParts * 3; i++)
            {
                out.v[i / 3][i % 3] = r.v[i];
            }
            return true;
        }
    }
    return false;
}

// The rifle's (Gun's) code for a gun category's (the same kind, pose and phase); -1 for any other class.
[[nodiscard]] int rifleOf(int code)
{
    const int cls = agpose::classOf(code);
    return cls != agpose::Gun && agpose::isGunCategory(cls)
               ? agpose::code(agpose::kindOf(code), agpose::Gun, agpose::poseOf(code), agpose::phaseOf(code))
               : -1;
}

// Code `code`'s shipped key: its row, else a gun category's rifle row.
[[nodiscard]] bool shippedOf(int code, agpose::Key& out)
{
    if(shippedRow(code, out))
    {
        return true;
    }
    const int rifle = rifleOf(code);
    return rifle >= 0 && shippedRow(rifle, out);
}

[[nodiscard]] float rounded(float f, float per)
{
    return static_cast<float>(static_cast<int>(f * per + (f < 0.f ? -0.5f : 0.5f))) / per;
}

// Whether key `code`'s part is a turn (degrees: kept to 0.1) or a place (units: 0.01); the legs' key: its pelvis's turn.
[[nodiscard]] bool isTurn(int code, int part)
{
    if(agpose::isLegCode(code))
    {
        return part == agpose::LegPelvisTurn;
    }
    return part == agpose::RTurn || part == agpose::LTurn || part == agpose::Torso || part == agpose::GripTurn;
}

[[nodiscard]] bool same(int code, const agpose::Key& a, const agpose::Key& b)
{
    for(int i = 0; i < agpose::NumParts; i++)
    {
        const float eps = isTurn(code, i) ? 0.05f : 0.005f;
        for(int c = 0; c < 3; c++)
        {
            if(za::abs(a.v[i][c] - b.v[i][c]) > eps)
            {
                return false;
            }
        }
    }
    return true;
}

void dropCode(int code)
{
    za::Vector<Entry> kept;
    for(const Entry& e : state.over)
    {
        if(e.code != code)
        {
            kept.pushBack(e);
        }
    }
    state.over = static_cast<za::Vector<Entry>&&>(kept);
}

// QC constants that are a stance's place (the grunt's: vr_agarms' pose places them from its chest, fitted): the key's
// part moves them by its change, exactly (its direction turned by the part's turn).
struct QcConstant
{
    int cls, pose, part;
    const char* name;
    glm::vec3 value;
    const char* fwName; // ("" none)
    glm::vec3 fw;
};
constexpr QcConstant qcConstants[] = {
    {agpose::Melee, agpose::Carry, agpose::RPos, "VR_AGP_CARRY_R", {6.f, -9.f, 3.f}, "VR_AGP_CARRY_R_FW", {0.95f, -0.1f, 0.15f}},
    {agpose::Melee2, agpose::Carry, agpose::RPos, "VR_AGP_PORT", {9.f, -5.f, 3.f}, "VR_AGP_PORT_FW", {0.35f, 0.55f, 0.75f}},
    {agpose::Melee, agpose::Aim, agpose::RPos, "VR_AGP_READY", {10.f, -7.f, 4.f}, "VR_AGP_READY_FW", {0.3f, 0.1f, 0.95f}},
    {agpose::Melee, agpose::Aim, agpose::LPos, "VR_AGP_FREE_L", {7.f, 9.f, 4.f}, "", {}},
    {agpose::Fists, agpose::Aim, agpose::RPos, "VR_AGP_GUARD_R", {9.f, -5.f, 13.f}, "", {}},
    {agpose::Fists, agpose::Aim, agpose::LPos, "VR_AGP_GUARD_L", {11.f, 5.f, 15.f}, "", {}},
};

[[nodiscard]] glm::vec3 turned(const glm::vec3& v, const glm::vec3& rotDeg)
{
    const float a = glm::length(rotDeg);
    if(a < 1e-4f)
    {
        return v;
    }
    return glm::angleAxis(a * (3.14159265f / 180.f), rotDeg / a) * v;
}

// vr_agrunt_pose_key <code> [<21 numbers> | reset]: key `code` printed; set (as the Pose Studio's drag keeps it); or
// its archived change dropped (the shipped key again). Old-format codes are read as the new (codeFromOld).
void key_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_agrunt_pose_key <code> [<21 numbers> | reset]\n");
        return;
    }
    const int c = agpose::codeFromOld(Q_atoi(Cmd_Argv(1)));
    if(!validCode(c))
    {
        Con_Printf("vr_agrunt_pose_key: %d is no key's code\n", c);
        return;
    }
    if(Cmd_Argc() == 3 && !strcmp(Cmd_Argv(2), "reset"))
    {
        parse();
        dropCode(c);
        write();
    }
    else if(Cmd_Argc() >= 2 + agpose::numOldParts * 3)
    {
        // (21 numbers, the first seven parts: the off hand's left as zero; or all 30)
        agpose::Key k;
        for(int i = 0; i < agpose::NumParts * 3 && 2 + i < Cmd_Argc(); i++)
        {
            k.v[i / 3][i % 3] = static_cast<float>(Q_atof(Cmd_Argv(2 + i)));
        }
        agpose::set(c, k);
    }
    const agpose::Key k = agpose::value(c);
    Con_Printf("agpose key %d (%s, %s, %s): R %g %g %g turn %g %g %g\n", c, agpose::kindName(agpose::kindOf(c)),
        agpose::className(agpose::classOf(c)), agpose::poseName(agpose::classOf(c), agpose::poseOf(c)),
        static_cast<double>(k.v[0].x), static_cast<double>(k.v[0].y), static_cast<double>(k.v[0].z),
        static_cast<double>(k.v[1].x), static_cast<double>(k.v[1].y), static_cast<double>(k.v[1].z));
    Con_Printf("agpose key %d off hand: %s, grip offset %g %g %g turn %g %g %g, free L %g %g %g turn %g %g %g\n", c,
        agpose::offModeName(agpose::offModeOf(k.v[agpose::OffHand])), static_cast<double>(k.v[agpose::GripPos].x),
        static_cast<double>(k.v[agpose::GripPos].y), static_cast<double>(k.v[agpose::GripPos].z),
        static_cast<double>(k.v[agpose::GripTurn].x), static_cast<double>(k.v[agpose::GripTurn].y),
        static_cast<double>(k.v[agpose::GripTurn].z), static_cast<double>(k.v[agpose::LPos].x),
        static_cast<double>(k.v[agpose::LPos].y), static_cast<double>(k.v[agpose::LPos].z),
        static_cast<double>(k.v[agpose::LTurn].x), static_cast<double>(k.v[agpose::LTurn].y),
        static_cast<double>(k.v[agpose::LTurn].z));
}

void dump_f()
{
    za::String text;
    agpose::dumpText(text);
    Con_Printf("%s", text.cStr());
}

} // namespace

namespace qvr::agpose
{

bool get(int c, Key& out)
{
    out = Key{};
    if(c == state.previewCode && c >= 0)
    {
        out = state.previewKey;
        return true;
    }
    parse();
    for(const Entry& e : state.over)
    {
        if(e.code == c)
        {
            out = e.k;
            return true;
        }
    }
    return shippedOf(c, out);
}

Key value(int c)
{
    Key k;
    (void)get(c, k);
    return k;
}

void setPreview(int c, const Key& k)
{
    state.previewCode = c;
    state.previewKey = k;
}

void clearPreview()
{
    state.previewCode = -1;
}

void set(int c, const Key& k)
{
    if(!validCode(c))
    {
        return;
    }
    parse();
    Key r;
    for(int i = 0; i < NumParts; i++)
    {
        for(int a = 0; a < 3; a++)
        {
            r.v[i][a] = rounded(k.v[i][a], isTurn(c, i) ? 10.f : 100.f);
        }
    }
    Key ship;
    const bool hasShip = shippedOf(c, ship);
    dropCode(c);
    if(!same(c, r, hasShip ? ship : Key{}) && static_cast<int>(state.over.size()) < maxEntries)
    {
        state.over.pushBack(Entry{c, r});
    }
    write();
}

void resetPose(int kind, int cls, int pose)
{
    parse();
    for(int ph = 0; ph < numPhases; ph++)
    {
        dropCode(code(kind, cls, pose, ph));
    }
    write();
}

void resetLegs(int kind)
{
    parse();
    dropCode(legCode(kind, Pickup));
    write();
}

int copyKind(int from, int to)
{
    if(from < 0 || from >= numKinds || to < 0 || to >= numKinds || from == to)
    {
        return 0;
    }
    parse();
    // The keys `from` has in effect (archived, else shipped), by their code within the kind; and the codes `to` ships
    // (each then the copy's, or none: its shipped key overridden by zeros where `from` has none).
    za::Vector<int> codes;
    const auto note = [&codes](int within) {
        for(const int c : codes)
        {
            if(c == within)
            {
                return;
            }
        }
        codes.pushBack(within);
    };
    for(const Entry& e : state.over)
    {
        if(kindOf(e.code) == from)
        {
            note(withinKind(e.code));
        }
    }
    for(const ShippedRow& r : shipped)
    {
        if(r.code >= 0 && (kindOf(r.code) == from || kindOf(r.code) == to))
        {
            note(withinKind(r.code));
            if(classOf(r.code) == Gun)
            {
                // (a rifle's shipped key is each gun category's with no row of its own: theirs copied too)
                for(const int cat : gunCategories)
                {
                    note(code(0, cat, poseOf(r.code), phaseOf(r.code)));
                }
            }
        }
    }
    za::Vector<Entry> kept;
    for(const Entry& e : state.over)
    {
        if(kindOf(e.code) != to)
        {
            kept.pushBack(e);
        }
    }
    int n = 0;
    const glm::vec3 s = fitScale[to] / fitScale[from];
    for(const int within : codes)
    {
        Entry c;
        c.code = to * kindStride + within;
        const bool has = get(from * kindStride + within, c.k) && !same(c.code, c.k, Key{});
        c.k.v[RPos] *= s;
        c.k.v[LPos] *= s;
        if(isLegCode(c.code))
        {
            c.k.v[LegKneelKnee] *= s; // (the legs' places: all but the pelvis's turn)
            c.k.v[LegKneelFoot] *= s;
            c.k.v[LegPlantKnee] *= s;
        }
        Key ship;
        const bool hasShip = shippedOf(c.code, ship);
        if(!same(c.code, c.k, hasShip ? ship : Key{}) && static_cast<int>(kept.size()) < maxEntries)
        {
            kept.pushBack(c);
        }
        n += has ? 1 : 0;
    }
    state.over = static_cast<za::Vector<Entry>&&>(kept);
    write();
    return n;
}

int dropShippedEqual()
{
    parse();
    const char* text = vr_agrunt_poses.string ? vr_agrunt_poses.string : "";
    int entries = 0; // (each ';'-ended, and a last one without)
    bool open = false;
    for(const char* p = text; *p; p++)
    {
        if(*p == ';')
        {
            entries++;
            open = false;
        }
        else if(*p != ' ')
        {
            open = true;
        }
    }
    entries += open ? 1 : 0;
    if(entries != static_cast<int>(state.over.size()))
    {
        return 0;
    }
    za::Vector<Entry> kept;
    for(const Entry& e : state.over)
    {
        Key ship;
        const bool hasShip = shippedOf(e.code, ship);
        if(!same(e.code, e.k, hasShip ? ship : Key{}))
        {
            kept.pushBack(e);
        }
    }
    const int dropped = static_cast<int>(state.over.size() - kept.size());
    if(dropped > 0)
    {
        state.over = static_cast<za::Vector<Entry>&&>(kept);
        write();
    }
    return dropped;
}

int migrateCategories(int* widened)
{
    parse();
    // (the old codes: parse read them as the new, write puts them so)
    int old = 0;
    const char* p = vr_agrunt_poses.string ? vr_agrunt_poses.string : "";
    while(*p)
    {
        char* end = nullptr;
        const int c = static_cast<int>(strtol(p, &end, 10));
        old += end != p && codeFromOld(c) != c ? 1 : 0;
        const char* semi = strchr(p, ';');
        if(!semi)
        {
            break;
        }
        p = semi + 1;
    }
    if(widened)
    {
        *widened = old;
    }
    za::Vector<Entry> added;
    for(const Entry& e : state.over)
    {
        if(classOf(e.code) != Gun)
        {
            continue;
        }
        for(const int cat : gunCategories)
        {
            const int c = code(kindOf(e.code), cat, poseOf(e.code), phaseOf(e.code));
            bool has = cat == Gun;
            for(const Entry& o : state.over)
            {
                has = has || o.code == c;
            }
            if(!has)
            {
                added.pushBack(Entry{c, e.k});
            }
        }
    }
    for(const Entry& a : added)
    {
        if(static_cast<int>(state.over.size()) < maxEntries)
        {
            state.over.pushBack(a);
        }
    }
    if(old > 0 || !added.empty())
    {
        write();
    }
    return static_cast<int>(added.size());
}

void dumpText(za::String& out)
{
    parse();
    out += "// vr_agrunt_pose_dump: rows for Quake/vr/vr_agpose.cpp's shipped[] (the changes laid on QC's poses):\n";
    out += "// {code (kind*100000 + class*1000 + pose*10 + phase), {right place, right turn, left place, left turn,\n";
    out += "//  right elbow's pole, left elbow's pole, torso yaw pitch roll[, off hand's grip place, its grip turn (the gun's\n";
    out += "//  frame), off hand mode 0 Auto 1 On the Gun 2 Free, 0, 0: when any is set]}}\n";
    for(const Entry& e : state.over)
    {
        const int kind = kindOf(e.code), cls = classOf(e.code), pose = poseOf(e.code), phase = phaseOf(e.code);
        out += va("    {%d, {", e.code);
        for(int i = 0; i < partsOf(e.k) * 3; i++)
        {
            out += va("%s%.*ff", i ? ", " : "", isTurn(e.code, i / 3) ? 1 : 2, static_cast<double>(e.k.v[i / 3][i % 3]));
        }
        if(isLegCode(e.code))
        {
            // (vr_aglegs: pelvis place, pelvis turn, planted foot, kneeling knee's pole, kneeling foot, planted knee's pole)
            out += va("}}, // %s, the pickup's legs (any weapon)\n", kindName(kind));
            continue;
        }
        out += va("}}, // %s, %s, %s%s%s\n", kindName(kind), className(cls), poseName(cls, pose),
            isStrike(pose) ? ", " : "", isStrike(pose) ? phaseName(phase) : "");
        // (a stance whose place is one QC constant: that constant moved, instead of the row; the grunt's units)
        for(const QcConstant& q : qcConstants)
        {
            if(q.cls != cls || q.pose != pose || phase != 0)
            {
                continue;
            }
            const glm::vec3 d = e.k.v[q.part] / fitScale[kind];
            const glm::vec3 v = q.value + d;
            out += va("    //   QC instead (%s): vector %s = '%.2f %.2f %.2f';", kindName(kind), q.name,
                static_cast<double>(v.x), static_cast<double>(v.y), static_cast<double>(v.z));
            if(q.fwName[0])
            {
                const glm::vec3 fw = turned(glm::normalize(q.fw), e.k.v[q.part + 1]);
                out += va(" vector %s = '%.2f %.2f %.2f';", q.fwName, static_cast<double>(fw.x), static_cast<double>(fw.y),
                    static_cast<double>(fw.z));
            }
            out += "\n";
        }
    }
    if(state.over.empty())
    {
        out += "// (no pose keys changed)\n";
    }
}

const char* kindName(int kind)
{
    return kind == 0 ? "grunt" : kind == 1 ? "enforcer" : "?";
}

const char* kindsName(int kind)
{
    return kind == 0 ? "grunts" : kind == 1 ? "enforcers" : "?";
}

const char* kindTitle(int kind)
{
    return kind == 0 ? "Grunt" : kind == 1 ? "Enforcer" : "?";
}

const char* offModeName(int mode)
{
    return mode == OffGun ? "On the Gun" : mode == OffFree ? "Free" : "Auto (the Weapon Holds)";
}

const char* className(int cls)
{
    switch(cls)
    {
        case Fists: return "fists";
        case Gun: return "a rifle";
        case Melee: return "a one-handed melee weapon";
        case Melee2: return "a two-handed melee weapon";
        case Saw: return "a chainsaw";
        case TwoMelee: return "two melee weapons";
        case TwoGuns: return "two guns";
        case Pistol: return "a pistol";
        case Launcher: return "a shoulder launcher";
        case Sprayer: return "a sprayer";
        case LaserCannon: return "the laser cannon";
        case Widowmaker: return "the Widowmaker";
        default: return "?";
    }
}

const char* classTitle(int cls)
{
    switch(cls)
    {
        case Fists: return "Fists";
        case Gun: return "Rifle";
        case Melee: return "One-Handed Melee";
        case Melee2: return "Two-Handed Melee";
        case Saw: return "Chainsaw";
        case TwoMelee: return "Two Melee Weapons";
        case TwoGuns: return "Two Guns";
        case Pistol: return "Pistol";
        case Launcher: return "Shoulder Launcher";
        case Sprayer: return "Sprayer";
        case LaserCannon: return "Laser Cannon";
        case Widowmaker: return "Widowmaker";
        default: return "?";
    }
}

const char* poseName(int cls, int pose)
{
    switch(pose)
    {
        case Carry: return cls == Fists ? "Idle" : cls == Melee2 ? "Port Arms" : "Idle / Carry";
        case Aim: return cls == Fists ? "Guard" : isGunClass(cls) ? "Aim" : "Ready";
        case Run: return "Run";
        case Pickup: return "Pickup";
        case Jab: return "Jab";
        case Cross: return "Cross";
        case Hook: return "Hook";
        case Uppercut: return "Uppercut";
        case Overhead: return "Overhead";
        case Diagonal: return "Diagonal";
        case Horizontal: return "Horizontal";
        case Backhand: return "Backhand";
        case SawThrust: return "Saw Thrust";
        case Dual: return "Dual Strike";
        default: return "?";
    }
}

const char* phaseName(int phase)
{
    return phase == 0 ? "wind-up" : phase == 1 ? "blow" : "follow-through";
}

int posesOf(int cls, int* out, int max)
{
    int n = 0;
    const auto add = [&](int p) {
        if(n < max)
        {
            out[n++] = p;
        }
    };
    add(Carry);
    add(Aim);
    add(Run);
    if(cls != Fists)
    {
        add(Pickup);
    }
    if(cls == Fists)
    {
        add(Jab);
        add(Cross);
        add(Hook);
        add(Uppercut);
    }
    if(cls == Melee || cls == Melee2 || cls == TwoMelee)
    {
        add(Overhead);
        add(Diagonal);
        add(Horizontal);
        add(Backhand);
    }
    if(cls == Saw)
    {
        add(SawThrust);
    }
    if(cls == TwoMelee)
    {
        add(Dual);
    }
    return n;
}

void registerCommands()
{
    Cmd_AddCommand("vr_agrunt_pose_dump", dump_f);
    Cmd_AddCommand("vr_agrunt_pose_key", key_f);
}

} // namespace qvr::agpose
