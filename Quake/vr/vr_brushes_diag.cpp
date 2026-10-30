// vr_brushes_diag.cpp -- vr_brush_selftest (the brushes' own check) and vr_brush_trace (Quake's trace between two points).
// Split from vr_brushes.cpp (a pure move); what they share is in vr_brushes_internal.hpp.

#include "vr_brushes_internal.hpp"
#include "vr_geom.hpp"
#include "vr_progs.hpp"

#include "vr_box3d.hpp"
#include "vr_brushbake.hpp"
#include "vr_brushlight.hpp"
#include "vr_cvars.hpp"
#include "vr_decals.hpp"
#include "vr_files.hpp"
#include "vr_held.hpp"
#include "vr_ledges.hpp"
#include "vr_mem.hpp"
#include "vr_modelcollide.hpp"

#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Ceil.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/Floor.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Remainder.hpp"
#include "Zancle/Math/Round.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace qvr::brushes
{
using namespace detail;

namespace
{

// vr_brush_trace <x y z> <X Y Z> [size] (tests): Quake's trace from one point to the other (a point, or a box `size`
// across: 32 the player's hull 1), as QC's traceline with nomonsters: where it stops and on what.
void trace_f()
{
    glm::dvec3 a, c;
    if(!parseVec(1, a) || !parseVec(4, c) || !serverReady())
    {
        Con_Printf("usage: vr_brush_trace <x y z> <X Y Z> [size]\n");
        return;
    }
    const progs::ServerVm vm;
    const float half = Cmd_Argc() > 7 ? Q_atof(Cmd_Argv(7)) * 0.5f : 0.f;
    vec3_t start{static_cast<float>(a.x), static_cast<float>(a.y), static_cast<float>(a.z)};
    vec3_t end{static_cast<float>(c.x), static_cast<float>(c.y), static_cast<float>(c.z)};
    vec3_t mins{-half, -half, half > 0.f ? -24.f : 0.f}, maxs{half, half, half > 0.f ? 32.f : 0.f};
    const trace_t tr = SV_Move(start, mins, maxs, end, MOVE_NOMONSTERS, progs::playerEdict()); // (not the world: what it owns, all, would be passed by)
    const int num = tr.ent ? NUM_FOR_EDICT(tr.ent) : -1;
    Con_Printf("brush trace: fraction %.3f, end %.1f %.1f %.1f, normal %.2f %.2f %.2f, entity %d%s%s%s\n", tr.fraction,
        tr.endpos[0], tr.endpos[1], tr.endpos[2], tr.plane.normal[0], tr.plane.normal[1], tr.plane.normal[2], num,
        num > 0 && ofEntity(num) ? va(" (brush %d)", ofEntity(num)) : "", tr.startsolid ? ", start solid" : "",
        tr.allsolid ? ", all solid" : "");
}

// The brushes' own check (vr_brush_selftest): the text of a box, a wedge (a sloped face) and a six-sided prism made and
// read back, their planes, textures and alignment the same; their models' hulls tested at points in and out (hull 0
// at the faces, hull 1 a box's reach further out); the made brushes' own texts too.
[[nodiscard]] bool samePlanes(const Brush& a, const Brush& b, za::String& why)
{
    if(a.faces.size() != b.faces.size())
    {
        why = va("%d faces read back as %d", static_cast<int>(a.faces.size()), static_cast<int>(b.faces.size()));
        return false;
    }
    for(za::SizeT i = 0; i < a.faces.size(); i++)
    {
        const Face& f = a.faces[i];
        const Face& g = b.faces[i];
        if(glm::length(f.normal - g.normal) > 1e-6 || za::fabs(f.dist - g.dist) > 1e-4 || strcmp(f.texture, g.texture) != 0 ||
            glm::length(f.uAxis - g.uAxis) > 1e-6 || glm::length(f.vAxis - g.vAxis) > 1e-6 || za::fabs(f.uOffset - g.uOffset) > 1e-6 ||
            za::fabs(f.vOffset - g.vOffset) > 1e-6 || za::fabs(f.scaleU - g.scaleU) > 1e-6 || za::fabs(f.scaleV - g.scaleV) > 1e-6)
        {
            why = va("face %d: %.3f %.3f %.3f %.3f %s read back as %.3f %.3f %.3f %.3f %s", static_cast<int>(i), f.normal.x, f.normal.y,
                f.normal.z, f.dist, f.texture, g.normal.x, g.normal.y, g.normal.z, g.dist, g.texture);
            return false;
        }
    }
    return true;
}

[[nodiscard]] int hullContents(const qmodel_t* m, int h, const glm::vec3& p)
{
    vec3_t q{p.x, p.y, p.z};
    return SV_HullPointContents(const_cast<hull_t*>(&m->hulls[h]), m->hulls[h].firstclipnode, q);
}

void selftest_f()
{
    int failed = 0, checked = 0;
    const auto check = [&](bool ok, const char* what) {
        checked++;
        if(!ok)
        {
            failed++;
            Con_Printf("brush selftest: FAIL %s\n", what);
        }
    };
    const char* tex = textureCount() > 0 ? textureAt(0)->name : "base";
    za::Vector<Brush> samples;
    samples.pushBack(box(glm::dvec3{-32.0, 16.0, 0.0}, glm::dvec3{48.0, 80.0, 72.0}, tex));
    {
        // A wedge: a box whose top slopes (its normal 45 degrees up), offsets and a scale on its faces.
        Brush w = box(glm::dvec3{0.0}, glm::dvec3{64.0, 64.0, 64.0}, tex);
        for(Face& f : w.faces)
        {
            if(f.normal.z > 0.5)
            {
                f.normal = glm::normalize(glm::dvec3{1.0, 0.0, 1.0});
                f.dist = glm::dot(f.normal, glm::dvec3{0.0, 0.0, 64.0});
                defaultAlignment(f);
                f.uOffset = 8.0;
                f.vOffset = -16.0;
                f.scaleU = 0.5;
            }
        }
        samples.pushBack(ZA_MOVE(w));
    }
    {
        // A six-sided prism (hexagonal) 48 tall.
        Brush p;
        for(int k = 0; k < 6; k++)
        {
            Face f;
            const double a = glm::radians(60.0 * k);
            f.normal = glm::dvec3{za::cos(a), za::sin(a), 0.0};
            f.dist = 40.0;
            q_strlcpy(f.texture, tex, sizeof(f.texture));
            defaultAlignment(f);
            p.faces.pushBack(f);
        }
        for(const double z : {1.0, -1.0})
        {
            Face f;
            f.normal = glm::dvec3{0.0, 0.0, z};
            f.dist = z > 0.0 ? 48.0 : 0.0;
            q_strlcpy(f.texture, tex, sizeof(f.texture));
            defaultAlignment(f);
            p.faces.pushBack(f);
        }
        samples.pushBack(ZA_MOVE(p));
    }
    // The tool's shapes, and a box turned 45 degrees about z (its hull 1 checked at its corner below: the bevels).
    const glm::dvec3 slo{0.0, 0.0, 0.0}, shi{96.0, 64.0, 48.0};
    for(const Shape s : {Shape::Wedge, Shape::Cylinder, Shape::Pyramid})
    {
        samples.pushBack(shape(s, slo, shi, 2, 1.0, 0, 1.0, s == Shape::Pyramid ? 4 : 8, tex));
    }
    const za::SizeT turnedIndex = samples.size();
    {
        Brush t = box(glm::dvec3{-32.0, -32.0, 0.0}, glm::dvec3{32.0, 32.0, 64.0}, tex);
        rotate(t, turnFromAngles(glm::dvec3{0.0, 45.0, 0.0}), glm::dvec3{0.0, 0.0, 32.0}, true);
        samples.pushBack(ZA_MOVE(t));
        // A box turned 90 degrees is a box again (its planes axial, whole).
        Brush q = box(glm::dvec3{0.0}, glm::dvec3{64.0, 32.0, 16.0}, tex);
        rotate(q, turnFromAngles(glm::dvec3{0.0, 90.0, 0.0}), glm::dvec3{32.0, 16.0, 8.0}, true);
        glm::dvec3 qlo, qhi;
        check(axial(q) && bounds(q, qlo, qhi) && qlo == glm::dvec3{16.0, -16.0, 0.0} && qhi == glm::dvec3{48.0, 48.0, 16.0},
            "a box turned 90 degrees: axial, 16 -16 0 .. 48 48 16");
        // Stretched to other bounds and back: the same planes.
        Brush w = shape(Shape::Wedge, slo, shi, 2, 1.0, 0, 1.0, 8, tex);
        Brush w2 = w;
        scaleTo(w2, slo, shi, glm::dvec3{8.0, 8.0, 8.0}, glm::dvec3{200.0, 40.0, 120.0});
        scaleTo(w2, glm::dvec3{8.0, 8.0, 8.0}, glm::dvec3{200.0, 40.0, 120.0}, slo, shi);
        za::String why;
        check(samePlanes(w, w2, why), va("a wedge stretched and back: %s", why.cStr()));
    }
    forEach([&](const Brush& b) { samples.pushBack(b); });
    for(za::SizeT i = 0; i < samples.size(); i++)
    {
        const Brush& b = samples[i];
        za::String text, error, why;
        toText(b, text);
        Brush back;
        const bool parsed = fromText(text, back, error);
        check(parsed, va("sample %d: not read back (%s)", static_cast<int>(i), error.cStr()));
        if(parsed)
        {
            check(samePlanes(b, back, why), va("sample %d: %s", static_cast<int>(i), why.cStr()));
            // Written again: the same text (a brush of whole planes), or the same planes read back once more (a sloped
            // face's points have digits a reading moves in their last place).
            za::String again, why2;
            toText(back, again);
            Brush back2;
            za::String error2;
            check(axial(b) ? again == text : fromText(again, back2, error2) && samePlanes(back, back2, why2),
                va("sample %d: its text written again differs %s", static_cast<int>(i), why2.cStr()));
        }
        check(invalid(b) == nullptr, va("sample %d: %s", static_cast<int>(i), invalid(b) ? invalid(b) : ""));
        // Its model: inside solid, outside empty, hull 1 grown by the player's box.
        Built built;
        build(&b, built);
        qmodel_t m{};
        apply(&m, &built, 0);
        // (its corners' average: inside a convex brush; the box tests on the flat-topped ones, where a level top is)
        glm::vec3 mid{0.f};
        for(const mvertex_t& v : built.vertexes)
        {
            mid += geom::vec(v.position) / static_cast<float>(built.vertexes.size());
        }
        check(hullContents(&m, 0, mid) == CONTENTS_SOLID, va("sample %d: hull 0 empty at its middle", static_cast<int>(i)));
        bool flatTop = false;
        for(const Face& f : b.faces)
        {
            flatTop = flatTop || f.normal.z > 1.0 - 1e-9;
        }
        if(flatTop)
        {
            const float top = built.maxs.z, bottom = built.mins.z;
            const struct
            {
                int hull;
                glm::vec3 at;
                int contents;
            } probes[] = {
                {0, {mid.x, mid.y, top + 1.f}, CONTENTS_EMPTY},
                {1, {mid.x, mid.y, top + 23.f}, CONTENTS_SOLID},  // (hull 1's box: 24 under its origin)
                {1, {mid.x, mid.y, top + 25.f}, CONTENTS_EMPTY},
                {1, {mid.x, mid.y, bottom - 31.f}, CONTENTS_SOLID}, // (32 over it)
                {1, {mid.x, mid.y, bottom - 33.f}, CONTENTS_EMPTY},
                {1, {built.maxs.x + 15.f, mid.y, mid.z}, CONTENTS_SOLID}, // (16 aside)
                {1, {built.maxs.x + 17.f, mid.y, mid.z}, CONTENTS_EMPTY},
                {2, {built.maxs.x + 31.f, mid.y, mid.z}, CONTENTS_SOLID}, // (hull 2: 32 aside)
                {2, {built.maxs.x + 33.f, mid.y, mid.z}, CONTENTS_EMPTY},
                {2, {mid.x, mid.y, bottom - 63.f}, CONTENTS_SOLID}, // (64 over it)
            };
            for(const auto& p : probes)
            {
                check(hullContents(&m, p.hull, p.at) == p.contents,
                    va("sample %d: hull %d %s at %.0f %.0f %.0f", static_cast<int>(i), p.hull,
                        p.contents == CONTENTS_SOLID ? "empty" : "solid", p.at.x, p.at.y, p.at.z));
            }
        }
        if(i == turnedIndex)
        {
            // The turned box's corner on +x is at x 45.25 (32 * sqrt 2): the player's box (16 aside) meets it 16 further,
            // not 32 (as its two faces' planes pushed out would meet without the bevel).
            const float cx = 32.f * 1.41421356f - built.origin.x;
            const float cz = 32.f - built.origin.z;
            check(hullContents(&m, 1, glm::vec3{cx + 15.f, -built.origin.y, cz}) == CONTENTS_SOLID, "turned box: hull 1 solid 15 off its corner");
            check(hullContents(&m, 1, glm::vec3{cx + 17.f, -built.origin.y, cz}) == CONTENTS_EMPTY, "turned box: hull 1 empty 17 off its corner (the bevel)");
            check(built.clipPlanes.size() >= 8, "turned box: its bevels");
        }
        if(i == 0)
        {
            Con_Printf("%s", text.cStr());
        }
    }
    Con_Printf("brush selftest: %s (%d checks, %d failed, %d brushes)\n", failed ? "FAIL" : "PASS", checked, failed,
        static_cast<int>(samples.size()));
}

} // namespace

void registerDiagCommands()
{
    Cmd_AddCommand("vr_brush_selftest", selftest_f);
    Cmd_AddCommand("vr_brush_trace", trace_f);
}

} // namespace qvr::brushes
