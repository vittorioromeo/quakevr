// vr_menu.hpp -- the VR Settings pages (Options > VR Settings); the engine's menu calls
// VR_Menu_Open/Draw/Key (vr_api.h).

#pragma once

namespace qvr::menu
{

// menu_vr [page]: the VR Settings, or one of its pages (1: Advanced VR Options).
void command_f();

// The page shown (while m_state is m_vr), and the VR Settings reopened at one (its selection,
// scroll and way back as they were left).
[[nodiscard]] int currentPage();
void reopen(int page);

// Scrolls the page shown by `rows` (the selection kept in view, moved only if it would leave it);
// false, doing nothing, when the page fits (or a slider or the scrollbar is held). 0: only asks.
bool scroll(int rows);

} // namespace qvr::menu
