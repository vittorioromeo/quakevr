#include "quakedef.hpp"
#include "vr.hpp"
#include "vr_cvars.hpp"
#include "vr_showfn.hpp"
#include "util.hpp"
#include "quakeglm.hpp"
#include "glquake.hpp"

#include <algorithm>
#include <cassert>
#include <tuple>
#include <utility>

namespace quake::vr::showfn
{

namespace
{

//
//
//
// ----------------------------------------------------------------------------
// OpenGL Helpers
// ----------------------------------------------------------------------------

class gl_guard
{
private:
    static void setup_gl() noexcept
    {
        glDisable(GL_DEPTH_TEST);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        GL_PolygonOffset(OFFSET_SHOWTRIS);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_CULL_FACE);
    }

    static void cleanup_gl() noexcept
    {
        glColor3f(1, 1, 1);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_CULL_FACE);
        glDisable(GL_BLEND);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        GL_PolygonOffset(OFFSET_NONE);
        glEnable(GL_DEPTH_TEST);
    }

public:
    [[nodiscard]] gl_guard() noexcept
    {
        setup_gl();
    }

    ~gl_guard() noexcept
    {
        cleanup_gl();
    }

    gl_guard(const gl_guard&) = delete;
    gl_guard(gl_guard&&) = delete;

    gl_guard& operator=(const gl_guard&) = delete;
    gl_guard& operator=(gl_guard&&) = delete;

    template <typename F>
    void draw_points_with_size(float size, F&& f_draw) noexcept
    {
        glEnable(GL_POINT_SMOOTH);
        glPointSize(size);

        glBegin(GL_POINTS);
        f_draw();
        glEnd();

        glDisable(GL_POINT_SMOOTH);
    }

    template <typename F>
    void draw_points(F&& f_draw) noexcept
    {
        draw_points_with_size(12.f, f_draw);
    }

    template <typename F>
    void draw_points_and_lines(F&& f_draw) noexcept
    {
        glLineWidth(2.f * glwidth / vid.width);

        glEnable(GL_LINE_SMOOTH);
        glShadeModel(GL_SMOOTH);

        glBegin(GL_LINES);
        f_draw();
        glEnd();

        glShadeModel(GL_FLAT);
        glDisable(GL_LINE_SMOOTH);

        draw_points(f_draw);
    }

    template <typename F>
    void draw_line_strip(const float size, F&& f_draw)
    {
        glLineWidth(size * glwidth / vid.width);

        glEnable(GL_LINE_SMOOTH);
        glShadeModel(GL_SMOOTH);

        glBegin(GL_LINE_STRIP);
        f_draw();
        glEnd();

        glShadeModel(GL_FLAT);
        glDisable(GL_LINE_SMOOTH);
    }
};

void gl_vertex(const qvec3& v) noexcept
{
    glVertex3f(v[0], v[1], v[2]);
}

//
//
//
// ----------------------------------------------------------------------------
// Implementation utilities
// ----------------------------------------------------------------------------

[[nodiscard]] std::pair<bool, bool> get_hands_to_draw(
    const int cvarValue) noexcept
{
    assert(cvarValue >= 0 && cvarValue <= 3);
    const auto selection = static_cast<VrOptionHandSelection>(cvarValue);

    const bool opt_main_hand = selection == VrOptionHandSelection::MainHand;
    const bool opt_off_hand = selection == VrOptionHandSelection::OffHand;
    const bool opt_both_hands = selection == VrOptionHandSelection::BothHands;

    const bool draw_main_hand = opt_main_hand || opt_both_hands;
    const bool draw_off_hand = opt_off_hand || opt_both_hands;

    return {draw_main_hand, draw_off_hand};
}

//
//
//
// ----------------------------------------------------------------------------
// Show Virtual Stock
// ----------------------------------------------------------------------------

void show_virtual_stock_impl(
    const HandIdx holding_hand, const HandIdx helping_hand)
{
    const qvec3 holding_hand_pos =
        VR_Get2HHoldingHandPos(holding_hand, helping_hand);

    const qvec3 helping_hand_pos =
        VR_Get2HHelpingHandPos(holding_hand, helping_hand);

    const qvec3 shoulder_pos =
        VR_GetShoulderStockPos(holding_hand, helping_hand);

    const qvec3 average_pos =
        VR_Get2HVirtualStockMix(holding_hand_pos, shoulder_pos);

    glColor4f(0, 1, 0, 0.75);
    gl_vertex(holding_hand_pos);
    gl_vertex(helping_hand_pos);

    glColor4f(0, 1, 0, 0.75);
    gl_vertex(shoulder_pos);
    gl_vertex(helping_hand_pos);

    if(VR_InStockDistance(holding_hand, helping_hand, shoulder_pos))
    {
        glColor4f(1, 1, 0, 0.75);
    }
    else
    {
        glColor4f(0, 1, 1, 0.75);
    }

    glLineWidth(4.f * glwidth / vid.width);
    gl_vertex(average_pos);
    gl_vertex(helping_hand_pos);
}

void show_virtual_stock()
{
    if(vr_show_virtual_stock.value == 0)
    {
        return;
    }

    gl_guard guard;

    guard.draw_points_and_lines([&] {
        const auto [draw_main_hand, draw_off_hand] =
            get_hands_to_draw(vr_show_virtual_stock.value);

        if(draw_main_hand)
        {
            show_virtual_stock_impl(cVR_MainHand, cVR_OffHand);
        }

        if(draw_off_hand)
        {
            show_virtual_stock_impl(cVR_OffHand, cVR_MainHand);
        }
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show VR Torso
// ----------------------------------------------------------------------------

void show_vr_torso_debug_lines()
{
    if(vr_vrtorso_debuglines_enabled.value == 0)
    {
        return;
    }

    gl_guard guard;

    guard.draw_points_and_lines([&] {
        const auto len = 20._qf;

        const auto [adj_player_origin, adj_player_originLeft,
            adj_player_originRight, head_fwd_dir, head_right_dir, head_up_dir,
            mix_hand_dir, mix_final_dir] = VR_GetBodyYawAngleCalculations();

        glColor4f(0, 1, 0, 0.75);
        gl_vertex(adj_player_originLeft);
        gl_vertex(cl.handpos[0]);

        glColor4f(0, 1, 0, 0.75);
        gl_vertex(adj_player_originRight);
        gl_vertex(cl.handpos[1]);

        glColor4f(0, 1, 0, 0.75);
        gl_vertex(adj_player_origin);
        gl_vertex(adj_player_origin + head_fwd_dir * len);

        glColor4f(0, 0, 1, 0.75);
        gl_vertex(adj_player_origin);
        gl_vertex(adj_player_origin + mix_hand_dir * len);

        glColor4f(1, 0, 0, 0.75);
        gl_vertex(adj_player_origin);
        gl_vertex(adj_player_origin + mix_final_dir * len * 1.25_qf);
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show Holsters Impl
// ----------------------------------------------------------------------------

template <typename FLeftPos, typename FRightPos>
void show_holster_impl(
    const int cvarValue, FLeftPos&& f_left_pos, FRightPos&& f_right_pos)
{
    if(cvarValue == 0)
    {
        return;
    }

    const auto& off_hand_pos = cl.handpos[cVR_OffHand];
    const auto& main_hand_pos = cl.handpos[cVR_MainHand];

    const auto left_holster_pos = f_left_pos();
    const auto right_holster_pos = f_right_pos();

    const auto do_color = [&](const qvec3& hand, const qvec3& holster) {
        if(VR_InHipHolsterDistance(hand, holster))
        {
            glColor4f(1, 1, 0, 0.95);
            return;
        }

        glColor4f(0, 1, hand == main_hand_pos ? 1 : 0, 0.75);
    };

    const auto do_line = [&](const qvec3& hand, const qvec3& holster) {
        do_color(hand, holster);
        gl_vertex(hand);
        gl_vertex(holster);
    };

    gl_guard guard;

    guard.draw_points_and_lines([&] {
        const auto [draw_main_hand, draw_off_hand] =
            get_hands_to_draw(cvarValue);

        if(draw_main_hand)
        {
            do_line(main_hand_pos, left_holster_pos);
            do_line(main_hand_pos, right_holster_pos);
        }

        if(draw_off_hand)
        {
            do_line(off_hand_pos, left_holster_pos);
            do_line(off_hand_pos, right_holster_pos);
        }
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show Hip Holsters
// ----------------------------------------------------------------------------

void show_hip_holsters()
{
    show_holster_impl(
        vr_show_hip_holsters.value, VR_GetLeftHipPos, VR_GetRightHipPos);
}

//
//
//
// ----------------------------------------------------------------------------
// Show Shoulder Holsters
// ----------------------------------------------------------------------------

void show_shoulder_holsters()
{
    show_holster_impl(vr_show_shoulder_holsters.value,
        VR_GetLeftShoulderHolsterPos, VR_GetRightShoulderHolsterPos);
}

//
//
//
// ----------------------------------------------------------------------------
// Show Upper Holsters
// ----------------------------------------------------------------------------

void show_upper_holsters()
{
    show_holster_impl(
        vr_show_upper_holsters.value, VR_GetLeftUpperPos, VR_GetRightUpperPos);
}

//
//
//
// ----------------------------------------------------------------------------
// Show Crosshair
// ----------------------------------------------------------------------------

void show_crosshair_impl_point(
    const float size, const float alpha, const qvec3& start, const qvec3& fwd)
{
    qvec3 end, impact;

    if(vr_crosshair_depth.value <= 0)
    {
        // trace to first wall
        end = start + 4096._qf * fwd;
        end[2] += vr_crosshairy.value;

        // TODO VR: (P1) trace in clientside
        impact = TraceLine(start, end).endpos;
    }
    else
    {
        // fix crosshair to specific depth
        impact = start + qfloat(vr_crosshair_depth.value) * fwd;
    }

    glEnable(GL_POINT_SMOOTH);
    glColor4f(1, 0, 0, alpha);
    glPointSize(size * glwidth / vid.width);

    glBegin(GL_POINTS);
    {
        gl_vertex(impact);
    }
    glEnd();

    glDisable(GL_POINT_SMOOTH);
}

void show_crosshair_impl_line(
    const float size, const float alpha, const qvec3& start, const qvec3& fwd)
{
    const qfloat depth =
        vr_crosshair_depth.value <= 0 ? 4096 : vr_crosshair_depth.value;

    // trace to first entity
    const auto end = start + depth * fwd;

    // TODO VR: (P1) trace in clientside
    const trace_t trace = TraceLineToEntity(start, end, getPlayerEdict());

    auto impact = quake::util::hitSomething(trace) ? trace.endpos : end;
    impact[2] += vr_crosshairy.value * 10.f;

    glLineWidth(size * glwidth / vid.width);

    glEnable(GL_LINE_SMOOTH);
    glShadeModel(GL_SMOOTH);

    glBegin(GL_LINE_STRIP);
    {
        if((int)vr_crosshair.value == VrCrosshair::e_LINE)
        {
            glColor4f(1, 0, 0, alpha);
            gl_vertex(start);
            gl_vertex(impact);
        }
        else
        {
            const auto midA = glm::mix(start, impact, 0.15);
            const auto midB = glm::mix(start, impact, 0.70);

            glColor4f(1, 0, 0, alpha * 0.01f);
            gl_vertex(start);

            glColor4f(1, 0, 0, alpha);
            gl_vertex(midA);
            gl_vertex(midB);

            glColor4f(1, 0, 0, alpha * 0.01f);
            gl_vertex(impact);
        }
    }
    glEnd();

    glShadeModel(GL_FLAT);
    glDisable(GL_LINE_SMOOTH);
}

void show_crosshair_impl(
    const float size, const float alpha, const qvec3& start, const qvec3& fwd)
{
    switch((int)vr_crosshair.value)
    {
        default: [[fallthrough]];
        case VrCrosshair::e_POINT:
        {
            show_crosshair_impl_point(size, alpha, start, fwd);
            break;
        }

        case VrCrosshair::e_LINE: [[fallthrough]];
        case VrCrosshair::e_LINE_SMOOTH:
        {
            show_crosshair_impl_line(size, alpha, start, fwd);
            break;
        }
    }
}

void show_crosshair_hand_impl(
    const HandIdx hand_idx, const float size, const float alpha)
{
    if(VR_GetWpnCrosshairMode(VR_GetWpnCvarEntry(hand_idx)) ==
        WpnCrosshairMode::Forbidden)
    {
        return;
    }

    const auto start = VR_CalcFinalWpnMuzzlePos(hand_idx);
    const auto [fwd, right, up] =
        quake::util::getAngledVectors(cl.handrot[hand_idx]);

    show_crosshair_impl(size, alpha, start, fwd);
}

//
//
//
// ----------------------------------------------------------------------------
// Show Teleport Line
// ----------------------------------------------------------------------------

void show_teleport_line()
{
    if(!vr_teleport_enabled.value || !vr_teleporting ||
        vr_aimmode.value != VrAimMode::e_CONTROLLER)
    {
        return;
    }

    constexpr float size = 2.f;
    constexpr float alpha = 0.5f;

    if(size <= 0 || alpha <= 0)
    {
        return;
    }

    // calc angles
    const auto start = cl.handpos[cVR_OffHand];

    // calc line
    const auto impact = vr_teleporting_impact;

    const auto midA = glm::mix(start, impact, 0.15);
    const auto midB = glm::mix(start, impact, 0.85);

    // draw line
    const auto set_color = [&](const float xAlpha) {
        if(vr_teleporting_impact_valid)
        {
            glColor4f(0, 0, 1, xAlpha);
        }
        else
        {
            glColor4f(1, 0, 0, xAlpha);
        }
    };

    gl_guard guard;
    guard.draw_line_strip(size, [&] {
        set_color(alpha * 0.01f);
        gl_vertex(start);

        set_color(alpha);
        gl_vertex(midA);
        gl_vertex(midB);

        set_color(alpha * 0.01f);
        gl_vertex(impact);
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show Weapon Helper: Offset
// ----------------------------------------------------------------------------

void show_wpn_offset_helper_offset()
{
    if(vr_impl_draw_wpnoffset_helper_offset == 0)
    {
        return;
    }

    const auto do_color = [&] { glColor4f(0, 1, 1, 0.75); };

    const auto do_line = [&](const qvec3& a, const qvec3& b) {
        do_color();
        gl_vertex(a);
        gl_vertex(b);
    };

    gl_guard guard;

    guard.draw_points_and_lines([&] {
        const auto [hand_pos, hand_rot, cvarEntry] = [&] {
            const HandIdx hand_idx = vr_impl_draw_wpnoffset_helper_offset == 1
                                         ? cVR_MainHand
                                         : cVR_OffHand;

            return std::tuple{cl.handpos[hand_idx], cl.handrot[hand_idx],
                VR_GetWpnCvarEntry(hand_idx)};
        }();

        const auto offsetPos =
            quake::util::redirectVector(VR_GetWpnOffsets(cvarEntry), hand_rot);

        do_line(hand_pos, hand_pos + offsetPos);
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show Weapon Helper: Muzzle
// ----------------------------------------------------------------------------

void show_wpn_offset_helper_muzzle()
{
    if(vr_impl_draw_wpnoffset_helper_muzzle == 0)
    {
        return;
    }

    gl_guard guard;

    guard.draw_points_and_lines([&] {
        const auto muzzle_pos = vr_impl_draw_wpnoffset_helper_muzzle == 1
                                    ? VR_CalcFinalWpnMuzzlePos(cVR_MainHand)
                                    : VR_CalcFinalWpnMuzzlePos(cVR_OffHand);

        glColor4f(0, 1, 1, 0.75);
        gl_vertex(muzzle_pos);
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show Weapon Helper: 2H Offset
// ----------------------------------------------------------------------------

void show_wpn_offset_helper_2h_offset()
{
    if(vr_impl_draw_wpnoffset_helper_2h_offset == 0)
    {
        return;
    }

    gl_guard guard;

    guard.draw_points_and_lines([&] {
        const auto pos =
            vr_impl_draw_wpnoffset_helper_2h_offset == 1
                ? VR_Get2HHelpingHandPos(cVR_MainHand, cVR_OffHand)
                : VR_Get2HHelpingHandPos(cVR_OffHand, cVR_MainHand);

        glColor4f(0, 1, 1, 0.75);
        gl_vertex(pos);
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show Hand Pos and Rot
// ----------------------------------------------------------------------------

void show_hand_pos_and_rot()
{
    if(vr_debug_show_hand_pos_and_rot.value == 0)
    {
        return;
    }

    gl_guard guard;

    guard.draw_points_and_lines([&] {
        const auto do_hand = [&](const HandIdx hand_idx) {
            const auto& pos = cl.handpos[hand_idx];
            const auto& rot = cl.handrot[hand_idx];

            const auto fwd = quake::util::getFwdVecFromPitchYawRoll(rot);
            const auto end = pos + fwd * 1._qf;

            gl_vertex(pos);
            gl_vertex(end);
        };

        glColor4f(0, 1, 0, 0.75);
        do_hand(cVR_MainHand);

        glColor4f(1, 0, 0, 0.75);
        do_hand(cVR_OffHand);
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show Anchor Vertex Impl
// ----------------------------------------------------------------------------

void show_anchor_vertex_impl(const HandIdx hand_idx, const WpnCVar wpn_cvar)
{
    entity_t* const anchor = VR_GetAnchorEntity(hand_idx);

    if(anchor->model == nullptr)
    {
        return;
    }

    const WpnCvarEntry wpn_cvar_entry = VR_GetWpnCvarEntry(hand_idx);

    const auto anchor_vertex =
        static_cast<VertexIdx>(VR_GetWpnCVarValue(wpn_cvar_entry, wpn_cvar));

    const qvec3 rots = VR_GetWpnAngleOffsets(wpn_cvar_entry);

    const bool horiz_flip = hand_idx == cVR_OffHand;

    const auto do_vertex = [&](const VertexIdx vertex_idx_offset) {
        const auto pos = VR_GetScaledAndAngledAliasVertexPosition(anchor,
            anchor_vertex + vertex_idx_offset, vec3_zero,
            cl.handrot[hand_idx] + rots, horiz_flip);

        gl_vertex(pos);
    };

    gl_guard guard;

    guard.draw_points_with_size(12.f, [&] {
        glColor4f(1.f, 1.f, 1.f, 1.0f);
        do_vertex(0);
    });

    guard.draw_points_with_size(6.f, [&] {
        glColor4f(0.f, 0.f, 1.f, 0.95f);
        do_vertex(1);

        glColor4f(0.f, 1.f, 0.f, 0.95f);
        do_vertex(-1);
    });

    guard.draw_points_with_size(3.25f, [&] {
        // TODO VR: (P1) use the proper limit instead of 500
        glColor4f(0.f, 0.f, 1.f, 0.9f);
        for(int i = 2; i < 500; ++i)
        {
            do_vertex(i);
        }

        glColor4f(0.f, 1.f, 0.f, 0.9f);
        for(int i = 2; i < 500; ++i)
        {
            do_vertex(-i);
        }
    });
}

//
//
//
// ----------------------------------------------------------------------------
// Show Hand Anchor Vertex
// ----------------------------------------------------------------------------

void show_hand_anchor_vertex()
{
    // TODO VR: (P2) cvar to always show all vertices
    // show_hand_anchor_vertex_impl(cVR_MainHand);
    // show_hand_anchor_vertex_impl(cVR_OffHand);

    if(vr_impl_draw_hand_anchor_vertex == 0)
    {
        return;
    }

    const HandIdx hand_idx =
        vr_impl_draw_hand_anchor_vertex == 1 ? cVR_MainHand : cVR_OffHand;

    show_anchor_vertex_impl(hand_idx, WpnCVar::HandAnchorVertex);
}

//
//
//
// ----------------------------------------------------------------------------
// Show 2H Hand Anchor Vertex
// ----------------------------------------------------------------------------

// TODO VR: (P1) code repetition, move all Show functions to some other file
void show_2h_hand_anchor_vertex()
{
    // TODO VR: (P2) cvar to always show all vertices
    // show_2h_hand_anchor_vertex_impl(cVR_MainHand);
    // show_2h_hand_anchor_vertex_impl(cVR_OffHand);

    if(vr_impl_draw_2h_hand_anchor_vertex == 0)
    {
        return;
    }

    const HandIdx hand_idx =
        vr_impl_draw_hand_anchor_vertex == 1 ? cVR_MainHand : cVR_OffHand;

    show_anchor_vertex_impl(hand_idx, WpnCVar::TwoHHandAnchorVertex);
}

//
//
//
// ----------------------------------------------------------------------------
// Show Weapon Button Anchor Vertex
// ----------------------------------------------------------------------------

// TODO VR: (P1) code repetition
void show_wpn_button_anchor_vertex()
{
    if(vr_impl_draw_wpnbutton_anchor_vertex == 0)
    {
        return;
    }

    const HandIdx hand_idx =
        vr_impl_draw_hand_anchor_vertex == 1 ? cVR_MainHand : cVR_OffHand;

    show_anchor_vertex_impl(hand_idx, WpnCVar::WpnButtonAnchorVertex);
}

//
//
//
// ----------------------------------------------------------------------------
// Show Muzzle Anchor Vertex
// ----------------------------------------------------------------------------

// TODO VR: (P1) code repetition
void show_muzzle_anchor_vertex()
{
    if(vr_impl_draw_wpnoffset_helper_muzzle == 0)
    {
        return;
    }

    const HandIdx hand_idx =
        vr_impl_draw_hand_anchor_vertex == 1 ? cVR_MainHand : cVR_OffHand;

    show_anchor_vertex_impl(hand_idx, WpnCVar::MuzzleAnchorVertex);
}

} // namespace

//
//
//
// ----------------------------------------------------------------------------
// Globals
// ----------------------------------------------------------------------------

// TODO VR: (P2) organize and encapsulate
int vr_impl_draw_wpnoffset_helper_offset{0};
int vr_impl_draw_wpnoffset_helper_muzzle{0};
int vr_impl_draw_wpnoffset_helper_2h_offset{0};
int vr_impl_draw_hand_anchor_vertex{0};
int vr_impl_draw_2h_hand_anchor_vertex{0};
int vr_impl_draw_wpnbutton_anchor_vertex{0};
int vr_impl_draw_wpnoffset_helper_length{0};

//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void draw_all_show_helpers()
{
    show_virtual_stock();
    show_hip_holsters();
    show_shoulder_holsters();
    show_upper_holsters();
    show_vr_torso_debug_lines();
    show_teleport_line();
    show_wpn_offset_helper_offset();
    show_wpn_offset_helper_muzzle();
    show_wpn_offset_helper_2h_offset();
    show_hand_pos_and_rot();
    show_hand_anchor_vertex();
    show_2h_hand_anchor_vertex();
    show_wpn_button_anchor_vertex();
    show_muzzle_anchor_vertex();
}

void show_crosshair()
{
    if(vr_crosshair.value == 0 || !svPlayerActive())
    {
        return;
    }

    const float size = std::clamp(vr_crosshair_size.value, 0.f, 32.f);
    const float alpha = std::clamp(vr_crosshair_alpha.value, 0.f, 1.f);

    if(size <= 0 || alpha <= 0)
    {
        return;
    }

    gl_guard guard;
    show_crosshair_hand_impl(cVR_OffHand, size, alpha);
    show_crosshair_hand_impl(cVR_MainHand, size, alpha);
}

} // namespace quake::vr::showfn
