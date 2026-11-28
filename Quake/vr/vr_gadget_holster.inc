// vr_gadget_holster.inc -- part of vr_gadget.cpp: flatscreen play's holster menu's image (vr_gadget.hpp
// renderHolsterMenu; vr_flatholster.cpp lays it out, hit-tests it and shows it in the screen's middle). Drawn by the
// screens' own code as the visor's panels are (its palette, drawText, the black face Shade::Screen's inkAlpha leaves
// clear), with each weapon's thumbnail (vr_toolgun_icons.cpp: its model drawn from its side) in its own colours.
//
//   LEFT SHOULDER   (o)   RIGHT SHOULDER        LEFT HAND   <=>   RIGHT HAND
//   [gun] [L][v][R]  |    [gun] [L][v][R]       [  gun  ]  [SWAP]  [  gun  ]
//      LEFT CHEST  -[ ]-  RIGHT CHEST            [ DROP ]          [ DROP ]
//      LEFT HIP     / \   RIGHT HIP

struct HolsterMenuState
{
    gfx::RenderTarget target;
    unsigned version{0};
};
HolsterMenuState holsterMenu;

// A rectangle's outline, a pixel wide.
void holsterOutline(const glm::vec4& r, const glm::vec3& c)
{
    fill(r.x, r.y, r.z, 1.f, c);
    fill(r.x, r.y + r.w - 1.f, r.z, 1.f, c);
    fill(r.x, r.y, 1.f, r.w, c);
    fill(r.x + r.z - 1.f, r.y, 1.f, r.w, c);
}

// A stroke of the figure, as squares along it (a pixel's steps).
void holsterStroke(const glm::vec4& l, const glm::vec3& c)
{
    const float dx = l.z - l.x, dy = l.w - l.y;
    const int steps = za::max(1, static_cast<int>(za::max(za::abs(dx), za::abs(dy)) * 2.f));
    for(int i = 0; i <= steps; i++)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        fill(l.x + dx * t - 0.5f, l.y + dy * t - 0.5f, 1.f, 1.f, c);
    }
}

// A button's icon, its middle at cx, cy, about 11 pixels tall.
void holsterIcon(MenuIcon icon, float cx, float cy, const glm::vec3& c)
{
    switch(icon)
    {
    case MenuIcon::HandLeft:
    case MenuIcon::HandRight:
    {
        // A palm, four fingers up, the thumb out at its side (the left hand's to the right: its back seen).
        const float thumbX = icon == MenuIcon::HandLeft ? cx + 3.f : cx - 5.f;
        fill(cx - 3.f, cy - 1.f, 6.f, 5.f, c);
        for(int f = 0; f < 4; f++)
        {
            fill(cx - 3.f + static_cast<float>(f) * 1.5f, cy - 5.f + (f == 0 || f == 3 ? 1.f : 0.f), 1.f, 4.f, c);
        }
        fill(thumbX, cy, 2.f, 1.f, c);
        fill(icon == MenuIcon::HandLeft ? thumbX + 1.f : thumbX, cy - 1.f, 1.f, 1.f, c);
        fill(cx - 2.f, cy + 4.f, 4.f, 1.5f, c); // (the wrist)
        break;
    }
    case MenuIcon::Drop:
    {
        // An arrow pointing down onto the ground.
        fill(cx - 1.f, cy - 5.f, 2.f, 5.f, c);
        for(int r = 0; r < 4; r++)
        {
            const float half = 3.5f - static_cast<float>(r);
            fill(cx - half, cy + static_cast<float>(r), half * 2.f, 1.f, c);
        }
        fill(cx - 4.5f, cy + 5.f, 9.f, 1.f, c);
        break;
    }
    case MenuIcon::Swap:
    {
        // Two arrows: the upper to the right, the lower to the left.
        fill(cx - 5.f, cy - 3.f, 8.f, 1.f, c);
        fill(cx - 5.f, cy + 3.f, 8.f, 1.f, c);
        fill(cx - 3.f, cy + 3.f, 8.f, 1.f, c);
        for(int r = 0; r < 3; r++)
        {
            const float h = 5.f - static_cast<float>(r) * 2.f;
            fill(cx + 3.f + static_cast<float>(r), cy - 3.f - h * 0.5f + 0.5f, 1.f, h, c);
            fill(cx - 4.f - static_cast<float>(r), cy + 3.f - h * 0.5f + 0.5f, 1.f, h, c);
        }
        break;
    }
    case MenuIcon::Unload:
    {
        // A magazine with its rounds stacked in it, an arrow out of its side: the rounds back into the reserve.
        holsterOutline({cx - 6.f, cy - 5.f, 6.f, 11.f}, c);
        for(int r = 0; r < 3; r++)
        {
            fill(cx - 4.f, cy - 3.f + static_cast<float>(r) * 3.f, 2.f, 1.5f, c);
        }
        fill(cx + 1.f, cy - 0.5f, 3.f, 1.f, c);
        for(int r = 0; r < 3; r++)
        {
            const float h = 5.f - static_cast<float>(r) * 2.f;
            fill(cx + 3.f + static_cast<float>(r), cy - h * 0.5f, 1.f, h, c);
        }
        break;
    }
    case MenuIcon::FlipGrip:
    {
        // Two blades side by side, the left point up and the right point down (a blade, its guard, its grip), an arc over
        // them from one to the other: the grip turned over.
        const float l = cx - 4.f, r = cx + 3.f;
        fill(l, cy - 5.f, 1.f, 1.f, c * 0.7f); // the left: its point,
        fill(l - 0.5f, cy - 4.f, 2.f, 4.f, c); // its blade,
        fill(l - 1.5f, cy, 4.f, 1.f, c);       // its guard,
        fill(l, cy + 1.f, 1.f, 3.f, c);        // its grip
        fill(r, cy - 4.f, 1.f, 3.f, c);        // the right, mirrored: its grip,
        fill(r - 1.5f, cy - 1.f, 4.f, 1.f, c); // its guard,
        fill(r - 0.5f, cy, 2.f, 4.f, c);       // its blade,
        fill(r, cy + 4.f, 1.f, 1.f, c * 0.7f); // its point
        // The arc from over the left to over the right, its arrowhead pointing down onto the right.
        holsterStroke({l, cy - 6.f, l + 1.5f, cy - 7.5f}, c);
        holsterStroke({l + 1.5f, cy - 7.5f, r - 1.f, cy - 7.5f}, c);
        holsterStroke({r - 1.f, cy - 7.5f, r + 0.5f, cy - 6.f}, c);
        fill(r - 1.f, cy - 6.f, 3.f, 1.f, c);
        break;
    }
    default: break;
    }
}

// Text `size` pixels a character, its middle at cx.
void holsterTextMid(float cx, float y, float size, const char* s, const glm::vec3& c, const Palette& pal)
{
    drawText(cx - static_cast<float>(strlen(s)) * size * 0.5f, y, size, s, c, pal);
}

// wheelui (his words 2026-11-28): a slot's holster title smaller and fainter than its weapon's name; the ammunition's
// line's characters; the attachment's glyph's width.
constexpr float holsterTitleSize = 5.f;
constexpr float holsterTitleInk = 0.55f;
constexpr float holsterAmmoSize = 5.f;
constexpr float holsterGlyphW = 8.f;

// A blade lying along the line, pointing right (its grip, guard, blade and point), `cy` its middle: an attachment (the
// bayonet, a dagger as one) after the weapon's name.
void holsterBladeGlyph(float x, float cy, const glm::vec3& c)
{
    fill(x, cy - 0.5f, 2.f, 1.f, c * 0.8f);       // the grip,
    fill(x + 2.f, cy - 2.f, 1.f, 4.f, c);         // the guard,
    fill(x + 3.f, cy - 1.f, 4.f, 2.f, c);         // the blade,
    fill(x + 7.f, cy - 0.5f, 1.f, 1.f, c * 0.7f); // its point
}

// The HUD's picture of QC ammo type `aid` (AID_*; -1 none): the mission pack's own lava nails, multi-rockets and plasma
// when its gfx.wad has them, else as the nails, rockets and cells (the visor's grid's).
[[nodiscard]] int holsterAmmoPic(int aid)
{
    if(aid >= 5 && aid <= 7 && greyPic(PicRogueAmmo + aid - 5))
    {
        return PicRogueAmmo + aid - 5;
    }
    return aid >= 1 && aid <= 7 ? PicAmmo + (aid <= 4 ? aid - 1 : aid - 4) : -1;
}

// A slot's ammunition, its middle at cx: "4/8" (in the magazine / its size; red when empty), then the ammo type's icon and
// the reserve ("4/8 [shells] 32"); either part alone where the other is none.
void holsterAmmoLine(const HolsterMenuSlot& s, float cx, float y, bool greyed, const Palette& pal)
{
    char clip[24] = "", reserve[16] = "";
    if(s.clipSize > 0)
    {
        q_snprintf(clip, sizeof(clip), "%d/%d", s.clip, s.clipSize);
    }
    if(s.reserve >= 0)
    {
        q_snprintf(reserve, sizeof(reserve), "%d", s.reserve);
    }
    const int pic = s.reserve >= 0 ? holsterAmmoPic(s.ammoType) : -1;
    constexpr float sz = holsterAmmoSize;
    const float gap = clip[0] && (reserve[0] || pic >= 0) ? 3.f : 0.f;
    const float picW = pic >= 0 ? sz + 1.f : 0.f;
    const float clipW = static_cast<float>(strlen(clip)) * sz;
    const float w = clipW + gap + picW + static_cast<float>(strlen(reserve)) * sz;
    float x = cx - w * 0.5f;
    if(clip[0])
    {
        drawText(x, y, sz, clip, greyed ? pal.dim : s.clip <= 0 ? pal.warn : pal.value, pal);
        x += clipW + gap;
    }
    if(pic >= 0)
    {
        drawPicSized(x, y, pic, sz, sz, 24.f, greyed ? pal.dim : pal.label, pal);
        x += picW;
    }
    if(reserve[0])
    {
        drawText(x, y, sz, reserve, greyed || s.reserve <= 0 ? pal.dim : pal.label, pal);
    }
}

void holsterSlot(const HolsterMenuSlot& s, const Palette& pal)
{
    const glm::vec4& r = s.rect;
    const bool empty = !s.name[0];
    using Drop = HolsterMenuSlot::Drop;
    // A drag: the places it may go lit (brighter under the cursor), those it may not greyed, its own faded.
    const bool valid = s.drop == Drop::Valid, invalid = s.drop == Drop::Invalid, source = s.drop == Drop::Source;
    const glm::vec3 face = valid ? pal.line * (s.hovered ? 0.5f : 0.3f)
                           : invalid ? pal.dim * 0.05f
                           : s.hovered && !source ? pal.line * 0.22f
                                                  : pal.dim * 0.12f;
    const glm::vec3 edge = valid ? glm::min(pal.value * (s.hovered ? 1.4f : 1.15f), glm::vec3{1.f})
                           : invalid || source ? pal.dim * 0.5f
                           : s.hovered ? pal.value
                                       : pal.line;
    fill(r.x, r.y, r.z, r.w, face);
    holsterOutline(r, edge);
    if(valid)
    {
        holsterOutline({r.x - 1.f, r.y - 1.f, r.z + 2.f, r.w + 2.f}, edge * 0.6f); // (a second ring: a drop target)
    }
    // The corners' ticks (the gadget's frames).
    fill(r.x + 1.f, r.y + 1.f, 3.f, 1.f, pal.value);
    fill(r.x + 1.f, r.y + 1.f, 1.f, 3.f, pal.value);
    fill(r.x + r.z - 4.f, r.y + r.w - 2.f, 3.f, 1.f, pal.value);
    fill(r.x + r.z - 2.f, r.y + r.w - 4.f, 1.f, 3.f, pal.value);
    // The holster's title smaller and fainter than the weapon's name (wheelui, his words 2026-11-28).
    holsterTextMid(r.x + r.z * 0.5f, r.y + 3.f, holsterTitleSize, s.title, pal.label * holsterTitleInk, pal);
    // The name as large as fits (6 pixels a character at most), a blade glyph after it for an attachment; the ammunition's
    // line under it.
    const char* name = empty ? "- EMPTY -" : s.name;
    const bool glyph = !empty && s.attach != 0;
    const float glyphW = glyph ? holsterGlyphW + 1.f : 0.f;
    const float size = za::min(6.f, (r.z - 4.f - glyphW) / static_cast<float>(za::max<za::SizeT>(strlen(name), 1)));
    const bool ammoLine = !empty && (s.clipSize > 0 || s.reserve >= 0);
    const float nameY = r.y + r.w - 3.f - size - (ammoLine ? holsterAmmoSize + 1.f : 0.f);
    const float picTop = r.y + 10.f;
    const float picSize = za::min(r.z - 8.f, nameY - 1.f - picTop);
    if(!empty && s.model[0] && picSize > 4.f)
    {
        if(const gfx::Picture* pic = toolgun::icons::get(s.model, 0, true, s.attach))
        {
            // On a lit plate, brightened: Quake's dark guns on the dark face read as nothing (Shade::Screen's inkAlpha
            // leaves the dark clear).
            const float px = r.x + (r.z - picSize) * 0.5f, py = picTop;
            const float k = source || invalid ? 0.35f : 1.f; // (dragged away, or greyed)
            fill(px, py + picSize * 0.2f, picSize, picSize * 0.6f, pal.label * 0.45f * k);
            gfx::draw2D::color(glm::vec4{1.8f * k, 1.8f * k, 1.8f * k, 1.f});
            gfx::draw2D::picture(*pic, px, py, picSize, picSize);
            gfx::draw2D::color(glm::vec4{1.f});
        }
    }
    const glm::vec3 nameInk = empty || invalid || source ? pal.dim : pal.value;
    const float nameW = static_cast<float>(strlen(name)) * size;
    const float nameX = r.x + (r.z - nameW - glyphW) * 0.5f;
    drawText(nameX, nameY, size, name, nameInk, pal);
    if(glyph)
    {
        holsterBladeGlyph(nameX + nameW + 1.f, nameY + size * 0.5f, nameInk);
    }
    if(ammoLine)
    {
        holsterAmmoLine(s, r.x + r.z * 0.5f, nameY + size + 1.f, invalid || source, pal);
    }
}

// The weapon being dragged: its thumbnail on a lit plate under the cursor (`at` its middle), its name below.
void holsterDragged(const HolsterMenuView& v, const Palette& pal)
{
    if(!v.dragModel[0] && !v.dragName[0])
    {
        return;
    }
    constexpr float picSize = 34.f;
    const float px = za::clamp(v.dragAt.x - picSize * 0.5f, 0.f, v.size.x - picSize);
    const float py = za::clamp(v.dragAt.y - picSize * 0.5f, 0.f, v.size.y - picSize - 8.f);
    fill(px, py + picSize * 0.2f, picSize, picSize * 0.6f, pal.label * 0.55f);
    holsterOutline({px - 1.f, py + picSize * 0.2f - 1.f, picSize + 2.f, picSize * 0.6f + 2.f}, pal.value);
    if(v.dragModel[0])
    {
        if(const gfx::Picture* pic = toolgun::icons::get(v.dragModel, 0, true))
        {
            gfx::draw2D::color(glm::vec4{2.f, 2.f, 2.f, 1.f});
            gfx::draw2D::picture(*pic, px, py, picSize, picSize);
            gfx::draw2D::color(glm::vec4{1.f});
        }
    }
    holsterTextMid(px + picSize * 0.5f, py + picSize * 0.8f + 2.f, 6.f, v.dragName, pal.value, pal);
}

void holsterButton(const HolsterMenuButton& b, const Palette& pal)
{
    const glm::vec4& r = b.rect;
    const glm::vec3 ink = !b.enabled ? pal.dim * 0.6f : b.hovered ? glm::min(pal.value * 1.25f, glm::vec3{1.f}) : pal.value;
    fill(r.x, r.y, r.z, r.w, b.hovered && b.enabled ? pal.line * 0.45f : pal.dim * 0.1f);
    holsterOutline(r, b.enabled ? (b.hovered ? pal.value : pal.line) : pal.dim * 0.5f);
    const bool labelled = b.label[0] != 0;
    if(labelled && r.w >= 24.f)
    {
        // Tall (the weapon wheel's actions): the icon above its label (as large as fits, 6 pixels a character at most);
        // lit with the drop target's second ring, as the wheel's lit slot.
        if(b.hovered && b.enabled)
        {
            holsterOutline({r.x - 1.f, r.y - 1.f, r.z + 2.f, r.w + 2.f}, pal.value * 0.6f);
        }
        holsterIcon(b.icon, r.x + r.z * 0.5f, r.y + r.w * 0.5f - 4.f, ink);
        const float size = za::min(6.f, (r.z - 4.f) / static_cast<float>(za::max<za::SizeT>(strlen(b.label), 1)));
        holsterTextMid(r.x + r.z * 0.5f, r.y + r.w * 0.5f + 4.f, size, b.label, ink, pal);
        return;
    }
    const float iconX = labelled ? r.x + 8.f : r.x + r.z * 0.5f;
    holsterIcon(b.icon, iconX, r.y + r.w * 0.5f, ink);
    if(labelled)
    {
        drawText(iconX + 7.f, r.y + r.w * 0.5f - 3.f, 6.f, b.label, ink, pal);
    }
}

VisorImage holsterMenuRender(const HolsterMenuView& v)
{
    VisorImage out;
    const int w = static_cast<int>(v.size.x), h = static_cast<int>(v.size.y);
    if(w <= 0 || h <= 0)
    {
        return out;
    }
    const Palette pal = palette();
    toolgun::icons::makePending(2);
    gfx::RenderTarget& t = holsterMenu.target;
    if(!t.ensure(w * visorTexels, h * visorTexels, gfx::RenderTargetFormat{.mipmaps = true, .fboName = "vr canvas"}, "holster menu")) // mipmaps: for its text's glow
    {
        Con_Warning("VR: the holster menu's framebuffer is incomplete\n");
        return out;
    }
    gfx::begin2D(t, w, h);
    fill(0.f, 0.f, v.size.x, v.size.y, visorBlack);
    holsterOutline({1.f, 1.f, v.size.x - 2.f, v.size.y - 2.f}, pal.line * 0.6f);
    if(v.header[0])
    {
        drawText(6.f, 4.f, 8.f, v.header, pal.label, pal);
        fill(6.f, 13.f, v.size.x - 12.f, 1.f, pal.line * 0.5f);
    }
    if(v.body)
    {
        // The player's body (flatholster's: grey, lit), in the screen's colour.
        gfx::draw2D::color(glm::vec4{pal.label * 0.35f, 1.f});
        gfx::draw2D::picture(*v.body, v.bodyRect.x, v.bodyRect.y, v.bodyRect.z, v.bodyRect.w);
        gfx::draw2D::color(glm::vec4{1.f});
    }
    for(int i = 0; i < v.lineCount; i++)
    {
        holsterStroke(v.lines[i], pal.line * 0.8f);
    }
    for(int i = 0; i < v.slotCount; i++)
    {
        holsterSlot(v.slots[i], pal);
    }
    for(int i = 0; i < v.buttonCount; i++)
    {
        holsterButton(v.buttons[i], pal);
    }
    holsterTextMid(v.size.x * 0.5f, v.size.y - 9.f, 6.f, v.footer, pal.dim * 1.6f, pal);
    holsterDragged(v, pal); // (over everything)
    gfx::end2D();
    out.texture = t.tex;
    out.size = v.size;
    out.shown = t.tex != 0;
    out.version = ++holsterMenu.version;
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// Flatscreen play's bindings reminder (wheelui; vr_bindpanel.cpp says what is on it): rows of an action and its keys as key
// caps, in columns, drawn as the holster menu is, on its own target (both show at once).
//
//   Fire          [MOUSE1]           Reload       [R]
//   Crouch        [C] [CTRL]         Holster menu [MOUSE3]

struct BindPanelState
{
    gfx::RenderTarget target;
    unsigned version{0};
};
BindPanelState bindPanel; // seat-shared: the image's target, made and drawn in the same seat's pass (as holsterMenu)

constexpr float bindChar = 6.f; // the text's characters
constexpr float bindRowH = 11.f;
constexpr float bindPad = 5.f;
constexpr float bindColGap = 12.f;

[[nodiscard]] float bindCapW(const char* key)
{
    return static_cast<float>(strlen(key)) * bindChar + 4.f;
}

struct BindPanelMetrics
{
    int perColumn{0};
    float labelW{0.f}; // the actions' column, the keys' caps after it
    float columnW{0.f};
};

[[nodiscard]] BindPanelMetrics bindPanelMetrics(const BindPanelView& v)
{
    BindPanelMetrics m;
    const int columns = za::max(v.columns, 1);
    m.perColumn = (v.rowCount + columns - 1) / columns;
    float keys = 0.f;
    for(int i = 0; i < v.rowCount; i++)
    {
        const BindPanelRow& r = v.rows[i];
        m.labelW = za::max(m.labelW, static_cast<float>(strlen(r.action)) * bindChar);
        float w = 0.f;
        for(const char* k : r.keys)
        {
            w += k[0] ? bindCapW(k) + 2.f : 0.f;
        }
        keys = za::max(keys, w);
    }
    m.labelW += bindChar;
    m.columnW = m.labelW + za::max(keys, bindCapW("???"));
    return m;
}

glm::vec2 bindPanelLayoutSize(const BindPanelView& v)
{
    if(v.rowCount <= 0)
    {
        return glm::vec2{0.f};
    }
    const BindPanelMetrics m = bindPanelMetrics(v);
    const int columns = za::max(v.columns, 1);
    return {bindPad * 2.f + m.columnW * static_cast<float>(columns) + bindColGap * static_cast<float>(columns - 1),
        bindPad * 2.f + bindRowH * static_cast<float>(m.perColumn) - 2.f};
}

VisorImage bindPanelRender(const BindPanelView& v)
{
    VisorImage out;
    const glm::vec2 size = bindPanelLayoutSize(v);
    const int w = static_cast<int>(size.x + 0.5f), h = static_cast<int>(size.y + 0.5f);
    if(w <= 0 || h <= 0)
    {
        return out;
    }
    const Palette pal = palette();
    gfx::RenderTarget& t = bindPanel.target;
    if(!t.ensure(w * visorTexels, h * visorTexels, gfx::RenderTargetFormat{.mipmaps = true, .fboName = "vr canvas"}, "bindings panel"))
    {
        return out;
    }
    const BindPanelMetrics m = bindPanelMetrics(v);
    gfx::begin2D(t, w, h);
    fill(0.f, 0.f, size.x, size.y, visorBlack);
    holsterOutline({1.f, 1.f, size.x - 2.f, size.y - 2.f}, pal.line * 0.6f);
    for(int i = 0; i < v.rowCount; i++)
    {
        const BindPanelRow& r = v.rows[i];
        const int col = m.perColumn > 0 ? i / m.perColumn : 0, row = m.perColumn > 0 ? i % m.perColumn : i;
        const float x = bindPad + static_cast<float>(col) * (m.columnW + bindColGap);
        const float y = bindPad + static_cast<float>(row) * bindRowH;
        drawText(x, y + 1.f, bindChar, r.action, pal.label, pal);
        float kx = x + m.labelW;
        const bool none = !r.keys[0][0] && !r.keys[1][0];
        for(const char* k : r.keys)
        {
            if(!k[0] && !(none && k == r.keys[0]))
            {
                continue;
            }
            const char* shown = k[0] ? k : "???";
            const float cw = bindCapW(shown);
            fill(kx, y - 1.f, cw, bindChar + 3.f, pal.dim * 0.15f);
            holsterOutline({kx, y - 1.f, cw, bindChar + 3.f}, k[0] ? pal.line : pal.dim * 0.5f);
            drawText(kx + 2.f, y + 1.f, bindChar, shown, k[0] ? pal.value : pal.dim, pal);
            kx += cw + 2.f;
        }
    }
    gfx::end2D();
    out.texture = t.tex;
    out.size = size;
    out.shown = t.tex != 0;
    out.version = ++bindPanel.version;
    return out;
}
