// vr_relight_bsp.cpp -- part of vr_relight.cpp (see vr_relight.hpp, vr_relight_internal.hpp): the BSP's lumps (BSPX,
// the entity lump written back), the entity text's keys and blocks, the map's own lights and their settings scaled or
// taken out, and the texture rules (relight_rules.txt) read and matched.

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

// The entity lump's text (up to its first NUL).
[[nodiscard]] za::String entitiesText(const Bsp& b)
{
    const int o = b.lumpOffset(0), l = b.lumpLength(0);
    za::SizeT n = 0;
    while(n < static_cast<za::SizeT>(l) && b.data[o + n])
    {
        n++;
    }
    return za::String{reinterpret_cast<const char*>(b.data + o), n};
}

[[nodiscard]] za::Vector<BspxLump> bspxLumps(const Bsp& b)
{
    za::Vector<BspxLump> out;
    za::SizeT end = 0;
    for(int i = 0; i < 15; i++)
    {
        end = za::max(end, static_cast<za::SizeT>(b.lumpOffset(i)) + static_cast<za::SizeT>(b.lumpLength(i)));
    }
    end = (end + 3) & ~static_cast<za::SizeT>(3);
    if(end + 8 > b.size || memcmp(b.data + end, "BSPX", 4))
    {
        return out;
    }
    const int count = b.i32(end + 4);
    for(int i = 0; i < count; i++)
    {
        const za::SizeT at = end + 8 + static_cast<za::SizeT>(i) * 32;
        if(at + 32 > b.size)
        {
            break;
        }
        BspxLump l;
        memcpy(l.name, b.data + at, 24);
        l.offset = b.i32(at + 24);
        l.length = b.i32(at + 28);
        if(l.offset < 0 || l.length < 0 || static_cast<za::SizeT>(l.offset) + static_cast<za::SizeT>(l.length) > b.size)
        {
            continue;
        }
        out.pushBack(l);
    }
    return out;
}

// The .bsp with its entity lump `entities` (and a NUL), its lumps one after another and then its BSPX lumps
// (vis_maps.packed): where engines look for them. With `patch`: its visibility and leaves in place of the map's
// (vis_maps.vispatch; the caller checked it fits).
[[nodiscard]] za::Vector<unsigned char> withEntities(const Bsp& b, za::StringView entities, const vis::Patch* patch)
{
    za::Vector<unsigned char> out;
    const auto pad = [&] {
        while(out.size() % 4)
        {
            out.pushBack(0);
        }
    };
    const auto put32 = [&](za::SizeT at, int v) {
        v = LittleLong(v);
        memcpy(out.data() + at, &v, 4);
    };
    out.resize(4 + 15 * 8, 0);
    memcpy(out.data(), b.data, 4);
    for(int i = 0; i < 15; i++)
    {
        pad();
        const za::SizeT at = out.size();
        if(i == 0)
        {
            out.resize(at + entities.size() + 1, 0);
            memcpy(out.data() + at, entities.data(), entities.size());
        }
        else if(patch && (i == 4 || i == 10))
        {
            const za::Vector<unsigned char>& lump = i == 4 ? patch->visibility : patch->leafs;
            out.resize(at + lump.size(), 0);
            if(!lump.empty())
            {
                memcpy(out.data() + at, lump.data(), lump.size());
            }
        }
        else
        {
            const int o = b.lumpOffset(i), l = b.lumpLength(i);
            out.resize(at + static_cast<za::SizeT>(l), 0);
            if(l)
            {
                memcpy(out.data() + at, b.data + o, static_cast<za::SizeT>(l));
            }
        }
        put32(4 + static_cast<za::SizeT>(i) * 8, static_cast<int>(at));
        put32(8 + static_cast<za::SizeT>(i) * 8, static_cast<int>(out.size() - at));
    }
    pad();
    const za::Vector<BspxLump> bspx = bspxLumps(b);
    if(!bspx.empty())
    {
        const za::SizeT table = out.size() + 8;
        out.resize(table + 32 * bspx.size(), 0);
        memcpy(out.data() + table - 8, "BSPX", 4);
        put32(table - 4, static_cast<int>(bspx.size()));
        for(za::SizeT i = 0; i < bspx.size(); i++)
        {
            pad();
            const za::SizeT at = out.size();
            out.resize(at + static_cast<za::SizeT>(bspx[i].length), 0);
            memcpy(out.data() + at, b.data + bspx[i].offset, static_cast<za::SizeT>(bspx[i].length));
            memcpy(out.data() + table + i * 32, bspx[i].name, 24);
            put32(table + i * 32 + 24, static_cast<int>(at));
            put32(table + i * 32 + 28, bspx[i].length);
        }
        pad();
    }
    return out;
}

// The keys of a piece of entity text: re.findall(r'"([^"]*)"\s+"([^"]*)"').
[[nodiscard]] za::Vector<Key> keysOf(za::StringView text)
{
    za::Vector<Key> out;
    za::SizeT p = 0;
    while(true)
    {
        p = text.find('"', p);
        if(p == za::StringView::nPos)
        {
            break;
        }
        const za::SizeT q1 = text.find('"', p + 1);
        if(q1 == za::StringView::nPos)
        {
            break;
        }
        za::SizeT w = q1 + 1;
        while(w < text.size() && (text[w] == ' ' || text[w] == '\t' || text[w] == '\n' || text[w] == '\r' ||
                                     text[w] == '\f' || text[w] == '\v'))
        {
            w++;
        }
        const za::SizeT q2 = w < text.size() && w > q1 + 1 && text[w] == '"' ? text.find('"', w + 1) : za::StringView::nPos;
        if(q2 == za::StringView::nPos)
        {
            p++;
            continue;
        }
        out.pushBack(Key{za::String{text.substrByPosLen(p + 1, q1 - p - 1)}, za::String{text.substrByPosLen(w + 1, q2 - w - 1)},
            p, q2 + 1});
        p = q2 + 1;
    }
    return out;
}

// dict(keys).get(name): the last of that name.
[[nodiscard]] const za::String* keyValue(const za::Vector<Key>& keys, const char* name)
{
    const za::String* found = nullptr;
    for(const Key& k : keys)
    {
        if(k.name == za::StringView{name})
        {
            found = &k.value;
        }
    }
    return found;
}

[[nodiscard]] za::Vector<Span> blocksOf(za::StringView text)
{
    za::Vector<Span> out;
    za::SizeT i = 0;
    while(i < text.size())
    {
        if(text[i] != '{')
        {
            i++;
            continue;
        }
        za::SizeT j = i + 1;
        while(j < text.size() && text[j] != '{' && text[j] != '}')
        {
            j++;
        }
        if(j < text.size() && text[j] == '}')
        {
            out.pushBack(Span{i, j + 1});
            i = j + 1;
        }
        else
        {
            i = j;
        }
    }
    return out;
}

// Python's float() of a word (False: not a number).
[[nodiscard]] bool parseFloat(za::StringView word, double& out)
{
    char buf[64];
    za::SizeT a = 0, b = word.size();
    while(a < b && (word[a] == ' ' || word[a] == '\t' || word[a] == '\n' || word[a] == '\r'))
    {
        a++;
    }
    while(b > a && (word[b - 1] == ' ' || word[b - 1] == '\t' || word[b - 1] == '\n' || word[b - 1] == '\r'))
    {
        b--;
    }
    if(a == b || b - a >= sizeof(buf))
    {
        return false;
    }
    memcpy(buf, word.data() + a, b - a);
    buf[b - a] = 0;
    char* end = nullptr;
    out = strtod(buf, &end);
    return end && *end == 0;
}

// The words of a value (str.split()).
[[nodiscard]] za::Vector<za::StringView> wordsOf(za::StringView s)
{
    za::Vector<za::StringView> out;
    za::SizeT i = 0;
    while(i < s.size())
    {
        while(i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
        {
            i++;
        }
        za::SizeT j = i;
        while(j < s.size() && !(s[j] == ' ' || s[j] == '\t' || s[j] == '\n' || s[j] == '\r'))
        {
            j++;
        }
        if(j > i)
        {
            out.pushBack(s.substrByPosLen(i, j - i));
        }
        i = j;
    }
    return out;
}

[[nodiscard]] bool startsWith(za::StringView s, const char* prefix)
{
    return s.startsWith(za::StringView{prefix});
}

[[nodiscard]] za::Vector<MapLight> mapLights(za::StringView text)
{
    za::Vector<MapLight> out;
    za::SizeT start = 0;
    while(start <= text.size())
    {
        za::SizeT end = text.find('}', start);
        if(end == za::StringView::nPos)
        {
            end = text.size();
        }
        const za::Vector<Key> keys = keysOf(text.substrByPosLen(start, end - start));
        start = end + 1;
        const za::String* cls = keyValue(keys, "classname");
        const za::String* origin = keyValue(keys, "origin");
        if(!cls || !startsWith(*cls, "light") || !origin)
        {
            continue;
        }
        const za::Vector<za::StringView> o = wordsOf(*origin);
        const za::String* flagsText = keyValue(keys, "spawnflags");
        const za::String* light = keyValue(keys, "light");
        if(!light)
        {
            light = keyValue(keys, "_light");
        }
        const za::Vector<za::StringView> lw = wordsOf(light ? za::StringView{*light} : za::StringView{"300"});
        MapLight m;
        double flags = 0.0;
        bool ok = o.size() >= 3 && !lw.empty() && parseFloat(flagsText ? za::StringView{*flagsText} : za::StringView{"0"}, flags) &&
                  parseFloat(lw.back(), m.light);
        for(int j = 0; ok && j < 3; j++)
        {
            ok = parseFloat(o[static_cast<za::SizeT>(j)], m.origin[j]);
        }
        if(!ok || !isfinite(flags))
        {
            continue;
        }
        m.light = za::abs(m.light);
        m.off = keyValue(keys, "targetname") && !keyValue(keys, "targetname")->empty() && (static_cast<long long>(flags) & 1);
        out.pushBack(m);
    }
    return out;
}

// id_light_values: a light of "light" "0" gets id's default, 300 (ericw-tools 2 takes it as 0).
[[nodiscard]] za::String idLightValues(za::StringView text)
{
    za::String out;
    za::SizeT last = 0;
    for(const Span& s : blocksOf(text))
    {
        out += text.substrByPosLen(last, s.start - last);
        last = s.end;
        const za::StringView block = text.substrByPosLen(s.start, s.end - s.start);
        const za::Vector<Key> keys = keysOf(block);
        const za::String* cls = keyValue(keys, "classname");
        const za::String* light = keyValue(keys, "light");
        double v = 300.0;
        bool zero = false;
        if(cls && startsWith(*cls, "light"))
        {
            const za::Vector<za::StringView> w = wordsOf(light ? za::StringView{*light} : za::StringView{"300"});
            zero = !w.empty() && parseFloat(w.back(), v) && v == 0.0;
        }
        if(!zero)
        {
            out += block;
            continue;
        }
        // Each key and its value in turn: a "light" that is a value (the classname's) is not the key.
        za::SizeT p = 0;
        for(const Key& k : keys)
        {
            if(k.name == za::StringView{"light"})
            {
                out += block.substrByPosLen(p, k.start - p);
                out += "\"light\" \"300\"";
                p = k.end;
            }
        }
        out += block.substrByPosLen(p, block.size() - p);
    }
    out += text.substrByPosLen(last, text.size() - last);
    return out;
}

// without_map_light_settings: the worldspawn's "_" keys left out (the re-release's maps carry ericw-tools settings of
// their own, which would take over the look's); for id's games' maps only (another map's mapper set them to be used).
[[nodiscard]] za::String withoutMapLightSettings(za::StringView text)
{
    const za::Vector<Span> blocks = blocksOf(text);
    if(blocks.empty())
    {
        return za::String{text};
    }
    const za::StringView first = text.substrByPosLen(blocks[0].start, blocks[0].end - blocks[0].start);
    if(first.find(za::StringView{"\"worldspawn\""}) == za::StringView::nPos)
    {
        return za::String{text};
    }
    // re.sub(r'\s*"_[^"]*"\s+"[^"]*"', "", block)
    za::String block;
    za::SizeT p = 0;
    while(p < first.size())
    {
        za::SizeT ws = p;
        while(ws < first.size() && (first[ws] == ' ' || first[ws] == '\t' || first[ws] == '\n' || first[ws] == '\r'))
        {
            ws++;
        }
        bool matched = false;
        if(ws + 1 < first.size() && first[ws] == '"' && first[ws + 1] == '_')
        {
            const za::SizeT q1 = first.find('"', ws + 1);
            if(q1 != za::StringView::nPos)
            {
                za::SizeT w = q1 + 1;
                while(w < first.size() && (first[w] == ' ' || first[w] == '\t' || first[w] == '\n' || first[w] == '\r'))
                {
                    w++;
                }
                if(w > q1 + 1 && w < first.size() && first[w] == '"')
                {
                    const za::SizeT q2 = first.find('"', w + 1);
                    if(q2 != za::StringView::nPos)
                    {
                        p = q2 + 1;
                        matched = true;
                    }
                }
            }
        }
        if(!matched)
        {
            block.pushBack(first[p]);
            p++;
        }
    }
    za::String out{text.substrByPosLen(0, blocks[0].start)};
    out += block;
    out += text.substrByPosLen(blocks[0].end, text.size() - blocks[0].end);
    return out;
}

// A number's text for a key's value (as a mapper would write it).
void appendNumber(za::String& out, double v)
{
    char buf[32];
    q_snprintf(buf, sizeof(buf), "%g", v);
    out += buf;
}

// The map's own lights `mapScale` times as bright (and their linear falloff as long: "wait" with it), its sunlight
// `sunScale` times: the page's Map Lights and Sunlight. `minLight` (> 0) is the page's Ambient Light Level: added to
// the worldspawn's own minimum light ("light" / "_minlight", light's minlight; most of id's maps have none, so there
// it is the level itself), and not darkened by the ambient occlusion ("_minlight_dirt" 0: -dirt otherwise darkens
// minlight too, and the darkest corners stayed dark). Added, not a floor: a map whose mapper set a minimum above the
// slider's top (vrarena's 90) looked the same at every level (the author's report, 2026-10-29). -minlight on light's
// command line would override the map's own instead. Each key set again at the block's end.
[[nodiscard]] za::String scaledLights(za::StringView text, double mapScale, double sunScale, double minLight)
{
    if(mapScale == 1.0 && sunScale == 1.0 && minLight <= 0.0)
    {
        return za::String{text};
    }
    za::String out;
    za::SizeT last = 0;
    for(const Span& s : blocksOf(text))
    {
        out += text.substrByPosLen(last, s.start - last);
        last = s.end;
        const za::StringView block = text.substrByPosLen(s.start, s.end - s.start);
        const za::Vector<Key> keys = keysOf(block);
        const za::String* cls = keyValue(keys, "classname");
        za::Vector<Key> set;
        if(cls && startsWith(*cls, "light") && mapScale != 1.0)
        {
            const za::String* light = keyValue(keys, "light");
            const za::Vector<za::StringView> w = wordsOf(light ? za::StringView{*light} : za::StringView{"300"});
            double v = 300.0;
            if(!w.empty() && parseFloat(w.back(), v))
            {
                za::String value;
                for(za::SizeT i = 0; i + 1 < w.size(); i++) // (a colour before it: kept)
                {
                    value += w[i];
                    value += " ";
                }
                appendNumber(value, v * mapScale);
                set.pushBack(Key{za::String{"light"}, ZA_MOVE(value)});
                const za::String* delay = keyValue(keys, "delay");
                double d = 0.0;
                if(!delay || (parseFloat(*delay, d) && d == 0.0)) // linear falloff: "wait" with it, the reach kept
                {
                    const za::String* wait = keyValue(keys, "wait");
                    double wv = 1.0;
                    if(!wait || !parseFloat(*wait, wv))
                    {
                        wv = 1.0;
                    }
                    za::String waitValue;
                    appendNumber(waitValue, wv * mapScale);
                    set.pushBack(Key{za::String{"wait"}, ZA_MOVE(waitValue)});
                }
            }
        }
        if(cls && *cls == za::StringView{"worldspawn"} && sunScale != 1.0)
        {
            for(const char* name : {"_sunlight", "_sun_light", "_sunlight2", "_sunlight3", "_sun2"})
            {
                const za::String* v = keyValue(keys, name);
                double x = 0.0;
                if(v && parseFloat(*v, x))
                {
                    za::String value;
                    appendNumber(value, x * sunScale);
                    set.pushBack(Key{za::String{name}, ZA_MOVE(value)});
                }
            }
        }
        if(cls && *cls == za::StringView{"worldspawn"} && minLight > 0.0)
        {
            double own = 0.0;
            for(const char* name : {"light", "_minlight"})
            {
                const za::String* v = keyValue(keys, name);
                double x = 0.0;
                if(v && parseFloat(*v, x))
                {
                    own = za::max(own, x);
                }
            }
            for(const char* name : {"light", "_minlight"}) // (both: light takes the later of the two)
            {
                za::String value;
                appendNumber(value, za::min(255.0, own + minLight));
                set.pushBack(Key{za::String{name}, ZA_MOVE(value)});
            }
            set.pushBack(Key{za::String{"_minlight_dirt"}, za::String{"0"}});
        }
        if(set.empty())
        {
            out += block;
            continue;
        }
        out += "{";
        for(const Key& k : keys)
        {
            bool replaced = false;
            for(const Key& n : set)
            {
                replaced = replaced || k.name == n.name;
            }
            if(!replaced)
            {
                out += "\n\"";
                out += k.name;
                out += "\" \"";
                out += k.value;
                out += "\"";
            }
        }
        for(const Key& n : set)
        {
            out += "\n\"";
            out += n.name;
            out += "\" \"";
            out += n.value;
            out += "\"";
        }
        out += "\n}";
    }
    out += text.substrByPosLen(last, text.size() - last);
    return out;
}

// fnmatch.fnmatchcase: *, ?, [seq], [!seq].
[[nodiscard]] bool fnmatch(const char* p, const char* s)
{
    for(; *p; p++)
    {
        if(*p == '*')
        {
            while(p[1] == '*')
            {
                p++;
            }
            for(const char* t = s;; t++)
            {
                if(fnmatch(p + 1, t))
                {
                    return true;
                }
                if(!*t)
                {
                    return false;
                }
            }
        }
        if(!*s)
        {
            return false;
        }
        if(*p == '?')
        {
            s++;
            continue;
        }
        if(*p == '[')
        {
            const char* q = p + 1;
            const bool negate = *q == '!';
            if(negate)
            {
                q++;
            }
            if(*q == ']')
            {
                q++; // (a ] first is in the set)
            }
            while(*q && *q != ']')
            {
                q++;
            }
            if(*q == ']')
            {
                bool in = false;
                const char* c = p + 1 + (negate ? 1 : 0);
                for(bool first = true; first || *c != ']'; first = false)
                {
                    if(c[1] == '-' && c[2] && c[2] != ']')
                    {
                        in = in || (*s >= c[0] && *s <= c[2]);
                        c += 3;
                    }
                    else
                    {
                        in = in || *s == *c;
                        c++;
                    }
                }
                if(in == negate)
                {
                    return false;
                }
                s++;
                p = q;
                continue;
            }
            // (no closing ]: a literal [)
        }
        if(*p != *s)
        {
            return false;
        }
        s++;
    }
    return !*s;
}

// The rules of relight_textures.cfg (load_rules); a line it cannot read is said and skipped.
[[nodiscard]] za::Vector<Rule> loadRules(const char* text, const char* file)
{
    za::Vector<Rule> rules;
    int number = 0;
    files::forLines(za::StringView{text}, [&](za::StringView line) {
        number++;
        const za::SizeT hash = line.find('#');
        if(hash != za::StringView::nPos)
        {
            line = line.substrByPosLen(0, hash);
        }
        const za::Vector<za::StringView> words = wordsOf(line);
        if(words.empty())
        {
            return;
        }
        Rule r;
        const za::String first = qvr::strings::lower(words[0]);
        bool ok = true;
        if(first == za::StringView{"strength"})
        {
            for(za::SizeT i = 1; i < words.size() && ok; i++)
            {
                const za::SizeT eq = words[i].find('=');
                ok = false;
                for(int k = 0; k < 3 && eq != za::StringView::nPos; k++)
                {
                    if(words[i].substrByPosLen(0, eq) == za::StringView{kindNames[k]})
                    {
                        ok = parseFloat(words[i].substrByPosLen(eq + 1, words[i].size() - eq - 1), r.strength[k]);
                        r.hasStrength[k] = ok;
                    }
                }
            }
        }
        else
        {
            int slashes = 0;
            for(char c : first)
            {
                slashes += c == '/';
            }
            for(int k = slashes; k < 2; k++)
            {
                r.pattern += "*/";
            }
            r.pattern += first;
            for(za::SizeT i = 1; i < words.size() && ok; i++)
            {
                const za::SizeT eq = words[i].find('=');
                ok = eq != za::StringView::nPos && eq + 1 < words[i].size();
                if(!ok)
                {
                    break;
                }
                const za::StringView key = words[i].substrByPosLen(0, eq);
                const za::StringView value = words[i].substrByPosLen(eq + 1, words[i].size() - eq - 1);
                if(key == za::StringView{"kind"})
                {
                    ok = false;
                    for(const char* k : kindNames)
                    {
                        if(value == za::StringView{k})
                        {
                            q_strlcpy(r.kind, k, sizeof(r.kind));
                            ok = r.hasKind = true;
                        }
                    }
                }
                else if(key == za::StringView{"scale"})
                {
                    ok = r.hasScale = parseFloat(value, r.scale);
                }
                else if(key == za::StringView{"light"})
                {
                    ok = r.hasLight = parseFloat(value, r.light);
                }
                else if(key == za::StringView{"reach"})
                {
                    ok = r.hasReach = parseFloat(value, r.reach);
                }
                else if(key == za::StringView{"color"})
                {
                    int n = 0;
                    za::SizeT a = 0;
                    while(ok && a <= value.size())
                    {
                        za::SizeT c = value.find(',', a);
                        if(c == za::StringView::nPos)
                        {
                            c = value.size();
                        }
                        ok = n < 3 && parseFloat(value.substrByPosLen(a, c - a), r.color[n]);
                        n++;
                        a = c + 1;
                    }
                    ok = ok && n == 3;
                    r.hasColor = ok;
                }
                else
                {
                    ok = false;
                }
            }
        }
        if(!ok)
        {
            Con_Printf("%s:%d: not understood, left out\n", file, number);
            return;
        }
        rules.pushBack(ZA_MOVE(r));
    });
    return rules;
}

// texture_rule: every rule matching game/map/texture, later ones' settings over earlier ones'.
[[nodiscard]] Rule textureRule(const za::Vector<Rule>& rules, const char* game, const char* map, const char* name)
{
    char full[256];
    q_snprintf(full, sizeof(full), "%s/%s/%s", game, map, qvr::strings::lower(za::StringView{name}).cStr());
    Rule merged;
    for(const Rule& r : rules)
    {
        if(r.pattern.empty() || !fnmatch(r.pattern.cStr(), full))
        {
            continue;
        }
        if(r.hasKind)
        {
            q_strlcpy(merged.kind, r.kind, sizeof(merged.kind));
            merged.hasKind = true;
        }
        if(r.hasScale)
        {
            merged.scale = r.scale;
            merged.hasScale = true;
        }
        if(r.hasLight)
        {
            merged.light = r.light;
            merged.hasLight = true;
        }
        if(r.hasReach)
        {
            merged.reach = r.reach;
            merged.hasReach = true;
        }
        if(r.hasColor)
        {
            merged.color = r.color;
            merged.hasColor = true;
        }
    }
    return merged;
}
} // namespace detail
} // namespace qvr::relight
