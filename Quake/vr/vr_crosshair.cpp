// vr_crosshair.cpp -- see vr_crosshair.hpp. From the old engine's show_crosshair (vr_showfn.cpp).
//
// vr_crosshair: 0 none, 1 a dot where the aim meets a wall, 2 a laser from the muzzle to the
// first thing it hits, 3 the same laser fading in and out. vr_crosshair_depth > 0 puts it at a
// fixed distance instead; vr_crosshair_size and vr_crosshair_alpha size and fade it. Weapons
// whose crosshair mode is "forbidden" (melee) have none.

#include "vr_crosshair.hpp"
#include "vr_engine.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_hue.hpp"
#include "vr_lines.hpp"
#include "vr_protocol.hpp"
#include "vr_trace.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Math/Fmax.hpp"

namespace qvr::crosshair
{
namespace
{

enum Mode : int
{
    NONE = 0,
    POINT = 1,
    LINE = 2,
    LINE_SMOOTH = 3
};

// Where the aim from `start` along `dir` meets the world (walls only, or also entities).
[[nodiscard]] glm::vec3 aimEnd(const glm::vec3& start, const glm::vec3& dir, bool entities)
{
    if(vr_crosshair_depth.value > 0.f)
    {
        return start + dir * vr_crosshair_depth.value;
    }

    const glm::vec3 farEnd = start + dir * 4096.f;
    const auto tr = worldtrace::move(start, glm::vec3{0.f}, glm::vec3{0.f}, farEnd, entities ? MOVE_NORMAL : MOVE_NOMONSTERS);
    return tr ? worldtrace::endPos(*tr) : farEnd; // not hosting: no world to trace against
}

// The frame flatscreen play's crosshairs were last queued (VR_SetupViewEntities may run more than once a frame).
int flatQueuedFrame = -1;

// Gun `h`'s muzzle as drawn (flatscreen play: the drawn weapon stands off the game's hand by the looks: sway, bob, the
// sights), else the game's.
[[nodiscard]] glm::vec3 drawnMuzzle(const hands::State& s, int h)
{
    const view::ViewEntity* ve = view::heldWeapon(h);
    const int slot = ve ? weapons::slotForModel(ve->ent.model) : -1;
    if(slot < 0)
    {
        return s.muzzle[h];
    }
    return view::anchorPosition(*ve, weapons::anchor(slot, weapons::Key::MuzzleAnchor),
        weapons::vec(slot, weapons::Key::MuzzleOffsetX, weapons::Key::MuzzleOffsetY, weapons::Key::MuzzleOffsetZ));
}

void queueFor(const hands::State& s, bool flat)
{
    const int mode = static_cast<int>(vr_crosshair.value);
    const float size = CLAMP(0.f, vr_crosshair_size.value, 32.f);
    const float alpha = CLAMP(0.f, vr_crosshair_alpha.value, 1.f);
    if(mode == NONE || size <= 0.f || alpha <= 0.f || !s.valid)
    {
        return;
    }

    // In the player's hue (vr_crosshair_hue; 0 its old red).
    const glm::vec4 red = hue::color(vr_crosshair_hue, 1.f, 1.f, alpha);
    const glm::vec4 faded{glm::vec3{red}, alpha * 0.01f};

    for(int h = 0; h < HAND_COUNT; h++)
    {
        const int slot = weapons::heldSlot(h);
        if(!s.muzzleValid[h] || slot < 0 || slot == weapons::fistSlot() ||
            weapons::value(slot, weapons::Key::CrosshairMode) == 1.f)
        {
            continue;
        }

        // Where shots go: from the game's muzzle along its aim. Flatscreen: the laser starts at the gun as drawn.
        const glm::vec3 shotFrom = s.muzzle[h];
        const glm::vec3 dir = hands::forward(weapons::shotAngles(s.rot[h], slot, h == HAND_OFF));
        const glm::vec3 start = flat ? drawnMuzzle(s, h) : shotFrom;
        const auto line = flat ? lines::sceneOverLine : lines::line;

        if(mode == POINT)
        {
            glm::vec3 end = aimEnd(shotFrom, dir, false);
            end.z += vr_crosshairy.value;
            const float dotSize = size * za::fmax(1.f, glm::distance(start, end) * 0.01f);
            (flat ? lines::sceneOverPoint : lines::point)(end, dotSize, red);
            continue;
        }

        glm::vec3 end = aimEnd(shotFrom, dir, true);
        end.z += vr_crosshairy.value * 10.f;

        const float width = size * 0.3f;
        if(mode == LINE)
        {
            line(start, end, width, red, red);
        }
        else
        {
            const glm::vec3 midA = glm::mix(start, end, 0.15f);
            const glm::vec3 midB = glm::mix(start, end, 0.7f);
            line(start, midA, width, faded, red);
            line(midA, midB, width, red, red);
            line(midB, end, width, red, faded);
        }
    }
}

} // namespace

void queue(const hands::State& s)
{
    queueFor(s, false);
}

void queueFlat(const hands::State& s)
{
    if(flatQueuedFrame == host_framecount)
    {
        return;
    }
    flatQueuedFrame = host_framecount;
    queueFor(s, true);
}

} // namespace qvr::crosshair
