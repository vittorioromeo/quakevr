// vr_bindsetup.cpp -- the Controller Bindings screen: see vr_bindsetup.hpp.

#include "vr_bindsetup.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_testkit.hpp"

#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Fabs.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

extern "C" {
extern float m_mousex, m_mousey;   // menu.c: the mouse (the laser) in menu coordinates
extern qboolean keydown[MAX_KEYS]; // keys.c
extern char* keybindings[MAX_KEYS]; // keys.c
int Key_StringToKeynum(const char* str);
void M_Menu_Keys_f(void);
}

namespace qvr::bindsetup
{
namespace
{

// ---------------------------------------------------------------------------------------------------------------------
// The pictures and their buttons (Misc/quakevr/make_controller_art.py)
// ---------------------------------------------------------------------------------------------------------------------

enum class Art : unsigned char
{
    Quest2,
    Quest3,
    QuestPro,
    RiftCV1,
    Pico,
    Index,
    Frame,
    Vive,
    Wmr,
    SimpleVr,
    Xbox360,
    XboxOne,
    PlayStation,
    SwitchPro,
    GenericPad,
};

constexpr const char* artFiles[] = {"quest2", "quest3", "questpro", "riftcv1", "pico", "index", "frame", "vive", "wmr",
    "simplevr", "xbox360", "xboxone", "playstation", "switchpro", "genericpad"};

enum class Pic : unsigned char
{
    Left,  // a VR controller, the left hand
    Right, // the right hand
    Pad,   // a gamepad
};

enum class Btn : unsigned char
{
    Trigger,
    Grip,
    Primary,
    Secondary,
    Stick,
    StickUp,
    StickDown,
    Menu,
    A,
    B,
    X,
    Y,
    LB,
    RB,
    LT,
    RT,
    LS,
    RS,
    DUp,
    DDown,
    DLeft,
    DRight,
    Start,
    Back,
};

struct ArtButton
{
    Art art;
    Pic pic;
    Btn btn;
    float u, v;        // its anchor on the picture (0..1)
    const char* name;  // "A", "Trigger"
    const char* glyph; // gfx/vr/glyphs/<glyph>
};

constexpr ArtButton artButtons[] = {
#include "vr_bindsetup_art.inc"
};

struct ModelInfo
{
    const char* id;
    const char* name;
    Art art;
    bool vr;
};

constexpr ModelInfo models[] = {
    {"quest3", "Meta Quest 3", Art::Quest3, true},
    {"quest3s", "Meta Quest 3S", Art::Quest3, true},
    {"quest2", "Meta Quest 2", Art::Quest2, true},
    {"quest1", "Quest / Rift S", Art::Quest2, true},
    {"questpro", "Meta Quest Pro", Art::QuestPro, true},
    {"riftcv1", "Oculus Rift", Art::RiftCV1, true},
    {"touch", "Touch controllers", Art::Quest3, true},
    {"index", "Valve Index", Art::Index, true},
    {"vive", "HTC Vive", Art::Vive, true},
    {"wmr", "Windows Mixed Reality", Art::Wmr, true},
    {"reverbg2", "HP Reverb G2", Art::Quest2, true},
    {"pico", "Pico", Art::Pico, true},
    {"vivecosmos", "Vive Cosmos / Focus 3", Art::Quest3, true},
    {"frame", "Steam Frame", Art::Frame, true},
    {"simple", "Simple controller", Art::SimpleVr, true},
    {"xbox360", "Xbox 360", Art::Xbox360, false},
    {"xboxone", "Xbox One / Series", Art::XboxOne, false},
    {"playstation", "PlayStation", Art::PlayStation, false},
    {"switchpro", "Switch Pro", Art::SwitchPro, false},
    {"gamepad", "Gamepad", Art::GenericPad, false},
    {"keyboard", "Keyboard and mouse", Art::GenericPad, false},
};
static_assert(sizeof(models) / sizeof(models[0]) == static_cast<int>(Model::Count), "a ModelInfo per Model");

[[nodiscard]] const ModelInfo& info(Model m)
{
    return models[static_cast<int>(m) < static_cast<int>(Model::Count) ? static_cast<int>(m) : 0];
}

// The key a button sends (vr_input.cpp buttonKeys: the off hand's on the left; Swap Face Buttons sends the other hand's
// face buttons' and stick click's keys), -1 for the menu button (Escape: never rebound).
[[nodiscard]] int keyFor(Pic pic, Btn b)
{
    if(pic == Pic::Pad)
    {
        switch(b)
        {
            case Btn::A: return K_ABUTTON;
            case Btn::B: return K_BBUTTON;
            case Btn::X: return K_XBUTTON;
            case Btn::Y: return K_YBUTTON;
            case Btn::LB: return K_LSHOULDER;
            case Btn::RB: return K_RSHOULDER;
            case Btn::LT: return K_LTRIGGER;
            case Btn::RT: return K_RTRIGGER;
            case Btn::LS: return K_LTHUMB;
            case Btn::RS: return K_RTHUMB;
            case Btn::DUp: return K_DPAD_UP;
            case Btn::DDown: return K_DPAD_DOWN;
            case Btn::DLeft: return K_DPAD_LEFT;
            case Btn::DRight: return K_DPAD_RIGHT;
            case Btn::Back: return K_BACK;
            default: return -1; // Start: the menu
        }
    }
    bool left = pic == Pic::Left;
    const bool face = b == Btn::Primary || b == Btn::Secondary || b == Btn::Stick;
    if(face && vr_button_swap.value != 0.f)
    {
        left = !left;
    }
    switch(b)
    {
        case Btn::Trigger: return left ? K_LTRIGGER : K_RTRIGGER;
        case Btn::Grip: return left ? K_LSHOULDER : K_RSHOULDER;
        case Btn::Primary: return left ? K_XBUTTON : K_ABUTTON;
        case Btn::Secondary: return left ? K_YBUTTON : K_BBUTTON;
        case Btn::Stick: return left ? K_LTHUMB : K_RTHUMB;
        case Btn::StickUp: return K_DPAD_UP;
        case Btn::StickDown: return K_DPAD_DOWN;
        default: return -1;
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// Pictures (gfx/vr/controllers, gfx/vr/glyphs), loaded once each and kept (their textures come back on a video restart)
// ---------------------------------------------------------------------------------------------------------------------

struct ImagePic
{
    char path[64]{};
    alignas(16) za::U8 pic[64]{}; // its qpic_t (Draw_LoadImagePic: Draw_PicBytes () bytes)
    int bounds[4]{};
    int state{0}; // 1 loaded, -1 none
};

constexpr int maxPics = 192;
ImagePic pics[maxPics]; // (fixed: the texture manager keeps each pic's address)
int picCount = 0;

[[nodiscard]] qpic_t* imagePic(const char* path, float* aspect = nullptr)
{
    ImagePic* p = nullptr;
    for(int i = 0; i < picCount && !p; i++)
    {
        p = ZA_STRCMP(pics[i].path, path) ? nullptr : &pics[i];
    }
    if(!p)
    {
        if(picCount >= maxPics || Draw_PicBytes() > sizeof(p->pic))
        {
            return nullptr;
        }
        p = &pics[picCount++];
        q_strlcpy(p->path, path, sizeof(p->path));
        p->state = Draw_LoadImagePic(reinterpret_cast<qpic_t*>(p->pic), path, p->bounds) ? 1 : -1;
        if(p->state < 0)
        {
            Con_DPrintf("bindsetup: no %s image\n", path);
        }
    }
    if(p->state <= 0)
    {
        return nullptr;
    }
    qpic_t* q = reinterpret_cast<qpic_t*>(p->pic);
    if(aspect)
    {
        *aspect = q->width > 0 ? static_cast<float>(q->height) / static_cast<float>(q->width) : 1.f;
    }
    return q;
}

// ---------------------------------------------------------------------------------------------------------------------
// Binds
// ---------------------------------------------------------------------------------------------------------------------

struct Slot
{
    const ArtButton* art;
    int key; // -1: fixed (the menu button)
};

struct Action
{
    za::String command;
    za::String name;
};

[[nodiscard]] const char* bindingOf(int key)
{
    return key >= 0 && keybindings[key] ? keybindings[key] : "";
}

void swapKeys(int a, int b)
{
    if(a < 0 || b < 0 || a == b)
    {
        return;
    }
    const za::String ba{bindingOf(a)}, bb{bindingOf(b)};
    Key_SetBinding(a, bb.empty() ? nullptr : bb.cStr());
    Key_SetBinding(b, ba.empty() ? nullptr : ba.cStr());
}

void setKey(int key, const char* command)
{
    if(key >= 0)
    {
        Key_SetBinding(key, command && command[0] ? command : nullptr);
    }
}

// The next token of `s` (a "quoted" one whole), into out; false at the line's end.
bool token(const char*& s, char* out, int size)
{
    while(*s == ' ' || *s == '\t')
    {
        s++;
    }
    if(!*s || *s == '\n' || *s == '\r' || (s[0] == '/' && s[1] == '/'))
    {
        return false;
    }
    int n = 0;
    if(*s == '"')
    {
        s++;
        while(*s && *s != '"' && *s != '\n')
        {
            if(n < size - 1)
            {
                out[n++] = *s;
            }
            s++;
        }
        if(*s == '"')
        {
            s++;
        }
    }
    else
    {
        while(*s && *s != ' ' && *s != '\t' && *s != '\n' && *s != '\r')
        {
            if(n < size - 1)
            {
                out[n++] = *s;
            }
            s++;
        }
    }
    out[n] = 0;
    return true;
}

// A key's shipped binding (vr_bindings.cfg's "bind KEY command" lines: the controllers' defaults); "" unbound.
[[nodiscard]] za::String defaultBinding(int key)
{
    za::String out;
    char* file = reinterpret_cast<char*>(COM_LoadMallocFile("vr_bindings.cfg", nullptr));
    if(!file)
    {
        return out;
    }
    const char* s = file;
    while(*s)
    {
        char word[32], name[32], command[256];
        const char* line = s;
        if(token(line, word, sizeof(word)) && !q_strcasecmp(word, "bind") && token(line, name, sizeof(name)) &&
            Key_StringToKeynum(name) == key)
        {
            command[0] = 0;
            (void)token(line, command, sizeof(command));
            out = command;
        }
        while(*s && *s != '\n')
        {
            s++;
        }
        if(*s)
        {
            s++;
        }
    }
    VR_HeapFree(file);
    return out;
}

// ---------------------------------------------------------------------------------------------------------------------
// The screen
// ---------------------------------------------------------------------------------------------------------------------

struct Label
{
    int slot;
    float x0, y0, x1, y1; // its box (menu x, y)
    float ax, ay;         // its button's anchor on the picture
    bool leftSide;
};

struct Rect
{
    float x0{0.f}, y0{0.f}, x1{-1.f}, y1{-1.f};
    [[nodiscard]] bool has(float x, float y) const { return x >= x0 && x < x1 && y >= y0 && y < y1; }
};

struct Screen
{
    Model shown{Model::Quest3};
    bool forced{false};
    int from{m_options}; // the menu it was opened from (Escape goes back there)
    za::Vector<Slot> slots;
    za::Vector<Action> actions;
    za::Vector<Label> labels;
    int cursor{0};        // the selected label (the sticks, the keys)
    int picked{-1};       // a label picked up to swap (X, Space), -1 none
    int pressed{-1};      // the label the mouse (the laser's trigger) went down on
    float pressX{0.f}, pressY{0.f};
    bool dragging{false};
    // the action list
    bool listOpen{false};
    int listSlot{-1};
    int listCursor{0};
    int listScroll{0};
    char search[32]{};
    za::Vector<int> filtered;
    Rect listBox;
    float listRowsTop{0.f};
    int listRows{0};
    // the buttons
    Rect resetButton, backButton, prevModel, nextModel;
};

Screen screen;

constexpr float rowHeight = 10.f; // the action list's rows (menu y)
constexpr float labelHeight = 19.f; // two lines: the button's name, its action

void buildSlots()
{
    screen.slots.clear();
    const Art art = info(screen.shown).art;
    for(const ArtButton& a : artButtons)
    {
        if(a.art == art)
        {
            screen.slots.pushBack({&a, keyFor(a.pic, a.btn)});
        }
    }
    screen.cursor = 0;
    screen.picked = screen.pressed = -1;
    screen.listOpen = false;
}

void addAction(const char* command, const char* name)
{
    for(const Action& a : screen.actions)
    {
        if(a.command == command)
        {
            return;
        }
    }
    screen.actions.pushBack({za::String{command}, za::String{name && name[0] ? name : command}});
}

void buildActions()
{
    screen.actions.clear();
    screen.actions.pushBack({za::String{}, za::String{"(nothing)"}});
    M_Keys_EachAction(addAction);
}

[[nodiscard]] const char* actionName(const char* binding)
{
    if(!binding[0])
    {
        return "(nothing)";
    }
    for(const Action& a : screen.actions)
    {
        if(a.command == binding)
        {
            return a.name.cStr();
        }
    }
    return binding;
}

void swapSlots(int a, int b)
{
    if(a < 0 || b < 0 || a >= static_cast<int>(screen.slots.size()) || b >= static_cast<int>(screen.slots.size()) ||
        a == b || screen.slots[a].key < 0 || screen.slots[b].key < 0)
    {
        menuui::uiSound(menuui::UiSound::Deny);
        return;
    }
    swapKeys(screen.slots[a].key, screen.slots[b].key);
    Con_DPrintf("bindsetup: %s and %s swapped\n", Key_KeynumToString(screen.slots[a].key),
        Key_KeynumToString(screen.slots[b].key));
    menuui::uiSound(menuui::UiSound::Adjust);
}

void resetSlots()
{
    for(const Slot& s : screen.slots)
    {
        if(s.key >= 0)
        {
            const za::String d = defaultBinding(s.key);
            setKey(s.key, d.cStr());
        }
    }
}

void refilter()
{
    screen.filtered.clear();
    for(int i = 0; i < static_cast<int>(screen.actions.size()); i++)
    {
        if(!screen.search[0] || q_strcasestr(screen.actions[i].name.cStr(), screen.search) ||
            q_strcasestr(screen.actions[i].command.cStr(), screen.search))
        {
            screen.filtered.pushBack(i);
        }
    }
    screen.listCursor = za::min(screen.listCursor, za::max(0, static_cast<int>(screen.filtered.size()) - 1));
}

void openList(int slot)
{
    if(slot < 0 || slot >= static_cast<int>(screen.slots.size()) || screen.slots[slot].key < 0)
    {
        menuui::uiSound(menuui::UiSound::Deny);
        return;
    }
    screen.listOpen = true;
    screen.listSlot = slot;
    screen.search[0] = 0;
    screen.listCursor = 0;
    refilter();
    const char* b = bindingOf(screen.slots[slot].key);
    for(int i = 0; i < static_cast<int>(screen.filtered.size()); i++)
    {
        if(screen.actions[screen.filtered[i]].command == b)
        {
            screen.listCursor = i;
        }
    }
    screen.listScroll = za::max(0, screen.listCursor - 4);
    menuui::uiSound(menuui::UiSound::Select);
}

void pickAction(int filteredIndex)
{
    if(filteredIndex >= 0 && filteredIndex < static_cast<int>(screen.filtered.size()) && screen.listSlot >= 0)
    {
        setKey(screen.slots[screen.listSlot].key, screen.actions[screen.filtered[filteredIndex]].command.cStr());
        menuui::uiSound(menuui::UiSound::Adjust);
    }
    screen.listOpen = false;
}

void leave()
{
    screen.listOpen = false;
    menuui::openMenu(screen.from);
}

void stepModel(int dir)
{
    int m = static_cast<int>(screen.shown);
    do
    {
        m = (m + dir + static_cast<int>(Model::Count)) % static_cast<int>(Model::Count);
    } while(m == static_cast<int>(Model::Keyboard));
    screen.shown = static_cast<Model>(m);
    screen.forced = true;
    buildSlots();
    menuui::uiSound(menuui::UiSound::Move);
}

// The mouse (the laser) went down on a label, and came up on `target` (-1: off every label) having moved (a drag) or not
// (a click): a drag swaps the two buttons' bindings, a click opens the action list.
void release(int target)
{
    const int from = screen.pressed;
    const bool drag = screen.dragging;
    screen.pressed = -1;
    screen.dragging = false;
    if(from < 0)
    {
        return;
    }
    if(!drag)
    {
        screen.cursor = from;
        openList(from);
    }
    else if(target >= 0 && target != from)
    {
        screen.cursor = target;
        swapSlots(from, target);
    }
}

[[nodiscard]] int labelAt(float x, float y)
{
    for(const Label& l : screen.labels)
    {
        if(x >= l.x0 && x < l.x1 && y >= l.y0 && y < l.y1)
        {
            return l.slot;
        }
    }
    return -1;
}

// ------------------------------------------------------------------------------------------------- drawing helpers

void fill(float x, float y, float w, float h, float r, float g, float b, float a)
{
    const float rgb[3] = {r, g, b};
    Draw_FillEx(x, y, w, h, rgb, a);
}

void line(float x0, float y0, float x1, float y1, float r, float g, float b, float a)
{
    const float dx = x1 - x0, dy = y1 - y0;
    const float len = za::max(0.001f, za::sqrt(dx * dx + dy * dy));
    const float nx = -dy / len * 0.8f, ny = dx / len * 0.8f;
    const float xy[8] = {x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny};
    const float rgb[3] = {r, g, b};
    Draw_FillQuad(xy, rgb, a);
}

void frame(const Rect& r, float red, float g, float b, float a)
{
    fill(r.x0, r.y0, r.x1 - r.x0, 1.f, red, g, b, a);
    fill(r.x0, r.y1 - 1.f, r.x1 - r.x0, 1.f, red, g, b, a);
    fill(r.x0, r.y0, 1.f, r.y1 - r.y0, red, g, b, a);
    fill(r.x1 - 1.f, r.y0, 1.f, r.y1 - r.y0, red, g, b, a);
}

// `text` cut to `chars` letters ("..." at its end when cut).
void fitText(const char* text, int chars, char* out, int size)
{
    const int len = static_cast<int>(ZA_STRLEN(text));
    chars = za::min(chars, size - 1);
    if(len <= chars)
    {
        q_strlcpy(out, text, size);
        return;
    }
    const int keep = za::max(0, chars - 2);
    q_strlcpy(out, text, keep + 1);
    q_strlcat(out, "..", size);
}

void printCentred(float cx, float y, const char* text, bool white, int dim = 8)
{
    const int x = static_cast<int>(cx - static_cast<float>(ZA_STRLEN(text)) * dim * 0.5f);
    if(white)
    {
        M_PrintWhiteEx(x, static_cast<int>(y), dim, text);
    }
    else
    {
        M_PrintEx(x, static_cast<int>(y), dim, text);
    }
}

void drawButton(Rect& r, float cx, float y, const char* text, bool hot)
{
    const float w = static_cast<float>(ZA_STRLEN(text)) * 8.f + 16.f;
    r = {cx - w * 0.5f, y, cx + w * 0.5f, y + 12.f};
    menuui::paintButton(r.x0, r.x1, y + 6.f, 6.f, hot);
    printCentred(cx, y + 2.f, text, true);
}

// A picture drawn `w` wide at x, y (menu x and y; its true aspect kept on the stretched canvas): its height in menu y.
float drawPicture(qpic_t* pic, float aspect, float x, float y, float w, float k)
{
    const float h = w * aspect; // (Draw_SubPic draws h / k high, centred: from y + h (1 - 1 / k) / 2)
    Draw_SubPic(x, y - h * (1.f - 1.f / k) * 0.5f, w, h, pic, 0.f, 0.f, 1.f, 1.f, nullptr, 1.f);
    return h / k;
}

// Places a column's labels: sorted by their anchors' height, each at its anchor's height as far as they fit apart.
void placeColumn(za::Vector<Label>& column, float top, float bottom)
{
    for(int i = 1; i < static_cast<int>(column.size()); i++)
    {
        for(int j = i; j > 0 && column[j].ay < column[j - 1].ay; j--)
        {
            const Label t = column[j];
            column[j] = column[j - 1];
            column[j - 1] = t;
        }
    }
    float y = top;
    for(Label& l : column)
    {
        l.y0 = za::max(y, l.ay - labelHeight * 0.5f);
        l.y1 = l.y0 + labelHeight;
        y = l.y1 + 3.f;
    }
    float limit = bottom;
    for(int i = static_cast<int>(column.size()) - 1; i >= 0; i--)
    {
        Label& l = column[i];
        if(l.y1 > limit)
        {
            l.y0 = limit - labelHeight;
            l.y1 = limit;
        }
        limit = l.y0 - 3.f;
    }
}

void layoutAndDrawPictures(float left, float top, float width, float bottom, float k)
{
    screen.labels.clear();
    const bool vr = info(screen.shown).vr;
    const char* file = artFiles[static_cast<int>(info(screen.shown).art)];
    const float areaH = bottom - top;
    za::Vector<Label> columns[2];

    // The label columns at the edges; between them the pictures: a VR pair (the left hand left) or a gamepad, as big as
    // they fit.
    const int count = vr ? 2 : 1;
    const float gap = 10.f;
    const float colW = za::min(170.f, width * 0.27f);
    const float middle = width - 2.f * (colW + gap);
    float picX[2]{}, picW = 0.f;
    for(int p = 0; p < count; p++)
    {
        float aspect = 1.f;
        qpic_t* pic = imagePic(va("gfx/vr/controllers/%s%s", file, vr ? (p == 0 ? "_l" : "_r") : ""), &aspect);
        picW = za::min((middle - (count - 1) * gap) / static_cast<float>(count), areaH * k / aspect);
        const float cx = left + width * 0.5f + (vr ? (p == 0 ? -1.f : 1.f) * (picW + gap) * 0.5f : 0.f);
        picX[p] = cx - picW * 0.5f;
        const float h = picW * aspect / k;
        const float y = top + (areaH - h) * 0.5f;
        if(pic)
        {
            drawPicture(pic, aspect, picX[p], y, picW, k);
        }
        else
        {
            fill(picX[p], y, picW, h, 0.2f, 0.2f, 0.2f, 0.5f);
        }
        for(int i = 0; i < static_cast<int>(screen.slots.size()); i++)
        {
            const ArtButton& a = *screen.slots[i].art;
            if(vr && (a.pic == Pic::Left) != (p == 0))
            {
                continue;
            }
            Label l{};
            l.slot = i;
            l.ax = picX[p] + a.u * picW;
            l.ay = y + a.v * h;
            l.leftSide = vr ? p == 0 : a.u < 0.5f;
            columns[l.leftSide ? 0 : 1].pushBack(l);
        }
    }
    for(int c = 0; c < 2; c++)
    {
        placeColumn(columns[c], top, bottom);
        for(Label& l : columns[c])
        {
            l.x0 = c == 0 ? left : left + width - colW;
            l.x1 = l.x0 + colW;
            screen.labels.pushBack(l);
        }
    }
}

void drawLabels()
{
    const bool pointer = menuui::pointerOn(true);
    const int hover = pointer ? labelAt(m_mousex, m_mousey) : -1;
    for(const Label& l : screen.labels)
    {
        const Slot& s = screen.slots[l.slot];
        const bool selected = l.slot == screen.cursor && !pointer;
        const bool hot = l.slot == hover || selected || l.slot == screen.picked;
        const bool target = screen.dragging && l.slot == hover && l.slot != screen.pressed;
        // the leader line, from the box's inner edge to the button
        const float ex = l.leftSide ? l.x1 : l.x0, ey = (l.y0 + l.y1) * 0.5f;
        line(ex, ey, l.ax, l.ay, 0.85f, 0.55f, 0.3f, hot ? 0.95f : 0.55f);
        fill(l.ax - 1.5f, l.ay - 1.5f, 3.f, 3.f, 1.f, 0.7f, 0.35f, 1.f);
        // the box
        fill(l.x0, l.y0, l.x1 - l.x0, l.y1 - l.y0, 0.05f, 0.04f, 0.04f, 0.82f);
        if(target || l.slot == screen.picked)
        {
            fill(l.x0, l.y0, l.x1 - l.x0, l.y1 - l.y0, 0.9f, 0.5f, 0.15f, 0.35f);
        }
        frame({l.x0, l.y0, l.x1, l.y1}, hot ? 1.f : 0.45f, hot ? 0.65f : 0.35f, hot ? 0.3f : 0.25f, 1.f);
        // the glyph, the button's name and its action
        float tx = l.x0 + 3.f;
        if(qpic_t* g = imagePic(va("gfx/vr/glyphs/%s", s.art->glyph)))
        {
            drawPicture(g, 1.f, tx, l.y0 + 2.f, 15.f, Draw_MenuStretch());
            tx += 17.f;
        }
        const int chars = static_cast<int>((l.x1 - tx - 2.f) / 6.f);
        char buf[64];
        fitText(s.art->name, chars, buf, sizeof(buf));
        M_PrintEx(static_cast<int>(tx), static_cast<int>(l.y0 + 2.f), 6, buf);
        const char* action = s.key < 0 ? "Menu (fixed)" : actionName(bindingOf(s.key));
        fitText(action, static_cast<int>((l.x1 - tx - 2.f) / 7.f), buf, sizeof(buf));
        M_PrintWhiteEx(static_cast<int>(tx), static_cast<int>(l.y0 + 10.f), 7, buf);
    }
    // a label being dragged follows the pointer
    if(screen.dragging && screen.pressed >= 0)
    {
        const Slot& s = screen.slots[screen.pressed];
        const char* action = actionName(bindingOf(s.key));
        const float w = static_cast<float>(ZA_STRLEN(action)) * 7.f + 8.f;
        fill(m_mousex + 6.f, m_mousey + 4.f, w, 11.f, 0.9f, 0.5f, 0.15f, 0.85f);
        M_PrintWhiteEx(static_cast<int>(m_mousex + 10.f), static_cast<int>(m_mousey + 6.f), 7, action);
    }
}

void drawList(float left, float top, float width, float bottom)
{
    if(!screen.listOpen || screen.listSlot < 0)
    {
        return;
    }
    const Slot& s = screen.slots[screen.listSlot];
    const float w = za::min(width - 40.f, 300.f);
    const float x0 = left + (width - w) * 0.5f;
    screen.listRows = za::max(4, static_cast<int>((bottom - top - 60.f) / rowHeight));
    screen.listRows = za::min(screen.listRows, 16);
    const float h = 34.f + screen.listRows * rowHeight;
    const float y0 = top + (bottom - top - h) * 0.5f;
    screen.listBox = {x0, y0, x0 + w, y0 + h};
    fill(x0, y0, w, h, 0.04f, 0.03f, 0.03f, 0.95f);
    frame(screen.listBox, 1.f, 0.65f, 0.3f, 1.f);
    char buf[96];
    q_snprintf(buf, sizeof(buf), "%s: pick an action", s.art->name);
    printCentred(x0 + w * 0.5f, y0 + 4.f, buf, true);
    q_snprintf(buf, sizeof(buf), "Search: %s%s", screen.search, (static_cast<int>(realtime * 4.0) & 1) ? "_" : " ");
    M_Print(static_cast<int>(x0 + 6.f), static_cast<int>(y0 + 16.f), buf);
    screen.listRowsTop = y0 + 28.f;
    const int n = static_cast<int>(screen.filtered.size());
    screen.listScroll = za::max(0, za::min(screen.listScroll, n - screen.listRows));
    const bool pointer = menuui::pointerOn(true);
    const char* current = bindingOf(s.key);
    for(int r = 0; r < screen.listRows && screen.listScroll + r < n; r++)
    {
        const int i = screen.listScroll + r;
        const Action& a = screen.actions[screen.filtered[i]];
        const float y = screen.listRowsTop + r * rowHeight;
        const bool hovered = pointer && m_mousex >= x0 && m_mousex < x0 + w && m_mousey >= y && m_mousey < y + rowHeight;
        if(hovered)
        {
            screen.listCursor = i;
        }
        if(i == screen.listCursor)
        {
            menuui::drawListHighlight(x0 + 2.f, x0 + w - 2.f, static_cast<int>(y + 1.f));
        }
        fitText(a.name.cStr(), static_cast<int>((w - 24.f) / 8.f), buf, sizeof(buf));
        if(a.command == current)
        {
            M_PrintWhite(static_cast<int>(x0 + 6.f), static_cast<int>(y + 1.f), "\x0d");
        }
        M_PrintWhite(static_cast<int>(x0 + 18.f), static_cast<int>(y + 1.f), buf);
    }
    if(n == 0)
    {
        M_Print(static_cast<int>(x0 + 18.f), static_cast<int>(screen.listRowsTop + 1.f), "No action matches");
    }
}

// The labels in the keys' order: a column top to bottom, then the other.
[[nodiscard]] int labelIndex(int slot)
{
    for(int i = 0; i < static_cast<int>(screen.labels.size()); i++)
    {
        if(screen.labels[i].slot == slot)
        {
            return i;
        }
    }
    return 0;
}

void moveCursor(int key)
{
    const int n = static_cast<int>(screen.labels.size());
    if(n == 0)
    {
        return;
    }
    const Label& at = screen.labels[labelIndex(screen.cursor)];
    int best = -1;
    float bestScore = 1e9f;
    for(int i = 0; i < n; i++)
    {
        const Label& l = screen.labels[i];
        if(l.slot == at.slot)
        {
            continue;
        }
        const float dx = (l.x0 - at.x0), dy = (l.y0 - at.y0);
        const bool ok = key == K_UPARROW ? (l.leftSide == at.leftSide && dy < 0.f)
                        : key == K_DOWNARROW ? (l.leftSide == at.leftSide && dy > 0.f)
                        : key == K_LEFTARROW ? (dx < 0.f) : (dx > 0.f);
        const float score = (key == K_UPARROW || key == K_DOWNARROW) ? za::fabs(dy) : za::fabs(dy) + za::fabs(dx) * 0.01f;
        if(ok && score < bestScore)
        {
            bestScore = score;
            best = l.slot;
        }
    }
    if(best >= 0)
    {
        screen.cursor = best;
        menuui::uiSound(menuui::UiSound::Move);
    }
}

[[nodiscard]] bool modelFromArgs(int argi, Model& out)
{
    return Cmd_Argc() > argi && modelFromId(Cmd_Argv(argi), out);
}

// ------------------------------------------------------------------------------------------------------- commands

void menu_f()
{
    Model m = Model::Count;
    if(Cmd_Argc() > 1 && !modelFromArgs(1, m))
    {
        Con_Printf("menu_bindsetup [model]: the Controller Bindings screen (the controller in use, or one of:");
        for(const ModelInfo& i : models)
        {
            Con_Printf(" %s", i.id);
        }
        Con_Printf(")\n");
        return;
    }
    if(!open(m))
    {
        M_Menu_Keys_f(); // keyboard and mouse: Options > Controls
    }
}

void print_f()
{
    Model m = screen.shown;
    if(Cmd_Argc() > 1 && !modelFromArgs(1, m))
    {
        Con_Printf("vr_bindsetup_print [model]: unknown model\n");
        return;
    }
    const Model keep = screen.shown;
    screen.shown = m;
    buildSlots();
    if(screen.actions.empty())
    {
        buildActions();
    }
    Con_Printf("bindsetup: %s (%s), detected %s\n", info(m).name, info(m).id, info(detect()).id);
    for(const Slot& s : screen.slots)
    {
        const char* side = s.art->pic == Pic::Left ? "left " : s.art->pic == Pic::Right ? "right " : "";
        Con_Printf("  %s%s: %s = \"%s\" (%s)\n", side, s.art->name, s.key >= 0 ? Key_KeynumToString(s.key) : "-",
            bindingOf(s.key), s.key >= 0 ? actionName(bindingOf(s.key)) : "the menu");
    }
    screen.shown = keep;
    buildSlots();
}

struct DetectCase
{
    const char* profile;
    const char* system;
    int pad; // (profile null: SDL's type)
    Model expect;
};

constexpr DetectCase detectCases[] = {
    {"/interaction_profiles/oculus/touch_controller", "Meta Quest 3", 0, Model::Quest3},
    {"/interaction_profiles/oculus/touch_controller", "Meta Quest 3S", 0, Model::Quest3S},
    {"/interaction_profiles/meta/touch_plus_controller", "", 0, Model::Quest3},
    {"/interaction_profiles/meta/touch_plus_controller", "Meta Quest 3S", 0, Model::Quest3S},
    {"/interaction_profiles/oculus/touch_controller", "Oculus Quest2", 0, Model::Quest2},
    {"/interaction_profiles/oculus/touch_controller", "Oculus Quest", 0, Model::Quest1},
    {"/interaction_profiles/oculus/touch_controller", "Oculus Rift S", 0, Model::Quest1},
    {"/interaction_profiles/oculus/touch_controller", "Oculus Rift CV1", 0, Model::RiftCV1},
    {"/interaction_profiles/oculus/touch_controller", "Meta Quest Pro", 0, Model::QuestPro},
    {"/interaction_profiles/meta/touch_pro_controller", "", 0, Model::QuestPro},
    {"/interaction_profiles/facebook/touch_controller_pro", "", 0, Model::QuestPro},
    {"/interaction_profiles/oculus/touch_controller", "SteamVR/OpenXR : lighthouse", 0, Model::Touch},
    {"/interaction_profiles/valve/index_controller", "SteamVR/OpenXR : lighthouse", 0, Model::Index},
    {"/interaction_profiles/valve/index_controller", "Meta Quest 3 (VDXR)", 0, Model::Quest3},
    {"/interaction_profiles/htc/vive_controller", "Vive MV", 0, Model::Vive},
    {"/interaction_profiles/microsoft/motion_controller", "Windows Mixed Reality", 0, Model::Wmr},
    {"/interaction_profiles/samsung/odyssey_controller", "", 0, Model::Wmr},
    {"/interaction_profiles/hp/mixed_reality_controller", "HP Reverb G2", 0, Model::ReverbG2},
    {"/interaction_profiles/bytedance/pico4_controller", "PICO 4", 0, Model::Pico},
    {"/interaction_profiles/bytedance/pico_neo3_controller", "", 0, Model::Pico},
    {"/interaction_profiles/htc/vive_cosmos_controller", "", 0, Model::ViveCosmos},
    {"/interaction_profiles/htc/vive_focus3_controller", "", 0, Model::ViveCosmos},
    {"/interaction_profiles/valve/frame_controller_valve", "Steam Frame", 0, Model::Frame},
    {"/interaction_profiles/khr/simple_controller", "", 0, Model::SimpleVr},
    {"", "Meta Quest 2", 0, Model::Quest2},
    {"", "", 0, Model::Touch},
    {nullptr, nullptr, -1, Model::Keyboard},
    {nullptr, nullptr, 0, Model::GenericPad},
    {nullptr, nullptr, 1, Model::Xbox360},
    {nullptr, nullptr, 2, Model::XboxOne},
    {nullptr, nullptr, 3, Model::PlayStation},
    {nullptr, nullptr, 4, Model::PlayStation},
    {nullptr, nullptr, 7, Model::PlayStation},
    {nullptr, nullptr, 5, Model::SwitchPro},
    {nullptr, nullptr, 13, Model::SwitchPro},
    {nullptr, nullptr, 6, Model::GenericPad},
    {nullptr, nullptr, 9, Model::GenericPad},
};

void detectTest_f()
{
    testkit::Tally t;
    for(const DetectCase& c : detectCases)
    {
        const Model got = c.profile ? modelForProfile(c.profile, c.system) : modelForPad(c.pad);
        t.check(got == c.expect, "bindsetup_detect_test",
            va("\"%s\" \"%s\" pad %d: %s, expected %s", c.profile ? c.profile : "-", c.system ? c.system : "-", c.pad,
                info(got).id, info(c.expect).id));
    }
    // every model's picture has its buttons, and every button's key is a controller key or the menu
    for(int m = 0; m < static_cast<int>(Model::Keyboard); m++)
    {
        int n = 0;
        for(const ArtButton& a : artButtons)
        {
            n += a.art == info(static_cast<Model>(m)).art;
        }
        t.check(n >= 2, "bindsetup_detect_test", va("%s has %d buttons", info(static_cast<Model>(m)).id, n));
    }
    t.verdict("bindsetup_detect_test");
}

// vr_bindsetup_test: the screen's binding changes, on the binds themselves (the binds it touches put back after).
void bindTest_f()
{
    testkit::Tally t;
    auto check = [&](bool ok, const char* what) { t.check(ok, "bindsetup_test", what); };
    constexpr int keys[] = {K_LTRIGGER, K_RTRIGGER, K_LSHOULDER, K_RSHOULDER, K_ABUTTON, K_BBUTTON, K_XBUTTON,
        K_YBUTTON, K_LTHUMB, K_RTHUMB, K_DPAD_UP, K_DPAD_DOWN, K_DPAD_LEFT, K_DPAD_RIGHT, K_BACK};
    za::String saved[sizeof(keys) / sizeof(keys[0])];
    for(int i = 0; i < static_cast<int>(sizeof(keys) / sizeof(keys[0])); i++)
    {
        saved[i] = bindingOf(keys[i]);
    }
    const float swapWas = vr_button_swap.value;
    Cvar_SetValueQuick(&vr_button_swap, 0.f);
    const Model keep = screen.shown;
    buildActions();

    auto slotOf = [&](Pic pic, Btn b) {
        for(int i = 0; i < static_cast<int>(screen.slots.size()); i++)
        {
            if(screen.slots[i].art->pic == pic && screen.slots[i].art->btn == b)
            {
                return i;
            }
        }
        return -1;
    };
    // the Quest 3: the right trigger dragged onto A swaps their bindings
    screen.shown = Model::Quest3;
    buildSlots();
    Key_SetBinding(K_RTRIGGER, "+attack");
    Key_SetBinding(K_ABUTTON, "+jump");
    const int trig = slotOf(Pic::Right, Btn::Trigger), a = slotOf(Pic::Right, Btn::Primary);
    check(trig >= 0 && a >= 0 && screen.slots[trig].key == K_RTRIGGER && screen.slots[a].key == K_ABUTTON,
        "quest3: right trigger and A are RTRIGGER and ABUTTON");
    screen.pressed = trig;
    screen.dragging = true;
    release(a);
    check(!ZA_STRCMP(bindingOf(K_RTRIGGER), "+jump") && !ZA_STRCMP(bindingOf(K_ABUTTON), "+attack"),
        "drag trigger onto A: swapped");
    // a click opens the list; picking an action binds it
    const int stick = slotOf(Pic::Right, Btn::Stick);
    screen.pressed = stick;
    screen.dragging = false;
    release(stick);
    check(screen.listOpen && screen.listSlot == stick, "click: the action list opens");
    q_strlcpy(screen.search, "Next weapon", sizeof(screen.search));
    refilter();
    check(!screen.filtered.empty(), "search: Next weapon found");
    if(!screen.filtered.empty())
    {
        pickAction(0);
    }
    check(!ZA_STRCMP(bindingOf(K_RTHUMB), "impulse 10") && !screen.listOpen, "list pick: RTHUMB is impulse 10");
    // the list has the VR actions (bindlist.lst) and Quake's
    bool grab = false, attack = false;
    for(const Action& act : screen.actions)
    {
        grab = grab || act.command == "+grabmain";
        attack = attack || act.command == "+attack";
    }
    check(grab && attack, "the actions list has +grabmain and +attack");
    // the menu button is fixed
    const int menu = slotOf(Pic::Left, Btn::Menu);
    check(menu >= 0 && screen.slots[menu].key < 0, "the left menu button is fixed");
    // Reset to Defaults: vr_bindings.cfg's
    resetSlots();
    check(!ZA_STRCMP(bindingOf(K_RTRIGGER), "+attack") && !ZA_STRCMP(bindingOf(K_ABUTTON), "+jump") &&
              !ZA_STRCMP(bindingOf(K_RTHUMB), "+reloadmain") && !ZA_STRCMP(bindingOf(K_LSHOULDER), "+graboff"),
        "reset: the shipped VR bindings");
    // Swap Face Buttons: A sends XBUTTON
    Cvar_SetValueQuick(&vr_button_swap, 1.f);
    buildSlots();
    check(screen.slots[slotOf(Pic::Right, Btn::Primary)].key == K_XBUTTON, "button swap: A's key is XBUTTON");
    Cvar_SetValueQuick(&vr_button_swap, 0.f);
    // a gamepad: X onto B, then reset
    screen.shown = Model::XboxOne;
    buildSlots();
    Key_SetBinding(K_XBUTTON, "+reloadoff");
    Key_SetBinding(K_BBUTTON, "impulse 10");
    swapSlots(slotOf(Pic::Pad, Btn::X), slotOf(Pic::Pad, Btn::B));
    check(!ZA_STRCMP(bindingOf(K_XBUTTON), "impulse 10") && !ZA_STRCMP(bindingOf(K_BBUTTON), "+reloadoff"),
        "xboxone: X and B swapped");
    check(slotOf(Pic::Pad, Btn::Start) >= 0 && screen.slots[slotOf(Pic::Pad, Btn::Start)].key < 0, "xboxone: Start fixed");
    resetSlots();
    check(!ZA_STRCMP(bindingOf(K_XBUTTON), "+reloadoff") && !ZA_STRCMP(bindingOf(K_BBUTTON), "impulse 10") &&
              !bindingOf(K_DPAD_LEFT)[0],
        "xboxone reset: the shipped bindings (D-pad left unbound)");

    for(int i = 0; i < static_cast<int>(sizeof(keys) / sizeof(keys[0])); i++)
    {
        setKey(keys[i], saved[i].cStr());
    }
    Cvar_SetValueQuick(&vr_button_swap, swapWas);
    screen.shown = keep;
    buildSlots();
    screen.listOpen = false;
    t.verdict("bindsetup_test");
}

} // namespace

const char* modelId(Model m)
{
    return info(m).id;
}

const char* modelName(Model m)
{
    return info(m).name;
}

bool modelFromId(const char* id, Model& out)
{
    for(int i = 0; i < static_cast<int>(Model::Count); i++)
    {
        if(!q_strcasecmp(id, models[i].id))
        {
            out = static_cast<Model>(i);
            return true;
        }
    }
    return false;
}

Model modelForProfile(const char* profile, const char* systemName)
{
    char sys[64];
    int n = 0;
    for(const char* c = systemName ? systemName : ""; *c && n < static_cast<int>(sizeof(sys)) - 1; c++)
    {
        const char lower = *c >= 'A' && *c <= 'Z' ? static_cast<char>(*c - 'A' + 'a') : *c;
        if((lower >= 'a' && lower <= 'z') || (lower >= '0' && lower <= '9'))
        {
            sys[n++] = lower;
        }
    }
    sys[n] = 0;
    auto has = [&](const char* s) { return ZA_STRSTR(sys, s) != nullptr; };
    const char* prefix = "/interaction_profiles/";
    const char* p = profile ? profile : "";
    if(!ZA_STRNCMP(p, prefix, ZA_STRLEN(prefix)))
    {
        p += ZA_STRLEN(prefix);
    }
    auto is = [&](const char* s) { return !ZA_STRCMP(p, s); };
    // Touch: the headset's name tells which (every runtime plays Quest controllers as oculus/touch_controller)
    auto touch = [&]() {
        return has("quest3s")    ? Model::Quest3S
               : has("quest3")   ? Model::Quest3
               : has("questpro") ? Model::QuestPro
               : has("quest2")   ? Model::Quest2
               : has("quest")    ? Model::Quest1
               : has("rifts")    ? Model::Quest1
               : has("rift")     ? Model::RiftCV1
                                 : Model::Touch;
    };
    if(is("meta/touch_pro_controller") || is("facebook/touch_controller_pro"))
    {
        return Model::QuestPro;
    }
    if(is("meta/touch_plus_controller"))
    {
        return has("quest3s") ? Model::Quest3S : Model::Quest3;
    }
    if(is("oculus/touch_controller") || !p[0])
    {
        return touch();
    }
    if(is("valve/index_controller"))
    {
        return has("quest") ? touch() : Model::Index; // (Virtual Desktop's Index emulation: Quest controllers)
    }
    return is("htc/vive_controller")                                                    ? Model::Vive
           : is("microsoft/motion_controller") || is("samsung/odyssey_controller")      ? Model::Wmr
           : is("hp/mixed_reality_controller")                                          ? Model::ReverbG2
           : !ZA_STRNCMP(p, "bytedance/pico", 14)                                       ? Model::Pico
           : is("htc/vive_cosmos_controller") || is("htc/vive_focus3_controller")       ? Model::ViveCosmos
           : is("valve/frame_controller_valve")                                         ? Model::Frame
           : is("khr/simple_controller")                                                ? Model::SimpleVr
                                                                                        : touch();
}

Model modelForPad(int sdlType)
{
    // SDL_GameControllerType: 1 Xbox 360, 2 Xbox One, 3 PS3, 4 PS4, 7 PS5, 5 Switch Pro, 11-13 Joy-Cons; 0 unknown,
    // 6 virtual, 8 Luna, 9 Stadia, 10 Shield...: a generic gamepad
    switch(sdlType)
    {
        case -1: return Model::Keyboard;
        case 1: return Model::Xbox360;
        case 2: return Model::XboxOne;
        case 3:
        case 4:
        case 7: return Model::PlayStation;
        case 5:
        case 11:
        case 12:
        case 13: return Model::SwitchPro;
        default: return Model::GenericPad;
    }
}

Model detect()
{
    if(const Backend* b = backend())
    {
        const char* p = b->controllerProfile(1);
        return modelForProfile(p[0] ? p : b->controllerProfile(0), b->systemName());
    }
    return modelForPad(IN_GetGamepadSdlType());
}

bool open(Model model)
{
    const Model shown = model != Model::Count ? model : detect();
    if(shown == Model::Keyboard)
    {
        return false; // (the caller opens the keyboard's bindings)
    }
    screen.forced = model != Model::Count;
    screen.shown = shown;
    if(m_state != m_bindsetup)
    {
        screen.from = m_state == m_none ? m_none : static_cast<int>(m_state);
    }
    buildActions();
    buildSlots();
    IN_DeactivateForMenu();
    key_dest = key_menu;
    m_state = m_bindsetup;
    m_entersound = true;
    return true;
}

int panelButtons(Model m, PanelPictures& pictures, PanelButton* out, int max)
{
    pictures = PanelPictures{};
    if(m == Model::Keyboard || static_cast<int>(m) >= static_cast<int>(Model::Count))
    {
        return 0;
    }
    if(screen.actions.empty())
    {
        buildActions(); // (the actions' names; open() builds them again)
    }
    const ModelInfo& mi = info(m);
    const char* file = artFiles[static_cast<int>(mi.art)];
    pictures.count = mi.vr ? 2 : 1;
    for(int p = 0; p < pictures.count; p++)
    {
        pictures.pic[p] = imagePic(va("gfx/vr/controllers/%s%s_s", file, mi.vr ? (p == 0 ? "_l" : "_r") : ""), &pictures.aspect[p]);
    }
    int n = 0;
    for(const ArtButton& a : artButtons)
    {
        if(a.art != mi.art || n >= max)
        {
            continue;
        }
        PanelButton& b = out[n++];
        const int key = keyFor(a.pic, a.btn);
        b.picture = a.pic == Pic::Right ? 1 : 0;
        b.u = a.u;
        b.v = a.v;
        b.name = a.name;
        b.glyphName = a.glyph;
        b.glyph = imagePic(va("gfx/vr/glyphs/%s", a.glyph));
        b.fixed = key < 0;
        b.action = key < 0 ? "Menu" : bindingOf(key)[0] ? actionName(bindingOf(key)) : "";
    }
    return n;
}

void registerCommands()
{
    Cmd_AddCommand("menu_bindsetup", menu_f);
    Cmd_AddCommand("vr_bindsetup_print", print_f);
    Cmd_AddCommand("vr_bindsetup_detect_test", detectTest_f);
    Cmd_AddCommand("vr_bindsetup_test", bindTest_f);
}

} // namespace qvr::bindsetup

using namespace qvr;
using namespace qvr::bindsetup;

extern "C" void VR_BindSetup_Open()
{
    if(!open())
    {
        M_Menu_Keys_f(); // keyboard and mouse: their bindings, Options > Controls
    }
}

extern "C" void VR_BindSetup_Draw()
{
    // The whole canvas's width (the pictures and their labels need it), clear of the corner's buttons, the status box and
    // the version label; the menus' bounds' height.
    int left, top, width, height;
    M_GetBounds(&left, &top, &width, &height);
    drawtransform_t transform;
    Draw_GetCanvasTransform(CANVAS_MENU, &transform);
    float cl, ct, cr, cb;
    Draw_GetTransformBounds(&transform, &cl, &ct, &cr, &cb);
    const float k = Draw_MenuStretch();
    float l = cl + 8.f, r = cr - 8.f;
    float t = static_cast<float>(top), b = static_cast<float>(top + height);
    if(menuui::toolbarRow())
    {
        t = za::max(t, menuui::toolbarBottom() + 4.f);
    }
    else
    {
        l = za::max(l, menuui::toolbarRight() + 10.f);
    }
    float vx, vy;
    if(menuui::versionLabelClearance(vx, vy))
    {
        if(vy > t + 160.f)
        {
            b = za::min(b, vy - 2.f);
        }
        else
        {
            r = za::min(r, vx - 4.f);
        }
    }
    const float widest = (b - t) * 2.6f; // (no wider than that: on a wide screen the labels stay near their pictures)
    if(r - l > widest)
    {
        l += (r - l - widest) * 0.5f;
        r = l + widest;
    }
    const float w = r - l;
    const float picturesTop = za::max(t + 30.f, menuui::statusBottom(r) + 4.f);
    const float cx = l + w * 0.5f;
    const bool pointer = menuui::pointerOn(true);

    printCentred(cx, t + 4.f, "Controller Bindings", true);
    char buf[128];
    q_snprintf(buf, sizeof(buf), "%s%s", modelName(screen.shown), screen.forced ? "" : " (detected)");
    printCentred(cx, t + 16.f, buf, false);
    const float half = static_cast<float>(ZA_STRLEN(buf)) * 4.f + 14.f;
    screen.prevModel = {cx - half - 8.f, t + 14.f, cx - half + 8.f, t + 26.f};
    screen.nextModel = {cx + half - 8.f, t + 14.f, cx + half + 8.f, t + 26.f};
    M_PrintWhite(static_cast<int>(cx - half - 4.f), static_cast<int>(t + 16.f), "\x10"); // (Quake's [ ] brackets)
    M_PrintWhite(static_cast<int>(cx + half - 4.f), static_cast<int>(t + 16.f), "\x11");

    layoutAndDrawPictures(l + 4.f, picturesTop, w - 8.f, b - 38.f, k);

    // the mouse's (the laser's) press: a drag once it moved a few pixels; its release swaps or opens the list
    if(screen.pressed >= 0)
    {
        if(za::fabs(m_mousex - screen.pressX) + za::fabs(m_mousey - screen.pressY) > 4.f)
        {
            screen.dragging = true;
        }
        if(!keydown[K_MOUSE1])
        {
            release(labelAt(m_mousex, m_mousey));
        }
    }
    drawLabels();

    const char* hint = screen.picked >= 0 ? "Pick the button to swap with (X / Space), or Back"
                       : info(screen.shown).vr
                           ? "Drag a label onto another to swap. Click (or A): pick its action. X: swap by stick"
                           : "Drag a label onto another to swap. Click (or Enter): pick its action";
    printCentred(cx, b - 34.f, hint, false, 6);
    drawButton(screen.resetButton, cx - 70.f, b - 22.f, "Reset to Defaults", pointer && screen.resetButton.has(m_mousex, m_mousey));
    drawButton(screen.backButton, cx + 70.f, b - 22.f, "Back", pointer && screen.backButton.has(m_mousex, m_mousey));

    drawList(l, t, w, b);
}

extern "C" void VR_BindSetup_Key(int key)
{
    if(screen.listOpen)
    {
        const int n = static_cast<int>(screen.filtered.size());
        switch(key)
        {
            case K_ESCAPE:
            case K_BBUTTON:
            case K_YBUTTON: screen.listOpen = false; return;
            case K_UPARROW:
            case K_MWHEELUP: screen.listCursor = za::max(0, screen.listCursor - 1); break;
            case K_DOWNARROW:
            case K_MWHEELDOWN: screen.listCursor = za::min(n - 1, screen.listCursor + 1); break;
            case K_PGUP: screen.listCursor = za::max(0, screen.listCursor - screen.listRows); break;
            case K_PGDN: screen.listCursor = za::min(n - 1, screen.listCursor + screen.listRows); break;
            case K_ENTER:
            case K_KP_ENTER:
            case K_ABUTTON: pickAction(screen.listCursor); return;
            case K_BACKSPACE:
            {
                const int len = static_cast<int>(ZA_STRLEN(screen.search));
                if(len > 0)
                {
                    screen.search[len - 1] = 0;
                    refilter();
                }
                return;
            }
            case K_MOUSE1:
                if(!screen.listBox.has(m_mousex, m_mousey))
                {
                    screen.listOpen = false;
                }
                else if(m_mousey >= screen.listRowsTop)
                {
                    const int row = static_cast<int>((m_mousey - screen.listRowsTop) / rowHeight);
                    if(row < screen.listRows && screen.listScroll + row < n)
                    {
                        pickAction(screen.listScroll + row);
                    }
                }
                return;
            default: return;
        }
        screen.listCursor = za::max(0, screen.listCursor);
        screen.listScroll = za::max(screen.listCursor - screen.listRows + 1, za::min(screen.listScroll, screen.listCursor));
        menuui::uiSound(menuui::UiSound::Move);
        return;
    }
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_YBUTTON:
            if(screen.picked >= 0)
            {
                screen.picked = -1;
                return;
            }
            leave();
            return;
        case K_UPARROW:
        case K_DOWNARROW:
        case K_LEFTARROW:
        case K_RIGHTARROW: moveCursor(key); return;
        case K_TAB: stepModel(keydown[K_SHIFT] ? -1 : 1); return;
        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            if(screen.picked >= 0)
            {
                swapSlots(screen.picked, screen.cursor);
                screen.picked = -1;
                return;
            }
            openList(screen.cursor);
            return;
        case K_SPACE:
        case K_XBUTTON:
            if(screen.picked >= 0)
            {
                swapSlots(screen.picked, screen.cursor);
                screen.picked = -1;
            }
            else if(screen.cursor < static_cast<int>(screen.slots.size()) && screen.slots[screen.cursor].key >= 0)
            {
                screen.picked = screen.cursor;
                menuui::uiSound(menuui::UiSound::Select);
            }
            return;
        case K_MOUSE1:
        {
            if(screen.resetButton.has(m_mousex, m_mousey))
            {
                resetSlots();
                menuui::uiSound(menuui::UiSound::Adjust);
                return;
            }
            if(screen.backButton.has(m_mousex, m_mousey))
            {
                leave();
                return;
            }
            if(screen.prevModel.has(m_mousex, m_mousey) || screen.nextModel.has(m_mousex, m_mousey))
            {
                stepModel(screen.prevModel.has(m_mousex, m_mousey) ? -1 : 1);
                return;
            }
            const int at = labelAt(m_mousex, m_mousey);
            if(at >= 0)
            {
                screen.pressed = at;
                screen.pressX = m_mousex;
                screen.pressY = m_mousey;
                screen.dragging = false;
                screen.cursor = at;
            }
            return;
        }
        default: return;
    }
}

extern "C" void VR_BindSetup_Char(int key)
{
    if(!screen.listOpen || key < 32 || key > 126)
    {
        return;
    }
    const int len = static_cast<int>(ZA_STRLEN(screen.search));
    if(len < static_cast<int>(sizeof(screen.search)) - 1)
    {
        screen.search[len] = static_cast<char>(key);
        screen.search[len + 1] = 0;
        screen.listCursor = 0;
        screen.listScroll = 0;
        refilter();
    }
}

extern "C" int VR_BindSetup_TextEntry()
{
    return screen.listOpen ? 1 : 0;
}
