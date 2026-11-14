// vr_menu_icons.inc -- icon picker rows (Item::icons; included by vr_menu.cpp; ROUND25.md, "iconpick"): a cycle drawn
// as its choices' pictures side by side under its label, each choice's name under its picture. The picked one is lit
// on a plate, the rest greyed and faded (vr_menu_icon_dim); the one under the mouse or the laser outlined. A click on a
// picture picks it, left and right step as a cycle's do; underneath it is the same cycle (its cvar, its choices by
// name), so menu paths, menu_vr, Search and the tests find each choice as before. The same code draws the flat panel
// and the headset's. Pictures: a model's thumbnail from the toolgun's icon maker (vr_toolgun_icons.cpp: the same
// models), brightened (Quake's dark guns on the dark panel) and a grey copy for the unpicked; that many white
// silhouettes of a model (Enemy Count's grunts); or an emblem drawn here (Difficulty's chevrons and skulls).
// Sizes: vr_menu_icon_size, the letters 8 pixels (a long name on two lines, smaller to fit its cell).

// The mouse's picture on an icon row (VR_Menu_Mousemove): page, row, picture; -1 none.
struct IconPickHover
{
    int page{-1};
    int row{-1};
    int icon{-1};
    const Item* item{nullptr}; // (the row as drawn: drawIconRow knows its row by it)
};
IconPickHover iconHover;

// The pictures made from the toolgun's thumbnails (bright, grey, silhouette) and the emblems (colour, grey), made when
// first drawn, kept for the session (made again after a game change: iconsGameDirChanged).
enum class IconTone : unsigned char
{
    Bright,
    Grey,
    Silhouette,
};
struct DerivedIcon
{
    const void* source{nullptr}; // the thumbnail's gfx::Picture, or the emblem's number (+1)
    IconTone tone{IconTone::Bright};
    bool made{false};
    gfx::Picture picture;
};
constexpr int maxDerivedIcons = 48;
constexpr int emblemSize = 64;
constexpr int maxThumbPixels = 128 * 128; // (the toolgun's thumbnails are 96 across)
struct IconPictures
{
    za::InPlaceVector<DerivedIcon, maxDerivedIcons> icons;
    unsigned char pixels[maxThumbPixels * 4]{}; // (an emblem painted, or a thumbnail's copy toned)
};
IconPictures iconPictures; // (its pictures' textures kept for the session)

[[nodiscard]] bool iconRowOn(const Item& item)
{
    return item.icons && item.kind == Item::Cycle && !item.choices.empty() && vr_menu_icons.value != 0.f;
}

// Its pictures' size (Difficulty, a big row: a quarter bigger), menu pixels.
[[nodiscard]] float iconSize(const Item& item)
{
    return CLAMP(16.f, vr_menu_icon_size.value, 48.f) * (item.look == Item::Big ? 1.25f : 1.f);
}

// The label's letters above the pictures (a big row's bigger).
[[nodiscard]] int iconTitleSize(const Item& item)
{
    return item.look == Item::Normal ? 8 : letterSize(item);
}

// Where its cells go: `n` side by side across the middle, each `cell` wide from `x0`.
struct IconRowLayout
{
    int n{0};
    float cell{0.f};
    float x0{0.f};
    float size{0.f};    // the pictures'
    float iconTop{0.f}; // below the row's top
    float namesTop{0.f};
    int lines{1};       // the names' lines (a long one on two)
};

// A choice's name in its cell: one line at 8 pixels if it fits, else split at its middle space onto two (`split`: where
// the second starts), each as large as fits (at least 5).
struct IconName
{
    int split{0};
    int dim{8};
};
[[nodiscard]] IconName iconName(const char* name, float cell)
{
    IconName out;
    const int len = static_cast<int>(strlen(name));
    const float room = cell - 3.f;
    if(static_cast<float>(len * 8) <= room)
    {
        return out;
    }
    int best = 0;
    for(int c = 1; c < len; c++)
    {
        if(name[c] == ' ' && (best == 0 || abs(len - 2 * c) < abs(len - 2 * best)))
        {
            best = c;
        }
    }
    const int longest = best > 0 ? q_max(best, len - best - 1) : len;
    out.split = best;
    out.dim = CLAMP(5, static_cast<int>(room / static_cast<float>(q_max(longest, 1))), 8);
    return out;
}

[[nodiscard]] IconRowLayout iconRowLayout(const Item& item)
{
    IconRowLayout l;
    l.n = static_cast<int>(item.choices.size());
    l.size = iconSize(item);
    const float cellMax = za::fmax(l.size + 20.f, 68.f);
    const float total = za::fmin(304.f, cellMax * static_cast<float>(l.n));
    l.cell = total / static_cast<float>(q_max(l.n, 1));
    l.x0 = 160.f - total * 0.5f;
    l.iconTop = static_cast<float>(iconTitleSize(item) + 5);
    l.namesTop = l.iconTop + l.size + 3.f;
    for(const Choice& c : item.choices)
    {
        l.lines = q_max(l.lines, iconName(c.label, l.cell).split > 0 ? 2 : 1);
    }
    return l;
}

int iconRowLeft(const Item& item)
{
    return static_cast<int>(iconRowLayout(item).x0);
}

int iconRowHeight(const Item& item)
{
    const IconRowLayout l = iconRowLayout(item);
    return static_cast<int>(za::ceil(l.namesTop)) + 9 * l.lines + 3;
}

// The picture under (cx, menu x) and (relY, below the row's top), or -1: its cell's width, from the picture's top to
// its name's bottom.
[[nodiscard]] int iconAt(const Item& item, float cx, float relY)
{
    const IconRowLayout l = iconRowLayout(item);
    if(relY < l.iconTop - 2.f || relY >= static_cast<float>(iconRowHeight(item)) || cx < l.x0)
    {
        return -1;
    }
    const int c = static_cast<int>((cx - l.x0) / l.cell);
    return c < l.n ? c : -1;
}

// The picture of row `i` under the mouse (menu coordinates), or -1 (not an icon row, not shown, between pictures).
[[nodiscard]] int iconAtRow(const za::Vector<Item>& list, int i, float cx, float cy)
{
    const int first = ui().scrolls[ui().page];
    if(i < first || i >= static_cast<int>(list.size()) || !iconRowOn(list[i]))
    {
        return -1;
    }
    return iconAt(list[i], cx, cy - static_cast<float>(layout().listTop + rowTop(list, first, i)));
}

// Choice `c`'s picture's centre (menu coordinates) on row `i` as shown (the tests click there).
[[nodiscard]] bool iconCentre(const za::Vector<Item>& list, int i, int c, float& x, float& y)
{
    const int first = ui().scrolls[ui().page];
    if(i < first || i >= static_cast<int>(list.size()) || !iconRowOn(list[i]))
    {
        return false;
    }
    const IconRowLayout l = iconRowLayout(list[i]);
    x = l.x0 + (static_cast<float>(c) + 0.5f) * l.cell;
    y = static_cast<float>(layout().listTop + rowTop(list, first, i)) + l.iconTop + l.size * 0.5f;
    return c >= 0 && c < l.n;
}

// ----------------------------------------------------------------------------
// Emblems: painted shapes, antialiased (4x4 samples a pixel), a dark outline round them.
// ----------------------------------------------------------------------------

// (The emblem's 2D: geom's, in the z = 0 plane.)
[[nodiscard]] float emblemSegment(float px, float py, float ax, float ay, float bx, float by)
{
    return geom::segmentDistance(glm::vec3{px, py, 0.f}, glm::vec3{ax, ay, 0.f}, glm::vec3{bx, by, 0.f});
}

[[nodiscard]] bool inEllipse(float px, float py, float cx, float cy, float rx, float ry)
{
    const float dx = (px - cx) / rx, dy = (py - cy) / ry;
    return rx > 0.f && ry > 0.f && dx * dx + dy * dy <= 1.f;
}

// Is (px, py) (0..64) in emblem `e`'s shape grown by `grow` pixels (the outline's)?
[[nodiscard]] bool inEmblem(MenuIcon::Emblem e, float px, float py, float grow)
{
    switch(e)
    {
        case MenuIcon::Easy:
        case MenuIcon::Normal:
        case MenuIcon::Hard:
        {
            // Chevrons pointing up, stacked: one, two, three.
            const int n = e == MenuIcon::Easy ? 1 : e == MenuIcon::Normal ? 2 : 3;
            const float step = 15.f;
            const float top = 32.f - step * static_cast<float>(n - 1) * 0.5f - 6.f;
            for(int k = 0; k < n; k++)
            {
                const float y = top + step * static_cast<float>(k);
                if(za::fmin(emblemSegment(px, py, 10.f, y + 16.f, 32.f, y), emblemSegment(px, py, 32.f, y, 54.f, y + 16.f)) <=
                    4.5f + grow)
                {
                    return true;
                }
            }
            return false;
        }
        case MenuIcon::Nightmare:
        case MenuIcon::Bloody:
        {
            // A skull: the cranium, the jaw; the eyes, the nose and the teeth's gaps cut out.
            const bool bone = inEllipse(px, py, 32.f, 26.f, 21.f + grow, 19.f + grow) ||
                              (px >= 19.f - grow && px <= 45.f + grow && py >= 30.f && py <= 51.f + grow);
            const bool drip = e == MenuIcon::Bloody && (emblemSegment(px, py, 23.f, 50.f, 23.f, 58.f) <= 2.2f + grow ||
                                                           emblemSegment(px, py, 34.f, 50.f, 34.f, 62.f) <= 2.4f + grow ||
                                                           emblemSegment(px, py, 42.f, 50.f, 42.f, 56.f) <= 2.f + grow);
            if(!bone && !drip)
            {
                return false;
            }
            const float g = grow > 0.f ? 0.f : 1.f; // (the outline round the outside only)
            const bool hole = g > 0.f && (inEllipse(px, py, 24.f, 30.f, 6.5f, 6.f) || inEllipse(px, py, 40.f, 30.f, 6.5f, 6.f) ||
                                             (py >= 36.f && py <= 42.f && za::fabs(px - 32.f) <= (py - 36.f) * 0.5f + 0.5f) ||
                                             (py >= 45.f && py <= 51.f && (za::fabs(px - 27.f) < 0.9f || za::fabs(px - 32.f) < 0.9f || za::fabs(px - 37.f) < 0.9f)));
            return drip || !hole;
        }
        case MenuIcon::Halls:
        {
            // Three arches (the halls' doorways), outlined.
            for(int k = 0; k < 3; k++)
            {
                const float cx = 14.f + 18.f * static_cast<float>(k);
                const float w = 7.f, top = 22.f, bottom = 52.f;
                const float d = py < top ? za::fabs(za::sqrt((px - cx) * (px - cx) + (py - top) * (py - top)) - w)
                                         : za::fabs(za::fabs(px - cx) - w);
                if(py <= bottom + grow && d <= 1.8f + grow)
                {
                    return true;
                }
            }
            return py >= 52.f - grow && py <= 56.f + grow && px >= 4.f - grow && px <= 60.f + grow; // (their floor)
        }
        case MenuIcon::Hand:
        {
            // An open hand, palm on: the palm, four fingers, the thumb out to the left.
            const bool palm = inEllipse(px, py, 33.f, 44.f, 14.f + grow, 13.f + grow) ||
                              (px >= 20.f - grow && px <= 46.f + grow && py >= 30.f - grow && py <= 46.f);
            const float fingers[4][3] = {{22.5f, 13.f, 3.4f}, {29.5f, 8.f, 3.6f}, {36.5f, 9.f, 3.6f}, {43.f, 15.f, 3.2f}};
            bool finger = false;
            for(const auto& f : fingers)
            {
                finger = finger || emblemSegment(px, py, f[0], 32.f, f[0], f[1]) <= f[2] + grow;
            }
            return palm || finger || emblemSegment(px, py, 22.f, 46.f, 9.f, 31.f) <= 3.8f + grow;
        }
        default: return false;
    }
}

// Emblem `e` painted into `out` (64 x 64 RGBA), grey with `grey`.
void paintEmblem(MenuIcon::Emblem e, bool grey, unsigned char* out)
{
    const float colours[][3] = {
        {0.f, 0.f, 0.f}, {0.40f, 0.85f, 0.40f}, {0.95f, 0.82f, 0.30f}, {0.98f, 0.55f, 0.18f}, {0.90f, 0.86f, 0.78f},
        {0.85f, 0.10f, 0.08f}, {0.85f, 0.75f, 0.55f}, {0.93f, 0.76f, 0.60f}};
    const float* c = colours[e < 8 ? e : 0];
    for(int y = 0; y < emblemSize; y++)
    {
        for(int x = 0; x < emblemSize; x++)
        {
            float shape = 0.f, edge = 0.f;
            for(int s = 0; s < 16; s++)
            {
                const float px = static_cast<float>(x) + (static_cast<float>(s % 4) + 0.5f) * 0.25f;
                const float py = static_cast<float>(y) + (static_cast<float>(s / 4) + 0.5f) * 0.25f;
                shape += inEmblem(e, px, py, 0.f) ? 1.f / 16.f : 0.f;
                edge += inEmblem(e, px, py, 2.f) ? 1.f / 16.f : 0.f;
            }
            const float a = za::fmax(shape, edge);
            const float k = a > 0.f ? shape / a : 0.f;
            const float light = 1.15f - 0.35f * static_cast<float>(y) / static_cast<float>(emblemSize); // (lit from above)
            unsigned char* p = out + (y * emblemSize + x) * 4;
            for(int ch = 0; ch < 3; ch++)
            {
                const float v = grey ? (c[0] * 0.3f + c[1] * 0.59f + c[2] * 0.11f) : c[ch];
                p[ch] = static_cast<unsigned char>(CLAMP(0.f, (v * light * k + 0.06f * (1.f - k)) * 255.f, 255.f));
            }
            p[3] = static_cast<unsigned char>(CLAMP(0.f, a * 255.f, 255.f));
        }
    }
}

// A thumbnail toned into `out` (its size): brightened 1.6x (Bright), grey and brightened (Grey), or white keeping its
// coverage (Silhouette). Framed anew round what it covers (a model's far tiny parts, too small to draw, made the
// thumbnail's framing wide: v_shot.mdl's shotgun a third of it), its middle kept, sampled nearest.
void toneThumbnail(const gfx::Picture& src, IconTone tone, unsigned char* out)
{
    const int w = src.width, h = src.height;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for(int y = 0; y < h; y++)
    {
        for(int x = 0; x < w; x++)
        {
            if(src.pixels[(y * w + x) * 4 + 3] > 24)
            {
                x0 = q_min(x0, x), x1 = q_max(x1, x), y0 = q_min(y0, y), y1 = q_max(y1, y);
            }
        }
    }
    const float side = x1 < x0 ? static_cast<float>(w) : static_cast<float>(q_max(x1 - x0, y1 - y0) + 1) * 1.08f;
    const float cx = x1 < x0 ? static_cast<float>(w) * 0.5f : static_cast<float>(x0 + x1 + 1) * 0.5f;
    const float cy = x1 < x0 ? static_cast<float>(h) * 0.5f : static_cast<float>(y0 + y1 + 1) * 0.5f;
    const float k = side / static_cast<float>(w);
    for(int i = 0; i < w * h; i++)
    {
        // Bilinear, weighted by coverage (no dark fringe from the clear pixels round it).
        const float fx = cx + (static_cast<float>(i % w) + 0.5f - static_cast<float>(w) * 0.5f) * k - 0.5f;
        const float fy = cy + (static_cast<float>(i / w) + 0.5f - static_cast<float>(h) * 0.5f) * k - 0.5f;
        const int ix = static_cast<int>(za::floor(fx)), iy = static_cast<int>(za::floor(fy));
        float rgb[3]{}, cover = 0.f;
        for(int tap = 0; tap < 4; tap++)
        {
            const int sx = ix + (tap & 1), sy = iy + (tap >> 1);
            if(sx < 0 || sy < 0 || sx >= w || sy >= h)
            {
                continue;
            }
            const float wt = ((tap & 1) ? fx - static_cast<float>(ix) : 1.f - (fx - static_cast<float>(ix))) *
                             ((tap >> 1) ? fy - static_cast<float>(iy) : 1.f - (fy - static_cast<float>(iy)));
            const unsigned char* s = src.pixels + (sy * w + sx) * 4;
            const float a = wt * static_cast<float>(s[3]);
            cover += a;
            for(int ch = 0; ch < 3; ch++)
            {
                rgb[ch] += a * static_cast<float>(s[ch]);
            }
        }
        unsigned char* d = out + i * 4;
        for(float& v : rgb)
        {
            v = cover > 0.f ? v / cover : 0.f;
        }
        const float lum = 0.3f * rgb[0] + 0.59f * rgb[1] + 0.11f * rgb[2];
        for(int ch = 0; ch < 3; ch++)
        {
            const float v = tone == IconTone::Silhouette ? 255.f : tone == IconTone::Grey ? lum * 1.6f : rgb[ch] * 1.6f;
            d[ch] = static_cast<unsigned char>(CLAMP(0.f, v, 255.f));
        }
        d[3] = static_cast<unsigned char>(CLAMP(0.f, cover, 255.f));
    }
}

// The picture to draw for `icon` toned `tone`, or nullptr (its thumbnail still being made, or none).
[[nodiscard]] const gfx::Picture* iconPicture(const MenuIcon& icon, IconTone tone)
{
    IconPictures& ip = iconPictures;
    const gfx::Picture* thumb = icon.model ? toolgun::icons::get(icon.model, 0, icon.side) : nullptr;
    if(icon.model && (!thumb || !thumb->pixels || thumb->width * thumb->height > maxThumbPixels))
    {
        return nullptr;
    }
    const void* source = icon.model ? static_cast<const void*>(thumb)
                                    : reinterpret_cast<const void*>(static_cast<uintptr_t>(icon.emblem) + 1u);
    for(DerivedIcon& d : ip.icons)
    {
        if(d.source == source && d.tone == tone)
        {
            if(!d.made)
            {
                break;
            }
            return &d.picture;
        }
    }
    DerivedIcon* d = nullptr;
    for(DerivedIcon& o : ip.icons)
    {
        d = o.source == source && o.tone == tone ? &o : d;
    }
    if(!d)
    {
        if(ip.icons.size() >= maxDerivedIcons)
        {
            return nullptr;
        }
        d = &ip.icons.emplaceBack();
        d->source = source;
        d->tone = tone;
    }
    if(icon.model)
    {
        toneThumbnail(*thumb, tone, ip.pixels);
        gfx::ensurePicture(d->picture, thumb->width, thumb->height, "menu icon");
        gfx::updatePicture(d->picture, ip.pixels);
    }
    else
    {
        paintEmblem(icon.emblem, tone == IconTone::Grey, ip.pixels);
        gfx::ensurePicture(d->picture, emblemSize, emblemSize, "menu emblem");
        gfx::updatePicture(d->picture, ip.pixels);
    }
    d->made = true;
    return &d->picture;
}

// A picture over x, y, w, h, tinted (the row's fade kept), without the menus' red recolour (its own colours).
void drawIconPicture(const gfx::Picture* pic, float x, float y, float w, float h, float r, float g, float b, float a)
{
    if(!pic || a <= 0.f)
    {
        return;
    }
    const qboolean recolor = Draw_SetMenuRecolor(false);
    pushRowColor(r, g, b, a);
    Draw_TextureRect(x, y, w, h, static_cast<gltexture_t*>(pic->texture), 1.f);
    GL_PopCanvasColor();
    Draw_SetMenuRecolor(recolor);
}

// A frame round x0..x1, y0..y1 (1 pixel), the menus' highlight brown.
void iconFrame(float x0, float x1, float y0, float y1, float alpha)
{
    const float rgb[3] = {0.85f, 0.65f, 0.35f};
    const float a = alpha * rowFade;
    Draw_FillEx(x0, y0, x1 - x0, 1.f, rgb, a);
    Draw_FillEx(x0, y1 - 1.f, x1 - x0, 1.f, rgb, a);
    Draw_FillEx(x0, y0 + 1.f, 1.f, y1 - y0 - 2.f, rgb, a);
    Draw_FillEx(x1 - 1.f, y0 + 1.f, 1.f, y1 - y0 - 2.f, rgb, a);
}

void drawIconRow(const Item& item, int y, bool selected)
{
    toolgun::icons::makePending(4);
    const IconRowLayout l = iconRowLayout(item);
    const int titleDim = iconTitleSize(item);
    const int len = static_cast<int>(strlen(item.label));
    const int tx = 160 - titleDim * len / 2;
    const int ty = y + 2;
    if(selected && !(vr_menu_hl_fit.value != 0.f &&
                       drawRowBar(static_cast<float>(tx - 6), static_cast<float>(tx + titleDim * len + 6), ty, titleDim)))
    {
        M_DrawArrowCursor(tx - 20, ty + (titleDim - 8) / 2);
    }
    if(item.look == Item::Big)
    {
        M_PrintWhiteEx(tx, ty, titleDim, item.label);
    }
    else
    {
        M_PrintEx(tx, ty, titleDim, item.label);
    }
    const int picked = currentChoice(item);
    const float dim = CLAMP(0.f, vr_menu_icon_dim.value, 0.9f);
    const int page = ui().page;
    const float rowH = static_cast<float>(iconRowHeight(item));
    for(int c = 0; c < l.n; c++)
    {
        const MenuIcon& icon = item.icons[c];
        const float cx0 = l.x0 + static_cast<float>(c) * l.cell;
        const float cx = cx0 + l.cell * 0.5f;
        const float top = static_cast<float>(y) + l.iconTop;
        const bool on = c == picked;
        const bool hover = iconHover.page == page && iconHover.icon == c && iconHover.item == &item;
        if(on)
        {
            // The picked one on a plate (brighter while the row is selected), framed.
            const float rgb[3] = {0.55f, 0.42f, 0.25f};
            Draw_FillEx(cx0 + 2.f, top - 2.f, l.cell - 4.f, rowH - l.iconTop, rgb, (selected ? 0.34f : 0.22f) * rowFade);
            iconFrame(cx0 + 2.f, cx0 + l.cell - 2.f, top - 2.f, static_cast<float>(y) + rowH, selected ? 0.9f : 0.55f);
        }
        else if(hover)
        {
            iconFrame(cx0 + 2.f, cx0 + l.cell - 2.f, top - 2.f, static_cast<float>(y) + rowH, 0.45f);
        }
        const float fade = on || hover ? 1.f : 1.f - dim;
        if(icon.figures > 0)
        {
            // That many silhouettes side by side (overlapping), the middle one in front.
            const int n = icon.figures;
            const float s = l.size * (n == 1 ? 1.f : n == 2 ? 0.86f : 0.76f);
            const float span = za::fmin(l.cell - 6.f, l.size * 1.5f) - s;
            const float tint = on ? 1.f : hover ? 0.85f : 0.62f;
            for(int k = 0; k < n; k++)
            {
                const int f = n == 3 ? (k == 2 ? 1 : k == 1 ? 2 : 0) : k; // (3: left, right, then the middle over them)
                const float fx = cx - s * 0.5f + (n > 1 ? span * (static_cast<float>(f) / static_cast<float>(n - 1) - 0.5f) : 0.f);
                drawIconPicture(iconPicture(icon, IconTone::Silhouette), fx, top + (l.size - s), s, s, 0.95f * tint,
                    0.85f * tint, 0.68f * tint, fade);
            }
        }
        else
        {
            drawIconPicture(iconPicture(icon, on || hover ? IconTone::Bright : IconTone::Grey), cx - l.size * 0.5f, top,
                l.size, l.size, 1.f, 1.f, 1.f, fade);
        }
        // Its name under it (two lines where it needs them), white while picked.
        const char* name = item.choices[c].label;
        const IconName nm = iconName(name, l.cell);
        char part[2][32]{};
        if(nm.split > 0)
        {
            q_strlcpy(part[0], name, q_min(static_cast<size_t>(nm.split + 1), sizeof(part[0])));
            q_strlcpy(part[1], name + nm.split + 1, sizeof(part[1]));
        }
        else
        {
            q_strlcpy(part[0], name, sizeof(part[0]));
        }
        const int lines = nm.split > 0 ? 2 : 1;
        for(int line = 0; line < lines; line++)
        {
            const int w = nm.dim * static_cast<int>(strlen(part[line]));
            const int nx = static_cast<int>(cx) - w / 2;
            const int ny = static_cast<int>(static_cast<float>(y) + l.namesTop) + line * 9 + (8 - nm.dim) / 2 +
                           (lines < l.lines ? 4 : 0);
            if(on)
            {
                M_PrintWhiteEx(nx, ny, nm.dim, part[line]);
            }
            else
            {
                pushRowColor(1.f, 1.f, 1.f, hover ? 1.f : 1.f - dim * 0.6f);
                M_PrintEx(nx, ny, nm.dim, part[line]);
                GL_PopCanvasColor();
            }
        }
    }
}

// The mouse (or the laser) over an icon row: the picture under it outlined.
void iconRowMouse(const za::Vector<Item>& list, float cx, float cy)
{
    const int raw = rowAtRaw(cy);
    const int icon = raw >= 0 ? iconAtRow(list, raw, cx, cy) : -1;
    iconHover = IconPickHover{ui().page, icon >= 0 ? raw : -1, icon, icon >= 0 ? &list[static_cast<size_t>(raw)] : nullptr};
}

// A click on the selected icon row: the picture under the mouse picked (true: it was on one).
[[nodiscard]] bool iconRowClick(const za::Vector<Item>& list, int cursor, float cx, float cy)
{
    const int c = iconAtRow(list, cursor, cx, cy);
    if(c < 0)
    {
        return false;
    }
    const Item& item = list[cursor];
    if(lockedItem(item))
    {
        qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
        return true;
    }
    Cvar_SetValueQuick(item.cvar, item.choices[static_cast<size_t>(c)].value); // (`item` may be rebuilt after this)
    qvr::menuui::uiSound(qvr::menuui::UiSound::Adjust);
    return true;
}

// The game changed: the pictures toned from the thumbnails made again from its files' (the emblems kept).
void iconsGameDirChangedImpl()
{
    for(DerivedIcon& d : iconPictures.icons)
    {
        d.made = d.made && reinterpret_cast<uintptr_t>(d.source) <= 16u; // (an emblem's number, not a thumbnail)
    }
}
