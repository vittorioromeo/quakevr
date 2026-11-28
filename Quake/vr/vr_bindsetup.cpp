// vr_bindsetup.cpp -- the Controller Bindings screen: see vr_bindsetup.hpp.

#include "vr_bindsetup.hpp"
#include "vr_backend.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_main.hpp"
#include "vr_menuui.hpp"
#include "vr_seat.hpp"
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
ImagePic pics[maxPics]; // (fixed: the texture manager keeps each pic's address) // seat-shared: the pictures loaded (a texture cache)
int picCount = 0; // seat-shared: the pictures loaded

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

// The model the screen shows (its binds' context: a gamepad's flat actions, below).
[[nodiscard]] Model shownModel();

// A gamepad (not a headset's controllers, not the keyboard): its buttons' flat-screen actions (padflat) are the flat keys
// (vr_flat_key_*, their gamepad button), which win over the buttons' binds in flat play; the binds are the headset's.
[[nodiscard]] bool flatPad(Model m)
{
    return m != Model::Keyboard && static_cast<int>(m) < static_cast<int>(Model::Count) && !info(m).vr;
}

// A flat key's cvar by its name (an action's command), nullptr: not a flat key.
[[nodiscard]] cvar_t* flatKeyNamed(const char* command)
{
    for(cvar_t* c : flatplay::keyCvars())
    {
        if(command && !ZA_STRCMP(c->name, command))
        {
            return c;
        }
    }
    return nullptr;
}

// What `key` does with model `m` shown: with a gamepad its flat key (the cvar's name, the action's command) if it has one,
// else its bind; "" nothing.
[[nodiscard]] const char* bindingOf(int key, Model m)
{
    if(key < 0)
    {
        return "";
    }
    if(flatPad(m))
    {
        for(const cvar_t* c : flatplay::keyCvars())
        {
            if(flatplay::keyIs(*c, key))
            {
                return c->name;
            }
        }
    }
    return Key_Seat()->bindings[key] ? Key_Seat()->bindings[key] : "";
}

[[nodiscard]] const char* bindingOf(int key)
{
    return bindingOf(key, shownModel()); // (split21: the current seat's binds and flat keys: the seat whose screen it is)
}

void setKey(int key, const char* command)
{
    if(key < 0)
    {
        return;
    }
    seat::bindsEdited(); // (split20: a seat 2-4's are then its own, saved; split21: its flat keys too)
    if(flatPad(shownModel()))
    {
        if(cvar_t* flat = flatKeyNamed(command))
        {
            flatplay::setPadKey(*flat, key); // (its bind left as it is: the headset's)
            return;
        }
        for(cvar_t* c : flatplay::keyCvars())
        {
            if(flatplay::keyIs(*c, key))
            {
                flatplay::setPadKey(*c, -1); // (a bind now: no flat key on the button)
            }
        }
    }
    Key_SetBinding(key, command && command[0] ? command : nullptr);
    seat::bindsEdited();
}

void swapKeys(int a, int b)
{
    if(a < 0 || b < 0 || a == b)
    {
        return;
    }
    const za::String ba{bindingOf(a)}, bb{bindingOf(b)};
    setKey(a, bb.cStr());
    setKey(b, ba.cStr());
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
    // split20: opened from a seat 2-4's own menu (openInSeat): drawn in its tile, driven by its pad, Back returns there
    bool inSeat{false};
};

// split20: each seat's (seat::Part::BindSetup): the seat that opens the screen edits its own bindings (Key_Seat), its
// own pad's picture; seat 1's is the full menu's (m_bindsetup), a seat 2-4's its seat menu's page.
[[nodiscard]] Screen& scr()
{
    return seat::part<Screen>(seat::current(), seat::Part::BindSetup);
}

// The mouse (the laser) points only on seat 1's screen: seats 2-4 have a pad alone.
[[nodiscard]] bool pointerOn()
{
    return seat::current().index == 0 && pointerOn();
}

// The mouse's buttons and Shift are seat 1's keys.
[[nodiscard]] bool hostDown(int key)
{
    return Key_SeatOf(0)->down[key] != 0;
}

// (panelButtons' model while it reads a panel's buttons: their binds in its context, not the screen's)
Model panelModel{Model::Count}; // seat-shared: set and cleared within one panelButtons call

Model shownModel()
{
    return panelModel != Model::Count ? panelModel : scr().shown;
}

constexpr float rowHeight = 10.f; // the action list's rows (menu y)
constexpr float labelHeight = 19.f; // two lines: the button's name, its action

void buildSlots()
{
    scr().slots.clear();
    const Art art = info(scr().shown).art;
    for(const ArtButton& a : artButtons)
    {
        if(a.art == art)
        {
            scr().slots.pushBack({&a, keyFor(a.pic, a.btn)});
        }
    }
    scr().cursor = 0;
    scr().picked = scr().pressed = -1;
    scr().listOpen = false;
}

void addAction(const char* command, const char* name)
{
    for(const Action& a : scr().actions)
    {
        if(a.command == command)
        {
            return;
        }
    }
    scr().actions.pushBack({za::String{command}, za::String{name && name[0] ? name : command}});
}

void buildActions()
{
    scr().actions.clear();
    scr().actions.pushBack({za::String{}, za::String{"(nothing)"}});
    M_Keys_EachAction(addAction);
    // A gamepad's flat-screen actions (padflat): the flat keys, listed with a gamepad shown (refilter).
    for(int i = 0; i < VR_FlatKeyCount(); i++)
    {
        addAction(VR_FlatKeyCvarName(i), VR_FlatKeyLabel(i));
    }
    int padCount = 0; // split20: a controller seat's flat-play layout's actions by their names (seat::padLayout)
    const seat::PadAction* layout = seat::padLayout(padCount);
    for(int i = 0; i < padCount; i++)
    {
        addAction(layout[i].command, layout[i].label);
    }
}

[[nodiscard]] const char* actionName(const char* binding)
{
    if(!binding[0])
    {
        return "(nothing)";
    }
    for(const Action& a : scr().actions)
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
    if(a < 0 || b < 0 || a >= static_cast<int>(scr().slots.size()) || b >= static_cast<int>(scr().slots.size()) ||
        a == b || scr().slots[a].key < 0 || scr().slots[b].key < 0)
    {
        menuui::uiSound(menuui::UiSound::Deny);
        return;
    }
    swapKeys(scr().slots[a].key, scr().slots[b].key);
    Con_DPrintf("bindsetup: %s and %s swapped\n", Key_KeynumToString(scr().slots[a].key),
        Key_KeynumToString(scr().slots[b].key));
    menuui::uiSound(menuui::UiSound::Adjust);
}

void resetSlots()
{
    for(const Slot& s : scr().slots)
    {
        if(s.key >= 0)
        {
            const za::String d = defaultBinding(s.key);
            Key_SetBinding(s.key, d.empty() ? nullptr : d.cStr());
        }
    }
    if(flatPad(scr().shown))
    {
        flatplay::resetPadKeys(); // (and every flat key's gamepad button its default's: padflat)
    }
}

void refilter()
{
    scr().filtered.clear();
    const bool pad = flatPad(scr().shown);
    for(int i = 0; i < static_cast<int>(scr().actions.size()); i++)
    {
        if(!pad && flatKeyNamed(scr().actions[i].command.cStr()))
        {
            continue; // (the flat keys: a gamepad's in flat play only)
        }
        if(!scr().search[0] || q_strcasestr(scr().actions[i].name.cStr(), scr().search) ||
            q_strcasestr(scr().actions[i].command.cStr(), scr().search))
        {
            scr().filtered.pushBack(i);
        }
    }
    scr().listCursor = za::min(scr().listCursor, za::max(0, static_cast<int>(scr().filtered.size()) - 1));
}

void openList(int slot)
{
    if(slot < 0 || slot >= static_cast<int>(scr().slots.size()) || scr().slots[slot].key < 0)
    {
        menuui::uiSound(menuui::UiSound::Deny);
        return;
    }
    scr().listOpen = true;
    scr().listSlot = slot;
    scr().search[0] = 0;
    scr().listCursor = 0;
    refilter();
    const char* b = bindingOf(scr().slots[slot].key);
    for(int i = 0; i < static_cast<int>(scr().filtered.size()); i++)
    {
        if(scr().actions[scr().filtered[i]].command == b)
        {
            scr().listCursor = i;
        }
    }
    scr().listScroll = za::max(0, scr().listCursor - 4);
    menuui::uiSound(menuui::UiSound::Select);
}

void pickAction(int filteredIndex)
{
    if(filteredIndex >= 0 && filteredIndex < static_cast<int>(scr().filtered.size()) && scr().listSlot >= 0)
    {
        setKey(scr().slots[scr().listSlot].key, scr().actions[scr().filtered[filteredIndex]].command.cStr());
        menuui::uiSound(menuui::UiSound::Adjust);
    }
    scr().listOpen = false;
}

void leave()
{
    scr().listOpen = false;
    if(scr().inSeat)
    {
        scr().inSeat = false; // (the seat menu sees it shut: seatKey's false)
        return;
    }
    menuui::openMenu(scr().from);
}

void stepModel(int dir)
{
    int m = static_cast<int>(scr().shown);
    do
    {
        m = (m + dir + static_cast<int>(Model::Count)) % static_cast<int>(Model::Count);
    } while(m == static_cast<int>(Model::Keyboard));
    scr().shown = static_cast<Model>(m);
    scr().forced = true;
    buildSlots();
    menuui::uiSound(menuui::UiSound::Move);
}

// The mouse (the laser) went down on a label, and came up on `target` (-1: off every label) having moved (a drag) or not
// (a click): a drag swaps the two buttons' bindings, a click opens the action list.
void release(int target)
{
    const int from = scr().pressed;
    const bool drag = scr().dragging;
    scr().pressed = -1;
    scr().dragging = false;
    if(from < 0)
    {
        return;
    }
    if(!drag)
    {
        scr().cursor = from;
        openList(from);
    }
    else if(target >= 0 && target != from)
    {
        scr().cursor = target;
        swapSlots(from, target);
    }
}

[[nodiscard]] int labelAt(float x, float y)
{
    for(const Label& l : scr().labels)
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
    scr().labels.clear();
    const bool vr = info(scr().shown).vr;
    const char* file = artFiles[static_cast<int>(info(scr().shown).art)];
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
        for(int i = 0; i < static_cast<int>(scr().slots.size()); i++)
        {
            const ArtButton& a = *scr().slots[i].art;
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
            scr().labels.pushBack(l);
        }
    }
}

void drawLabels()
{
    const bool pointer = pointerOn();
    const int hover = pointer ? labelAt(m_mousex, m_mousey) : -1;
    for(const Label& l : scr().labels)
    {
        const Slot& s = scr().slots[l.slot];
        const bool selected = l.slot == scr().cursor && !pointer;
        const bool hot = l.slot == hover || selected || l.slot == scr().picked;
        const bool target = scr().dragging && l.slot == hover && l.slot != scr().pressed;
        // the leader line, from the box's inner edge to the button
        const float ex = l.leftSide ? l.x1 : l.x0, ey = (l.y0 + l.y1) * 0.5f;
        line(ex, ey, l.ax, l.ay, 0.85f, 0.55f, 0.3f, hot ? 0.95f : 0.55f);
        fill(l.ax - 1.5f, l.ay - 1.5f, 3.f, 3.f, 1.f, 0.7f, 0.35f, 1.f);
        // the box
        fill(l.x0, l.y0, l.x1 - l.x0, l.y1 - l.y0, 0.05f, 0.04f, 0.04f, 0.82f);
        if(target || l.slot == scr().picked)
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
    if(scr().dragging && scr().pressed >= 0)
    {
        const Slot& s = scr().slots[scr().pressed];
        const char* action = actionName(bindingOf(s.key));
        const float w = static_cast<float>(ZA_STRLEN(action)) * 7.f + 8.f;
        fill(m_mousex + 6.f, m_mousey + 4.f, w, 11.f, 0.9f, 0.5f, 0.15f, 0.85f);
        M_PrintWhiteEx(static_cast<int>(m_mousex + 10.f), static_cast<int>(m_mousey + 6.f), 7, action);
    }
}

void drawList(float left, float top, float width, float bottom)
{
    if(!scr().listOpen || scr().listSlot < 0)
    {
        return;
    }
    const Slot& s = scr().slots[scr().listSlot];
    const float w = za::min(width - 40.f, 300.f);
    const float x0 = left + (width - w) * 0.5f;
    scr().listRows = za::max(4, static_cast<int>((bottom - top - 60.f) / rowHeight));
    scr().listRows = za::min(scr().listRows, 16);
    const float h = 34.f + scr().listRows * rowHeight;
    const float y0 = top + (bottom - top - h) * 0.5f;
    scr().listBox = {x0, y0, x0 + w, y0 + h};
    fill(x0, y0, w, h, 0.04f, 0.03f, 0.03f, 0.95f);
    frame(scr().listBox, 1.f, 0.65f, 0.3f, 1.f);
    char buf[96];
    q_snprintf(buf, sizeof(buf), "%s: pick an action", s.art->name);
    printCentred(x0 + w * 0.5f, y0 + 4.f, buf, true);
    q_snprintf(buf, sizeof(buf), "Search: %s%s", scr().search, (static_cast<int>(realtime * 4.0) & 1) ? "_" : " ");
    M_Print(static_cast<int>(x0 + 6.f), static_cast<int>(y0 + 16.f), buf);
    scr().listRowsTop = y0 + 28.f;
    const int n = static_cast<int>(scr().filtered.size());
    scr().listScroll = za::max(0, za::min(scr().listScroll, n - scr().listRows));
    const bool pointer = pointerOn();
    const char* current = bindingOf(s.key);
    for(int r = 0; r < scr().listRows && scr().listScroll + r < n; r++)
    {
        const int i = scr().listScroll + r;
        const Action& a = scr().actions[scr().filtered[i]];
        const float y = scr().listRowsTop + r * rowHeight;
        const bool hovered = pointer && m_mousex >= x0 && m_mousex < x0 + w && m_mousey >= y && m_mousey < y + rowHeight;
        if(hovered)
        {
            scr().listCursor = i;
        }
        if(i == scr().listCursor)
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
        M_Print(static_cast<int>(x0 + 18.f), static_cast<int>(scr().listRowsTop + 1.f), "No action matches");
    }
}

// The labels in the keys' order: a column top to bottom, then the other.
[[nodiscard]] int labelIndex(int slot)
{
    for(int i = 0; i < static_cast<int>(scr().labels.size()); i++)
    {
        if(scr().labels[i].slot == slot)
        {
            return i;
        }
    }
    return 0;
}

void moveCursor(int key)
{
    const int n = static_cast<int>(scr().labels.size());
    if(n == 0)
    {
        return;
    }
    const Label& at = scr().labels[labelIndex(scr().cursor)];
    int best = -1;
    float bestScore = 1e9f;
    for(int i = 0; i < n; i++)
    {
        const Label& l = scr().labels[i];
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
        scr().cursor = best;
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
    Model m = scr().shown;
    if(Cmd_Argc() > 1 && !modelFromArgs(1, m))
    {
        Con_Printf("vr_bindsetup_print [model]: unknown model\n");
        return;
    }
    const Model keep = scr().shown;
    scr().shown = m;
    buildSlots();
    if(scr().actions.empty())
    {
        buildActions();
    }
    Con_Printf("bindsetup: %s (%s), detected %s\n", info(m).name, info(m).id, info(detect()).id);
    for(const Slot& s : scr().slots)
    {
        const char* side = s.art->pic == Pic::Left ? "left " : s.art->pic == Pic::Right ? "right " : "";
        Con_Printf("  %s%s: %s = \"%s\" (%s)\n", side, s.art->name, s.key >= 0 ? Key_KeynumToString(s.key) : "-",
            bindingOf(s.key), s.key >= 0 ? actionName(bindingOf(s.key)) : "the menu");
    }
    scr().shown = keep;
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

// Every flat key's value, put back by restore() (the tests change them).
struct FlatKeysSaved
{
    za::Vector<za::String> values;
    FlatKeysSaved()
    {
        for(const cvar_t* c : flatplay::keyCvars())
        {
            values.pushBack(za::String{c->string});
        }
    }
    void restore() const
    {
        int i = 0;
        for(cvar_t* c : flatplay::keyCvars())
        {
            Cvar_SetQuick(c, values[static_cast<za::SizeT>(i++)].cStr());
        }
    }
};

// vr_flat_pad_test (padflat; Misc/quakevr/flatpad_test.sh, flat play in a map): the flat-screen gamepad's default layout
// (FLATPLAY.md "A gamepad"): every common action on a button, no button twice, the alt layer and its key handling, the
// defaults filled into an older config, the Controller Bindings screen's view of them.
void padTest_f()
{
    testkit::Tally t;
    auto check = [&](bool ok, const char* what) { t.check(ok, "flat_pad_test", what); };
    const FlatKeysSaved flatSaved;
    for(cvar_t* c : flatplay::keyCvars())
    {
        Cvar_SetQuick(c, c->default_string);
    }
    // 1. Every listed action's button: the flat keys' (their defaults' gamepad buttons) and the shipped binds'.
    const struct
    {
        const char* what;
        const cvar_t* flat; // nullptr: a bind (vr_bindings.cfg)
        int key;
    } layout[] = {{"fire (main hand)", nullptr, K_RTRIGGER}, {"fire / ironsight / parry (off hand)", nullptr, K_LTRIGGER},
        {"jump", nullptr, K_ABUTTON}, {"alt layer", nullptr, K_BACK}, {"scoreboard", nullptr, K_RTHUMB_ALT},
        {"crouch / slide", &vr_flat_key_crouch, K_BBUTTON}, {"lie down", &vr_flat_key_prone, K_ABUTTON_ALT},
        {"reload", &vr_flat_key_reload, K_XBUTTON}, {"force grab", &vr_flat_key_grab, K_DPAD_RIGHT},
        {"shove", &vr_flat_key_shove, K_YBUTTON}, {"throw / drop", &vr_flat_key_throw, K_DPAD_LEFT},
        {"use (grab / pull)", &vr_flat_key_pull, K_BBUTTON_ALT}, {"left weapon wheel", &vr_flat_key_cycle_left, K_LSHOULDER},
        {"right weapon wheel", &vr_flat_key_cycle_right, K_RSHOULDER}, {"holster menu", &vr_flat_key_holster_menu, K_LTHUMB},
        {"swap hands", &vr_flat_key_swap, K_YBUTTON_ALT}, {"flashlight", &vr_flat_key_flashlight, K_DPAD_UP},
        {"quick grenade", &vr_flat_key_grenade, K_DPAD_DOWN}, {"next ammo", &vr_flat_key_ammo, K_XBUTTON_ALT},
        {"melee (punch)", &vr_flat_key_punch, K_RTHUMB}, {"attach", &vr_flat_key_attach, K_LSHOULDER_ALT},
        {"bullet time", &vr_flat_key_bullettime, K_RSHOULDER_ALT}};
    for(const auto& l : layout)
    {
        const za::String bind = defaultBinding(l.key);
        const bool ok = l.flat ? flatplay::keyNumPad(*l.flat) == l.key : !bind.empty();
        check(ok, va("%s on %s (%s)", l.what, Key_KeynumToString(l.key), l.flat ? l.flat->name : bind.cStr()));
    }
    // 2. No gamepad button on two flat keys, nor on a flat key and a bind flat play needs (fire, jump, the alt layer).
    int dupes = 0, clash = 0, pads = 0;
    for(int k = K_GAMEPAD_BEGIN; k < K_GAMEPAD_END; k++)
    {
        int owners = 0;
        for(const cvar_t* c : flatplay::keyCvars())
        {
            owners += flatplay::keyIs(*c, k) ? 1 : 0;
        }
        pads += owners > 0 ? 1 : 0;
        dupes += owners > 1 ? 1 : 0;
        clash += owners > 0 && (k == K_RTRIGGER || k == K_LTRIGGER || k == K_ABUTTON || k == K_BACK || k == K_START) ? 1 : 0;
    }
    check(dupes == 0 && clash == 0 && pads == 17, va("%d buttons on flat keys, %d on two, %d on fire/jump/alt/menu", pads, dupes, clash));
    // every flat key's default: two keyboard keys at most, one gamepad button at most
    int badSlots = 0;
    for(const cvar_t* c : flatplay::keyCvars())
    {
        int pad = 0;
        for(int s = 0; s < flatplay::keySlots; s++)
        {
            pad += flatplay::isPadKey(flatplay::keyNum(*c, s)) ? 1 : 0;
        }
        badSlots += pad > 1 || flatplay::keyNumKeyboard(*c, 2) >= 0 ? 1 : 0;
    }
    check(badSlots == 0, va("every flat key: <= 2 keyboard keys and <= 1 button (%d not)", badSlots));
    // 3. Key handling in flat play: B is the crouch's (its bind does not run), the alt layer turns X into X (alt), and
    // letting go of the alt button first still lets go of X (alt).
    check(flatplay::active() && VR_FlatKeyOwned(K_BBUTTON) && VR_FlatKeyOwned(K_XBUTTON_ALT) && !VR_FlatKeyOwned(K_ABUTTON),
        "flat play owns B and X (alt), not A (jump's bind)");
    check(!ZA_STRCMP(defaultBinding(K_BACK).cStr(), "+altmodifier"), "Back's shipped bind: +altmodifier");
    Key_Seat()->altmodifier = true; // (Back held: its bind's +altmodifier)
    Key_Event(K_XBUTTON, true);
    const bool altDown = Key_Seat()->down[K_XBUTTON_ALT] && !Key_Seat()->down[K_XBUTTON] && flatplay::keyHeld(vr_flat_key_ammo) &&
                         !flatplay::keyHeld(vr_flat_key_reload);
    Key_Seat()->altmodifier = false; // (Back let go first)
    Key_Event(K_XBUTTON, false);
    check(altDown && !Key_Seat()->down[K_XBUTTON_ALT] && !Key_Seat()->down[K_XBUTTON], "Back held: X is next ammo (X alt), let go after Back: released");
    Key_Event(K_XBUTTON, true);
    const bool plain = Key_Seat()->down[K_XBUTTON] && flatplay::keyHeld(vr_flat_key_reload);
    Key_Event(K_XBUTTON, false);
    check(plain && !Key_Seat()->down[K_XBUTTON], "X alone: reload");
    // 4. An older config: a flat key of its own without a button gets its default's; one whose button another has not.
    Cvar_SetQuick(&vr_flat_key_reload, "r t");
    Cvar_SetQuick(&vr_flat_key_grenade, "g");
    Cvar_SetQuick(&vr_flat_key_flashlight, "f DPAD_DOWN"); // (the grenade's button taken)
    Cvar_SetValueQuick(&vr_flat_pad_defaults_version, 0.f);
    flatplay::fillPadDefaults();
    check(!ZA_STRCMP(vr_flat_key_reload.string, "r t XBUTTON") && !ZA_STRCMP(vr_flat_key_grenade.string, "g") &&
              !ZA_STRCMP(vr_flat_key_flashlight.string, "f DPAD_DOWN") && vr_flat_pad_defaults_version.value == 1.f,
        va("fill: reload \"%s\", grenade \"%s\", flashlight \"%s\"", vr_flat_key_reload.string, vr_flat_key_grenade.string,
            vr_flat_key_flashlight.string));
    Cvar_SetQuick(&vr_flat_key_reload, "r t");
    flatplay::fillPadDefaults();
    check(!ZA_STRCMP(vr_flat_key_reload.string, "r t"), "fill: once only");
    flatplay::resetPadKeys();
    check(!ZA_STRCMP(vr_flat_key_reload.string, "r t XBUTTON") && !ZA_STRCMP(vr_flat_key_grenade.string, "g DPAD_DOWN") &&
              !ZA_STRCMP(vr_flat_key_flashlight.string, "f DPAD_UP"),
        "reset: every button its default's, the keyboard keys kept");
    for(cvar_t* c : flatplay::keyCvars())
    {
        Cvar_SetQuick(c, c->default_string);
    }
    // 5. The Controller Bindings screen and the pad's reminder panel name the flat actions.
    if(scr().actions.empty())
    {
        buildActions();
    }
    PanelPictures pics;
    PanelButton buttons[40];
    const int n = panelButtons(Model::XboxOne, pics, buttons, 40);
    bool crouch = false, wheel = false, jump = false;
    for(int i = 0; i < n; i++)
    {
        crouch = crouch || !ZA_STRCMP(buttons[i].action, "Crouch / slide");
        wheel = wheel || !ZA_STRCMP(buttons[i].action, "Cycle right hand");
        jump = jump || !ZA_STRCMP(buttons[i].action, "Jump / swim up");
    }
    check(crouch && wheel && jump, va("xboxone panel: Crouch / slide %d, Cycle right hand %d, Jump %d", crouch, wheel, jump));
    check(!ZA_STRCMP(bindingOf(K_BBUTTON, Model::Quest3), Key_Seat()->bindings[K_BBUTTON] ? Key_Seat()->bindings[K_BBUTTON] : ""),
        "quest3: B shows its bind (a headset's)");
    flatSaved.restore();
    t.verdict("flat_pad_test");
}

// vr_menu_pad_test (padflat; Misc/quakevr/flatpad_test.sh, flat screen): the menus' gamepad cursor on this screen with an
// Xbox pad shown, a stage a frame (stepped at the end of VR_BindSetup_Draw, the labels laid out): the stick moves it, it
// is steered onto B's label and A clicks (the action list opens), the d-pad steps the labels and hides it.
struct MenuPadTest
{
    int stage{0}; // 0 not running
    int frames{0};
    testkit::Tally tally;
    float startX{0.f};
    int target{-1}; // B's slot
    int cursorWas{-1};
};
MenuPadTest menuPadTest; // seat-shared: a test's own state

void menuPadCheck(bool ok, const char* what)
{
    if(menuPadTest.tally.check(ok, "menu_pad_test", what))
    {
        Con_Printf("menu_pad_test: PASS %s\n", what);
    }
}

void menuPadTest_f()
{
    menuPadTest = MenuPadTest{};
    menuPadTest.stage = open(Model::XboxOne) ? 1 : 0;
    if(!menuPadTest.stage)
    {
        menuPadCheck(false, "the screen opens");
        menuPadTest.tally.verdict("menu_pad_test");
    }
}

void menuPadStep()
{
    MenuPadTest& t = menuPadTest;
    if(t.stage <= 0)
    {
        return;
    }
    constexpr float dt = 1.f / 72.f; // (a fixed frame's: the stick's moves the same at any frame rate)
    float px, py;
    t.frames++;
    if(t.frames > 900)
    {
        menuPadCheck(false, va("timed out at stage %d", t.stage));
        t.stage = 99;
    }
    switch(t.stage)
    {
        case 1: // hidden to start with; the stick right for 12 frames
            VR_MenuPadHide();
            menuPadCheck(!menuui::padCursor(px, py), "hidden before the stick moves");
            t.startX = m_mousex;
            t.stage = 2;
            t.frames = 0;
            break;
        case 2:
            (void)VR_MenuPadStick(1.f, 0.f, dt);
            if(t.frames >= 12)
            {
                menuPadCheck(menuui::padCursor(px, py) && m_mousex > t.startX + 4.f,
                    va("the stick moved it right: menu x %.0f -> %.0f, shown %d", t.startX, m_mousex, menuui::padCursor(px, py)));
                for(int i = 0; i < static_cast<int>(scr().slots.size()); i++)
                {
                    t.target = scr().slots[i].art->btn == Btn::B ? i : t.target;
                }
                t.stage = 3;
                t.frames = 0;
            }
            break;
        case 3: // steered onto B's label
        {
            const Label* goal = nullptr;
            for(const Label& l : scr().labels)
            {
                goal = l.slot == t.target ? &l : goal;
            }
            if(!goal)
            {
                menuPadCheck(false, "B's label laid out");
                t.stage = 99;
                break;
            }
            if(labelAt(m_mousex, m_mousey) == t.target)
            {
                menuPadCheck(true, va("steered onto B's label in %d frames", t.frames));
                t.stage = 4;
                break;
            }
            const float dx = (goal->x0 + goal->x1) * 0.5f - m_mousex, dy = (goal->y0 + goal->y1) * 0.5f - m_mousey;
            const float d = za::sqrt(dx * dx + dy * dy);
            const float tilt = za::clamp(d / 30.f, 0.45f, 1.f);
            (void)VR_MenuPadStick(dx / d * tilt, dy / d * tilt, dt);
            break;
        }
        case 4: // A pressed: the left mouse button on the label
        {
            const int k = VR_MenuPadKey(K_ABUTTON, 1);
            menuPadCheck(k == K_MOUSE1, va("A sends the left mouse button (%s)", Key_KeynumToString(k)));
            if(k >= 0)
            {
                Key_Event(k, true);
            }
            t.stage = 5;
            break;
        }
        case 5: // ... and let go: the click (the draw's release, next frame) opens the list
        {
            const int k = VR_MenuPadKey(K_ABUTTON, 0);
            if(k >= 0)
            {
                Key_Event(k, false);
            }
            t.stage = 6;
            break;
        }
        case 6:
            menuPadCheck(scr().listOpen && scr().listSlot == t.target && !Key_Seat()->down[K_MOUSE1],
                va("A clicked B's label: its action list opens (open %d, slot %d)", scr().listOpen, scr().listSlot));
            scr().listOpen = false;
            t.cursorWas = scr().cursor;
            {
                const int k = VR_MenuPadKey(K_DPAD_DOWN, 1); // (the d-pad: discrete, as in_sdl.c sends it)
                Key_Event(k, true);
                Key_Event(k, false);
            }
            t.stage = 7;
            break;
        case 7:
            menuPadCheck(scr().cursor != t.cursorWas && !menuui::padCursor(px, py),
                va("the d-pad steps the labels (%d -> %d) and hides the cursor", t.cursorWas, scr().cursor));
            t.stage = 99;
            break;
        default:
            break;
    }
    if(t.stage == 99)
    {
        t.tally.verdict("menu_pad_test");
        t.stage = 0;
    }
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
        saved[i] = Key_Seat()->bindings[keys[i]] ? Key_Seat()->bindings[keys[i]] : "";
    }
    const FlatKeysSaved flatSaved;
    const float swapWas = vr_button_swap.value;
    Cvar_SetValueQuick(&vr_button_swap, 0.f);
    const Model keep = scr().shown;
    buildActions();

    auto slotOf = [&](Pic pic, Btn b) {
        for(int i = 0; i < static_cast<int>(scr().slots.size()); i++)
        {
            if(scr().slots[i].art->pic == pic && scr().slots[i].art->btn == b)
            {
                return i;
            }
        }
        return -1;
    };
    // the Quest 3: the right trigger dragged onto A swaps their bindings
    scr().shown = Model::Quest3;
    buildSlots();
    Key_SetBinding(K_RTRIGGER, "+attack");
    Key_SetBinding(K_ABUTTON, "+jump");
    const int trig = slotOf(Pic::Right, Btn::Trigger), a = slotOf(Pic::Right, Btn::Primary);
    check(trig >= 0 && a >= 0 && scr().slots[trig].key == K_RTRIGGER && scr().slots[a].key == K_ABUTTON,
        "quest3: right trigger and A are RTRIGGER and ABUTTON");
    scr().pressed = trig;
    scr().dragging = true;
    release(a);
    check(!ZA_STRCMP(bindingOf(K_RTRIGGER), "+jump") && !ZA_STRCMP(bindingOf(K_ABUTTON), "+attack"),
        "drag trigger onto A: swapped");
    // a click opens the list; picking an action binds it
    const int stick = slotOf(Pic::Right, Btn::Stick);
    scr().pressed = stick;
    scr().dragging = false;
    release(stick);
    check(scr().listOpen && scr().listSlot == stick, "click: the action list opens");
    q_strlcpy(scr().search, "Next weapon", sizeof(scr().search));
    refilter();
    check(!scr().filtered.empty(), "search: Next weapon found");
    if(!scr().filtered.empty())
    {
        pickAction(0);
    }
    check(!ZA_STRCMP(bindingOf(K_RTHUMB), "impulse 10") && !scr().listOpen, "list pick: RTHUMB is impulse 10");
    // the list has the VR actions (bindlist.lst) and Quake's
    bool grab = false, attack = false;
    for(const Action& act : scr().actions)
    {
        grab = grab || act.command == "+grabmain";
        attack = attack || act.command == "+attack";
    }
    check(grab && attack, "the actions list has +grabmain and +attack");
    // the menu button is fixed
    const int menu = slotOf(Pic::Left, Btn::Menu);
    check(menu >= 0 && scr().slots[menu].key < 0, "the left menu button is fixed");
    // Reset to Defaults: vr_bindings.cfg's
    resetSlots();
    check(!ZA_STRCMP(bindingOf(K_RTRIGGER), "+attack") && !ZA_STRCMP(bindingOf(K_ABUTTON), "+jump") &&
              !ZA_STRCMP(bindingOf(K_RTHUMB), "+reloadmain") && !ZA_STRCMP(bindingOf(K_LSHOULDER), "+graboff"),
        "reset: the shipped VR bindings");
    // Swap Face Buttons: A sends XBUTTON
    Cvar_SetValueQuick(&vr_button_swap, 1.f);
    buildSlots();
    check(scr().slots[slotOf(Pic::Right, Btn::Primary)].key == K_XBUTTON, "button swap: A's key is XBUTTON");
    Cvar_SetValueQuick(&vr_button_swap, 0.f);
    // a gamepad (padflat): its buttons' flat actions (the flat keys' gamepad buttons): X onto B swaps them, then reset
    scr().shown = Model::XboxOne;
    buildSlots();
    resetSlots();
    check(!ZA_STRCMP(bindingOf(K_XBUTTON), "vr_flat_key_reload") && !ZA_STRCMP(bindingOf(K_BBUTTON), "vr_flat_key_crouch"),
        "xboxone: X reloads, B crouches (flat keys)");
    swapSlots(slotOf(Pic::Pad, Btn::X), slotOf(Pic::Pad, Btn::B));
    check(!ZA_STRCMP(bindingOf(K_XBUTTON), "vr_flat_key_crouch") && !ZA_STRCMP(bindingOf(K_BBUTTON), "vr_flat_key_reload") &&
              !ZA_STRCMP(vr_flat_key_reload.string, "r BBUTTON") && !ZA_STRCMP(vr_flat_key_crouch.string, "c CTRL XBUTTON"),
        "xboxone: X and B swapped (the flat keys' buttons, their keyboard keys kept)");
    check(slotOf(Pic::Pad, Btn::Start) >= 0 && scr().slots[slotOf(Pic::Pad, Btn::Start)].key < 0, "xboxone: Start fixed");
    // a bind onto a flat key's button: A (jump, a bind) onto Y (shove, a flat key)
    swapSlots(slotOf(Pic::Pad, Btn::A), slotOf(Pic::Pad, Btn::Y));
    check(!ZA_STRCMP(bindingOf(K_ABUTTON), "vr_flat_key_shove") && !ZA_STRCMP(bindingOf(K_YBUTTON), "+jump"),
        "xboxone: A and Y swapped (a bind and a flat key)");
    resetSlots();
    check(!ZA_STRCMP(bindingOf(K_XBUTTON), "vr_flat_key_reload") && !ZA_STRCMP(bindingOf(K_BBUTTON), "vr_flat_key_crouch") &&
              !ZA_STRCMP(bindingOf(K_DPAD_LEFT), "vr_flat_key_throw") && !ZA_STRCMP(bindingOf(K_ABUTTON), "+jump") &&
              !ZA_STRCMP(bindingOf(K_YBUTTON), "vr_flat_key_shove") && !ZA_STRCMP(Key_Seat()->bindings[K_BACK] ? Key_Seat()->bindings[K_BACK] : "", "+altmodifier"),
        "xboxone reset: the shipped flat pad layout (D-pad left throws, Back the alt layer)");
    check(!ZA_STRCMP(vr_flat_key_reload.string, "r XBUTTON") && !ZA_STRCMP(vr_flat_key_crouch.string, "c CTRL BBUTTON"),
        "xboxone reset: the flat keys' keyboard keys kept");
    // the list: a gamepad's has the flat actions, a headset's not
    q_strlcpy(scr().search, "Quick grenade", sizeof(scr().search));
    refilter();
    check(!scr().filtered.empty(), "xboxone list: Quick grenade (a flat key) listed");
    scr().shown = Model::Quest3;
    refilter();
    check(scr().filtered.empty(), "quest3 list: no flat keys");
    scr().search[0] = 0;

    for(int i = 0; i < static_cast<int>(sizeof(keys) / sizeof(keys[0])); i++)
    {
        Key_SetBinding(keys[i], saved[i].empty() ? nullptr : saved[i].cStr());
    }
    flatSaved.restore();
    Cvar_SetValueQuick(&vr_button_swap, swapWas);
    scr().shown = keep;
    buildSlots();
    scr().listOpen = false;
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
    scr().forced = model != Model::Count;
    scr().shown = shown;
    if(m_state != m_bindsetup)
    {
        scr().from = m_state == m_none ? m_none : static_cast<int>(m_state);
    }
    buildActions();
    buildSlots();
    IN_DeactivateForMenu();
    key_dest = key_menu;
    m_state = m_bindsetup;
    m_entersound = true;
    return true;
}

void drawScreen(); // (below)

bool openInSeat()
{
    if(seat::current().index <= 0)
    {
        return false;
    }
    Screen& s = scr();
    s.shown = modelForPad(IN_GetGamepadSdlType()); // (the seat's own pad's picture)
    s.shown = s.shown == Model::Keyboard ? Model::GenericPad : s.shown;
    s.forced = false;
    s.inSeat = true;
    s.picked = s.pressed = -1;
    s.dragging = false;
    buildActions();
    buildSlots();
    Con_DPrintf("bindsetup: seat %d's screen (%s)\n", seat::current().index + 1, info(s.shown).id);
    return true;
}

int cursorKey()
{
    const Screen& s = scr();
    return s.cursor >= 0 && s.cursor < static_cast<int>(s.slots.size()) ? s.slots[s.cursor].key : -1;
}

bool seatScreenOpen()
{
    return scr().inSeat;
}

void seatDraw()
{
    if(scr().inSeat)
    {
        drawScreen();
    }
}

bool seatKey(int key)
{
    if(!scr().inSeat)
    {
        return false;
    }
    switch(key) // (a pad's d-pad as the arrows; Start as Back; the shoulders step the picture)
    {
        case K_DPAD_UP: key = K_UPARROW; break;
        case K_DPAD_DOWN: key = K_DOWNARROW; break;
        case K_DPAD_LEFT: key = K_LEFTARROW; break;
        case K_DPAD_RIGHT: key = K_RIGHTARROW; break;
        case K_LSHOULDER:
        case K_RSHOULDER:
            if(!scr().listOpen)
            {
                stepModel(key == K_LSHOULDER ? -1 : 1);
                return true;
            }
            key = key == K_LSHOULDER ? K_PGUP : K_PGDN;
            break;
        default: break;
    }
    VR_BindSetup_Key(key);
    return scr().inSeat;
}

int panelButtons(Model m, PanelPictures& pictures, PanelButton* out, int max)
{
    pictures = PanelPictures{};
    if(m == Model::Keyboard || static_cast<int>(m) >= static_cast<int>(Model::Count))
    {
        return 0;
    }
    if(scr().actions.empty())
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
    panelModel = m;
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
    panelModel = Model::Count;
    return n;
}

void registerCommands()
{
    Cmd_AddCommand("menu_bindsetup", menu_f);
    Cmd_AddCommand("vr_bindsetup_print", print_f);
    Cmd_AddCommand("vr_bindsetup_detect_test", detectTest_f);
    Cmd_AddCommand("vr_bindsetup_test", bindTest_f);
    Cmd_AddCommand("vr_flat_pad_test", padTest_f);
    Cmd_AddCommand("vr_menu_pad_test", menuPadTest_f);
    Cmd_AddCommand("vr_menu_pad_report", menuui::padCursorReport);
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

namespace qvr::bindsetup
{
void drawScreen()
{
    // The whole canvas's width (the pictures and their labels need it), clear of the corner's buttons, the status box and
    // the version label; the menus' bounds' height. (split20: in a seat's tile, its canvas: the full menu's toolbar and
    // labels are seat 1's.)
    drawtransform_t transform;
    Draw_GetCanvasTransform(CANVAS_MENU, &transform);
    float canvL, canvT, canvR, canvB;
    Draw_GetTransformBounds(&transform, &canvL, &canvT, &canvR, &canvB);
    const float k = Draw_MenuStretch();
    float l = canvL + 8.f, r = canvR - 8.f;
    float t = canvT + 4.f, b = canvB - 4.f;
    if(!scr().inSeat)
    {
        int left, top, width, height;
        M_GetBounds(&left, &top, &width, &height);
        t = static_cast<float>(top);
        b = static_cast<float>(top + height);
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
    }
    else
    {
        fill(canvL, canvT, canvR - canvL, canvB - canvT, 0.f, 0.f, 0.f, 0.7f); // (over the seat's game, as the full menu's backdrop)
    }
    const float widest = (b - t) * 2.6f; // (no wider than that: on a wide screen the labels stay near their pictures)
    if(r - l > widest)
    {
        l += (r - l - widest) * 0.5f;
        r = l + widest;
    }
    const float w = r - l;
    const float picturesTop = scr().inSeat ? t + 30.f : za::max(t + 30.f, menuui::statusBottom(r) + 4.f);
    const float cx = l + w * 0.5f;
    const bool pointer = pointerOn();

    char buf[128];
    // split20: whose bindings, while more than one plays (each seat's are its own)
    if(seat::inPlay() > 1)
    {
        q_snprintf(buf, sizeof(buf), "Controller Bindings - Player %d", seat::current().index + 1);
        printCentred(cx, t + 4.f, buf, true);
    }
    else
    {
        printCentred(cx, t + 4.f, "Controller Bindings", true);
    }
    q_snprintf(buf, sizeof(buf), "%s%s", modelName(scr().shown), scr().forced ? "" : " (detected)");
    printCentred(cx, t + 16.f, buf, false);
    const float half = static_cast<float>(ZA_STRLEN(buf)) * 4.f + 14.f;
    scr().prevModel = {cx - half - 8.f, t + 14.f, cx - half + 8.f, t + 26.f};
    scr().nextModel = {cx + half - 8.f, t + 14.f, cx + half + 8.f, t + 26.f};
    M_PrintWhite(static_cast<int>(cx - half - 4.f), static_cast<int>(t + 16.f), "\x10"); // (Quake's [ ] brackets)
    M_PrintWhite(static_cast<int>(cx + half - 4.f), static_cast<int>(t + 16.f), "\x11");

    layoutAndDrawPictures(l + 4.f, picturesTop, w - 8.f, b - 38.f, k);

    // the mouse's (the laser's) press: a drag once it moved a few pixels; its release swaps or opens the list
    if(scr().pressed >= 0)
    {
        if(za::fabs(m_mousex - scr().pressX) + za::fabs(m_mousey - scr().pressY) > 4.f)
        {
            scr().dragging = true;
        }
        if(!hostDown(K_MOUSE1))
        {
            release(labelAt(m_mousex, m_mousey));
        }
    }
    drawLabels();

    const char* hint = scr().picked >= 0 ? "Pick the button to swap with (X / Space), or Back"
                       : scr().inSeat ? "A: pick its action. X: swap with another. Shoulders: picture. B: back"
                       : info(scr().shown).vr
                           ? "Drag a label onto another to swap. Click (or A): pick its action. X: swap by stick"
                           : "Drag a label onto another to swap. Click (or Enter): pick its action";
    printCentred(cx, b - 34.f, hint, false, 6);
    if(!scr().inSeat) // (a seat's pad: B is Back; the defaults are seat 1's controllers')
    {
        drawButton(scr().resetButton, cx - 70.f, b - 22.f, "Reset to Defaults", pointer && scr().resetButton.has(m_mousex, m_mousey));
        drawButton(scr().backButton, cx + 70.f, b - 22.f, "Back", pointer && scr().backButton.has(m_mousex, m_mousey));
    }

    drawList(l, t, w, b);
    menuPadStep(); // (vr_menu_pad_test, a stage a frame)
}
} // namespace qvr::bindsetup

extern "C" void VR_BindSetup_Draw()
{
    drawScreen();
}

extern "C" void VR_BindSetup_Key(int key)
{
    if(scr().listOpen)
    {
        const int n = static_cast<int>(scr().filtered.size());
        switch(key)
        {
            case K_ESCAPE:
            case K_BBUTTON:
            case K_YBUTTON: scr().listOpen = false; return;
            case K_UPARROW:
            case K_MWHEELUP: scr().listCursor = za::max(0, scr().listCursor - 1); break;
            case K_DOWNARROW:
            case K_MWHEELDOWN: scr().listCursor = za::min(n - 1, scr().listCursor + 1); break;
            case K_PGUP: scr().listCursor = za::max(0, scr().listCursor - scr().listRows); break;
            case K_PGDN: scr().listCursor = za::min(n - 1, scr().listCursor + scr().listRows); break;
            case K_ENTER:
            case K_KP_ENTER:
            case K_ABUTTON: pickAction(scr().listCursor); return;
            case K_BACKSPACE:
            {
                const int len = static_cast<int>(ZA_STRLEN(scr().search));
                if(len > 0)
                {
                    scr().search[len - 1] = 0;
                    refilter();
                }
                return;
            }
            case K_MOUSE1:
                if(!scr().listBox.has(m_mousex, m_mousey))
                {
                    scr().listOpen = false;
                }
                else if(m_mousey >= scr().listRowsTop)
                {
                    const int row = static_cast<int>((m_mousey - scr().listRowsTop) / rowHeight);
                    if(row < scr().listRows && scr().listScroll + row < n)
                    {
                        pickAction(scr().listScroll + row);
                    }
                }
                return;
            default: return;
        }
        scr().listCursor = za::max(0, scr().listCursor);
        scr().listScroll = za::max(scr().listCursor - scr().listRows + 1, za::min(scr().listScroll, scr().listCursor));
        menuui::uiSound(menuui::UiSound::Move);
        return;
    }
    switch(key)
    {
        case K_ESCAPE:
        case K_BBUTTON:
        case K_YBUTTON:
            if(scr().picked >= 0)
            {
                scr().picked = -1;
                return;
            }
            leave();
            return;
        case K_UPARROW:
        case K_DOWNARROW:
        case K_LEFTARROW:
        case K_RIGHTARROW: moveCursor(key); return;
        case K_TAB: stepModel(hostDown(K_SHIFT) ? -1 : 1); return;
        case K_ENTER:
        case K_KP_ENTER:
        case K_ABUTTON:
            if(scr().picked >= 0)
            {
                swapSlots(scr().picked, scr().cursor);
                scr().picked = -1;
                return;
            }
            openList(scr().cursor);
            return;
        case K_SPACE:
        case K_XBUTTON:
            if(scr().picked >= 0)
            {
                swapSlots(scr().picked, scr().cursor);
                scr().picked = -1;
            }
            else if(scr().cursor < static_cast<int>(scr().slots.size()) && scr().slots[scr().cursor].key >= 0)
            {
                scr().picked = scr().cursor;
                menuui::uiSound(menuui::UiSound::Select);
            }
            return;
        case K_MOUSE1:
        {
            if(scr().resetButton.has(m_mousex, m_mousey))
            {
                resetSlots();
                menuui::uiSound(menuui::UiSound::Adjust);
                return;
            }
            if(scr().backButton.has(m_mousex, m_mousey))
            {
                leave();
                return;
            }
            if(scr().prevModel.has(m_mousex, m_mousey) || scr().nextModel.has(m_mousex, m_mousey))
            {
                stepModel(scr().prevModel.has(m_mousex, m_mousey) ? -1 : 1);
                return;
            }
            const int at = labelAt(m_mousex, m_mousey);
            if(at >= 0)
            {
                scr().pressed = at;
                scr().pressX = m_mousex;
                scr().pressY = m_mousey;
                scr().dragging = false;
                scr().cursor = at;
            }
            return;
        }
        default: return;
    }
}

extern "C" void VR_BindSetup_Char(int key)
{
    if(!scr().listOpen || key < 32 || key > 126)
    {
        return;
    }
    const int len = static_cast<int>(ZA_STRLEN(scr().search));
    if(len < static_cast<int>(sizeof(scr().search)) - 1)
    {
        scr().search[len] = static_cast<char>(key);
        scr().search[len + 1] = 0;
        scr().listCursor = 0;
        scr().listScroll = 0;
        refilter();
    }
}

extern "C" int VR_BindSetup_TextEntry()
{
    return scr().listOpen ? 1 : 0;
}
