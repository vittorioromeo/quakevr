#include "quakedef.hpp"
#include "vr.hpp"
#include "vr_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"

#include <string>
#include <cassert>
#include <array>

// TODO VR: move to other menu?
extern cvar_t r_particles;
extern cvar_t r_particle_mult;

static int vr_options_cursor = 0;

#define VR_MAX_TURN_SPEED 10.0f
#define VR_MAX_FLOOR_OFFSET 200.0f
#define VR_MAX_GUNANGLE 180.0f

extern void M_DrawSlider(int x, int y, float range);

static void VR_MenuPlaySound(const char* sound, float fvol)
{
    if(sfx_t* sfx = S_PrecacheSound(sound))
    {
        S_StartSound(cl.viewentity, 0, sfx, vec3_origin, fvol, 1);
    }
}

static void VR_MenuPrintOptionValue(int cx, int cy, VRMenuOpt option)
{
    char value_buffer[32] = {0};
    const char* value_string = nullptr;

    const auto printAsStr = [&](const auto& cvar) {
        snprintf(value_buffer, sizeof(value_buffer), "%.4f", cvar.value);
        M_Print(cx, cy, value_buffer);
    };

    const auto fmt = [&](const auto& fmtStr, const auto& cvar) {
        snprintf(value_buffer, sizeof(value_buffer), fmtStr, cvar.value);
        value_string = value_buffer;
    };

    switch(option)
    {
        case VRMenuOpt::VR_AIMMODE:
            switch((int)vr_aimmode.value)
            {
                case VrAimMode::e_HEAD_MYAW: value_string = "HEAD_MYAW"; break;
                case VrAimMode::e_HEAD_MYAW_MPITCH:
                    value_string = "HEAD_MYAW_MPITCH";
                    break;
                case VrAimMode::e_MOUSE_MYAW:
                    value_string = "MOUSE_MYAW";
                    break;
                case VrAimMode::e_MOUSE_MYAW_MPITCH:
                    value_string = "MOUSE_MYAW_MPITCH";
                    break;
                case VrAimMode::e_BLENDED: value_string = "BLENDED"; break;
                case VrAimMode::e_BLENDED_NOPITCH:
                    value_string = "BLENDED_NOPITCH";
                    break;
                case VrAimMode::e_CONTROLLER:
                    value_string = "CONTROLLER";
                    break;
                default: assert(false);
            }
            break;
        case VRMenuOpt::VR_DEADZONE:
            if(vr_deadzone.value > 0)
            {
                fmt("%.1f degrees", vr_deadzone);
            }
            else
            {
                value_string = "off";
            }
            break;
        case VRMenuOpt::VR_CROSSHAIR:
            if((int)vr_crosshair.value == 3)
            {
                value_string = "smooth line";
            }
            else if((int)vr_crosshair.value == 2)
            {
                value_string = "line";
            }
            else if((int)vr_crosshair.value == 1)
            {
                value_string = "point";
            }
            else
            {
                value_string = "off";
            }
            break;
        case VRMenuOpt::VR_CROSSHAIR_DEPTH:
            if(vr_crosshair_depth.value > 0)
            {
                fmt("%.1f units", vr_crosshair_depth);
            }
            else
            {
                value_string = "off";
            }
            break;
        case VRMenuOpt::VR_CROSSHAIR_SIZE:
            if(vr_crosshair_size.value > 0)
            {
                fmt("%.1f pixels", vr_crosshair_size);
                value_string = value_buffer;
            }
            else
            {
                value_string = "off";
            }
            break;
        case VRMenuOpt::VR_CROSSHAIR_ALPHA:
            M_DrawSlider(cx, cy, vr_crosshair_alpha.value);
            break;
        case VRMenuOpt::VR_WORLD_SCALE:
            M_DrawSlider(cx, cy, vr_world_scale.value / 2.0f);
            break;
        case VRMenuOpt::VR_MOVEMENT_MODE:
            switch((int)vr_movement_mode.value)
            {
                case VrMovementMode::e_FOLLOW_HAND:
                    value_string = "Follow hand";
                    break;
                case VrMovementMode::e_RAW_INPUT:
                    value_string = "Raw input";
                    break;
            }
            break;
        case VRMenuOpt::VR_ENABLE_JOYSTICK_TURN:
            value_string = vr_enable_joystick_turn.value == 0 ? "Off" : "On";
            break;
        case VRMenuOpt::VR_SNAP_TURN:
            if(vr_snap_turn.value == 0)
            {
                value_string = "Smooth";
            }
            else
            {
                fmt("%.2f Degrees", vr_snap_turn);
            }
            break;
        case VRMenuOpt::VR_TURN_SPEED:
            M_DrawSlider(cx, cy, vr_turn_speed.value / VR_MAX_TURN_SPEED);
            break;
        case VRMenuOpt::VR_MSAA:
            if(vr_msaa.value == 0)
            {
                value_string = "Off";
            }
            else
            {
                fmt("%.0f Samples", vr_msaa);
            }
            break;
        case VRMenuOpt::VR_SBAR_MODE:
        {
            static std::array strings{"Main Hand", "Off Hand"};
            value_string = strings[(int)vr_sbar_mode.value];
            break;
        }
        case VRMenuOpt::VR_GUNANGLE: printAsStr(vr_gunangle); break;
        case VRMenuOpt::VR_FLOOR_OFFSET: printAsStr(vr_floor_offset); break;
        case VRMenuOpt::VR_GUNMODELPITCH: printAsStr(vr_gunmodelpitch); break;
        case VRMenuOpt::VR_GUNMODELSCALE: printAsStr(vr_gunmodelscale); break;
        case VRMenuOpt::VR_GUNMODELY: printAsStr(vr_gunmodely); break;
        case VRMenuOpt::VR_CROSSHAIRY: printAsStr(vr_crosshairy); break;
        case VRMenuOpt::VR_HUD_SCALE: printAsStr(vr_hud_scale); break;
        case VRMenuOpt::VR_MENU_SCALE: printAsStr(vr_menu_scale); break;
        case VRMenuOpt::VR_GUNYAW: printAsStr(vr_gunyaw); break;
        case VRMenuOpt::VR_GUN_Z_OFFSET: printAsStr(vr_gun_z_offset); break;
        case VRMenuOpt::VR_VIEWKICK:
            value_string = vr_viewkick.value == 0 ? "Off" : "On";
            break;
        case VRMenuOpt::VR_MENU_DISTANCE: printAsStr(vr_menu_distance); break;
        case VRMenuOpt::VR_PARTICLES:
            value_string = r_particles.value == 0 ? "Off" : "On";
            break;
        case VRMenuOpt::VR_PARTICLE_MULT: printAsStr(r_particle_mult); break;
        default: assert(false); break;
    }

    if(value_string)
    {
        M_Print(cx, cy, value_string);
    }
}

static void M_VR_KeyOption(int key, VRMenuOpt option)
{
#define _sizeofarray(x) ((sizeof(x) / sizeof(x[0])))
#define _maxarray(x) (_sizeofarray(x) - 1)

    const bool isLeft = (key == K_LEFTARROW);
    int intValue = 0;

    int aimmode[] = {1, 2, 3, 4, 5, 6, 7};
    int deadzoneDiff = 5;
    int crosshair[] = {0, 1, 2, 3};

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

    switch(option)
    {
        case VRMenuOpt::VR_AIMMODE:
            intValue = (int)vr_aimmode.value;
            intValue = CLAMP(aimmode[0], isLeft ? intValue - 1 : intValue + 1,
                (int)_sizeofarray(aimmode));
            intValue -= 1;
            Cvar_SetValue("vr_aimmode", aimmode[intValue]);
            break;
        case VRMenuOpt::VR_DEADZONE:
            adjustF(vr_deadzone, deadzoneDiff, 0.f, 180.f);
            break;
        case VRMenuOpt::VR_CROSSHAIR:
            adjustI(
                vr_crosshair, 1, crosshair[0], crosshair[_maxarray(crosshair)]);
            break;
        case VRMenuOpt::VR_CROSSHAIR_DEPTH:
            adjustF(vr_crosshair_depth, 16.f, 0.f, 4096.f);
            break;
        case VRMenuOpt::VR_CROSSHAIR_SIZE:
            adjustF(vr_crosshair_size, 0.5f, 0.f, 32.f);
            break;
        case VRMenuOpt::VR_CROSSHAIR_ALPHA:
            adjustF(vr_crosshair_alpha, 0.05f, 0.f, 1.f);
            break;
        case VRMenuOpt::VR_WORLD_SCALE:
            adjustF(vr_world_scale, 0.05f, 0.f, 2.f);
            break;
        case VRMenuOpt::VR_MOVEMENT_MODE:
            adjustI(vr_movement_mode, 1, 0, VrMovementMode::k_Max - 1);
            break;
        case VRMenuOpt::VR_ENABLE_JOYSTICK_TURN:
            adjustI(vr_enable_joystick_turn, 1, 0, 1);
            break;
        case VRMenuOpt::VR_SNAP_TURN: adjustI(vr_snap_turn, 5, 0, 90); break;
        case VRMenuOpt::VR_TURN_SPEED:
            adjustF(vr_turn_speed, 0.25f, 0.f, VR_MAX_TURN_SPEED);
            break;
        case VRMenuOpt::VR_MSAA:
            int max;
            glGetIntegerv(GL_MAX_SAMPLES, &max);
            adjustI(vr_msaa, 1, 0, max);
            break;
        case VRMenuOpt::VR_GUNANGLE:
            adjustF(vr_gunangle, 0.75f, -VR_MAX_GUNANGLE, VR_MAX_GUNANGLE);
            break;
        case VRMenuOpt::VR_FLOOR_OFFSET:
            adjustF(vr_floor_offset, 2.5f, -VR_MAX_FLOOR_OFFSET,
                VR_MAX_FLOOR_OFFSET);
            break;
        case VRMenuOpt::VR_GUNMODELPITCH:
            adjustF(vr_gunmodelpitch, 0.25f, -90.f, 90.f);
            break;
        case VRMenuOpt::VR_GUNMODELSCALE:
            adjustF(vr_gunmodelscale, 0.05f, 0.1f, 2.f);
            break;
        case VRMenuOpt::VR_GUNMODELY:
            adjustF(vr_gunmodely, 0.1f, -5.0f, 5.f);
            break;
        case VRMenuOpt::VR_CROSSHAIRY:
            adjustF(vr_crosshairy, 0.05f, -10.0f, 10.f);
            break;
        case VRMenuOpt::VR_HUD_SCALE:
            adjustF(vr_hud_scale, 0.005f, 0.01f, 0.1f);
            break;
        case VRMenuOpt::VR_MENU_SCALE:
            adjustF(vr_menu_scale, 0.01f, 0.05f, 0.6f);
            break;
        case VRMenuOpt::VR_GUNYAW:
            adjustF(vr_gunyaw, 0.25f, -90.f, 90.f);
            break;
        case VRMenuOpt::VR_GUN_Z_OFFSET:
            adjustF(vr_gun_z_offset, 0.25f, -30.f, 30.f);
            break;
        case VRMenuOpt::VR_SBAR_MODE: adjustI(vr_sbar_mode, 1, 0, 1); break;
        case VRMenuOpt::VR_VIEWKICK: adjustI(vr_viewkick, 1, 0, 1); break;
        case VRMenuOpt::VR_MENU_DISTANCE:
            adjustI(vr_menu_distance, 1, 24, 256);
            break;
        case VRMenuOpt::VR_PARTICLES: adjustI(r_particles, 1, 0, 1); break;
        case VRMenuOpt::VR_PARTICLE_MULT:
            adjustF(r_particle_mult, 0.25f, 0.25f, 10.f);
            break;
        default: assert(false); break;
    }

#undef _maxarray
#undef _sizeofarray
}

void M_VR_Key(int key)
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
            vr_options_cursor--;
            if(vr_options_cursor < 0)
            {
                vr_options_cursor = (int)VRMenuOpt::VR_MAX - 1;
            }
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            vr_options_cursor++;
            if(vr_options_cursor >= (int)VRMenuOpt::VR_MAX)
            {
                vr_options_cursor = 0;
            }
            break;

        case K_LEFTARROW: [[fallthrough]];
        case K_RIGHTARROW:
            S_LocalSound("misc/menu3.wav");
            M_VR_KeyOption(key, (VRMenuOpt)vr_options_cursor);
            break;

        case K_ENTER:
            m_entersound = true;
            M_VR_KeyOption(key, (VRMenuOpt)vr_options_cursor);
            break;

        default: break;
    }
}

void M_VR_Draw()
{
    int y = 4;

    // plaque
    M_DrawTransPic(16, y, Draw_CachePic("gfx/qplaque.lmp"));

    // customize header
    qpic_t* p = Draw_CachePic("gfx/ttl_cstm.lmp");
    M_DrawPic((320 - p->width) / 2, y, p);

    y += 28;

    // title
    const char* title = "VR OPTIONS";
    M_PrintWhite((320 - 8 * strlen(title)) / 2, y, title);

    y += 16;
    int idx = 0;

    static const auto adjustedLabels = quake::util::makeAdjustedMenuLabels(
        "Aim Mode", "Deadzone", "Crosshair", "Crosshair Depth",
        "Crosshair Size", "Crosshair Alpha", "World Scale", "Movement mode",
        "Enable Joystick Turn", "Turn", "Turn Speed", "MSAA", "Gun Angle",
        "Floor Offset", "Gun Model Pitch", "Gun Model Scale",
        "Gun Model Z Offset", "Crosshair Z Offset", "HUD Scale", "Menu Scale",
        "Gun Yaw", "Gun Z Offset", "Status Bar Mode", "Viewkick",
        "Menu Distance", "Particle Effects", "Particle Multiplier");

    static_assert(adjustedLabels.size() == (int)VRMenuOpt::VR_MAX);

    for(const std::string& label : adjustedLabels)
    {
        M_Print(16, y, label.data());
        VR_MenuPrintOptionValue(240, y, (VRMenuOpt)idx);

        // draw the blinking cursor
        if(vr_options_cursor == idx)
        {
            M_DrawCharacter(220, y, 12 + ((int)(realtime * 4) & 1));
        }

        ++idx;
        y += 8;
    }
}

void M_Menu_VR_f()
{
    const char* sound = "items/r_item1.wav";

    IN_Deactivate(modestate == MS_WINDOWED);
    key_dest = key_menu;
    m_state = m_vr;
    m_entersound = true;

    VR_MenuPlaySound(sound, 0.5);
}

// TODO VR:
// * difficulty options
// * vignette?
