// vr_bodyclone.cpp -- see vr_bodyclone.hpp.
//
// The mirror is a vertical plane between your eyes and the image: the image of your eyes is `distance` from them, in
// the direction picked beside the menu panel (the panel is drawn over everything: an image behind it would be hidden
// by it). Worked out on the first call of a frame (the view's entities, body::queueDebug and the flashlight's zones ask in
// any order), from hands::current().

#include "vr_bodyclone.hpp"
#include "vr_backend.hpp"
#include "vr_engine.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_menu.hpp"
#include "vr_panel.hpp"
#include "vr_units.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Fmax.hpp"
#include "Zancle/Math/Fmin.hpp"

namespace qvr::bodyclone
{
namespace
{

struct Mirror
{
    int frame{-1};          // host_framecount it was worked out for
    bool shown{false};      // an image this frame
    bool placed{false};     // `head` and `yaw` hold the mirror's place (kept from frame to frame while shown)
    glm::vec3 head{0.f};    // the eyes it was placed from
    float yaw{0.f};         // degrees: the way from them to the image
    glm::mat4 world{1.f};   // the world to the image's place
    int drawn{0};           // the view's entities drawn reflected (noteDrawn), its frame's
    bool body{false};       // the body among them
    glm::vec3 bodyOrigin{0.f};
};

Mirror state;

[[nodiscard]] float wrapDegrees(float a)
{
    while(a > 180.f)
    {
        a -= 360.f;
    }
    while(a < -180.f)
    {
        a += 360.f;
    }
    return a;
}

void update()
{
    state.frame = host_framecount;
    state.shown = false;
    const hands::State& s = hands::current();
    glm::vec3 corner, xAxis, yAxis;
    if(vr_menu_body_clone.value == 0.f || !s.valid || !menu::bodyClonePage() || !panel::menuQuad(s, corner, xAxis, yAxis))
    {
        state.placed = false;
        return;
    }

    // The panel: the way to its middle, flat, and the angle half its width takes up.
    const float m2u = units::metresToUnits();
    const glm::vec3 centre = corner + 0.5f * (xAxis + yAxis);
    glm::vec3 ahead{centre.x - s.head.x, centre.y - s.head.y, 0.f};
    const float panelDistance = glm::length(ahead);
    if(panelDistance < 1e-3f)
    {
        ahead = hands::forward({0.f, s.headAngles.y, 0.f});
    }
    const float panelYaw = glm::degrees(za::atan2(ahead.y, ahead.x));
    const float halfPanel = glm::degrees(za::atan2(0.5f * glm::length(xAxis), za::fmax(panelDistance, 1.f)));

    // Beside it: its half width, the image's (half an arm span, about 0.45 m) and a little more; 3: straight ahead.
    const float distance = za::clamp(vr_menu_body_clone_distance.value, 0.5f, 5.f) * m2u;
    const int side = static_cast<int>(vr_menu_body_clone_side.value);
    float sign = 0.f;
    if(side == 1)
    {
        sign = 1.f; // left (Quake's yaw turns left)
    }
    else if(side == 2)
    {
        sign = -1.f;
    }
    else if(side != 3)
    {
        // Away from the main hand (its laser works the menu): on the side the other hand is on.
        const glm::vec3 left = hands::forward({0.f, panelYaw + 90.f, 0.f});
        sign = glm::dot(s.pos[HAND_MAIN] - s.pos[HAND_OFF], left) > 0.f ? -1.f : 1.f;
    }
    const float off = za::fmin(halfPanel + glm::degrees(za::atan2(0.45f * m2u, distance)) + 4.f, 80.f);
    const float yaw = panelYaw + sign * off;

    // Kept while you stand about where it was put and the panel stays (it moves with your head, and is turned again
    // when the menu is opened again).
    const glm::vec2 moved{s.head.x - state.head.x, s.head.y - state.head.y};
    if(!state.placed || glm::length(moved) > 0.6f * m2u || za::fabs(wrapDegrees(yaw - state.yaw)) > 2.f)
    {
        state.placed = true;
        state.head = s.head;
        state.yaw = yaw;
    }

    // The plane half way, facing you: p' = p - 2 ((p - P) . n) n.
    const glm::vec3 n = hands::forward({0.f, state.yaw, 0.f});
    const glm::vec3 plane = state.head + n * (0.5f * distance);
    glm::mat4 m{glm::mat3{1.f} - 2.f * glm::outerProduct(n, n)};
    m[3] = glm::vec4{2.f * glm::dot(plane, n) * n, 1.f};
    state.world = m;
    state.shown = true;
}

} // namespace

bool mirror(glm::mat4& out)
{
    if(state.frame != host_framecount)
    {
        update();
    }
    if(!state.shown)
    {
        return false;
    }
    out = state.world;
    return true;
}

void noteDrawn(int entities, bool body, const glm::vec3& bodyOrigin)
{
    state.drawn = entities;
    state.body = body;
    state.bodyOrigin = bodyOrigin;
}

void info_f()
{
    glm::mat4 m;
    if(!mirror(m))
    {
        Con_Printf("bodyclone: off (vr_menu_body_clone %g, page tweaks the body %d)\n", vr_menu_body_clone.value,
            menu::bodyClonePage() ? 1 : 0);
        return;
    }
    const hands::State& s = hands::current();
    const glm::vec3 eyes = glm::vec3{m * glm::vec4{s.head, 1.f}};
    const float m2u = units::metresToUnits();
    Con_Printf("bodyclone: on, yaw %.1f (view %.1f), its eyes %.2f m off at %.1f %.1f %.1f; drawn %d entities, body %d at "
               "%.1f %.1f %.1f\n",
        state.yaw, s.headAngles.y, glm::distance(eyes, s.head) / m2u, eyes.x, eyes.y, eyes.z, state.drawn,
        state.body ? 1 : 0, state.bodyOrigin.x, state.bodyOrigin.y, state.bodyOrigin.z);
}

} // namespace qvr::bodyclone
