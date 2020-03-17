#include "quakedef.hpp"
#include "map_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"

#include <string>
#include <cassert>
#include <array>
#include <tuple>
#include <cstddef>
#include <string_view>

using namespace std::literals;

static std::size_t mapmenu_cursor = 0;

// clang-format off
static const std::array maps{
    "e1m1"sv,
    "e1m2"sv,
    "e1m3"sv,
    "e1m4"sv,
    "e1m5"sv,
    "e1m6"sv,
    "e1m7"sv,
    "e1m8"sv,
    "e2m1"sv,
    "e2m2"sv,
    "e2m3"sv,
    "e2m4"sv,
    "e2m5"sv,
    "e2m6"sv,
    "e2m7"sv,
    "e3m1"sv,
    "e3m2"sv,
    "e3m3"sv,
    "e3m4"sv,
    "e3m5"sv,
    "e3m6"sv,
    "e3m7"sv,
    "e4m1"sv,
    "e4m2"sv,
    "e4m3"sv,
    "e4m4"sv,
    "e4m5"sv,
    "e4m6"sv,
    "e4m7"sv,
    "e4m8"sv,
    "end"sv,
    "hip1m1"sv,
    "hip1m2"sv,
    "hip1m3"sv,
    "hip1m4"sv,
    "hip1m5"sv,
    "hip2m1"sv,
    "hip2m2"sv,
    "hip2m3"sv,
    "hip2m4"sv,
    "hip2m5"sv,
    "hip2m6"sv,
    "hip3m1"sv,
    "hip3m2"sv,
    "hip3m3"sv,
    "hip3m4"sv,
    "hipdm1"sv,
    "hipend"sv
};
// clang-format on

static void MapMenu_MenuPlaySound(const char* sound, float fvol)
{
    if(sfx_t* const sfx = S_PrecacheSound(sound))
    {
        S_StartSound(cl.viewentity, 0, sfx, { 0.f, 0.f, 0.f }, fvol, 1);
    }
}

static void MapMenu_MenuPrintOptionValue(
    const int cx, const int cy, const int option)
{
    (void)cx;
    (void)cy;
    (void)option;
}

static void M_MapMenu_KeyOption(const int key, const int option)
{
    (void)key;

    const auto& mapName = maps[option];
    Cmd_ExecuteString(va("map %s", mapName.data()), src_command);
}

void M_MapMenu_Key(int key)
{
    switch(key)
    {
        case K_ESCAPE:
        {
            VID_SyncCvars(); // sync cvars before leaving menu. FIXME: there are
                             // other ways to leave menu
            S_LocalSound("misc/menu1.wav");
            M_Menu_Options_f();
            break;
        }

        case K_UPARROW:
        {
            S_LocalSound("misc/menu1.wav");

            if(mapmenu_cursor == 0)
            {
                mapmenu_cursor = maps.size() - 1;
            }
            else
            {
                --mapmenu_cursor;
            }

            break;
        }

        case K_DOWNARROW:
        {
            S_LocalSound("misc/menu1.wav");
            ++mapmenu_cursor;
            if(mapmenu_cursor >= maps.size())
            {
                mapmenu_cursor = 0;
            }
            break;
        }

        case K_LEFTARROW: [[fallthrough]];
        case K_RIGHTARROW:
        {
            S_LocalSound("misc/menu3.wav");
            M_MapMenu_KeyOption(key, mapmenu_cursor);
            break;
        }

        case K_ENTER:
        {
            m_entersound = true;
            M_MapMenu_KeyOption(key, mapmenu_cursor);
            break;
        }
    }
}

void M_MapMenu_Draw()
{
    int y = 4;

    // plaque
    M_DrawTransPic(16, y, Draw_CachePic("gfx/qplaque.lmp"));

    // customize header
    qpic_t* p = Draw_CachePic("gfx/ttl_cstm.lmp");
    M_DrawPic((320 - p->width) / 2, y, p);

    y += 28;

    // title
    const char* title = "Change Map";
    M_PrintWhite((320 - 8 * strlen(title)) / 2, y, title);

    y += 16;
    std::size_t idx = 0;

    for(const std::string_view& label : maps)
    {
        M_Print(70 + (120 * (idx / 25)), y, label.data());
        MapMenu_MenuPrintOptionValue(240, y, idx);

        // draw the blinking cursor
        if(mapmenu_cursor == idx)
        {
            M_DrawCharacter((70 - 15) + (120 * (idx / 25)), y, 12 + ((int)(realtime * 4) & 1));
        }

        ++idx;
        y += 8;
        if(idx % 25 == 0)
        {
            y = 32 + 16;
        }
    }
}

void M_Menu_MapMenu_f()
{
    const char* sound = "items/r_item1.wav";

    IN_Deactivate(modestate == MS_WINDOWED);
    key_dest = key_menu;
    m_state = m_map;
    m_entersound = true;

    MapMenu_MenuPlaySound(sound, 0.5);
}
