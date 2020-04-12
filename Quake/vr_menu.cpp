#include "quakedef.hpp"
#include "vr.hpp"
#include "vr_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"
#include "menu_util.hpp"

#include <string>
#include <cassert>
#include <array>

// TODO VR: move to other menu?
extern cvar_t r_particles;
extern cvar_t r_particle_mult;

[[nodiscard]] static quake::menu make_menu()
{
    constexpr float max_turn_speed = 10.0f;
    constexpr float max_floor_offset = 200.0f;
    constexpr float max_gunangle = 180.0f;

    int max_msaa;
    glGetIntegerv(GL_MAX_SAMPLES, &max_msaa);

    quake::menu m{"VR Options"};

    m.add_cvar_getter_enum_entry<VrAimMode>( //
        "Aim Mode",                          //
        [] { return &vr_aimmode; },          //
        "HEAD_MYAW",                         //
        "HEAD_MYAW_MPITCH",                  //
        "MOUSE_MYAW",                        //
        "MOUSE_MYAW_MPITCH",                 //
        "BLENDED",                           //
        "BLENDED_NOPITCH",                   //
        "CONTROLLER"                         //
    );

    m.add_cvar_entry<float>("Deadzone", vr_deadzone, {5.f, 0.f, 180.f});

    m.add_cvar_getter_enum_entry<int>(        //
        "Crosshair",                          //
        [] { return &vr_crosshair; },         //
        "Off", "Point", "Line", "Smooth line" //
    );

    m.add_cvar_entry<float>(
        "Crosshair Depth", vr_crosshair_depth, {16.f, 0.f, 4096.f});
    m.add_cvar_entry<float>(
        "Crosshair Size", vr_crosshair_size, {0.5f, 0.f, 32.f});
    m.add_cvar_entry<float>(
        "Crosshair Alpha", vr_crosshair_alpha, {0.05f, 0.f, 1.f});
    m.add_cvar_entry<float>("World Scale", vr_world_scale, {0.05f, 0.f, 2.f});

    m.add_cvar_getter_enum_entry<int>(    //
        "Movement Mode",                  //
        [] { return &vr_movement_mode; }, //
        "Follow Hand", "Raw Input"        //
    );

    m.add_cvar_entry<bool>("Enable Joystick Turn", vr_enable_joystick_turn);

    {
        auto& e = m.add_cvar_entry<int>("Turn", vr_snap_turn, {5, 0, 90});
        e._printer = [](char* buf, const int buf_size, const int x) {
            if(x == 0)
            {
                snprintf(buf, buf_size, "Smooth");
            }
            else
            {
                snprintf(buf, buf_size, "%d Degrees", x);
            }
        };
    }

    m.add_cvar_entry<float>(
        "Turn Speed", vr_turn_speed, {0.25f, 0.f, max_turn_speed});
    m.add_cvar_entry<int>("MSAA Samples", vr_msaa, {1, 0, max_msaa});

    m.add_cvar_entry<float>(
        "Gun Angle", vr_gunangle, {0.75f, -max_gunangle, max_gunangle});
    m.add_cvar_entry<float>("Floor Offset", vr_floor_offset,
        {2.5f, -max_floor_offset, max_floor_offset});
    m.add_cvar_entry<float>(
        "Gun Model Pitch", vr_gunmodelpitch, {0.25f, -90.f, 90.f});
    m.add_cvar_entry<float>(
        "Gun Model Scale", vr_gunmodelscale, {0.05f, 0.1f, 2.f});
    m.add_cvar_entry<float>(
        "Gun Model Z Offset", vr_gunmodely, {0.1f, -5.0f, 5.f});
    m.add_cvar_entry<float>(
        "Crosshair Z Offset", vr_crosshairy, {0.05f, -10.0f, 10.f});
    m.add_cvar_entry<float>("HUD Scale", vr_hud_scale, {0.005f, 0.01f, 0.1f});
    m.add_cvar_entry<float>("Menu Scale", vr_menu_scale, {0.01f, 0.05f, 0.6f});
    m.add_cvar_entry<float>("Gun Yaw", vr_gunyaw, {0.25f, -90.f, 90.f});
    m.add_cvar_entry<float>(
        "Gun Z Offset", vr_gun_z_offset, {0.25f, -30.f, 30.f});

    m.add_cvar_getter_enum_entry<int>( //
        "Status Bar Mode",             //
        [] { return &vr_sbar_mode; },  //
        "Main Hand", "Off Hand"        //
    );

    m.add_cvar_entry<bool>("Viewkick", vr_viewkick);
    m.add_cvar_entry<float>("Menu Distance", vr_menu_distance, {1, 24, 256});
    m.add_cvar_entry<bool>("Particle Effects", r_particles);
    m.add_cvar_entry<float>(
        "Particle Multiplier", r_particle_mult, {0.25f, 0.25f, 10.f});
    m.add_cvar_entry<float>(
        "Off-Hand Pitch", vr_offhandpitch, {0.25f, -90.f, 90.f});
    m.add_cvar_entry<float>(
        "Off-Hand Yaw", vr_offhandyaw, {0.25f, -90.f, 90.f});

    m.add_cvar_entry<bool>("Holster Haptics", vr_holster_haptics);

    // TODO VR: menu tooltips

    return m;
}

static quake::menu g_menu = make_menu();

void M_VR_Key(int key)
{
    g_menu.key(key);
}

void M_VR_Draw()
{
    g_menu.draw();
}
