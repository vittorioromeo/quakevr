#include "quakedef.hpp"
#include "vr.hpp"
#include "wpnoffset_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"
#include "menu_util.hpp"

static bool wpnoff_offhand = false;

static int getIdx()
{
    return wpnoff_offhand ? VR_GetOffHandWpnCvarEntry()
                          : VR_GetMainHandWpnCvarEntry();
}

[[nodiscard]] static quake::menu make_menu()
{
    const float oInc = VR_GetMenuMult() == 2 ? 1.5f : 0.1f;
    constexpr float oBound = 100.f;

    const float rInc = VR_GetMenuMult() == 2 ? 1.5f : 0.1f;
    constexpr float rBound = 90.f;

    const float wInc = VR_GetMenuMult() == 2 ? 1.5f : 0.01f;
    constexpr float wBound = 1.f;

    const quake::menu_bounds<float> oBounds{oInc, -oBound, oBound};
    const quake::menu_bounds<float> rBounds{rInc, -rBound, rBound};
    const quake::menu_bounds<float> wBounds{wInc, 0.f, wBound};

    quake::menu m{"Weapon Offsets"};

    const auto o_wpncvar = [&](const char* title, const WpnCVar c) {
        m.add_cvar_getter_entry<float>(                  //
            title,                                       //
            [c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            oBounds                                      //
        );
    };

    const auto r_wpncvar = [&](const char* title, const WpnCVar c) {
        m.add_cvar_getter_entry<float>(                  //
            title,                                       //
            [c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            rBounds                                      //
        );
    };

    m.add_getter_entry<bool>(          //
        "Off-Hand",                    //
        [] { return &wpnoff_offhand; } //
    );

    o_wpncvar("X", WpnCVar::OffsetX);
    o_wpncvar("Y", WpnCVar::OffsetY);
    o_wpncvar("Z", WpnCVar::OffsetZ);

    o_wpncvar("Scale", WpnCVar::Scale);

    r_wpncvar("Roll", WpnCVar::Roll);
    r_wpncvar("Pitch", WpnCVar::Pitch);
    r_wpncvar("Yaw", WpnCVar::Yaw);

    o_wpncvar("Muzzle X", WpnCVar::MuzzleOffsetX);
    o_wpncvar("Muzzle Y", WpnCVar::MuzzleOffsetY);
    o_wpncvar("Muzzle Z", WpnCVar::MuzzleOffsetZ);

    o_wpncvar("2H X", WpnCVar::TwoHOffsetX);
    o_wpncvar("2H Y", WpnCVar::TwoHOffsetY);
    o_wpncvar("2H Z", WpnCVar::TwoHOffsetZ);

    r_wpncvar("2H Pitch", WpnCVar::TwoHPitch);
    r_wpncvar("2H Yaw", WpnCVar::TwoHYaw);
    r_wpncvar("2H Roll", WpnCVar::TwoHRoll);

    o_wpncvar("Weight", WpnCVar::Weight);

    m.add_cvar_getter_entry<int>(                                           //
        "Hand Anchor Vertex",                                               //
        [] { return &VR_GetWpnCVar(getIdx(), WpnCVar::HandAnchorVertex); }, //
        {1, 0, 1024}                                                        //
    );

    o_wpncvar("Hand X", WpnCVar::HandOffsetX);
    o_wpncvar("Hand Y", WpnCVar::HandOffsetY);
    o_wpncvar("Hand Z", WpnCVar::HandOffsetZ);

    o_wpncvar("Off-Hand X", WpnCVar::OffHandOffsetX);
    o_wpncvar("Off-Hand Y", WpnCVar::OffHandOffsetY);
    o_wpncvar("Off-Hand Z", WpnCVar::OffHandOffsetZ);

    m.add_cvar_getter_enum_entry<Wpn2HMode>(                        //
        "2H Mode",                                                  //
        [] { return &VR_GetWpnCVar(getIdx(), WpnCVar::TwoHMode); }, //
        "Default", "Ignore Virtual Stock", "Forbidden"              //
    );

    return m;
}

static quake::menu g_menu = make_menu();

void M_WpnOffset_Key(int key)
{
    g_menu.key(key);
}

void M_WpnOffset_Draw()
{
    g_menu.draw();
}
