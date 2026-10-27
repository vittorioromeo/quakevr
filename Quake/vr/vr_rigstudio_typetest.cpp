// vr_rigstudio_typetest.cpp -- vr_rigstudio_test_typing: the Rig Studio's text fields typed in through the game's own
// input loop. SDL events (the mouse moved and clicked, keys pressed and let go, the typed characters) are pushed onto
// SDL's queue as a keyboard and mouse would make them, a few a frame: IN_SendKeyEvents hands them to ImGui (vr_imgui.cpp)
// as it does the real ones. A typed character is pushed only while SDL's text input is on (SDL_TEXTINPUT enabled), as
// SDL itself sends it from the keyboard: the fields' characters come only when the game has started it for ImGui.
//
// The steps: the sequence's FPS (a DragFloat) Ctrl+clicked, "12.5r" typed (the field takes any text: its number read
// up to the R), Enter: 12.5, the R no tool change; Ctrl+Z: back to 10; Ctrl+clicked again, "77" and Escape: still 10; the Outliner's search clicked, "armx" typed and
// Backspace, Enter: "arm" (the bones filtered, the tool and the game's keys untouched); the console's key opens and
// closes the console. Ends on `rigstudio typing: N passed, M failed`.

#include "vr_rigstudio_internal.hpp"
#include "vr_engine.hpp"

#include "thirdparty/imgui/imgui.h"
#include "thirdparty/imgui/imgui_internal.h"

#include "Zancle/Math/Abs.hpp"

#include <SDL.h>

#include <string.h>

extern "C" qboolean keydown[MAX_KEYS]; // keys.c (the game's own key state)

using namespace qvr;
using namespace qvr::rigstudio::detail;

namespace
{

enum class EvKind : int
{
    Key,
    Text,
    Move,
    Button,
    Break, // the frame's events end here
};

struct TyEvent
{
    EvKind kind{EvKind::Break};
    SDL_Scancode sc{SDL_SCANCODE_UNKNOWN};
    SDL_Keycode sym{0};
    Uint16 mod{0};
    bool down{false};
    char text[8]{};
    float x{0.f}, y{0.f};
};

struct TyRect
{
    ImVec2 min{0.f, 0.f}, max{0.f, 0.f};
    int seen{-1}; // the test frame it was last drawn in
};

struct TypingTest
{
    int step{-1}; // -1 not running
    int phase{0};
    int passed{0}, failed{0};
    int frame{0};      // the test's frames
    int waited{0};     // frames in this phase
    int sinceLast{0};  // frames since the last event pushed
    za::Vector<TyEvent> events;
    int next{0};
    TyRect rect[3];
    float fps0{0.f};
};
TypingTest tt;

constexpr int giveUp = 600; // frames (a stuck step: FAIL "timed out")

void brk()
{
    tt.events.pushBack(TyEvent{});
}

void keyEv(SDL_Scancode sc, SDL_Keycode sym, bool down, Uint16 mod)
{
    TyEvent e;
    e.kind = EvKind::Key;
    e.sc = sc;
    e.sym = sym;
    e.down = down;
    e.mod = mod;
    tt.events.pushBack(e);
}

// A key pressed and let go (the modifiers held as `mod` says)
void tap(SDL_Scancode sc, SDL_Keycode sym, Uint16 mod = 0)
{
    keyEv(sc, sym, true, mod);
    brk();
    keyEv(sc, sym, false, mod);
    brk();
}

// A character typed: its key down, the character, its key up (SDL's order)
void typeChar(char c)
{
    SDL_Scancode sc = SDL_SCANCODE_UNKNOWN;
    if(c >= 'a' && c <= 'z')
    {
        sc = static_cast<SDL_Scancode>(SDL_SCANCODE_A + (c - 'a'));
    }
    else if(c >= '1' && c <= '9')
    {
        sc = static_cast<SDL_Scancode>(SDL_SCANCODE_1 + (c - '1'));
    }
    else if(c == '0')
    {
        sc = SDL_SCANCODE_0;
    }
    else if(c == '.')
    {
        sc = SDL_SCANCODE_PERIOD;
    }
    else if(c == '-')
    {
        sc = SDL_SCANCODE_MINUS;
    }
    keyEv(sc, static_cast<SDL_Keycode>(c), true, 0);
    TyEvent t;
    t.kind = EvKind::Text;
    t.text[0] = c;
    tt.events.pushBack(t);
    brk();
    keyEv(sc, static_cast<SDL_Keycode>(c), false, 0);
    brk();
}

void typeText(const char* s)
{
    for(; *s; s++)
    {
        typeChar(*s);
    }
}

void mouseAt(float x, float y)
{
    TyEvent e;
    e.kind = EvKind::Move;
    e.x = x;
    e.y = y;
    tt.events.pushBack(e);
    brk();
}

void button(bool down)
{
    TyEvent e;
    e.kind = EvKind::Button;
    e.down = down;
    tt.events.pushBack(e);
    brk();
}

// A click in the middle of the rect, with Ctrl held (a DragFloat's text mode) or not
void clickOn(const TyRect& r, bool ctrl)
{
    const float x = (r.min.x + r.max.x) * 0.5f, y = (r.min.y + r.max.y) * 0.5f;
    mouseAt(x, y);
    if(ctrl)
    {
        keyEv(SDL_SCANCODE_LCTRL, SDLK_LCTRL, true, KMOD_LCTRL);
        brk();
    }
    button(true);
    button(false);
    if(ctrl)
    {
        keyEv(SDL_SCANCODE_LCTRL, SDLK_LCTRL, false, 0);
        brk();
    }
}

void ctrlKey(SDL_Scancode sc, SDL_Keycode sym)
{
    keyEv(SDL_SCANCODE_LCTRL, SDLK_LCTRL, true, KMOD_LCTRL);
    brk();
    tap(sc, sym, KMOD_LCTRL);
    keyEv(SDL_SCANCODE_LCTRL, SDLK_LCTRL, false, 0);
    brk();
}

// This frame's events onto SDL's queue (up to the next break): the input loop takes them at the next frame's start
void pushFrame()
{
    SDL_Window* window = static_cast<SDL_Window*>(VID_GetWindow());
    const Uint32 id = window ? SDL_GetWindowID(window) : 0;
    bool any = false;
    while(tt.next < static_cast<int>(tt.events.size()))
    {
        const TyEvent& e = tt.events[static_cast<za::SizeT>(tt.next++)];
        if(e.kind == EvKind::Break)
        {
            break;
        }
        SDL_Event ev;
        memset(&ev, 0, sizeof(ev));
        switch(e.kind)
        {
        case EvKind::Key:
            ev.type = e.down ? SDL_KEYDOWN : SDL_KEYUP;
            ev.key.windowID = id;
            ev.key.state = e.down ? SDL_PRESSED : SDL_RELEASED;
            ev.key.keysym.scancode = e.sc;
            ev.key.keysym.sym = e.sym;
            ev.key.keysym.mod = e.mod;
            break;
        case EvKind::Text:
            if(SDL_GetEventState(SDL_TEXTINPUT) != SDL_ENABLE)
            {
                continue; // (SDL's own keyboard sends no character while text input is off: neither do we)
            }
            ev.type = SDL_TEXTINPUT;
            ev.text.windowID = id;
            q_strlcpy(ev.text.text, e.text, sizeof(ev.text.text));
            break;
        case EvKind::Move:
            ev.type = SDL_MOUSEMOTION;
            ev.motion.windowID = id;
            ev.motion.x = static_cast<Sint32>(e.x);
            ev.motion.y = static_cast<Sint32>(e.y);
            break;
        case EvKind::Button:
            ev.type = e.down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
            ev.button.windowID = id;
            ev.button.button = SDL_BUTTON_LEFT;
            ev.button.state = e.down ? SDL_PRESSED : SDL_RELEASED;
            ev.button.clicks = 1;
            break;
        default: break;
        }
        SDL_PushEvent(&ev);
        any = true;
    }
    if(any)
    {
        tt.sinceLast = 0;
    }
}

// Every queued event pushed, taken by the input loop and through ImGui's own queue (its trickling: a frame each)
[[nodiscard]] bool eventsDone()
{
    if(tt.next < static_cast<int>(tt.events.size()))
    {
        return false;
    }
    return tt.sinceLast >= 2 && GImGui && GImGui->InputEventsQueue.Size == 0;
}

void queueReset()
{
    tt.events.clear();
    tt.next = 0;
    tt.waited = 0;
}

void check(bool ok, const char* what)
{
    Con_Printf("rigstudio typing %d: %s: %s\n", tt.step + 1, what, ok ? "PASS" : "FAIL");
    (ok ? tt.passed : tt.failed)++;
}

[[nodiscard]] bool shown(int which)
{
    const TyRect& r = tt.rect[which];
    const ImVec2 ds = ImGui::GetIO().DisplaySize;
    return r.seen >= tt.frame - 1 && r.max.x > r.min.x && r.min.x >= 0.f && r.min.y >= 0.f && r.max.x <= ds.x && r.max.y <= ds.y;
}

[[nodiscard]] int filteredBones()
{
    ImGuiTextFilter filter;
    q_strlcpy(filter.InputBuf, st.search, sizeof(filter.InputBuf));
    filter.Build();
    int n = 0;
    for(int b = 0; b < st.numBones; b++)
    {
        n += filter.PassFilter(st.rig->bones[b].name) ? 1 : 0;
    }
    return n;
}

void finish()
{
    Con_Printf("rigstudio typing: %d passed, %d failed\n", tt.passed, tt.failed);
    tt.step = -1;
}

// The steps: 0 set up, 1 FPS typed, 2 undone, 3 cancelled, 4 the search typed, 5 the console's key
void nextStep()
{
    tt.step++;
    tt.phase = 0;
    queueReset();
}

} // namespace

namespace qvr::rigstudio::detail
{

void typingProbe(int which)
{
    if(tt.step < 0 || which < 1 || which > 2)
    {
        return;
    }
    tt.rect[which].min = ImGui::GetItemRectMin();
    tt.rect[which].max = ImGui::GetItemRectMax();
    tt.rect[which].seen = tt.frame;
}

void typingTestTick()
{
    if(tt.step < 0)
    {
        return;
    }
    if(!st.open)
    {
        Con_Printf("rigstudio typing: FAIL the studio closed\n");
        tt.failed++;
        finish();
        return;
    }
    tt.frame++;
    tt.sinceLast++;
    tt.waited++;
    pushFrame();
    if(tt.waited > giveUp)
    {
        check(false, "timed out");
        nextStep();
        if(tt.step > 5)
        {
            finish();
        }
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    rigdata::Sequence* s = curSeq();
    switch(tt.step)
    {
    case 0: // the subject and its rig, a sequence, the two windows placed
        if(!st.rig || !st.model)
        {
            return;
        }
        if(tt.phase == 0)
        {
            newSequence("typetest", 10, 10.f, false);
            st.tool = Tool::Select;
            st.search[0] = '\0';
            st.showTools = st.showProps = st.showTimeline = st.showPoseData = false;
            st.showSequence = st.showOutliner = true;
            tt.phase = 1;
            return;
        }
        if(!s || !shown(1) || !shown(2))
        {
            // (the windows placed, every frame until both are drawn with their fields: a window not made yet takes
            // no place)
            const ImVec2 ds = io.DisplaySize;
            ImGui::SetWindowPos("Sequence", ImVec2{ds.x - 330.f, 40.f}, ImGuiCond_Always);
            ImGui::SetWindowSize("Sequence", ImVec2{320.f, 400.f}, ImGuiCond_Always);
            ImGui::SetWindowCollapsed("Sequence", false, ImGuiCond_Always);
            ImGui::SetWindowPos("Outliner", ImVec2{10.f, 40.f}, ImGuiCond_Always);
            ImGui::SetWindowSize("Outliner", ImVec2{260.f, 400.f}, ImGuiCond_Always);
            ImGui::SetWindowCollapsed("Outliner", false, ImGuiCond_Always);
            return;
        }
        check(SDL_IsTextInputActive() == SDL_TRUE, va("SDL's text input on while the studio is open (%s)",
                                                      SDL_IsTextInputActive() ? "on" : "off"));
        nextStep();
        return;
    case 1: // FPS: Ctrl+click, "12.5r", Enter
        if(tt.phase == 0)
        {
            tt.fps0 = s ? s->fps : 0.f;
            clickOn(tt.rect[1], true);
            tt.phase = 1;
            return;
        }
        if(tt.phase == 1)
        {
            if(!eventsDone() || !io.WantTextInput)
            {
                return; // (the field in its text mode)
            }
            queueReset();
            typeText("12.5r");
            tap(SDL_SCANCODE_RETURN, SDLK_RETURN);
            tt.phase = 2;
            return;
        }
        if(!eventsDone() || io.WantTextInput)
        {
            return;
        }
        check(s && za::abs(s->fps - 12.5f) < 1e-4f && tt.fps0 == 10.f,
            va("FPS typed \"12.5r\" + Enter: %.3f (was %.3f, want 12.5)", s ? s->fps : -1.f, tt.fps0));
        check(st.tool == Tool::Select, va("the R typed into the field: the tool %d (want Select 0)", static_cast<int>(st.tool)));
        nextStep();
        return;
    case 2: // Ctrl+Z after the field's edit: the value back
        if(tt.phase == 0)
        {
            ctrlKey(SDL_SCANCODE_Z, SDLK_z);
            tt.phase = 1;
            return;
        }
        if(!eventsDone())
        {
            return;
        }
        s = curSeq();
        check(s && za::abs(s->fps - 10.f) < 1e-4f, va("Ctrl+Z: FPS %.3f (want 10)", s ? s->fps : -1.f));
        nextStep();
        return;
    case 3: // Ctrl+click again, "77", Escape: cancelled
        if(tt.phase == 0)
        {
            clickOn(tt.rect[1], true);
            tt.phase = 1;
            return;
        }
        if(tt.phase == 1)
        {
            if(!eventsDone() || !io.WantTextInput)
            {
                return;
            }
            queueReset();
            typeText("77");
            tap(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE);
            tt.phase = 2;
            return;
        }
        if(!eventsDone() || io.WantTextInput)
        {
            return;
        }
        check(s && za::abs(s->fps - 10.f) < 1e-4f && key_dest == key_game,
            va("FPS \"77\" + Escape: %.3f (want 10, cancelled), key_dest %d (want the game's 0)", s ? s->fps : -1.f,
                static_cast<int>(key_dest)));
        nextStep();
        return;
    case 4: // the Outliner's search: click, "armx", Backspace, Enter
        if(tt.phase == 0)
        {
            clickOn(tt.rect[2], false);
            tt.phase = 1;
            return;
        }
        if(tt.phase == 1)
        {
            if(!eventsDone() || !io.WantTextInput)
            {
                return;
            }
            queueReset();
            typeText("armx");
            tap(SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE);
            tt.phase = 2;
            return;
        }
        if(tt.phase == 2)
        {
            if(!eventsDone())
            {
                return;
            }
            // (still in the field: the game's keys not down, the tool as it was though R and A were typed)
            check(!strcmp(st.search, "arm"), va("the search typed \"armx\" + Backspace: \"%s\" (want \"arm\")", st.search));
            const int n = filteredBones();
            check(n > 0 && n < st.numBones, va("the Outliner filtered: %d of %d bones", n, st.numBones));
            check(st.tool == Tool::Select && !keydown[static_cast<int>('r')] && !keydown[static_cast<int>('a')] && key_dest == key_game,
                va("while typing: the tool %d (want Select 0), the game's keys r %d a %d, key_dest %d", static_cast<int>(st.tool),
                    static_cast<int>(keydown[static_cast<int>('r')]), static_cast<int>(keydown[static_cast<int>('a')]),
                    static_cast<int>(key_dest)));
            queueReset();
            tap(SDL_SCANCODE_RETURN, SDLK_RETURN);
            tt.phase = 3;
            return;
        }
        if(!eventsDone() || io.WantTextInput)
        {
            return;
        }
        check(!strcmp(st.search, "arm"), va("Enter: the field let go, the search kept: \"%s\"", st.search));
        nextStep();
        return;
    case 5: // the console's key (no field typed in): the console opens; again: it closes
        if(tt.phase == 0)
        {
            tap(SDL_SCANCODE_GRAVE, SDLK_BACKQUOTE);
            tt.phase = 1;
            return;
        }
        if(tt.phase == 1)
        {
            if(!eventsDone())
            {
                return;
            }
            check(key_dest == key_console, va("the console's key: key_dest %d (want the console's 1)", static_cast<int>(key_dest)));
            queueReset();
            tap(SDL_SCANCODE_GRAVE, SDLK_BACKQUOTE);
            tt.phase = 2;
            return;
        }
        if(!eventsDone())
        {
            return;
        }
        check(key_dest == key_game, va("the console's key again: key_dest %d (want the game's 0)", static_cast<int>(key_dest)));
        finish();
        return;
    default: finish(); return;
    }
}

void testTyping_f()
{
    if(!st.open)
    {
        Con_Printf("rigstudio typing: the studio is not open\n");
        return;
    }
    tt.step = 0;
    tt.phase = 0;
    tt.passed = tt.failed = 0;
    tt.frame = 0;
    tt.sinceLast = 0;
    tt.rect[1] = tt.rect[2] = TyRect{};
    queueReset();
}

} // namespace qvr::rigstudio::detail
