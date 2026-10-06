// vr_relight_vis.cpp -- see vr_relight_vis.hpp.

#include "vr_relight_vis.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"

#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"

#include <stdlib.h>
#include <string.h>

namespace qvr::relight::vis
{

namespace
{

constexpr int lumpTextures = 2, lumpVisibility = 4, lumpTexinfo = 6, lumpFaces = 7, lumpLeafs = 10,
              lumpMarksurfaces = 11, lumpModels = 14;
constexpr za::SizeT leafSize = 28; // int contents, int visofs, short mins[3], maxs[3], ushort firstmark, nummark, byte ambient[4]
constexpr int contentsWater = -3, contentsSlime = -4, contentsLava = -5;
constexpr int kindWater = 1, kindTele = 2, kindSlime = 4, kindLava = 8;
constexpr const char* games[] = {"id1", "hipnotic", "rogue"};

// Little-endian reads, 0 past the end.
[[nodiscard]] int i32(const unsigned char* d, za::SizeT size, za::SizeT at)
{
    int v = 0;
    if(at + 4 <= size)
    {
        memcpy(&v, d + at, 4);
    }
    return LittleLong(v);
}

[[nodiscard]] int u16(const unsigned char* d, za::SizeT size, za::SizeT at)
{
    unsigned short v = 0;
    if(at + 2 <= size)
    {
        memcpy(&v, d + at, 2);
    }
    return static_cast<unsigned short>(LittleShort(static_cast<short>(v)));
}

[[nodiscard]] int s16(const unsigned char* d, za::SizeT size, za::SizeT at)
{
    short v = 0;
    if(at + 2 <= size)
    {
        memcpy(&v, d + at, 2);
    }
    return LittleShort(v);
}

[[nodiscard]] bool isBsp29(const unsigned char* d, za::SizeT size)
{
    return size >= 124 && i32(d, size, 0) == 29;
}

// A lump's [offset, offset + length) in the file (false: not all in it).
[[nodiscard]] bool lumpSpan(const unsigned char* d, za::SizeT size, int index, za::SizeT& offset, za::SizeT& length)
{
    const int o = i32(d, size, 4 + static_cast<za::SizeT>(index) * 8);
    const int l = i32(d, size, 8 + static_cast<za::SizeT>(index) * 8);
    if(o < 0 || l < 0 || static_cast<za::SizeT>(o) + static_cast<za::SizeT>(l) > size)
    {
        return false;
    }
    offset = static_cast<za::SizeT>(o);
    length = static_cast<za::SizeT>(l);
    return true;
}

// The folders the data is looked for in, in order (dataFile).
[[nodiscard]] za::Vector<za::String> dataDirs()
{
    za::Vector<za::String> dirs;
    if(vr_relight_vispatch_dir.string[0])
    {
        dirs.pushBack(za::String{vr_relight_vispatch_dir.string});
        return dirs;
    }
    for(const searchpath_t* s = com_searchpaths; s; s = s->next)
    {
        if(!s->pack)
        {
            dirs.pushBack(za::String{va("%s/tools/vispatch", s->filename)});
        }
    }
    if(const char* env = getenv("QUAKEVR_VISPATCH"); env && env[0])
    {
        dirs.pushBack(za::String{env});
    }
    return dirs;
}

} // namespace

za::String dataFile(const char* game)
{
    char lower[MAX_QPATH];
    q_strlcpy(lower, game, sizeof(lower));
    q_strlwr(lower);
    for(const za::String& dir : dataDirs())
    {
        // (vis_maps.load_patches' order)
        for(const char* path : {va("%s/%s.vis", dir.cStr(), lower), va("%s/%s/vispatch.dat", dir.cStr(), lower)})
        {
            if(files::isFile(path))
            {
                return za::String{path};
            }
        }
    }
    return {};
}

bool available()
{
    for(const char* g : games)
    {
        if(!dataFile(g).empty())
        {
            return true;
        }
    }
    return false;
}

za::String placesText()
{
    za::String out;
    for(const za::String& dir : dataDirs())
    {
        if(!out.empty())
        {
            out += "; ";
        }
        out += dir;
    }
    if(!vr_relight_vispatch_dir.string[0] && !getenv("QUAKEVR_VISPATCH"))
    {
        out += "; QUAKEVR_VISPATCH (not set)";
    }
    return out;
}

bool find(const char* game, const char* map, Patch& out)
{
    const za::String path = dataFile(game);
    za::Vector<unsigned char> data;
    if(path.empty() || !files::readBytes(path.cStr(), data))
    {
        return false;
    }
    // vis_maps.read_vis_file: entries of a 32-byte name ("e1m1.bsp"), the entry's length (from the next field on), the
    // visibility's length and bytes, the leaves' length and bytes. (A later entry of the same name wins, as there.)
    char want[MAX_QPATH];
    q_snprintf(want, sizeof(want), "%s.bsp", map);
    const unsigned char* d = data.data();
    const za::SizeT size = data.size();
    bool found = false;
    za::SizeT pos = 0;
    while(pos + 40 <= size)
    {
        char name[33]{};
        memcpy(name, d + pos, 32);
        const int length = i32(d, size, pos + 32);
        const int visLength = i32(d, size, pos + 36);
        if(length <= 0 || visLength < 0)
        {
            break; // (a broken file: what was read so far stands)
        }
        const za::SizeT visAt = pos + 40;
        const za::SizeT leafLengthAt = visAt + static_cast<za::SizeT>(visLength);
        const int leafLength = i32(d, size, leafLengthAt);
        const za::SizeT leafAt = leafLengthAt + 4;
        if(leafLength >= 0 && leafAt + static_cast<za::SizeT>(leafLength) <= size && !q_strcasecmp(name, want))
        {
            out.visibility.resize(static_cast<za::SizeT>(visLength));
            if(visLength)
            {
                memcpy(out.visibility.data(), d + visAt, static_cast<za::SizeT>(visLength));
            }
            out.leafs.resize(static_cast<za::SizeT>(leafLength));
            if(leafLength)
            {
                memcpy(out.leafs.data(), d + leafAt, static_cast<za::SizeT>(leafLength));
            }
            found = true;
        }
        pos += 36 + static_cast<za::SizeT>(length);
    }
    if(found)
    {
        out.file = path;
    }
    return found;
}

bool fits(const unsigned char* bsp, za::SizeT size, const Patch& patch)
{
    za::SizeT offset = 0, length = 0;
    if(!isBsp29(bsp, size) || !lumpSpan(bsp, size, lumpLeafs, offset, length) || length != patch.leafs.size())
    {
        return false;
    }
    // vis_maps.leaf_shape: each leaf but for what vis computes (its visibility offset and ambient sound levels).
    for(za::SizeT at = 0; at < length; at += leafSize)
    {
        const za::SizeT n = za::SizeT{24} <= length - at ? za::SizeT{24} : length - at;
        if(memcmp(bsp + offset + at, patch.leafs.data() + at, n < 4 ? n : 4) ||
            (n > 8 && memcmp(bsp + offset + at + 8, patch.leafs.data() + at + 8, n - 8)))
        {
            return false;
        }
    }
    return true;
}

Liquids liquids(const unsigned char* d, za::SizeT size)
{
    Liquids out;
    za::SizeT tOfs, tLen, tiOfs, tiLen, fOfs, fLen, mOfs, mLen, lOfs, lLen, vOfs, vLen, modOfs, modLen;
    if(!isBsp29(d, size) || !lumpSpan(d, size, lumpTextures, tOfs, tLen) || !lumpSpan(d, size, lumpTexinfo, tiOfs, tiLen) ||
        !lumpSpan(d, size, lumpFaces, fOfs, fLen) || !lumpSpan(d, size, lumpMarksurfaces, mOfs, mLen) ||
        !lumpSpan(d, size, lumpLeafs, lOfs, lLen) || !lumpSpan(d, size, lumpVisibility, vOfs, vLen) ||
        !lumpSpan(d, size, lumpModels, modOfs, modLen) || modLen < 64)
    {
        return out;
    }
    // Each face's kind of liquid, by its texture's name: only water and teleporter faces tell a water leaf's kind.
    const int numMip = tLen >= 4 ? i32(d, size, tOfs) : 0;
    const za::SizeT numFaces = fLen / 20;
    za::Vector<unsigned char> faceKind(numFaces, 0);
    for(za::SizeT f = 0; f < numFaces; f++)
    {
        const int ti = s16(d, size, fOfs + f * 20 + 10);
        if(ti < 0 || static_cast<za::SizeT>(ti) >= tiLen / 40)
        {
            continue;
        }
        const int mipIndex = i32(d, size, tiOfs + static_cast<za::SizeT>(ti) * 40 + 32);
        if(mipIndex < 0 || mipIndex >= numMip)
        {
            continue;
        }
        const int mip = i32(d, size, tOfs + 4 + static_cast<za::SizeT>(mipIndex) * 4);
        if(mip < 0 || tOfs + static_cast<za::SizeT>(mip) + 16 > size)
        {
            continue;
        }
        char name[17]{};
        memcpy(name, d + tOfs + static_cast<za::SizeT>(mip), 16);
        q_strlwr(name);
        if(name[0] != '*' || !strncmp(name, "*lava", 5) || !strncmp(name, "*slime", 6))
        {
            continue;
        }
        faceKind[f] = !strncmp(name, "*tele", 5) ? kindTele : kindWater;
    }
    const za::SizeT numLeafs = lLen / leafSize;
    const int visLeafs = i32(d, size, modOfs + 52);
    if(visLeafs <= 0)
    {
        return out;
    }
    const za::SizeT row = (static_cast<za::SizeT>(visLeafs) + 7) >> 3;
    za::Vector<unsigned char> vis(row, 0);
    const auto leafAt = [&](za::SizeT i) { return lOfs + i * leafSize; };
    const za::SizeT last = za::SizeT(visLeafs) + 1 < numLeafs ? za::SizeT(visLeafs) + 1 : numLeafs;
    for(za::SizeT i = 1; i < last; i++)
    {
        const int contents = i32(d, size, leafAt(i));
        int kind = 0;
        if(contents == contentsWater)
        {
            const int first = u16(d, size, leafAt(i) + 20), count = u16(d, size, leafAt(i) + 22);
            for(int j = 0; j < count && !kind; j++)
            {
                const int face = u16(d, size, mOfs + static_cast<za::SizeT>(first + j) * 2);
                kind = static_cast<za::SizeT>(face) < numFaces ? faceKind[static_cast<za::SizeT>(face)] : 0;
            }
            if(!kind)
            {
                continue;
            }
        }
        else if(contents == contentsSlime)
        {
            kind = kindSlime;
        }
        else if(contents == contentsLava)
        {
            kind = kindLava;
        }
        else
        {
            continue;
        }
        out.found |= kind;
        if(out.seeThrough & kind)
        {
            continue;
        }
        // The leaf's row of the PVS (run-length coded zeros; none: sees everything).
        const int visOfs = i32(d, size, leafAt(i) + 4);
        if(visOfs < 0 || vLen == 0)
        {
            memset(vis.data(), 0xff, row);
        }
        else
        {
            za::SizeT p = vOfs + static_cast<za::SizeT>(visOfs), n = 0;
            while(n < row && p < size)
            {
                if(d[p])
                {
                    vis[n++] = d[p++];
                }
                else
                {
                    const za::SizeT zeros = p + 1 < size ? d[p + 1] : 0;
                    for(za::SizeT z = 0; z < zeros && n < row; z++)
                    {
                        vis[n++] = 0;
                    }
                    p += 2;
                }
            }
            while(n < row)
            {
                vis[n++] = 0;
            }
        }
        for(za::SizeT j = 0; j < static_cast<za::SizeT>(visLeafs); j++)
        {
            if((vis[j >> 3] & (1 << (j & 7))) && j + 1 < numLeafs && i32(d, size, leafAt(j + 1)) != contents)
            {
                out.seeThrough |= kind;
                break;
            }
        }
    }
    return out;
}

za::String describe(const Liquids& l)
{
    if(!l.found)
    {
        return za::String{"no liquids"};
    }
    constexpr struct
    {
        int bit;
        const char* name;
    } kinds[] = {{kindWater, "water"}, {kindTele, "tele"}, {kindSlime, "slime"}, {kindLava, "lava"}};
    za::String yes, no;
    for(const auto& k : kinds)
    {
        za::String& list = (l.seeThrough & k.bit) ? yes : no;
        if(!(l.found & k.bit))
        {
            continue;
        }
        if(!list.empty())
        {
            list += ", ";
        }
        list += k.name;
    }
    za::String out{"see-through: "};
    out += yes.empty() ? za::String{"none"} : yes;
    if(!no.empty())
    {
        out += "; opaque: ";
        out += no;
    }
    return out;
}

} // namespace qvr::relight::vis
