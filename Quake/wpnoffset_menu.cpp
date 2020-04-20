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

    // ------------------------------------------------------------------------

    const auto o_wpncvar = [&](const char* title, const WpnCVar c) {
        return m.add_cvar_getter_entry<float>(           //
            title,                                       //
            [c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            oBounds                                      //
        );
    };

    const auto r_wpncvar = [&](const char* title, const WpnCVar c) {
        return m.add_cvar_getter_entry<float>(           //
            title,                                       //
            [c] { return &VR_GetWpnCVar(getIdx(), c); }, //
            rBounds                                      //
        );
    };

    const auto makeHoverFn = [&](int& implVar) {
        return [&](const bool x) {
            if(!x)
            {
                implVar = 0;
                return;
            }

            implVar = wpnoff_offhand ? 2 : 1;
        };
    };

    // ------------------------------------------------------------------------

    m.add_getter_entry<bool>(          //
        "Off-Hand",                    //
        [] { return &wpnoff_offhand; } //
    );

    // ------------------------------------------------------------------------

    const auto hoverOffset = makeHoverFn(vr_impl_draw_wpnoffset_helper_offset);
    const auto hoverMuzzle = makeHoverFn(vr_impl_draw_wpnoffset_helper_muzzle);
    const auto hover2HOffset =
        makeHoverFn(vr_impl_draw_wpnoffset_helper_2h_offset);

    // ------------------------------------------------------------------------

    const char* offsetTooltip =
        "Offset of the weapon relative to the center of its model. Does not "
        "affect aiming.";

    o_wpncvar("X", WpnCVar::OffsetX).hover(hoverOffset).tooltip(offsetTooltip);
    o_wpncvar("Y", WpnCVar::OffsetY).hover(hoverOffset).tooltip(offsetTooltip);
    o_wpncvar("Z", WpnCVar::OffsetZ).hover(hoverOffset).tooltip(offsetTooltip);

    o_wpncvar("Scale", WpnCVar::Scale).tooltip("Scale of the weapon model.");

    // ------------------------------------------------------------------------

    const char* rotationTooltip =
        "Rotation of the weapon model. Does not affect aiming.";

    r_wpncvar("Roll", WpnCVar::Roll).tooltip(rotationTooltip);
    r_wpncvar("Pitch", WpnCVar::Pitch).tooltip(rotationTooltip);
    r_wpncvar("Yaw", WpnCVar::Yaw).tooltip(rotationTooltip);

    // ------------------------------------------------------------------------

    const char* muzzleTooltip =
        "Position of the weapon muzzle. Relative to the XYZ offsets above. "
        "Affected by the weapon model scale. DOES affect aiming. Bullets and "
        "projectiles spawn from this position.";

    o_wpncvar("Muzzle X", WpnCVar::MuzzleOffsetX)
        .hover(hoverMuzzle)
        .tooltip(muzzleTooltip);
    o_wpncvar("Muzzle Y", WpnCVar::MuzzleOffsetY)
        .hover(hoverMuzzle)
        .tooltip(muzzleTooltip);
    o_wpncvar("Muzzle Z", WpnCVar::MuzzleOffsetZ)
        .hover(hoverMuzzle)
        .tooltip(muzzleTooltip);

    // ------------------------------------------------------------------------

    const char* twoHXYZTooltip =
        "Offset applied to the off-hand when aiming with two hands. Allows "
        "tweaking of the weapon's position and angle, and how close the "
        "off-hand appears to the model. DOES affect aiming.";

    o_wpncvar("2H X", WpnCVar::TwoHOffsetX)
        .hover(hover2HOffset)
        .tooltip(twoHXYZTooltip);
    o_wpncvar("2H Y", WpnCVar::TwoHOffsetY)
        .hover(hover2HOffset)
        .tooltip(twoHXYZTooltip);
    o_wpncvar("2H Z", WpnCVar::TwoHOffsetZ)
        .hover(hover2HOffset)
        .tooltip(twoHXYZTooltip);

    // ------------------------------------------------------------------------

    const char* twoHRotTooltip =
        "Angle offset applied to the weapon when aiming with two hands. "
        "Allows tweaking of the weapon's angle. DOES affect aiming.";

    r_wpncvar("2H Pitch", WpnCVar::TwoHPitch).tooltip(twoHRotTooltip);
    r_wpncvar("2H Yaw", WpnCVar::TwoHYaw).tooltip(twoHRotTooltip);
    r_wpncvar("2H Roll", WpnCVar::TwoHRoll).tooltip(twoHRotTooltip);

    // ------------------------------------------------------------------------

    o_wpncvar("Weight", WpnCVar::Weight)
        .tooltip(
            "How heavy the weapon 'feels'. Values closer to '1' are heavier. "
            "'1' itself is 'infinite' weight. Affects weapon movement and "
            "rotation speed, and also throwing distance and damage.");

    // ------------------------------------------------------------------------

    m.add_cvar_getter_entry<int>(                                            //
         "Hand Anchor Vertex",                                               //
         [] { return &VR_GetWpnCVar(getIdx(), WpnCVar::HandAnchorVertex); }, //
         {1, 0, 1024}                                                        //
         )
        .tooltip(
            "Index of the mesh vertex where the hand will be attached. Useful "
            "to ensure that the hand follows the weapon animations "
            "properly.");

    // ------------------------------------------------------------------------

    const char* handOffsetTooltip =
        "Visual offset of the hand, relative to the anchor vertex.";

    o_wpncvar("Hand X", WpnCVar::HandOffsetX).tooltip(handOffsetTooltip);
    o_wpncvar("Hand Y", WpnCVar::HandOffsetY).tooltip(handOffsetTooltip);
    o_wpncvar("Hand Z", WpnCVar::HandOffsetZ).tooltip(handOffsetTooltip);

    // ------------------------------------------------------------------------

    const char* offHandOffsetTooltip =
        "Visual offset of the hand, relative to the above hand offset.";

    o_wpncvar("Off-Hand X", WpnCVar::OffHandOffsetX)
        .tooltip(offHandOffsetTooltip);
    o_wpncvar("Off-Hand Y", WpnCVar::OffHandOffsetY)
        .tooltip(offHandOffsetTooltip);
    o_wpncvar("Off-Hand Z", WpnCVar::OffHandOffsetZ)
        .tooltip(offHandOffsetTooltip);

    // ------------------------------------------------------------------------

    m.add_cvar_getter_enum_entry<Wpn2HMode>(                         //
         "2H Mode",                                                  //
         [] { return &VR_GetWpnCVar(getIdx(), WpnCVar::TwoHMode); }, //
         "Default", "Ignore Virtual Stock", "Forbidden"              //
         )
        .tooltip(
            "Defines whether the weapon is eligible for 2H aiming. "
            "Virtual stock can be ignored for weapons like the laser "
            "cannon.");

    // ------------------------------------------------------------------------

    return m;
}

static quake::menu g_menu = make_menu();

quake::menu& M_WpnOffset_Menu()
{
    return g_menu;
}

void M_WpnOffset_Key(int key)
{
    g_menu.key(key);

    // TODO VR: (P2) hackish
    VR_ModAllWeapons();
}

void M_WpnOffset_Draw()
{
    g_menu.draw();
}
