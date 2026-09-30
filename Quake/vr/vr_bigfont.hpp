// vr_bigfont.hpp -- the main menu's lettering as a font (vr_bigfont.cpp): menu.c's M_Main_Draw draws its rows as text
// in it (VR_BigFont_CanDraw, VR_BigFont_Draw: vr_api.h), VR Calibration in the same letters as Quake's own rows.

#pragma once

namespace qvr::bigfont
{

// The game directory changed (VR_OnGameDirChanged): the letters are cut again from its pictures when next drawn.
void onGameDirChanged();

// vr_bigfont (a debug command): which letters the pictures gave and which were left out (their hash differed).
void report_f();

} // namespace qvr::bigfont
