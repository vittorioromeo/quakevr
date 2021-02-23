#pragma once

//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace quake
{
namespace vr
{
namespace showfn
{

void draw_all_show_helpers();
void show_crosshair();

} // namespace showfn
} // namespace vr
} // namespace quake

//
//
//
// ----------------------------------------------------------------------------
// Extern Declarations
// ----------------------------------------------------------------------------

namespace quake
{
namespace vr
{
namespace showfn
{

// TODO VR: (P2) encapsulate nicely
extern int vr_impl_draw_wpnoffset_helper_offset;
extern int vr_impl_draw_wpnoffset_helper_muzzle;
extern int vr_impl_draw_wpnoffset_helper_2h_offset;
extern int vr_impl_draw_hand_anchor_vertex;
extern int vr_impl_draw_2h_hand_anchor_vertex;
extern int vr_impl_draw_wpnbutton_anchor_vertex;
extern int vr_impl_draw_wpntext_anchor_vertex;

} // namespace showfn
} // namespace vr
} // namespace quake
