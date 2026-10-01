// vr_motion.cpp -- the motion recorder: see vr_motion.hpp and docs/vr-port/MOTIONS.md. Recording, the
// take file, the server's samples and the QC's events; playback is vr_motion_play.cpp.

#include "vr_motion.hpp"
#include "vr_motion_take.hpp"
#include "vr_motion_review.hpp"

#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_files.hpp"
#include "vr_hands.hpp"
#include "vr_jobs.hpp"
#include "vr_main.hpp"
#include "vr_mem.hpp"
#include "vr_progs.hpp"
#include "vr_protocol.hpp"
#include "vr_text3d.hpp"
#include "vr_twohand.hpp"
#include "vr_units.hpp"
#include "vr_view.hpp"
#include "vr_weapons.hpp"

#include "Zancle/Algorithm/AnyOf.hpp"
#include "Zancle/Algorithm/Find.hpp"
#include "Zancle/Algorithm/Replace.hpp"
#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/GetArraySize.hpp"
#include "Zancle/Base/Macros.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/String/ToString.hpp"
#include "vr_zancle.hpp"

#include <stdio.h>
#include <string.h>
#include <time.h>

extern "C" float host_netinterval; // host.c

namespace qvr::motion
{

// ----------------------------------------------------------------------------
// Labels
// ----------------------------------------------------------------------------

const za::Vector<Category>& categories()
{
    static const Choice any{"", "-"};
    static const za::Vector<Category> list = {
        {{"slash", "Expected Slash"},
            {any, {"overhead", "Overhead"}, {"horizontal_ltr", "Horizontal L-R"}, {"horizontal_rtl", "Horizontal R-L"},
                {"diagonal_down_left", "Diagonal Down-L"}, {"diagonal_down_right", "Diagonal Down-R"},
                {"backswing_up_left", "Backswing Up-L"}, {"backswing_up_right", "Backswing Up-R"}}},
        {{"stab", "Expected Stab"}, {any, {"one_hand", "One Hand"}, {"two_hands", "Two Hands"}}},
        {{"no_hit", "No Hit"},
            {any, {"wiggling", "Wiggling"}, {"weak", "Weak Motion"}, {"idle", "Idle"}, {"slow_waving", "Slow Waving"},
                {"reloading", "Reloading"}, {"aiming", "Aiming"}, {"walking", "Walking"}, {"reaching", "Reaching"}}},
        {{"bash", "Expected Bash"}, {any, {"sword_1h", "Sword 1H"}, {"sword_2h", "Sword 2H"}, {"gun", "Gun"}}},
        {{"parry_pose", "Expected Parry Pose"},
            {any, {"sword_1h", "Sword 1H"}, {"sword_2h_blade", "Sword 2H on Blade"}, {"sword_2h", "Sword 2H"},
                {"gun", "Gun"}}},
        {{"parry_bash", "Expected Parry Bash"},
            {any, {"sword_1h", "Sword 1H"}, {"sword_2h_blade", "Sword 2H on Blade"}, {"sword_2h", "Sword 2H"},
                {"gun", "Gun"}}},
        {{"hilt_pommel", "Expected Hilt/Pommel"}, {any}},
        {{"punch", "Expected Punch"},
            {any, {"straight", "Straight"}, {"jab", "Jab"}, {"hook", "Hook"}, {"uppercut", "Uppercut"},
                {"overhead", "Overhead"}}},
        {{"palm_shove_1h", "Expected Palm Shove 1H"}, {any}},
        {{"palm_shove_2h", "Expected Palm Shove 2H"}, {any}},
        {{"gun_strike", "Expected Gun Strike"}, {any, {"swing", "Swing"}, {"butt", "Butt"}}},
        {{"other", "Other (vr_motion_note)"}, {any}},
        // Added later: at the end, so vr_motion_category's saved index keeps its category (the menu shows them in
        // categoryOrder's order).
        {{"not_parry_pose", "Not Parry Pose"},
            {any, {"weapon_angled", "Weapon Angled"}, {"hands_up", "Hands Up"}, {"resting", "Resting"},
                {"aiming", "Aiming"}, {"other", "Other"}}},
    };
    return list;
}

za::Vector<int> categoryOrder()
{
    // The menu's order: each category after its kin (Not Parry Pose after Expected Parry Pose).
    static const char* const order[] = {"slash", "stab", "no_hit", "bash", "parry_pose", "not_parry_pose", "parry_bash",
        "hilt_pommel", "punch", "palm_shove_1h", "palm_shove_2h", "gun_strike", "other"};
    const auto& list = categories();
    za::Vector<int> out;
    for(const char* name : order)
    {
        for(size_t i = 0; i < list.size(); i++)
        {
            if(!strcmp(list[i].choice.name, name))
            {
                out.pushBack(static_cast<int>(i));
            }
        }
    }
    for(size_t i = 0; i < list.size(); i++) // (any not in the order)
    {
        if(za::find(out.begin(), out.end(), static_cast<int>(i)) == out.end())
        {
            out.pushBack(static_cast<int>(i));
        }
    }
    return out;
}

const Category& chosenCategory()
{
    const auto& list = categories();
    return list[CLAMP(0, static_cast<int>(vr_motion_category.value), static_cast<int>(list.size()) - 1)];
}

const Choice& chosenDetail()
{
    const Category& c = chosenCategory();
    return c.details[CLAMP(0, static_cast<int>(vr_motion_detail.value), static_cast<int>(c.details.size()) - 1)];
}

za::String chosenLabel()
{
    const Choice& d = chosenDetail();
    return d.name[0] ? za::String{chosenCategory().choice.name} + "_" + d.name : chosenCategory().choice.name;
}

za::String categoryOf(const za::String& label)
{
    za::String best;
    for(const Category& c : categories())
    {
        const za::String name = c.choice.name;
        if((label == name || label.rfind(name + "_", 0) == 0) && name.size() > best.size())
        {
            best = name;
        }
    }
    return best;
}

za::String safeLabel(const za::String& label)
{
    za::String out;
    for(const char c : label)
    {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        out += ok ? c : '_';
    }
    return out.empty() ? "unlabelled" : out;
}

za::String motionsDir()
{
    const za::String dir = za::String{com_gamedir} + "/motions";
    Sys_mkdir(dir.cStr());
    return dir;
}

const glm::vec3* ServerSample::value(const char* key) const
{
    for(const auto& [k, v] : values)
    {
        if(k == key)
        {
            return &v;
        }
    }
    return nullptr;
}

namespace
{

// ----------------------------------------------------------------------------
// The take file
// ----------------------------------------------------------------------------

// A row's cells, or the header row's names: the same code writes both, so they always match.
class Sink
{
public:
    explicit Sink(bool names) : names_(names)
    {
    }

    void num(const char* name, double v, int decimals)
    {
        if(names_)
        {
            add(name);
            return;
        }
        char buf[64];
        q_snprintf(buf, sizeof(buf), "%.*f", decimals, v);
        // "-0.0000" is "0.0000".
        const char* p = buf;
        if(*p == '-' && strspn(p + 1, "0.") == strlen(p + 1))
        {
            p++;
        }
        add(p);
    }

    void integer(const char* name, int v)
    {
        if(names_)
        {
            add(name);
            return;
        }
        char buf[32];
        q_snprintf(buf, sizeof(buf), "%d", v);
        add(buf);
    }

    void text(const char* name, const za::String& v)
    {
        add(names_ ? za::String{name} : clean(v));
    }

    void empty(const char* name)
    {
        add(names_ ? name : "");
    }

    // A position in the player's frame: units, then metres.
    void pos(const za::String& prefix, bool ok, const glm::vec3& units, float u2m)
    {
        static const char* const axes[] = {"x", "y", "z"};
        for(int unit = 0; unit < 2; unit++)
        {
            for(int i = 0; i < 3; i++)
            {
                const za::String name = prefix + "_" + axes[i] + (unit == 0 ? "_u" : "_m");
                if(!ok)
                {
                    empty(name.cStr());
                }
                else
                {
                    num(name.cStr(), unit == 0 ? units[i] : units[i] * u2m, unit == 0 ? 3 : 5);
                }
            }
        }
    }

    void vec(const za::String& prefix, bool ok, const glm::vec3& v, int decimals, const char* const (&axes)[3])
    {
        for(int i = 0; i < 3; i++)
        {
            const za::String name = prefix + axes[i];
            if(!ok)
            {
                empty(name.cStr());
            }
            else
            {
                num(name.cStr(), v[i], decimals);
            }
        }
    }

    [[nodiscard]] const za::String& line() const
    {
        return line_;
    }

    // No commas, quotes or line breaks in a cell (the events' separators are ';' and ':').
    [[nodiscard]] static za::String clean(const za::String& s)
    {
        za::String out = s;
        for(char& c : out)
        {
            if(c == ',' || c == '"' || c == '\n' || c == '\r')
            {
                c = ' ';
            }
        }
        return out;
    }

private:
    void add(const za::String& cell)
    {
        if(!first_)
        {
            line_ += ',';
        }
        first_ = false;
        line_ += cell;
    }

    bool names_;
    bool first_{true};
    za::String line_;
};

constexpr const char* xyz[3] = {"x", "y", "z"};
constexpr const char* pxyz[3] = {"px", "py", "pz"};
constexpr const char* vxyz[3] = {"vx", "vy", "vz"};
constexpr const char* wxyz[3] = {"wx", "wy", "wz"};
constexpr const char* gxyz[3] = {"gx", "gy", "gz"};
constexpr const char* avxyz[3] = {"avx", "avy", "avz"};

[[nodiscard]] float wrapYaw(float y)
{
    return qza::remainder(y, 360.f);
}

[[nodiscard]] za::String eventField(za::String s)
{
    for(char& c : s)
    {
        if(c == ';' || c == ':' || c == ',' || c == '"' || c == '\n' || c == '\r')
        {
            c = ' ';
        }
    }
    return s;
}

[[nodiscard]] const char* handName(int hand)
{
    return hand == HAND_MAIN ? "main" : hand == HAND_OFF ? "off" : "-";
}

// One row (or, with names, the header row).
// (Any thread: the take files are written in the background.)
void emitRow(Sink& s, const Row& r, int frame, const TakeInfo& info, float u2m)
{
    const float yaw0 = info.yaw0;
    // The player's frame: from the player's origin, turned by the take's yaw.
    const auto pf = [&](const glm::vec3& world) { return hands::rotateYaw(world - r.origin, -yaw0); };
    const auto dir = [&](const glm::vec3& v) { return hands::rotateYaw(v, -yaw0); };
    const ServerSample* sv = r.sv.get();
    const bool mon = sv && sv->monster;
    // The monster's frame: from its origin, turned by its yaw (x its facing, y its left, z up).
    const auto df = [&](const glm::vec3& world) {
        return mon ? hands::rotateYaw(world - sv->monOrigin, -sv->monAngles.y) : glm::vec3{0.f};
    };

    // Timing.
    s.integer("frame", frame);
    s.num("t", r.realtime - info.t0, 5);
    s.text("phase", r.phase == PhasePre ? "pre" : r.phase == PhaseRec ? "rec" : "tail");
    s.num("dt", r.dt, 6);
    s.num("realtime", r.realtime, 5);
    s.num("xr_time", r.xrTime, 5);
    s.integer("sv_tick", r.tick ? 1 : 0);
    s.num("sv_dt", r.tick ? r.svDt : 0.0, 6);
    if(sv)
    {
        s.num("sv_time", sv->time, 5);
    }
    else
    {
        s.empty("sv_time");
    }

    // The player: world, its own frame's, and relative to the monster.
    s.vec("org_w_", true, r.origin, 3, xyz);
    s.num("yaw", wrapYaw(r.headAngles.y), 3);
    s.num("play_yaw", r.playYaw, 3);
    const bool svPlayer = sv && sv->player;
    const glm::vec3 pvel = svPlayer ? dir(sv->velocity) : glm::vec3{0.f};
    s.pos("pvel", svPlayer, pvel, u2m);
    // The server's own origin (the client's, org_w, lags a moving player a little), and on the ground.
    s.vec("sv_org_w_", svPlayer, svPlayer ? sv->origin : glm::vec3{0.f}, 3, xyz);
    if(svPlayer)
    {
        s.integer("sv_onground", sv->onGround ? 1 : 0);
    }
    else
    {
        s.empty("sv_onground");
    }
    s.pos("lean", true, dir(r.lean), u2m);
    s.num("body_yaw", wrapYaw(r.bodyYaw - yaw0), 3);
    s.num("crouch", r.crouch, 4);
    s.num("view_pitch", r.headAngles.x, 3);
    s.num("view_yaw", wrapYaw(r.headAngles.y - yaw0), 3);
    s.num("view_roll", r.headAngles.z, 3);
    s.num("aim_pitch", r.aim.x, 3);
    s.num("aim_yaw", wrapYaw(r.aim.y - yaw0), 3);
    s.num("aim_roll", r.aim.z, 3);
    s.pos("org_d", mon, df(r.origin), u2m);
    if(mon)
    {
        s.num("view_yaw_d", wrapYaw(r.headAngles.y - sv->monAngles.y), 3);
    }
    else
    {
        s.empty("view_yaw_d");
    }

    // The head.
    s.pos("head", true, pf(r.head), u2m);
    s.vec("head_w_", true, r.head, 3, xyz);
    s.pos("head_d", mon, df(r.head), u2m);
    s.vec("head_", true, dir(r.headVel), 4, vxyz);

    // The hands.
    for(const int h : {HAND_MAIN, HAND_OFF})
    {
        const HandRow& hr = r.hands[h];
        const za::String p = h == HAND_MAIN ? "m_" : "o_";
        const auto name = [&](const char* n) { return p + n; };
        s.pos(p + "pos", true, pf(hr.pos), u2m);
        s.vec(p + "pos_w_", true, hr.pos, 3, xyz);
        s.pos(p + "pos_d", mon, df(hr.pos), u2m);
        s.num(name("pitch").cStr(), hr.rot.x, 3);
        s.num(name("yaw").cStr(), wrapYaw(hr.rot.y - yaw0), 3);
        s.num(name("roll").cStr(), hr.rot.z, 3);
        if(mon)
        {
            s.num(name("yaw_d").cStr(), wrapYaw(hr.rot.y - sv->monAngles.y), 3);
        }
        else
        {
            s.empty(name("yaw_d").cStr());
        }
        s.vec(p, true, dir(hr.vel), 4, vxyz);
        s.vec(p, true, dir(hr.angVel), 4, avxyz);
        s.num(name("trigger").cStr(), hr.input.triggerValue, 3);
        s.num(name("grip").cStr(), hr.input.gripValue, 3);
        s.integer(name("thumb").cStr(), hr.input.thumbTouch ? 1 : 0);
        s.num(name("stick_x").cStr(), hr.input.stick.x, 3);
        s.num(name("stick_y").cStr(), hr.input.stick.y, 3);
        const int buttons = (hr.input.trigger ? 1 : 0) | (hr.input.grip ? 2 : 0) | (hr.input.primary ? 4 : 0) |
                            (hr.input.secondary ? 8 : 0) | (hr.input.stickClick ? 16 : 0) | (hr.input.menu ? 32 : 0);
        s.integer(name("buttons").cStr(), buttons);
        static const char* const fingers[] = {"curl_thumb", "curl_index", "curl_middle", "curl_ring", "curl_pinky"};
        for(int f = 0; f < 5; f++)
        {
            s.num(name(fingers[f]).cStr(), hr.curl[f], 3);
        }
        s.integer(name("wid").cStr(), hr.wid);
        s.integer(name("wflags").cStr(), hr.wflags);
        s.text(name("model").cStr(), hr.model);
        s.integer(name("helping").cStr(), hr.helping ? 1 : 0);
        s.integer(name("2h").cStr(), hr.grip2h);
        s.num(name("2h_t").cStr(), hr.twoHand, 3);
        s.integer(name("carried").cStr(), hr.carried ? 1 : 0);
        s.integer(name("hotspot").cStr(), hr.hotspot);
        s.integer(name("muzzle_ok").cStr(), hr.muzzleOk ? 1 : 0);
        s.pos(p + "muzzle", hr.muzzleOk, pf(hr.muzzle), u2m);
        s.pos(p + "muzzle_d", hr.muzzleOk && mon, df(hr.muzzle), u2m);

        // The QC's striking points (VR_Blow_Points): up to six, and the weapon's line (its butt: a
        // sword's pommel, an axe's or a hammer's handle end, a gun's grip; and its far end).
        const bool qc = sv && sv->qc;
        za::String names;
        if(qc)
        {
            for(const Point& pt : sv->points[h])
            {
                names += (names.empty() ? "" : "|") + pt.name;
            }
        }
        s.text(name("pt_names").cStr(), names);
        for(int i = 0; i < 6; i++)
        {
            const bool ok = qc && i < static_cast<int>(sv->points[h].size());
            s.pos(p + "pt" + za::toString(i), ok, ok ? pf(sv->points[h][i].at) : glm::vec3{0.f}, u2m);
        }
        const glm::vec3* butt = qc ? sv->value((p + "butt").cStr()) : nullptr;
        const glm::vec3* end = qc ? sv->value((p + "end").cStr()) : nullptr;
        s.pos(p + "butt", butt != nullptr, butt ? pf(*butt) : glm::vec3{0.f}, u2m);
        s.pos(p + "end", end != nullptr, end ? pf(*end) : glm::vec3{0.f}, u2m);
        s.pos(p + "butt_d", butt != nullptr && mon, butt ? df(*butt) : glm::vec3{0.f}, u2m);
        s.pos(p + "end_d", end != nullptr && mon, end ? df(*end) : glm::vec3{0.f}, u2m);
        const glm::vec3* parry = qc ? sv->value("parry") : nullptr;
        if(parry)
        {
            s.integer(name("parry").cStr(), (h == HAND_MAIN ? parry->x : parry->y) != 0.f ? 1 : 0);
        }
        else
        {
            s.empty(name("parry").cStr());
        }
    }

    // The QC's guard state, and its other values.
    const glm::vec3* parry = sv && sv->qc ? sv->value("parry") : nullptr;
    const glm::vec3* guard = sv && sv->qc ? sv->value("guard") : nullptr;
    if(parry)
    {
        s.integer("parry_arms", parry->z != 0.f ? 1 : 0);
    }
    else
    {
        s.empty("parry_arms");
    }
    if(guard)
    {
        s.integer("guard", static_cast<int>(guard->x));
        s.integer("guard_hand", static_cast<int>(guard->y));
        s.integer("guard_ready", guard->z != 0.f ? 1 : 0);
    }
    else
    {
        s.empty("guard");
        s.empty("guard_hand");
        s.empty("guard_ready");
    }
    za::String extra;
    if(sv && sv->qc)
    {
        for(const auto& [k, v] : sv->values)
        {
            if(k == "parry" || k == "guard" || k == "m_butt" || k == "m_end" || k == "o_butt" || k == "o_end")
            {
                continue;
            }
            char buf[160];
            q_snprintf(buf, sizeof(buf), "%s%s=%g:%g:%g", extra.empty() ? "" : ";", eventField(k).cStr(), v.x, v.y, v.z);
            extra += buf;
        }
    }
    s.text("qc", extra);

    // The nearest monster (the training dummy): who, where (world, and in the player's frame), how it
    // faces, its box.
    if(mon)
    {
        s.integer("mon_ent", sv->monEnt);
        s.text("mon_class", sv->monClass);
        s.text("mon_targetname", sv->monTargetname);
        s.num("mon_health", sv->monHealth, 1);
    }
    else
    {
        s.empty("mon_ent");
        s.empty("mon_class");
        s.empty("mon_targetname");
        s.empty("mon_health");
    }
    s.vec("mon_w_", mon, mon ? sv->monOrigin : glm::vec3{0.f}, 3, xyz);
    s.pos("mon", mon, mon ? pf(sv->monOrigin) : glm::vec3{0.f}, u2m);
    if(mon)
    {
        s.num("mon_pitch", sv->monAngles.x, 3);
        s.num("mon_yaw_w", wrapYaw(sv->monAngles.y), 3);
        s.num("mon_roll", sv->monAngles.z, 3);
        s.num("mon_yaw", wrapYaw(sv->monAngles.y - yaw0), 3);
    }
    else
    {
        s.empty("mon_pitch");
        s.empty("mon_yaw_w");
        s.empty("mon_roll");
        s.empty("mon_yaw");
    }
    s.pos("mon_mins", mon, mon ? sv->monMins : glm::vec3{0.f}, u2m);
    s.pos("mon_maxs", mon, mon ? sv->monMaxs : glm::vec3{0.f}, u2m);
    if(mon)
    {
        // From the player's origin to the nearest point of its box.
        const glm::vec3 lo = sv->monOrigin + sv->monMins;
        const glm::vec3 hi = sv->monOrigin + sv->monMaxs;
        const glm::vec3 nearest = glm::clamp(r.origin, lo, hi);
        s.num("mon_dist_u", glm::distance(r.origin, nearest), 3);
    }
    else
    {
        s.empty("mon_dist_u");
    }

    // The melee events of this frame: kind:sub:hand:value:x:y:z:target:detail, ';' between events;
    // x y z in the player's frame (units).
    za::String events;
    for(const Event& e : r.events)
    {
        char at[96] = "::";
        if(e.hasAt)
        {
            const glm::vec3 a = pf(e.at);
            q_snprintf(at, sizeof(at), "%.2f:%.2f:%.2f", a.x, a.y, a.z);
        }
        char buf[512];
        q_snprintf(buf, sizeof(buf), "%s%s:%s:%s:%.3f:%s:%s:%s", events.empty() ? "" : ";", eventField(e.kind).cStr(),
            eventField(e.sub).cStr(), handName(e.hand), e.value, at, eventField(e.target).cStr(),
            eventField(e.detail).cStr());
        events += buf;
    }
    s.text("events", events);

    // The runtime's tracking, as it came (tracking space: metres, +x right, +y up, -z forward).
    const auto raw = [&](const za::String& p, const Pose& pose, bool grip) {
        s.vec(p, true, pose.position, 5, pxyz);
        s.num((p + "qw").cStr(), pose.orientation.w, 6);
        s.num((p + "qx").cStr(), pose.orientation.x, 6);
        s.num((p + "qy").cStr(), pose.orientation.y, 6);
        s.num((p + "qz").cStr(), pose.orientation.z, 6);
        s.vec(p, true, pose.linearVelocity, 5, vxyz);
        s.vec(p, true, pose.angularVelocity, 5, wxyz);
        s.integer((p + "valid").cStr(), pose.valid ? 1 : 0);
        s.integer((p + "vvalid").cStr(), pose.velocityValid ? 1 : 0);
        if(grip)
        {
            s.vec(p, true, pose.gripVelocity, 5, gxyz);
            s.integer((p + "gvalid").cStr(), pose.gripVelocityValid ? 1 : 0);
        }
    };
    raw("raw_head_", r.rawHead, false);
    raw("raw_m_", r.hands[HAND_MAIN].raw, true);
    raw("raw_o_", r.hands[HAND_OFF].raw, true);
}

[[nodiscard]] za::String weaponLine(const HandRow& h)
{
    return va("%d (flags %d, %s)%s%s", h.wid, h.wflags, h.model.empty() ? "-" : h.model.cStr(),
        h.helping ? ", helping the other hand" : "",
        h.grip2h == 1 ? ", two-handed by the foregrip" : h.grip2h == 2 ? ", two-handed by the blade" : "");
}

// The settings the melee reads, for the header.
[[nodiscard]] za::String meleeSettings()
{
    static const char* const prefixes[] = {"vr_melee_", "vr_bash", "vr_shove", "vr_parry", "vr_deflect", "vr_headbutt",
        "vr_sword_", "vr_damage_", "vr_push", "vr_hit_push", "vr_kill_push", "vr_carry_melee_mult", "vr_positional_damage",
        "vr_headshot_mult", "vr_limbshot_mult", "vr_legshot_mult"};
    za::String out;
    for(const cvar_t* var = Cvar_FindVarAfter("", 0); var; var = Cvar_FindVarAfter(var->name, 0))
    {
        for(const char* p : prefixes)
        {
            if(!strncmp(var->name, p, strlen(p)))
            {
                out += (out.empty() ? "" : " ") + za::String{var->name} + "=" + var->string;
                break;
            }
        }
    }
    return out;
}

// Every Quake VR setting a player keeps (archived vr_ cvars; the weapons' offsets are weaponSettings'),
// for playback to place the hands and weapons as they were (vr_motion_play applies the ones that do).
[[nodiscard]] za::String allSettings()
{
    za::String out;
    for(const cvar_t* var = Cvar_FindVarAfter("", CVAR_ARCHIVE); var; var = Cvar_FindVarAfter(var->name, CVAR_ARCHIVE))
    {
        if(strncmp(var->name, "vr_", 3) != 0 || !strncmp(var->name, "vr_wofs_", 8) || !strncmp(var->name, "vr_motion_", 10))
        {
            continue;
        }
        za::String value = var->string;
        za::replace(value.begin(), value.end(), ' ', '_'); // (none has spaces that matter here)
        out += (out.empty() ? "" : " ") + za::String{var->name} + "=" + value;
    }
    const cvar_t* maxfps = Cvar_FindVar("host_maxfps");
    out += za::String{" host_maxfps="} + (maxfps ? maxfps->string : "0");
    return out;
}

// The weapon offsets (vr_wofs_*) of the empty hand's slot and of the weapons in the hands.
[[nodiscard]] za::String weaponSettings()
{
    za::Vector<int> slots{weapons::fistSlot(), weapons::heldSlot(HAND_MAIN), weapons::heldSlot(HAND_OFF)};
    za::String out;
    for(size_t i = 0; i < slots.size(); i++)
    {
        const int slot = slots[i];
        if(slot < 0 || za::find(slots.begin(), slots.begin() + static_cast<long>(i), slot) != slots.begin() + static_cast<long>(i))
        {
            continue;
        }
        for(int key = 0; key < static_cast<int>(weapons::Key::Count); key++)
        {
            if(const cvar_t* var = weapons::cvar(slot, static_cast<weapons::Key>(key)))
            {
                out += (out.empty() ? "" : " ") + za::String{var->name} + "=" + var->string;
            }
        }
    }
    return out;
}

} // namespace

za::String takeHeader(const TakeInfo& info, const za::Vector<Row>& rows)
{
    int pre = 0, rec = 0, tail = 0;
    const Row* first = nullptr;
    for(const Row& r : rows)
    {
        pre += r.phase == PhasePre;
        rec += r.phase == PhaseRec;
        tail += r.phase == PhaseTail;
        if(!first && r.phase == PhaseRec)
        {
            first = &r;
        }
    }
    if(!first && !rows.empty())
    {
        first = &rows.front();
    }

    za::String h = "# Quake VR motion take (docs/vr-port/MOTIONS.md)\n";
    const auto line = [&](const char* key, za::String value) {
        za::replace(value.begin(), value.end(), '\n', ' ');
        za::replace(value.begin(), value.end(), '\r', ' ');
        h += za::String{"# "} + key + ": " + value + "\n";
    };
    line("format", za::toString(formatVersion));
    line("label", info.label);
    line("category", info.category);
    line("detail", info.detail);
    line("note", info.note);
    line("take", za::toString(info.take));
    line("date", info.date);
    line("source", info.source);
    line("map", info.map);
    line("dominant hand", vr_lefthanded.value ? "left (vr_lefthanded 1)" : "right (vr_lefthanded 0)");
    if(first)
    {
        line("main weapon", weaponLine(first->hands[HAND_MAIN]));
        line("off weapon", weaponLine(first->hands[HAND_OFF]));
    }
    line("vr_world_scale", vr_world_scale.string);
    line("units per metre", va("%.4f", units::metresToUnits()));
    line("vr_height_calibration", vr_height_calibration.string);
    line("vr_floor_offset", vr_floor_offset.string);
    line("hand angles", va("vr_gunangle %s vr_gunyaw %s vr_offhandpitch %s vr_offhandyaw %s vr_controller_legacy_pose %s",
                            vr_gunangle.string, vr_gunyaw.string, vr_offhandpitch.string, vr_offhandyaw.string,
                            vr_controller_legacy_pose.string));
    line("hand calibration", va("vr_handcal_x %s vr_handcal_y %s vr_handcal_z %s vr_handcal_roll %s vr_handcal_off_mirror %s "
                                "vr_handcal_off_x %s vr_handcal_off_y %s vr_handcal_off_z %s vr_handcal_off_roll %s",
                                 vr_handcal_x.string, vr_handcal_y.string, vr_handcal_z.string, vr_handcal_roll.string,
                                 vr_handcal_off_mirror.string, vr_handcal_off_x.string, vr_handcal_off_y.string,
                                 vr_handcal_off_z.string, vr_handcal_off_roll.string));
    line("grips", va("vr_weapon_grip_mode %s vr_2h_mode %s", vr_weapon_grip_mode.string, vr_2h_mode.string));
    const cvar_t* maxfps = Cvar_FindVar("host_maxfps");
    line("server rate", za::String{"host_maxfps "} + (maxfps ? maxfps->string : "?") + ", server frame " +
                            (host_netinterval > 0.f ? va("%.4f s (72 Hz)", host_netinterval) : "every host frame"));
    line("yaw0", va("%.3f", info.yaw0));
    line("origin0", va("%.3f %.3f %.3f", info.origin0.x, info.origin0.y, info.origin0.z));
    if(first && first->sv && first->sv->monster)
    {
        const ServerSample& sv = *first->sv;
        line("target", va("%s #%d%s%s at %.3f %.3f %.3f, angles %.1f %.1f %.1f, box %.1f %.1f %.1f .. %.1f %.1f %.1f",
                           sv.monClass.cStr(), sv.monEnt, sv.monTargetname.empty() ? "" : " targetname ",
                           sv.monTargetname.cStr(), sv.monOrigin.x, sv.monOrigin.y, sv.monOrigin.z, sv.monAngles.x,
                           sv.monAngles.y, sv.monAngles.z, sv.monMins.x, sv.monMins.y, sv.monMins.z, sv.monMaxs.x,
                           sv.monMaxs.y, sv.monMaxs.z));
    }
    else
    {
        line("target", "none");
    }
    // The training dummy struck back (vr_dummy_attacks, at any moment of the take): its "strike" events, which a
    // replay reproduces (vr_motion_play.cpp). Not written without.
    if(za::anyOf(rows.begin(), rows.end(), [](const Row& r) { return r.dummyAttacks; }))
    {
        line("dummy attacks", "on");
    }
    line("melee settings", meleeSettings());
    line("settings", allSettings());
    line("weapon settings", weaponSettings());
    line("rows", va("%d (pre-roll %d, held %d, tail %d)", static_cast<int>(rows.size()), pre, rec, tail));
    return h;
}

bool writeTakeFile(const za::String& path, const za::String& header, const TakeInfo& info, float u2m,
    const za::Vector<Row>& rows)
{
    FILE* f = fopen(path.cStr(), "wb");
    if(!f)
    {
        return false;
    }
    fputs(header.cStr(), f);
    Sink names(true);
    emitRow(names, rows.empty() ? Row{} : rows.front(), 0, info, u2m);
    fprintf(f, "%s\n", names.line().cStr());
    int frame = 0;
    for(const Row& r : rows)
    {
        Sink cells(false);
        emitRow(cells, r, frame++, info, u2m);
        fprintf(f, "%s\n", cells.line().cStr());
    }
    const bool ok = !ferror(f);
    return fclose(f) == 0 && ok;
}

bool writeTake(const za::String& path, const TakeInfo& info, const za::Vector<Row>& rows)
{
    if(!writeTakeFile(path, takeHeader(info, rows), info, 1.f / units::metresToUnits(), rows))
    {
        Con_Printf("Motion recorder: can't write %s\n", path.cStr());
        return false;
    }
    return true;
}

za::String takeHeader(const TakeInfo& info, const za::Vector<Row>& rows);
bool writeTakeFile(const za::String& path, const za::String& header, const TakeInfo& info, float u2m,
    const za::Vector<Row>& rows);
bool playDummyAttacks(); // vr_motion_play.cpp: a replay reproducing the dummy's strikes

namespace
{

// ----------------------------------------------------------------------------
// The server's samples and the QC's events
// ----------------------------------------------------------------------------

std::shared_ptr<const ServerSample> latest; // ZANCLE-TODO: no shared ownership (std::shared_ptr)
ServerSample* sampling = nullptr; // the sample VR_Motion_Sample is filling
bool tickThisFrame = false;
double tickDt = 0.0;
za::Vector<Event> frameEvents;

bool armedOrRecording();

// The nearest live monster to `from` (FL_MONSTER, the training dummy too), or null.
[[nodiscard]] edict_t* nearestMonster(const glm::vec3& from)
{
    edict_t* best = nullptr;
    float bestDist = 4096.f;
    for(int i = 1; i < qcvm->num_edicts; i++)
    {
        edict_t* e = EDICT_NUM(i);
        if(e->free || !(static_cast<int>(e->v.flags) & FL_MONSTER) || e->v.health <= 0.f ||
           static_cast<int>(e->v.solid) == SOLID_NOT)
        {
            continue;
        }
        const glm::vec3 mid{e->v.origin[0] + (e->v.mins[0] + e->v.maxs[0]) * 0.5f,
            e->v.origin[1] + (e->v.mins[1] + e->v.maxs[1]) * 0.5f, e->v.origin[2] + (e->v.mins[2] + e->v.maxs[2]) * 0.5f};
        const float d = glm::distance(mid, from);
        if(d < bestDist)
        {
            bestDist = d;
            best = e;
        }
    }
    return best;
}

// ----------------------------------------------------------------------------
// Recording
// ----------------------------------------------------------------------------

constexpr double maxTakeSeconds = 60.0;
constexpr double minHeldSeconds = 0.2; // shorter: an accidental press, dropped

enum class Rec
{
    Off,
    Recording, // the motion (from the button's press to its next)
    Tail,      // after it, for vr_motion_tail seconds
};

Rec rec = Rec::Off;
void finishTake();
za::Vector<qza::Pair<TakeInfo, za::Vector<Row>>> unsaved; // takes that could not be written
// The buttons the recorder can take (vr_motion_button's), an index into taken's.
constexpr bool HandInput::*recButtons[] = {
    &HandInput::trigger, &HandInput::grip, &HandInput::primary, &HandInput::secondary, &HandInput::stickClick};
constexpr int recButtonCount = static_cast<int>(za::getArraySize(recButtons));
bool taken[HAND_COUNT][recButtonCount]{}; // the recorder took this button's press (its release too)
za::Vector<Row> rows;
za::Vector<Row> preroll; // (first in, first out)
TrackingState rawTracking;
double stopTime = 0.0;
za::String takeLabel;
za::String takeStamp;
za::String takeDate;
int takeNumber = 0;
za::String lastSavedName;
za::Vector<za::String> savedThisSession; // for Delete Last Take
double feedbackUntil = 0.0; // "SAVED" / "DROPPED" shown until then
za::String feedbackText;
ankerl::unordered_dense::map<za::String, int> takeCounts;
bool countsValid = false;

// vr_motion_button's choices (VR Settings > Advanced > Motion Recorder, "Record Button"): a button of one hand, pressed
// to start a take and again to end it; with `withTrigger`, held while that hand's trigger is pulled (the button alone
// does nothing then). Whichever it is, it is the recorder's while armed: the game never gets it (only the trigger
// pulled without the button, for a combination).
struct RecordBinding
{
    int hand;
    bool HandInput::*button;
    bool withTrigger;
    const char* text; // the HUD's instruction
};

constexpr RecordBinding recordBindings[] = {
    {HAND_OFF, &HandInput::stickClick, false, "click the off stick"},              // 0 (the default)
    {HAND_MAIN, &HandInput::stickClick, false, "click the main stick"},            // 1
    {HAND_MAIN, &HandInput::primary, false, "press A"},                            // 2
    {HAND_MAIN, &HandInput::secondary, false, "press B"},                          // 3
    {HAND_OFF, &HandInput::primary, false, "press X"},                             // 4
    {HAND_OFF, &HandInput::secondary, false, "press Y"},                           // 5
    {HAND_OFF, &HandInput::grip, false, "squeeze the off grip"},                   // 6
    {HAND_MAIN, &HandInput::grip, false, "squeeze the main grip"},                 // 7
    {HAND_MAIN, &HandInput::secondary, true, "hold B, pull the main trigger"},     // 8
    {HAND_OFF, &HandInput::secondary, true, "hold Y, pull the off trigger"},       // 9
};

[[nodiscard]] const RecordBinding& recordBinding()
{
    const int i = static_cast<int>(vr_motion_button.value);
    return recordBindings[i >= 0 && i < static_cast<int>(za::getArraySize(recordBindings)) ? i : 0];
}

[[nodiscard]] int recordHand()
{
    return recordBinding().hand;
}

[[nodiscard]] int recButtonIndex(bool HandInput::*button)
{
    for(int i = 0; i < recButtonCount; i++)
    {
        if(recButtons[i] == button)
        {
            return i;
        }
    }
    return -1;
}

bool armedOrRecording()
{
    return rec != Rec::Off || vr_motion_armed.value != 0.f;
}

// The label of a take's file name, <label>_YYYY-MM-DD_HH-MM-SS[-n].csv ("" for another file).
[[nodiscard]] za::String labelOf(const za::String& name)
{
    za::StringView label, stamp;
    return parseTakeName(name, label, stamp) ? za::String{label} : za::String{};
}

void countTakes()
{
    takeCounts.clear();
    countsValid = true;
    files::forEachEntry(motionsDir().cStr(), [](const char* name, bool) {
        if(const za::String label = labelOf(name); !label.empty())
        {
            takeCounts[label]++;
        }
    });
}

[[nodiscard]] int takeCount(const za::String& label)
{
    if(!countsValid)
    {
        countTakes();
    }
    const auto it = takeCounts.find(label);
    return it == takeCounts.end() ? 0 : it->second;
}

// The takes of a category, all its details together.
[[nodiscard]] int categoryCount(const za::String& category)
{
    if(!countsValid)
    {
        countTakes();
    }
    int n = 0;
    for(const auto& [label, count] : takeCounts)
    {
        if(categoryOf(label) == category)
        {
            n += count;
        }
    }
    return n;
}

void haptic(int hand, float seconds, float amplitude)
{
    if(Backend* be = backend(); be && !vr_disablehaptics.value)
    {
        be->haptic(hand, seconds, 160.f, amplitude);
    }
}

void startTake(const za::String& label)
{
    if(rec != Rec::Off)
    {
        return;
    }
    if(!hands::current().valid)
    {
        Con_Printf("Motion recorder: not in a game with VR hands\n");
        S_LocalSound("doors/basetry.wav");
        return;
    }

    takeLabel = safeLabel(label);
    takeNumber = takeCount(takeLabel) + 1;
    const time_t now = time(nullptr);
    char stamp[64], date[64];
    strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", localtime(&now));
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", localtime(&now));
    takeStamp = stamp;
    takeDate = date;

    rows.assignRange(preroll.begin(), preroll.end());
    for(Row& r : rows)
    {
        r.phase = PhasePre;
    }
    preroll.clear();
    rec = Rec::Recording;
    haptic(recordHand(), 0.06f, 0.8f);
    S_LocalSound("misc/talk.wav");
    Con_Printf("Motion recorder: recording %s #%d\n", takeLabel.cStr(), takeNumber);
}

void stopTake()
{
    if(rec != Rec::Recording)
    {
        return;
    }
    rec = Rec::Tail;
    stopTime = realtime;
    if(vr_motion_tail.value <= 0.f)
    {
        finishTake();
    }
}

// A take being written in the background (a long take's file takes a few frames to format).
struct PendingSave
{
    jobs::Future<int> result; // (the game's thread pool) 1 written in motions/, 2 in the game folder (fallback), 0 not at all
    za::String path;
    za::String fallback;
    TakeInfo info;
    std::shared_ptr<const za::Vector<Row>> rows; // ZANCLE-TODO: no shared ownership (std::shared_ptr)
    double held{0.0};
};
za::Vector<PendingSave> pendingSaves;

// Where a take goes: <dir>/<label>_<date>_<time>.csv, never over another (nor one being written).
[[nodiscard]] za::String takePath(const za::String& dir)
{
    const za::String name = takeLabel + "_" + takeStamp;
    const auto taken = [&](const za::String& path) {
        if(Sys_FileType(path.cStr()) != FS_ENT_NONE)
        {
            return true;
        }
        return za::anyOf(pendingSaves.begin(), pendingSaves.end(),
            [&](const PendingSave& p) { return p.path == path || p.fallback == path; });
    };
    za::String path = dir + "/" + name + ".csv";
    for(int n = 2; taken(path); n++)
    {
        path = dir + "/" + name + "-" + za::toString(n) + ".csv";
    }
    return path;
}

void finishTake()
{
    rec = Rec::Off;
    za::Vector<Row> take;
    take.swap(rows);

    const Row* first = nullptr;
    double held = 0.0;
    for(const Row& r : take)
    {
        if(r.phase == PhaseRec)
        {
            if(!first)
            {
                first = &r;
            }
            held = r.realtime - first->realtime;
        }
    }
    // The last rows go on as the next take's lead-in.
    for(const Row& r : take)
    {
        if(vr_motion_armed.value && r.realtime >= take.back().realtime - vr_motion_preroll.value)
        {
            preroll.pushBack(r);
        }
    }
    if(!first || held < minHeldSeconds)
    {
        Con_Printf("Motion recorder: %s #%d too short (%.2f s), not kept\n", takeLabel.cStr(), takeNumber, held);
        feedbackText = va("NOT KEPT: too short (%.2f s)", held);
        feedbackUntil = realtime + 2.0;
        haptic(recordHand(), 0.25f, 0.4f);
        S_LocalSound("doors/basetry.wav");
        return;
    }

    TakeInfo info;
    info.label = takeLabel;
    info.category = categoryOf(takeLabel);
    info.detail = info.category.empty() || takeLabel.size() <= info.category.size()
                      ? za::String{}
                      : takeLabel.substrByPosLen(info.category.size() + 1);
    info.note = vr_motion_note.string;
    info.date = takeDate;
    info.map = cl.mapname;
    const Backend* be = backend();
    info.source = be ? va("%s (%s)", be->name(), be->runtimeName()) : "flat screen";
    info.yaw0 = first->headAngles.y;
    info.origin0 = first->origin;
    info.t0 = first->realtime;
    info.take = takeNumber;

    // Written in the background; never lost quietly: if it can't go into motions/, it goes into the
    // game folder, and failing that stays in memory (vr_motion_save_unsaved tries again), with a warning
    // (pollSaves).
    PendingSave p;
    p.path = takePath(motionsDir());
    p.fallback = takePath(com_gamedir);
    p.info = info;
    p.held = held;
    auto shared = std::make_shared<const za::Vector<Row>>(ZA_MOVE(take));
    p.rows = shared;
    const za::String header = takeHeader(info, *shared);
    const float u2m = 1.f / units::metresToUnits();
    p.result = jobs::async([path = p.path, fallback = p.fallback, header, info, u2m, shared]() {
        if(writeTakeFile(path, header, info, u2m, *shared))
        {
            return 1;
        }
        return writeTakeFile(fallback, header, info, u2m, *shared) ? 2 : 0;
    });
    pendingSaves.pushBack(ZA_MOVE(p));
}

// The takes written since the last frame: their feedback (or the alarm).
void pollSaves(bool wait)
{
    for(auto it = pendingSaves.begin(); it != pendingSaves.end();)
    {
        if(!wait && !it->result.ready())
        {
            ++it;
            continue;
        }
        const int written = it->result.get();
        PendingSave p = ZA_MOVE(*it);
        it = pendingSaves.erase(it);
        const za::String label = p.info.label;
        if(written == 0)
        {
            Con_Warning("Motion recorder: %s #%d NOT SAVED (%s): kept in memory, vr_motion_save_unsaved retries\n",
                label.cStr(), p.info.take, p.path.cStr());
            unsaved.pushBack({p.info, *p.rows});
            feedbackText = "NOT SAVED! (see the console)";
            feedbackUntil = realtime + 6.0;
            haptic(recordHand(), 0.4f, 1.f);
            S_LocalSound("doors/basetry.wav");
            continue;
        }
        const za::String path = written == 1 ? p.path : p.fallback;
        if(written == 2)
        {
            Con_Warning("Motion recorder: couldn't write %s; saved in the game folder instead\n", p.path.cStr());
        }
        savedThisSession.pushBack(path);
        lastSavedName = za::String{files::fileName(path)};
        takeCounts[label]++;
        review::invalidate(); // a new take to list
        int events = 0;
        for(const Row& r : *p.rows)
        {
            events += static_cast<int>(r.events.size());
        }
        const za::String category = p.info.category.empty() ? label : p.info.category;
        const za::String shown = written == 1 ? "motions/" + lastSavedName : path;
        const int count = p.info.category.empty() ? takeCount(label) : categoryCount(category);
        Con_Printf("Motion recorder: saved %s (%.2f s, %d frames, %d events); %s: %d take%s\n", shown.cStr(), p.held,
            static_cast<int>(p.rows->size()), events, category.cStr(), count, count == 1 ? "" : "s");
        feedbackText = va("SAVED %s #%d (%s: %d)", label.cStr(), p.info.take, category.cStr(), count);
        feedbackUntil = realtime + 2.5;
        haptic(recordHand(), 0.12f, 0.6f);
        S_LocalSound("misc/menu2.wav");
    }
}

// The frame's row, from the hands (world), the runtime's tracking, the weapons and the last sample.
[[nodiscard]] Row makeRow(const hands::State& s, bool tick, double svDt)
{
    Row r;
    r.realtime = realtime;
    r.dt = host_rawframetime;
    r.xrTime = rawTracking.time;
    r.tick = tick;
    r.svDt = svDt;
    r.dummyAttacks = vr_dummy_attacks.value != 0.f || playDummyAttacks();
    r.origin = s.playerOrigin;
    r.lean = s.lean;
    r.head = s.head;
    r.headAngles = s.headAngles;
    r.headVel = s.headVel;
    r.aim = {cl.viewangles[0], cl.viewangles[1], cl.viewangles[2]};
    r.bodyYaw = s.bodyYaw;
    r.crouch = s.crouchRatio;
    r.playYaw = hands::playSpaceYaw();
    r.rawHead = rawTracking.head;

    for(int h = 0; h < HAND_COUNT; h++)
    {
        HandRow& hr = r.hands[h];
        hr.pos = s.pos[h];
        hr.rot = s.rot[h];
        hr.vel = s.vel[h];
        hr.angVel = s.angVel[h];
        hr.raw = rawTracking.hands[h];
        hr.input = rawTracking.input.hands[h];
        for(int b = 0; b < recButtonCount; b++)
        {
            if(taken[h][b])
            {
                hr.input.*recButtons[b] = false; // the recorder's: the game never saw it
                if(recButtons[b] == &HandInput::grip)
                {
                    hr.input.gripValue = 0.f; // (else a replay's grip filter would make it a press again)
                }
            }
        }
        for(int f = 0; f < 5; f++)
        {
            hr.curl[f] = view::fingerCurl(h, f);
        }
        hr.wid = cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPON : protocol::STAT_QVR_WEAPON2];
        hr.wflags = cl.stats[h == HAND_MAIN ? protocol::STAT_QVR_WEAPONFLAGS : protocol::STAT_QVR_WEAPONFLAGS2];
        const int model = cl.stats[h == HAND_MAIN ? int{STAT_WEAPON} : int{protocol::STAT_QVR_WEAPONMODEL2}];
        hr.model = model > 0 && model < MAX_MODELS && cl.model_precache[model] ? cl.model_precache[model]->name : "";
        hr.helping = twohand::helping(h);
        hr.twoHand = twohand::transition(h);
        hr.grip2h = hr.twoHand > 0.f ? (twohand::bladeGrip(h) ? 2 : 1) : 0;
        hr.carried = twohand::carrying(h);
        hr.hotspot = s.hotspot[h];
        hr.muzzleOk = s.muzzleValid[h];
        hr.muzzle = s.muzzle[h];
    }
    r.sv = latest;
    r.events.swap(frameEvents);
    return r;
}

void saveUnsaved_f()
{
    if(unsaved.empty())
    {
        Con_Printf("Motion recorder: nothing unsaved\n");
        return;
    }
    for(auto it = unsaved.begin(); it != unsaved.end();)
    {
        takeLabel = it->first.label;
        takeStamp = it->first.date; // YYYY-MM-DD HH:MM:SS, as a file name's
        za::replace(takeStamp.begin(), takeStamp.end(), ' ', '_');
        za::replace(takeStamp.begin(), takeStamp.end(), ':', '-');
        const za::String path = takePath(motionsDir());
        if(writeTake(path, it->first, it->second))
        {
            Con_Printf("Motion recorder: saved %s\n", path.cStr());
            it = unsaved.erase(it);
            countsValid = false;
            review::invalidate();
        }
        else
        {
            ++it;
        }
    }
}

// vr_motion_record [<label>]: a take of the label (by default the menu's category and detail), until
// vr_motion_stop.
void record_f()
{
    if(rec != Rec::Off)
    {
        Con_Printf("Motion recorder: already recording %s\n", takeLabel.cStr());
        return;
    }
    startTake(Cmd_Argc() > 1 ? Cmd_Argv(1) : chosenLabel());
}

void stop_f()
{
    stopTake();
}

// vr_motion_list [<label or category>]: the takes of each category and label, or the files of one.
void list_f()
{
    countTakes();
    const za::String only = Cmd_Argc() > 1 ? Cmd_Argv(1) : "";
    if(!only.empty())
    {
        za::Vector<za::String> takes;
        files::forEachEntry(motionsDir().cStr(), [&](const char* name, bool) {
            const za::String label = labelOf(name);
            if(!label.empty() && (label == only || categoryOf(label) == only))
            {
                takes.pushBack(za::String{name});
            }
        });
        za::quickSort(takes.begin(), takes.end());
        for(const za::String& f : takes)
        {
            Con_Printf("  %s\n", f.cStr());
        }
        Con_Printf("%d take%s of %s in %s\n", static_cast<int>(takes.size()), takes.size() == 1 ? "" : "s", only.cStr(),
            motionsDir().cStr());
        return;
    }
    int total = 0;
    for(const Category& c : categories())
    {
        const int n = categoryCount(c.choice.name);
        total += n;
        Con_Printf("%-24s %3d\n", c.choice.name, n);
        for(const auto* entry : qza::sortedByKey(takeCounts)) // (in the labels' order, as a std::map had them)
        {
            const auto& [label, count] = *entry;
            if(categoryOf(label) == c.choice.name && label != c.choice.name)
            {
                Con_Printf("  %-22s %3d\n", label.cStr() + strlen(c.choice.name) + 1, count);
            }
        }
    }
    for(const auto* entry : qza::sortedByKey(takeCounts)) // (in the labels' order, as a std::map had them)
    {
        const auto& [label, n] = *entry;
        if(categoryOf(label).empty())
        {
            total += n;
            Con_Printf("%-24s %3d (no category)\n", label.cStr(), n);
        }
    }
    Con_Printf("%d takes in %s\n", total, motionsDir().cStr());
}

void discard_f()
{
    discardLast();
}

// Disarming ends a take going on.
void armedChanged(cvar_t* /* var */)
{
    if(!vr_motion_armed.value && rec == Rec::Recording)
    {
        stopTake();
    }
}

// The category's details reset to none when the category changes.
void categoryChanged(cvar_t* /* var */)
{
    if(vr_motion_detail.value != 0.f)
    {
        Cvar_SetValueQuick(&vr_motion_detail, 0.f);
    }
}


} // namespace

// ----------------------------------------------------------------------------
// The interface
// ----------------------------------------------------------------------------

std::shared_ptr<const ServerSample> latestSample() // ZANCLE-TODO: no shared ownership (std::shared_ptr)
{
    return latest;
}

void initPlayback(); // vr_motion_play.cpp
void playAfterTracking(TrackingState& tracking, FrameState& frame);
void playServerSample(edict_t*& target);
void playFrameEnd(za::Vector<Event>& events, bool tick, double svDt);
void playServerFrame();
bool playWantsSamples();

void invalidateTakeCounts()
{
    countsValid = false;
}

void init()
{
    review::init();
    Cmd_AddCommand("vr_motion_record", record_f);
    Cmd_AddCommand("vr_motion_stop", stop_f);
    Cmd_AddCommand("vr_motion_list", list_f);
    Cmd_AddCommand("vr_motion_discard", discard_f);
    Cmd_AddCommand("vr_motion_save_unsaved", saveUnsaved_f);
    Cvar_SetCallback(&vr_motion_category, categoryChanged);
    Cvar_SetCallback(&vr_motion_armed, armedChanged);
    initPlayback();
}

void shutdown()
{
    if(rec != Rec::Off)
    {
        finishTake();
    }
    pollSaves(true); // the takes being written finish
}

void afterTracking(TrackingState& tracking, FrameState& frame)
{
    playAfterTracking(tracking, frame);
    rawTracking = tracking;
}

// Press to start a take, press again to end it (the release does nothing, and is the recorder's too).
bool button(int hand, bool HandInput::*which, bool pressed)
{
    const int b = recButtonIndex(which);
    if(hand < 0 || hand >= HAND_COUNT || b < 0)
    {
        return false;
    }
    if(!pressed)
    {
        const bool was = taken[hand][b];
        taken[hand][b] = false;
        return was;
    }
    // (A take going on ends on the button whatever else: disarmed meanwhile, or a menu open.)
    const RecordBinding& k = recordBinding();
    if(hand != k.hand || playing() || (rec != Rec::Recording && (!vr_motion_armed.value || key_dest != key_game)))
    {
        return false;
    }
    if(which == k.button)
    {
        taken[hand][b] = true; // a combination's button alone does nothing
        if(k.withTrigger)
        {
            return true;
        }
    }
    else if(!(k.withTrigger && which == &HandInput::trigger && taken[hand][recButtonIndex(k.button)]))
    {
        return false;
    }
    taken[hand][b] = true;
    if(rec == Rec::Recording)
    {
        stopTake();
    }
    else
    {
        if(rec == Rec::Tail)
        {
            finishTake(); // the last one saved at once: the next starts
        }
        startTake(chosenLabel());
    }
    return true;
}

const char* recordButtonText()
{
    return recordBinding().text;
}

Row captureRow(bool tick, double svDt)
{
    return makeRow(hands::current(), tick, svDt);
}

void serverFrame()
{
    tickThisFrame = true;
    tickDt = host_frametime;
    playServerFrame(); // a playback's placing and weapons
    review::serverFrame(); // where the review's ghost goes
    if(!armedOrRecording() && !playWantsSamples())
    {
        latest.reset();
        return;
    }
    if(!sv.active || !svs.clients[0].active || !progs::bindings().isVrProgs)
    {
        return;
    }

    edict_t* player = svs.clients[0].edict;
    auto s = std::make_shared<ServerSample>();
    s->time = qcvm->time;
    s->player = true;
    s->origin = {player->v.origin[0], player->v.origin[1], player->v.origin[2]};
    s->velocity = {player->v.velocity[0], player->v.velocity[1], player->v.velocity[2]};
    s->onGround = (static_cast<int>(player->v.flags) & FL_ONGROUND) != 0;

    edict_t* target = nearestMonster(s->origin);
    playServerSample(target); // a playback's target, while it plays
    if(target)
    {
        s->monster = true;
        s->monEnt = NUM_FOR_EDICT(target);
        s->monClass = PR_GetString(target->v.classname);
        s->monOrigin = {target->v.origin[0], target->v.origin[1], target->v.origin[2]};
        s->monMins = {target->v.mins[0], target->v.mins[1], target->v.mins[2]};
        s->monMaxs = {target->v.maxs[0], target->v.maxs[1], target->v.maxs[2]};
        s->monAngles = {target->v.angles[0], target->v.angles[1], target->v.angles[2]};
        s->monTargetname = PR_GetString(target->v.targetname);
        s->monHealth = target->v.health;
    }

    if(const func_t fn = progs::bindings().Motion_Sample)
    {
        sampling = s.get();
        s->qc = true;
        pr_global_struct->time = qcvm->time;
        pr_global_struct->self = EDICT_TO_PROG(player);
        pr_global_struct->other = EDICT_TO_PROG(qcvm->edicts);
        G_INT(OFS_PARM0) = target ? EDICT_TO_PROG(target) : 0; // (world)
        PR_ExecuteProgram(fn);
        sampling = nullptr;
    }
    latest = ZA_MOVE(s);
}

void hostFrameEnd()
{
    const bool tick = tickThisFrame;
    tickThisFrame = false;
    const double svDt = tick ? tickDt : 0.0;

    pollSaves(false);
    playFrameEnd(frameEvents, tick, svDt);
    if(!armedOrRecording())
    {
        frameEvents.clear();
        preroll.clear();
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid)
    {
        frameEvents.clear();
        preroll.clear();
        if(rec != Rec::Off)
        {
            Con_Printf("Motion recorder: the game ended; the take is kept as it is\n");
            finishTake();
        }
        return;
    }

    Row r = makeRow(s, tick, svDt);
    if(rec == Rec::Off)
    {
        preroll.pushBack(ZA_MOVE(r));
        za::SizeT drop = 0; // (the oldest, gone at once)
        while(drop < preroll.size() && preroll[drop].realtime < realtime - vr_motion_preroll.value)
        {
            drop++;
        }
        preroll.erase(preroll.begin(), preroll.begin() + drop);
        return;
    }

    r.phase = rec == Rec::Recording ? PhaseRec : PhaseTail;
    rows.pushBack(ZA_MOVE(r));
    if(rec == Rec::Recording)
    {
        const Row* first = nullptr;
        for(const Row& row : rows)
        {
            if(row.phase == PhaseRec)
            {
                first = &row;
                break;
            }
        }
        if(first && realtime - first->realtime > maxTakeSeconds)
        {
            Con_Printf("Motion recorder: a take is at most %.0f seconds\n", maxTakeSeconds);
            stopTake();
        }
    }
    if(rec == Rec::Tail && realtime - stopTime >= vr_motion_tail.value)
    {
        finishTake();
    }
}

void frame()
{
    review::frame(); // the review's ghost and re-evaluation
    if(!armedOrRecording() && realtime >= feedbackUntil)
    {
        return;
    }
    const hands::State& s = hands::current();
    if(!s.valid || key_dest != key_game)
    {
        return;
    }

    za::String text;
    if(rec == Rec::Recording)
    {
        double held = 0.0;
        for(const Row& r : rows)
        {
            if(r.phase == PhaseRec)
            {
                held = realtime - r.realtime;
                break;
            }
        }
        // "REC" in Quake's alternate (gold) letters.
        text = va("%c%c%c %s #%d  %.1f s", 'R' | 0x80, 'E' | 0x80, 'C' | 0x80, takeLabel.cStr(), takeNumber, held);
    }
    else if(rec == Rec::Tail)
    {
        text = va("%s #%d: saving", takeLabel.cStr(), takeNumber);
    }
    else if(realtime < feedbackUntil)
    {
        text = feedbackText;
    }
    else
    {
        const za::String label = chosenLabel();
        text = va("armed: %s #%d (%s)", label.cStr(), takeCount(label) + 1, recordButtonText());
    }

    const float m2u = units::metresToUnits();
    const glm::vec3 fwd = hands::forward(glm::vec3{0.f, s.headAngles.y, 0.f});
    const glm::vec3 at = s.head + fwd * (0.7f * m2u) - glm::vec3{0.f, 0.f, 0.22f * m2u};
    text3d::queueOverlay(text, at, glm::vec3{0.f, s.headAngles.y, 0.f}, rec == Rec::Off ? 0.05f : 0.07f);
}

void qcEvent(const char* kind, const char* sub, int hand, float value, const float* at, edict_t* targ, const char* detail)
{
    if(!armedOrRecording() && !playing())
    {
        return;
    }
    Event e;
    e.kind = kind ? kind : "";
    e.sub = sub ? sub : "";
    e.hand = hand;
    e.value = value;
    e.hasAt = at && (at[0] != 0.f || at[1] != 0.f || at[2] != 0.f);
    if(e.hasAt)
    {
        e.at = {at[0], at[1], at[2]};
    }
    e.target = targ && NUM_FOR_EDICT(targ) != 0 ? PR_GetString(targ->v.classname) : "";
    e.detail = detail ? detail : "";
    frameEvents.pushBack(ZA_MOVE(e));
}

void qcPoint(int hand, const float* at, const char* name)
{
    if(sampling && (hand == HAND_OFF || hand == HAND_MAIN))
    {
        sampling->points[hand].pushBack({{at[0], at[1], at[2]}, name ? name : ""});
    }
}

void qcValue(const char* key, const float* value)
{
    if(sampling && key)
    {
        sampling->values.emplaceBack(key, glm::vec3{value[0], value[1], value[2]});
    }
}

namespace
{

// The menu's text, valid until the next call (the menu draws it at once).
struct MotionReadouts
{
    za::String labelStatus;
    auto members() { return qvr::mem::list(labelStatus); }
};
mem::Scratch<MotionReadouts> readouts{"motion readouts"};

} // namespace

const char* labelStatus()
{
    za::String& text = readouts.labelStatus;
    const za::String label = chosenLabel();
    const za::String category = chosenCategory().choice.name;
    const int n = takeCount(label);
    text = va("%s: %d take%s", label.cStr(), n, n == 1 ? "" : "s");
    if(label != category)
    {
        text += va(" (%d in all)", categoryCount(category));
    }
    return text.cStr();
}

const char* lastSaved()
{
    return lastSavedName.empty() ? "-" : lastSavedName.cStr();
}

// Moves the last take saved (again: the one before) into motions/discarded/.
void discardLast()
{
    // Takes the review moved or relabelled since (Review Takes) are no longer this session's to delete.
    while(!savedThisSession.empty() && !files::exists(savedThisSession.back().cStr()))
    {
        savedThisSession.popBack();
    }
    if(savedThisSession.empty())
    {
        Con_Printf("Motion recorder: no take saved in this session to delete\n");
        S_LocalSound("doors/basetry.wav");
        return;
    }
    const za::String from = savedThisSession.back();
    const za::String name{files::fileName(from)};
    const za::String dir = motionsDir() + "/discarded";
    Sys_mkdir(dir.cStr());
    if(rename(from.cStr(), (dir + "/" + name).cStr()) != 0) // (<stdio.h>'s)
    {
        Con_Printf("Motion recorder: can't move %s into motions/discarded/\n", name.cStr());
        S_LocalSound("doors/basetry.wav");
        return;
    }
    savedThisSession.popBack();
    countsValid = false;
    review::invalidate();
    Con_Printf("Motion recorder: deleted %s (moved into motions/discarded/)\n", name.cStr());
    feedbackText = va("DELETED %s", name.cStr());
    feedbackUntil = realtime + 2.0;
    lastSavedName = savedThisSession.empty() ? za::String{} : za::String{files::fileName(savedThisSession.back())};
    S_LocalSound("misc/menu3.wav");
}

bool parseTakeName(za::StringView name, za::StringView& label, za::StringView& stamp)
{
    // As the regular expression ^(.+)_(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}(-\d+)?)\.csv$ matched it: the longest label
    // (.+ is greedy) whose rest is the stamp and ".csv".
    const auto digits = [](za::StringView s, za::SizeT at, za::SizeT n) {
        for(za::SizeT i = at; i < at + n; i++)
        {
            if(i >= s.size() || s[i] < '0' || s[i] > '9')
            {
                return false;
            }
        }
        return true;
    };
    const auto isStamp = [&](za::StringView s) { // _YYYY-MM-DD_HH-MM-SS[-n].csv, whole
        if(s.size() < 24 || s[0] != '_' || !digits(s, 1, 4) || s[5] != '-' || !digits(s, 6, 2) || s[8] != '-' ||
            !digits(s, 9, 2) || s[11] != '_' || !digits(s, 12, 2) || s[14] != '-' || !digits(s, 15, 2) || s[17] != '-' ||
            !digits(s, 18, 2) || !s.endsWith(".csv"))
        {
            return false;
        }
        const za::StringView rest = s.substrByPosLen(20, s.size() - 24); // between the seconds and ".csv"
        return rest.empty() || (rest.size() >= 2 && rest[0] == '-' && digits(rest, 1, rest.size() - 1));
    };
    for(za::SizeT k = name.size(); k-- > 1;)
    {
        if(name[k] == '_' && isStamp(name.substrByPosLen(k)))
        {
            label = name.substrByPosLen(0, k);
            stamp = name.substrByPosLen(k + 1, name.size() - k - 5);
            return true;
        }
    }
    return false;
}

} // namespace qvr::motion
