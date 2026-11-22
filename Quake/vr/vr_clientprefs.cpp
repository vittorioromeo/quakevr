// vr_clientprefs.cpp -- see vr_clientprefs.hpp.

#include "vr_clientprefs.hpp"

#include "vr_client.hpp"

#include "vr_cvars.hpp"
#include "vr_cvars_util.hpp"
#include "vr_grip.hpp"
#include "vr_gripfit.hpp"
#include "vr_held.hpp"
#include "vr_heldpose.hpp"
#include "vr_moveguard.hpp"
#include "vr_progs.hpp"
#include "vr_tunables_api.hpp"
#include "vr_mem.hpp"
#include "Zancle/Base/IsFinite.hpp"
#include "Zancle/Base/Memcpy.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include "Zancle/Base/Strlen.hpp"
#include "Zancle/Math/Abs.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <math.h>

namespace qvr::clientprefs
{
namespace
{

// The per-player preferences: settings of how a player's own hands and controls work that the server's QC reads (each
// of them through cvar_hget). Balance stays the server's (server rules: vr_serverrules.cpp, B6); what only his own
// client reads needs no sending. The list's order is the "vr_prefs" command's: QVR_NET_VERSION changes with it.
constexpr const char* prefNames[] = {
    // how he holds the guns (each hand's aim)
    "vr_gunangle", "vr_gunyaw", "vr_offhandpitch", "vr_offhandyaw", "vr_handcal_off_mirror",
    // grips, holsters, two hands
    "vr_weapon_grip_mode", "vr_weapon_cycle_mode", "vr_holster_mode", "vr_holster_haptics", "vr_2h_handoff",
    "vr_ammo_switch_empty_hand",
    // reloading
    "vr_reload_mode", "vr_reload_mode_shot", "vr_reload_mode_sshot", "vr_reload_mode_nail", "vr_reload_mode_snail",
    "vr_reload_mode_gl", "vr_reload_mode_prox", "vr_reload_mode_rl", "vr_reload_mode_light", "vr_reload_mode_widow",
    "vr_reload_mode_adplasma", "vr_reload_mode_npistol", "vr_reload_mode_revolver",
    // grabbing and grenades
    "vr_forcegrab_mode", "vr_handgrenade_arm",
    // flat-screen play's
    "vr_flat_autoreload", "vr_flat_reload_hold", "vr_flat_grenade_arm_mode", "vr_flat_holster_drop_ahead",
    "vr_flat_holsters", "vr_flat_saw_autostart",
    // the reverse grip (B3f: the server's choice for him at a grab, vr_heldpose.cpp reverseGrip)
    "vr_revgrip",
    // his link: the most the server sends him a second (B12, vr_netsnap.cpp)
    "vr_net_rate"};
constexpr int prefCount = static_cast<int>(sizeof(prefNames) / sizeof(prefNames[0]));
// A preference's place in prefNames by name, at compile time (-1: none).
[[nodiscard]] constexpr int prefIndex(const char* name)
{
    for(int i = 0; i < prefCount; i++)
    {
        const char* a = prefNames[i];
        const char* b = name;
        while(*a && *a == *b)
        {
            a++;
            b++;
        }
        if(*a == *b)
        {
            return i;
        }
    }
    return -1;
}
// vr_net_rate's place: asked for each client every server frame (vr_netsnap.cpp, vr_voicerelay.cpp), so indexOf answers
// it without the search.
constexpr int netRatePref = prefIndex("vr_net_rate");
static_assert(netRatePref >= 0, "vr_net_rate is a per-player preference");
constexpr int prefsCommandVersion = 1; // "vr_prefs <version> <count> <values>": another version's or count's is ignored

// The rig (B3): each hand's fist as this client poses it (held::fist: spheres in the hand's frame, world units), in
// 64ths of a unit, up as "vr_rig <version> <hand> <total> <first> <hex>" (4 shorts a sphere, at most rigChunk spheres
// a command, one command a frame) when a sphere moved more than a step.
constexpr int rigCommandVersion = 2;
constexpr int maxFistSpheres = 128;
constexpr int rigChunk = 24;
constexpr float rigStep = 64.f;

struct Fist
{
    int n{0};
    short v[maxFistSpheres * 4]{};
};

// The palm frame (B3d): each hand's grip::HandFrame as this client measures it (the jointed hand as drawn: the palm, the
// way it faces, the grip channel), up as "vr_rigframe <version> <hand> <values>" when a value moved more than frameStep
// (units; the directions' parts as much), at most every vr_net_avatarcal_interval. The server places what the player
// holds by it (grip::handFrameOf). With it the grip fit's hand rig (B3f: gripfit::PlayerRig, measured with the frame): its
// rig-to-hand matrix and fingers' shifts; zeros: none.
constexpr int rigFrameCommandVersion = 2;
constexpr int frameValueCount = 13; // palm, palm normal, channel point, channel direction (3 each), channel radius
constexpr int handRigValueCount = 12 + 3 * gripfit::rigFingers; // the matrix's columns' x y z, each finger's shift
constexpr int rigFrameValueCount = frameValueCount + handRigValueCount;
constexpr float frameStep = 0.05f;

void handRigValues(const gripfit::PlayerRig& r, float out[handRigValueCount])
{
    for(int c = 0; c < 4; c++)
    {
        for(int k = 0; k < 3; k++)
        {
            out[c * 3 + k] = r.valid ? r.rigToHand[c][k] : 0.f;
        }
    }
    for(int f = 0; f < gripfit::rigFingers; f++)
    {
        for(int k = 0; k < 3; k++)
        {
            out[12 + f * 3 + k] = r.valid ? r.shift[f][k] : 0.f;
        }
    }
}

[[nodiscard]] float handRigSum(const gripfit::PlayerRig& r)
{
    float v[handRigValueCount];
    handRigValues(r, v);
    float sum = 0.f;
    for(const float x : v)
    {
        sum += za::abs(x);
    }
    return sum;
}

void frameValues(const grip::HandFrame& f, float out[frameValueCount])
{
    const glm::vec3 parts[4]{f.palm, f.palmNormal, f.channelPoint, f.channelDir};
    for(int p = 0; p < 4; p++)
    {
        for(int k = 0; k < 3; k++)
        {
            out[p * 3 + k] = parts[p][k];
        }
    }
    out[12] = f.channelRadius;
}

[[nodiscard]] float frameSum(const grip::HandFrame& f)
{
    float v[frameValueCount];
    frameValues(f, v);
    float sum = 0.f;
    for(const float x : v)
    {
        sum += za::abs(x);
    }
    return sum;
}

struct State
{
    const cvar_t* vars[prefCount]{}; // (found by name on first use: cvars and tuning constants)
    bool resolved{false};
    // Client: what went up last.
    float lastSent[prefCount]{};
    bool sent{false};
    double nextSend{0.0};
    Fist fistSent[2];
    bool fistWasSent[2]{};
    double nextRigSend{0.0};
    Fist fistGoing[2];             // (being sent, a chunk a frame)
    int nextChunk[2]{-1, -1};      // (its next sphere; -1 none going)
    float frameSent[2][rigFrameValueCount]{};
    bool frameWasSent[2]{};
    double nextFrameSend{0.0};
    // Server: each slot's fists as they come in (whole: held::setPlayerFist).
    glm::vec4 slotFist[MAX_SCOREBOARD][2][maxFistSpheres]{};
    // Server: each slot's.
    float slot[MAX_SCOREBOARD][prefCount]{};
    bool slotHas[MAX_SCOREBOARD]{};
};
State state;

void resolve()
{
    if(state.resolved)
    {
        return;
    }
    for(int i = 0; i < prefCount; i++)
    {
        const cvar_t* var = Cvar_FindVar(prefNames[i]);
        state.vars[i] = var ? var : tunableFind(prefNames[i]);
    }
    state.resolved = true;
    // The list against the settings' kinds (vr_cvars.inc's QVR_PREF): each setting of it says QVR_PREF (a tuning constant
    // has no row to say it), and none says it that is not in it.
    for(int i = 0; i < prefCount; i++)
    {
        if(state.vars[i] && Cvar_FindVar(prefNames[i]) && cvars::kindOf(state.vars[i]) != cvars::Kind::Pref)
        {
            Con_Warning("vr_clientprefs: %s is a per-player preference, but its vr_cvars.inc row says %s\n", prefNames[i],
                cvars::kindName(cvars::kindOf(state.vars[i])));
        }
    }
    cvars::forEachWithKind([](const cvar_t* var, cvars::Kind kind) {
        bool listed = false;
        for(int i = 0; i < prefCount && !listed; i++)
        {
            listed = state.vars[i] == var;
        }
        if(kind == cvars::Kind::Pref && !listed)
        {
            Con_Warning("vr_clientprefs: %s says QVR_PREF in vr_cvars.inc but is not in the preferences sent (prefNames)\n",
                var->name);
        }
    });
}

[[nodiscard]] float ownValue(int i)
{
    return state.vars[i] ? state.vars[i]->value : 0.f;
}

// The slot of player edict number `num` whose preferences the server has, or -1.
[[nodiscard]] int slotWithPrefs(int num)
{
    const int slot = progs::clientSlotOfNum(num);
    return slot >= 0 && state.slotHas[slot] ? slot : -1;
}

constexpr float prefMost = 1e9f; // a preference's magnitude at most (none is near it)

// "vr_prefs <version> <count> <value>...": a client's preferences (send), kept for his slot.
void prefs_f()
{
    if(cmd_source != src_client || !sv.active || !host_client)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot < 0 || slot >= MAX_SCOREBOARD || Q_atoi(Cmd_Argv(1)) != prefsCommandVersion ||
        Q_atoi(Cmd_Argv(2)) != prefCount || Cmd_Argc() != 3 + prefCount)
    {
        return;
    }
    for(int i = 0; i < prefCount; i++)
    {
        const float v = Q_atof(Cmd_Argv(3 + i));
        // (not finite: 0 -- Q_atof gives infinity for a number of 40 digits; and within what an int holds: the rates
        // are cast to one, vr_netsnap.cpp, vr_voicerelay.cpp)
        state.slot[slot][i] = ZA_ISFINITE(v) ? za::clamp(v, -prefMost, prefMost) : 0.f;
    }
    state.slotHas[slot] = true;
}

[[nodiscard]] int hexDigit(char c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

// "vr_rig <version> <hand> <total> <first> <hex>|-": spheres `first`... of a client's fist of that hand (sendRig); with
// the last, the whole fist kept for his slot (held::setPlayerFist).
void rig_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || Cmd_Argc() != 6 || Q_atoi(Cmd_Argv(1)) != rigCommandVersion)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    const int hand = Q_atoi(Cmd_Argv(2));
    if(slot < 0 || slot >= MAX_SCOREBOARD || (hand != 0 && hand != 1))
    {
        return;
    }
    const int total = Q_atoi(Cmd_Argv(3));
    const int first = Q_atoi(Cmd_Argv(4));
    const char* hex = Cmd_Argv(5);
    const int len = !ZA_STRCMP(hex, "-") ? 0 : static_cast<int>(ZA_STRLEN(hex));
    if(total < 0 || total > maxFistSpheres || first < 0 || len % 16 != 0 || first + len / 16 > total)
    {
        return;
    }
    {
        // Each sphere a hand's size (vr_moveguard.hpp): its centre near the hand's point, its radius a finger's or a
        // palm's; a bigger one is held to it and counted (a fist of 500-unit spheres grabbed and shoved a room).
        const float upm = moveguard::unitsPerMetre(moveguard::slotScale(slot));
        int bounded = 0;
        for(int s = 0; s < len / 16; s++)
        {
            float c[4];
            for(int k = 0; k < 4; k++)
            {
                int u = 0;
                for(int d = 0; d < 4; d++)
                {
                    const int x = hexDigit(hex[s * 16 + k * 4 + d]);
                    if(x < 0)
                    {
                        return;
                    }
                    u = u * 16 + x;
                }
                c[k] = static_cast<float>(static_cast<short>(static_cast<unsigned short>(u))) / rigStep;
            }
            const glm::vec3 centre = moveguard::within(glm::vec3{c[0], c[1], c[2]}, moveguard::fistReachMetres * upm, bounded);
            const float radius = za::min(za::max(c[3], 0.f), moveguard::fistRadiusMetres * upm);
            bounded += radius < c[3] ? 1 : 0;
            state.slotFist[slot][hand][first + s] = glm::vec4{centre, radius};
        }
        moveguard::countFist(slot, bounded);
    }
    if(first + len / 16 == total)
    {
        za::Vector<glm::vec4> spheres;
        for(int s = 0; s < total; s++)
        {
            spheres.pushBack(state.slotFist[slot][hand][s]);
        }
        held::setPlayerFist(slot, hand, spheres);
    }
}

// "vr_rigframe <version> <hand> <40 values>": a client's measured frame of that hand and its rig (sendFrame), kept for his
// slot (grip::setPlayerFrame, gripfit::setPlayerRig). Values out of reach (not numbers, farther than a hand) leave them as
// they were.
void rigFrame_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || Cmd_Argc() != 3 + rigFrameValueCount ||
        Q_atoi(Cmd_Argv(1)) != rigFrameCommandVersion)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    const int hand = Q_atoi(Cmd_Argv(2));
    if(slot < 0 || slot >= MAX_SCOREBOARD || (hand != 0 && hand != 1))
    {
        return;
    }
    float v[rigFrameValueCount];
    for(int i = 0; i < rigFrameValueCount; i++)
    {
        v[i] = Q_atof(Cmd_Argv(3 + i));
        if(!(za::abs(v[i]) <= (i < frameValueCount ? 32.f : 64.f)))
        {
            return;
        }
    }
    gripfit::PlayerRig rig;
    const float* r = v + frameValueCount;
    for(int c = 0; c < 4; c++)
    {
        rig.rigToHand[c] = glm::vec4{r[c * 3], r[c * 3 + 1], r[c * 3 + 2], c == 3 ? 1.f : 0.f};
    }
    for(int f = 0; f < gripfit::rigFingers; f++)
    {
        rig.shift[f] = {r[12 + f * 3], r[12 + f * 3 + 1], r[12 + f * 3 + 2]};
    }
    // (a matrix with no turn in it: none measured)
    rig.valid = glm::length(glm::vec3{rig.rigToHand[0]}) > 1e-4f && glm::length(glm::vec3{rig.rigToHand[1]}) > 1e-4f &&
                glm::length(glm::vec3{rig.rigToHand[2]}) > 1e-4f;
    gripfit::setPlayerRig(slot, hand, rig.valid ? rig : gripfit::PlayerRig{});
    grip::HandFrame f;
    f.measured = true;
    f.palm = {v[0], v[1], v[2]};
    f.palmNormal = {v[3], v[4], v[5]};
    f.channelPoint = {v[6], v[7], v[8]};
    f.channelDir = {v[9], v[10], v[11]};
    f.channelRadius = za::max(v[12], 0.f);
    if(glm::length(f.palmNormal) < 0.5f || glm::length(f.channelDir) < 0.5f)
    {
        return;
    }
    f.palmNormal = glm::normalize(f.palmNormal);
    f.channelDir = glm::normalize(f.channelDir);
    grip::setPlayerFrame(slot, hand, f);
}

// One hand's measured frame up a frame at most, when it moved, at most every vr_net_avatarcal_interval; only with room
// in the reliable buffer (else the next frame).
void sendFrame()
{
    if(realtime < state.nextFrameSend)
    {
        return;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        const grip::HandFrame f = grip::handFrame(hand, hand == 0, true);
        if(!f.measured)
        {
            continue;
        }
        float v[rigFrameValueCount];
        frameValues(f, v);
        handRigValues(gripfit::ownRig(hand), v + frameValueCount);
        bool moved = !state.frameWasSent[hand];
        for(int i = 0; i < rigFrameValueCount && !moved; i++)
        {
            moved = za::abs(v[i] - state.frameSent[hand][i]) > frameStep;
        }
        if(!moved)
        {
            continue;
        }
        char text[640];
        int n = q_snprintf(text, sizeof(text), "vr_rigframe %d %d", rigFrameCommandVersion, hand);
        for(int i = 0; i < rigFrameValueCount && n > 0 && n < static_cast<int>(sizeof(text)); i++)
        {
            n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), za::abs(v[i]) < 5e-4f ? " 0" : " %.3f", v[i]);
        }
        if(n <= 0 || n >= static_cast<int>(sizeof(text)) || !qvr::client::roomForCommand(n))
        {
            return; // (no room: the next frame)
        }
        MSG_WriteByte(&cls.message, clc_stringcmd);
        MSG_WriteString(&cls.message, text);
        ZA_MEMCPY(state.frameSent[hand], v, sizeof(v));
        state.frameWasSent[hand] = true;
        state.nextFrameSend = realtime + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
        return;
    }
}

[[nodiscard]] Fist quantized(const za::Vector<glm::vec4>& spheres)
{
    Fist q;
    for(const glm::vec4& s : spheres)
    {
        if(q.n >= maxFistSpheres)
        {
            break;
        }
        for(int k = 0; k < 4; k++)
        {
            q.v[q.n * 4 + k] = static_cast<short>(za::clamp(lroundf(s[k] * rigStep), -32767L, 32767L));
        }
        q.n++;
    }
    return q;
}

// Off by more than a step anywhere (a hand's settings changed), or another number of spheres.
[[nodiscard]] bool moved(const Fist& a, const Fist& b)
{
    if(a.n != b.n)
    {
        return true;
    }
    for(int i = 0; i < a.n * 4; i++)
    {
        if(za::abs(static_cast<int>(a.v[i]) - static_cast<int>(b.v[i])) > 1)
        {
            return true;
        }
    }
    return false;
}

// The sum of a fist's coordinates and radii (vr_mp_prefs: the server's and the client's compared).
[[nodiscard]] float fistSum(const za::Vector<glm::vec4>& spheres)
{
    float sum = 0.f;
    for(const glm::vec4& s : spheres)
    {
        sum += za::abs(s.x) + za::abs(s.y) + za::abs(s.z) + s.w;
    }
    return sum;
}

// One hand's fist up a frame at most, when it moved, at most every vr_net_avatarcal_interval; only with room in the
// reliable buffer (else the next frame).
void sendRig()
{
    if(realtime < state.nextRigSend)
    {
        return;
    }
    for(int hand = 0; hand < 2; hand++)
    {
        if(state.nextChunk[hand] <= 0)
        {
            const Fist now = quantized(held::fist(hand));
            if(state.fistWasSent[hand] && !moved(now, state.fistSent[hand]))
            {
                continue;
            }
            state.fistGoing[hand] = now;
            state.nextChunk[hand] = 0;
        }
        const Fist& now = state.fistGoing[hand];
        const int first = za::max(state.nextChunk[hand], 0);
        const int last = za::min(first + rigChunk, now.n);
        char text[64 + rigChunk * 16];
        int n = q_snprintf(text, sizeof(text), "vr_rig %d %d %d %d ", rigCommandVersion, hand, now.n, first);
        if(last <= first)
        {
            text[n++] = '-';
        }
        for(int i = first * 4; i < last * 4; i++)
        {
            n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), "%04x", static_cast<unsigned short>(now.v[i]));
        }
        text[n] = 0;
        if(!qvr::client::roomForCommand(n))
        {
            return; // (no room: the next frame)
        }
        MSG_WriteByte(&cls.message, clc_stringcmd);
        MSG_WriteString(&cls.message, text);
        state.nextChunk[hand] = last;
        if(last >= now.n)
        {
            state.fistSent[hand] = now;
            state.fistWasSent[hand] = true;
            state.nextChunk[hand] = -1;
            state.nextRigSend = realtime + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
        }
        return;
    }
}

// vr_mp_prefs: this client's preferences (the ones off the defaults: "mp_prefs_own:") and, hosting, each client's as the
// server has them (those that differ from the server's own: "mp_prefs_sv:").
void mpPrefs_f()
{
    resolve();
    if(cls.state == ca_connected && cl.viewentity > 0)
    {
        Con_Printf("mp_prefs_own: ent=%d sent=%d fist0=%d fist1=%d fistsum0=%.3f fistsum1=%.3f", cl.viewentity,
            state.sent ? 1 : 0, static_cast<int>(held::fist(0).size()), static_cast<int>(held::fist(1).size()),
            fistSum(held::fist(0)), fistSum(held::fist(1)));
        for(int h = 0; h < 2; h++)
        {
            const grip::HandFrame f = grip::handFrame(h, h == 0, true);
            const gripfit::PlayerRig r = gripfit::ownRig(h);
            Con_Printf(" frame%d=%d framesum%d=%.3f rig%d=%d rigsum%d=%.3f", h, f.measured ? 1 : 0, h, frameSum(f), h,
                r.valid ? 1 : 0, h, handRigSum(r));
        }
        for(int i = 0; i < prefCount; i++)
        {
            const cvar_t* var = state.vars[i];
            if(var && var->default_string && ZA_STRCMP(var->string, var->default_string) != 0)
            {
                Con_Printf(" %s=%g", prefNames[i], var->value);
            }
        }
        Con_Printf("\n");
    }
    if(sv.active)
    {
        for(int slot = 0; slot < svs.maxclients && slot < MAX_SCOREBOARD; slot++)
        {
            if(!svs.clients[slot].active)
            {
                continue;
            }
            const edict_t* player = (const edict_t*)((const byte*)sv.qcvm.edicts + (slot + 1) * sv.qcvm.edict_size);
            Con_Printf("mp_prefs_sv: ent=%d has=%d fist0=%d fist1=%d fistsum0=%.3f fistsum1=%.3f fistgrabs=%d", slot + 1,
                state.slotHas[slot] ? 1 : 0, static_cast<int>(held::fistOf(player, 0).size()),
                static_cast<int>(held::fistOf(player, 1).size()), fistSum(held::fistOf(player, 0)),
                fistSum(held::fistOf(player, 1)), held::playerFistGrabs(slot));
            for(int h = 0; h < 2; h++)
            {
                const grip::HandFrame f = grip::handFrameOf(player, h, h == 0);
                const gripfit::PlayerRig r = gripfit::playerRig(slot, h);
                Con_Printf(" frame%d=%d framesum%d=%.3f rig%d=%d rigsum%d=%.3f", h, f.measured ? 1 : 0, h, frameSum(f), h,
                    r.valid ? 1 : 0, h, handRigSum(r));
            }
            Con_Printf(" rigpts=%d", gripfit::rigPointCount());
            int same = 0;
            for(int i = 0; i < prefCount && state.slotHas[slot]; i++)
            {
                if(state.slot[slot][i] != ownValue(i))
                {
                    Con_Printf(" %s=%g", prefNames[i], state.slot[slot][i]);
                }
                else
                {
                    same++;
                }
            }
            Con_Printf(" same=%d\n", same);
        }
    }
}

// "vr_mp_prefs_qc <self> <other> <cvar>": the server's QC read of a preference (cvar_hget: forQc) with self and other
// those entity numbers ("mp_prefs_qc:", the harness's mp_prefs: a player without preferences -- a bot, a slot whose
// have not come -- reads the server's, never other's).
void prefsQc_f()
{
    if(Cmd_Argc() != 4 || !sv.active)
    {
        Con_Printf("vr_mp_prefs_qc <self> <other> <cvar>: cvar_hget's value of a per-player preference with those self, other\n");
        return;
    }
    const cvar_t* var = Cvar_FindVar(Cmd_Argv(3));
    const int pref = indexOf(var);
    const int self = Q_atoi(Cmd_Argv(1)), other = Q_atoi(Cmd_Argv(2));
    if(pref < 0 || self < 0 || other < 0 || self >= sv.qcvm.num_edicts || other >= sv.qcvm.num_edicts)
    {
        Con_Printf("vr_mp_prefs_qc: %s is no per-player preference, or no such entity\n", Cmd_Argv(3));
        return;
    }
    const progs::ServerVm vm;
    const int oldSelf = pr_global_struct->self, oldOther = pr_global_struct->other;
    pr_global_struct->self = self * qcvm->edict_size;
    pr_global_struct->other = other * qcvm->edict_size;
    const float v = forQc(pref, var);
    pr_global_struct->self = oldSelf;
    pr_global_struct->other = oldOther;
    Con_Printf("mp_prefs_qc: self=%d other=%d name=%s value=%g server=%g\n", self, other, var->name, v, var->value);
}

} // namespace

int indexOf(const cvar_t* var)
{
    if(var == &vr_net_rate)
    {
        return netRatePref; // (the per-frame one: no search)
    }
    resolve();
    for(int i = 0; i < prefCount; i++)
    {
        if(var && state.vars[i] == var)
        {
            return i;
        }
    }
    return -1;
}

float value(int pref, const cvar_t* var, const edict_t* player)
{
    if(pref >= 0 && pref < prefCount && player && sv.active && qcvm == &sv.qcvm) // (a slot without them: the own)
    {
        const int slot = progs::clientSlot(player);
        if(slot >= 0 && state.slotHas[slot])
        {
            return state.slot[slot][pref];
        }
    }
    return var ? var->value : 0.f;
}

float forQc(int pref, const cvar_t* var)
{
    if(pref >= 0 && pref < prefCount && sv.active && qcvm == &sv.qcvm)
    {
        for(const int e : {pr_global_struct->self, pr_global_struct->other})
        {
            const int num = e / qcvm->edict_size;
            const int slot = slotWithPrefs(num);
            if(slot >= 0)
            {
                return state.slot[slot][pref];
            }
            if(progs::clientSlotOfNum(num) >= 0)
            {
                break; // (self a player -- a bot, one whose preferences have not come: the server's, never other's)
            }
        }
    }
    return var ? var->value : 0.f;
}

void send()
{
    if(cls.state != ca_connected || cls.signon != SIGNONS || cls.demoplayback || cl.maxclients <= 1)
    {
        return;
    }
    resolve();
    float now[prefCount];
    bool changed = !state.sent;
    for(int i = 0; i < prefCount; i++)
    {
        now[i] = ownValue(i);
        changed = changed || now[i] != state.lastSent[i];
    }
    if(ownSlot() < 0) // (hosting: its own hands are in its slot already, publishOwn)
    {
        sendRig();
        sendFrame();
    }
    heldpose::send(); // (the held weapon's drawn pose: vr_heldpose.cpp)
    if(!changed || (state.sent && realtime < state.nextSend))
    {
        return;
    }
    char text[1024];
    int n = q_snprintf(text, sizeof(text), "vr_prefs %d %d", prefsCommandVersion, prefCount);
    for(int i = 0; i < prefCount && n > 0 && n < static_cast<int>(sizeof(text)); i++)
    {
        n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), " %g", now[i]);
    }
    if(!qvr::client::roomForCommand(n))
    {
        return; // (no room: the next frame)
    }
    MSG_WriteByte(&cls.message, clc_stringcmd);
    MSG_WriteString(&cls.message, text);
    ZA_MEMCPY(state.lastSent, now, sizeof(now));
    state.sent = true;
    state.nextSend = realtime + static_cast<double>(za::max(vr_net_avatarcal_interval.value, 0.f));
}

void clientClear()
{
    state.sent = false;
    state.fistWasSent[0] = state.fistWasSent[1] = false;
    state.nextChunk[0] = state.nextChunk[1] = -1;
    state.frameWasSent[0] = state.frameWasSent[1] = false;
    heldpose::clientClear();
}

namespace
{
// Forgotten when the client's state is cleared (vr_mem.hpp's resets): ours go up to the new server again.
mem::Reset clientStateReset{"clientprefs", mem::ResetEvent::ClientState, mem::ResetPhase::State, &clientClear};
} // namespace

void serverReset(int slot)
{
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        state.slotHas[slot] = false;
        held::setPlayerFist(slot, 0, {});
        held::setPlayerFist(slot, 1, {});
        grip::setPlayerFrame(slot, 0, {});
        grip::setPlayerFrame(slot, 1, {});
        gripfit::setPlayerRig(slot, 0, {});
        gripfit::setPlayerRig(slot, 1, {});
        heldpose::serverReset(slot);
        if(slot == ownSlot())
        {
            publishOwn(0);
            publishOwn(1);
        }
    }
}

int ownSlot()
{
    return progs::hostSlot(); // (the loopback client's slot: 0, VR_CoopPickSlot; -1 on a dedicated server)
}

void publishOwn(int hand)
{
    const int slot = ownSlot();
    if(slot >= 0 && (hand == 0 || hand == 1))
    {
        held::setPlayerFist(slot, hand, held::fist(hand));
        grip::setPlayerFrame(slot, hand, grip::ownFrame(hand));
        gripfit::setPlayerRig(slot, hand, gripfit::ownRig(hand));
    }
}

void init()
{
    Cmd_AddCommand("vr_mp_prefs", mpPrefs_f);
    Cmd_AddCommand("vr_mp_prefs_qc", prefsQc_f);
    Cmd_AddCommand_ClientCommand("vr_prefs", prefs_f);
    Cmd_AddCommand_ClientCommand("vr_rig", rig_f);
    Cmd_AddCommand_ClientCommand("vr_rigframe", rigFrame_f);
    heldpose::init(); // (vr_mp_drawn, the "vr_heldpose" client command)
}

} // namespace qvr::clientprefs
