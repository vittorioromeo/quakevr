// vr_msgtokens.cpp -- see vr_msgtokens.hpp.

#include "vr_msgtokens.hpp"
#include "vr_engine.hpp"
#include "vr_flatplay.hpp"
#include "vr_main.hpp"
#include "vr_testkit.hpp"
#include "vr_tips.hpp"
#include "vr_worldtext.hpp"

#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Base/Strncmp.hpp"
#include "Zancle/Base/Strstr.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "vr_api.h"

extern "C" int key_bindgeneration;                  // keys.c: Key_SetBinding's count
extern "C" int Key_StringToKeynum(const char* str); // keys.c

namespace qvr::msgtokens
{
namespace
{

constexpr const char* flatToken = "{flat}";
constexpr const char* keyToken = "{key:";

// The commands a message may name, and what an unbound one is called ("[Jump: not bound]").
struct CommandName
{
    const char* command;
    const char* label;
};
constexpr CommandName commandNames[] = {{"+jump", "Jump"}, {"+attack", "Fire"}, {"+speed", "Run"}, {"+use", "Use"},
    {"+forward", "Move forward"}, {"+back", "Move back"}, {"+moveleft", "Move left"}, {"+moveright", "Move right"},
    {"+flatcrouch", "Crouch"}, {"togglemenu", "Menu"}};

void appendUpper(za::String& out, const char* s)
{
    for(; *s; s++)
    {
        out += (*s >= 'a' && *s <= 'z') ? static_cast<char>(*s - 'a' + 'A') : *s;
    }
}

// A key as a message names it: 'G', 'SPACE', 'Mouse 4'.
void appendKey(za::String& out, int key)
{
    out += '\'';
    switch(key)
    {
        case K_MOUSE1: out += "Left Click"; break;
        case K_MOUSE2: out += "Right Click"; break;
        case K_MOUSE3: out += "Middle Click"; break;
        case K_MOUSE4: out += "Mouse 4"; break;
        case K_MOUSE5: out += "Mouse 5"; break;
        case K_MWHEELUP: out += "Wheel Up"; break;
        case K_MWHEELDOWN: out += "Wheel Down"; break;
        default: appendUpper(out, Key_KeynumToFriendlyString(key)); break;
    }
    out += '\'';
}

void appendUnbound(za::String& out, const char* what)
{
    out += '[';
    out += what;
    out += ": not bound]";
}

// The flat key cvar called `name`'s index in Controls' list (VR_FlatKeyLabel), else -1.
[[nodiscard]] int flatKeyIndex(const char* name)
{
    const za::Span<cvar_t* const> keys = flatplay::keyCvars();
    for(za::SizeT i = 0; i < keys.size(); i++)
    {
        if(!q_strcasecmp(keys[i]->name, name))
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// {key:<what>}: a key cvar's key (a flat key's first), else the first key bound to the command.
void appendKeyFor(za::String& out, const char* what)
{
    if(!q_strcasecmp(what, "vr_flat_key_drop"))
    {
        what = "vr_flat_key_throw"; // merged into Throw / drop (B); existing baked maps keep working
    }
    if(const cvar_t* var = Cvar_FindVar(what))
    {
        const int key = flatplay::keyNum(*var, 0);
        if(key > 0 && key < MAX_KEYS)
        {
            appendKey(out, key);
        }
        else
        {
            const int i = flatKeyIndex(what);
            appendUnbound(out, i >= 0 ? VR_FlatKeyLabel(i) : what);
        }
        return;
    }
    // The likeliest of the keys bound to it: a letter or a digit (W over UPARROW, A over ","), a mouse button (the left
    // button over CTRL), another key, a gamepad's.
    const auto rank = [](int key) {
        if((key >= 'a' && key <= 'z') || (key >= '0' && key <= '9'))
        {
            return 0;
        }
        if(key >= K_MOUSE_BEGIN && key < K_MOUSE_END)
        {
            return 1;
        }
        return key >= K_GAMEPAD_BEGIN ? 3 : 2;
    };
    const bool flat = flatplay::active();
    int best = -1;
    for(int key = 1; key < MAX_KEYS; key++)
    {
        if(Key_Seat()->bindings[key] && !q_strcasecmp(Key_Seat()->bindings[key], what) && !(flat && VR_FlatKeyOwned(key)) &&
           (best < 0 || rank(key) < rank(best)))
        {
            best = key;
        }
    }
    if(best > 0)
    {
        appendKey(out, best);
        return;
    }
    const char* label = what;
    for(const CommandName& c : commandNames)
    {
        if(!q_strcasecmp(c.command, what))
        {
            label = c.label;
        }
    }
    appendUnbound(out, label);
}

// The test's state (test_f, frame).
struct TestRun
{
    bool waiting{false};
    int frames{0};
};
TestRun testRun;

// VR_ExpandMessageTokens's result (valid until its next call).
za::String expandedForC;

} // namespace

bool has(const char* text)
{
    return text && (ZA_STRSTR(text, flatToken) || ZA_STRSTR(text, keyToken));
}

za::String expand(const char* text)
{
    if(!has(text))
    {
        return za::String{text ? text : ""};
    }
    const char* from = text;
    const char* to = text + ZA_STRLEN(text);
    if(const char* split = ZA_STRSTR(text, flatToken))
    {
        if(vrActive())
        {
            to = split;
        }
        else
        {
            from = split + ZA_STRLEN(flatToken);
        }
    }
    za::String out;
    const za::SizeT keyLen = ZA_STRLEN(keyToken);
    while(from < to)
    {
        const char* at = ZA_STRSTR(from, keyToken);
        const char* end = at && at < to ? ZA_STRSTR(at, "}") : nullptr;
        if(!end || end >= to)
        {
            out.append(from, static_cast<za::SizeT>(to - from));
            break;
        }
        out.append(from, static_cast<za::SizeT>(at - from));
        char what[64];
        const za::SizeT n = za::min(static_cast<za::SizeT>(end - at) - keyLen, static_cast<za::SizeT>(sizeof(what) - 1));
        q_strlcpy(what, at + keyLen, n + 1);
        appendKeyFor(out, what);
        from = end + 1;
    }
    return out;
}

unsigned signature()
{
    // FNV-1a over the mode, the binds' count and every flat key's name.
    unsigned h = 2166136261u;
    const auto mix = [&h](unsigned v) { h = (h ^ v) * 16777619u; };
    mix(vrActive() ? 1u : 0u);
    mix(flatplay::active() ? 1u : 0u);
    mix(static_cast<unsigned>(key_bindgeneration));
    for(const cvar_t* c : flatplay::keyCvars())
    {
        for(const char* s = c->string; *s; s++)
        {
            mix(static_cast<unsigned char>(*s));
        }
        mix(0u);
    }
    return h;
}

// ----------------------------------------------------------------------------
// vr_msgtokens_test: the tutorial's messages with the player's keys, in the mode the game runs in (-flatscreenmode:
// the flat wording, a rebind followed; a headset: the headset's wording, untouched). Run it in vrtutorial; it waits for
// the map's tips to come, then ends on "msgtokens: N passed, M failed".

namespace
{

using Tally = testkit::Tally;

// A check, printed whichever way it went (with the text it looked at).
void check(Tally& t, bool ok, const char* name, const char* detail)
{
    t.check(ok);
    za::String line{detail};
    line.replaceAllOccurrences("\n", " / "); // (on one line)
    Con_Printf("msgtokens: %s %s: %s\n", ok ? "ok" : "FAIL", name, line.cStr());
}

[[nodiscard]] bool contains(const za::String& s, const char* part)
{
    return ZA_STRSTR(s.cStr(), part) != nullptr;
}

// The client's world text board whose text starts with `title` (its text as shown), else "".
[[nodiscard]] za::String boardText(const char* title)
{
    for(const worldtext::WorldText& wt : worldtext::clientTexts())
    {
        if(!ZA_STRNCMP(wt.text.cStr(), title, ZA_STRLEN(title)))
        {
            return wt.text;
        }
    }
    return za::String{};
}

void runTest()
{
    Tally t;
    const bool flat = !vrActive();
    Con_Printf("msgtokens: %s wording\n", flat ? "flat-screen" : "headset");

    // No token left on anything shown: the boards and the tips the test names.
    bool clean = true;
    for(const worldtext::WorldText& wt : worldtext::clientTexts())
    {
        clean = clean && !has(wt.text.cStr());
    }
    const char* tipNames[] = {
        "t2_move", "t2_jump", "t2_grenade", "t2_forcegrab", "t2_punch", "t3_bash", "t1_menu", "t2_throw", "t3_axe"};
    for(const char* name : tipNames)
    {
        const za::String s = tips::shownTextOf(name);
        clean = clean && !s.empty() && !has(s.cStr());
    }
    check(t, clean, "resolved", "no {flat} or {key:} left on the boards and the tips");

    if(!flat)
    {
        const za::String g = tips::shownTextOf("t2_grenade");
        check(t, contains(g, "pouch on your lower") && !contains(g, "'"), "headset wording", g.cStr());
        check(t, expand("A{flat}B") == za::String{"A"}, "split", "the headset's part before {flat}");
    }
    else
    {
        cvar_t* grenade = Cvar_FindVar("vr_flat_key_grenade");
        const za::String grenadeWas{grenade->string};
        Cvar_SetQuick(grenade, "g");
        za::String g = tips::shownTextOf("t2_grenade");
        check(t, contains(g, "'G'"), "grenade key", g.cStr());
        // A centre print's path (SCR_CenterPrint).
        const char* c = VR_ExpandMessageTokens("A{flat}B {key:vr_flat_key_grenade}");
        check(t, !ZA_STRCMP(c, "B 'G'"), "centre print", c);
        Cvar_SetQuick(grenade, "j");
        g = tips::shownTextOf("t2_grenade");
        check(t, contains(g, "'J'") && !contains(g, "'G'"), "grenade rebound", g.cStr());
        za::String board = boardText("GRENADES");
        check(t, contains(board, "'J'"), "board follows", board.cStr());
        Cvar_SetQuick(grenade, "");
        g = tips::shownTextOf("t2_grenade");
        check(t, contains(g, "[Quick grenade: not bound]"), "grenade unbound", g.cStr());
        Cvar_SetQuick(grenade, grenadeWas.cStr());

        // The throw key (vr_flat_key_throw, default B; set to J here) on lessons 10 and 12: their tips and boards.
        cvar_t* throwKey = Cvar_FindVar("vr_flat_key_throw");
        const za::String throwWas{throwKey->string};
        Cvar_SetQuick(throwKey, "j");
        const za::String rock = tips::shownTextOf("t2_throw"), axe = tips::shownTextOf("t3_axe");
        const za::String b10 = boardText("LESSON 10: THROWING"), b12 = boardText("LESSON 12: BLADES");
        check(t, contains(rock, "'J'") && !contains(rock, "not in flat play"), "throw key: rock", rock.cStr());
        check(t, contains(axe, "'J'") && !contains(axe, "not in flat play"), "throw key: axe", axe.cStr());
        check(t, contains(b10, "'J'") && contains(b12, "'J'"), "throw key: boards", b10.cStr());
        Cvar_SetQuick(throwKey, throwWas.cStr());

        // A command's bind: +jump on SPACE, then on U alone.
        const za::String spaceWas{Key_Seat()->bindings[K_SPACE] ? Key_Seat()->bindings[K_SPACE] : ""};
        const za::String uWas{Key_Seat()->bindings['u'] ? Key_Seat()->bindings['u'] : ""};
        Key_SetBinding(K_SPACE, "+jump");
        za::String j = tips::shownTextOf("t2_jump");
        check(t, contains(j, "'SPACE'"), "jump key", j.cStr());
        Key_SetBinding(K_SPACE, nullptr);
        Key_SetBinding('u', "+jump");
        j = tips::shownTextOf("t2_jump");
        check(t, contains(j, "'U'") && !contains(j, "'SPACE'"), "jump rebound", j.cStr());
        Key_SetBinding(K_SPACE, spaceWas.empty() ? nullptr : spaceWas.cStr());
        Key_SetBinding('u', uWas.empty() ? nullptr : uWas.cStr());

        // Of two keys bound, the letter (A over ",", as default.cfg binds +moveleft).
        const za::String aWas{Key_Seat()->bindings['a'] ? Key_Seat()->bindings['a'] : ""};
        const za::String commaWas{Key_Seat()->bindings[','] ? Key_Seat()->bindings[','] : ""};
        Key_SetBinding('a', "+moveleft");
        Key_SetBinding(',', "+moveleft");
        const za::String m = tips::shownTextOf("t2_move");
        check(t, contains(m, "'A'") && !contains(m, "','"), "the letter first", m.cStr());
        Key_SetBinding('a', aWas.empty() ? nullptr : aWas.cStr());
        Key_SetBinding(',', commaWas.empty() ? nullptr : commaWas.cStr());
    }
    t.verdict("msgtokens");
}

} // namespace

void test_f()
{
    testRun = TestRun{};
    testRun.waiting = true;
    Con_Printf("msgtokens: waiting for the map's tips\n");
}

void frame()
{
    if(!testRun.waiting)
    {
        return;
    }
    if(cls.signon == SIGNONS && !tips::shownTextOf("t2_grenade").empty())
    {
        testRun.waiting = false;
        runTest();
        return;
    }
    if(++testRun.frames > 72 * 30)
    {
        testRun.waiting = false;
        Con_Printf("msgtokens: FAIL timed out (no tip t2_grenade: run it in vrtutorial)\n");
        testkit::verdict("msgtokens", 0, 1);
    }
}

} // namespace qvr::msgtokens

extern "C" const char* VR_ExpandMessageTokens(const char* text)
{
    if(!qvr::msgtokens::has(text))
    {
        return text;
    }
    qvr::msgtokens::expandedForC = qvr::msgtokens::expand(text);
    return qvr::msgtokens::expandedForC.cStr();
}
