// vr_bigfont.cpp -- the main menu's lettering as a font: menu.c's M_Main_Draw draws its rows as text in it, so that
// the VR Calibration row has the same letters as Quake's Single Player, Multiplayer, Options... (a picture,
// gfx/mainmenu.lmp, that has no such row).
//
// There is no such font in Quake: the letters are cut from id's menu pictures (gfx/mainmenu.lmp, sp_menu.lmp,
// mp_menu.lmp) in the player's own pak, when the main menu is first drawn after the game directory is set. Where each
// one is, with a hash of each piece, is vr_bigfont_glyphs.inc (Misc/quakevr/make_bigfont.py writes it from Quake
// 1.06's pak; no pixels are shipped). A piece whose hash differs (a mod's own picture, another release's) leaves its
// letter out, and a row with a letter missing is drawn as before (the picture). V, R, C and b, which the pictures do
// not have, are made of others (v, r, G and p: the script says how). The letters go side by side into one texture,
// drawn a letter at a time (Draw_SubPic).

#include "vr_bigfont.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"

#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/StringView.hpp"
#include "vr_zancle.hpp"

#include <stdlib.h>
#include <string.h>

namespace
{

// A strip of rows of one picture that has one row of the menu's lettering: its first row, its rows, and how far down
// the cell its rows go (the small capitals' bottom on the cell's row 15 in every strip).
struct Band
{
    int pic, top, rows, dy;
};

// A letter is made by copying pieces of strips into its cell (and clearing what a neighbour's pixels left in it).
struct Op
{
    enum Kind : za::U8
    {
        Copy,
        Clear
    };
    Kind kind;
    int band, x0, x1, y0, y1; // Copy: the strip's columns x0..x1 and rows y0..y1; Clear: the cell's
    int dx, dy;               // Copy: to column dx, down by the strip's dy and this
    bool flip;                // Copy: the rows upside down
    za::U32 hash;       // Copy: FNV-1a of the piece (its rows in order)
};

struct GlyphDef
{
    const char* text; // one letter, or two that overlap in the pictures ("ay")
    int width, advance, firstOp, opCount;
};

#include "vr_bigfont_glyphs.inc"

constexpr const char* picNames[]{"gfx/mainmenu.lmp", "gfx/sp_menu.lmp", "gfx/mp_menu.lmp"};
constexpr int glyphCount = static_cast<int>(za::getArraySize(glyphDefs));
constexpr int pad = 2; // transparent columns round each letter in the texture (its filtering)

struct Picture
{
    int width{0}, height{0};
    const byte* pixels{nullptr};

    [[nodiscard]] byte at(int x, int y) const { return pixels[y * width + x]; }
};

// The letters as cut (the main thread: the menu's drawing).
struct Font
{
    bool built{false};
    za::Vector<byte> atlas;              // cellRows rows, the letters side by side (reloads of the texture read it)
    int atlasWidth{0};
    za::Array<int, glyphCount> column{}; // each letter's first column in it; -1: left out (a piece's hash differed)
    za::Vector<byte> pic;                // the texture's qpic_t (Draw_ReplacePic), kept from game to game
    za::Array<bool, za::getArraySize(picNames)> picFound{};
};
Font font;

[[nodiscard]] za::U32 pieceHash(const Picture& p, const Band& b, const Op& op)
{
    za::U32 h = 2166136261u;
    for(int y = op.y0; y < op.y1; y++)
    {
        for(int x = op.x0; x < op.x1; x++)
        {
            h = (h ^ p.at(x, b.top + y)) * 16777619u;
        }
    }
    return h;
}

[[nodiscard]] bool piecesMatch(const GlyphDef& g, const za::Array<Picture, za::getArraySize(picNames)>& pics)
{
    for(int i = g.firstOp; i < g.firstOp + g.opCount; i++)
    {
        const Op& op = ops[i];
        if(op.kind != Op::Copy)
        {
            continue;
        }
        const Band& b = bands[op.band];
        const Picture& p = pics[b.pic];
        if(!p.pixels || op.x1 > p.width || b.top + op.y1 > p.height || pieceHash(p, b, op) != op.hash)
        {
            return false;
        }
    }
    return true;
}

void paint(const GlyphDef& g, int column, const za::Array<Picture, za::getArraySize(picNames)>& pics)
{
    byte* const cell = font.atlas.data() + column;
    const auto put = [&](int x, int y, byte v) {
        if(x >= 0 && x < g.width && y >= 0 && y < cellRows)
        {
            cell[y * font.atlasWidth + x] = v;
        }
    };
    for(int i = g.firstOp; i < g.firstOp + g.opCount; i++)
    {
        const Op& op = ops[i];
        if(op.kind == Op::Clear)
        {
            for(int y = op.y0; y < op.y1; y++)
            {
                for(int x = op.x0; x < op.x1; x++)
                {
                    put(x, y, 255);
                }
            }
            continue;
        }
        const Band& b = bands[op.band];
        const Picture& p = pics[b.pic];
        for(int y = op.y0; y < op.y1; y++)
        {
            const int sy = op.flip ? op.y0 + op.y1 - 1 - y : y;
            for(int x = op.x0; x < op.x1; x++)
            {
                if(const byte v = p.at(x, b.top + sy); v != 255)
                {
                    put(x - op.x0 + op.dx, y + b.dy + op.dy, v);
                }
            }
        }
    }
}

void build()
{
    font.built = true;
    qza::fill(font.column, -1);

    za::Array<byte*, za::getArraySize(picNames)> files{};
    za::Array<Picture, za::getArraySize(picNames)> pics{};
    for(za::SizeT i = 0; i < za::getArraySize(picNames); i++)
    {
        files[i] = COM_LoadMallocFile(picNames[i], nullptr);
        if(files[i] && com_filesize >= 8)
        {
            int w, h;
            memcpy(&w, files[i], 4);
            memcpy(&h, files[i] + 4, 4);
            w = LittleLong(w);
            h = LittleLong(h);
            if(w > 0 && h > 0 && w <= 4096 && h <= 4096 && 8 + w * h <= com_filesize)
            {
                pics[i] = {w, h, files[i] + 8};
            }
        }
        font.picFound[i] = pics[i].pixels != nullptr;
    }

    int width = pad;
    for(int g = 0; g < glyphCount; g++)
    {
        if(piecesMatch(glyphDefs[g], pics))
        {
            font.column[g] = width;
            width += glyphDefs[g].width + pad;
        }
    }
    font.atlasWidth = width;
    font.atlas.clear();
    font.atlas.resize(static_cast<za::SizeT>(width) * cellRows, 255);
    for(int g = 0; g < glyphCount; g++)
    {
        if(font.column[g] >= 0)
        {
            paint(glyphDefs[g], font.column[g], pics);
        }
    }
    for(byte* f : files)
    {
        free(f);
    }

    if(width > pad)
    {
        if(font.pic.empty())
        {
            font.pic.resize(Draw_PicBytes(), 0);
        }
        Draw_ReplacePic(reinterpret_cast<qpic_t*>(font.pic.data()), "vr_bigfont", width, cellRows, font.atlas.data());
    }
}

// The letter at the start of `text` (the longest there is), and its length; -1 if none.
[[nodiscard]] int glyphAt(za::StringView text, int& length)
{
    int best = -1;
    length = 0;
    for(int g = 0; g < glyphCount; g++)
    {
        const za::StringView t = glyphDefs[g].text;
        if(font.column[g] >= 0 && static_cast<int>(t.size()) > length && text.startsWith(t))
        {
            best = g;
            length = static_cast<int>(t.size());
        }
    }
    return best;
}

void ensureBuilt()
{
    if(!font.built)
    {
        build();
    }
}

} // namespace

namespace qvr::bigfont
{

void onGameDirChanged()
{
    font.built = false; // (the texture stays until the next build replaces it)
}

void report_f()
{
    ensureBuilt();
    for(za::SizeT i = 0; i < za::getArraySize(picNames); i++)
    {
        Con_Printf("%s: %s\n", picNames[i], font.picFound[i] ? "read" : "not found");
    }
    int cut = 0;
    Con_Printf("letters left out (a piece of the picture differs):");
    for(int g = 0; g < glyphCount; g++)
    {
        if(font.column[g] >= 0)
        {
            cut++;
        }
        else
        {
            Con_Printf(" %s", glyphDefs[g].text);
        }
    }
    Con_Printf("%s\nvr_bigfont: %d of %d letters cut, texture %d x %d\n", cut == glyphCount ? " none" : "", cut, glyphCount,
        font.atlasWidth, cellRows);
}

} // namespace qvr::bigfont

extern "C" int VR_BigFont_CanDraw(const char* text)
{
    if(!qvr::vr_menu_bigfont.value)
    {
        return 0;
    }
    ensureBuilt();
    if(font.pic.empty())
    {
        return 0;
    }
    za::StringView s{text};
    while(!s.empty())
    {
        int length = 1;
        if(s[0] != ' ' && glyphAt(s, length) < 0)
        {
            return 0;
        }
        s.removePrefix(length);
    }
    return 1;
}

extern "C" int VR_BigFont_Draw(int x, int y, const char* text)
{
    ensureBuilt();
    if(font.pic.empty())
    {
        return 0;
    }
    qpic_t* const pic = reinterpret_cast<qpic_t*>(font.pic.data());
    const float w = static_cast<float>(font.atlasWidth);
    const int x0 = x;
    za::StringView s{text};
    while(!s.empty())
    {
        int length = 1;
        if(s[0] == ' ')
        {
            x += spaceWidth;
        }
        else if(const int g = glyphAt(s, length); g >= 0)
        {
            const GlyphDef& d = glyphDefs[g];
            Draw_SubPic(static_cast<float>(x), static_cast<float>(y), static_cast<float>(d.width), static_cast<float>(cellRows), pic,
                static_cast<float>(font.column[g]) / w, 0.f, static_cast<float>(d.width) / w, 1.f, nullptr, 1.f);
            x += d.advance;
        }
        s.removePrefix(length);
    }
    return x - x0;
}
