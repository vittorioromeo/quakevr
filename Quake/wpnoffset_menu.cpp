#include "quakedef.hpp"
#include "vr.hpp"
#include "wpnoffset_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"

#include <string>
#include <cassert>
#include <array>
#include <tuple>

static bool wpnoff_offhand = false;
static int wpnoff_cursor = 0;

static void WpnOffset_MenuPlaySound(const char* sound, float fvol)
{
    if(sfx_t* const sfx = S_PrecacheSound(sound))
    {
        S_StartSound(cl.viewentity, 0, sfx, vec3_zero, fvol, 1);
    }
}

static auto getCvars()
{
    const auto idx = wpnoff_offhand ? VR_GetOffHandWpnCvarEntry()
                                    : VR_GetMainHandWpnCvarEntry();

    // clang-format off
    return std::tie(
        VR_GetWpnCVar(idx, WpnCVar::OffsetX),
        VR_GetWpnCVar(idx, WpnCVar::OffsetY),
        VR_GetWpnCVar(idx, WpnCVar::OffsetZ),
        VR_GetWpnCVar(idx, WpnCVar::Scale),
        VR_GetWpnCVar(idx, WpnCVar::Roll),
        VR_GetWpnCVar(idx, WpnCVar::Pitch),
        VR_GetWpnCVar(idx, WpnCVar::Yaw),
        VR_GetWpnCVar(idx, WpnCVar::MuzzleOffsetX),
        VR_GetWpnCVar(idx, WpnCVar::MuzzleOffsetY),
        VR_GetWpnCVar(idx, WpnCVar::MuzzleOffsetZ),
        VR_GetWpnCVar(idx, WpnCVar::TwoHOffsetX),
        VR_GetWpnCVar(idx, WpnCVar::TwoHOffsetY),
        VR_GetWpnCVar(idx, WpnCVar::TwoHOffsetZ),
        VR_GetWpnCVar(idx, WpnCVar::TwoHPitch),
        VR_GetWpnCVar(idx, WpnCVar::TwoHYaw),
        VR_GetWpnCVar(idx, WpnCVar::TwoHRoll),
        VR_GetWpnCVar(idx, WpnCVar::TwoHMode),
        VR_GetWpnCVar(idx, WpnCVar::Length),
        VR_GetWpnCVar(idx, WpnCVar::Weight),
        VR_GetWpnCVar(idx, WpnCVar::HandOffsetX),
        VR_GetWpnCVar(idx, WpnCVar::HandOffsetY),
        VR_GetWpnCVar(idx, WpnCVar::HandOffsetZ),
        VR_GetWpnCVar(idx, WpnCVar::HandAnchorVertex),
        VR_GetWpnCVar(idx, WpnCVar::OffHandOffsetX),
        VR_GetWpnCVar(idx, WpnCVar::OffHandOffsetY),
        VR_GetWpnCVar(idx, WpnCVar::OffHandOffsetZ)
    );
    // clang-format on
}

static void WpnOffset_MenuPrintOptionValue(
    const int cx, const int cy, const WpnOffsetMenuOpt option)
{
    char value_buffer[32] = {0};

    const auto printBool = [&](const auto& value) {
        snprintf(value_buffer, sizeof(value_buffer), value ? "On" : "Off");
        M_Print(cx, cy, value_buffer);
    };

    const auto printAsStr = [&](const auto& cvar) {
        snprintf(value_buffer, sizeof(value_buffer), "%.4f", cvar.value);
        M_Print(cx, cy, value_buffer);
    };

    const auto& [ox, oy, oz, sc, rr, rp, ry, mx, my, mz, thox, thoy, thoz, thrp,
        thry, thrr, thmode, len, wgh, hox, hoy, hoz, hav, ohox, ohoy, ohoz] = getCvars();

    switch(option)
    {
        case WpnOffsetMenuOpt::OffHand: printBool(wpnoff_offhand); break;
        case WpnOffsetMenuOpt::OffsetX: printAsStr(ox); break;
        case WpnOffsetMenuOpt::OffsetY: printAsStr(oy); break;
        case WpnOffsetMenuOpt::OffsetZ: printAsStr(oz); break;
        case WpnOffsetMenuOpt::Scale: printAsStr(sc); break;
        case WpnOffsetMenuOpt::Roll: printAsStr(rr); break;
        case WpnOffsetMenuOpt::Pitch: printAsStr(rp); break;
        case WpnOffsetMenuOpt::Yaw: printAsStr(ry); break;
        case WpnOffsetMenuOpt::MuzzleOffsetX: printAsStr(mx); break;
        case WpnOffsetMenuOpt::MuzzleOffsetY: printAsStr(my); break;
        case WpnOffsetMenuOpt::MuzzleOffsetZ: printAsStr(mz); break;
        case WpnOffsetMenuOpt::TwoHOffsetX: printAsStr(thox); break;
        case WpnOffsetMenuOpt::TwoHOffsetY: printAsStr(thoy); break;
        case WpnOffsetMenuOpt::TwoHOffsetZ: printAsStr(thoz); break;
        case WpnOffsetMenuOpt::TwoHPitch: printAsStr(thrp); break;
        case WpnOffsetMenuOpt::TwoHYaw: printAsStr(thry); break;
        case WpnOffsetMenuOpt::TwoHRoll: printAsStr(thrr); break;
        case WpnOffsetMenuOpt::TwoHMode:
        {
            const auto mode =
                static_cast<Wpn2HMode>(static_cast<int>(thmode.value));

            if(mode == Wpn2HMode::Default)
            {
                M_Print(cx, cy, "Default");
            }
            else if(mode == Wpn2HMode::NoVirtualStock)
            {
                M_Print(cx, cy, "Ignore Virtual Stock");
            }
            else if(mode == Wpn2HMode::Forbidden)
            {
                M_Print(cx, cy, "Forbidden");
            }
            break;
        }
        case WpnOffsetMenuOpt::Length: printAsStr(len); break;
        case WpnOffsetMenuOpt::Weight: printAsStr(wgh); break;
        case WpnOffsetMenuOpt::HandOffsetX: printAsStr(hox); break;
        case WpnOffsetMenuOpt::HandOffsetY: printAsStr(hoy); break;
        case WpnOffsetMenuOpt::HandOffsetZ: printAsStr(hoz); break;
        case WpnOffsetMenuOpt::HandAnchorVertex: printAsStr(hav); break;
        case WpnOffsetMenuOpt::OffHandOffsetX: printAsStr(ohox); break;
        case WpnOffsetMenuOpt::OffHandOffsetY: printAsStr(ohoy); break;
        case WpnOffsetMenuOpt::OffHandOffsetZ: printAsStr(ohoz); break;
        default: assert(false); break;
    }
}

static void M_WpnOffset_KeyOption(int key, WpnOffsetMenuOpt option)
{
    const bool isLeft = (key == K_LEFTARROW);
    const auto adjustF = quake::util::makeMenuAdjuster<float>(isLeft);
    const auto adjustI = quake::util::makeMenuAdjuster<int>(isLeft);

    const auto& [ox, oy, oz, sc, rr, rp, ry, mx, my, mz, thox, thoy, thoz, thrp,
        thry, thrr, thmode, len, wgh, hox, hoy, hoz, hav, ohox, ohoy, ohoz] =
        getCvars();

    const float oInc = VR_GetMenuMult() == 2 ? 1.5f : 0.1f;
    constexpr float oBound = 100.f;

    const float rInc = VR_GetMenuMult() == 2 ? 1.5f : 0.1f;
    constexpr float rBound = 90.f;

    const float wInc = VR_GetMenuMult() == 2 ? 1.5f : 0.01f;
    constexpr float wBound = 1.f;

    switch(option)
    {
        case WpnOffsetMenuOpt::OffHand: wpnoff_offhand = !wpnoff_offhand; break;
        case WpnOffsetMenuOpt::OffsetX:
            adjustF(ox, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::OffsetY:
            adjustF(oy, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::OffsetZ:
            adjustF(oz, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::Scale: adjustF(sc, 0.01f, 0.01f, 2.f); break;
        case WpnOffsetMenuOpt::Roll: adjustF(rr, rInc, -rBound, rBound); break;
        case WpnOffsetMenuOpt::Pitch: adjustF(rp, rInc, -rBound, rBound); break;
        case WpnOffsetMenuOpt::Yaw: adjustF(ry, rInc, -rBound, rBound); break;
        case WpnOffsetMenuOpt::MuzzleOffsetX:
            adjustF(mx, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::MuzzleOffsetY:
            adjustF(my, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::MuzzleOffsetZ:
            adjustF(mz, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::TwoHOffsetX:
            adjustF(thox, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::TwoHOffsetY:
            adjustF(thoy, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::TwoHOffsetZ:
            adjustF(thoz, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::TwoHPitch:
            adjustF(thrp, rInc, -rBound, rBound);
            break;
        case WpnOffsetMenuOpt::TwoHYaw:
            adjustF(thry, rInc, -rBound, rBound);
            break;
        case WpnOffsetMenuOpt::TwoHRoll:
            adjustF(thrr, rInc, -rBound, rBound);
            break;
        case WpnOffsetMenuOpt::TwoHMode: adjustI(thmode, 1, 0, 2); break;
        case WpnOffsetMenuOpt::Length: adjustF(len, oInc, 0.f, oBound); break;
        case WpnOffsetMenuOpt::Weight: adjustF(wgh, wInc, 0.f, wBound); break;
        case WpnOffsetMenuOpt::HandOffsetX:
            adjustF(hox, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::HandOffsetY:
            adjustF(hoy, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::HandOffsetZ:
            adjustF(hoz, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::HandAnchorVertex:
            adjustI(hav, 1, 0, 1024);
            break;
        case WpnOffsetMenuOpt::OffHandOffsetX:
            adjustF(ohox, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::OffHandOffsetY:
            adjustF(ohoy, oInc, -oBound, oBound);
            break;
        case WpnOffsetMenuOpt::OffHandOffsetZ:
            adjustF(ohoz, oInc, -oBound, oBound);
            break;
        default: assert(false); break;
    }
}

void M_WpnOffset_Key(int key)
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
            wpnoff_cursor--;
            if(wpnoff_cursor < 0)
            {
                wpnoff_cursor = (int)WpnOffsetMenuOpt::Max - 1;
            }
            break;
        }

        case K_DOWNARROW:
        {
            S_LocalSound("misc/menu1.wav");
            wpnoff_cursor++;
            if(wpnoff_cursor >= (int)WpnOffsetMenuOpt::Max)
            {
                wpnoff_cursor = 0;
            }
            break;
        }

        case K_LEFTARROW: [[fallthrough]];
        case K_RIGHTARROW:
        {
            S_LocalSound("misc/menu3.wav");
            M_WpnOffset_KeyOption(key, (WpnOffsetMenuOpt)wpnoff_cursor);
            break;
        }

        case K_ENTER:
        {
            m_entersound = true;
            M_WpnOffset_KeyOption(key, (WpnOffsetMenuOpt)wpnoff_cursor);
            break;
        }
    }
}

void M_WpnOffset_Draw()
{
    int y = 4;

    // plaque
    M_DrawTransPic(16, y, Draw_CachePic("gfx/qplaque.lmp"));

    // customize header
    qpic_t* p = Draw_CachePic("gfx/ttl_cstm.lmp");
    M_DrawPic((320 - p->width) / 2, y, p);

    y += 28;

    // title
    const char* title = "Weapon Offsets";
    M_PrintWhite((320 - 8 * strlen(title)) / 2, y, title);

    y += 16;
    int idx = 0;

    static const auto adjustedLabels =
        quake::util::makeAdjustedMenuLabels("Offhand", "Offset X", "Offset Y",
            "Offset Z", "Scale", "Roll", "Pitch", "Yaw", "Muzzle Offset X",
            "Muzzle Offset Y", "Muzzle Offset Z", "2H Offset X", "2H Offset Y",
            "2H Offset Z", "2H Aim Pitch", "2H Aim Yaw", "2H Aim Roll",
            "2H Mode", "Gun Length", "Gun Weight", "Hand Offset X",
            "Hand Offset Y", "Hand Offset Z", "Hand Anchor Vertex", "Offhand Offset X",
            "Offhand Offset Y", "Offhand Offset Z");

    static_assert(adjustedLabels.size() == (int)WpnOffsetMenuOpt::Max);

    for(const std::string& label : adjustedLabels)
    {
        M_Print(16, y, label.data());
        WpnOffset_MenuPrintOptionValue(240, y, (WpnOffsetMenuOpt)idx);

        // draw the blinking cursor
        if(wpnoff_cursor == idx)
        {
            M_DrawCharacter(220, y, 12 + ((int)(realtime * 4) & 1));
        }

        ++idx;
        y += 8;
    }
}

void M_Menu_WpnOffset_f()
{
    const char* sound = "items/r_item1.wav";

    IN_Deactivate(modestate == MS_WINDOWED);
    key_dest = key_menu;
    m_state = m_wpn_offset;
    m_entersound = true;

    WpnOffset_MenuPlaySound(sound, 0.5);
}
