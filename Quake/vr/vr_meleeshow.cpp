// vr_meleeshow.cpp -- Show Melee Blades: see vr_meleeshow.hpp.

#include "vr_meleeshow.hpp"

#include "vr_engine.hpp"
#include "vr_api.h"
#include "vr_client.hpp"
#include "vr_cvars.hpp"
#include "vr_lines.hpp"
#include "vr_protocol.hpp"
#include "vr_text3d.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/MinMax.hpp"

using namespace qvr;

namespace
{

constexpr int frameMost = 24;      // a hand's sweeps kept (a third of a second at 72 Hz)
constexpr int ribMost = 48;        // a sweep's lines (QC VR_MELEE_RIBS)
constexpr int hitMost = 8;         // a hand's blows landed kept
constexpr float sweepFade = 0.3f;  // s: a sweep's lines fade out over this
constexpr float lineKeep = 0.15f;  // s: the blow line stays drawn this long after the hand's last sweep
constexpr float hitRed = 0.4f;     // s: the blow line red this long after a blow landed
constexpr float hitKeep = 1.5f;    // s: a blow's dot and damage stay this long
constexpr float joinGap = 0.1f;    // s: two sweeps this far apart at most are joined by the blade's surface

struct ShowFrame
{
    float time = -1.f;
    bool fast = false; // the hand fast enough to strike
    bool hit = false;  // a blow landed on this sweep
    bool head = false; // a hammer's or mace's head
    glm::vec3 nearEnd{0.f};
    glm::vec3 strikeFrom{0.f}; // its first striking point (where its blade starts; zero: not sent)
    glm::vec3 grip{0.f};
    glm::vec3 tip{0.f};
    glm::vec3 headLo{0.f};
    glm::vec3 headHi{0.f};
    glm::vec3 headGrip{0.f};
    glm::vec3 headFar{0.f};
    glm::vec3 headY{0.f};
    int ribs = 0;
    glm::vec3 ribFrom[ribMost];
    glm::vec3 ribTo[ribMost];
};

struct ShowHit
{
    float time = -1.f;
    glm::vec3 at{0.f};
    float damage = -1.f;
};

struct ShowHand
{
    ShowFrame frames[frameMost];
    int last = -1; // the newest frame
    ShowHit hits[hitMost];
    int lastHit = -1;
};

// The two hands' (main 0, off 1), as QC last sent them.
ShowHand meleeShows[2];

// Show Melee Arcs: the armed humanoids' swinging weapons (addFoe), a slot each by its edict number; a new one takes the
// slot swung longest ago.
constexpr int foeMost = 8;
struct FoeShow
{
    int ent = 0; // its weapon's edict number (0: free)
    ShowHand h;
};
struct FoeShows
{
    FoeShow slots[foeMost];
    int drawnFrames = 0;   // the frames draw drew any (foeDrawnFrames)
    int drawnSegments = 0; // ... the trail's segments (foeDrawnSegments)
};
FoeShows foeShows;

// Multiplayer (mparcs): the server's addFoe records of this frame, for the remote clients drawing them (VR_WriteMeleeArcs),
// and the slots whose player draws them ("vr_meleearcs_want 1").
struct FoeSend
{
    static constexpr int most = 32; // a frame's records (8 humanoids' two blades' grip and butt)
    struct Rec
    {
        int ent = 0;
        int what = 0;
        bool fast = false;
        glm::vec3 a{0.f};
        glm::vec3 b{0.f};
    };
    Rec recs[most];
    int count = 0; // this server frame's (serverFrameBegin empties them)
    bool wanted[MAX_SCOREBOARD]{};
};
FoeSend foeSend;
bool foeAsked = false; // the client: "vr_meleearcs_want 1" sent this connection

// The time the shows are stamped and drawn by: the server's on the host; a remote client's, the server time of the last
// message read (the records came with it: none in its future).
[[nodiscard]] float showNow()
{
    return static_cast<float>(sv.active ? sv.qcvm.time : cl.mtime[0]);
}

[[nodiscard]] glm::vec4 faded(const glm::vec4& c, float k)
{
    return glm::vec4{c.r, c.g, c.b, c.a * k};
}

// The head's box as the sweep has it (QC VR_Melee_Ribs): its long sides along the line, its width across it in the
// blade's plane, its depth along the model's +y.
void headBox(const ShowFrame& f, const glm::vec4& colour)
{
    const glm::vec3 a = f.headFar - f.headGrip;
    const float len = glm::length(a);
    if(len < 1.f || glm::length(f.headY) < 0.01f)
    {
        return;
    }
    const glm::vec3 y = glm::normalize(f.headY);
    const glm::vec3 n = glm::normalize(glm::cross(y, a)) * len;
    glm::vec3 c[8];
    for(int i = 0; i < 8; ++i)
    {
        const float sx = (i & 1) ? f.headHi.x : f.headLo.x;
        const float sy = (i & 2) ? f.headHi.y : f.headLo.y;
        const float sz = (i & 4) ? f.headHi.z : f.headLo.z;
        c[i] = f.headGrip + a * sx + n * sy + y * (sz * len);
    }
    for(int i = 0; i < 8; ++i)
    {
        for(int bit = 1; bit < 8; bit <<= 1)
        {
            if(!(i & bit))
            {
                lines::line(c[i], c[i | bit], 0.35f, colour, colour);
            }
        }
    }
}

void addTo(ShowHand& h, int what, const glm::vec3& a, const glm::vec3& b, float v)
{
    const float now = showNow();
    if(what == 0)
    {
        h.last = (h.last + 1) % frameMost;
        ShowFrame& f = h.frames[h.last];
        f.time = now;
        f.fast = v > 0.f;
        f.hit = false;
        f.head = false;
        f.grip = a;
        f.tip = b;
        f.nearEnd = a;
        f.strikeFrom = glm::vec3{0.f};
        f.ribs = 0;
        return;
    }
    if(what == 6)
    {
        ShowHit* hit = h.lastHit >= 0 && h.hits[h.lastHit].time == now ? &h.hits[h.lastHit] : nullptr;
        if(!hit)
        {
            h.lastHit = (h.lastHit + 1) % hitMost;
            hit = &h.hits[h.lastHit];
            hit->time = now;
            hit->damage = -1.f;
            hit->at = a;
        }
        if(v >= 0.f)
        {
            hit->damage = v; // (the damage comes first, from T_Damage_VRMelee; the contact after it)
        }
        else
        {
            hit->at = a;
        }
        if(h.last >= 0 && h.frames[h.last].time == now)
        {
            h.frames[h.last].hit = true;
        }
        return;
    }
    if(h.last < 0 || h.frames[h.last].time != now)
    {
        return; // (no frame begun this server frame)
    }
    ShowFrame& f = h.frames[h.last];
    if(what == 1)
    {
        f.nearEnd = a;
        f.strikeFrom = b;
    }
    else if(what == 2 && f.ribs < ribMost)
    {
        f.ribFrom[f.ribs] = a;
        f.ribTo[f.ribs] = b;
        ++f.ribs;
    }
    else if(what == 3)
    {
        f.head = true;
        f.headLo = a;
        f.headHi = b;
    }
    else if(what == 4)
    {
        f.headGrip = a;
        f.headFar = b;
    }
    else if(what == 5)
    {
        f.headY = a;
    }
}

} // namespace

void qvr::meleeshow::add(int hand, int what, const glm::vec3& a, const glm::vec3& b, float v)
{
    if(vr_debug_melee_blades.value == 0.f || hand < 0 || hand > 1)
    {
        return;
    }
    addTo(meleeShows[hand], what, a, b, v);
}

namespace
{

// An armed humanoid's swing kept for draw (addFoe on the host; parseFoes on a remote client): while the option is on.
void keepFoe(int ent, int what, const glm::vec3& a, const glm::vec3& b, float v)
{
    if(vr_enemy_melee_arc_show.value == 0.f || ent <= 0)
    {
        return;
    }
    FoeShow* slot = nullptr;
    for(FoeShow& s : foeShows.slots)
    {
        if(s.ent == ent)
        {
            slot = &s;
            break;
        }
    }
    if(!slot)
    {
        if(what != 0)
        {
            return; // (no frame begun for it)
        }
        // (a free slot, else the one swung longest ago: its frames cleared)
        const auto newest = [](const FoeShow& s)
        { return s.ent == 0 || s.h.last < 0 ? -1.f : s.h.frames[s.h.last].time; };
        slot = &foeShows.slots[0];
        for(FoeShow& s : foeShows.slots)
        {
            if(newest(s) < newest(*slot))
            {
                slot = &s;
            }
        }
        slot->ent = ent;
        slot->h.last = -1;
        slot->h.lastHit = -1;
        for(ShowFrame& f : slot->h.frames)
        {
            f.time = -1.f;
        }
        for(ShowHit& hit : slot->h.hits)
        {
            hit.time = -1.f;
        }
    }
    addTo(slot->h, what, a, b, v);
}

// "vr_meleearcs_want <0 | 1>": a remote client draws the armed humanoids' swings (his vr_enemy_melee_arc_show), on the server.
void foesWant_f()
{
    if(cmd_source != src_client || !sv.active || !host_client || Cmd_Argc() != 2)
    {
        return;
    }
    const int slot = static_cast<int>(host_client - svs.clients);
    if(slot >= 0 && slot < MAX_SCOREBOARD)
    {
        foeSend.wanted[slot] = atoi(Cmd_Argv(1)) != 0;
    }
}

// The client: the swings asked of the server while the option is on (a remote client's: a listen host draws its own), and
// forgotten when off or on a new connection.
void askFoes()
{
    const bool connected = cls.state == ca_connected && cls.signon == SIGNONS && !cls.demoplayback;
    const bool want = connected && !sv.active && vr_enemy_melee_arc_show.value != 0.f;
    if(want != foeAsked)
    {
        if(connected)
        {
            qvr::client::forwardCommand(va("vr_meleearcs_want %d", want ? 1 : 0)); // (mpoverflow: held while full)
        }
        foeAsked = want;
    }
}

} // namespace

void qvr::meleeshow::addFoe(int ent, int what, const glm::vec3& a, const glm::vec3& b, float v)
{
    if(ent > 0 && sv.active && remoteWanted())
    {
        // (this frame's records for the remote clients drawing them: VR_WriteMeleeArcs)
        if(foeSend.count < FoeSend::most && (what == 0 || what == 1 || what == 6))
        {
            FoeSend::Rec& r = foeSend.recs[foeSend.count++];
            r.ent = ent;
            r.what = what;
            r.fast = what == 0 && v > 0.f;
            r.a = a;
            r.b = b;
        }
    }
    keepFoe(ent, what, a, b, v);
}

void qvr::meleeshow::serverFrameBegin()
{
    foeSend.count = 0;
}

void qvr::meleeshow::init()
{
    Cmd_AddCommand_ClientCommand("vr_meleearcs_want", foesWant_f);
}

bool qvr::meleeshow::remoteWanted()
{
    if(!sv.active)
    {
        return false;
    }
    bool any = false;
    for(int s = 0; s < MAX_SCOREBOARD; ++s)
    {
        if(foeSend.wanted[s] && (s >= svs.maxclients || !svs.clients[s].active))
        {
            foeSend.wanted[s] = false; // (gone: a new player in the slot asks for himself)
        }
        any = any || foeSend.wanted[s];
    }
    return any;
}

// SV_SendClientDatagram: this frame's swings to a remote client drawing them, as many records as fit (unreliable: a frame
// it gets no datagram for, or a lost one, is a gap in the trail).
extern "C" void VR_WriteMeleeArcs(sizebuf_t* msg, client_s* client)
{
    const int slot = static_cast<int>(client - svs.clients);
    if(foeSend.count == 0 || !(sv.protocolflags & PRFL_QUAKEVR) || slot < 0 ||
        slot >= MAX_SCOREBOARD || !foeSend.wanted[slot] || SV_IsLocalClient(client))
    {
        return;
    }
    constexpr int recMost = 2 + 1 + 6 * 4; // a record's bytes at most (coordinates of 4 bytes)
    if(msg->maxsize - msg->cursize < 3 + recMost)
    {
        return;
    }
    MSG_WriteByte(msg, protocol::svc_quakevr);
    MSG_WriteByte(msg, protocol::QVR_SVC_MELEEARC);
    const int countAt = msg->cursize;
    MSG_WriteByte(msg, 0);
    int written = 0;
    for(int i = 0; i < foeSend.count && msg->maxsize - msg->cursize >= recMost; ++i)
    {
        const FoeSend::Rec& r = foeSend.recs[i];
        MSG_WriteShort(msg, r.ent);
        MSG_WriteByte(msg, r.what | (r.fast ? 128 : 0));
        for(int k = 0; k < 3; ++k)
        {
            MSG_WriteCoord(msg, r.a[k], sv.protocolflags);
        }
        if(r.what == 0)
        {
            for(int k = 0; k < 3; ++k)
            {
                MSG_WriteCoord(msg, r.b[k], sv.protocolflags);
            }
        }
        ++written;
    }
    msg->data[countAt] = static_cast<byte>(written);
}

void qvr::meleeshow::parseFoes()
{
    const int count = MSG_ReadByte();
    for(int i = 0; i < count; ++i)
    {
        const int ent = MSG_ReadShort();
        const int bits = MSG_ReadByte();
        const int what = bits & 127;
        glm::vec3 a{0.f};
        glm::vec3 b{0.f};
        for(int k = 0; k < 3; ++k)
        {
            a[k] = MSG_ReadCoord(cl.protocolflags);
        }
        if(what == 0)
        {
            for(int k = 0; k < 3; ++k)
            {
                b[k] = MSG_ReadCoord(cl.protocolflags);
            }
        }
        if(!sv.active) // (a listen host keeps the server's own: addFoe)
        {
            keepFoe(ent, what, a, b, what == 6 ? -1.f : ((bits & 128) ? 1.f : 0.f));
        }
    }
}

int qvr::meleeshow::foeDrawnFrames()
{
    return foeShows.drawnFrames;
}

int qvr::meleeshow::foeDrawnSegments()
{
    return foeShows.drawnSegments;
}

bool qvr::meleeshow::lastLine(int hand, glm::vec3& nearEnd, glm::vec3& grip, glm::vec3& tip)
{
    if(hand < 0 || hand > 1 || meleeShows[hand].last < 0)
    {
        return false;
    }
    const ShowFrame& f = meleeShows[hand].frames[meleeShows[hand].last];
    if(f.time < 0.f || showNow() - f.time > 0.5f)
    {
        return false;
    }
    nearEnd = f.nearEnd;
    grip = f.grip;
    tip = f.tip;
    return true;
}

bool qvr::meleeshow::lastStrikeFrom(int hand, glm::vec3& at)
{
    glm::vec3 n, g, t;
    if(!lastLine(hand, n, g, t))
    {
        return false;
    }
    at = meleeShows[hand].frames[meleeShows[hand].last].strikeFrom;
    return at != glm::vec3{0.f};
}

namespace
{

// One hand's (or an armed humanoid's weapon's) last frames; TRUE if it drew any. `segments` (not null) counts the swept
// surface's segments drawn.
bool drawHand(const ShowHand& h, const glm::vec3& eye, float now, int* segments = nullptr)
{
    const glm::vec4 white{1.f, 1.f, 1.f, 0.85f};
    const glm::vec4 yellow{1.f, 0.9f, 0.1f, 0.95f};
    const glm::vec4 red{1.f, 0.15f, 0.15f, 0.95f};
    bool drew = false;
    if(h.last < 0)
    {
        return false;
    }
    float lastHitTime = -1.f;
    for(const ShowHit& hit : h.hits)
    {
        lastHitTime = za::max(lastHitTime, hit.time);
    }
    // The sweeps, newest first, fading out: the lines the sweep tested, and the blade's surface between two poses.
    for(int k = 0; k < frameMost; ++k)
    {
        const ShowFrame& f = h.frames[(h.last - k + frameMost) % frameMost];
        const float age = now - f.time;
        if(f.time < 0.f || age < 0.f || age > sweepFade)
        {
            break;
        }
        drew = true;
        const float fade = 1.f - age / sweepFade;
        const glm::vec4 c = f.hit ? red : (f.fast ? yellow : white);
        for(int i = 0; i < f.ribs; ++i)
        {
            lines::line(f.ribFrom[i], f.ribTo[i], 0.2f, faded(c, fade * 0.8f), faded(c, fade * 0.8f));
        }
        lines::line(f.grip, f.tip, 0.25f, faded(c, fade * 0.6f), faded(c, fade * 0.6f));
        const ShowFrame& o = h.frames[(h.last - k - 1 + frameMost) % frameMost];
        if(k + 1 < frameMost && o.time >= 0.f && f.time - o.time > 0.f && f.time - o.time <= joinGap)
        {
            const glm::vec4 s = faded(c, fade * 0.22f);
            lines::triangle(o.grip, o.tip, f.tip, s, true);
            lines::triangle(o.grip, f.tip, f.grip, s, true);
            if(segments)
            {
                ++*segments;
            }
        }
    }
    // The blow line now: the hand to the tip bold, the near end behind the hand thinner; the head's box.
    const ShowFrame& f = h.frames[h.last];
    if(f.time >= 0.f && now >= f.time && now - f.time <= lineKeep)
    {
        drew = true;
        const glm::vec4 c = (lastHitTime >= 0.f && now - lastHitTime <= hitRed) ? red : (f.fast ? yellow : white);
        lines::line(f.grip, f.tip, 0.6f, c, c);
        lines::line(f.nearEnd, f.grip, 0.25f, faded(c, 0.4f), faded(c, 0.4f));
        lines::point(f.grip, 1.2f, c);
        lines::point(f.tip, 1.2f, c);
        if(f.head)
        {
            headBox(f, c);
        }
    }
    // The blows landed: a red dot where each struck, its damage above it.
    for(const ShowHit& hit : h.hits)
    {
        const float age = now - hit.time;
        if(hit.time < 0.f || age < 0.f || age > hitKeep)
        {
            continue;
        }
        drew = true;
        const float fade = 1.f - age / hitKeep;
        // (A white cross round the red dot: it stands out of the blood the blow sprays there.)
        const glm::vec4 w = faded(white, za::max(0.3f, fade));
        for(int axis = 0; axis < 3; ++axis)
        {
            glm::vec3 d{0.f};
            d[axis] = 3.f;
            lines::line(hit.at - d, hit.at + d, 0.3f, w, w);
        }
        lines::point(hit.at, 2.f, faded(red, za::max(0.3f, fade)));
        if(hit.damage >= 0.f)
        {
            char text[16];
            q_snprintf(text, sizeof(text), "%.1f", static_cast<double>(hit.damage));
            const glm::vec3 at = hit.at + glm::vec3{0.f, 0.f, 6.f};
            const glm::vec3 toText = at - eye;
            text3d::queueOverlay(text, at, glm::vec3{0.f, glm::degrees(za::atan2(toText.y, toText.x)), 0.f}, 0.1f);
        }
    }
    return drew;
}

} // namespace

void qvr::meleeshow::draw(const glm::vec3& eye)
{
    askFoes();
    // (the hands' come from this game's own server; the armed humanoids' on a remote client too: parseFoes)
    const bool blades = vr_debug_melee_blades.value != 0.f && sv.active;
    const bool foes = vr_enemy_melee_arc_show.value != 0.f && (sv.active || cls.state == ca_connected);
    if(!blades && !foes)
    {
        return;
    }
    const float now = showNow();
    if(blades)
    {
        for(const ShowHand& h : meleeShows)
        {
            drawHand(h, eye, now);
        }
    }
    if(foes)
    {
        // (Show Melee Arcs: the armed humanoids' swings, as the hands' are drawn)
        bool any = false;
        for(const FoeShow& s : foeShows.slots)
        {
            if(s.ent > 0 && drawHand(s.h, eye, now, &foeShows.drawnSegments))
            {
                any = true;
            }
        }
        foeShows.drawnFrames += any ? 1 : 0;
    }
}
