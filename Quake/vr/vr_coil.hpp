// vr_coil.hpp -- a coiled cord, as an old telephone's (the flashlight's, from the belt clip to the torch): a springy
// cord between two moving ends, sagging under its own weight and swinging as the ends move, drawn as a helix of wire
// round it (or a plain cable, or a chain of links along it), lit per vertex by the world's light and the dynamic
// lights, depth-tested in each eye's opaque scene.
//
// The cord's line is a chain of masses and springs (both ends pinned, a short stub at each so that it leaves them
// along their directions): stretched, it pulls straight with a little sag; slack, it droops. The coil keeps its wire's
// length: stretched, its turns open out and it narrows, as a real one does. Everything in world units.

#pragma once

#include "vr_engine.hpp"
#include "vr_gfx.hpp"

#include "Zancle/Container/Vector.hpp"


namespace qvr::coil
{

// The world's light at `p` as the alias models get it (1: Quake's full light): for a tube drawn beside a cord (the
// chainsaw's cord handle, vr_chainsaw.cpp).
[[nodiscard]] glm::vec3 lightAt(const glm::vec3& p);

struct Style
{
    int turns{64};             // turns of the coil (0: a plain cable)
    float coilRadius{0.0065f}; // metres from the line to the wire's middle, relaxed
    float wireRadius{0.0019f}; // metres, the wire's thickness / 2
    glm::vec3 albedo{0.1f};
    float length{0.f}; // metres, the line's relaxed length (0: the coil's turns touching)
    // A chain instead (round 21, the flashlight's rusty chain): links of wireRadius wire along the same line, each turned
    // a quarter round from the last, paid out of the first end (the belt clip) as the line stretches: the links keep
    // their places from the second end (the torch). Each link's iron rusted by its own amount (towards `rust`), grimy.
    bool chain{false};
    float linkLength{0.013f}; // metres, a link's inside length (the chain's pitch)
    float linkWidth{0.0075f}; // metres, its inside width
    glm::vec3 rust{0.3f, 0.14f, 0.06f};
};

class Cord
{
public:
    // Once a frame: the ends and the directions the cord leaves them (world), and the style. The body's movement (the
    // first end's) carries the cord along; the other end's moves swing it.
    void update(const glm::vec3& a, const glm::vec3& aDir, const glm::vec3& b, const glm::vec3& bDir, const Style& style);

    // Not drawn this frame: starts afresh when drawn again.
    void hide() { valid_ = false; }

    [[nodiscard]] bool visible() const { return valid_; }

    // Its rings for gfx::drawTube, lit, and the sides round them, seen from `eye` (the detail: fewer further away).
    // False: nothing to draw. Once a frame, for both eyes.
    [[nodiscard]] bool build(const glm::vec3& eye, za::Vector<gfx::TubeRing>& out, int& sides) const;

    [[nodiscard]] const glm::vec3& albedo() const { return style_.albedo; }
    [[nodiscard]] const glm::vec3& rust() const { return style_.rust; }

    // The number of rings and links (a chain's) of the last build, and its sides (vr_flashlight_cord_info).
    [[nodiscard]] int rings() const { return rings_; }
    [[nodiscard]] int links() const { return links_; }
    [[nodiscard]] float length() const { return length_; } // the line's, metres

private:
    void reset(const glm::vec3& a, const glm::vec3& b);

    Style style_;
    bool valid_{false};
    double time_{0.0};
    za::Vector<glm::vec3> pos_, vel_;
    glm::vec3 lastA_{0.f}, lastB_{0.f};
    mutable int rings_{0};
    mutable int links_{0};
    mutable float length_{0.f};
};

} // namespace qvr::coil
