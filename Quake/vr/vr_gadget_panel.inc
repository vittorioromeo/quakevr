// vr_gadget_panel.inc -- part of vr_gadget.cpp: panel boards (vr_gadget.hpp): the Survival Arena's banners, a map's board
// drawn by the screen's own code (its palette, drawText, the status bar's numbers, its rules) so that they read as the
// wrist gadget does: to be read from the floor, so each thing's number is big and its label small over it.
//
//   | WAVE | FOES LEFT | TIME   03:21 |
//   |  12  |    14     | -------------|
//   |      | ====bar== | SCORE   4500 |
//
// Between waves the middle says NEXT WAVE and its seconds, or CHOOSE A PERK while the cards hold the rest; at the end of a
// run the wave and the middle give way to YOU FELL (the frame red) or CLEARED under the wave's number.

// The text's numbers (QC/vr_arena_banners.qc VR_ARENA_BannerText writes them).
struct PanelState
{
    int wave{0}, boss{0}, show{0}, value{0}, total{0}, secs{0}, score{0};
};
enum PanelShow : int // (QC's AR_BANNER_SHOW_*)
{
    PanelFoes = 0,
    PanelNext = 1,
    PanelPerk = 2,
    PanelFell = 3,
    PanelCleared = 4
};

// Its one image and what is in it.
struct PanelBoard
{
    gfx::Target target;
    int width{0}; // of its image, 0 before it is drawn
    za::String drawn;
    glm::vec3 drawnValue{-1.f}, drawnLine{-1.f}; // the palette it was drawn in
    int drawnFont{-1};
    int draws{0};
};
PanelBoard panelBoard;

constexpr int panelTexels = 6; // an image's pixels a panel pixel: a board's digit is 53 units, sharp from across the room or a tower's top

constexpr float panelLabelY = 2.f;
constexpr float panelBigY = 11.f; // (the big numbers: 24 x 24, as the gadget's tiles')
constexpr float panelWaveX = 4.f, panelWaveW = 48.f;
constexpr float panelMidX = 58.f, panelMidW = 72.f;
constexpr float panelBarY = 36.f, panelBarH = 2.5f;
constexpr float panelRuleTop = 3.f, panelRuleH = 34.f;
constexpr float panelRule1X = 55.f, panelRule2X = 133.f;
constexpr float panelRowsX = 137.f, panelRowsRight = 246.f;
constexpr float panelRowTop[2] = {3.f, 22.f};
constexpr float panelRowDigit = 15.f;

// The look the boards have: the gadget's palette, not dimmed with the gear lights (they light a room, not a wrist).
[[nodiscard]] Palette panelBoardPalette()
{
    return paletteAt(CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f));
}

void panelCentred(float x, float w, float y, float size, const char* str, const glm::vec3& rgb, const Palette& pal)
{
    drawText(x + (w - size * static_cast<float>(strlen(str))) * 0.5f, y, size, str, rgb, pal);
}

// A big number in the middle of its tile (narrowed, not smaller, when it has more digits than the tile has places).
void panelBig(float x, float w, int value, const glm::vec3& rgb, const Palette& pal)
{
    char str[16];
    q_snprintf(str, sizeof(str), "%d", CLAMP(0, value, 999));
    const float n = static_cast<float>(strlen(str));
    const float each = za::min(24.f, w / n);
    numberRun(x + (w - each * n) * 0.5f, panelBigY, str, each, 24.f, rgb, pal);
}

// A row of the right side: its label, and its value at the right edge, narrowed to the room the label leaves.
void panelRow(int row, const char* label, const char* value, const glm::vec3& rgb, const Palette& pal)
{
    const float y = panelRowTop[row];
    const float labelW = 8.f * static_cast<float>(strlen(label));
    drawText(panelRowsX, y + (panelRowDigit - 8.f) * 0.5f, 8.f, label, pal.label, pal);
    float widths = 0.f;
    for(const char* c = value; *c; c++)
    {
        widths += *c == ':' ? 0.5f : 1.f;
    }
    const float each = za::min(panelRowDigit, (panelRowsRight - panelRowsX - labelW - 3.f) / widths);
    numberRun(panelRowsRight - each * widths, y, value, each, panelRowDigit, rgb, pal);
}

void panelLayout(const PanelState& s, const Palette& pal)
{
    const float width = static_cast<float>(worldtext::panelWidth), height = static_cast<float>(worldtext::panelHeight);
    const bool fell = s.show == PanelFell, cleared = s.show == PanelCleared;
    fill(0.f, 0.f, width, height, pal.background);
    meterFrame(0.f, 0.f, width, height, fell ? pal.warn : cleared ? pal.value : pal.line);

    const char* const waveName = s.boss ? "BOSS" : "WAVE";
    const glm::vec3 waveColor = s.boss ? pal.warn : pal.value;
    if(fell || cleared)
    {
        // The run's end: what happened, big, under the wave it happened on.
        char label[32];
        q_snprintf(label, sizeof(label), "%s %d", waveName, s.wave);
        const float w = panelMidX + panelMidW - panelWaveX;
        panelCentred(panelWaveX, w, panelLabelY, 8.f, label, s.boss ? pal.warn : pal.label, pal);
        const char* const say = fell ? "YOU FELL" : "CLEARED";
        const float size = za::min(18.f, w / static_cast<float>(strlen(say)));
        panelCentred(panelWaveX, w, panelBigY + (24.f - size) * 0.5f, size, say, fell ? pal.warn : pal.value, pal);
    }
    else
    {
        panelCentred(panelWaveX, panelWaveW, panelLabelY, 8.f, waveName, s.boss ? pal.warn : pal.label, pal);
        if(s.wave > 0)
        {
            panelBig(panelWaveX, panelWaveW, s.wave, waveColor, pal);
        }
        else // (before the run's first wave)
        {
            numberRun(panelWaveX + panelWaveW * 0.5f - 16.f, panelBigY, "--", 16.f, 24.f, pal.dim, pal);
        }
        fill(panelRule1X, panelRuleTop, 1.f, panelRuleH, pal.line);
        if(s.show == PanelPerk)
        {
            panelCentred(panelMidX, panelMidW, panelLabelY, 8.f, "CHOOSE A", pal.label, pal);
            panelCentred(panelMidX, panelMidW, panelBigY + 3.f, 18.f, "PERK", pal.value, pal);
        }
        else if(s.show == PanelNext)
        {
            panelCentred(panelMidX, panelMidW, panelLabelY, 8.f, "NEXT WAVE", pal.label, pal);
            panelBig(panelMidX, panelMidW, s.value, s.value <= 3 ? pal.warn : pal.value, pal);
        }
        else
        {
            panelCentred(panelMidX, panelMidW, panelLabelY, 8.f, "FOES LEFT", pal.label, pal);
            panelBig(panelMidX, panelMidW, s.value, s.value > 0 ? pal.value : pal.dim, pal);
            // What's left of the wave, a bar under its number.
            const float left = s.total > 0 ? CLAMP(0.f, static_cast<float>(s.value) / static_cast<float>(s.total), 1.f) : 0.f;
            fill(panelMidX, panelBarY, panelMidW, panelBarH, pal.background * 3.f);
            fill(panelMidX, panelBarY, panelMidW * left, panelBarH, s.boss ? pal.warn : glm::vec3{pal.text});
        }
    }
    fill(panelRule2X, panelRuleTop, 1.f, panelRuleH, pal.line);

    // The run's clock and score: smaller, one over the other.
    char str[24];
    const int secs = CLAMP(0, s.secs, 5999);
    q_snprintf(str, sizeof(str), "%02d:%02d", secs / 60, secs % 60);
    panelRow(0, "TIME", str, secs > 0 ? pal.value : pal.dim, pal);
    fill(panelRowsX, 20.f, panelRowsRight - panelRowsX, 1.f, pal.line);
    q_snprintf(str, sizeof(str), "%d", CLAMP(0, s.score, 9999999));
    panelRow(1, "SCORE", str, s.score > 0 ? pal.value : pal.dim, pal);
}

void drawPanelBoard(const char* text)
{
    const Palette pal = panelBoardPalette();
    const int style = fontstyle::generation(fontstyle::Surface::Gadget);
    if(panelBoard.width > 0 && panelBoard.target.texture && panelBoard.drawn == text && panelBoard.drawnValue == pal.value &&
        panelBoard.drawnLine == pal.line && panelBoard.drawnFont == style)
    {
        return;
    }
    PanelState s;
    if(sscanf(text, "{arena:%d %d %d %d %d %d %d}", &s.wave, &s.boss, &s.show, &s.value, &s.total, &s.secs, &s.score) != 7)
    {
        return;
    }
    QVR_GPU_PROFILE("panel board");
    const fontstyle::Use font{fontstyle::Surface::Gadget};
    gfx::ensureTarget(panelBoard.target, worldtext::panelWidth * panelTexels, worldtext::panelHeight * panelTexels, true, "panel board");
    gfx::begin2D(panelBoard.target, worldtext::panelWidth, worldtext::panelHeight);
    panelLayout(s, pal);
    gfx::end2D();
    panelBoard.width = panelBoard.target.width;
    panelBoard.drawn = text;
    panelBoard.drawnValue = pal.value;
    panelBoard.drawnLine = pal.line;
    panelBoard.drawnFont = style;
    panelBoard.draws++;
}

[[nodiscard]] float readPanelBoard(const char* text, float x, float y, float w, float h, int what)
{
    if(what == 2)
    {
        return panelBoard.width > 0 && panelBoard.target.texture ? 1.f : 0.f;
    }
    if(panelBoard.width <= 0 || !panelBoard.target.framebuffer || panelBoard.drawn != text)
    {
        return -1.f;
    }
    const int width = panelBoard.target.width, height = panelBoard.target.height;
    za::Vector<byte> rgba(static_cast<za::SizeT>(width) * height * 4);
    GLint previous = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previous);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, panelBoard.target.framebuffer);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previous));

    // The part asked (the panel's y runs down; the image's rows up: its first is the panel's bottom).
    const int x0 = CLAMP(0, static_cast<int>(x * panelTexels), width), x1 = CLAMP(0, static_cast<int>((x + w) * panelTexels), width);
    const int y0 = CLAMP(0, height - static_cast<int>((y + h) * panelTexels), height), y1 = CLAMP(0, height - static_cast<int>(y * panelTexels), height);
    const glm::vec3 back = panelBoardPalette().background;
    const float background = za::max(back.r, za::max(back.g, back.b));
    int count = 0, inked = 0;
    float reds = 0.f;
    uint32_t sum = 2166136261U;
    for(int py = y0; py < y1; py++)
    {
        const byte* row = rgba.data() + static_cast<za::SizeT>(py) * width * 4;
        for(int px = x0; px < x1; px++)
        {
            const byte* c = row + px * 4;
            count++;
            for(int k = 0; k < 3; k++)
            {
                sum = (sum ^ c[k]) * 16777619U;
            }
            const float r = static_cast<float>(c[0]) / 255.f, g = static_cast<float>(c[1]) / 255.f, b = static_cast<float>(c[2]) / 255.f;
            if(za::max(r, za::max(g, b)) > background + 0.2f)
            {
                inked++;
                reds += r - g;
            }
        }
    }
    if(what == 1)
    {
        return static_cast<float>((sum ^ (sum >> 24)) & 0xffffff);
    }
    if(what == 3)
    {
        return inked > 0 ? reds / static_cast<float>(inked) : 0.f;
    }
    return count > 0 ? static_cast<float>(inked) / static_cast<float>(count) : 0.f;
}
