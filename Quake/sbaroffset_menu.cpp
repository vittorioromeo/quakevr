#include "quakedef.hpp"
#include "vr.hpp"
#include "sbaroffset_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"
#include "menu_util.hpp"

#include <string>
#include <cassert>
#include <array>
#include <tuple>

[[nodiscard]] static quake::menu make_menu()
{
    constexpr float oInc = 1.f;
    constexpr float oBound = 200.f;

    constexpr float rInc = 0.1f;
    constexpr float rBound = 90.f;

    const quake::menu_bounds<float> oBounds{oInc, -oBound, oBound};
    const quake::menu_bounds<float> rBounds{rInc, -rBound, rBound};

    quake::menu m{"Status Bar Offsets"};

    m.add_cvar_entry<float>("Offset X", vr_sbar_offset_x, oBounds);
    m.add_cvar_entry<float>("Offset Y", vr_sbar_offset_y, oBounds);
    m.add_cvar_entry<float>("Offset Z", vr_sbar_offset_z, oBounds);
    m.add_cvar_entry<float>("Scale", vr_hud_scale, {0.005f, 0.01f, 2.f});
    m.add_cvar_entry<float>("Roll", vr_sbar_offset_roll, rBounds);
    m.add_cvar_entry<float>("Pitch", vr_sbar_offset_pitch, rBounds);
    m.add_cvar_entry<float>("Yaw", vr_sbar_offset_yaw, rBounds);

    return m;
}

static quake::menu g_menu = make_menu();

quake::menu& M_SbarOffset_Menu()
{
    return g_menu;
}

void M_SbarOffset_Key(int key)
{
    g_menu.key(key);
}

void M_SbarOffset_Draw()
{
    g_menu.draw();
}
