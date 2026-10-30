// vr_gadget_holo.inc -- the wrist gadget's message hologram (vr_messages_hologram): messages, the notification, its image and placement.
// Split from vr_gadget.cpp (a move): included where it was, so it shares that file's (anonymous-namespace) state.

// ---------------------------------------------------------------------------------------------
// The hologram (vr_messages_hologram, vr_gadget.hpp).

// A console line, plain (the coloured characters' high bit off, trailing spaces trimmed): how old it
// is, and whether it is the game's (a server print that is not the engine's own reply, engineLine).
struct NotifyLine
{
    za::String text;
    double seconds{0.0};
    bool game{false};
};

[[nodiscard]] za::SizeT heldBytes(const NotifyLine& l) // (vr_mem.hpp)
{
    return mem::heldBytes(l.text);
}

// The hologram's and the wrist log's buffers, laid out again each frame (the main thread).
struct GadgetScratch
{
    Lines messageLines;                               // a message's lines (makeMessage)
    za::Vector<bool> keep;                           // the queued messages shown (the hologram's queue)
    za::String hologramKey;                          // the shown messages' text: its image's key
    za::String hologramLine;                         // a line drawn into the image
    za::Vector<bool> used;                           // the image's blocks drawn this frame
    Lines wrapped;                                    // the wrist log's lines (notifyLines: out.lines views them)
    za::Vector<za::Pair<za::SizeT, float>> picked; // those shown: wrapped's line, its alpha
    NotifyLine notifyLine;                            // a console line read
    za::String plain;                                // a line to check (VR_GameLineOnWrist)
    auto members() { return qvr::mem::list(messageLines, keep, hologramKey, hologramLine, used, wrapped, picked, notifyLine, plain); }
};
mem::Scratch<GadgetScratch> scratch{"gadget"};

// Whether a server print's line is the engine's own reply rather than the game's (the progs'): the
// server's banner, a cheat's "godmode ON", setpos's and ping's figures, a server cvar changed, a pause.
[[nodiscard]] bool engineLine(za::StringView s)
{
    while(s.startsWith(' '))
    {
        s.removePrefix(1);
    }
    constexpr za::StringView prefixes[] = {"VERSION ", "usage:", "current values:", "Client ping times",
        "Can't suicide", "Pause not allowed", "Kicked by"};
    for(const za::StringView p : prefixes)
    {
        if(s.startsWith(p))
        {
            return true;
        }
    }
    if(s.find("\" changed to \"") != za::StringView::nPos || s.endsWith(" paused the game") ||
        s.endsWith(" unpaused the game"))
    {
        return true;
    }
    // "godmode ON", "noclip OFF": a word, then ON or OFF.
    const size_t space = s.find(' ');
    if(space != za::StringView::nPos && s.find(' ', space + 1) == za::StringView::nPos)
    {
        const za::StringView word = s.substrByPosLen(space + 1);
        if(word == "ON" || word == "OFF")
        {
            return true;
        }
    }
    // Figures only.
    return !za::anyOf(s.begin(), s.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); });
}

// The console's line `age` back from the newest, if it is a notify line (Con_NotifyLine).
[[nodiscard]] bool notifyLine(int age, NotifyLine& out)
{
    const char* text = nullptr;
    int length = 0;
    int server = 0;
    if(!Con_NotifyLine(age, &text, &length, &out.seconds, &server))
    {
        return false;
    }
    while(length > 0 && (text[length - 1] & 127) == ' ')
    {
        length--;
    }
    out.text.assign(text, static_cast<za::SizeT>(length));
    for(char& c : out.text)
    {
        c = static_cast<char>(static_cast<unsigned char>(c) & 127);
    }
    out.game = server && !engineLine(out.text);
    return true;
}

// Its image: the messages' lines in the console font, white on black, on a virtual screen of font
// pixels (holoTexels texels each, with mipmaps for the glow), each message a block of its own (its
// lines centred), oldest at the top; drawn again only when the messages change.
constexpr int holoWidth = 336; // 40 characters and the room round them
constexpr int holoHeight = 128;
constexpr int holoTexels = 4;
constexpr int holoPad = 4;      // font pixels round a block's text (its glow)
constexpr int holoColumns = 40; // a line's characters at most
constexpr int holoRows = 10;    // lines at most (the oldest messages go)
constexpr int holoMessagesMax = 6;
constexpr float holoFadeIn = 0.15f, holoFadeOut = 0.6f;
constexpr float holoOpen = 0.35f; // seconds it takes to project (grow out of the screen)
constexpr float holoGrow = 0.2f;  // and a new message to

struct HoloMessage
{
    za::String text; // its lines, joined by '\n'
    int columns{0}, rows{0};
    double start{0.0};    // realtime its life counts from: printed (or last repeated); held, when first seen
    double appeared{0.0}; // realtime it first was (held: first seen): its fade-in and growth
    double printed{0.0};  // a console line's print time, which it is taken by (0: a centre print)
    int generation{-1};   // the map's (worldGeneration): a new map's messages start afresh
    unsigned id{0};
    bool centre{false};
    bool notify{false}; // a notification (anything but a pickup): see vr_messages_hologram_only
    bool seen{false};   // shown to the player once (the hologram facing them, at least half faded in)
    bool tip{false};    // a tip (gadget::tip): waits to be seen, as with vr_messages_hologram_only
    float life{0.f};    // seconds it shows once seen (0: vr_messages_hologram_time)
};

// A block of the image.
struct HoloBlock
{
    za::String text;
    int columns{0}, rows{0};
    float v0{0.f}, v1{0.f}; // its texture rows (v up: v0 its bottom)
};

za::Vector<HoloMessage> queue;        // the game's messages, oldest first
double newestLine = 0.0;               // the newest console line taken into it (its print time)
unsigned nextId = 1;
za::String latestCentre;              // the latest centre print's text (SCR_CenterPrint's)
za::Vector<HoloMessage> holoMessages; // this frame's, oldest first
int holoCollected = -1;                // the host frame they were collected in
gfx::Target holoTarget;
za::Vector<HoloBlock> holoDrawn; // what the image holds
za::String holoDrawnKey;

// Laid out once a frame (layoutHologram), for both eyes.
struct HoloFrame
{
    int frame{-1};
    za::Vector<gfx::Vertex> text, beam;
    glm::vec4 params{0.f};
    float top{0.f};            // world units its top is over the screen's centre (the view's up); 0: not shown
    bool centreShown{false};   // it shows the centre print
    double lastShown{-100.0};  // realtime it was last shown
    double opened{-100.0};     // realtime it last appeared: its projection's start
    float shown{0.f};          // how much the gadget is in view and faces the viewer (0..1), this frame
    int shownFrame{-100};      // the frame of that
};
HoloFrame holo;
float logLift = -1.f; // the log's lift, following the hologram's top smoothly (world units; -1 not yet)

[[nodiscard]] bool hologramOn()
{
    return vr_messages_hologram.value != 0.f && active() && !recordingClean(); // (recording mode: none)
}

[[nodiscard]] float hologramLife()
{
    return za::max(1.f, vr_messages_hologram_time.value);
}

[[nodiscard]] float hologramEffect()
{
    return CLAMP(0.f, vr_messages_hologram_effect.value, 2.f);
}

// vr_messages_hologram_only: the game's messages are the hologram's alone (never in front of the head), and a
// notification waits in it until seen, announced by a chime from the gadget and a buzz of its hand.
[[nodiscard]] bool hologramOnly()
{
    return vr_messages_hologram_only.value != 0.f && hologramOn();
}

// How long a notification waits to be seen at most (a stale one is no use).
constexpr double heldMax = 300.0;

[[nodiscard]] bool held(const HoloMessage& m)
{
    return m.notify && !m.seen && (hologramOnly() || m.tip);
}

[[nodiscard]] double lifeOf(const HoloMessage& m)
{
    return m.life > 0.f ? static_cast<double>(m.life) : static_cast<double>(hologramLife());
}

[[nodiscard]] bool alive(const HoloMessage& m)
{
    return m.generation == worldGeneration() && (held(m) ? realtime - m.appeared < heldMax : realtime - m.start < lifeOf(m));
}

// Whether the gadget was in view and facing the viewer last frame (the hologram's `shown`, at least half).
[[nodiscard]] bool gadgetInView()
{
    return holo.shownFrame >= host_framecount - 1 && holo.shown >= 0.5f;
}

// A pickup's print ("You got the Grenade Launcher", "You get 20 shells", "You receive 25 health", "You got armor"): the
// thing is in the hand already, so not a notification.
[[nodiscard]] bool pickupLine(za::StringView s)
{
    constexpr za::StringView prefixes[] = {"You got ", "You get ", "You receive "};
    return za::anyOf(prefixes, prefixes + za::getArraySize(prefixes), [&](za::StringView p) { return s.startsWith(p); });
}

// `text` as a message: plain, its lines wrapped to holoColumns, the empty ones at its ends left out.
void makeMessage(za::StringView text, HoloMessage& m)
{
    Lines& lines = scratch.messageLines;
    lines.clear();
    while(!text.empty())
    {
        const size_t end = za::min(text.find('\n'), text.size());
        const size_t before = lines.size();
        wrap(text.substrByPosLen(0, end), lines, holoColumns);
        if(lines.size() == before)
        {
            (void)lines.add(); // an empty line
        }
        text.removePrefix(za::min(end + 1, text.size()));
    }
    while(!lines.empty() && lines.back().empty())
    {
        lines.popBack();
    }
    size_t first = 0;
    while(first < lines.size() && lines[first].empty())
    {
        first++;
    }
    m.text.clear();
    m.columns = 0;
    m.rows = 0;
    for(size_t i = first; i < lines.size(); i++)
    {
        if(i > first)
        {
            m.text += '\n';
        }
        m.text += lines[i];
        m.columns = za::max(m.columns, static_cast<int>(lines[i].size()));
        m.rows++;
    }
}

// ---- The notification (vr_messages_hologram_only): a chime from the gadget and a buzz of its hand ----------------

// The chime is Quake's message sound (misc/talk.wav, the one the maps' messages come with), played at the gadget's
// screen on an entity number of its own and kept there as the arm moves (setPose): heard from the wrist.
constexpr int chimeEntity = -0x5C12;
double lastChime = -100.0;

// A screen tap's feedback (tapFeedback): its click from the screen, on an entity number of its own (kept at the gadget
// as the chime is), and the screen's glitch.
constexpr int tapEntity = -0x5C13;
struct TapFeedback
{
    double soundAt = -100.0; // realtime the click started
    double glitchAt = -100.0; // realtime the glitch started
    float glitchStrength = 0.f;
    float glitchLength = 0.f; // real seconds
    int glitchFrames = 0;     // vr_debug_bullettime: frames drawn glitching (printed as it ends)
};
TapFeedback tapFx;

// The tap's glitch now (0..2): rising at once, holding, falling over its last third.
[[nodiscard]] float tapGlitch()
{
    const double t = realtime - tapFx.glitchAt;
    if(tapFx.glitchLength <= 0.f || t < 0.0 || t >= tapFx.glitchLength)
    {
        return 0.f;
    }
    const float x = static_cast<float>(t) / tapFx.glitchLength;
    return tapFx.glitchStrength * za::min(1.f, 3.f * (1.f - x));
}
double buzzAgain = -1.0; // realtime the buzz's second pulse is due (<0: none)

[[nodiscard]] int gadgetHand()
{
    return hands::gadgetHand();
}

void chime(sfx_t* sfx = nullptr)
{
    if(realtime - lastChime < 0.3 || !current.valid)
    {
        return; // one just played (the game's own, moved here, or ours)
    }
    sfx = sfx ? sfx : S_PrecacheSound("misc/talk.wav");
    if(!sfx)
    {
        return;
    }
    lastChime = realtime;
    Con_DPrintf("gadget: chime (%s) from the gadget\n", sfx->name);
    const glm::vec3 c = screenCentre();
    vec3_t org{c.x, c.y, c.z};
    S_StartSound(chimeEntity, 1, sfx, org, 1.f, 1.f);
}

// A watch's buzz: two short gentle pulses.
void buzz()
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value)
    {
        Con_DPrintf("gadget: buzz (%s hand)\n", gadgetHand() == HAND_MAIN ? "main" : "off");
        be->haptic(gadgetHand(), 0.05f, 160.f, 0.4f);
        buzzAgain = realtime + 0.13;
    }
}

// A message came: with vr_messages_hologram_only, a notification not in view chimes and buzzes.
void announce(const HoloMessage& m)
{
    if(!m.notify || !(hologramOnly() || m.tip) || gadgetInView())
    {
        return;
    }
    Con_DPrintf("gadget: notification \"%s\"\n", m.text.cStr());
    chime();
    buzz();
}

// Adds a message to the queue: the same text again while it is up (a locked door touched again) keeps that one on
// instead (announced again if it had been seen and is out of view).
void addMessage(HoloMessage m)
{
    m.generation = worldGeneration();
    for(HoloMessage& e : queue)
    {
        if(e.text == m.text && alive(e))
        {
            if(held(e))
            {
                return; // still waiting to be seen
            }
            e.start = m.start;
            e.printed = za::max(e.printed, m.printed);
            if(hologramOnly() && e.notify && !gadgetInView())
            {
                e.seen = false; // not in view: waits again
                e.appeared = realtime;
                announce(e);
            }
            return;
        }
    }
    m.id = nextId++;
    announce(m);
    queue.pushBack(ZA_MOVE(m));
    if(queue.size() > 16)
    {
        queue.erase(queue.begin());
    }
}

// This frame's messages (once a frame): the game's new console lines taken into the queue as they are printed (a
// line pieced together from several prints, as a pickup's, taken whole within its first second), the queue's dead
// ones dropped; then the newest that fit, oldest first, beyond holoRows lines or holoMessagesMax messages left out
// (the ones waiting to be seen kept first).
// collectMessages's buffers (the main thread's frame).
struct CollectScratch
{
    NotifyLine line;          // a console line read
    HoloMessage continuation; // a queued message's text, laid out again (continued since)
    auto members() { return qvr::mem::list(line, continuation); }
};
[[nodiscard]] za::SizeT heldBytes(const HoloMessage& m) // (vr_mem.hpp)
{
    return mem::heldBytes(m.text);
}
mem::Scratch<CollectScratch> collectScratch{"gadget messages"};

void collectMessages()
{
    if(holoCollected == host_framecount)
    {
        return;
    }
    holoCollected = host_framecount;
    if(!hologramOn())
    {
        holoMessages.clear();
        queue.clear();
        return;
    }

    NotifyLine& line = collectScratch.line;
    HoloMessage& continuation = collectScratch.continuation;
    for(int age = 0; age < 16; age++)
    {
        if(!notifyLine(age, line))
        {
            continue;
        }
        if(line.seconds >= 1.0)
        {
            break; // older lines are older still (taken already)
        }
        if(!line.game || line.text.empty())
        {
            continue;
        }
        const double printed = realtime - line.seconds;
        auto it = za::findIf(queue.begin(), queue.end(),
            [&](const HoloMessage& m) { return m.printed > 0.0 && za::abs(m.printed - printed) < 1e-4; });
        if(it != queue.end())
        {
            makeMessage(line.text, continuation);
            if(continuation.rows > 0 && continuation.text != it->text)
            {
                it->text = continuation.text; // continued since
                it->columns = continuation.columns;
                it->rows = continuation.rows;
            }
            continue;
        }
        if(printed <= newestLine + 1e-4)
        {
            continue; // taken before, gone since
        }
        newestLine = printed;
        HoloMessage m;
        makeMessage(line.text, m);
        if(m.rows == 0)
        {
            continue;
        }
        m.start = m.appeared = m.printed = printed;
        m.notify = !pickupLine(m.text);
        addMessage(ZA_MOVE(m));
    }
    za::vectorEraseIf(queue, [](const HoloMessage& m) { return !alive(m); });

    // The newest that fit: the waiting ones first, then the rest; one too long alone, its first lines.
    za::Vector<bool>& keep = scratch.keep;
    keep.clear();
    keep.resize(queue.size(), false);
    int rows = 0, count = 0;
    for(const bool waiting : {true, false})
    {
        for(size_t i = queue.size(); i-- > 0;)
        {
            const HoloMessage& m = queue[i];
            const int r = za::min(m.rows, holoRows);
            if(keep[i] || held(m) != waiting || count >= holoMessagesMax || rows + r > holoRows)
            {
                continue;
            }
            keep[i] = true;
            rows += r;
            count++;
        }
    }
    // (Copied into the elements holoMessages already has: their strings' buffers are reused.)
    za::SizeT shown = 0;
    for(size_t i = 0; i < queue.size(); i++)
    {
        if(!keep[i])
        {
            continue;
        }
        if(shown == holoMessages.size())
        {
            holoMessages.emplaceBack();
        }
        HoloMessage& m = holoMessages[shown++];
        m = queue[i];
        if(m.rows > holoRows)
        {
            int lines = 1;
            for(size_t c = 0; c < m.text.size(); c++)
            {
                if(m.text[c] == '\n' && ++lines > holoRows)
                {
                    m.text.resize(c);
                    break;
                }
            }
            m.rows = holoRows;
        }
    }
    holoMessages.resize(shown);
    za::stableSort(holoMessages.begin(), holoMessages.end(),
        [](const HoloMessage& a, const HoloMessage& b) { return a.start < b.start; });
}

// Marks this frame's messages seen, the hologram facing the viewer (layoutHologram): a waiting one's life starts now,
// and it grows in as if it had just come.
void markSeen()
{
    for(HoloMessage& shown : holoMessages)
    {
        auto it = za::findIf(queue.begin(), queue.end(), [&](const HoloMessage& m) { return m.id == shown.id; });
        if(it == queue.end() || it->seen)
        {
            continue;
        }
        if(held(*it))
        {
            it->start = it->appeared = realtime;
            shown.start = shown.appeared = realtime;
        }
        it->seen = shown.seen = true;
    }
}

// The image, if the messages changed (end of the 2D pass).
void renderHologram()
{
    collectMessages();
    if(holoMessages.empty())
    {
        return; // the old image holds (its blocks are matched by their text)
    }
    za::String& key = scratch.hologramKey;
    key.clear();
    for(const HoloMessage& m : holoMessages)
    {
        key += m.text;
        key += '\x1f';
    }
    if(key == holoDrawnKey && holoTarget.texture)
    {
        return;
    }
    holoDrawnKey = key;

    QVR_GPU_PROFILE("gadget hologram");
    gfx::ensureTarget(holoTarget, holoWidth * holoTexels, holoHeight * holoTexels, true, "gadget hologram");
    gfx::begin2D(holoTarget, holoWidth, holoHeight);
    fill(0.f, 0.f, holoWidth, holoHeight, glm::vec3{0.f});
    gfx::draw2D::color(white);
    holoDrawn.clear();
    int y = 0;
    for(const HoloMessage& m : holoMessages)
    {
        const int h = m.rows * 8 + holoPad * 2;
        if(y + h > holoHeight)
        {
            break;
        }
        za::StringView text = m.text;
        for(int row = 0; row < m.rows; row++)
        {
            const size_t end = za::min(text.find('\n'), text.size());
            za::String& line = scratch.hologramLine; // (a c_str for draw2D::text)
            line = text.substrByPosLen(0, end);
            const float x = static_cast<float>(holoWidth - static_cast<int>(line.size()) * 8) * 0.5f;
            gfx::draw2D::text(x, static_cast<float>(y + holoPad + row * 8), 8.f, line.cStr());
            text.removePrefix(za::min(end + 1, text.size()));
        }
        holoDrawn.pushBack({m.text, m.columns, m.rows, 1.f - static_cast<float>(y + h) / holoHeight,
            1.f - static_cast<float>(y) / holoHeight});
        y += h;
    }
    gfx::end2D();
}

// 0..1 eased out (cubic).
[[nodiscard]] float easeOut(float t)
{
    t = CLAMP(0.f, t, 1.f);
    return 1.f - (1.f - t) * (1.f - t) * (1.f - t);
}

// The hologram's triangles this frame (once, facing the first view drawn): each message's block a
// quad facing the viewer, the newest at the bottom, `vr_messages_hologram_height` over the gadget as
// seen (the view's up); the beam from the screen up to them.
void layoutHologram()
{
    if(portals::viewing() || holo.frame == host_framecount)
    {
        return;
    }
    holo.frame = host_framecount;
    holo.text.clear();
    holo.beam.clear();
    holo.top = 0.f;
    holo.centreShown = false;
    holo.shown = 0.f;
    holo.shownFrame = host_framecount;
    collectMessages();
    if(!current.valid)
    {
        return;
    }

    glm::vec3 eye, right, up;
    gfx::sceneCamera(eye, right, up);
    const float scale = current.scale;
    const glm::vec3 c = screenCentre();
    const float lift = gadgetTop(up) + za::max(0.f, vr_messages_hologram_height.value) / cmPerModelUnit * scale;
    const glm::vec3 base = c + up * lift;

    // While the screen faces the viewer, and it is in view (with or without messages: gadgetInView).
    float shown = facing(eye);
    const glm::vec4 clip = gfx::sceneViewProjection() * glm::vec4{base, 1.f};
    if(clip.w <= 0.f || za::abs(clip.x) > clip.w * 1.1f || za::abs(clip.y) > clip.w * 1.1f)
    {
        shown = 0.f;
    }
    holo.shown = shown;
    if(shown <= 0.f || holoMessages.empty() || !holoTarget.texture || holoDrawn.empty())
    {
        return;
    }
    if(shown >= 0.5f)
    {
        markSeen();
    }
    if(realtime - holo.lastShown > 0.4)
    {
        holo.opened = realtime; // projected again
    }
    holo.lastShown = realtime;

    const float k = hologramEffect();
    const float open = k > 0.f ? easeOut(static_cast<float>(realtime - holo.opened) / holoOpen) : 1.f;
    const float px = 0.26f * CLAMP(0.25f, vr_messages_hologram_size.value, 4.f) * scale / 8.f; // a font pixel
    const float bright = CLAMP(0.f, vr_gadget_screen_brightness.value, 2.f);
    const glm::vec3 rgb = hue::color(vr_gadget_screen_hue, 0.5f, 0.95f * za::max(bright, 0.3f) * gearlights::screen());
    // The blocks, newest first from the bottom up, each growing as it appears.
    za::Vector<bool>& used = scratch.used;
    used.clear();
    used.resize(holoDrawn.size(), false);
    float y = 0.f, widest = 0.f, strongest = 0.f;
    double newest = -100.0;
    for(const HoloMessage& m : za::reversed(holoMessages))
    {
        size_t b = 0;
        while(b < holoDrawn.size() && (used[b] || holoDrawn[b].text != m.text))
        {
            b++;
        }
        if(b == holoDrawn.size())
        {
            continue; // not in the image yet (next frame)
        }
        used[b] = true;
        const HoloBlock& block = holoDrawn[b];

        const float age = static_cast<float>(realtime - m.appeared);
        const float fade = CLAMP(0.f, age / holoFadeIn, 1.f) *
                           (held(m) ? 1.f : CLAMP(0.f, static_cast<float>(m.start + lifeOf(m) - realtime) / holoFadeOut, 1.f));
        if(fade <= 0.f)
        {
            continue;
        }
        const float grow = k > 0.f ? easeOut(age / holoGrow) : 1.f;
        const float alpha = fade * shown * (0.4f + 0.6f * open);
        newest = za::max(newest, m.appeared);
        strongest = za::max(strongest, alpha);
        if(m.centre && m.text == latestCentre)
        {
            holo.centreShown = shown >= 0.5f;
        }

        const float w = static_cast<float>(block.columns * 8 + holoPad * 2) * px * (0.3f + 0.7f * open) * (0.6f + 0.4f * grow);
        const float h = static_cast<float>(block.rows * 8 + holoPad * 2) * px * open * grow;
        const float du = static_cast<float>(block.columns * 8 + holoPad * 2) * 0.5f / holoWidth;
        const glm::vec3 bl = base - right * (w * 0.5f) + up * y;
        const glm::vec3 br = bl + right * w;
        const glm::vec3 tr = br + up * h;
        const glm::vec3 tl = bl + up * h;
        const glm::vec4 color{rgb, alpha};
        const gfx::Vertex v[4] = {{bl, {0.5f - du, block.v0}, color}, {br, {0.5f + du, block.v0}, color},
            {tr, {0.5f + du, block.v1}, color}, {tl, {0.5f - du, block.v1}, color}};
        for(const gfx::Vertex* p : {&v[0], &v[1], &v[2], &v[0], &v[2], &v[3]})
        {
            holo.text.pushBack(*p);
        }
        y += h;
        widest = za::max(widest, w);
    }
    if(holo.text.empty())
    {
        return;
    }
    holo.top = lift + y;

    // Glitches: now and then, as a message comes, and as it is projected.
    const float since = static_cast<float>(realtime - newest);
    float g = glitch(realtime + 4.1) * 0.7f;
    if(since < 0.35f)
    {
        g = za::max(g, 0.8f * za::sin(since / 0.35f * 3.14159265f));
    }
    g = za::max(g, 0.8f * (1.f - open));
    const float time = static_cast<float>(za::fmod(realtime, 1000.0));
    holo.params = {time, k, g * za::min(k, 1.f), 0.f};

    // The beam: from the screen (a little inside its edge) to the blocks' outline, its corners
    // matched by where they are in the view (whichever way the gadget is turned).
    const float beam = 0.3f * za::min(k, 1.f) * strongest * (1.f + 1.5f * (1.f - open));
    if(beam <= 0.f)
    {
        return;
    }
    glm::vec3 corner;
    glm::vec2 size;
    screenRect(corner, size);
    za::Array<glm::vec3, 4> s;
    za::Array<float, 4> angle;
    for(int i = 0; i < 4; i++)
    {
        const glm::vec3 m{((i == 1 || i == 2) ? 0.5f : -0.5f) * size.x * 0.85f, (i >= 2 ? 0.5f : -0.5f) * size.y * 0.85f,
            corner.z + 0.02f};
        s[static_cast<size_t>(i)] = current.origin + current.axes * (m * scale);
        const glm::vec3 d = s[static_cast<size_t>(i)] - c;
        angle[static_cast<size_t>(i)] = za::atan2(glm::dot(d, up), glm::dot(d, right));
    }
    // Round the screen from the corner nearest the view's lower left (-135 degrees), as the blocks' outline.
    za::Array<int, 4> order{0, 1, 2, 3};
    za::quickSort(order.begin(), order.end(), [&](int a, int b) { return angle[static_cast<size_t>(a)] < angle[static_cast<size_t>(b)]; });
    int first = 0;
    float nearest = 10.f;
    for(int i = 0; i < 4; i++)
    {
        float d = za::abs(angle[static_cast<size_t>(order[static_cast<size_t>(i)])] + 2.3561945f);
        d = za::min(d, 6.2831853f - d);
        if(d < nearest)
        {
            nearest = d;
            first = i;
        }
    }
    const glm::vec3 p[4] = {base - right * (widest * 0.5f), base + right * (widest * 0.5f),
        base + right * (widest * 0.5f) + up * y, base - right * (widest * 0.5f) + up * y};
    const glm::vec4 color{rgb, beam};
    for(int i = 0; i < 4; i++)
    {
        const int j = (i + 1) % 4;
        const glm::vec3& sa = s[static_cast<size_t>(order[static_cast<size_t>((first + i) % 4)])];
        const glm::vec3& sb = s[static_cast<size_t>(order[static_cast<size_t>((first + j) % 4)])];
        const gfx::Vertex a{sa, {-1.f, 0.f}, color}, b{sb, {1.f, 0.f}, color}, pb{p[j], {1.f, 1.f}, color},
            pa{p[i], {-1.f, 1.f}, color};
        for(const gfx::Vertex* q : {&a, &b, &pb, &a, &pb, &pa})
        {
            holo.beam.pushBack(*q);
        }
    }
}
