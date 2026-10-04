// vr_tips.cpp -- tips for new players (vr_tips.hpp).

#include "vr_tips.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_gadget.hpp"
#include "vr_hands.hpp"
#include "vr_text3d.hpp"
#include "vr_units.hpp"
#include "vr_walltorch.hpp"

#include "Zancle/Algorithm/StableSort.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Sqrt.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"

namespace qvr::tips
{
namespace
{

struct Tip
{
    const char* name; // in vr_tips_seen
    const char* text; // its lines (the panel's and the hologram's)
    bool (*about)(const entity_t& e);
};

[[nodiscard]] bool wallTorch(const entity_t& e)
{
    return walltorch::onWall(e);
}

constexpr Tip tips[] = {
    {"walltorch", "Wall torches can be taken:\ngrip one and pull it\noff its wall. Hit enemies\nwith it or throw it to\nset them on fire.",
        wallTorch},
};
constexpr int tipCount = static_cast<int>(sizeof(tips) / sizeof(tips[0]));

// The tip waiting to show: what it is about has been near and seen since `since`.
struct Candidate
{
    int tip{-1};
    int ent{0};
    double since{0.0};
};
Candidate candidate;

// The tip showing as a floating panel.
struct Showing
{
    int tip{-1};
    int ent{0};
    double start{0.0};
    double until{0.0};
    glm::vec3 panel{0.f}; // its place, following what it is about smoothly
    bool placed{false};
    glm::vec3 lastTarget{0.f}; // last valid target, retained while the main-thread tip fades
    double lastFrame{0.0};
};
Showing showing;
int pendingTest = -1; // vr_tips_test's tip, shown once back in the game (the menu or the console closed)

[[nodiscard]] bool seen(const Tip& tip)
{
    const za::StringView list{vr_tips_seen.string};
    const za::StringView name{tip.name};
    for(size_t i = 0; i < list.size();)
    {
        while(i < list.size() && list[i] == ' ')
        {
            i++;
        }
        size_t j = i;
        while(j < list.size() && list[j] != ' ')
        {
            j++;
        }
        if(list.substrByPosLen(i, j - i) == name)
        {
            return true;
        }
        i = j;
    }
    return false;
}

void markSeen(const Tip& tip)
{
    if(seen(tip))
    {
        return;
    }
    za::String list{vr_tips_seen.string};
    if(!list.empty())
    {
        list += ' ';
    }
    list += tip.name;
    Cvar_Set(vr_tips_seen.name, list.cStr());
}

// Where the line from the panel points: the middle of what the tip is about (its model's box, turned with it: a wall
// torch's origin is in its wall).
[[nodiscard]] glm::vec3 targetPoint(const entity_t& e)
{
    const glm::vec3 origin{e.origin[0], e.origin[1], e.origin[2]};
    if(!e.model)
    {
        return origin;
    }
    const glm::vec3 c = (glm::vec3{e.model->mins[0], e.model->mins[1], e.model->mins[2]} +
                            glm::vec3{e.model->maxs[0], e.model->maxs[1], e.model->maxs[2]}) *
                        0.5f;
    glm::vec3 f, r, u;
    hands::angleVectors(glm::vec3{e.angles[0], e.angles[1], e.angles[2]}, f, r, u);
    return origin + f * c.x - r * c.y + u * c.z; // (a model's y is its left)
}

[[nodiscard]] bool connected()
{
    return cls.state == ca_connected && cls.signon == SIGNONS && cl.worldmodel;
}

// Playing: no menu or console open, no intermission (a tip showing waits meanwhile).
[[nodiscard]] bool inGame()
{
    return connected() && !cl.intermission && key_dest == key_game && hands::current().valid;
}

// Whether entity `i` is there this frame (in the last message) and what `tip` is about.
[[nodiscard]] const entity_t* entityFor(const Tip& tip, int i)
{
    if(i <= 0 || i >= cl.num_entities || i == cl.viewentity)
    {
        return nullptr;
    }
    const entity_t& e = cl_entities[i];
    return e.model && e.msgtime == cl.mtime[0] && tip.about(e) ? &e : nullptr;
}

// Nothing of the world between the eyes and `at`, pulled a few units towards them (what is on a wall is partly in it).
[[nodiscard]] bool clearTo(const glm::vec3& eye, const glm::vec3& at)
{
    const glm::vec3 toEye = eye - at;
    const float d = glm::length(toEye);
    if(d < 1.f)
    {
        return true;
    }
    const glm::vec3 nearPoint = at + toEye / d * za::min(8.f, d);
    vec3_t start{eye.x, eye.y, eye.z}, end{nearPoint.x, nearPoint.y, nearPoint.z};
    trace_t trace;
    memset(&trace, 0, sizeof trace);
    trace.fraction = 1.f;
    SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0.f, 1.f, start, end, &trace);
    return trace.fraction >= 1.f && !trace.allsolid;
}

// Whether the eyes see `e`: its middle, or its top (a torch's flame over a ledge).
[[nodiscard]] bool inSight(const glm::vec3& eye, const entity_t& e)
{
    const glm::vec3 at = targetPoint(e);
    const float top = e.model ? e.model->maxs[2] : 0.f;
    return clearTo(eye, at) || clearTo(eye, glm::vec3{e.origin[0], e.origin[1], e.origin[2] + top + 8.f});
}

// How well `e` is placed for its tip now: near enough, in view and in sight (else -1); the nearer the better.
[[nodiscard]] float placing(const entity_t& e, bool anyDistance)
{
    const hands::State& s = hands::current();
    const glm::vec3 at = targetPoint(e);
    const glm::vec3 to = at - s.head;
    const float d = glm::length(to);
    if(d < 1e-3f || (!anyDistance && d > za::max(vr_tips_distance.value, 1.f)))
    {
        return -1.f;
    }
    const float maxAngle = vr_tips_view_angle.value;
    if(maxAngle > 0.f && maxAngle < 180.f)
    {
        const float cosine = glm::dot(to / d, hands::forward(s.headAngles));
        if(cosine < glm::cos(glm::radians(maxAngle)))
        {
            return -1.f;
        }
    }
    if(vr_tips_line_of_sight.value != 0.f && !inSight(s.head, e))
    {
        return -1.f;
    }
    return d;
}

// The entity `tip` is best shown on now (0: none).
[[nodiscard]] int bestFor(const Tip& tip, bool anyDistance)
{
    int best = 0;
    float bestDistance = 1e9f;
    for(int i = 1; i < cl.num_entities; i++)
    {
        if(const entity_t* e = entityFor(tip, i))
        {
            const float d = placing(*e, anyDistance);
            if(d >= 0.f && d < bestDistance)
            {
                best = i;
                bestDistance = d;
            }
        }
    }
    return best;
}

// Shows tip `t` on entity `ent`: in the gadget's hologram (vr_tips 2, if there is one), else as the floating panel.
void show(int t, int ent, bool count)
{
    const Tip& tip = tips[t];
    const float time = za::max(vr_tips_time.value, 1.f);
    if(count)
    {
        markSeen(tip);
    }
    if(vr_tips.value >= 2.f && gadget::tip(tip.text, time))
    {
        Con_DPrintf("tips: \"%s\" in the gadget's hologram\n", tip.name);
        showing.tip = -1;
        return;
    }
    Con_DPrintf("tips: \"%s\" by entity %d\n", tip.name, ent);
    showing.tip = t;
    showing.ent = ent;
    showing.start = realtime;
    showing.until = realtime + time;
    showing.placed = false;
    showing.lastFrame = realtime;
}

// The floating panel this frame: by what it is about, a little below and to the right of it as seen, facing the eyes,
// with a line to it; fading in and out.
void drawShowing()
{
    const Tip& tip = tips[showing.tip];
    const entity_t* e = entityFor(tip, showing.ent);
    if(!e && showing.until > realtime + 0.4)
    {
        showing.until = realtime + 0.4; // taken (or gone): it fades now
    }
    if(realtime >= showing.until)
    {
        showing.tip = -1;
        return;
    }
    const hands::State& s = hands::current();
    glm::vec3& lastTarget = showing.lastTarget;
    const glm::vec3 target = e ? targetPoint(*e) : lastTarget;
    lastTarget = target;

    glm::vec3 fwd, right, up;
    hands::angleVectors(s.headAngles, fwd, right, up);
    const float m2u = units::metresToUnits();
    const glm::vec3 to = target - s.head;
    const float d = za::max(glm::length(to), 1.f);
    const float panelDistance = CLAMP(0.5f * m2u, d * 0.75f, 1.2f * m2u);
    const glm::vec3 side = glm::normalize(glm::cross(to / d, glm::vec3{0.f, 0.f, 1.f})); // the eyes' right, level
    const glm::vec3 dir = glm::normalize(to / d + side * 0.32f - glm::vec3{0.f, 0.f, 0.2f});
    const glm::vec3 wanted = s.head + dir * panelDistance;
    const float dt = static_cast<float>(za::min(realtime - showing.lastFrame, 0.1));
    showing.lastFrame = realtime;
    showing.panel = showing.placed ? glm::mix(showing.panel, wanted, 1.f - glm::exp(-8.f * dt)) : wanted;
    showing.placed = true;

    const float fadeIn = CLAMP(0.f, static_cast<float>(realtime - showing.start) / 0.3f, 1.f);
    const float fadeOut = CLAMP(0.f, static_cast<float>(showing.until - realtime) / 0.6f, 1.f);
    const float alpha = fadeIn * fadeOut;

    const float charSize = glm::distance(s.head, showing.panel) * 0.026f * CLAMP(0.25f, vr_tips_size.value, 4.f);
    // Its plane: turned towards the eyes, level (vr_tips_facing 0), or square to the view, as the head is turned
    // (1: flat in front of you, however you tilt your head).
    glm::vec3 panelRight, panelUp;
    if(vr_tips_facing.value != 0.f)
    {
        glm::vec3 f;
        hands::angleVectors(s.headAngles, f, panelRight, panelUp);
    }
    else
    {
        const glm::vec3 look = glm::normalize(showing.panel - s.head);
        const glm::vec3 level = glm::cross(look, glm::vec3{0.f, 0.f, 1.f});
        panelRight = glm::length(level) > 1e-4f ? glm::normalize(level) : right;
        panelUp = glm::cross(panelRight, look);
    }
    // A CRT screen as the map boards', with a cable of the same screen to what it is about.
    text3d::queueOverlayScreen(tip.text, showing.panel, panelRight, panelUp, charSize, alpha, s.head, &target, 0.0022f);
}

} // namespace

void frame()
{
    if(vr_tips.value == 0.f || !connected())
    {
        showing.tip = -1;
        candidate.tip = -1;
        return;
    }
    if(!inGame())
    {
        candidate.tip = -1;
        showing.lastFrame = realtime;
        return;
    }
    if(pendingTest >= 0)
    {
        const int t = pendingTest;
        pendingTest = -1;
        if(const int ent = bestFor(tips[t], true))
        {
            show(t, ent, false);
        }
        else
        {
            Con_Printf("vr_tips_test: nothing for \"%s\" in view\n", tips[t].name);
        }
    }
    if(showing.tip >= 0)
    {
        drawShowing();
        return;
    }
    // The first tip not shown yet whose subject is near and seen; it shows once it has been so vr_tips_delay seconds.
    for(int t = 0; t < tipCount; t++)
    {
        if(seen(tips[t]))
        {
            continue;
        }
        const int ent = bestFor(tips[t], false);
        if(ent == 0)
        {
            continue;
        }
        if(candidate.tip != t || candidate.ent != ent)
        {
            candidate = {t, ent, realtime};
        }
        if(realtime - candidate.since >= static_cast<double>(za::max(vr_tips_delay.value, 0.f)))
        {
            candidate.tip = -1;
            show(t, ent, true);
        }
        return;
    }
    candidate.tip = -1;
}

void reset_f()
{
    Cvar_Set(vr_tips_seen.name, "");
    showing.tip = -1;
    candidate.tip = -1;
    Con_Printf("VR: every tip will show again\n");
}

void test_f()
{
    int t = 0;
    if(Cmd_Argc() > 1)
    {
        t = -1;
        for(int i = 0; i < tipCount && t < 0; i++)
        {
            t = q_strcasecmp(tips[i].name, Cmd_Argv(1)) ? -1 : i;
        }
        if(t < 0)
        {
            Con_Printf("vr_tips_test: no tip \"%s\" (the tips: ", Cmd_Argv(1));
            for(int i = 0; i < tipCount; i++)
            {
                Con_Printf("%s%s", i ? ", " : "", tips[i].name);
            }
            Con_Printf(")\n");
            return;
        }
    }
    if(!connected())
    {
        Con_Printf("vr_tips_test: in a game\n");
        return;
    }
    // The nearest few of what it is about, and how each stands against the settings now.
    const hands::State& head = hands::current();
    struct Near
    {
        int ent;
        float d;
    };
    za::Vector<Near> nearby;
    for(int i = 1; i < cl.num_entities; i++)
    {
        if(const entity_t* e = entityFor(tips[t], i))
        {
            nearby.pushBack({i, glm::distance(targetPoint(*e), head.head)});
        }
    }
    za::stableSort(nearby.begin(), nearby.end(), [](const Near& a, const Near& b) { return a.d < b.d; });
    for(size_t i = 0; i < nearby.size() && i < 3; i++)
    {
        const entity_t& e = cl_entities[nearby[i].ent];
        const glm::vec3 to = glm::normalize(targetPoint(e) - head.head);
        const float angle = glm::degrees(glm::acos(CLAMP(-1.f, glm::dot(to, hands::forward(head.headAngles)), 1.f)));
        Con_Printf("vr_tips_test: %s %d: %.0f units (%s), %.0f degrees from the view (%s), %s\n", tips[t].name, nearby[i].ent,
            nearby[i].d, nearby[i].d <= vr_tips_distance.value ? "near enough" : "too far", angle,
            vr_tips_view_angle.value <= 0.f || angle <= vr_tips_view_angle.value ? "in view" : "out of view",
            inSight(head.head, e) ? "in sight" : "hidden");
    }
    pendingTest = t; // (as the menu or the console closes)
    showing.tip = -1;
}

} // namespace qvr::tips
