#include "quakedef.hpp"
#include "vr.hpp"
#include "debug_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"

#include <string>
#include <cassert>
#include <array>

extern cvar_t r_showbboxes;
extern cvar_t host_timescale;

static int debug_options_cursor = 0;

extern void M_DrawSlider(int x, int y, float range);

static void Debug_MenuPlaySound(const char* sound, float fvol)
{
    if(sfx_t* sfx = S_PrecacheSound(sound))
    {
        S_StartSound(cl.viewentity, 0, sfx, vec3_origin, fvol, 1);
    }
}

static void Debug_MenuPrintOptionValue(int cx, int cy, DebugMenuOpt option)
{
    char value_buffer[32] = {0};

    const auto printAsStr = [&](const auto& cvar) {
        snprintf(value_buffer, sizeof(value_buffer), "%.4f", cvar.value);
        M_Print(cx, cy, value_buffer);
    };

    switch(option)
    {
        case DebugMenuOpt::e_Showbboxes:
            M_Print(cx, cy, r_showbboxes.value == 0 ? "Off" : "On");
            break;
        case DebugMenuOpt::e_Impulse9: break;
        case DebugMenuOpt::e_Impulse11: break;
        case DebugMenuOpt::e_God: break;
        case DebugMenuOpt::e_Noclip: break;
        case DebugMenuOpt::e_Fly: break;
        case DebugMenuOpt::e_HostTimescale: printAsStr(host_timescale); break;
        default: assert(false); break;
    }
}

static void M_Debug_KeyOption(int key, DebugMenuOpt option)
{
    const bool isLeft = (key == K_LEFTARROW);

    const auto adjustF = [&isLeft](const cvar_t& cvar, auto incr, auto min,
                             auto max) {
        Cvar_SetValue(cvar.name,
            CLAMP(min, isLeft ? cvar.value - incr : cvar.value + incr, max));
    };

    const auto adjustI = [&isLeft](const cvar_t& cvar, auto incr, auto min,
                             auto max) {
        Cvar_SetValue(cvar.name,
            (int)CLAMP(
                min, isLeft ? cvar.value - incr : cvar.value + incr, max));
    };

    const auto doCheat = [&](const char* cmd) {
        Debug_MenuPlaySound("items/r_item2.wav", 0.5);
        Cmd_ExecuteString(cmd, cmd_source_t::src_command);
    };

    switch(option)
    {
        case DebugMenuOpt::e_Showbboxes: adjustI(r_showbboxes, 1, 0, 1); break;
        case DebugMenuOpt::e_Impulse9: doCheat("impulse 9"); break;
        case DebugMenuOpt::e_Impulse11: doCheat("impulse 11"); break;
        case DebugMenuOpt::e_God: doCheat("god"); break;
        case DebugMenuOpt::e_Noclip: doCheat("noclip"); break;
        case DebugMenuOpt::e_Fly: doCheat("fly"); break;
        case DebugMenuOpt::e_HostTimescale:
            adjustF(host_timescale, 0.05f, 0.1f, 5.f);
            break;
        default: assert(false); break;
    }
}

void M_Debug_Key(int key)
{
    switch(key)
    {
        case K_ESCAPE:
            VID_SyncCvars(); // sync cvars before leaving menu. FIXME: there are
                             // other ways to leave menu
            S_LocalSound("misc/menu1.wav");
            M_Menu_Options_f();
            break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            debug_options_cursor--;
            if(debug_options_cursor < 0)
            {
                debug_options_cursor = (int)DebugMenuOpt::k_Max - 1;
            }
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            debug_options_cursor++;
            if(debug_options_cursor >= (int)DebugMenuOpt::k_Max)
            {
                debug_options_cursor = 0;
            }
            break;

        case K_LEFTARROW: [[fallthrough]];
        case K_RIGHTARROW:
            S_LocalSound("misc/menu3.wav");
            M_Debug_KeyOption(key, (DebugMenuOpt)debug_options_cursor);
            break;

        case K_ENTER:
            m_entersound = true;
            M_Debug_KeyOption(key, (DebugMenuOpt)debug_options_cursor);
            break;

        default: break;
    }
}

void M_Debug_Draw()
{
    int y = 4;

    // plaque
    M_DrawTransPic(16, y, Draw_CachePic("gfx/qplaque.lmp"));

    // customize header
    qpic_t* p = Draw_CachePic("gfx/ttl_cstm.lmp");
    M_DrawPic((320 - p->width) / 2, y, p);

    y += 28;

    // titl
    const char* title = "DEBUG";
    M_PrintWhite((320 - 8 * strlen(title)) / 2, y, title);

    y += 16;
    int idx = 0;

    static const auto adjustedLabels =
        quake::util::makeAdjustedMenuLabels("Show BBoxes", "Impulse 9",
            "Impuse 11", "God", "Noclip", "Fly", "Timescale");

    static_assert(adjustedLabels.size() == (int)DebugMenuOpt::k_Max);

    for(const std::string& label : adjustedLabels)
    {
        M_Print(16, y, label.data());
        Debug_MenuPrintOptionValue(240, y, (DebugMenuOpt)idx);

        // draw the blinking cursor
        if(debug_options_cursor == idx)
        {
            M_DrawCharacter(220, y, 12 + ((int)(realtime * 4) & 1));
        }

        ++idx;
        y += 8;
    }
}

void M_Menu_Debug_f()
{
    const char* sound = "items/r_item1.wav";

    IN_Deactivate(modestate == MS_WINDOWED);
    key_dest = key_menu;
    m_state = m_debug;
    m_entersound = true;

    Debug_MenuPlaySound(sound, 0.5);
}
