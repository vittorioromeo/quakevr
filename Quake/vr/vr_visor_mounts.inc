// vr_visor_mounts.inc -- part of vr_visor.cpp: the panels' mounts on the screen's edges and the visor's lens.
//
// Each panel sits in a mount: a flat dark translucent face with a frame line in the screens' green, whose outer sides run
// off the screen's edges (the corners' mounts with an angled strut back to the side, the top middle one (the log) a
// trapezoid widening into the top edge, the side ones narrowing towards the middle; the centre print's popup the one
// closed panel, under the log's), laid out on a flat layer in the view's
// pixels, left and right exactly mirrored. The sway moves the whole layer (the helmet). Then the layer goes through the
// visor's zoom about the middle (vr_flat_visor_zoom) and its lens (vr_flat_visor_warp): a point seen at p (the view's
// -1..1) shows what the layer has at q = p (1 + k (|p|^2 - 1)): k > 0 the edges show the layer beyond them, the middle
// a touch magnified (barrel); k < 0 the other way (pincushion); the mounts run far enough past the edges for either.
// The outlines are laid out as seen (q to p, solved along the ray, finely subdivided: the frame lines and glows their
// widths in the view's pixels), then every vertex taken back into the layer (lensSample, the explicit way) at rest
// (no sway): each panel's depths (back plate, face, contents, what lies over them, the glass's shapes) drawn unwarped
// into an atlas when what they show changes; the composite (vr_visor.cpp) applies the sway, each depth its own share,
// the zoom and the lens per pixel. The panels read as lit glass in metal mounts: a back plate a little behind and below the face that lags
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
    Popup,  // a closed panel off the edges: the contents centred on the anchor's x, Layer::centerTop down from it
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
    {{0.5f, 0.f}, {1.f, 1.f}, MountKind::Popup},   // the centre print: under the log's mount (vr_flat_visor_center_gap)
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
    float sideScale{1.f};    // and their size (vr_flat_visor_side_scale, shrunk to fit between the corners')
    float logScale{1.f};     // the top middle mount's size (vr_flat_visor_log_width, shrunk to fit between the corners')
    float logOut{1.f};       // and its gap to the top edge (vr_flat_visor_log_out)
    float centerTop{0.f};    // the centre print's popup: its contents' top (virtual pixels down from the top edge)
    float centerScale{1.f};  // and its size (1, shrunk to fit the view's width)
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
            // Newton: f rises and is concave below r*, so from either side the steps land below the root and climb to
            // it (a step at most to r*).
            for(int i = 0; i < 6; i++)
            {
                r = za::clamp(r - (f(r) - len) / za::max(1e-4f, 1.f + k * (3.f * r * r - 1.f)), 0.f, top);
            }
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

// And back: the layer pixel (at rest) a view pixel shows -- the composite's per-pixel map (vr_visor.cpp layerFs's
// layerOf). Exact where the lens can be undone: below lensReach (k < 0: r*, past the screen's corners), everywhere for
// k >= 0.
[[nodiscard]] glm::vec2 unshownPx(const Layer& L, const glm::vec2& viewPx)
{
    const glm::vec2 mid{L.w * 0.5f, L.h * 0.5f};
    return mid + (toPx(L, lensSample(toNdc(L, viewPx), L.k)) - mid) / L.zoom;
}
[[nodiscard]] float lensReach(float k)
{
    return k < 0.f ? std::sqrt((1.f - k) / (-3.f * k)) : 1e9f;
}

// The sway as layer pixels at the view's horizontal tan half-angle `tx` (degrees to pixels; never more than most of the
// contents' inset).
[[nodiscard]] glm::vec2 swayShift(const Layer& L, const glm::vec2& swayDeg, float tx)
{
    const glm::vec2 sway{glm::tan(glm::radians(swayDeg.x)), glm::tan(glm::radians(swayDeg.y))};
    const float most = 0.8f * mountInset * L.unit;
    return glm::clamp(glm::vec2{sway.x, -sway.y} / tx * (L.w * 0.5f), glm::vec2{-most}, glm::vec2{most});
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

// A side mount's (panel `p`'s) screen edge as seen at rest: the local x (virtual pixels, + into the screen) the edge of
// the view shows over its rows (`halfH` either side of its middle), the innermost; under 0 where the zoom and the lens
// show the layer past its own edge (vr_flat_visor_zoom under 1, a pincushion lens).
constexpr float sideSwayRoom = 0.8f * mountInset; // (the sway's most: swayShift)
[[nodiscard]] float sideEdgeSeen(const Layer& L, int p, float halfH)
{
    const MountPlace& m = mountPlaces[p];
    const glm::vec2 a{m.anchor.x * L.w, m.anchor.y * L.h};
    float most = -1e9f;
    for(int i = -2; i <= 2; i++)
    {
        const glm::vec2 screen{a.x, a.y + static_cast<float>(i) * 0.5f * halfH * L.unit * L.zoom};
        most = za::max(most, (unshownPx(L, screen).x - a.x) * m.dir.x / L.unit);
    }
    return most;
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
            // The contents from the screen's edge as seen (the zoom and the lens show the layer past its own edge there:
            // an empty strip of face otherwise), with room for the sway. (vr_flat_visor_side_out: the gap to the edge
            // and the inner padding scaled, the chamfer clear of the contents; under 0.2 the padding and the chamfer stay
            // at 0.2's, only the gap goes on shrinking, then past the edge.)
            const float inset = sideEdgeSeen(L, p, H * 0.5f) + sideSwayRoom + reachInset(L.sideOut);
            const float pad = mountPad * za::max(0.2f, L.sideOut);
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
        case MountKind::Popup:
        {
            // A rectangle, every corner cut (convex: the face's fan from its first point covers it).
            m.lo = {-W * 0.5f, L.centerTop};
            m.hi = {W * 0.5f, L.centerTop + H};
            const float X = W * 0.5f + mountPad, T = m.lo.y - mountPad, B = m.hi.y + mountPad;
            add(-X, T + c);
            add(-X + c, T);
            add(X - c, T);
            add(X, T + c);
            add(X, B - c);
            add(X - c, B);
            add(-X + c, B);
            add(-X, B - c);
            break;
        }
    }
    return m;
}

// Where the log's mount ends (its frame's bottom: virtual pixels down from the top edge), shown or hidden.
[[nodiscard]] float logBottom(const Layer& L, const glm::vec2& logSize)
{
    return reachInset(L.logOut) + logSize.y * L.logScale + mountPad;
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
        case MountKind::Popup: return true; // (off the edges: nothing to meet)
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
// outward normal (the mitred mean of its two pieces'); each point's layer pixel (src) and where it was first seen (base:
// grow moves `at`).
struct Ring
{
    za::Array<glm::vec2, 512> at{};
    za::Array<glm::vec2, 512> out{};
    za::Array<glm::vec2, 512> src{};
    za::Array<glm::vec2, 512> base{};
    int n{0};
};

// A point seen near ring point i (view pixels) as a vertex of the layer (clip space over the whole layer at rest): its
// layer pixel, unshownPx; past lensReach (far off the screen, where the lens folds back) the ring point's own layer
// pixel moved as far as the point is from it.
[[nodiscard]] glm::vec3 layerAt(const Layer& L, const Ring& r, int i, const glm::vec2& seen)
{
    const glm::vec2 n = toNdc(L, seen);
    const float reach = lensReach(L.k) - 1e-3f;
    const glm::vec2 px = glm::dot(n, n) < reach * reach ? unshownPx(L, seen) : r.src[i] + (seen - r.base[i]);
    return clip(L, px);
}

void ringOf(const Layer& L, int p, const Mount& m, float parallax, const glm::vec2& nudge, Ring& r)
{
    r.n = 0;
    for(int i = 0; i < m.points && r.n < 500; i++)
    {
        const glm::vec2 a = localToPx(L, p, m.outline[i], parallax, nudge);
        const glm::vec2 b = localToPx(L, p, m.outline[(i + 1) % m.points], parallax, nudge);
        // (a piece seen wholly past one of the view's edges, well clear of it, drawn straight: nothing of it is seen)
        const glm::vec2 sa = shownPx(L, a), sb = shownPx(L, b);
        constexpr float clear = 160.f;
        const bool off = (sa.x < -clear && sb.x < -clear) || (sa.y < -clear && sb.y < -clear) ||
                         (sa.x > L.w + clear && sb.x > L.w + clear) || (sa.y > L.h + clear && sb.y > L.h + clear);
        const int pieces = off ? 1 : za::clamp(static_cast<int>(glm::length(b - a) / 8.f), 1, 80);
        for(int s = 0; s < pieces && r.n < 500; s++)
        {
            r.src[r.n] = glm::mix(a, b, static_cast<float>(s) / static_cast<float>(pieces));
            r.at[r.n] = r.base[r.n] = shownPx(L, r.src[r.n]);
            r.n++;
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
        const gfx::Vertex a0{layerAt(L, r, i, r.at[i] + r.out[i] * off0), {0.f, 0.f}, scaled(c0, li)};
        const gfx::Vertex a1{layerAt(L, r, i, r.at[i] + r.out[i] * off1), {0.f, 0.f}, scaled(c1, li)};
        const gfx::Vertex b0{layerAt(L, r, j, r.at[j] + r.out[j] * off0), {0.f, 0.f}, scaled(c0, lj)};
        const gfx::Vertex b1{layerAt(L, r, j, r.at[j] + r.out[j] * off1), {0.f, 0.f}, scaled(c1, lj)};
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
    const gfx::Vertex apex{layerAt(L, r, 0, r.at[0]), {0.f, 0.f}, colour(r.at[0])};
    for(int i = 1; i + 1 < r.n; i++)
    {
        out.pushBack(apex);
        out.pushBack({layerAt(L, r, i, r.at[i]), {0.f, 0.f}, colour(r.at[i])});
        out.pushBack({layerAt(L, r, i + 1, r.at[i + 1]), {0.f, 0.f}, colour(r.at[i + 1])});
    }
}

// The glass's glint over the whole face (clipped to its outline: the face's own fan): a soft diagonal band
// (Shade::SoftEdge, its falloff across uv.y, exact over any triangles: it is linear in the position) from the top left
// of what is seen of it, `box`, a third of the way down to its bottom right.
void glint(const Layer& L, const Ring& r, const glm::vec4& box, float strength, za::Vector<gfx::Vertex>& out)
{
    const glm::vec2 size{box.z - box.x, box.w - box.y};
    const auto vertex = [&](int i) {
        const glm::vec2 t = (r.at[i] - glm::vec2{box.x, box.y}) / size; // 0 the top left .. 1 the bottom right
        return gfx::Vertex{layerAt(L, r, i, r.at[i]), {0.f, ((t.x + t.y) * 0.5f - 0.3f) * 5.f}, glm::vec4{1.f, 1.f, 1.f, 0.05f * strength}};
    };
    const gfx::Vertex apex = vertex(0);
    for(int i = 1; i + 1 < r.n; i++)
    {
        out.pushBack(apex);
        out.pushBack(vertex(i));
        out.pushBack(vertex(i + 1));
    }
}

// The contents: the panel's image over its rectangle in the layer (at rest: the lens is the composite's), as a small CRT,
// its face clear (premultiplied out of Shade::Screen's inkAlpha), through `mvp` (the atlas's slot). `rect`: where it is
// seen at rest (view pixels).
void drawContents(const Layer& L, int p, const Mount& m, const gadget::VisorImage& img, const gfx::State& state,
    const glm::vec4& phosphor, const glm::mat4& mvp, glm::vec4& rect)
{
    // The rectangle in layer pixels (the image never mirrored: its left at the screen's left).
    const glm::vec2 a = localToPx(L, p, m.lo), b = localToPx(L, p, m.hi);
    const glm::vec2 lo = glm::min(a, b), hi = glm::max(a, b);
    glm::vec2 seenLo{1e9f}, seenHi{-1e9f};
    for(int j = 0; j <= 4; j++)
    {
        for(int i = 0; i <= 8; i++)
        {
            const glm::vec2 seen = shownPx(L, glm::mix(lo, hi, glm::vec2{static_cast<float>(i) / 8.f, static_cast<float>(j) / 4.f}));
            seenLo = glm::min(seenLo, seen);
            seenHi = glm::max(seenHi, seen);
        }
    }
    const auto vertex = [&](float u, float v) { // (v up the image)
        return gfx::Vertex{clip(L, {glm::mix(lo.x, hi.x, u), glm::mix(hi.y, lo.y, v)}), {u, v}, phosphor};
    };
    const gfx::Vertex quad[6] = {vertex(0.f, 0.f), vertex(1.f, 0.f), vertex(1.f, 1.f), vertex(0.f, 0.f), vertex(1.f, 1.f), vertex(0.f, 1.f)};
    gfx::draw(quad, mvp, state, img.texture);
    rect = {seenLo, seenHi};
}
