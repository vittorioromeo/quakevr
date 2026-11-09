// vr_gadget_visor.inc -- part of vr_gadget.cpp: the flat-screen visor's panels (vr_gadget.hpp renderVisor; vr_visor.cpp
// shows them). The HUD drawn by the screen's own code (its palette, drawText, the status bar's grey numbers and icons,
// the made-up readings of vr_gadget_test_state) into three small images, so that the visor reads as the wrist gadget:
//
//   vitals (top left)        ammo (top right)            log (low in the middle)
//   [face]  100              100  [ammo]  the main hand   the console's last notify lines, short
//   [armr]   50   (smaller)   25  [ammo]  the off hand    TIME [==========]  bullet time's meter
//   ========== health bar    [keys, powerups, sigils]
//
// Low health (VisorInput::alarm): the vitals panel's face and frame pulse towards red. A power-on (VisorInput::boot): a
// bright line sweeps down each panel, what is under it still dark.

constexpr float visorSizes[VisorPanelCount][2] = {{120.f, 62.f}, {120.f, 62.f}, {240.f, 40.f}};
constexpr int visorTexels = 3; // texels a virtual pixel (crisp at the panels' size on a 1080p screen and over)
constexpr int visorLogRows = 3;
constexpr int visorLogColumns = 33; // 7-pixel characters across the low panel

struct VisorState
{
    za::Array<gfx::Target, VisorPanelCount> targets{};
    za::Array<bool, VisorPanelCount> shown{};
    za::Array<glm::vec2, VisorPanelCount> band{}; // the rows in use (virtual pixels, top and bottom): the low panel's shrink
    int logLines{0};
};
VisorState visor;

// The low panel's lines (the console's notify lines, wrapped), kept from frame to frame.
struct VisorScratch
{
    Lines wrapped;
    za::Vector<za::Pair<za::SizeT, float>> picked; // wrapped's line, its alpha
    NotifyLine line;
    auto members() { return qvr::mem::list(wrapped, picked, line); }
};
mem::Scratch<VisorScratch> visorScratch{"visor"};

// The panel's face and frame (the frame `frame`'s colour), as the gadget's screen.
// From row `top` to row `bottom` (the low panel shows only the rows it uses).
void visorFace(float w, float h, const Palette& pal, const glm::vec3& frame, float top = 0.f)
{
    fill(0.f, 0.f, w, h, pal.background);
    for(int y = 0; y < static_cast<int>(h) && crtStrength() <= 0.f; y += 3)
    {
        fill(0.f, static_cast<float>(y), w, 1.f, pal.scanline);
    }
    fill(0.f, top, w, 1.f, frame);
    fill(0.f, h - 1.f, w, 1.f, frame);
    fill(0.f, top, 1.f, h - top, frame);
    fill(w - 1.f, top, 1.f, h - top, frame);
}

// A power-on sweep `b` (0 .. 1): below the line still dark, the line bright.
void visorBoot(float w, float top, float bottom, float b, const Palette& pal)
{
    if(b >= 1.f)
    {
        return;
    }
    const float y = top + za::floor(CLAMP(0.f, b, 1.f) * (bottom - top));
    fill(0.f, y, w, bottom - y, pal.background * 0.6f);
    fill(0.f, za::max(top, y - 1.f), w, 2.f, glm::min(pal.value * 1.2f, glm::vec3{1.f}));
}

// Health (the face its icon) and armour, a bar under them for the health.
void visorVitals(const Readout& r, const Palette& basePal, float alarm)
{
    const float w = visorSizes[VisorVitals][0], h = visorSizes[VisorVitals][1];
    Palette pal = basePal;
    const glm::vec3 redBack{0.16f, 0.015f, 0.01f};
    pal.background = glm::mix(pal.background, redBack, CLAMP(0.f, alarm, 1.f) * 0.85f);
    visorFace(w, h, pal, glm::mix(pal.line, pal.warn, CLAMP(0.f, alarm * 1.5f, 1.f)));

    const int face = r.health >= 100 ? 4 : CLAMP(0, r.health / 20, 4);
    drawPicSized(5.f, 5.f, PicFace + face, 24.f, 24.f, 24.f, glm::mix(pal.label, pal.warn, CLAMP(0.f, alarm, 1.f)), pal);
    number(34.f, 5.f, r.health, 3, r.health < 25 ? blinking(pal.warn) : pal.value, 1.f, pal);

    if(r.armorType >= 0)
    {
        drawPicSized(8.f, 33.f, PicArmor + r.armorType, 18.f, 18.f, 24.f, pal.label, pal);
    }
    number(52.f, 33.f, r.armor, 3, r.armor > 0 ? pal.value : pal.dim, 0.75f, pal);

    // The bar: a hundred across, over a hundred in white past it (megahealth).
    const float barX = 5.f, barW = w - 10.f, barY = 56.f;
    fill(barX, barY, barW, 2.f, pal.dim * 0.5f);
    const float share = CLAMP(0.f, static_cast<float>(r.health) / 100.f, 1.f);
    fill(barX, barY, za::round(barW * share), 2.f, r.health < 25 ? blinking(pal.warn) : pal.line);
}

// Each hand's ammo (the main hand's big, a clip's reserve small beside it), then the keys, powerups and sigils.
void visorAmmo(const Readout& r, const Palette& pal)
{
    const float w = visorSizes[VisorAmmo][0], h = visorSizes[VisorAmmo][1];
    visorFace(w, h, pal, pal.line);

    for(int hand = 0; hand < 2; hand++)
    {
        const bool main = hand == 0;
        const float s = main ? 1.f : 0.75f, size = 24.f * s, y = main ? 5.f : 33.f;
        const float iconX = w - 5.f - size, numberX = iconX - 4.f - 3.f * size;
        const int type = r.handType[hand];
        int count = r.handAmmo[hand];
        int clip = 0, clipSize = 0;
        const bool clipShown = vr_gadget_test_state.value == 0.f && VR_FlatHudClip(main ? 0 : 1, &clip, &clipSize) != 0;
        if(type >= 0)
        {
            drawPicSized(iconX, y, PicAmmo + type, size, size, 24.f, pal.label, pal);
        }
        if(type < 0 && count <= 0) // (a fist, a melee weapon: nothing to count)
        {
            drawPicSized(numberX + size, y, PicNumMinus, size, size, 24.f, pal.dim, pal);
            drawPicSized(numberX + 2.f * size, y, PicNumMinus, size, size, 24.f, pal.dim, pal);
            continue;
        }
        const int big = clipShown ? clip : count;
        const bool empty = clipShown ? clip <= 0 : count <= 0;
        number(numberX, y, big, 3, empty ? blinking(pal.warn) : pal.value, s, pal);
        if(clipShown)
        {
            // The reserve: in the big number's free places when the clip has two digits or fewer, else under it.
            char reserve[16];
            q_snprintf(reserve, sizeof(reserve), "/%d", CLAMP(0, count, 999));
            const int digits = big >= 100 ? 3 : big >= 10 ? 2 : 1;
            const float textW = static_cast<float>(strlen(reserve)) * 7.f;
            const float left = numberX + static_cast<float>(3 - digits) * size;
            const bool fits = left - 2.f - textW >= 3.f;
            drawText(fits ? left - 2.f - textW : 3.f, fits ? y + size - 7.f : y + size + 1.f, 7.f, reserve,
                count > 0 ? pal.label : pal.dim, pal);
        }
    }

    // The keys, powerups and sigils, small, from the right.
    constexpr int itemBits[10] = {IT_KEY1, IT_KEY2, IT_INVISIBILITY, IT_INVULNERABILITY, IT_SUIT, IT_QUAD,
        1 << 28, 1 << 29, 1 << 30, static_cast<int>(1u << 31)};
    float x = w - 5.f;
    for(int i = 9; i >= 0; i--)
    {
        if(!(r.items & itemBits[i]))
        {
            continue;
        }
        const int pic = i < 2 ? PicKey + i : i < 6 ? PicPowerup + (i - 2) : PicSigil + (i - 6);
        const qpic_t* p = greyPic(pic);
        const float iw = p ? 8.f * static_cast<float>(p->width) / za::max(1.f, static_cast<float>(p->height)) : 8.f;
        x -= iw;
        if(x < 3.f)
        {
            break;
        }
        drawPicSized(x, 52.f, pic, iw, 8.f, p ? static_cast<float>(p->width) : 16.f, i >= 2 && i < 6 ? pal.value : pal.label, pal);
        x -= 2.f;
    }
}

// The console's newest notify lines (as the wrist log takes them: vr_hud_console_log), wrapped short, oldest first;
// false when there are none.
bool visorLog(float life)
{
    VisorScratch& s = visorScratch;
    s.wrapped.clear();
    s.picked.clear();
    if(life <= 0.f)
    {
        return false;
    }
    for(int age = 0; age < 16 && static_cast<int>(s.picked.size()) < visorLogRows; age++)
    {
        if(!notifyLine(age, s.line))
        {
            continue;
        }
        const float alpha = CLAMP(0.f, (life - static_cast<float>(s.line.seconds)) / logFade, 1.f);
        if(alpha <= 0.f)
        {
            break; // older lines are older still
        }
        if(!s.line.game && vr_hud_console_log.value == 0.f)
        {
            continue; // the console's alone
        }
        const za::SizeT first = s.wrapped.size();
        wrap(s.line.text, s.wrapped, visorLogColumns);
        for(za::SizeT i = s.wrapped.size(); i-- > first && static_cast<int>(s.picked.size()) < visorLogRows;)
        {
            s.picked.emplaceBack(i, alpha);
        }
    }
    return !s.picked.empty();
}

// The log's lines and bullet time's meter; false when it shows neither.
bool visorLow(const Readout& r, const Palette& pal, bool logOn)
{
    const float w = visorSizes[VisorLog][0], h = visorSizes[VisorLog][1];
    const float life = static_cast<float>(Cvar_VariableValue("con_notifytime"));
    const bool lines = logOn && visorLog(life);
    visor.logLines = lines ? static_cast<int>(visorScratch.picked.size()) : 0;
    const bullettime::Meter& m = r.time;
    if(!lines && !m.enabled)
    {
        return false;
    }
    // The lines' rows (0..27) and the meter's (26..40), as many as are in use: the meter alone a thin strip.
    const float top = lines ? 0.f : 26.f, bottom = m.enabled ? h : 28.f;
    visor.band[VisorLog] = {top, bottom};
    visorFace(w, bottom, pal, pal.line, top);
    if(lines)
    {
        const za::Vector<za::Pair<za::SizeT, float>>& picked = visorScratch.picked;
        int row = visorLogRows - static_cast<int>(picked.size()); // (the newest at the bottom)
        for(za::SizeT k = picked.size(); k-- > 0; row++)
        {
            const auto& [line, alpha] = picked[k];
            drawText(6.f, 4.f + 8.f * static_cast<float>(row), 7.f, visorScratch.wrapped[line].cStr(),
                glm::mix(pal.background, pal.value, alpha), pal);
        }
    }
    if(m.enabled)
    {
        const float y = 30.f, mh = 6.f, x = 38.f, mw = w - 6.f - x;
        drawText(6.f, y - 0.5f, 7.f, "TIME", m.active ? pal.value : pal.label, pal);
        const glm::vec3 frame = m.active ? (blinkOn() ? pal.value : pal.line)
                              : m.gained ? pal.value
                              : m.cooling ? pal.dim * 0.6f : pal.line;
        meterFrame(x, y, mw, mh, frame);
        if(!m.cooling || m.earned)
        {
            fill(x + 2.f, y + 2.f, (mw - 4.f) * CLAMP(0.f, m.level, 1.f), mh - 4.f, m.active || m.ready ? pal.value : pal.dim);
        }
    }
    return true;
}

void visorRender(const VisorInput& in)
{
    const Palette pal = palette();
    const Readout r = readout();
    for(int p = 0; p < VisorPanelCount; p++)
    {
        const int w = static_cast<int>(visorSizes[p][0]), h = static_cast<int>(visorSizes[p][1]);
        gfx::Target& t = visor.targets[p];
        gfx::ensureTarget(t, w * visorTexels, h * visorTexels, true, "visor panel"); // mipmaps: for its text's glow
        gfx::begin2D(t, w, h);
        bool shown = true;
        visor.band[p] = {0.f, static_cast<float>(h)};
        switch(p)
        {
            case VisorVitals: visorVitals(r, pal, in.alarm); break;
            case VisorAmmo: visorAmmo(r, pal); break;
            default: shown = visorLow(r, pal, in.log); break;
        }
        if(shown)
        {
            visorBoot(static_cast<float>(w), visor.band[p].x, visor.band[p].y, in.boot[p], pal);
        }
        gfx::end2D();
        visor.shown[p] = shown;
    }
}
