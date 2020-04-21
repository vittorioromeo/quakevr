#include "quakedef.hpp"
#include "vr.hpp"
#include "debug_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"
#include "menu_util.hpp"

#include <string>
#include <cassert>
#include <array>

extern cvar_t r_showbboxes;
extern cvar_t r_shadows;
extern cvar_t host_timescale;
extern cvar_t skill;

[[nodiscard]] static quake::menu make_menu()
{
    const auto runCmd = [](const char* cmd) {
        return [cmd] {
            quake::menu_util::playMenuSound("items/r_item2.wav", 0.5);
            Cmd_ExecuteString(cmd, cmd_source_t::src_command);
        };
    };

    quake::menu m{"Debug", &M_Menu_QuakeVRSettings_f};

    m.add_action_entry("Impulse 9 (Give All)", runCmd("impulse 9"));
    m.add_action_entry("Impulse 11 (Rune)", runCmd("impulse 11"));
    m.add_action_entry("Impulse 14 (Spawn All)", runCmd("impulse 14"));
    m.add_action_entry("Impulse 255 (Quad)", runCmd("impulse 255"));
    m.add_action_entry("God Mode", runCmd("god"));
    m.add_action_entry("Noclip", runCmd("noclip"));
    m.add_action_entry("Fly", runCmd("fly"));

    m.add_separator();

    m.add_cvar_getter_enum_entry<int>(        //
        "Skill",                              //
        [] { return &skill; },                //
        "Easy", "Normal", "Hard", "Nightmare" //
    );

    m.add_separator();

    m.add_cvar_entry<bool>("Show BBoxes", r_showbboxes);
    m.add_cvar_entry<bool>("Show Shadows", r_shadows);

    m.add_cvar_getter_enum_entry<VrPlayerShadows>( //
        "Player Shadows",                          //
        [] { return &vr_player_shadows; },         //
        "Off",                                     //
        "View Entities",                           //
        "Third Person",                            //
        "Both");

    m.add_separator();

    m.add_cvar_entry<bool>(
        "Show VR Torso Debug Lines", vr_vrtorso_debuglines_enabled);

    m.add_cvar_entry<bool>("Fake VR Mode", vr_fakevr);

    m.add_cvar_entry<float>("Timescale", host_timescale, {0.05f, 0.f, 5.f});

    m.add_cvar_entry<bool>("Print Handvel", vr_debug_print_handvel);
    m.add_cvar_entry<bool>("Show Hand Pos/Rot", vr_debug_show_hand_pos_and_rot);

    return m;
}

static quake::menu g_menu = make_menu();

quake::menu& M_Debug_Menu()
{
    return g_menu;
}

void M_Debug_Key(int key)
{
    g_menu.key(key);
}

void M_Debug_Draw()
{
    g_menu.draw();
}
