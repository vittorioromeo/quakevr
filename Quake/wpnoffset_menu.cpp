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
        S_StartSound(cl.viewentity, 0, sfx, vec3_origin, fvol, 1);
    }
}

static auto getCvars()
{
    // TODO VR: hardcoded hand/fist cvar number
    const auto idx = wpnoff_offhand ? 16 : weaponCVarEntry;

    // clang-format off
    return std::tie(
        vr_weapon_offset[idx * VARS_PER_WEAPON],     // OffsetX
        vr_weapon_offset[idx * VARS_PER_WEAPON + 1], // OffsetY
        vr_weapon_offset[idx * VARS_PER_WEAPON + 2], // OffsetZ
        vr_weapon_offset[idx * VARS_PER_WEAPON + 3], // Scale
        vr_weapon_offset[idx * VARS_PER_WEAPON + 7], // Roll
        vr_weapon_offset[idx * VARS_PER_WEAPON + 5], // Pitch
        vr_weapon_offset[idx * VARS_PER_WEAPON + 6], // Yaw
        vr_weapon_offset[idx * VARS_PER_WEAPON + 8], // MuzzleOffsetX
        vr_weapon_offset[idx * VARS_PER_WEAPON + 9], // MuzzleOffsetY
        vr_weapon_offset[idx * VARS_PER_WEAPON + 10] // MuzzleOffsetZ
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

    const auto& [ox, oy, oz, sc, rr, rp, ry, mx, my, mz] = getCvars();

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
        default: assert(false); break;
    }
}

static void M_WpnOffset_KeyOption(int key, WpnOffsetMenuOpt option)
{
    const bool isLeft = (key == K_LEFTARROW);

    const auto adjustF = [&isLeft](const cvar_t& cvar, auto incr, auto min,
                             auto max) {
        Cvar_SetValue(cvar.name,
            CLAMP(min, isLeft ? cvar.value - incr : cvar.value + incr, max));
    };

    const auto& [ox, oy, oz, sc, rr, rp, ry, mx, my, mz] = getCvars();

    const float oInc = in_speed.state ? 5.f : 0.1f;
    constexpr float oBound = 100.f;

    const float rInc = in_speed.state ? 5.f : 0.5f;
    constexpr float rBound = 90.f;

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

    static const auto adjustedLabels = quake::util::makeAdjustedMenuLabels(
        "Offhand", "Offset X", "Offset Y", "Offset Z", "Scale", "Roll", "Pitch",
        "Yaw", "Muzzle Offset X", "Muzzle Offset Y", "Muzzle Offset Z");

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
