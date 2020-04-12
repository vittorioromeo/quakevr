#include "quakedef.hpp"
#include "map_menu.hpp"
#include "cmd.hpp"
#include "util.hpp"
#include "menu_util.hpp"

#include <string>
#include <cassert>
#include <array>
#include <tuple>
#include <cstddef>
#include <string_view>

using namespace std::literals;

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

[[nodiscard]] static quake::menu make_menu()
{
    const auto changeMap = [](const int option) {
        return [option] {
            quake::menu_util::playMenuSound("items/r_item2.wav", 0.5);
            Cmd_ExecuteString(va("map %s", maps[option].data()), src_command);
        };
    };

    quake::menu m{"Maps", true};

    int idx{0};
    for(const auto& map : maps)
    {
        m.add_action_entry(map, changeMap(idx));
        ++idx;
    }

    return m;
}

static quake::menu g_menu = make_menu();

quake::menu& M_MapMenu_Menu()
{
    return g_menu;
}

void M_MapMenu_Key(int key)
{
    g_menu.key(key);
}

void M_MapMenu_Draw()
{
    g_menu.draw();
}
