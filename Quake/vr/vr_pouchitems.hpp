// vr_pouchitems.hpp -- what the ammo pouch holds (immersive reloading, vr_reload_mode 3; ROUND22.md, "Ammo pouch
// contents"): the rounds' own held models at their held sizes (props' Size: they match the ones in a hand), laid out in
// the pouch's model space (make_ammo_pouch.py: +x out of the body, +y left, +z up, units at its scale 1) by what it gives
// and how much is left, and wobbling as the pouch moves:
//   shells   a loose pile, lying every way, never through each other, gathering in the middle (one a shell, up to
//            vr_ammo_pouch_shells_max, 20);
//   magazines (the nailgun's, the super nailgun's, cells) standing, a row of up to three in the middle, then two rows
//            of three (up to vr_ammo_pouch_mags_max, 6), each row centred; the reload finds them where they stand
//            (QC vr_reload.qc VR_Reload_PouchMagTop: magSlot's layout through the rounds' box);
//   grenades standing, a row of up to three, then two more in front in the back row's gaps (up to 5);
//   rockets  lying along it in a zig-zag stack against its back and front, nose either way (up to 5).
// The leather's frame (make_ammo_pouch.py FRAMES) stretches round them: shapeFor. The wobble: the pouch's own
// acceleration (the body's walking and turning, the hips' IK) drives a damped spring a frame; standing rounds lean about
// their foot, lying ones shift, the higher in the pile the more (vr_ammo_pouch_wobble; Debug's Wobble Test shakes it).

#pragma once

#include "vr_engine.hpp"

namespace qvr::pouchitems
{

inline constexpr int most = 20;    // rounds drawn at most (the shells' pile)
inline constexpr int magsMost = 6; // magazines

// The pouch's frame (its leather's shape) for what it gives (STAT_QVR_POUCHKIND & 7, with some left; 0 empty).
[[nodiscard]] int frameFor(int kind, int left);
struct Shape
{
    float halfW, depth, floor; // half its width, its depth out of its back at the middle, the inside's floor
};
[[nodiscard]] const Shape& shapeOf(int frame);
[[nodiscard]] float innerCentreX(float depth); // the middle of its inside, out of the body

// Magazine `j` of `n` for kind 2..4 (the nailgun's, the super nailgun's, cells) where it stands (its top's middle: x out
// of the body, y left; its top at magTop), as QC VR_Reload_PouchMagTop finds it. magsShown: how many `left` rounds show.
inline constexpr float magTop = 2.9f;
[[nodiscard]] glm::vec2 magSlot(int kind, int j, int n);
[[nodiscard]] int magsShown(int kind, int left);

struct Item
{
    qmodel_t* model{nullptr};
    int skin{0};
    glm::vec3 at{0.f};        // its model's origin (pouch units)
    glm::mat3 axes{1.f};      // its model's x, y, z in the pouch's axes (unit)
    float share{1.f};         // its drawn scale as a share of its held one (grenades: a little smaller)
    float size{1.f};          // pouch units a model unit (share times its held size)
    glm::vec3 pivot{0.f};     // where it wobbles about (a standing one's foot; a lying one's middle)
    float height{1.f};        // a standing one's top above its pivot
    float gain{1.f};          // its share of the wobble
    bool lying{false};
    // Its collision shape (layoutCheck): a capsule from p0 to p1 (model units) of radius `radius` (pouch units); a
    // magazine a box (model units: lo..hi).
    bool box{false};
    glm::vec3 p0{0.f}, p1{0.f}, lo{0.f}, hi{0.f};
    float radius{0.f};
};

// The rounds for kind `kind` (1..7; `variant` STAT_QVR_POUCHKIND's 8: skin 1), `left` of them in reserve, at rest:
// how many.
[[nodiscard]] int layout(int kind, bool variant, int left, Item (&out)[most]);

// Once a frame (the pouch placed): its origin and unit axes (world) and world units a pouch unit; the wobble's spring
// steps by the time since.
void wobbleStep(const glm::vec3& origin, const glm::mat3& axes, float k);
// The wobble now (pouch units: how far the rounds lag, out, left, up), and an item moved by it.
[[nodiscard]] glm::vec3 wobble();
void applyWobble(Item& it, const glm::vec3& s);

void init(); // vr_ammo_pouch_layout

} // namespace qvr::pouchitems
