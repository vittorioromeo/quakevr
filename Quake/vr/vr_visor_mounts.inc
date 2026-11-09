// vr_visor_mounts.inc -- part of vr_visor.cpp: the panels' mounts on the screen's edges and the visor's lens.
//
// Each panel sits in a mount: a flat dark translucent face with a frame line in the screens' green, whose outer sides run
// off the screen's edges (the corners' mounts with an angled strut back to the side, the bottom middle one a trapezoid
// widening into the bottom edge, the side ones narrowing towards the middle), laid out on a flat layer in the view's
// pixels, left and right exactly mirrored. The sway moves the whole layer (the helmet). Then the layer goes through the
// visor's lens (vr_flat_visor_warp): a point seen at p (the view's -1..1) shows what the layer has at
// q = p (1 + k (|p|^2 - 1)): the screen's edges show the layer at or beyond them (so no mount ever comes off its edge),
// the middle a touch magnified, straight lines near the edges bowing round the middle like a curved glass. Drawn by
// warping the vertices (q to p, solved along the ray), the faces' outlines and the contents' grids finely subdivided.

constexpr float mountPad = 4.f;     // virtual pixels round a panel's contents
constexpr float mountChamfer = 8.f; // its inner corners cut
constexpr float mountInset = 7.f;   // the contents' distance from the screen's edges
constexpr float strutSlope = 0.55f; // the corner mounts' strut: down (or up) this much a pixel back to the side
constexpr float trapSlope = 0.45f;  // the bottom middle mount's sides: out this much a pixel down

// Where each panel's mount stands: its anchor on the screen (x, y in shares of the view: 0 left/top, 1 right/bottom),
// which way its local x and y run into the screen (+1 right/down, -1 left/up), and its kind.
enum class MountKind
{
    Corner, // a corner's: the contents at (inset, inset) from the corner, a strut back to the side
    Side,   // the middle of a side: the contents centred on the anchor's y
    Bottom, // the bottom's middle: the contents centred on the anchor's x
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
    {{0.5f, 1.f}, {1.f, -1.f}, MountKind::Bottom}, // the log: the bottom's middle
    {{0.f, 0.5f}, {1.f, 1.f}, MountKind::Side},    // keys and sigils: the left side's middle
    {{1.f, 0.5f}, {-1.f, 1.f}, MountKind::Side},   // powerups: the right side's middle
};

// The layer for this frame: the view's size, the virtual pixel's size in pixels, the sway (pixels), the lens's strength.
struct Layer
{
    float w{0.f}, h{0.f};
    float unit{1.f};
    glm::vec2 shift{0.f};
    float k{0.f};
    glm::vec2 overhang{0.f}; // how far (virtual pixels) the mounts run past the edges, across and down
};

// The lens: the layer's point a screen point shows (both in the view's -1..1, y up), and back (solved along the ray: the
// lens is radial, |q| = r (1 + k (r^2 - 1)), increasing for k < 0.5).
[[nodiscard]] glm::vec2 lensSample(const glm::vec2& p, float k)
{
    return p * (1.f + k * (glm::dot(p, p) - 1.f));
}

[[nodiscard]] glm::vec2 lensShow(const glm::vec2& q, float k)
{
    const float len = glm::length(q);
    if(k <= 0.f || len < 1e-6f)
    {
        return q;
    }
    float r = len;
    for(int i = 0; i < 6; i++) // Newton
    {
        const float f = r * (1.f + k * (r * r - 1.f)) - len;
        r -= f / (1.f + k * (3.f * r * r - 1.f));
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

// A layer pixel as drawn: through the lens, at depth halfway (clip space; no depth test).
[[nodiscard]] glm::vec3 shown(const Layer& L, const glm::vec2& px)
{
    return {lensShow(toNdc(L, px), L.k), 0.5f};
}

// Panel `p`'s local point (virtual pixels: x, y into the screen from its anchor) as a layer pixel, the sway in it.
[[nodiscard]] glm::vec2 localToPx(const Layer& L, int p, const glm::vec2& local)
{
    const MountPlace& m = mountPlaces[p];
    return glm::vec2{m.anchor.x * L.w, m.anchor.y * L.h} + m.dir * local * L.unit + L.shift;
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
            m.lo = {mountInset, -H * 0.5f};
            m.hi = {mountInset + W, H * 0.5f};
            const float R = m.hi.x + mountPad, T = H * 0.5f + mountPad;
            add(-ex, -T - ex * 0.5f);
            add(R - c, -T);
            add(R, -T + c);
            add(R, T - c);
            add(R - c, T);
            add(-ex, T + ex * 0.5f);
            break;
        }
        case MountKind::Bottom:
        {
            m.lo = {-W * 0.5f, mountInset};
            m.hi = {W * 0.5f, mountInset + H};
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
// bottom's middle and a point either side -- each looked up through the lens and the sway into the layer: all inside.
[[nodiscard]] bool edgeContact(const Layer& L, int p, const Mount& m)
{
    const MountPlace& place = mountPlaces[p];
    const glm::vec2 a{place.anchor.x * L.w, place.anchor.y * L.h};
    const glm::vec2 span = (m.hi + glm::vec2{mountPad}) * L.unit * 0.5f;
    za::Array<glm::vec2, 3> pts{};
    switch(place.kind)
    {
        case MountKind::Corner: pts = {a, a + glm::vec2{place.dir.x * span.x, 0.f}, a + glm::vec2{0.f, place.dir.y * span.y}}; break;
        case MountKind::Side: pts = {a, a + glm::vec2{0.f, span.y}, a - glm::vec2{0.f, span.y}}; break;
        case MountKind::Bottom: pts = {a, a + glm::vec2{span.x, 0.f}, a - glm::vec2{span.x, 0.f}}; break;
    }
    for(const glm::vec2& screen : pts)
    {
        const glm::vec2 layer = toPx(L, lensSample(toNdc(L, screen), L.k)) - L.shift;
        const glm::vec2 local = (layer - a) / L.unit * place.dir;
        if(!inside(m, local))
        {
            return false;
        }
    }
    return true;
}

// The mount's face (a fan from its far corner over its outline, finely subdivided so the lens bends it) and its frame
// line (`width` pixels), in clip space.
void drawMount(const Layer& L, int p, const Mount& m, const glm::vec4& face, const glm::vec4& line, float width)
{
    za::Array<glm::vec2, 512> ring{}; // the outline, subdivided, as layer pixels
    int n = 0;
    for(int i = 0; i < m.points && n < 500; i++)
    {
        const glm::vec2 a = localToPx(L, p, m.outline[i]), b = localToPx(L, p, m.outline[(i + 1) % m.points]);
        const int pieces = za::clamp(static_cast<int>(glm::length(b - a) / 8.f), 1, 80);
        for(int s = 0; s < pieces && n < 500; s++)
        {
            ring[n++] = glm::mix(a, b, static_cast<float>(s) / static_cast<float>(pieces));
        }
    }
    za::Array<gfx::Vertex, 512 * 3> tris{};
    int t = 0;
    const glm::vec3 apex = shown(L, ring[0]);
    for(int i = 1; i + 1 < n; i++)
    {
        tris[t++] = {apex, {0.f, 0.f}, face};
        tris[t++] = {shown(L, ring[i]), {0.f, 0.f}, face};
        tris[t++] = {shown(L, ring[i + 1]), {0.f, 0.f}, face};
    }
    gfx::draw({tris.data(), static_cast<za::SizeT>(t)}, glm::mat4{1.f},
        {.shade = gfx::Shade::Color, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});

    // The frame line: each piece a quad `width` across, in the drawn pixels (the edges past the screen drawn too: unseen).
    za::Array<gfx::Vertex, 512 * 6> quads{};
    int q = 0;
    for(int i = 0; i < n; i++)
    {
        const glm::vec2 a = toPx(L, glm::vec2{shown(L, ring[i])}), b = toPx(L, glm::vec2{shown(L, ring[(i + 1) % n])});
        const glm::vec2 d = b - a;
        if(glm::dot(d, d) < 1e-6f)
        {
            continue;
        }
        const glm::vec2 side = glm::normalize(glm::vec2{-d.y, d.x}) * (width * 0.5f);
        const auto clip = [&](const glm::vec2& px) { return glm::vec3{toNdc(L, px), 0.5f}; };
        const gfx::Vertex v0{clip(a - side), {0.f, 0.f}, line}, v1{clip(b - side), {0.f, 0.f}, line};
        const gfx::Vertex v2{clip(b + side), {0.f, 0.f}, line}, v3{clip(a + side), {0.f, 0.f}, line};
        quads[q++] = v0, quads[q++] = v1, quads[q++] = v2, quads[q++] = v0, quads[q++] = v2, quads[q++] = v3;
    }
    gfx::draw({quads.data(), static_cast<za::SizeT>(q)}, glm::mat4{1.f},
        {.shade = gfx::Shade::Color, .blend = gfx::Blend::Alpha, .depthTest = false, .depthWrite = false});
}

// The contents: the panel's image over its rectangle as a grid through the lens, as a small CRT, its face clear.
void drawContents(const Layer& L, int p, const Mount& m, const gadget::VisorImage& img, const gfx::State& state,
    const glm::vec4& phosphor, glm::vec4& rect)
{
    // The rectangle in layer pixels (the image never mirrored: its left at the screen's left).
    const glm::vec2 a = localToPx(L, p, m.lo), b = localToPx(L, p, m.hi);
    const glm::vec2 lo = glm::min(a, b), hi = glm::max(a, b);
    constexpr int gx = 12, gy = 6;
    za::Array<gfx::Vertex, (gx + 1) * (gy + 1)> grid{};
    glm::vec2 drawnLo{1e9f}, drawnHi{-1e9f};
    for(int j = 0; j <= gy; j++)
    {
        for(int i = 0; i <= gx; i++)
        {
            const glm::vec2 uv{static_cast<float>(i) / gx, static_cast<float>(j) / gy};
            const glm::vec2 px{glm::mix(lo.x, hi.x, uv.x), glm::mix(hi.y, lo.y, uv.y)}; // (v up the image)
            const glm::vec3 c = shown(L, px);
            grid[j * (gx + 1) + i] = {c, uv, phosphor};
            const glm::vec2 seen = toPx(L, glm::vec2{c});
            drawnLo = glm::min(drawnLo, seen);
            drawnHi = glm::max(drawnHi, seen);
        }
    }
    za::Array<gfx::Vertex, gx * gy * 6> tris{};
    int t = 0;
    for(int j = 0; j < gy; j++)
    {
        for(int i = 0; i < gx; i++)
        {
            const gfx::Vertex& v00 = grid[j * (gx + 1) + i];
            const gfx::Vertex& v10 = grid[j * (gx + 1) + i + 1];
            const gfx::Vertex& v11 = grid[(j + 1) * (gx + 1) + i + 1];
            const gfx::Vertex& v01 = grid[(j + 1) * (gx + 1) + i];
            tris[t++] = v00, tris[t++] = v10, tris[t++] = v11, tris[t++] = v00, tris[t++] = v11, tris[t++] = v01;
        }
    }
    gfx::draw({tris.data(), static_cast<za::SizeT>(t)}, glm::mat4{1.f}, state, img.texture);
    rect = {drawnLo, drawnHi};
}
