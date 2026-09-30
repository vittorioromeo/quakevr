// vr_bigfont_glyphs.inc -- written by Misc/quakevr/make_bigfont.py: do not edit (edit the script).
// Where each letter of the main menu's lettering is in id's menu pictures (no pixels: vr_bigfont.cpp cuts
// them from the pak at run time, each piece only when its hash matches).

constexpr Band bands[]{
    {0, 0, 20, 1}, // M0: gfx/mainmenu.lmp
    {0, 20, 20, 0}, // M1: gfx/mainmenu.lmp
    {0, 40, 20, 0}, // M2: gfx/mainmenu.lmp
    {0, 60, 20, 0}, // M3: gfx/mainmenu.lmp
    {0, 80, 24, 0}, // M4: gfx/mainmenu.lmp
    {1, 0, 20, 1}, // S0: gfx/sp_menu.lmp
    {1, 20, 20, 0}, // S1: gfx/sp_menu.lmp
    {1, 40, 20, 0}, // S2: gfx/sp_menu.lmp
    {2, 0, 20, 1}, // J0: gfx/mp_menu.lmp
    {2, 40, 20, 0}, // J2: gfx/mp_menu.lmp
};

constexpr Op ops[]{
    {Op::Copy, 0, 2, 20, 0, 20, 0, 0, false, 0xe921698du},
    {Op::Copy, 0, 22, 30, 0, 20, 0, 0, false, 0x22a2d17du},
    {Op::Copy, 0, 32, 50, 0, 20, 0, 0, false, 0x64ea27f7u},
    {Op::Copy, 0, 50, 69, 0, 20, 0, 0, false, 0x92a5b0a3u},
    {Op::Copy, 0, 70, 87, 0, 20, 0, 0, false, 0x0c2192eeu},
    {Op::Copy, 0, 87, 105, 0, 20, 0, 0, false, 0x38b6a6f8u},
    {Op::Copy, 0, 114, 134, 0, 20, 0, 0, false, 0x88a8b338u},
    {Op::Copy, 0, 200, 221, 0, 20, 0, 0, false, 0x525684c6u},
    {Op::Copy, 8, 71, 90, 0, 20, 0, 0, false, 0x12fad742u},
    {Op::Copy, 1, 1, 23, 0, 20, 0, 0, false, 0x2f0f53f1u},
    {Op::Copy, 1, 122, 153, 0, 20, 0, 0, false, 0xfaf8eb2cu},
    {Op::Copy, 9, 59, 79, 0, 20, 0, 0, false, 0x3da00e08u},
    {Op::Copy, 9, 40, 58, 0, 20, 0, 0, false, 0x7a1b6e0au},
    {Op::Copy, 2, 22, 41, 0, 20, 0, 0, false, 0xce5abbacu},
    {Op::Clear, 0, 0, 1, 7, 11, 0, 0, false, 0u},
    {Op::Clear, 0, 18, 19, 3, 6, 0, 0, false, 0u},
    {Op::Copy, 2, 0, 23, 0, 20, 0, 0, false, 0x8ec6aaf1u},
    {Op::Clear, 0, 22, 23, 0, 7, 0, 0, false, 0u},
    {Op::Clear, 0, 22, 23, 11, 20, 0, 0, false, 0u},
    {Op::Copy, 2, 67, 88, 0, 20, 0, 0, false, 0xf36c75bbu},
    {Op::Copy, 2, 106, 123, 0, 20, 0, 0, false, 0x2c65d1e9u},
    {Op::Copy, 3, 1, 22, 0, 20, 0, 0, false, 0x375bc1a8u},
    {Op::Copy, 3, 76, 91, 0, 20, 0, 0, false, 0x1e82598bu},
    {Op::Copy, 6, 58, 77, 0, 20, 0, 0, false, 0x06cc6366u},
    {Op::Copy, 4, 0, 23, 0, 24, 0, 0, false, 0x6cdcbfc5u},
    {Op::Copy, 0, 200, 221, 2, 6, 0, -1, false, 0x87826392u},
    {Op::Copy, 0, 200, 221, 5, 12, 0, 0, false, 0x26571842u},
    {Op::Copy, 0, 200, 221, 11, 15, 0, 1, false, 0x2d02c3bau},
    {Op::Copy, 7, 32, 51, 3, 7, 0, -2, false, 0x2baa85cbu},
    {Op::Copy, 7, 32, 51, 6, 11, 0, -1, false, 0x43ebdbb6u},
    {Op::Copy, 7, 32, 51, 10, 17, 0, 0, false, 0x504b04beu},
    {Op::Clear, 0, 0, 1, 4, 7, 0, 0, false, 0u},
    {Op::Clear, 0, 0, 2, 7, 8, 0, 0, false, 0u},
    {Op::Clear, 0, 0, 3, 8, 11, 0, 0, false, 0u},
    {Op::Clear, 0, 0, 4, 11, 12, 0, 0, false, 0u},
    {Op::Clear, 0, 0, 5, 12, 13, 0, 0, false, 0u},
    {Op::Clear, 0, 0, 6, 13, 14, 0, 0, false, 0u},
    {Op::Clear, 0, 0, 7, 14, 16, 0, 0, false, 0u},
    {Op::Copy, 5, 71, 82, 0, 17, 0, 0, false, 0x3222567du},
    {Op::Copy, 5, 83, 90, 0, 6, 12, 0, false, 0x590c663cu},
    {Op::Copy, 5, 83, 90, 0, 6, 12, 10, true, 0x590c663cu},
    {Op::Copy, 2, 22, 31, 0, 20, 0, 0, false, 0x785b7beau},
    {Op::Clear, 0, 0, 1, 7, 11, 0, 0, false, 0u},
    {Op::Copy, 2, 31, 41, 3, 7, 9, 0, false, 0x0d8ca07du},
    {Op::Copy, 2, 31, 41, 10, 13, 9, -3, false, 0xf113e883u},
    {Op::Copy, 2, 31, 41, 3, 7, 9, 6, false, 0x0d8ca07du},
    {Op::Copy, 2, 31, 41, 10, 13, 9, 3, false, 0xf113e883u},
    {Op::Clear, 0, 18, 19, 3, 6, 0, 0, false, 0u},
    {Op::Clear, 0, 18, 19, 9, 12, 0, 0, false, 0u},
};

constexpr GlyphDef glyphDefs[]{
    {"S", 18, 18, 0, 1},
    {"i", 8, 8, 1, 1},
    {"n", 18, 18, 2, 1},
    {"g", 19, 19, 3, 1},
    {"l", 17, 17, 4, 1},
    {"e", 18, 18, 5, 1},
    {"P", 20, 20, 6, 1},
    {"r", 21, 21, 7, 1},
    {"a", 19, 19, 8, 1},
    {"M", 22, 22, 9, 1},
    {"ay", 31, 31, 10, 1},
    {"u", 20, 20, 11, 1},
    {"t", 18, 18, 12, 1},
    {"p", 19, 19, 13, 3},
    {"O", 23, 22, 16, 3},
    {"o", 21, 21, 19, 1},
    {"s", 17, 17, 20, 1},
    {"H", 21, 21, 21, 1},
    {"/", 15, 15, 22, 1},
    {"d", 19, 19, 23, 1},
    {"Q", 23, 23, 24, 1},
    {"R", 21, 21, 25, 3},
    {"V", 19, 19, 28, 10},
    {"C", 19, 19, 38, 3},
    {"b", 19, 19, 41, 8},
};

constexpr int cellRows = 24;
constexpr int spaceWidth = 9;
