// vr_brushes_text.cpp -- the brush (vr_brushes.cpp): the default alignment, the shapes made, moved, turned and scaled,
// and a brush's .map text written and read.

#include "vr_brushes_internal.hpp"

namespace qvr::brushes
{
using namespace detail;

namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// The default alignment (Quake's TextureAxisFromPlane): the world's axis nearest the normal picks the texture's axes.

constexpr double baseAxes[18][3] = {
    {0, 0, 1}, {1, 0, 0}, {0, -1, 0},  // floor
    {0, 0, -1}, {1, 0, 0}, {0, -1, 0}, // ceiling
    {1, 0, 0}, {0, 1, 0}, {0, 0, -1},  // west wall
    {-1, 0, 0}, {0, 1, 0}, {0, 0, -1}, // east wall
    {0, 1, 0}, {1, 0, 0}, {0, 0, -1},  // south wall
    {0, -1, 0}, {1, 0, 0}, {0, 0, -1}, // north wall
};

[[nodiscard]] int baseAxisOf(const glm::dvec3& n)
{
    int best = 0;
    double bestDot = 0.0;
    for(int i = 0; i < 6; i++)
    {
        const double d = n.x * baseAxes[i * 3][0] + n.y * baseAxes[i * 3][1] + n.z * baseAxes[i * 3][2];
        if(d > bestDot)
        {
            bestDot = d;
            best = i;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------------------------------------------------
// The text.

void appendNumber(za::String& out, double v)
{
    char buf[48];
    if(za::fabs(v - za::round(v)) < 1e-9 && za::fabs(v) < 1e15)
    {
        q_snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(za::round(v)));
    }
    else
    {
        q_snprintf(buf, sizeof(buf), "%.12g", v);
    }
    out += buf;
}

// The three points written for a face: on its polygon (clockwise seen from outside: qbsp's plane from points gives
// the outward normal), the corners spanning the largest triangle; a face with no polygon: three on its plane.
void facePoints(const Face& f, const Winding& w, glm::dvec3 (&p)[3])
{
    if(w.size() >= 3)
    {
        double best = -1.0;
        for(za::SizeT b = 1; b + 1 < w.size(); b++)
        {
            for(za::SizeT c = b + 1; c < w.size(); c++)
            {
                const double area = glm::length(glm::cross(w[b] - w[0], w[c] - w[0]));
                if(area > best)
                {
                    best = area;
                    p[0] = w[0];
                    p[1] = w[b];
                    p[2] = w[c];
                }
            }
        }
        return;
    }
    const Winding base = baseWinding(f.normal, f.dist);
    p[0] = base[0] / worldExtent * 64.0 + f.normal * f.dist * (1.0 - 64.0 / worldExtent);
    p[1] = base[1] / worldExtent * 64.0 + f.normal * f.dist * (1.0 - 64.0 / worldExtent);
    p[2] = base[2] / worldExtent * 64.0 + f.normal * f.dist * (1.0 - 64.0 / worldExtent);
}

// The text's tokens: words, numbers, ( ) [ ] { }; // comments skipped (the header read before).
struct Tokens
{
    za::StringView text;
    za::SizeT at{0};
    char token[64]{};

    bool next()
    {
        while(at < text.size())
        {
            const char c = text[at];
            if(c == '/' && at + 1 < text.size() && text[at + 1] == '/')
            {
                while(at < text.size() && text[at] != '\n')
                {
                    at++;
                }
                continue;
            }
            if(c == ' ' || c == '\t' || c == '\r' || c == '\n')
            {
                at++;
                continue;
            }
            break;
        }
        if(at >= text.size())
        {
            token[0] = '\0';
            return false;
        }
        const char c = text[at];
        if(c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}')
        {
            token[0] = c;
            token[1] = '\0';
            at++;
            return true;
        }
        za::SizeT n = 0;
        while(at < text.size() && n + 1 < sizeof(token))
        {
            const char d = text[at];
            if(d == ' ' || d == '\t' || d == '\r' || d == '\n' || d == '(' || d == ')' || d == '[' || d == ']' ||
                d == '{' || d == '}')
            {
                break;
            }
            token[n++] = d;
            at++;
        }
        token[n] = '\0';
        return true;
    }
    bool expect(const char* s) { return next() && !strcmp(token, s); }
    bool number(double& v)
    {
        if(!next())
        {
            return false;
        }
        char* end = nullptr;
        v = strtod(token, &end);
        return end && *end == '\0' && end != token;
    }
};

} // namespace

// ---------------------------------------------------------------------------------------------------------------------
// The brush.

void defaultAlignment(Face& f)
{
    const int i = baseAxisOf(f.normal);
    f.uAxis = glm::dvec3{baseAxes[i * 3 + 1][0], baseAxes[i * 3 + 1][1], baseAxes[i * 3 + 1][2]};
    f.vAxis = glm::dvec3{baseAxes[i * 3 + 2][0], baseAxes[i * 3 + 2][1], baseAxes[i * 3 + 2][2]};
    f.uOffset = f.vOffset = 0.0;
    f.rotation = 0.0;
    f.scaleU = f.scaleV = 1.0;
}

Brush box(const glm::dvec3& lo, const glm::dvec3& hi, const char* texture)
{
    Brush b;
    for(int axis = 0; axis < 3; axis++)
    {
        for(const double sign : {1.0, -1.0})
        {
            Face f;
            f.normal = glm::dvec3{0.0};
            f.normal[axis] = sign;
            f.dist = sign > 0.0 ? hi[axis] : -lo[axis];
            q_strlcpy(f.texture, texture && texture[0] ? texture : "base", sizeof(f.texture));
            defaultAlignment(f);
            b.faces.pushBack(f);
        }
    }
    return b;
}

bool bounds(const Brush& b, glm::dvec3& lo, glm::dvec3& hi)
{
    za::Vector<Winding> polys;
    lo = glm::dvec3{0.0};
    hi = glm::dvec3{0.0};
    const bool solid = polygons(b, polys);
    glm::dvec3 a{1e30}, c{-1e30};
    for(const Winding& w : polys)
    {
        for(const glm::dvec3& p : w)
        {
            a = glm::min(a, p);
            c = glm::max(c, p);
        }
    }
    if(a.x > c.x)
    {
        return false;
    }
    lo = a;
    hi = c;
    return solid && c.x - a.x >= 1.0 && c.y - a.y >= 1.0 && c.z - a.z >= 1.0;
}

const char* invalid(const Brush& b)
{
    if(b.faces.size() < 4)
    {
        return "fewer than four faces";
    }
    if(static_cast<int>(b.faces.size()) > maxFaces)
    {
        return "more faces than a brush may have";
    }
    for(const Face& f : b.faces)
    {
        if(za::fabs(glm::length(f.normal) - 1.0) > 1e-6)
        {
            return "a face's normal is not a unit";
        }
    }
    glm::dvec3 lo, hi;
    if(!bounds(b, lo, hi))
    {
        return "no volume (a side under a unit, or not closed)";
    }
    for(int i = 0; i < 3; i++)
    {
        if(lo[i] < -16384.0 || hi[i] > 16384.0)
        {
            return "outside the world's bounds";
        }
    }
    za::Vector<Winding> polys;
    (void)polygons(b, polys);
    int verts = 0, tris = 0;
    for(const Winding& w : polys)
    {
        verts += static_cast<int>(w.size());
        tris += w.size() >= 3 ? static_cast<int>(w.size()) - 2 : 0;
    }
    if(verts > QVR_BRUSH_SLOT_VERTS || tris * 3 > QVR_BRUSH_SLOT_INDEXES)
    {
        return "too many corners for its slot";
    }
    return nullptr;
}

void translate(Brush& b, const glm::dvec3& delta)
{
    for(Face& f : b.faces)
    {
        f.dist += glm::dot(f.normal, delta);
        // The texture moves with the face: its coordinate at a moved point is what it was at the point before.
        f.uOffset -= glm::dot(f.uAxis, delta) / (f.scaleU != 0.0 ? f.scaleU : 1.0);
        f.vOffset -= glm::dot(f.vAxis, delta) / (f.scaleV != 0.0 ? f.scaleV : 1.0);
    }
}

namespace
{

constexpr const char* shapeNames[] = {"box", "wedge", "cylinder", "pyramid"};

[[nodiscard]] Face planeFace(const glm::dvec3& normal, double dist, const char* texture)
{
    Face f;
    f.normal = normal;
    f.dist = dist;
    q_strlcpy(f.texture, texture && texture[0] ? texture : "base", sizeof(f.texture));
    defaultAlignment(f);
    return f;
}

// A plane's normal and distance with the dust of turning and stretching swept off: components a hair from 0 or 1 put
// there (a box turned 90 degrees is a box again), a distance a hair from a whole number on it.
void tidy(glm::dvec3& n, double& d)
{
    for(int k = 0; k < 3; k++)
    {
        if(za::fabs(n[k]) < 1e-12)
        {
            n[k] = 0.0;
        }
        else if(za::fabs(za::fabs(n[k]) - 1.0) < 1e-12)
        {
            n[k] = n[k] > 0.0 ? 1.0 : -1.0;
        }
    }
    n = glm::normalize(n);
    if(za::fabs(d - za::round(d)) < 1e-7)
    {
        d = za::round(d);
    }
}

} // namespace

const char* shapeName(Shape s)
{
    const int i = static_cast<int>(s);
    return i >= 0 && i < static_cast<int>(Shape::Count) ? shapeNames[i] : "box";
}

Shape shapeFromName(const char* name)
{
    for(int i = 0; i < static_cast<int>(Shape::Count); i++)
    {
        if(!q_strcasecmp(name, shapeNames[i]))
        {
            return static_cast<Shape>(i);
        }
    }
    const int n = Q_atoi(name);
    return n >= 0 && n < static_cast<int>(Shape::Count) ? static_cast<Shape>(n) : Shape::Box;
}

Brush shape(Shape s, const glm::dvec3& lo, const glm::dvec3& hi, int upAxis, double upSign, int riseAxis, double riseSign,
    int sides, const char* texture)
{
    upAxis = za::clamp(upAxis, 0, 2);
    if(riseAxis == upAxis || riseAxis < 0 || riseAxis > 2)
    {
        riseAxis = (upAxis + 1) % 3;
    }
    if(s == Shape::Box)
    {
        return box(lo, hi, texture);
    }
    Brush b;
    glm::dvec3 up{0.0};
    up[upAxis] = upSign >= 0.0 ? 1.0 : -1.0;
    const double base = upSign >= 0.0 ? lo[upAxis] : hi[upAxis], top = upSign >= 0.0 ? hi[upAxis] : lo[upAxis];
    const double height = hi[upAxis] - lo[upAxis];
    if(s == Shape::Wedge)
    {
        const int side = 3 - upAxis - riseAxis;
        glm::dvec3 fwd{0.0};
        fwd[riseAxis] = riseSign >= 0.0 ? 1.0 : -1.0;
        const double length = hi[riseAxis] - lo[riseAxis];
        const double low = riseSign >= 0.0 ? lo[riseAxis] : hi[riseAxis], high = riseSign >= 0.0 ? hi[riseAxis] : lo[riseAxis];
        b.faces.pushBack(planeFace(-up, -base * up[upAxis], texture));     // the bottom
        b.faces.pushBack(planeFace(fwd, high * fwd[riseAxis], texture));    // the high end
        for(const double sg : {1.0, -1.0})                                  // the sides
        {
            glm::dvec3 n{0.0};
            n[side] = sg;
            b.faces.pushBack(planeFace(n, sg > 0.0 ? hi[side] : -lo[side], texture));
        }
        // The slope: through the low end's bottom edge and the high end's top edge.
        const glm::dvec3 n = glm::normalize(up * length - fwd * height);
        glm::dvec3 p{0.0};
        p[upAxis] = base;
        p[riseAxis] = low;
        double d = glm::dot(n, p);
        glm::dvec3 nn = n;
        tidy(nn, d);
        b.faces.pushBack(planeFace(nn, d, texture));
        (void)top;
        return b;
    }
    // A cylinder's or pyramid's polygon: `sides` corners round a circle (a face flat to the first axis), stretched to fill
    // the box across the up axis.
    sides = za::clamp(sides, 3, 12);
    const int a1 = (upAxis + 1) % 3, a2 = (upAxis + 2) % 3;
    za::InPlaceVector<glm::dvec2, 12> poly;
    glm::dvec2 pmin{1e30}, pmax{-1e30};
    for(int k = 0; k < sides; k++)
    {
        const double ang = 6.283185307179586 * (static_cast<double>(k) + 0.5) / static_cast<double>(sides);
        const glm::dvec2 p{za::cos(ang), za::sin(ang)};
        poly.pushBack(p);
        pmin = glm::min(pmin, p);
        pmax = glm::max(pmax, p);
    }
    for(glm::dvec2& p : poly)
    {
        p = glm::dvec2{lo[a1], lo[a2]} + (p - pmin) / (pmax - pmin) * glm::dvec2{hi[a1] - lo[a1], hi[a2] - lo[a2]};
    }
    const auto at = [&](const glm::dvec2& p, double h) {
        glm::dvec3 q{0.0};
        q[a1] = p.x;
        q[a2] = p.y;
        q[upAxis] = h;
        return q;
    };
    glm::dvec2 mid{(lo[a1] + hi[a1]) * 0.5, (lo[a2] + hi[a2]) * 0.5};
    const glm::dvec3 inside = at(mid, (base + top) * 0.5);
    b.faces.pushBack(planeFace(-up, -base * up[upAxis], texture)); // the base
    if(s == Shape::Cylinder)
    {
        b.faces.pushBack(planeFace(up, top * up[upAxis], texture));
    }
    const glm::dvec3 apex = at(mid, top);
    for(int k = 0; k < sides; k++)
    {
        const glm::dvec3 p = at(poly[static_cast<za::SizeT>(k)], base), q = at(poly[static_cast<za::SizeT>((k + 1) % sides)], base);
        const glm::dvec3 r = s == Shape::Cylinder ? p + up : apex;
        glm::dvec3 n = glm::normalize(glm::cross(q - p, r - p));
        if(glm::dot(n, inside - p) > 0.0)
        {
            n = -n;
        }
        double d = glm::dot(n, p);
        tidy(n, d);
        b.faces.pushBack(planeFace(n, d, texture));
    }
    return b;
}

glm::dmat3 turnFromAngles(const glm::dvec3& angles)
{
    // Quake's AngleVectors in doubles (sines of whole quarter turns exact).
    const auto sc = [](double deg, double& s, double& c) {
        const double q = deg / 90.0;
        if(za::fabs(q - za::round(q)) < 1e-12)
        {
            const int k = ((static_cast<int>(za::round(q)) % 4) + 4) % 4;
            s = k == 1 ? 1.0 : k == 3 ? -1.0 : 0.0;
            c = k == 0 ? 1.0 : k == 2 ? -1.0 : 0.0;
            return;
        }
        s = za::sin(glm::radians(deg));
        c = za::cos(glm::radians(deg));
    };
    double sp, cp, sy, cy, sr, cr;
    sc(angles.x, sp, cp);
    sc(angles.y, sy, cy);
    sc(angles.z, sr, cr);
    const glm::dvec3 forward{cp * cy, cp * sy, -sp};
    const glm::dvec3 right{-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp};
    const glm::dvec3 up{cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp};
    return glm::dmat3{forward, -right, up};
}

void rotate(Brush& b, const glm::dmat3& r, const glm::dvec3& pivot, bool lockTexture)
{
    for(Face& f : b.faces)
    {
        glm::dvec3 n = r * f.normal;
        double d = glm::dot(n, pivot) + (f.dist - glm::dot(f.normal, pivot));
        tidy(n, d);
        if(lockTexture)
        {
            glm::dvec3 u = r * f.uAxis, v = r * f.vAxis;
            for(int k = 0; k < 3; k++)
            {
                u[k] = za::fabs(u[k]) < 1e-12 ? 0.0 : u[k];
                v[k] = za::fabs(v[k]) < 1e-12 ? 0.0 : v[k];
            }
            f.uOffset += (glm::dot(pivot, f.uAxis) - glm::dot(pivot, u)) / (f.scaleU != 0.0 ? f.scaleU : 1.0);
            f.vOffset += (glm::dot(pivot, f.vAxis) - glm::dot(pivot, v)) / (f.scaleV != 0.0 ? f.scaleV : 1.0);
            f.uAxis = u;
            f.vAxis = v;
            f.normal = n;
            f.dist = d;
        }
        else
        {
            f.normal = n;
            f.dist = d;
            const double su = f.scaleU, sv = f.scaleV;
            defaultAlignment(f);
            f.scaleU = su;
            f.scaleV = sv;
        }
    }
}

void scaleTo(Brush& b, const glm::dvec3& lo, const glm::dvec3& hi, const glm::dvec3& lo2, const glm::dvec3& hi2)
{
    // p' = lo2 + (p - lo) * k: a plane n.p <= d becomes (n / k).p' <= d - n.lo + (n / k).lo2.
    const glm::dvec3 k = (hi2 - lo2) / glm::max(hi - lo, glm::dvec3{1e-9});
    for(Face& f : b.faces)
    {
        const glm::dvec3 m = f.normal / glm::max(k, glm::dvec3{1e-9});
        const double len = glm::length(m);
        glm::dvec3 n = m / len;
        double d = (f.dist - glm::dot(f.normal, lo) + glm::dot(m, lo2)) / len;
        tidy(n, d);
        f.normal = n;
        f.dist = d;
    }
}

void forEachPolygon(const Brush& b, za::FunctionRef<void(int face, const glm::dvec3* points, int count)> f)
{
    za::Vector<Winding> polys;
    (void)polygons(b, polys);
    for(za::SizeT i = 0; i < polys.size(); i++)
    {
        if(polys[i].size() >= 3)
        {
            f(static_cast<int>(i), polys[i].data(), static_cast<int>(polys[i].size()));
        }
    }
}

bool axial(const Brush& b)
{
    for(const Face& f : b.faces)
    {
        if(za::fabs(f.normal.x) != 1.0 && za::fabs(f.normal.y) != 1.0 && za::fabs(f.normal.z) != 1.0)
        {
            return false;
        }
    }
    return true;
}

void toText(const Brush& b, za::String& out)
{
    out.clear();
    out += va("// qvrbrush id %d rev %u group %d kind solid\n{\n", b.id, b.rev, b.group);
    za::Vector<Winding> polys;
    (void)polygons(b, polys);
    for(za::SizeT i = 0; i < b.faces.size(); i++)
    {
        const Face& f = b.faces[i];
        glm::dvec3 p[3];
        facePoints(f, polys[i], p);
        for(const glm::dvec3& q : p)
        {
            out += "( ";
            for(int k = 0; k < 3; k++)
            {
                appendNumber(out, q[k]);
                out += ' ';
            }
            out += ") ";
        }
        out += f.texture[0] ? f.texture : "base";
        const glm::dvec4 axes[2] = {glm::dvec4{f.uAxis, f.uOffset}, glm::dvec4{f.vAxis, f.vOffset}};
        for(const glm::dvec4& a : axes)
        {
            out += " [ ";
            for(int k = 0; k < 4; k++)
            {
                appendNumber(out, a[k]);
                out += ' ';
            }
            out += ']';
        }
        out += ' ';
        appendNumber(out, f.rotation);
        out += ' ';
        appendNumber(out, f.scaleU);
        out += ' ';
        appendNumber(out, f.scaleV);
        out += '\n';
    }
    out += "}\n";
}

bool fromText(za::StringView text, Brush& out, za::String& error)
{
    out = Brush{};
    // The header comment, if any: its id, revision and group.
    const char* header = "// qvrbrush "; // (not a brush file's "// qvrbrushes")
    for(za::SizeT i = 0; i + strlen(header) <= text.size(); i++)
    {
        if(!strncmp(text.data() + i, header, strlen(header)))
        {
            char line[128];
            za::SizeT n = 0;
            for(za::SizeT k = i; k < text.size() && text[k] != '\n' && n + 1 < sizeof(line); k++)
            {
                line[n++] = text[k];
            }
            line[n] = '\0';
            unsigned rev = 0;
            int id = 0, group = 0;
            if(sscanf(line, "// qvrbrush id %d rev %u group %d", &id, &rev, &group) >= 1)
            {
                out.id = id;
                out.rev = rev;
                out.group = group;
            }
            break;
        }
    }
    Tokens t{text};
    if(!t.expect("{"))
    {
        error = "no {";
        return false;
    }
    for(;;)
    {
        if(!t.next())
        {
            error = "no }";
            return false;
        }
        if(!strcmp(t.token, "}"))
        {
            break;
        }
        if(strcmp(t.token, "("))
        {
            error = va("a face starts with %s, not (", t.token);
            return false;
        }
        glm::dvec3 p[3];
        for(int k = 0; k < 3; k++)
        {
            if((k > 0 && !t.expect("(")) || !t.number(p[k].x) || !t.number(p[k].y) || !t.number(p[k].z) || !t.expect(")"))
            {
                error = va("face %d: a point is not ( x y z )", static_cast<int>(out.faces.size()));
                return false;
            }
        }
        Face f;
        const glm::dvec3 n = glm::cross(p[0] - p[1], p[2] - p[1]);
        if(glm::length(n) < 1e-9)
        {
            error = va("face %d: its points are on one line", static_cast<int>(out.faces.size()));
            return false;
        }
        f.normal = glm::normalize(n);
        f.dist = glm::dot(p[0], f.normal);
        // (snapped: a plane of whole numbers read back exactly)
        for(int k = 0; k < 3; k++)
        {
            if(za::fabs(f.normal[k] - za::round(f.normal[k])) < 1e-12)
            {
                f.normal[k] = za::round(f.normal[k]) + 0.0; // (no -0)
            }
        }
        if(za::fabs(f.dist - za::round(f.dist)) < 1e-9)
        {
            f.dist = za::round(f.dist);
        }
        if(!t.next())
        {
            error = "a face without a texture";
            return false;
        }
        q_strlcpy(f.texture, t.token, sizeof(f.texture));
        defaultAlignment(f);
        // Valve 220 ([ u ] [ v ] rotation scales) or Quake's (offsets, rotation, scales).
        const za::SizeT mark = t.at;
        if(t.next() && !strcmp(t.token, "["))
        {
            glm::dvec4 axes[2];
            for(int a = 0; a < 2; a++)
            {
                if((a > 0 && !t.expect("[")) || !t.number(axes[a].x) || !t.number(axes[a].y) || !t.number(axes[a].z) ||
                    !t.number(axes[a].w) || !t.expect("]"))
                {
                    error = va("face %d: its axes are not [ x y z offset ]", static_cast<int>(out.faces.size()));
                    return false;
                }
            }
            f.uAxis = glm::dvec3{axes[0]};
            f.uOffset = axes[0].w;
            f.vAxis = glm::dvec3{axes[1]};
            f.vOffset = axes[1].w;
            if(!t.number(f.rotation) || !t.number(f.scaleU) || !t.number(f.scaleV))
            {
                error = va("face %d: no rotation and scales", static_cast<int>(out.faces.size()));
                return false;
            }
        }
        else
        {
            t.at = mark;
            double shift[2], rot, scale[2];
            if(!t.number(shift[0]) || !t.number(shift[1]) || !t.number(rot) || !t.number(scale[0]) || !t.number(scale[1]))
            {
                error = va("face %d: no offsets, rotation and scales", static_cast<int>(out.faces.size()));
                return false;
            }
            // qbsp's: the default axes turned by the rotation about the plane's axis, scaled.
            glm::dvec3 vecs[2] = {f.uAxis, f.vAxis};
            const double ang = glm::radians(rot);
            double sinv = za::sin(ang), cosv = za::cos(ang);
            if(rot == 0.0) { sinv = 0.0; cosv = 1.0; }
            else if(rot == 90.0) { sinv = 1.0; cosv = 0.0; }
            else if(rot == 180.0) { sinv = 0.0; cosv = -1.0; }
            else if(rot == 270.0) { sinv = -1.0; cosv = 0.0; }
            const int sv = vecs[0].x != 0.0 ? 0 : vecs[0].y != 0.0 ? 1 : 2;
            const int tv = vecs[1].x != 0.0 ? 0 : vecs[1].y != 0.0 ? 1 : 2;
            for(glm::dvec3& v : vecs)
            {
                const double ns = cosv * v[sv] - sinv * v[tv];
                const double nt = sinv * v[sv] + cosv * v[tv];
                v[sv] = ns;
                v[tv] = nt;
            }
            f.uAxis = vecs[0];
            f.vAxis = vecs[1];
            f.uOffset = shift[0];
            f.vOffset = shift[1];
            f.rotation = rot;
            f.scaleU = scale[0] != 0.0 ? scale[0] : 1.0;
            f.scaleV = scale[1] != 0.0 ? scale[1] : 1.0;
        }
        if(static_cast<int>(out.faces.size()) >= maxFaces)
        {
            error = "more faces than a brush may have";
            return false;
        }
        out.faces.pushBack(f);
    }
    if(out.faces.size() < 4)
    {
        error = "fewer than four faces";
        return false;
    }
    return true;
}
} // namespace qvr::brushes
