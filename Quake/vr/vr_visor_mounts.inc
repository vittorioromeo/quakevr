// vr_visor_mounts.inc -- part of vr_visor.cpp: the panels' mounts on the screen's edges and the visor's lens.
//
// Each panel sits in a mount: a flat dark translucent face with a frame line in the screens' green, whose outer sides run
// off the screen's edges (the corners' mounts with an angled strut back to the side, the top middle one (the log) a
// trapezoid widening into the top edge, the side ones narrowing towards the middle), laid out on a flat layer in the view's
// pixels, left and right exactly mirrored. The sway moves the whole layer (the helmet). Then the layer goes through the
// visor's zoom about the middle (vr_flat_visor_zoom) and its lens (vr_flat_visor_warp): a point seen at p (the view's
// -1..1) shows what the layer has at q = p (1 + k (|p|^2 - 1)): k > 0 the edges show the layer beyond them, the middle
// a touch magnified (barrel); k < 0 the other way (pincushion); the mounts run far enough past the edges for either.
// Drawn by warping the vertices (q to p, solved along the ray), the faces' outlines and the contents' grids finely
// subdivided. The panels read as lit glass in metal mounts: a back plate a little behind and below the face that lags
// the sway (vr_flat_visor_depth; its _depth_* tweaks: opacity, colour, size, offset, lag, its own bevel), a frame lit
// from above (vr_flat_visor_bevel) with an inner highlight, a sheen down each whole face and a glint across it, the
// contents a little behind the glass (vr_flat_visor_sheen), glows outside and inside the frame (vr_flat_visor_glow_outer,
// _inner, _width), the frame vr_flat_visor_edge across.

constexpr float mountPad = 4.f;     // virtual pixels round a panel's contents
constexpr float mountChamfer = 8.f; // its inner corners cut
constexpr float mountInset = 7.f;   // the contents' distance from the screen's edges
constexpr float strutSlope = 0.55f; // the corner mounts' strut: down (or up) this much a pixel back to the side
constexpr float trapSlope = 0.45f;  // the top middle mount's sides: out this much a pixel towards the edge
constexpr float logGap = 12.f;      // at least this between the top middle mount and the corners' (virtual pixels, at the edge)

// Where each panel's mount stands: its anchor on the screen (x, y in shares of the view: 0 left/top, 1 right/bottom),
// which way its local x and y run into the screen (+1 right/down, -1 left/up), and its kind.
enum class MountKind
{
    Corner, // a corner's: the contents at (inset, inset) from the corner, a strut back to the side
    Side,   // the middle of a side: the contents centred on the anchor's y
    Middle, // the middle of the top (or bottom) edge: the contents centred on the anchor's x
};
struct MountPlace
{
    glm::vec2 anchor;
    glm::vec2 dir;
    MountKind kind;
};
constexpr MountPlace mountPlaces[VisorPanelCount] = {
    {{0.f, 0.f}, {1.f, 1.f}, MountKind::Corner},   // vitals: top left
    {{1.f, 0.f}, {-1.f, 1.f}, MountKind::Corner},  // status: top right
    {{0.f, 1.f}, {1.f, -1.f}, MountKind::Corner},  // the left hand: bottom left
    {{1.f, 1.f}, {-1.f, -1.f}, MountKind::Corner}, // the right hand: bottom right
    {{0.5f, 0.f}, {1.f, 1.f}, MountKind::Middle},  // the log: the top's middle (vr_flat_visor_log_width, _log_out)
    {{0.f, 0.5f}, {1.f, 1.f}, MountKind::Side},    // keys and sigils: the left side's middle
    {{1.f, 0.5f}, {-1.f, 1.f}, MountKind::Side},   // powerups: the right side's middle
};

// The layer for this frame: the view's size, the virtual pixel's size in pixels, the sway (pixels), the lens's strength,
// the zoom about the view's middle (vr_flat_visor_zoom), a pixel at 1080p in the view's pixels.
struct Layer
{
    float w{0.f}, h{0.f};
    float unit{1.f};
    glm::vec2 shift{0.f};
    float k{0.f};
    float zoom{1.f};
    float px1080{1.f};
    glm::vec2 overhang{0.f}; // how far (virtual pixels) the mounts run past the edges, across and down
    float sideOut{1.f};      // the side mounts' reach in from the edges (vr_flat_visor_side_out)
    float logScale{1.f};     // the top middle mount's size (vr_flat_visor_log_width, shrunk to fit between the corners')
    float logOut{1.f};       // and its gap to the top edge (vr_flat_visor_log_out)
};

// The lens: the layer's point a screen point shows (both in the view's -1..1, y up), and back (solved along the ray: the
// lens is radial, |q| = f(r) = r (1 + k (r^2 - 1)). For k > 0 f always grows (Newton); for k < 0 it grows up to
// r* = sqrt((1 - k) / (-3 k)), past the screen's corners (r* >= sqrt 2) for k >= -0.2 (the clamp): bisection below it,
// a straight continuation past it (points off the screen stay off it).
[[nodiscard]] glm::vec2 lensSample(const glm::vec2& p, float k)
{
    return p * (1.f + k * (glm::dot(p, p) - 1.f));
}

[[nodiscard]] glm::vec2 lensShow(const glm::vec2& q, float k)
{
    const float len = glm::length(q);
    if(k == 0.f || len < 1e-6f)
    {
        return q;
    }
    const auto f = [k](float r) { return r * (1.f + k * (r * r - 1.f)); };
    float r = len;
    if(k > 0.f)
    {
        for(int i = 0; i < 6; i++) // Newton
        {
            r -= (f(r) - len) / (1.f + k * (3.f * r * r - 1.f));
        }
    }
    else
    {
        const float top = std::sqrt((1.f - k) / (-3.f * k));
        const float fTop = f(top);
        if(len >= fTop)
        {
            r = top + (len - fTop);
        }
        else
        {
            float lo = 0.f, hi = top;
            for(int i = 0; i < 24; i++)
            {
                const float mid = (lo + hi) * 0.5f;
                (f(mid) < len ? lo : hi) = mid;
            }
            r = (lo + hi) * 0.5f;
        }
    }
    return q * (r / len);
}

// Layer pixels (y down) and the view's -1..1 (y up).
[[nodiscard]] glm::vec2 toNdc(const Layer& L, const glm::vec2& px)
{
    return {px.x / L.w * 2.f - 1.f, 1.f - px.y / L.h * 2.f};
}
[[nodiscard]] glm::vec2 toPx(const Layer& L, const glm::vec2& ndc)
{
    return {(ndc.x + 1.f) * 0.5f * L.w, (1.f - ndc.y) * 0.5f * L.h};
}

// A layer pixel as drawn: zoomed about the middle, through the lens, as a view pixel (y down).
[[nodiscard]] glm::vec2 shownPx(const Layer& L, const glm::vec2& px)
{
    const glm::vec2 mid{L.w * 0.5f, L.h * 0.5f};
    return toPx(L, lensShow(toNdc(L, mid + (px - mid) * L.zoom), L.k));
}
[[nodiscard]] glm::vec3 clip(const Layer& L, const glm::vec2& viewPx)
{
    return {toNdc(L, viewPx), 0.5f};
}

// Panel `p`'s local point (virtual pixels: x, y into the screen from its anchor) as a layer pixel: the sway in it times
// `parallax` (1 the face; under 1 deeper: it lags the helmet's move a little), and `nudge` pixels more.
[[nodiscard]] glm::vec2 localToPx(const Layer& L, int p, const glm::vec2& local, float parallax = 1.f,
    const glm::vec2& nudge = glm::vec2{0.f})
{
    const MountPlace& m = mountPlaces[p];
    return glm::vec2{m.anchor.x * L.w, m.anchor.y * L.h} + m.dir * local * L.unit + L.shift * parallax + nudge;
}

// A mount's gap to the edge for a reach `out` (vr_flat_visor_side_out, _log_out; virtual pixels): mountInset times it,
// at least 1.5 at 0 and over; under 0 the contents pulled back past the edge, by mountInset a step (sliding off the
// screen: allowed). Continuous through 0.
[[nodiscard]] float reachInset(float out)
{
    return out >= 0.f ? za::max(1.5f, mountInset * out) : 1.5f + mountInset * out;
}

// Its contents' rectangle (local) and its mount's outline (local, from the far off-screen corner: a fan from its first
// point covers it).
struct Mount
{
    glm::vec2 lo{0.f}, hi{0.f}; // the contents, local
    za::Array<glm::vec2, 8> outline{};
    int points{0};
};

[[nodiscard]] Mount mountOf(const Layer& L, int p, const glm::vec2& size)
{
    Mount m;
    const float W = size.x, H = size.y, c = mountChamfer, ex = L.overhang.x, ey = L.overhang.y;
    const auto add = [&](float x, float y) { m.outline[m.points++] = {x, y}; };
    switch(mountPlaces[p].kind)
    {
        case MountKind::Corner:
        {
            m.lo = {mountInset, mountInset};
            m.hi = m.lo + glm::vec2{W, H};
            const float R = m.hi.x + mountPad, B = m.hi.y + mountPad, sx = m.lo.x + W * 0.15f;
            add(-ex, -ey);
            add(R, -ey);
            add(R, B - c);
            add(R - c, B);
            add(sx, B);
            add(-ex, B + (sx + ex) * strutSlope);
            break;
        }
        case MountKind::Side:
        {
            // (vr_flat_visor_side_out: the gap to the edge and the inner padding scaled, the chamfer clear of the contents;
            // under 0.2 the padding and the chamfer stay at 0.2's, only the gap goes on shrinking, then past the edge)
            const float inset = reachInset(L.sideOut), pad = mountPad * za::max(0.2f, L.sideOut);
            const float cut = mountChamfer * za::clamp(L.sideOut, 0.2f, 1.f);
            m.lo = {inset, -H * 0.5f};
            m.hi = {inset + W, H * 0.5f};
            const float R = m.hi.x + pad, T = H * 0.5f + mountPad;
            add(-ex, -T - ex * 0.5f);
            add(R - cut, -T);
            add(R, -T + cut);
            add(R, T - cut);
            add(R - cut, T);
            add(-ex, T + ex * 0.5f);
            break;
        }
        case MountKind::Middle:
        {
            // (vr_flat_visor_log_out: its gap to the edge scaled; under 0 up past it)
            const float inset = reachInset(L.logOut);
            m.lo = {-W * 0.5f, inset};
            m.hi = {W * 0.5f, inset + H};
            const float X = W * 0.5f + mountPad, T = m.hi.y + mountPad;
            add(-X - (T + ey) * trapSlope, -ey);
            add(-X, T - c);
            add(-X + c, T);
            add(X - c, T);
            add(X, T - c);
            add(X + (T + ey) * trapSlope, -ey);
            break;
        }
    }
    return m;
}

// The top middle mount's size: vr_flat_visor_log_width, no larger than leaves it `logGap` clear of the top corners' mounts
// where it meets the edge (its half width there: the contents' half, the padding, the trapezoid's flare over its depth).
// `logSize`: its image's (virtual pixels); `cornerReach`: the wider top corner mount's reach in from its side.
[[nodiscard]] float logFit(const Layer& L, const glm::vec2& logSize, float cornerReach, float wanted)
{
    const float inset = za::max(0.f, reachInset(L.logOut)); // (pulled up past the edge: no wider than at the edge)
    const float room = L.w / L.unit * 0.5f - cornerReach - logGap - mountPad - (inset + mountPad) * trapSlope;
    const float most = room / za::max(1.f, logSize.x * 0.5f + logSize.y * trapSlope);
    return za::max(0.2f, za::min(wanted, most));
}

// Whether a local point is inside the outline (crossings).
[[nodiscard]] bool inside(const Mount& m, const glm::vec2& pt)
{
    bool in = false;
    for(int i = 0, j = m.points - 1; i < m.points; j = i++)
    {
        const glm::vec2 a = m.outline[i], b = m.outline[j];
        if((a.y > pt.y) != (b.y > pt.y) && pt.x < (b.x - a.x) * (pt.y - a.y) / (b.y - a.y) + a.x)
        {
            in = !in;
        }
    }
    return in;
}

// The edge contact (vr_flat_visor_report, the test): the screen's points the mount must cover where it meets the edges --
// a corner's corner and a point on each of its two edges, a side's middle and a point either side of it on its edge, the
// top's middle and a point either side -- each looked up through the lens, the zoom and the sway into the layer: all
// inside its face (and its back plate's: the face is what meets the edge).
[[nodiscard]] bool edgeContact(const Layer& L, int p, const Mount& m)
{
    const MountPlace& place = mountPlaces[p];
    const glm::vec2 a{place.anchor.x * L.w, place.anchor.y * L.h};
    const glm::vec2 span = (m.hi + glm::vec2{mountPad}) * L.unit * 0.25f;
    za::Array<glm::vec2, 3> pts{};
    switch(place.kind)
    {
        case MountKind::Corner: pts = {a, a + glm::vec2{place.dir.x * span.x, 0.f}, a + glm::vec2{0.f, place.dir.y * span.y}}; break;
        case MountKind::Side: pts = {a, a + glm::vec2{0.f, span.y}, a - glm::vec2{0.f, span.y}}; break;
        case MountKind::Middle: pts = {a, a + glm::vec2{span.x, 0.f}, a - glm::vec2{span.x, 0.f}}; break;
    }
    const glm::vec2 mid{L.w * 0.5f, L.h * 0.5f};
    for(const glm::vec2& screen : pts)
    {
        const glm::vec2 zoomed = toPx(L, lensSample(toNdc(L, screen), L.k));
        const glm::vec2 layer = mid + (zoomed - mid) / L.zoom - L.shift;
        const glm::vec2 local = (layer - a) / L.unit * place.dir;
        if(!inside(m, local))
        {
            return false;
        }
    }
    return true;
}

// The outline as drawn: subdivided every 8 layer pixels, through the zoom and the lens (view pixels), with each point's
// outward normal (the mitred mean of its two pieces').
struct Ring
{
    za::Array<glm::vec2, 512> at{};
    za::Array<glm::vec2, 512> out{};
    int n{0};
};

void ringOf(const Layer& L, int p, const Mount& m, float parallax, const glm::vec2& nudge, Ring& r)
{
    r.n = 0;
    for(int i = 0; i < m.points && r.n < 500; i++)
    {
        const glm::vec2 a = localToPx(L, p, m.outline[i], parallax, nudge);
        const glm::vec2 b = localToPx(L, p, m.outline[(i + 1) % m.points], parallax, nudge);
        const int pieces = za::clamp(static_cast<int>(glm::length(b - a) / 8.f), 1, 80);
        for(int s = 0; s < pieces && r.n < 500; s++)
        {
            r.at[r.n++] = shownPx(L, glm::mix(a, b, static_cast<float>(s) / static_cast<float>(pieces)));
        }
    }
    // Its winding (view pixels, y down): the normals turned outwards.
    float area = 0.f;
    for(int i = 0; i < r.n; i++)
    {
        const glm::vec2 a = r.at[i], b = r.at[(i + 1) % r.n];
        area += a.x * b.y - b.x * a.y;
    }
    const float side = area > 0.f ? -1.f : 1.f;
    const auto normal = [&](int i) {
        const glm::vec2 d = r.at[(i + 1) % r.n] - r.at[i];
        const float len = glm::length(d);
        return len > 1e-5f ? glm::vec2{d.y, -d.x} * (side / len) : glm::vec2{0.f};
    };
    for(int i = 0; i < r.n; i++)
    {
        const glm::vec2 n0 = normal((i + r.n - 1) % r.n), n1 = normal(i);
        const glm::vec2 sum = n0 + n1;
        const float len = glm::length(sum);
        if(len < 1e-4f)
        {
            r.out[i] = n1;
            continue;
        }
        const glm::vec2 mean = sum / len;
        r.out[i] = mean / za::max(0.5f, glm::dot(mean, n1)); // (mitred, at most twice)
    }
}

// A band along the ring between two offsets (view pixels, outwards positive) with a colour at each (premultiplied),
// `shade` brightening the pieces facing up (the light from above) and darkening those facing down by that much
// (`shadeAlpha`: the whole premultiplied colour scaled, never past opaque, so a dark band's strength follows it too).
void band(const Layer& L, const Ring& r, float off0, float off1, const glm::vec4& c0, const glm::vec4& c1, float shade,
    za::Vector<gfx::Vertex>& out, bool shadeAlpha = false)
{
    const auto scaled = [shadeAlpha](const glm::vec4& c, float l) {
        if(!shadeAlpha)
        {
            return c * glm::vec4{glm::vec3{l}, 1.f};
        }
        const float k = za::max(0.f, l);
        return c * (c.a * k > 1.f ? 1.f / c.a : k);
    };
    for(int i = 0; i < r.n; i++)
    {
        const int j = (i + 1) % r.n;
        const auto lit = [&](int k) {
            const float up = -r.out[k].y / za::max(1e-4f, glm::length(r.out[k])); // (y down: up is -y)
            return 1.f + shade * up;
        };
        const float li = lit(i), lj = lit(j);
        const gfx::Vertex a0{clip(L, r.at[i] + r.out[i] * off0), {0.f, 0.f}, scaled(c0, li)};
        const gfx::Vertex a1{clip(L, r.at[i] + r.out[i] * off1), {0.f, 0.f}, scaled(c1, li)};
        const gfx::Vertex b0{clip(L, r.at[j] + r.out[j] * off0), {0.f, 0.f}, scaled(c0, lj)};
        const gfx::Vertex b1{clip(L, r.at[j] + r.out[j] * off1), {0.f, 0.f}, scaled(c1, lj)};
        out.pushBack(a0);
        out.pushBack(b0);
        out.pushBack(b1);
        out.pushBack(a0);
        out.pushBack(b1);
        out.pushBack(a1);
    }
}

// A line `width` pixels across centred on the ring, its edges feathered over a pixel (antialiased whatever the
// supersampling), shaded by `shade`.
void line(const Layer& L, const Ring& r, float offset, float width, const glm::vec4& c, float shade,
    za::Vector<gfx::Vertex>& out)
{
    const float half = za::max(0.25f, width * 0.5f - 0.5f), feather = 1.f;
    const glm::vec4 clear{0.f};
    band(L, r, offset - half - feather, offset - half, clear, c, shade, out);
    band(L, r, offset - half, offset + half, c, c, shade, out);
    band(L, r, offset + half, offset + half + feather, c, clear, shade, out);
}

// The ring pushed out by `by` pixels all round (along its mitred normals: the back plate grown, vr_flat_visor_depth_size).
void grow(Ring& r, float by)
{
    for(int i = 0; i < r.n; i++)
    {
        r.at[i] += r.out[i] * by;
    }
}

// What of the ring is on the screen: its bounding box clamped to the view (x0, y0, x1, y1; view pixels).
[[nodiscard]] glm::vec4 shownBox(const Layer& L, const Ring& r)
{
    glm::vec2 lo{1e9f}, hi{-1e9f};
    for(int i = 0; i < r.n; i++)
    {
        lo = glm::min(lo, r.at[i]);
        hi = glm::max(hi, r.at[i]);
    }
    lo = glm::clamp(lo, glm::vec2{0.f}, glm::vec2{L.w, L.h});
    hi = glm::clamp(hi, glm::vec2{0.f}, glm::vec2{L.w, L.h});
    return {lo, glm::max(hi, lo + glm::vec2{1.f})};
}

// The face: a fan from the ring's first point (far off the screen) over the rest, premultiplied; a sheen brightening it
// towards its top (the light from above) by `sheen`, `topBottom` the face's top and bottom as seen.
void face(const Layer& L, const Ring& r, const glm::vec4& c, const glm::vec2& topBottom, float sheen,
    za::Vector<gfx::Vertex>& out)
{
    const auto colour = [&](const glm::vec2& px) {
        const float t = glm::clamp((px.y - topBottom.x) / za::max(1.f, topBottom.y - topBottom.x), 0.f, 1.f);
        return glm::vec4{glm::vec3{c} * (1.f + sheen * 0.9f * (1.f - t)) + glm::vec3{0.025f * sheen * (1.f - t)} * c.a, c.a};
    };
    const gfx::Vertex apex{clip(L, r.at[0]), {0.f, 0.f}, colour(r.at[0])};
    for(int i = 1; i + 1 < r.n; i++)
    {
        out.pushBack(apex);
        out.pushBack({clip(L, r.at[i]), {0.f, 0.f}, colour(r.at[i])});
        out.pushBack({clip(L, r.at[i + 1]), {0.f, 0.f}, colour(r.at[i + 1])});
    }
}

// The glass's glint over the whole face (clipped to its outline: the face's own fan): a soft diagonal band
// (Shade::SoftEdge, its falloff across uv.y, exact over any triangles: it is linear in the position) from the top left
// of what is seen of it, `box`, a third of the way down to its bottom right.
void glint(const Layer& L, const Ring& r, const glm::vec4& box, float strength, za::Vector<gfx::Vertex>& out)
{
    const glm::vec2 size{box.z - box.x, box.w - box.y};
    const auto vertex = [&](const glm::vec2& px) {
        const glm::vec2 t = (px - glm::vec2{box.x, box.y}) / size; // 0 the top left .. 1 the bottom right
        return gfx::Vertex{clip(L, px), {0.f, ((t.x + t.y) * 0.5f - 0.3f) * 5.f}, glm::vec4{1.f, 1.f, 1.f, 0.05f * strength}};
    };
    const gfx::Vertex apex = vertex(r.at[0]);
    for(int i = 1; i + 1 < r.n; i++)
    {
        out.pushBack(apex);
        out.pushBack(vertex(r.at[i]));
        out.pushBack(vertex(r.at[i + 1]));
    }
}

// The contents: the panel's image over its rectangle as a grid through the zoom and the lens, as a small CRT, its face
// clear (premultiplied out of Shade::Screen's inkAlpha). `rect`: where it landed (view pixels).
void drawContents(const Layer& L, int p, const Mount& m, const gadget::VisorImage& img, const gfx::State& state,
    const glm::vec4& phosphor, float parallax, glm::vec4& rect)
{
    // The rectangle in layer pixels (the image never mirrored: its left at the screen's left).
    const glm::vec2 a = localToPx(L, p, m.lo, parallax), b = localToPx(L, p, m.hi, parallax);
    const glm::vec2 lo = glm::min(a, b), hi = glm::max(a, b);
    constexpr int gx = 12, gy = 6;
    za::Array<gfx::Vertex, (gx + 1) * (gy + 1)> grid{};
    glm::vec2 drawnLo{1e9f}, drawnHi{-1e9f};
    for(int j = 0; j <= gy; j++)
    {
        for(int i = 0; i <= gx; i++)
        {
            const glm::vec2 uv{static_cast<float>(i) / gx, static_cast<float>(j) / gy};
            const glm::vec2 seen = shownPx(L, {glm::mix(lo.x, hi.x, uv.x), glm::mix(hi.y, lo.y, uv.y)}); // (v up the image)
            grid[j * (gx + 1) + i] = {clip(L, seen), uv, phosphor};
            drawnLo = glm::min(drawnLo, seen);
            drawnHi = glm::max(drawnHi, seen);
        }
    }
    za::Array<gfx::Vertex, gx * gy * 6> tris{};
    const auto quads = [&](auto vertex) {
        int t = 0;
        for(int j = 0; j < gy; j++)
        {
            for(int i = 0; i < gx; i++)
            {
                const gfx::Vertex v00 = vertex(i, j), v10 = vertex(i + 1, j), v11 = vertex(i + 1, j + 1), v01 = vertex(i, j + 1);
                tris[t++] = v00, tris[t++] = v10, tris[t++] = v11, tris[t++] = v00, tris[t++] = v11, tris[t++] = v01;
            }
        }
        return t;
    };
    const int count = quads([&](int i, int j) { return grid[j * (gx + 1) + i]; });
    gfx::draw({tris.data(), static_cast<za::SizeT>(count)}, glm::mat4{1.f}, state, img.texture);
    rect = {drawnLo, drawnHi};
}
