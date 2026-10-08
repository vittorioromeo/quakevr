// vr_coil.hpp -- a cord between two moving ends (the flashlight's, from the belt clip to the torch; the chainsaw's starter
// cord), sagging under its own weight and swinging as the ends move, drawn as a plain cable, as a helix of wire round
// it (a coiled cord, as an old telephone's) or as a low-poly chain of links along it, lit per vertex by the world's
// light and the dynamic lights, depth-tested in each eye's opaque scene.
//
// The cord's line is a chain of masses and springs (both ends pinned, a short stub at each so that it leaves them
// along their directions): stretched, it pulls straight with a little sag; slack, it droops. A coil keeps its wire's
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
    int turns{0};              // turns of a coiled cord (0: a plain cable or the chain)
    float coilRadius{0.0065f}; // metres from the line to a coil's wire's middle, relaxed
    float wireRadius{0.0019f}; // metres, the wire's thickness / 2
    glm::vec3 albedo{0.1f};
    float length{0.f}; // metres, the line's relaxed length (0: the coil's turns touching, at least 64)
    // A chain instead (round 21, the flashlight's: NOTES.md start_2026-10-03_02-19-12, Quake's look): links of square
    // bar along the same line, each a hexagon of wireRadius half-thickness, its faces flat-shaded (gfx::drawTube's
    // `flat`), the same at every distance, each turned a quarter round from the last, paid out of the first end (the
    // belt clip) as the line stretches: the links keep their places from the second end (the torch). Each link's iron
    // rusted by its own amount (towards `rust`), grimy.
    bool chain{false};
    float linkLength{0.016f}; // metres, a link's inside length (the chain's pitch)
    float linkWidth{0.008f};  // metres, its inside width
    glm::vec3 rust{0.3f, 0.14f, 0.06f};
};

// The coiled cord as an old telephone's (the flashlight's Coiled, vr_flashlight_cord 2; the cell cords'): 64 turns of
// 3.8 mm dark wire, 1.3 cm across relaxed (its relaxed length the turns touching, 0.243 m).
[[nodiscard]] inline Style coiled()
{
    Style style;
    style.turns = 64;
    style.coilRadius = 0.0065f;
    style.wireRadius = 0.0019f;
    style.albedo = glm::vec3{0.14f, 0.14f, 0.135f};
    return style;
}

class Cord
{
public:
    // Once a frame: the ends and the directions the cord leaves them (world), and the style. The body's movement (the
    // first end's) carries the cord along; the other end's moves swing it.
    void update(const glm::vec3& a, const glm::vec3& aDir, const glm::vec3& b, const glm::vec3& bDir, const Style& style);

    // The same with the first end's ways (the cell cords, vr_cellcord.cpp): `body` the point whose movement carries the
    // cord along (the first end's, but for a plug flying back to it: a); `aLoose` the first end not held at all, hanging
    // off the second (a pulled out, a dangling plug): `a` then only where a new cord starts.
    void update(const glm::vec3& a, const glm::vec3& aDir, const glm::vec3& b, const glm::vec3& bDir, const Style& style,
        const glm::vec3& body, bool aLoose);

    // The ends as last simulated (a loose first one's, where it hangs), and the way the line leaves the first.
    [[nodiscard]] glm::vec3 firstEnd() const { return pos_.empty() ? glm::vec3{0.f} : pos_[0]; }
    [[nodiscard]] glm::vec3 secondEnd() const { return pos_.empty() ? glm::vec3{0.f} : pos_.back(); }
    [[nodiscard]] glm::vec3 firstDir() const
    {
        const glm::vec3 d = pos_.size() < 2 ? glm::vec3{0.f} : pos_[1] - pos_[0];
        return glm::length(d) > 1e-6f ? glm::normalize(d) : glm::vec3{0.f, 0.f, 1.f};
    }

    // Not drawn this frame: starts afresh when drawn again.
    void hide() { valid_ = false; }

    [[nodiscard]] bool visible() const { return valid_; }

    // Its rings for gfx::drawTube, lit, and the sides round them, seen from `eye` (the detail: fewer further away).
    // False: nothing to draw. Once a frame, for both eyes.
    [[nodiscard]] bool build(const glm::vec3& eye, za::Vector<gfx::TubeRing>& out, int& sides) const;

    [[nodiscard]] const glm::vec3& albedo() const { return style_.albedo; }
    [[nodiscard]] const glm::vec3& rust() const { return style_.rust; }
    [[nodiscard]] bool flat() const { return style_.chain; } // drawTube's `flat`

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
