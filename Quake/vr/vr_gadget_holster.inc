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
    gfx::Target target;
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
    default: break;
    }
}

// Text `size` pixels a character, its middle at cx.
void holsterTextMid(float cx, float y, float size, const char* s, const glm::vec3& c, const Palette& pal)
{
    drawText(cx - static_cast<float>(strlen(s)) * size * 0.5f, y, size, s, c, pal);
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
    holsterTextMid(r.x + r.z * 0.5f, r.y + 3.f, 6.f, s.title, pal.label, pal);
    const float picSize = za::min(r.z - 8.f, r.w - 22.f);
    if(!empty && s.model[0] && picSize > 4.f)
    {
        if(const gfx::Picture* pic = toolgun::icons::get(s.model, 0, true))
        {
            // On a lit plate, brightened: Quake's dark guns on the dark face read as nothing (Shade::Screen's inkAlpha
            // leaves the dark clear).
            const float px = r.x + (r.z - picSize) * 0.5f, py = r.y + 11.f;
            const float k = source || invalid ? 0.35f : 1.f; // (dragged away, or greyed)
            fill(px, py + picSize * 0.2f, picSize, picSize * 0.6f, pal.label * 0.45f * k);
            gfx::draw2D::color(glm::vec4{1.8f * k, 1.8f * k, 1.8f * k, 1.f});
            gfx::draw2D::picture(*pic, px, py, picSize, picSize);
            gfx::draw2D::color(glm::vec4{1.f});
        }
    }
    // The name as large as fits (6 pixels a character at most).
    const char* name = empty ? "- EMPTY -" : s.name;
    const float size = za::min(6.f, (r.z - 4.f) / static_cast<float>(za::max<size_t>(strlen(name), 1)));
    holsterTextMid(r.x + r.z * 0.5f, r.y + r.w - 3.f - size, size, name, empty || invalid || source ? pal.dim : pal.value, pal);
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
    gfx::Target& t = holsterMenu.target;
    gfx::ensureTarget(t, w * visorTexels, h * visorTexels, true, "holster menu"); // mipmaps: for its text's glow
    gfx::begin2D(t, w, h);
    fill(0.f, 0.f, v.size.x, v.size.y, visorBlack);
    holsterOutline({1.f, 1.f, v.size.x - 2.f, v.size.y - 2.f}, pal.line * 0.6f);
    drawText(6.f, 4.f, 8.f, v.header, pal.label, pal);
    fill(6.f, 13.f, v.size.x - 12.f, 1.f, pal.line * 0.5f);
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
    out.texture = t.texture;
    out.size = v.size;
    out.shown = t.texture != 0;
    out.version = ++holsterMenu.version;
    return out;
}
