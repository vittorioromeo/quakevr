// vr_gadget_visor.inc -- part of vr_gadget.cpp: the flat-screen visor's panels (vr_gadget.hpp renderVisor; vr_visor.cpp
// mounts them on the screen's edges). The HUD drawn by the screen's own code (its palette, drawText, the status bar's grey
// numbers and icons, the made-up readings of vr_gadget_test_state) into seven small images on a black face (left clear on
// the screen: Shade::Screen's inkAlpha; the mount behind is the face), so that the visor reads as the wrist gadget:
//
//   vitals (top left)            status (top right)
//   [face]  100                  TIME  [==========]       bullet time's meter
//   [armr]   50                  STAM  [][][][][][]...    parry stamina (vr_gadget_stamina)
//   ========== health bar        [s] 25 [n] 0 [r] 3 [c] 0 every ammo carried, the held guns' kinds framed; Rogue's
//                                [l]  0 [m] 0 [p] 0       row in the mission pack (or once any is carried)
//   side left: keys, sigils,     side right: powerups and their seconds, AD's artifacts (seconds) and debuffs
//   AD's custom keys
//   left hand (bottom left)      log (bottom middle)      right hand (bottom right)
//   [ammo]  8                    the console's notify      8  [ammo]   the magazine big (warning colours as the
//          /24                   lines, the last three   /24          ammo screens'), the total small and dim
//
// Low health (VisorInput::alarm): the vitals' face and bar pulse towards red. A power-on (VisorInput::boot): a bright line
// sweeps down each panel, what is under it still dark.

constexpr float visorSizes[VisorPanelCount][2] = {
    {120.f, 62.f}, {128.f, 62.f}, {96.f, 36.f}, {96.f, 36.f}, {240.f, 28.f}, {30.f, 120.f}, {30.f, 120.f}};
constexpr int visorTexels = 3; // texels a virtual pixel (crisp at the panels' size on a 1080p screen and over)
constexpr int visorLogRows = 3;
constexpr int visorLogColumns = 33; // 7-pixel characters across the low panel
const glm::vec3 visorBlack{0.f};    // (the face: clear on the screen)

struct VisorState
{
    za::Array<gfx::Target, VisorPanelCount> targets{};
    za::Array<bool, VisorPanelCount> shown{};
    int logLines{0};
    // Each panel's last drawing (its dry run's signature: gfx::draw2D::beginSignature), the texture it went into and how
    // many times it was drawn (VisorImage::version): drawn again only when what it shows changed.
    za::Array<unsigned long long, VisorPanelCount> signatures{};
    za::Array<unsigned, VisorPanelCount> drawnInto{};
    za::Array<unsigned, VisorPanelCount> versions{};
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

// A power-on sweep `b` (0 .. 1): below the line still dark, the line bright.
void visorBoot(float w, float h, float b, const Palette& pal)
{
    if(b >= 1.f)
    {
        return;
    }
    const float y = za::floor(CLAMP(0.f, b, 1.f) * h);
    fill(0.f, y, w, h - y, visorBlack);
    fill(0.f, za::max(0.f, y - 1.f), w, 2.f, glm::min(pal.value * 1.2f, glm::vec3{1.f}));
}

// Health (the face its icon) and armour, a bar under them for the health.
void visorVitals(const Readout& r, const Palette& pal, float alarm)
{
    const float w = visorSizes[VisorVitals][0];
    const float a = CLAMP(0.f, alarm, 1.f);
    const int face = r.health >= 100 ? 4 : CLAMP(0, r.health / 20, 4);
    drawPicSized(6.f, 5.f, PicFace + face, 24.f, 24.f, 24.f, glm::mix(pal.label, pal.warn, a), pal);
    number(36.f, 5.f, r.health, 3, r.health < 25 ? blinking(pal.warn) : pal.value, 1.f, pal);
    if(r.armorType >= 0)
    {
        drawPicSized(9.f, 33.f, PicArmor + r.armorType, 18.f, 18.f, 24.f, pal.label, pal);
    }
    number(54.f, 33.f, r.armor, 3, r.armor > 0 ? pal.value : pal.dim, 0.75f, pal);

    // The bar: a hundred across.
    const float barX = 6.f, barW = w - 12.f, barY = 56.f;
    fill(barX, barY, barW, 2.f, pal.dim * 0.5f);
    const float share = CLAMP(0.f, static_cast<float>(r.health) / 100.f, 1.f);
    fill(barX, barY, za::round(barW * share), 2.f, r.health < 25 ? blinking(pal.warn) : glm::mix(pal.line, pal.warn, a));
}

// The gadget's bars, small: bullet time's meter (as timeRow) and parry stamina's ten cells (as staminaRow, shorter).
void visorBars(const Readout& r, const Palette& pal, float w)
{
    const float x = 40.f, mw = w - 6.f - x;
    const bullettime::Meter& t = r.time;
    drawText(6.f, 4.f, 7.f, "TIME", t.enabled ? (t.active ? pal.value : pal.label) : pal.dim, pal);
    if(t.enabled)
    {
        const glm::vec3 frame = t.active ? (blinkOn() ? pal.value : pal.line)
                              : t.gained ? pal.value
                              : t.cooling ? pal.dim * 0.6f : pal.line;
        meterFrame(x, 4.f, mw, 7.f, frame);
        if(!t.cooling || t.earned)
        {
            fill(x + 2.f, 6.f, (mw - 4.f) * CLAMP(0.f, t.level, 1.f), 3.f, t.active || t.ready ? pal.value : pal.dim);
        }
    }
    else
    {
        meterFrame(x, 4.f, mw, 7.f, pal.dim * 0.5f);
    }

    const meleehud::State& m = r.melee;
    const bool on = vr_gadget_stamina.value != 0.f && (m.stamina || m.counter > 0.f);
    const bool out = on && m.stamina && m.left <= 0.f;
    const char* label = !on ? "STAM" : m.counter > 0.f ? "CNTR" : m.draining ? "HANG" : out ? "OUT" : "STAM";
    drawText(6.f, 15.f, 7.f, label, !on ? pal.dim : out ? blinking(pal.warn) : m.counter > 0.f ? pal.value : pal.label, pal);
    constexpr int cells = 10;
    const float cw = (mw + 2.f) / static_cast<float>(cells);
    const float left = on && m.stamina ? CLAMP(0.f, m.left, 1.f) * static_cast<float>(cells) : 0.f;
    for(int i = 0; i < cells; i++)
    {
        const float cx = x + cw * static_cast<float>(i);
        const float lit = CLAMP(0.f, left - static_cast<float>(i), 1.f);
        meterFrame(cx, 15.f, cw - 2.f, 7.f, on ? pal.line : pal.dim * 0.5f);
        if(lit > 0.f)
        {
            fill(cx + 1.f, 16.f, (cw - 4.f) * lit, 5.f, m.low ? blinking(pal.warn) : pal.value);
        }
    }
    if(on && m.counter > 0.f) // (the counter's window running out: a thin bar under the cells)
    {
        fill(x, 23.f, mw * CLAMP(0.f, m.counter, 1.f), 1.f, pal.value);
    }
}

// Every ammo carried, a grid of four across: the base four, and Rogue's three in the mission pack (or once any is
// carried); the kinds the hands' guns fire framed, none left dim.
void visorAmmoGrid(const Readout& r, const Palette& pal)
{
    const int rogueAmmo[3] = {cl.stats[protocol::STAT_QVR_AMMOLAVA], cl.stats[protocol::STAT_QVR_AMMOMULTI],
        cl.stats[protocol::STAT_QVR_AMMOPLASMA]};
    const bool rogueRow = rogue || rogueAmmo[0] > 0 || rogueAmmo[1] > 0 || rogueAmmo[2] > 0;
    const int held[2] = {cl.stats[protocol::STAT_QVR_AMMOTYPE], cl.stats[protocol::STAT_QVR_AMMO2]}; // (AID_*: 1 .. 7)
    // Each cell: its AID, its count, its icon (the base kind's) and that icon's tint (Rogue's: lava warm, plasma cool).
    const glm::vec3 lava{1.f, 0.55f, 0.25f}, plasma{0.55f, 0.75f, 1.f};
    fill(6.f, 26.f, visorSizes[VisorStatus][0] - 12.f, 1.f, pal.line * 0.6f);
    for(int i = 0; i < (rogueRow ? 7 : 4); i++)
    {
        const int aid = i + 1;
        const int count = i < 4 ? r.pools[i] : rogueAmmo[i - 4];
        const int icon = PicAmmo + (i < 4 ? i : i - 3);
        const float x = 7.f + 29.f * static_cast<float>(i % 4), y = 30.f + 14.f * static_cast<float>(i / 4);
        const glm::vec3 tint = i == 4 ? glm::mix(pal.label, lava, 0.6f) : i == 6 ? glm::mix(pal.label, plasma, 0.6f) : pal.label;
        drawPicSized(x, y + 1.f, icon, 8.f, 8.f, 24.f, count > 0 ? tint : pal.dim, pal);
        char text[8];
        q_snprintf(text, sizeof(text), "%d", CLAMP(0, count, 999));
        drawText(x + 10.f, y + 2.f, 6.f, text, count > 0 ? pal.value : pal.dim, pal);
        if(held[0] == aid || held[1] == aid)
        {
            meterFrame(x - 2.f, y - 1.f, 30.f, 12.f, pal.value);
        }
    }
}

// A hand's warning colour setting (vr_ammo_color_low / _empty, as the ammo screens': vr_view_weapon.cpp ammoWarnColor).
[[nodiscard]] glm::vec3 visorWarnColor(const cvar_t& c)
{
    float r = 0.f, g = 0.f, b = 0.f;
    if(sscanf(c.string, "%f %f %f", &r, &g, &b) == 3)
    {
        return glm::clamp(glm::vec3{r, g, b}, glm::vec3{0.f}, glm::vec3{1.f});
    }
    return hsv(za::fmod(za::fmod(c.value, 360.f) + 360.f, 360.f), 1.f, 1.f);
}

// One hand's weapon (`left`: HAND_OFF, the left hand): the magazine's rounds big (the ammo screens' warning colours),
// the total small and dim under them (vr_ammo_total_scale, _bright); its ammo's icon at the screen's side. An empty hand:
// nothing (the mount stays).
void visorHand(const Readout& r, const Palette& pal, bool left)
{
    const int hand = left ? 1 : 0; // (Readout's: 0 the main hand, the right)
    const int wid = static_cast<int>(cl.stats[left ? protocol::STAT_QVR_WEAPON2 : protocol::STAT_QVR_WEAPON]);
    const bool test = vr_gadget_test_state.value != 0.f;
    if(wid == WID_FIST && !test)
    {
        return;
    }
    const float w = visorSizes[VisorLeftAmmo][0];
    const int type = r.handType[hand], count = r.handAmmo[hand];
    const float iconSize = 20.f, s = 0.75f, size = 24.f * s;
    const float iconX = left ? 6.f : w - 6.f - iconSize;
    const float numberX = left ? iconX + iconSize + 4.f : iconX - 4.f - 3.f * size;
    if(type >= 0)
    {
        drawPicSized(iconX, 8.f, PicAmmo + type, iconSize, iconSize, 24.f, pal.label, pal);
    }
    if(type < 0 && count <= 0) // (a melee weapon: nothing to count)
    {
        drawPicSized(numberX + size, 4.f, PicNumMinus, size, size, 24.f, pal.dim, pal);
        drawPicSized(numberX + 2.f * size, 4.f, PicNumMinus, size, size, 24.f, pal.dim, pal);
        return;
    }
    int clip = 0, clipSize = 0;
    const bool clipShown = !test && VR_FlatHudClip(left ? 1 : 0, &clip, &clipSize) != 0;
    const int loaded = clipShown ? clip : count;
    int warning = 0;
    if(vr_ammo_color_warn.value != 0.f)
    {
        const float frac = CLAMP(0.f, vr_ammo_low_frac.value, 1.f);
        warning = loaded <= 0 ? 2 : clipShown && static_cast<float>(loaded) < frac * static_cast<float>(clipSize) ? 1 : 0;
    }
    const glm::vec3 big = warning == 2 ? blinking(visorWarnColor(vr_ammo_color_empty))
                        : warning == 1 ? visorWarnColor(vr_ammo_color_low) : pal.value;
    number(numberX, 4.f, loaded, 3, big, s, pal);
    if(clipShown)
    {
        char total[16];
        q_snprintf(total, sizeof(total), "/%d", CLAMP(0, count, 999));
        const float ts = CLAMP(5.f, 7.f * vr_ammo_total_scale.value / 0.6f, 10.f);
        const float tw = static_cast<float>(strlen(total)) * ts;
        const glm::vec3 dim = glm::mix(visorBlack, warning ? big : pal.value, CLAMP(0.f, vr_ammo_total_bright.value, 1.f));
        drawText(left ? numberX + 3.f * size - tw : numberX, 4.f + size + 2.f, ts, total, dim, pal);
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

void visorLowLog(const Palette& pal, bool logOn)
{
    const float life = static_cast<float>(Cvar_VariableValue("con_notifytime"));
    const bool lines = logOn && visorLog(life);
    visor.logLines = lines ? static_cast<int>(visorScratch.picked.size()) : 0;
    if(!lines)
    {
        return;
    }
    const za::Vector<za::Pair<za::SizeT, float>>& picked = visorScratch.picked;
    int row = visorLogRows - static_cast<int>(picked.size()); // (the newest at the bottom)
    for(za::SizeT k = picked.size(); k-- > 0; row++)
    {
        const auto& [line, alpha] = picked[k];
        drawText(6.f, 3.f + 8.f * static_cast<float>(row), 7.f, visorScratch.wrapped[line].cStr(),
            glm::mix(visorBlack, pal.value, alpha), pal);
    }
}

// A side mount's column: icons (a key, a sigil; their own aspect, 12 high) or a line of text, each with a small line of
// text under it (a powerup's seconds), from the top; as many as fit.
struct VisorEntry
{
    int pic{-1};
    char text[8]{};
    char note[8]{};
    glm::vec3 rgb{1.f};
};

void visorColumn(const za::Array<VisorEntry, 16>& entries, int count, const Palette& pal)
{
    const float w = visorSizes[VisorSideLeft][0], h = visorSizes[VisorSideLeft][1];
    float y = 5.f;
    for(int i = 0; i < count; i++)
    {
        const VisorEntry& e = entries[i];
        const float step = 14.f + (e.note[0] ? 8.f : 0.f);
        if(y + step > h - 3.f)
        {
            break;
        }
        if(e.pic >= 0)
        {
            const qpic_t* p = greyPic(e.pic);
            const float iw = p ? za::min(w - 6.f, 12.f * static_cast<float>(p->width) / za::max(1.f, static_cast<float>(p->height))) : 12.f;
            drawPicSized(za::round((w - iw) * 0.5f), y, e.pic, iw, 12.f, p ? static_cast<float>(p->width) : 16.f, e.rgb, pal);
        }
        else
        {
            const float tw = static_cast<float>(strlen(e.text)) * 5.f;
            drawText(za::round((w - tw) * 0.5f), y + 3.f, 5.f, e.text, e.rgb, pal);
        }
        if(e.note[0])
        {
            const float nw = static_cast<float>(strlen(e.note)) * 6.f;
            drawText(za::round((w - nw) * 0.5f), y + 13.f, 6.f, e.note, e.rgb, pal);
        }
        y += step;
    }
}

// The left side mount: the keys, the sigils and AD's custom keys.
void visorSideLeft(const Readout& r, const Palette& pal)
{
    za::Array<VisorEntry, 16> e{};
    int n = 0;
    const auto add = [&](int pic, const glm::vec3& rgb) {
        if(n < 16) { e[n].pic = pic; e[n].rgb = rgb; n++; }
    };
    for(int k = 0; k < 2; k++)
    {
        if(r.items & (k == 0 ? IT_KEY1 : IT_KEY2)) { add(PicKey + k, pal.label); }
    }
    for(int s = 0; s < 4; s++)
    {
        if(r.items & static_cast<int>(1u << (28 + s))) { add(PicSigil + s, pal.label); }
    }
    for(int k = 0; k < 4; k++)
    {
        const int pic = PicAdKey + CLAMP(0, (r.adKeys >> (4 * k)) & 15, 13);
        if((r.adHud & (512 << k)) && picOk(pic)) { add(pic, pal.label); }
    }
    visorColumn(e, n, pal);
}

// The right side mount: the powerups with their seconds left (Quake's thirty from the pickup), AD's artifacts with
// theirs, its debuffs (blinking).
void visorSideRight(const Readout& r, const Palette& pal)
{
    za::Array<VisorEntry, 16> e{};
    int n = 0;
    const auto add = [&](int pic, const char* text, int secs, const glm::vec3& rgb) {
        if(n >= 16) { return; }
        e[n].pic = pic;
        q_strlcpy(e[n].text, text ? text : "", sizeof(e[n].text));
        if(secs > 0) { q_snprintf(e[n].note, sizeof(e[n].note), "%d", za::min(secs, 999)); }
        e[n].rgb = rgb;
        n++;
    };
    constexpr int powerups[4] = {IT_INVISIBILITY, IT_INVULNERABILITY, IT_SUIT, IT_QUAD};
    for(int i = 0; i < 4; i++)
    {
        if(!(r.items & powerups[i]))
        {
            continue;
        }
        int bit = 0;
        while((1 << bit) != powerups[i]) { bit++; }
        const int secs = static_cast<int>(30.0 - (cl.time - static_cast<double>(cl.item_gettime[bit])) + 0.999);
        add(PicPowerup + i, nullptr, vr_gadget_test_state.value != 0.f ? 27 - 5 * i : CLAMP(0, secs, 30),
            secs > 0 && secs <= 5 ? blinking(pal.warn) : pal.value);
    }
    for(int i = 0; i < 6; i++)
    {
        if(!(r.adHud & (1 << i)))
        {
            continue;
        }
        const int secs = (r.adArt[i / 3] >> (8 * (i % 3))) & 255;
        add(picOk(PicAdArtifact + i) ? PicAdArtifact + i : -1, "ART", secs > 0 && secs < 255 ? secs : 0,
            secs > 0 && secs <= 5 ? blinking(pal.warn) : pal.value);
    }
    constexpr const char* debuffs[3] = {"BURN", "POIS", "BLEED"};
    for(int d = 0; d < 3; d++)
    {
        if(r.adHud & (64 << d)) { add(-1, debuffs[d], 0, blinking(pal.warn)); }
    }
    visorColumn(e, n, pal);
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
        const auto contents = [&] {
            fill(0.f, 0.f, static_cast<float>(w), static_cast<float>(h), visorBlack);
            switch(p)
            {
                case VisorVitals: visorVitals(r, pal, in.alarm); break;
                case VisorStatus:
                    visorBars(r, pal, static_cast<float>(w));
                    visorAmmoGrid(r, pal);
                    break;
                case VisorLeftAmmo: visorHand(r, pal, true); break;
                case VisorRightAmmo: visorHand(r, pal, false); break;
                case VisorLog: visorLowLog(pal, in.log); break;
                case VisorSideLeft: visorSideLeft(r, pal); break;
                default: visorSideRight(r, pal); break;
            }
            visorBoot(static_cast<float>(w), static_cast<float>(h), in.boot[p], pal);
        };
        // What it would draw now, as a dry run: the same as last time into the same texture, nothing to do (its text,
        // numbers and pictures change a few times a second at most; the GPU's fill and mipmaps were most of the visor's
        // cost drawn every frame).
        gfx::draw2D::beginSignature();
        contents();
        const unsigned long long signature = gfx::draw2D::endSignature();
        if(visor.shown[p] && visor.signatures[p] == signature && visor.drawnInto[p] == t.texture && vr_flat_visor_cache.value != 0.f)
        {
            continue;
        }
        visor.signatures[p] = signature;
        visor.drawnInto[p] = t.texture;
        visor.versions[p]++;
        gfx::begin2D(t, w, h);
        contents();
        gfx::end2D();
        visor.shown[p] = true;
    }
}
