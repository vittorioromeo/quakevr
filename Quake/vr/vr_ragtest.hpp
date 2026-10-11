// vr_ragtest.hpp -- the active ragdolls' tests (Debug > Tests > Ragdoll Behaviours; `vr_ragdoll_act <name>`; ROUND22.md,
// "Ragdoll tests, tuning, self-collision"): each behaviour of an active ragdoll (vr_ragactive.cpp: the deaths by where
// they were struck, the stumble, the wound held, the crawl; the lost limbs' reactions; the swim and the flail; the wall,
// the ledge and the catch; the living hit reactions, the staggers, the throw, the get-up) set going on the nearest monster
// of a kind that has them (box3d::activeKind), then watched until it begins (or not: why), one console line each
// (`ragact:`). A behaviour that needs a place (a wall behind him, a ledge's lip) moves him to the nearest one; one that
// needs a liquid forces it on dry land when he stands in none. The actions are one table (vr_ragtest.cpp, `actions`): a
// row there is a menu row and a console name at once.

#pragma once

#include "vr_engine.hpp"
#include "vr_ragactive.hpp"

namespace qvr::ragtest
{

// A test's forcing of edict `num`'s next active death (vr_box3d.cpp asks, startActiveDeath and driveActive), until `until`
// (server time): the wound's or the crawl's chance sure, no pelvis pin (thrown where the shot throws him, not where his
// death animation falls), its own motion as it dies (units/s: a stumble; 0 its own; `still`: none, moved there by the
// test, not walking), the liquid its drive feels everywhere (a swim on dry land; None: the world's).
struct Force
{
    int num{0};
    bool wound{false};
    bool crawl{false};
    bool noPin{false};
    bool still{false};
    glm::vec3 walk{0.f};
    ragactive::Liquid liquid{ragactive::Liquid::None};
    double until{0.0};
};
[[nodiscard]] const Force* forced(int num);

// The server's frame (VR_PhysicsFrameEnd): each test under way's next step, whether its behaviour began.
void frame();

// vr_ragdoll_act <name> [entity]: the action `name` on the nearest supported monster (or that entity); no name: the list.
void registerCommands();

// The menu's rows (vr_menu.cpp, Debug > Tests > Ragdoll Behaviours), in the table's order: its group (a heading where it
// changes), its label, its command (vr_ragdoll_act <name>), its help.
struct Row
{
    const char* group;
    const char* label;
    const char* command;
    const char* help;
};
[[nodiscard]] int rowCount();
[[nodiscard]] Row row(int i);

} // namespace qvr::ragtest
