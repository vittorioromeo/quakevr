// vr_relight_glow.cpp -- part of vr_relight.cpp (see vr_relight.hpp, vr_relight_internal.hpp): what becomes lights (a
// port of relight_maps.py's glow_lights): the faces by texture, the things they make up, fixtures and liquids' spots,
// the glowing textures' images measured, and the lights written out.

#include "vr_relight_internal.hpp"

extern "C" {
void VR_HeapFree(void* pointer);                                       // vr_alloccount.cpp
void VR_FileCacheEnable(int on);                                       // vr_fscache.cpp
void VR_FileCacheForget();                                             // vr_fscache.cpp
int VR_MapGameFolder(const char* name, char* out, size_t size);        // vr_gamedir.cpp
}

namespace qvr::relight
{
using namespace detail;

namespace detail
{

// Each texture's faces, by its index in the texture lump (BSP29).
[[nodiscard]] za::Vector<za::Vector<Face>> textureFaces(const Bsp& b, int textures)
{
    za::Vector<za::Vector<Face>> out;
    out.resize(static_cast<za::SizeT>(za::max(textures, 0)));
    const int pofs = b.lumpOffset(1), plen = b.lumpLength(1) / 20;
    const int vofs = b.lumpOffset(3), vlen = b.lumpLength(3) / 12;
    const int tofs = b.lumpOffset(6), tlen = b.lumpLength(6) / 40;
    const int fofs = b.lumpOffset(7), flen = b.lumpLength(7) / 20;
    const int eofs = b.lumpOffset(12), elen = b.lumpLength(12) / 4;
    const int sofs = b.lumpOffset(13), slen = b.lumpLength(13) / 4;
    const auto vert = [&](int i) {
        return V3{{b.f32(vofs + i * 12), b.f32(vofs + i * 12 + 4), b.f32(vofs + i * 12 + 8)}};
    };
    for(int i = 0; i < flen; i++)
    {
        const za::SizeT at = static_cast<za::SizeT>(fofs) + static_cast<za::SizeT>(i) * 20;
        const int planenum = b.i16(at), side = b.i16(at + 2), first = b.i32(at + 4), count = b.i16(at + 8), texinfo = b.i16(at + 10);
        Face f;
        bool ok = true;
        for(int k = 0; k < count && ok; k++)
        {
            ok = first + k >= 0 && first + k < slen;
            if(!ok)
            {
                break;
            }
            const int e = b.i32(sofs + (first + k) * 4);
            const int ei = e >= 0 ? e : -e;
            ok = ei < elen;
            if(!ok)
            {
                break;
            }
            const int v = b.u16(eofs + ei * 4 + (e >= 0 ? 0 : 2));
            ok = v < vlen;
            if(ok)
            {
                f.pts.pushBack(vert(v));
            }
        }
        if(!ok || f.pts.empty())
        {
            continue;
        }
        double area = 0.0;
        for(za::SizeT k = 1; k + 1 < f.pts.size(); k++)
        {
            V3 a, c, d;
            for(int j = 0; j < 3; j++)
            {
                a[j] = f.pts[k][j] - f.pts[0][j];
                c[j] = f.pts[k + 1][j] - f.pts[0][j];
            }
            d = V3{{a[1] * c[2] - a[2] * c[1], a[2] * c[0] - a[0] * c[2], a[0] * c[1] - a[1] * c[0]}};
            area += 0.5 * za::pow(sq(d[0]) + sq(d[1]) + sq(d[2]), 0.5);
        }
        f.area = area;
        for(int j = 0; j < 3; j++)
        {
            PySum s;
            for(const V3& p : f.pts)
            {
                s.add(p[j]);
            }
            f.centre[j] = s.value() / static_cast<double>(f.pts.size());
        }
        f.normal = planenum >= 0 && planenum < plen ?
            V3{{b.f32(pofs + planenum * 20), b.f32(pofs + planenum * 20 + 4), b.f32(pofs + planenum * 20 + 8)}} :
            V3{{0.0, 0.0, 1.0}};
        if(side)
        {
            for(int j = 0; j < 3; j++)
            {
                f.normal[j] = -f.normal[j];
            }
        }
        const int m = texinfo >= 0 && texinfo < tlen ? b.i32(tofs + texinfo * 40 + 32) : -1;
        if(m >= 0 && m < textures)
        {
            out[static_cast<za::SizeT>(m)].pushBack(ZA_MOVE(f));
        }
    }
    return out;
}

[[nodiscard]] za::Vector<Thing> thingsOf(const za::Vector<Face>& faces)
{
    za::Vector<za::Vector<const Face*>> groups;
    for(const Face& f : faces)
    {
        za::Vector<const Face*> merged{&f};
        za::Vector<za::Vector<const Face*>> kept;
        for(za::Vector<const Face*>& g : groups)
        {
            bool joins = false;
            for(const Face* h : g)
            {
                if(isNear(f.centre, h->centre, glowObject))
                {
                    joins = true;
                    break;
                }
            }
            if(joins)
            {
                for(const Face* h : g)
                {
                    merged.pushBack(h);
                }
            }
            else
            {
                kept.pushBack(ZA_MOVE(g));
            }
        }
        kept.pushBack(ZA_MOVE(merged));
        groups = ZA_MOVE(kept);
    }
    za::Vector<Thing> out;
    for(za::Vector<const Face*>& g : groups)
    {
        PySum area;
        for(const Face* h : g)
        {
            area.add(h->area);
        }
        Thing t;
        t.n = za::max(za::pow(static_cast<double>(g.size()), 0.5), area.value() / sq(surflight));
        t.faces = ZA_MOVE(g);
        out.pushBack(ZA_MOVE(t));
    }
    return out;
}

[[nodiscard]] double crowd(const Thing& thing, const za::Vector<const Thing*>& everyone)
{
    const V3& c = thing.faces[0]->centre;
    PySum s;
    for(const Thing* g : everyone)
    {
        if(isNear(c, g->faces[0]->centre, glowRoom))
        {
            s.add(g->n);
        }
    }
    return za::max(1.0, s.value());
}

[[nodiscard]] za::Vector<V3> faceSamples(const Face& face, double step)
{
    const za::Vector<V3>& pts = face.pts;
    if(face.area <= sq(step) || pts.size() < 3)
    {
        return {face.centre};
    }
    const V3& n = face.normal;
    const V3 helper = za::abs(n[2]) < 0.9 ? V3{{0.0, 0.0, 1.0}} : V3{{1.0, 0.0, 0.0}};
    V3 u{{n[1] * helper[2] - n[2] * helper[1], n[2] * helper[0] - n[0] * helper[2], n[0] * helper[1] - n[1] * helper[0]}};
    const double ul = za::pow(selfDot(u), 0.5);
    for(int j = 0; j < 3; j++)
    {
        u[j] = u[j] / ul;
    }
    const V3 v{{n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2], n[0] * u[1] - n[1] * u[0]}};
    za::Vector<double> fx, fy;
    for(const V3& p : pts)
    {
        fx.pushBack(dot(p, u));
        fy.pushBack(dot(p, v));
    }
    const double base = dot(pts[0], n);
    const auto inside = [&](double x, double y) {
        double sign = 0.0;
        for(za::SizeT k = 0; k < fx.size(); k++)
        {
            const za::SizeT k1 = (k + 1) % fx.size();
            const double c = (fx[k1] - fx[k]) * (y - fy[k]) - (fy[k1] - fy[k]) * (x - fx[k]);
            if(za::abs(c) > 1e-3)
            {
                if(sign != 0.0 && (c > 0) != (sign > 0))
                {
                    return false;
                }
                sign = c;
            }
        }
        return true;
    };
    double lo[2] = {fx[0], fy[0]}, hi[2] = {fx[0], fy[0]};
    for(za::SizeT k = 0; k < fx.size(); k++)
    {
        lo[0] = za::min(lo[0], fx[k]);
        hi[0] = za::max(hi[0], fx[k]);
        lo[1] = za::min(lo[1], fy[k]);
        hi[1] = za::max(hi[1], fy[k]);
    }
    int counts[2];
    for(int k = 0; k < 2; k++)
    {
        counts[k] = za::max(1, static_cast<int>((hi[k] - lo[k]) / step + 0.5));
    }
    za::Vector<V3> out;
    for(int i = 0; i < counts[0]; i++)
    {
        for(int j = 0; j < counts[1]; j++)
        {
            const double x = lo[0] + (hi[0] - lo[0]) * (i + 0.5) / counts[0];
            const double y = lo[1] + (hi[1] - lo[1]) * (j + 0.5) / counts[1];
            if(inside(x, y))
            {
                out.pushBack(V3{{x * u[0] + y * v[0] + base * n[0], x * u[1] + y * v[1] + base * n[1],
                    x * u[2] + y * v[2] + base * n[2]}});
            }
        }
    }
    if(out.empty())
    {
        out.pushBack(face.centre);
    }
    return out;
}

[[nodiscard]] double recessDepth(const Face& face, const World& world)
{
    const za::Vector<V3>& pts = face.pts;
    if(pts.size() < 3)
    {
        return fixtureOffset;
    }
    struct Outside
    {
        V3 o, out;
    };
    za::Vector<Outside> outside;
    for(za::SizeT k = 0; k < pts.size(); k++)
    {
        V3 mid, out;
        for(int j = 0; j < 3; j++)
        {
            mid[j] = (pts[k][j] + pts[(k + 1) % pts.size()][j]) / 2;
            out[j] = mid[j] - face.centre[j];
        }
        const double a = dot(out, face.normal);
        for(int j = 0; j < 3; j++)
        {
            out[j] = out[j] - a * face.normal[j];
        }
        const double length = za::pow(selfDot(out), 0.5);
        if(length > 1e-3)
        {
            for(int j = 0; j < 3; j++)
            {
                out[j] = out[j] / length;
            }
            outside.pushBack(Outside{V3{{mid[0] + out[0] * 4, mid[1] + out[1] * 4, mid[2] + out[2] * 4}}, out});
        }
    }
    double last = fixtureOffset;
    double d = fixtureOffset;
    za::Vector<const V3*> walls;
    while(d <= recessMax)
    {
        if(world.solid(along(face.centre, face.normal, d)))
        {
            return last;
        }
        walls.clear();
        for(const Outside& o : outside)
        {
            if(world.solid(along(o.o, face.normal, d)))
            {
                walls.pushBack(&o.out);
            }
        }
        bool opposite = false;
        for(const V3* a : walls)
        {
            for(const V3* c : walls)
            {
                opposite = opposite || dot(*a, *c) < -0.5;
            }
        }
        if(!opposite)
        {
            return d == fixtureOffset ? d : za::min(recessMax, d + fixtureOffset);
        }
        last = d;
        d += 2.0;
    }
    return last;
}

[[nodiscard]] za::Vector<V3> fixtureSpots(const za::Vector<const Face*>& faces, const World& world)
{
    PySum totalSum;
    for(const Face* f : faces)
    {
        totalSum.add(f->area);
    }
    const double total = totalSum.value() != 0.0 ? totalSum.value() : 1.0;
    V3 centre;
    for(int j = 0; j < 3; j++)
    {
        PySum s;
        for(const Face* f : faces)
        {
            s.add(f->centre[j] * f->area);
        }
        centre[j] = s.value() / total;
    }
    double spread = 0.0;
    bool small = true;
    for(const Face* f : faces)
    {
        PySum s;
        for(int j = 0; j < 3; j++)
        {
            s.add(sq(f->centre[j] - centre[j]));
        }
        spread = za::max(spread, za::pow(s.value(), 0.5));
        small = small && f->area <= sq(fixtureStep);
    }
    if(spread <= fixtureStep / 2 && small)
    {
        V3 facing;
        for(int j = 0; j < 3; j++)
        {
            PySum s;
            for(const Face* f : faces)
            {
                s.add(f->normal[j] * f->area);
            }
            facing[j] = s.value() / total;
        }
        const double length = za::pow(selfDot(facing), 0.5);
        za::Vector<V3> candidates;
        if(length > 0.3)
        {
            for(int j = 0; j < 3; j++)
            {
                facing[j] = facing[j] / length;
            }
            bool any = false;
            double depth = 0.0;
            for(const Face* f : faces)
            {
                if(dot(f->normal, facing) > 0.8)
                {
                    const double r = recessDepth(*f, world);
                    depth = any ? za::max(depth, r) : r;
                    any = true;
                }
            }
            if(!any)
            {
                depth = fixtureOffset;
            }
            for(const double o : {depth, fixtureOffset, 2.0})
            {
                candidates.pushBack(along(centre, facing, o));
            }
        }
        double top = centre[2];
        if(!faces[0]->pts.empty())
        {
            bool first = true;
            for(const Face* f : faces)
            {
                for(const V3& p : f->pts)
                {
                    top = first ? p[2] : za::max(top, p[2]);
                    first = false;
                }
            }
        }
        candidates.pushBack(centre);
        candidates.pushBack(V3{{centre[0], centre[1], top + fixtureOffset}});
        za::Vector<const Face*> byArea = faces;
        za::stableSort(byArea.begin(), byArea.end(), [](const Face* a, const Face* c) { return -a->area < -c->area; });
        for(const Face* f : byArea)
        {
            candidates.pushBack(along(f->centre, f->normal, fixtureOffset));
        }
        for(const V3& p : candidates)
        {
            if(!world.solid(p))
            {
                return {p};
            }
        }
        return {};
    }
    za::Vector<V3> spots;
    for(const Face* f : faces)
    {
        const double depth = recessDepth(*f, world);
        for(const V3& p : faceSamples(*f, fixtureStep))
        {
            for(const double offset : {depth, fixtureOffset, 2.0})
            {
                const V3 q = along(p, f->normal, offset);
                if(!world.solid(q))
                {
                    bool close = false;
                    for(const V3& r : spots)
                    {
                        close = close || isNear(q, r, fixtureStep / 2);
                    }
                    if(!close)
                    {
                        spots.pushBack(q);
                    }
                    break;
                }
            }
        }
    }
    return spots;
}

[[nodiscard]] za::Vector<V3> liquidSpots(const za::Vector<Face>& faces, const World& world)
{
    za::Vector<V3> spots;
    for(const Face& f : faces)
    {
        for(const V3& p : faceSamples(f, liquidStep))
        {
            for(const double offset : {liquidHeight, 8.0, 2.0})
            {
                const V3 q = along(p, f.normal, offset);
                if(world.contents(q) == contentsEmpty)
                {
                    bool close = false;
                    for(const V3& r : spots)
                    {
                        close = close || isNear(q, r, liquidStep / 2);
                    }
                    if(!close)
                    {
                        spots.pushBack(q);
                    }
                    break;
                }
            }
        }
    }
    return spots;
}

// The first of <stem>.png, .tga, .jpg in the search path (Image_LoadImage's order): its file (where) or false.
[[nodiscard]] bool imageFile(const char* stem, char* where, size_t size)
{
    for(const char* ext : {"png", "tga", "jpg"})
    {
        char name[MAX_QPATH * 2];
        q_snprintf(name, sizeof(name), "%s.%s", stem, ext);
        if(COM_FileExists(name, nullptr))
        {
            q_snprintf(where, size, "%s/%s", com_filesource, name);
            return true;
        }
    }
    return false;
}

// The material maps' image <stem> (vr_extmaps_dir: in the game folders, or a full path): its file, or false.
[[nodiscard]] bool extmapsFile(const char* stem, char* where, size_t size)
{
    const char* dir = vr_extmaps_dir.string;
    const bool absolute = dir[0] == '/' || dir[0] == '\\' || (dir[0] && dir[1] == ':');
    if(!absolute)
    {
        char rel[MAX_QPATH * 2];
        q_snprintf(rel, sizeof(rel), "%s/%s", dir, stem);
        return imageFile(rel, where, size);
    }
    for(const char* ext : {"png", "tga", "jpg"})
    {
        q_snprintf(where, size, "%s/%s.%s", dir, stem, ext);
        if(Sys_FileType(where) & FS_ENT_FILE)
        {
            return true;
        }
    }
    return false;
}

// glow_measure of an image Image_LoadImage reads (`load`: its name for it, without extension).
[[nodiscard]] Measured measure(const char* load, const char* where)
{
    Measured m;
    q_strlcpy(m.where, where, sizeof(m.where));
    const int mark = Hunk_LowMark();
    int w = 0, h = 0;
    enum srcformat fmt = SRC_RGBA;
    const byte* data = Image_LoadImage(load, &w, &h, &fmt);
    if(data && fmt == SRC_RGBA && w > 0 && h > 0)
    {
        const za::SizeT n = static_cast<za::SizeT>(w) * static_cast<za::SizeT>(h);
        int peak = 0;
        for(za::SizeT i = 0; i < n; i++)
        {
            peak = za::max(peak, static_cast<int>(za::max(data[i * 4], data[i * 4 + 1], data[i * 4 + 2])));
        }
        if(peak >= 32)
        {
            long long count = 0, sum[3] = {0, 0, 0};
            for(za::SizeT i = 0; i < n; i++)
            {
                const byte* p = data + i * 4;
                if(za::max(p[0], p[1], p[2]) * 2 >= peak)
                {
                    count++;
                    for(int j = 0; j < 3; j++)
                    {
                        sum[j] += p[j];
                    }
                }
            }
            m.found = true;
            m.share = static_cast<double>(count) / static_cast<double>(n);
            for(int j = 0; j < 3; j++)
            {
                m.colour[j] = static_cast<double>(sum[j]) / static_cast<double>(count);
            }
        }
    }
    Hunk_FreeToLowMark(mark);
    return m;
}

// Glows.glow: (share, colour) of the texture `name`'s glow image in map `map`, or not found.
[[nodiscard]] Measured glowImage(const char* map, const char* name)
{
    char file[64];
    q_strlcpy(file, qvr::strings::lower(za::StringView{name}).cStr(), sizeof(file));
    for(char& c : file)
    {
        if(c == '*')
        {
            c = '#';
        }
    }
    char where[MAX_OSPATH];
    char stems[2][MAX_QPATH * 2];
    q_snprintf(stems[0], sizeof(stems[0]), "textures/%s/%s", map, file);
    q_snprintf(stems[1], sizeof(stems[1]), "textures/%s", file);
    for(const char* stem : stems)
    {
        if(!imageFile(stem, where, sizeof(where)))
        {
            continue;
        }
        for(const char* suffix : {"_glow", "_luma"})
        {
            char glow[MAX_QPATH * 2];
            q_snprintf(glow, sizeof(glow), "%s%s", stem, suffix);
            if(imageFile(glow, where, sizeof(where)))
            {
                return measure(glow, where);
            }
        }
        break;
    }
    // The material maps' (vr_extmaps.cpp's packName: an animation's frame after the name).
    if(!vr_extmaps.value || !vr_extmaps_dir.string[0])
    {
        return {};
    }
    const char* s = file[0] == '#' ? file + 1 : file;
    char base[64];
    if(s[0] == '+' && s[1] && s[2])
    {
        q_snprintf(base, sizeof(base), "%s+%c", s + 2, s[1]);
    }
    else
    {
        q_strlcpy(base, s, sizeof(base));
    }
    if(!extmapsFile(base, where, sizeof(where)))
    {
        char* plus = strchr(base, '+');
        if(!plus)
        {
            return {};
        }
        *plus = 0;
        if(!extmapsFile(base, where, sizeof(where)))
        {
            return {};
        }
    }
    for(const char* suffix : {"_luma", "_glow"})
    {
        char stem[96];
        q_snprintf(stem, sizeof(stem), "%s%s", base, suffix);
        if(extmapsFile(stem, where, sizeof(where)))
        {
            char load[MAX_QPATH * 2];
            q_snprintf(load, sizeof(load), "vrext/%s", stem); // (Image_LoadImage reads vr_extmaps_dir's files so)
            return measure(load, where);
        }
    }
    return {};
}

void lightEntity(za::String& out, const V3& origin, double value, double wait, const char* colour, const char* extra)
{
    char buf[512];
    q_snprintf(buf, sizeof(buf), "{\n\"classname\" \"light\"\n\"origin\" \"%g %g %g\"\n\"light\" \"%d\"\n\"wait\" \"%.2f\"\n\"_color\" \"%s\"\n%s}\n",
        origin[0], origin[1], origin[2], static_cast<int>(value), wait, colour, extra);
    out += buf;
}

[[nodiscard]] za::String glowLights(const Bsp& b, const byte* palette, const za::Vector<Rule>& rules, const char* game,
    const char* map, const Strengths& strength, Report& report)
{
    za::String out;
    if(!b.bsp29())
    {
        return out;
    }
    const int offset = b.lumpOffset(2), length = b.lumpLength(2);
    if(length < 4)
    {
        return out;
    }
    const int count = b.i32(offset);
    if(count <= 0 || 4 + count * 4 > length)
    {
        return out;
    }
    double t0 = Sys_DoubleTime();
    const za::Vector<za::Vector<Face>> faces = textureFaces(b, count);
    report.facesMs = (Sys_DoubleTime() - t0) * 1000.0;
    const World world{&b};
    double kinds[3];
    for(int k = 0; k < 3; k++)
    {
        kinds[k] = 1.0;
        for(const Rule& r : rules)
        {
            if(r.pattern.empty() && r.hasStrength[k])
            {
                kinds[k] = r.strength[k];
            }
        }
        kinds[k] *= strength.kinds[k];
    }

    za::Vector<Glow> glows;
    for(int i = 0; i < count; i++)
    {
        const int mip = b.i32(offset + 4 + i * 4);
        if(mip < 0 || faces[static_cast<za::SizeT>(i)].empty())
        {
            continue;
        }
        const za::SizeT base = static_cast<za::SizeT>(offset) + static_cast<za::SizeT>(mip);
        if(base + 40 > b.size)
        {
            continue;
        }
        char name[17] = {};
        memcpy(name, b.data + base, 16);
        const int width = b.i32(base + 16), height = b.i32(base + 20), pix = b.i32(base + 24);
        const za::String low = qvr::strings::lower(za::StringView{name});
        const za::SizeT npix = width > 0 && height > 0 ? static_cast<za::SizeT>(width) * static_cast<za::SizeT>(height) : 0;
        const bool pixelsOk = pix > 0 && npix && base + static_cast<za::SizeT>(pix) + npix <= b.size;
        const byte* pixels = pixelsOk ? b.data + base + pix : nullptr;
        if(low.size() && low[0] == '*' && pix > 0 && width * height)
        {
            const Rule rule = textureRule(rules, game, map, name);
            if(rule.hasKind && !strcmp(rule.kind, "liquid") && pixels)
            {
                Glow g;
                g.name = za::String{name};
                g.kind = 2;
                g.faces = &faces[static_cast<za::SizeT>(i)];
                g.rule = rule;
                if(rule.hasColor)
                {
                    g.colour = rule.color;
                }
                else
                {
                    for(int j = 0; j < 3; j++)
                    {
                        long long s = 0;
                        for(za::SizeT k = 0; k < npix; k++)
                        {
                            s += palette[pixels[k] * 3 + j];
                        }
                        g.colour[j] = static_cast<double>(s) / static_cast<double>(npix);
                    }
                }
                q_strlcpy(g.source, "liquid", sizeof(g.source));
                glows.pushBack(ZA_MOVE(g));
            }
            continue;
        }
        if(!name[0] || pix <= 0 || startsWith(low, "sky") || width * height == 0 || !pixels)
        {
            continue;
        }
        const Rule rule = textureRule(rules, game, map, name);
        if(rule.hasKind && !strcmp(rule.kind, "liquid"))
        {
            continue;
        }
        int kind = rule.hasKind ? (!strcmp(rule.kind, "fixture") ? 0 : !strcmp(rule.kind, "glow") ? 1 : 3) :
                                  (low.toStringView().find(za::StringView{"light"}) != za::StringView::nPos ||
                                      low.toStringView().find(za::StringView{"lamp"}) != za::StringView::nPos) ? 0 : 1;
        if(kind == 3)
        {
            continue;
        }
        const double least = kind == 0 ? fixtureMinShare : glowMinShare;
        long long glowing = 0, sum[3] = {0, 0, 0};
        for(za::SizeT k = 0; k < npix; k++)
        {
            if(pixels[k] >= 224 && pixels[k] <= 254)
            {
                glowing++;
                for(int j = 0; j < 3; j++)
                {
                    sum[j] += palette[pixels[k] * 3 + j];
                }
            }
        }
        Glow g;
        g.share = static_cast<double>(glowing) / static_cast<double>(npix);
        q_strlcpy(g.source, "fullbright", sizeof(g.source));
        if(g.share >= least)
        {
            for(int j = 0; j < 3; j++)
            {
                g.colour[j] = static_cast<double>(sum[j]) / static_cast<double>(glowing);
            }
        }
        else
        {
            const double ti = Sys_DoubleTime();
            const Measured luma = cachedGlowImage(map, name);
            report.imagesMs += (Sys_DoubleTime() - ti) * 1000.0;
            if(luma.found && luma.share >= least)
            {
                g.share = luma.share;
                g.colour = luma.colour;
                q_strlcpy(g.source, "luma", sizeof(g.source));
                q_strlcpy(g.where, luma.where, sizeof(g.where));
            }
            else if(kind == 0 && rule.hasKind)
            {
                // Named a fixture but nothing glows: the colour of its brightest tenth (a stable sort, as Python's).
                za::Vector<int> order;
                order.resize(npix);
                for(za::SizeT k = 0; k < npix; k++)
                {
                    order[k] = static_cast<int>(k);
                }
                const auto bright = [&](int k) {
                    const int c = pixels[k];
                    return palette[c * 3] + palette[c * 3 + 1] + palette[c * 3 + 2];
                };
                za::stableSort(order.begin(), order.end(), [&](int a, int c) { return -bright(a) < -bright(c); });
                const za::SizeT top = za::max(static_cast<za::SizeT>(1), npix / 10);
                for(int j = 0; j < 3; j++)
                {
                    long long s = 0;
                    for(za::SizeT k = 0; k < top; k++)
                    {
                        s += palette[pixels[order[k]] * 3 + j];
                    }
                    g.colour[j] = static_cast<double>(s) / static_cast<double>(top);
                }
                g.share = 0.1;
                q_strlcpy(g.source, "brightest", sizeof(g.source));
            }
            else
            {
                continue;
            }
        }
        if(rule.hasColor)
        {
            g.colour = rule.color;
        }
        g.name = za::String{name};
        g.kind = kind;
        g.faces = &faces[static_cast<za::SizeT>(i)];
        g.things = thingsOf(faces[static_cast<za::SizeT>(i)]);
        g.rule = rule;
        glows.pushBack(ZA_MOVE(g));
    }
    t0 = Sys_DoubleTime();
    za::Vector<const Thing*> everyone, fixtures;
    for(const Glow& g : glows)
    {
        for(const Thing& t : g.things)
        {
            everyone.pushBack(&t);
            if(g.kind == 0)
            {
                fixtures.pushBack(&t);
            }
        }
    }
    za::Vector<MapLight> lamps;
    if(!fixtures.empty())
    {
        lamps = mapLights(entitiesText(b));
    }

    for(const Glow& g : glows)
    {
        double r = g.colour[0], gg = g.colour[1], bb = g.colour[2];
        const Rule& rule = g.rule;
        za::Vector<double> made;
        char colour[64];
        if(g.kind == 2)
        {
            const double top = za::max(r, gg, bb, 1.0);
            q_snprintf(colour, sizeof(colour), "%d %d %d", static_cast<int>(r * 255 / top), static_cast<int>(gg * 255 / top),
                static_cast<int>(bb * 255 / top));
            const double value = (rule.hasLight ? rule.light : liquidLight) * rule.scale * strength.scale * kinds[2];
            const za::Vector<V3> spots = value >= 12 ? liquidSpots(*g.faces, world) : za::Vector<V3>{};
            for(const V3& p : spots)
            {
                int crowded = 0;
                for(const V3& q : spots)
                {
                    crowded += isNear(p, q, liquidRoom) ? 1 : 0;
                }
                const double v = value * za::pow(za::min(1.0, liquidLone / crowded), liquidCrowd);
                made.pushBack(v);
                lightEntity(out, p, v, 1 / rule.reach, colour, "\"_dirt\" \"-1\"\n");
            }
        }
        else
        {
            const double top = za::max(r, gg, bb, 1.0);
            const double sat = (top - za::min(r, gg, bb)) / top;
            const double white = 0.25 - 0.15 * sat;
            r = r * (1 - white) + top * white;
            gg = gg * (1 - white) + top * white;
            bb = bb * (1 - white) + top * white;
            q_snprintf(colour, sizeof(colour), "%d %d %d", static_cast<int>(r * 255 / top), static_cast<int>(gg * 255 / top),
                static_cast<int>(bb * 255 / top));
            const double wait = 1 / (1 + sat) / rule.reach;
            const double own = rule.scale * strength.scale * kinds[g.kind];
            if(g.kind == 0)
            {
                const double each = (rule.hasLight ? rule.light : fixtureLight) * own * 1.0 * (0.6 + 0.4 * za::min(1.0, g.share * 5));
                for(const Thing& thing : g.things)
                {
                    double value = each;
                    const V3& c = thing.faces[0]->centre;
                    bool by = false;
                    double strongest = 0.0;
                    for(const MapLight& l : lamps)
                    {
                        if(l.off)
                        {
                            continue;
                        }
                        for(const Face* f : thing.faces)
                        {
                            if(isNear(l.origin, f->centre, fixtureNear))
                            {
                                strongest = by ? za::max(strongest, l.light) : l.light;
                                by = true;
                                break;
                            }
                        }
                    }
                    if(by)
                    {
                        value *= 1 - (1 - fixtureLit) * za::min(1.0, strongest / 300.0);
                    }
                    int room = 0;
                    for(const Thing* t : fixtures)
                    {
                        room += isNear(c, t->faces[0]->centre, glowRoom) ? 1 : 0;
                    }
                    value /= za::pow(static_cast<double>(room), 0.5);
                    PySum area;
                    for(const Face* f : thing.faces)
                    {
                        area.add(f->area);
                    }
                    value *= za::min(1.0, za::max(0.5, za::pow(area.value() / 1024.0, 0.5)));
                    const za::Vector<V3> spots = fixtureSpots(thing.faces, world);
                    for(const V3& p : spots)
                    {
                        const double v = za::min(300 * own * 1.0, value * za::max(0.4, za::pow(static_cast<double>(spots.size()), -0.5)));
                        if(v >= 12)
                        {
                            made.pushBack(v);
                            lightEntity(out, p, v, wait, colour, "\"_dirt\" \"-1\"\n");
                        }
                    }
                }
            }
            else
            {
                const double budget = glowBudget * za::min(1.0, 0.4 + g.share * 2) * own;
                const double cap = 130 * own * (1 + 0.3 * sat);
                bool smallFaces = true;
                for(const Face& f : *g.faces)
                {
                    smallFaces = smallFaces && f.area <= 2 * sq(surflight);
                }
                if(smallFaces)
                {
                    for(const Thing& thing : g.things)
                    {
                        const double value = za::min(cap, budget * (1 + sat) / crowd(thing, everyone));
                        za::Vector<V3> spots;
                        for(const Face* f : thing.faces)
                        {
                            const V3 p = along(f->centre, f->normal, 2);
                            if(!world.solid(p))
                            {
                                spots.pushBack(p);
                            }
                        }
                        for(const V3& p : spots)
                        {
                            const double v = value / za::pow(static_cast<double>(spots.size()), 0.5);
                            if(v >= 12)
                            {
                                made.pushBack(v);
                                lightEntity(out, p, v, wait, colour, "");
                            }
                        }
                    }
                }
                else
                {
                    double most = 0.0;
                    bool first = true;
                    for(const Thing& t : g.things)
                    {
                        const double c = crowd(t, everyone);
                        most = first ? c : za::max(most, c);
                        first = false;
                    }
                    const double value = za::min(cap, budget * (1 + sat) / most);
                    if(value >= 12)
                    {
                        made.pushBack(value);
                        char buf[512];
                        q_snprintf(buf, sizeof(buf),
                            "{\n\"classname\" \"light\"\n\"_surface\" \"%s\"\n\"light\" \"%d\"\n\"wait\" \"%.2f\"\n\"_color\" \"%s\"\n\"_surface_offset\" \"2\"\n}\n",
                            g.name.cStr(), static_cast<int>(value), wait, colour);
                        out += buf;
                    }
                }
            }
        }
        report.textures++;
        report.lights += static_cast<int>(made.size());
        double lo = 0.0, hi = 0.0;
        for(za::SizeT k = 0; k < made.size(); k++)
        {
            lo = k ? za::min(lo, made[k]) : made[k];
            hi = k ? za::max(hi, made[k]) : made[k];
        }
        char line[MAX_OSPATH + 160];
        q_snprintf(line, sizeof(line), "  %-16s %-7s %-10s glows %5.1f%%  lights %4d  %s%s%s\n", g.name.cStr(),
            kindNames[g.kind], g.source, g.share * 100, static_cast<int>(made.size()),
            made.empty() ? "(none)" : va("light %d..%d", static_cast<int>(lo), static_cast<int>(hi)), g.where[0] ? "  " : "",
            g.where);
        report.lines += line;
    }
    report.spotsMs = (Sys_DoubleTime() - t0) * 1000.0;
    return out;
}
} // namespace detail
} // namespace qvr::relight
