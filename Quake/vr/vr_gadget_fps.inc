// vr_gadget_fps.inc -- the wrist gadget's FPS counter (vr_gadget_fps): figures, graphs, its image and placement.
// Split from vr_gadget.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// ---------------------------------------------------------------------------------------------
// The FPS counter (vr_gadget_fps), floating just under the gadget as seen, facing the viewer as the hologram over it
// does, in the screen's colour (the hologram's shade with a little of its look: faint scanlines, no glitches, so that
// the figures read steadily), characters about 5 mm; the words dimmer than the figures. Drawn into a small image when
// it changes.
//
// 1, Basic: " 90 FPS CPU  5.2 GPU  8.1", the frames' over the last half second (qvr::frameRate: the memory log's
// busy_ms and gpu_eyes_ms), drawn again when the figures change (twice a second).
//
// 2, Detailed (as fpsVR's), from the frames one by one (profile::frameSample, the same phases):
//
//      90 FPS   90 HZ   0 LATE       the rate over the last quarter second; the headset's refresh; the frames in the
//   MS   NOW  AVG  MIN  MAX          last 5 s that missed a refresh (their period over 1.25 refreshes: the runtime
//   CPU  5.2  5.1  4.8  7.9          showed the previous frame again, reprojected)
//   GPU  8.1  8.0  7.6 12.3          the last frame; the last quarter second's average; the last 5 s's least and most
//   CPU __.___|___                    the last 3 s, each column the worst frame over it, up to twice the budget (a frame
//   GPU ___.______                    period); the dotted line is the budget, and what is over it is brighter
//
// Figures over the budget are on a lit block. The figures change four times a second, the graphs twenty.

constexpr int fpsTexels = 4;
constexpr float fpsCharSize = 0.17f; // model units a character is tall (about 5 mm at the default size)
constexpr float fpsGap = 0.25f;      // model units under the gadget as seen

// Basic.
constexpr int fpsColumns = 25;
constexpr int fpsBasicWidth = fpsColumns * 8 + holoPad * 2; // font pixels
constexpr int fpsBasicHeight = 8 + holoPad * 2;

// Detailed.
constexpr int fpsDetailColumns = 26;
constexpr int fpsRowPitch = 10;       // font pixels a text row takes
constexpr int fpsTextRows = 4;
constexpr int fpsStrip = 16;          // a graph's height
constexpr int fpsStripGap = 3;
constexpr int fpsLabel = 3 * 8 + 4;   // a graph's label and the space after it
constexpr int fpsDetailWidth = fpsDetailColumns * 8 + holoPad * 2;
constexpr int fpsGraphWidth = fpsDetailColumns * 8 - fpsLabel;
constexpr int fpsDetailHeight = holoPad * 2 + fpsTextRows * fpsRowPitch + 2 + fpsStrip * 2 + fpsStripGap;
constexpr double fpsAverageTime = 0.25; // seconds: the rate, AVG
constexpr double fpsRangeTime = 5.0;    // MIN, MAX, LATE
constexpr double fpsGraphTime = 3.0;
constexpr double fpsFiguresEvery = 0.25;
constexpr double fpsGraphEvery = 0.05;
constexpr float fpsLateOver = 1.25f;    // periods over this many refreshes missed one (as the memory log's slow frames)
constexpr float fpsAssumedHz = 90.f;    // the budget when the runtime doesn't tell its refresh (the mock)

gfx::Target fpsTarget;
za::String fpsDrawn; // Basic: the text the image holds
int fpsDrawnMode = 0; // the mode the image is of (0: none)
int fpsWidth = fpsBasicWidth, fpsHeight = fpsBasicHeight; // the image's, in font pixels

struct FpsFrame
{
    int frame{-1};
    za::Vector<gfx::Vertex> quad;
    float time{0.f};
};
FpsFrame fps;

[[nodiscard]] int fpsMode()
{
    return static_cast<int>(CLAMP(0.f, vr_gadget_fps.value, 2.f));
}

[[nodiscard]] bool fpsOn()
{
    return fpsMode() != 0;
}

// Detailed: one series' figures (milliseconds; <0 unknown).
struct FpsSeries
{
    float now{-1.f}, avg{-1.f}, min{-1.f}, max{-1.f};
};

struct FpsFigures
{
    float fps{-1.f};
    float hz{0.f};     // the runtime's refresh (0: not told)
    float budget{0.f}; // ms: a refresh period
    int late{0};
    FpsSeries cpu, gpu;
};

FpsFigures fpsFigures;
double fpsFiguresAt = -1.0, fpsGraphAt = -1.0; // realtime each was last made

// The frames kept (profile::frameSample), for the figures.
void measureFps(FpsFigures& f)
{
    f = FpsFigures{};
    const double period = profile::displayPeriodMs();
    f.hz = period > 0.0 ? static_cast<float>(1000.0 / period) : 0.f;
    f.budget = period > 0.0 ? static_cast<float>(period) : 1000.f / fpsAssumedHz;

    profile::FrameSample newest;
    if(!profile::frameSample(0, newest))
    {
        return;
    }
    struct Acc
    {
        double sum{0.0};
        int n{0};
        float min{1e9f}, max{-1.f}, now{-1.f};
        void add(float v, bool recent)
        {
            if(v < 0.f)
            {
                return;
            }
            now = now < 0.f ? v : now;
            min = za::min(min, v);
            max = za::max(max, v);
            if(recent)
            {
                sum += v;
                n++;
            }
        }
        [[nodiscard]] FpsSeries series() const
        {
            return n > 0 ? FpsSeries{now, static_cast<float>(sum / n), min, max} : FpsSeries{};
        }
    };
    Acc cpu, gpu;
    double periods = 0.0;
    int frames = 0;
    profile::FrameSample fs;
    for(int back = 0; profile::frameSample(back, fs) && newest.time - fs.time <= fpsRangeTime; back++)
    {
        const bool recent = newest.time - fs.time < fpsAverageTime;
        cpu.add(fs.cpuMs, recent);
        gpu.add(fs.gpuMs, recent || gpu.n == 0); // (the newest few aren't read back yet)
        if(recent)
        {
            periods += fs.periodMs;
            frames++;
        }
        f.late += fs.periodMs > fpsLateOver * f.budget ? 1 : 0;
    }
    f.fps = periods > 0.0 ? static_cast<float>(1000.0 * frames / periods) : -1.f;
    f.cpu = cpu.series();
    f.gpu = gpu.series();
}

// A figure in five characters (" 12.3", " 123", "    -").
void formatMs(char (&out)[8], float ms)
{
    if(ms < 0.f)
    {
        q_strlcpy(out, "    -", sizeof(out));
    }
    else if(ms < 99.95f)
    {
        q_snprintf(out, sizeof(out), "%5.1f", ms);
    }
    else
    {
        q_snprintf(out, sizeof(out), "%5d", za::min(static_cast<int>(za::lround(ms)), 9999));
    }
}

// Detailed: the image (figures and graphs).
void renderFpsDetailed()
{
    const double now = realtime;
    const bool figures = fpsFiguresAt < 0.0 || now < fpsFiguresAt || now - fpsFiguresAt >= fpsFiguresEvery;
    const bool graph = figures || now < fpsGraphAt || now - fpsGraphAt >= fpsGraphEvery;
    if(fpsDrawnMode == 2 && fpsTarget.texture && !graph)
    {
        return;
    }
    if(figures)
    {
        measureFps(fpsFigures);
        fpsFiguresAt = now;
    }
    fpsGraphAt = now;
    fpsDrawnMode = 2;
    fpsWidth = fpsDetailWidth;
    fpsHeight = fpsDetailHeight;
    const FpsFigures& f = fpsFigures;

    QVR_GPU_PROFILE("gadget fps");
    gfx::ensureTarget(fpsTarget, fpsWidth * fpsTexels, fpsHeight * fpsTexels, true, "gadget fps");
    gfx::begin2D(fpsTarget, fpsWidth, fpsHeight);
    fill(0.f, 0.f, static_cast<float>(fpsWidth), static_cast<float>(fpsHeight), glm::vec3{0.f});
    const glm::vec4 word{0.6f, 0.6f, 0.6f, 1.f};
    const glm::vec3 dim{0.45f};
    const float x0 = static_cast<float>(holoPad);
    float x = x0, y = static_cast<float>(holoPad);
    const auto part = [&](const char* s, const glm::vec4& c, bool over = false) {
        const float w = 8.f * static_cast<float>(ZA_STRLEN(s));
        if(over)
        {
            // The figure (not its leading spaces) on a lit block (dim: the glow round it stays soft).
            const char* t = s;
            while(*t == ' ')
            {
                t++;
            }
            const float lead = 8.f * static_cast<float>(t - s);
            fill(x + lead - 1.f, y - 1.f, w - lead + 2.f, 10.f, glm::vec3{0.3f});
        }
        gfx::draw2D::color(c);
        gfx::draw2D::text(x, y, 8.f, s);
        x += w;
    };
    const auto newRow = [&] {
        x = x0;
        y += static_cast<float>(fpsRowPitch);
    };

    char a[8], b[8], c[8];
    q_snprintf(a, sizeof(a), f.fps >= 0.f ? "%3d" : "  -", CLAMP(0, static_cast<int>(za::lround(f.fps)), 999));
    q_snprintf(b, sizeof(b), f.hz > 0.f ? "%5d" : "    -", CLAMP(0, static_cast<int>(za::lround(f.hz)), 999));
    q_snprintf(c, sizeof(c), "%4d", za::min(f.late, 9999));
    part(a, white);
    part(" FPS", word);
    part(b, white);
    part(" HZ", word);
    part(c, white, f.late > 0);
    part(" LATE", word);
    newRow();
    part(" MS  NOW  AVG  MIN  MAX", word);
    for(const auto& [label, series] : {za::Pair{"CPU", &f.cpu}, za::Pair{"GPU", &f.gpu}})
    {
        newRow();
        part(label, word);
        for(const float v : {series->now, series->avg, series->min, series->max})
        {
            char t[8];
            formatMs(t, v);
            part(t, white, v > f.budget);
        }
    }

    // The graphs: the last fpsGraphTime seconds, newest at the right; a column the worst frame over it.
    const float top = static_cast<float>(holoPad + fpsTextRows * fpsRowPitch + 2);
    float cpuCol[fpsGraphWidth], gpuCol[fpsGraphWidth];
    za::fill(cpuCol, -1.f);
    za::fill(gpuCol, -1.f);
    profile::FrameSample newest, fs;
    if(profile::frameSample(0, newest))
    {
        const double column = fpsGraphTime / fpsGraphWidth;
        for(int back = 0; profile::frameSample(back, fs); back++)
        {
            // Over the columns its period spans (a hitch is as wide as it lasted).
            const double end = newest.time + newest.periodMs * 1e-3;
            const int first = fpsGraphWidth - 1 - static_cast<int>((end - fs.time) / column);
            const int last = fpsGraphWidth - 1 - static_cast<int>((end - fs.time - fs.periodMs * 1e-3) / column);
            if(last < 0)
            {
                break;
            }
            for(int col = za::max(first, 0); col <= za::min(last, fpsGraphWidth - 1); col++)
            {
                cpuCol[col] = za::max(cpuCol[col], fs.cpuMs);
                gpuCol[col] = za::max(gpuCol[col], fs.gpuMs);
            }
        }
    }
    const float gx = x0 + static_cast<float>(fpsLabel);
    for(int strip = 0; strip < 2; strip++)
    {
        const float sy = top + static_cast<float>(strip * (fpsStrip + fpsStripGap));
        x = x0;
        y = sy + static_cast<float>(fpsStrip - 8) * 0.5f;
        part(strip == 0 ? "CPU" : "GPU", word);
        const float* v = strip == 0 ? cpuCol : gpuCol;
        const float half = static_cast<float>(fpsStrip) * 0.5f; // the budget's height
        const float bottom = sy + static_cast<float>(fpsStrip);
        for(int i = 0; i < fpsGraphWidth; i++)
        {
            if(v[i] < 0.f)
            {
                continue;
            }
            const float h = za::max(1.f, za::min(v[i] / f.budget, 2.f) * half);
            const float low = za::min(h, half);
            fill(gx + static_cast<float>(i), bottom - low, 1.f, low, dim);
            if(h > half)
            {
                // Over the budget: brighter, its top lit (a solid lit block would glow too much in the bloom).
                fill(gx + static_cast<float>(i), bottom - h, 1.f, h - half, glm::vec3{0.65f});
                fill(gx + static_cast<float>(i), bottom - h, 1.f, 1.f, glm::vec3{1.f});
            }
        }
        for(int i = 0; i < fpsGraphWidth; i += 4) // the budget, dotted
        {
            fill(gx + static_cast<float>(i), bottom - half - 0.5f, 2.f, 1.f, glm::vec3{0.8f});
        }
        fill(gx, bottom, static_cast<float>(fpsGraphWidth), 0.5f, dim); // the baseline
    }
    gfx::draw2D::color(white);
    gfx::end2D();
}

// The image, when it changes (end of the 2D pass).
void renderFps()
{
    const int mode = fpsMode();
    if(mode == 0)
    {
        fpsDrawn.clear();
        fpsDrawnMode = 0;
        return;
    }
    if(mode == 2)
    {
        fpsDrawn.clear();
        renderFpsDetailed();
        return;
    }
    FrameRate r;
    const bool known = frameRate(r);
    char figures[3][8];
    q_snprintf(figures[0], sizeof(figures[0]), known ? "%3d" : "  -", CLAMP(0, static_cast<int>(za::lround(r.fps)), 999));
    q_snprintf(figures[1], sizeof(figures[1]), known ? "%5.1f" : "    -", za::min(r.cpuMs, 999.9f));
    q_snprintf(figures[2], sizeof(figures[2]), known && r.gpuMs >= 0.f ? "%5.1f" : "    -", za::min(r.gpuMs, 999.9f));
    const za::String text = za::String{figures[0]} + figures[1] + figures[2];
    if(text == fpsDrawn && fpsDrawnMode == 1 && fpsTarget.texture)
    {
        return;
    }
    fpsDrawn = text;
    fpsDrawnMode = 1;
    fpsWidth = fpsBasicWidth;
    fpsHeight = fpsBasicHeight;

    QVR_GPU_PROFILE("gadget fps");
    gfx::ensureTarget(fpsTarget, fpsWidth * fpsTexels, fpsHeight * fpsTexels, true, "gadget fps");
    gfx::begin2D(fpsTarget, fpsWidth, fpsHeight);
    fill(0.f, 0.f, static_cast<float>(fpsWidth), static_cast<float>(fpsHeight), glm::vec3{0.f});
    const glm::vec4 word{0.6f, 0.6f, 0.6f, 1.f};
    float x = static_cast<float>(holoPad);
    const auto part = [&](const char* s, const glm::vec4& c) {
        gfx::draw2D::color(c);
        gfx::draw2D::text(x, static_cast<float>(holoPad), 8.f, s);
        x += 8.f * static_cast<float>(ZA_STRLEN(s));
    };
    part(figures[0], white);
    part(" FPS CPU", word);
    part(figures[1], white);
    part(" GPU", word);
    part(figures[2], white);
    gfx::draw2D::color(white);
    gfx::end2D();
}

// Its quad this frame (once, facing the first view drawn): centred under the gadget as seen, while the screen faces
// the viewer (as the hologram and the log).
void layoutFps()
{
    if(fps.frame == host_framecount)
    {
        return;
    }
    fps.frame = host_framecount;
    fps.quad.clear();
    if(!fpsOn() || !current.valid || !fpsTarget.texture || fpsDrawnMode != fpsMode())
    {
        return;
    }
    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float shown = facing(eye);
    if(shown <= 0.f)
    {
        return;
    }
    const float scale = current.scale;
    const glm::vec3 top = screenCentre() - up * (gadgetTop(-up) + fpsGap * scale);
    const float px = fpsCharSize * scale / 8.f; // a font pixel
    const float w = static_cast<float>(fpsWidth) * px;
    const float h = static_cast<float>(fpsHeight) * px;
    const glm::vec3 tl = top - right * (w * 0.5f);
    const glm::vec3 tr = tl + right * w;
    const glm::vec3 br = tr - up * h;
    const glm::vec3 bl = tl - up * h;
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    const glm::vec4 color{hue::color(vr_gadget_screen_hue, 0.5f, 0.95f * za::max(bright, 0.3f)), shown};
    const gfx::Vertex v[4] = {{bl, {0.f, 0.f}, color}, {br, {1.f, 0.f}, color}, {tr, {1.f, 1.f}, color}, {tl, {0.f, 1.f}, color}};
    for(const gfx::Vertex* p : {&v[0], &v[1], &v[2], &v[0], &v[2], &v[3]})
    {
        fps.quad.pushBack(*p);
    }
    fps.time = static_cast<float>(za::fmod(realtime, 1000.0));
}

void drawFps()
{
    layoutFps();
    if(fps.quad.empty())
    {
        return;
    }
    QVR_GPU_PROFILE("gadget fps");
    // Not depth tested: over the other hand, as the hologram.
    gfx::draw(fps.quad, gfx::sceneViewProjection(),
        {.shade = gfx::Shade::Hologram, .blend = gfx::Blend::Premultiplied, .depthTest = false, .depthWrite = false,
            .params = {fps.time, 0.35f * za::min(hologramEffect(), 1.f), 0.f, 0.f}, .screen = {static_cast<float>(fpsWidth), static_cast<float>(fpsHeight), 0.5f}},
        fpsTarget.texture);
}
