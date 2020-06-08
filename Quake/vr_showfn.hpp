#pragma once

//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

namespace quake::vr::showfn
{

void draw_all_show_helpers();
void show_crosshair();

} // namespace quake::vr::showfn

//
//
//
// ----------------------------------------------------------------------------
// Extern Declarations
// ----------------------------------------------------------------------------

namespace quake::vr::showfn
{

// TODO VR: (P2) encapsulate nicely
extern int vr_impl_draw_wpnoffset_helper_offset;
extern int vr_impl_draw_wpnoffset_helper_muzzle;
extern int vr_impl_draw_wpnoffset_helper_2h_offset;
extern int vr_impl_draw_hand_anchor_vertex;
extern int vr_impl_draw_2h_hand_anchor_vertex;
extern int vr_impl_draw_wpnbutton_anchor_vertex;

}
