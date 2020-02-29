#include "quakedef.hpp"
#include "vr.hpp"
#include "sbaroffset_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"

#include <string>
#include <cassert>
#include <array>
#include <tuple>

static int sbaroff_cursor = 0;

static void SbarOffset_MenuPlaySound(const char* sound, float fvol)
{
    sfx_t* sfx = S_PrecacheSound(sound);

    if(sfx)
    {
        S_StartSound(cl.viewentity, 0, sfx, vec3_origin, fvol, 1);
    }
}

static auto getCvars()
{
    return std::tie(vr_sbar_offset_x, vr_sbar_offset_y, vr_sbar_offset_z,
        vr_hud_scale, vr_sbar_offset_roll, vr_sbar_offset_pitch,
        vr_sbar_offset_yaw);
}

static void SbarOffset_MenuPrintOptionValue(
    int cx, int cy, SbarOffsetMenuOpt option)
{
    char value_buffer[32] = {0};

    const auto printAsStr = [&](const auto& cvar) {
        snprintf(value_buffer, sizeof(value_buffer), "%.4f", cvar.value);
        M_Print(cx, cy, value_buffer);
    };

    auto [ox, oy, oz, sc, rr, rp, ry] = getCvars();

    switch(option)
    {
        case SbarOffsetMenuOpt::OffsetX: printAsStr(ox); break;
        case SbarOffsetMenuOpt::OffsetY: printAsStr(oy); break;
        case SbarOffsetMenuOpt::OffsetZ: printAsStr(oz); break;
        case SbarOffsetMenuOpt::Scale: printAsStr(sc); break;
        case SbarOffsetMenuOpt::Roll: printAsStr(rr); break;
        case SbarOffsetMenuOpt::Pitch: printAsStr(rp); break;
        case SbarOffsetMenuOpt::Yaw: printAsStr(ry); break;
        default: assert(false); break;
    }
}

static void M_SbarOffset_KeyOption(int key, SbarOffsetMenuOpt option)
{
    const bool isLeft = (key == K_LEFTARROW);

    const auto adjustF = [&isLeft](const cvar_t& cvar, auto incr, auto min,
                             auto max) {
        Cvar_SetValue(cvar.name,
            CLAMP(min, isLeft ? cvar.value - incr : cvar.value + incr, max));
    };

    auto [ox, oy, oz, sc, rr, rp, ry] = getCvars();

    constexpr float oInc = 1.f;
    constexpr float oBound = 200.f;

    constexpr float rInc = 0.1f;
    constexpr float rBound = 90.f;

    switch(option)
    {
        case SbarOffsetMenuOpt::OffsetX:
            adjustF(ox, oInc, -oBound, oBound);
            break;
        case SbarOffsetMenuOpt::OffsetY:
            adjustF(oy, oInc, -oBound, oBound);
            break;
        case SbarOffsetMenuOpt::OffsetZ:
            adjustF(oz, oInc, -oBound, oBound);
            break;
        case SbarOffsetMenuOpt::Scale: adjustF(sc, 0.005f, 0.01f, 2.f); break;
        case SbarOffsetMenuOpt::Roll: adjustF(rr, rInc, -rBound, rBound); break;
        case SbarOffsetMenuOpt::Pitch:
            adjustF(rp, rInc, -rBound, rBound);
            break;
        case SbarOffsetMenuOpt::Yaw: adjustF(ry, rInc, -rBound, rBound); break;
        default: assert(false); break;
    }
}

void M_SbarOffset_Key(int key)
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
            sbaroff_cursor--;
            if(sbaroff_cursor < 0)
            {
                sbaroff_cursor = (int)SbarOffsetMenuOpt::Max - 1;
            }
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            sbaroff_cursor++;
            if(sbaroff_cursor >= (int)SbarOffsetMenuOpt::Max)
            {
                sbaroff_cursor = 0;
            }
            break;

        case K_LEFTARROW: [[fallthrough]];
        case K_RIGHTARROW:
            S_LocalSound("misc/menu3.wav");
            M_SbarOffset_KeyOption(key, (SbarOffsetMenuOpt)sbaroff_cursor);
            break;

        case K_ENTER:
            m_entersound = true;
            M_SbarOffset_KeyOption(key, (SbarOffsetMenuOpt)sbaroff_cursor);
            break;

        default: break;
    }
}

void M_SbarOffset_Draw()
{
    int y = 4;

    // plaque
    M_DrawTransPic(16, y, Draw_CachePic("gfx/qplaque.lmp"));

    // customize header
    qpic_t* p = Draw_CachePic("gfx/ttl_cstm.lmp");
    M_DrawPic((320 - p->width) / 2, y, p);

    y += 28;

    // title
    const char* title = "Status Bar Offset";
    M_PrintWhite((320 - 8 * strlen(title)) / 2, y, title);

    y += 16;
    int idx = 0;

    static const auto adjustedLabels = quake::util::makeAdjustedMenuLabels(
        "Offset X", "Offset Y", "Offset Z", "Scale", "Roll", "Pitch", "Yaw");

    static_assert(adjustedLabels.size() == (int)SbarOffsetMenuOpt::Max);

    for(const std::string& label : adjustedLabels)
    {
        M_Print(16, y, label.data());
        SbarOffset_MenuPrintOptionValue(240, y, (SbarOffsetMenuOpt)idx);

        // draw the blinking cursor
        if(sbaroff_cursor == idx)
        {
            M_DrawCharacter(220, y, 12 + ((int)(realtime * 4) & 1));
        }

        ++idx;
        y += 8;
    }
}

void M_Menu_SbarOffset_f()
{
    const char* sound = "items/r_item1.wav";

    IN_Deactivate(modestate == MS_WINDOWED);
    key_dest = key_menu;
    m_state = m_sbar_offset;
    m_entersound = true;

    SbarOffset_MenuPlaySound(sound, 0.5);
}
