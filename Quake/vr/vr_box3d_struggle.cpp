// vr_box3d_struggle.cpp -- the box3d side of a living knockdown's struggle in a player's hands (vr_struggle;
// vr_ragstruggle.cpp; docs/vr-port/STRUGGLE.md): whether it may struggle and what holds it (ragactive::StruggleInput),
// its class's profile (vr_struggle_classes), its drive begun; the overlay (vr_struggle_debug) and vr_struggle_status.

#include "vr_box3d_internal.hpp"

#include "vr_ragtool.hpp"
#include "vr_text3d.hpp"

#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace box3d::detail
{

namespace
{

// A class that struggles (vr_struggle_classes: classname[:strength[:speed[:health[:arms]]]]). `arms`: its arms' springs
// this many times stiffer while it struggles (1..2.5): an enforcer's forearms (his gun hands, 5 kg) hang on 1-2 kg upper
// arms and a zombie's whole arm is 2.4 kg, and a Box3D joint's spring acts on its two bodies' own inertia: their hands
// lagged their goals (STRUGGLE.md, "Classes").
struct StruggleClass
{
    char classname[40]{};
    float strength{1.f}, speed{1.f}, health{0.f}, arms{1.f};
};
constexpr int mostClasses = 64;

// vr_struggle_classes parsed (again when it changes).
struct StruggleClasses
{
    char source[2048]{};
    bool parsed{false};
    za::Array<StruggleClass, mostClasses> list{};
    int count{0};
};
StruggleClasses struggleClasses;

void parseClasses()
{
    StruggleClasses& sc = struggleClasses;
    const char* src = vr_struggle_classes.string ? vr_struggle_classes.string : "";
    if(sc.parsed && std::strcmp(sc.source, src) == 0)
    {
        return;
    }
    sc.parsed = true;
    q_strlcpy(sc.source, src, sizeof(sc.source));
    sc.count = 0;
    const char* p = src;
    while(*p && sc.count < mostClasses)
    {
        while(*p == ' ' || *p == ',' || *p == '\t')
        {
            p++;
        }
        char word[96];
        int n = 0;
        while(*p && *p != ' ' && *p != ',' && *p != '\t')
        {
            if(n < static_cast<int>(sizeof(word)) - 1)
            {
                word[n++] = *p;
            }
            p++;
        }
        word[n] = 0;
        if(n == 0)
        {
            continue;
        }
        StruggleClass& c = sc.list[static_cast<za::SizeT>(sc.count)];
        c = StruggleClass{};
        char* field = std::strchr(word, ':');
        if(field)
        {
            *field++ = 0;
        }
        q_strlcpy(c.classname, word, sizeof(c.classname));
        float* values[4] = {&c.strength, &c.speed, &c.health, &c.arms};
        for(int i = 0; i < 4 && field && *field; i++)
        {
            char* next = std::strchr(field, ':');
            if(next)
            {
                *next++ = 0;
            }
            *values[i] = static_cast<float>(std::atof(field));
            field = next;
        }
        c.strength = za::clamp(c.strength, 0.f, 3.f);
        c.speed = za::clamp(c.speed, 0.2f, 3.f);
        c.arms = za::clamp(c.arms, 1.f, 2.5f);
        sc.count++;
    }
}

[[nodiscard]] const StruggleClass* struggleClassOf(const edict_t* ent)
{
    parseClasses();
    const char* name = PR_GetString(ent->v.classname);
    for(int i = 0; i < struggleClasses.count; i++)
    {
        const StruggleClass& c = struggleClasses.list[static_cast<za::SizeT>(i)];
        if(!std::strcmp(c.classname, name))
        {
            return &c;
        }
    }
    return nullptr;
}

// Why it may not struggle now (null: it may), with `in`'s holds filled in.
[[nodiscard]] const char* holdsOf(RagdollBodies& r, edict_t* ent, ragactive::StruggleInput& in)
{
    in.holds = 0;
    const edict_t* player = nullptr;
    for(const World::RagdollGrab& g : world->ragdollGrabs)
    {
        // (The ragdoll tool's virtual hand is the physgun's hold, player 0: a hand of the first player's to the struggle.)
        const bool toolHand = g.player == 0 && ragtool::active() && svs.maxclients >= 1;
        if(g.num != r.num || (g.player < 1 && !toolHand) || g.player > svs.maxclients || g.part < 0 || g.part >= r.count ||
            B3_IS_NULL(g.anchor) || !b3Body_IsValid(g.anchor) || B3_IS_NULL(g.joint) || !b3Joint_IsValid(g.joint) ||
            in.holds >= 2)
        {
            continue;
        }
        ragactive::StruggleHold& h = in.hold[static_cast<za::SizeT>(in.holds++)];
        h.part = g.part;
        h.hand = toolHand ? 1 : g.hand;
        h.at = glmv(b3Body_GetPosition(g.anchor));
        h.grip = glmv(b3Body_GetWorldPoint(r.body[static_cast<za::SizeT>(g.part)], b3Joint_GetLocalFrameB(g.joint).p));
        player = player ? player : EDICT_NUM(toolHand ? 1 : g.player);
    }
    // A test's (vr_struggle_force): held by its chest where it lies, by the first player.
    if(in.holds == 0 && vr_struggle_force.value != 0.f && svs.maxclients >= 1)
    {
        int chest = 0;
        for(int b = 0; b < r.count; b++)
        {
            chest = chest == 0 && r.rig->bones[b].role == modelmeta::BoneRole::Chest ? b : chest;
        }
        ragactive::StruggleHold& h = in.hold[0];
        in.holds = 1;
        h.part = chest;
        h.at = h.grip = glmv(b3Body_GetWorldCenter(r.body[static_cast<za::SizeT>(chest)]));
        player = EDICT_NUM(1);
    }
    if(in.holds == 0)
    {
        return "not held";
    }
    in.feet = glmv(world->toM(vec(player->v.origin) + glm::vec3{0.f, 0.f, player->v.mins[2]}));
    in.eye = glmv(world->toM(vec(player->v.origin) + vec(player->v.view_ofs)));
    (void)ent;
    return nullptr;
}

// Its voice while it struggles (vr_struggle_voice): its own class's samples. AD's monsters name theirs in their fields
// (ad_pain_sound, ad_pain_sound2, ad_sight_sound, ad_idle_sound); id1's from this table (each precached by its spawn).
// Only those precached are played. Never its sight sound (its cry on noticing the player: wrong in a struggle, his
// note).
struct VoiceSet
{
    const char* classname;
    const char* samples[6];
};
constexpr VoiceSet voiceSets[] = {
    {"monster_army", {"soldier/pain1.wav", "soldier/pain2.wav", "soldier/idle.wav"}},
    {"monster_enforcer", {"enforcer/pain1.wav", "enforcer/pain2.wav", "enforcer/idle1.wav"}},
    {"monster_knight", {"knight/khurt.wav", "knight/idle.wav"}},
    {"monster_hell_knight", {"hknight/pain1.wav", "hknight/grunt.wav", "hknight/idle.wav"}},
    {"monster_zombie", {"zombie/z_pain.wav", "zombie/z_pain1.wav", "zombie/z_idle.wav", "zombie/z_idle1.wav"}},
};
constexpr const char* voiceFields[] = {"ad_pain_sound", "ad_pain_sound2", "ad_idle_sound"};
constexpr int mostVoices = 10;

[[nodiscard]] bool precachedSound(const char* sample)
{
    for(int i = 1; i < MAX_SOUNDS && sv.sound_precache[i]; i++)
    {
        if(!std::strcmp(sample, sv.sound_precache[i]))
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] uint32_t voiceHash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 13;
    return h;
}

// Its samples (at most mostVoices, each once; an idle's last: `idleFrom` the first of those).
int voicesOf(edict_t* ent, za::Array<const char*, mostVoices>& out, int& idleFrom)
{
    int n = 0;
    idleFrom = 0;
    const auto add = [&](const char* s) {
        if(!s || !*s || n >= mostVoices || std::strstr(s, "sight") || !precachedSound(s))
        {
            return;
        }
        for(int i = 0; i < n; i++)
        {
            if(!std::strcmp(out[static_cast<za::SizeT>(i)], s))
            {
                return;
            }
        }
        out[static_cast<za::SizeT>(n++)] = s;
    };
    const char* name = PR_GetString(ent->v.classname);
    for(int pass = 0; pass < 2; pass++) // (the cries, then the idles)
    {
        for(const char* field : voiceFields)
        {
            const bool idle = std::strstr(field, "idle") != nullptr;
            if(idle == (pass == 1))
            {
                const eval_t* v = GetEdictFieldValueByName(ent, field);
                add(v && v->string ? PR_GetString(v->string) : nullptr);
            }
        }
        for(const VoiceSet& set : voiceSets)
        {
            if(std::strcmp(set.classname, name))
            {
                continue;
            }
            for(const char* sample : set.samples)
            {
                if(sample && (std::strstr(sample, "idle") != nullptr) == (pass == 1))
                {
                    add(sample);
                }
            }
        }
        idleFrom = pass == 0 ? n : idleFrom;
    }
    return n;
}

// A cry now and then while it struggles: on its own beat (about vr_struggle_voice_gap at full effort, longer as it
// tires), one as it is taken and one as a burst begins; none just after a hit (its pain sound is the QC's), never
// the same sample twice running; quieter and more often its idle grumble as its effort falls. CHAN_AUTO: it cuts no
// sound of its own or another's.
void struggleVoice(edict_t* ent, ragactive::Struggle& g, double now)
{
    const float level = za::clamp(vr_struggle_voice.value, 0.f, 2.f);
    if(level <= 0.f)
    {
        return;
    }
    const bool burst = now >= g.burstFrom && now < g.burstUntil && g.burstFrom > g.voicedAt && now - g.voicedAt > 0.7;
    if((now < g.nextVoice && !burst) || now - g.hitAt < 0.6)
    {
        return;
    }
    za::Array<const char*, mostVoices> samples{};
    int idleFrom = 0;
    const int n = voicesOf(ent, samples, idleFrom);
    const float effort = za::clamp(g.effort, 0.f, 1.f);
    const uint32_t h = voiceHash(g.seed, static_cast<uint32_t>(g.voices) * 7919u + 13u);
    const auto roll = [&](uint32_t k) { return static_cast<float>(voiceHash(h, k) >> 8) / static_cast<float>(1u << 24); };
    g.nextVoice = now + static_cast<double>(za::max(vr_struggle_voice_gap.value, 0.3f) * (0.6f + 0.8f * roll(1u)) /
                                            (0.45f + 0.55f * effort));
    if(n == 0)
    {
        return;
    }
    // (Its cries while it fights hard; its grumbles too as it weakens.)
    const int pool = effort > 0.5f && idleFrom > 0 ? idleFrom : n;
    int pick = static_cast<int>(roll(2u) * static_cast<float>(pool)) % pool;
    if(pick == g.lastVoice && pool > 1)
    {
        pick = (pick + 1 + static_cast<int>(roll(3u) * static_cast<float>(pool - 1))) % pool;
        pick = pick == g.lastVoice ? (pick + 1) % pool : pick;
    }
    const float volume = za::min(level * (0.4f + 0.6f * effort) * (0.85f + 0.15f * roll(4u)), 1.f);
    const float pitch = 0.94f + 0.12f * roll(5u);
    const char* sample = samples[static_cast<za::SizeT>(pick)];
    SV_StartSoundPitch(ent, 0, sample, static_cast<int>(255.f * volume + 0.5f), 1.f, pitch); // (CHAN_AUTO, ATTN_NORM)
    g.lastVoice = pick;
    g.voicedAt = now;
    g.voices++;
    if(vr_struggle_debug.value)
    {
        Con_Printf("struggle: %d voice %s (effort %.2f volume %.2f pitch %.2f%s), %d so far\n", NUM_FOR_EDICT(ent), sample,
            effort, volume, pitch, burst ? ", a burst" : "", g.voices);
    }
}

} // namespace

void livingStruggle(RagdollBodies& r, ragactive::StruggleInput& in)
{
    in = ragactive::StruggleInput{};
    if(vr_struggle.value == 0.f || r.num <= 0 || !r.rig)
    {
        return;
    }
    edict_t* ent = EDICT_NUM(r.num);
    ragactive::State& a = r.active;
    if(ent->v.health <= 0.f || !knockedDown(ent))
    {
        in.health = ent->v.health <= 0.f ? 0.f : 1.f; // (its struggle's end says why)
        return;
    }
    const StruggleClass* sc = struggleClassOf(ent);
    if(!sc || holdsOf(r, ent, in))
    {
        return;
    }
    in.strength = sc->strength;
    in.speed = sc->speed;
    in.arms = sc->arms;
    in.health = sc->health > 0.f ? za::clamp(ent->v.health / sc->health, 0.f, 1.f) : 1.f;
    // (Not while something else leads its body: a shove's topple, a stagger, a get-up, a climb, a liquid; a throw's
    // fall until a moment after it lands.)
    const ragactive::Liquid liquid = liquidFor(r.num)(glmv(b3Body_GetPosition(r.body[0])));
    if(r.shove.on || a.stagger.on || a.rise.on || a.climb.on || a.death.on || a.dismember.on ||
        (a.on && a.swim.liquid != ragactive::Liquid::None && a.swim.liquid != ragactive::Liquid::Air) ||
        liquid != ragactive::Liquid::None)
    {
        return;
    }
    if(a.on && a.fall.on && (a.fall.landed < 0.f || static_cast<float>(qcvm->time - a.begun) < a.fall.landed + 0.3f))
    {
        return;
    }
    in.want = true;
    if(a.on && a.struggle.on)
    {
        struggleVoice(ent, a.struggle, qcvm->time);
        return;
    }
    if(!a.on)
    {
        ragactive::begin(a, activeParts(r), modelOf(ent), glmv(world->toM(vec(ent->v.origin))), r.joints.data(),
            r.jointFriction, r.jointStiffness, qcvm->time);
    }
    ragactive::Context c;
    c.world = world->id;
    c.now = qcvm->time;
    c.num = r.num;
    c.struggle = &in;
    ragactive::beginStruggle(a, activeParts(r), c);
}

} // namespace box3d::detail

namespace qvr::box3d
{

void struggleDraw(const glm::vec3& eye)
{
    if(!world || !sv.active || vr_struggle_debug.value == 0.f)
    {
        return;
    }
    for(const RagdollBodies& r : world->ragdolls)
    {
        if(r.num <= 0 || !r.active.on || !r.active.struggle.on)
        {
            continue;
        }
        const ragactive::Struggle& g = r.active.struggle;
        char text[900];
        int n = q_snprintf(text, sizeof(text), "struggle %d  effort %.2f  fatigue %.2f\n", r.num, g.effort, g.fatigue);
        for(int b = 0; b < ragactive::behaviourCount && n < static_cast<int>(sizeof(text)); b++)
        {
            n += q_snprintf(text + n, sizeof(text) - static_cast<size_t>(n), "%-7s %4.2f %10s\n", ragactive::behaviourName(b),
                g.weight[static_cast<za::SizeT>(b)], "");
        }
        q_snprintf(text + n, sizeof(text) - static_cast<size_t>(za::min(n, static_cast<int>(sizeof(text)))),
            "held %s%s%s%s\nfloor %.2f drag %.2f\nprone %.2f head-down %.2f\ngrip lag %.1f u  guard %.2f",
            g.heldTorso > 0.f ? "torso " : "", g.heldHead > 0.f ? "head " : "", g.heldLegs > 0.f ? "legs " : "",
            g.heldArms > 0.f ? "arm " : "", g.contact, g.drag, g.prone, g.inverted, g.gripError, g.guard);
        za::Array<text3d::OverlayBar, ragactive::behaviourCount> bars{};
        for(int b = 0; b < ragactive::behaviourCount; b++)
        {
            const float w = za::clamp(g.weight[static_cast<za::SizeT>(b)], 0.f, 1.f);
            bars[static_cast<za::SizeT>(b)] = text3d::OverlayBar{b + 1, 13.f, 10.f * w, 10.f,
                w > 0.5f ? glm::vec4{0.2f, 1.f, 0.3f, 0.9f} : glm::vec4{1.f, 0.55f, 0.1f, 0.9f}};
        }
        const glm::vec3 top = world->toU(b3Body_GetPosition(r.body[0])) + glm::vec3{0.f, 0.f, 40.f};
        const glm::vec3 to = top - eye;
        const float yaw = glm::degrees(za::atan2(to.y, to.x));
        text3d::queueOverlay(text, top, glm::vec3{0.f, yaw, 0.f}, 0.06f,
            za::Span<const text3d::OverlayBar>{bars.data(), static_cast<za::SizeT>(ragactive::behaviourCount)});
    }
}

// vr_struggle_status: each struggling one's weights and measures, the cost since the last call, the ragdolls' hash.
} // namespace qvr::box3d

namespace box3d::detail
{

void struggleStatus_f()
{
    if(!world || !sv.active)
    {
        return;
    }
    const VmScope vm;
    uint64_t h = 1469598103934665603ull;
    int struggling = 0;
    for(RagdollBodies& r : world->ragdolls)
    {
        if(r.num <= 0)
        {
            continue;
        }
        for(int b = 0; b < r.count; b++)
        {
            const b3Vec3 p = b3Body_GetPosition(r.body[static_cast<za::SizeT>(b)]);
            const float v[3] = {p.x, p.y, p.z};
            const auto* bytes = reinterpret_cast<const unsigned char*>(v);
            for(size_t i = 0; i < sizeof(v); i++)
            {
                h = (h ^ bytes[i]) * 1099511628211ull;
            }
        }
        if(!r.active.on || !r.active.struggle.on)
        {
            continue;
        }
        struggling++;
        ragactive::Struggle& g = r.active.struggle;
        Con_Printf("struggle status: %d %s %.2f s: swat %.2f pry %.2f crunch %.2f claw %.2f brace %.2f kick %.2f thrash %.2f; "
                   "effort %.2f fatigue %.2f; floor %.2f drag %.2f prone %.2f inverted %.2f; grip lag %.1f (most %.1f) "
                   "guard %.2f; gap %.1f bend %.0f straight %.2f; its hands %.1f from its head, %.1f over it; upright %.2f "
                   "held up %.0f N chest up %.2f; voices %d; flail %.1f u/s miss %.1f u\n",
            r.num, PR_GetString(EDICT_NUM(r.num)->v.classname), qcvm->time - g.from, g.weight[0], g.weight[1], g.weight[2],
            g.weight[3], g.weight[4], g.weight[5], g.weight[6], g.effort, g.fatigue, g.contact, g.drag, g.prone, g.inverted,
            g.gripError, g.gripErrorMost, g.guard, g.handGap, g.bend, g.straight, g.handsToHead, g.handsOverHead, g.upright,
            g.uprightForce, g.chestUp, g.voices, g.handTime > 0.f ? g.handPath / g.handTime : 0.f,
            g.handTime > 0.f ? g.goalMiss / g.handTime : 0.f);
        g.handPath = g.handTime = g.goalMiss = 0.f; // (the next read's window)
    }
    // Each hand's hold (struggling or not: the tests' grip lag and pose with the struggle off too): the held point's lag
    // from the hand, and its body's pose (its waist's bend, belly down, its head over its pelvis, its nearest arm's
    // forearm or hand from the player's head).
    for(const World::RagdollGrab& g : world->ragdollGrabs)
    {
        const RagdollBodies* r = ragdollOf(g.num);
        if(!r || g.player < 1 || g.player > svs.maxclients || g.part < 0 || g.part >= r->count || B3_IS_NULL(g.joint) ||
            !b3Joint_IsValid(g.joint))
        {
            continue;
        }
        const glm::vec3 at = glmv(b3Body_GetPosition(g.anchor));
        const glm::vec3 grip =
            glmv(b3Body_GetWorldPoint(r->body[static_cast<za::SizeT>(g.part)], b3Joint_GetLocalFrameB(g.joint).p));
        int chest = 0;
        for(int b = 0; b < r->count; b++)
        {
            chest = chest == 0 && r->rig->bones[b].role == modelmeta::BoneRole::Chest ? b : chest;
        }
        const glm::quat qp = fromB3(b3Body_GetRotation(r->body[0])), qc = fromB3(b3Body_GetRotation(r->body[static_cast<za::SizeT>(chest)]));
        const glm::vec3 up = qp * glm::vec3{0.f, 0.f, 1.f}, cu = qc * glm::vec3{0.f, 0.f, 1.f};
        const float bend = glm::degrees(za::atan2(glm::length(glm::cross(up, cu)), glm::dot(up, cu))) *
                           (glm::dot(cu, qp * glm::vec3{1.f, 0.f, 0.f}) >= 0.f ? 1.f : -1.f);
        const edict_t* player = EDICT_NUM(g.player);
        const glm::vec3 eye = glmv(world->toM(vec(player->v.origin) + vec(player->v.view_ofs)));
        float gap = 1e9f, toHand = 1e9f, toHead = 1e9f;
        const float k = activeParts(*r).k;
        const int headPart = r->rig->head > 0 && r->rig->head < r->count ? r->rig->head : 0;
        const glm::vec3 ownHead = glmv(b3Body_GetWorldCenter(r->body[static_cast<za::SizeT>(headPart)]));
        const char* heldName = r->rig->bones[g.part].name;
        const size_t heldLen = std::strlen(heldName);
        float legs = 0.f;
        int legCount = 0;
        for(int b = 0; b < r->count; b++)
        {
            const ragdoll::Bone& bone = r->rig->bones[b];
            const char* n = bone.name;
            const size_t len = std::strlen(n);
            const auto at3 = [&](int part, const glm::vec3& rest) {
                return glmv(b3Body_GetWorldPoint(r->body[static_cast<za::SizeT>(part)], b3v(rest * k)));
            };
            if(!std::strncmp(n, "forearm_", 8))
            {
                const glm::vec3 tip = at3(b, bone.end);
                gap = za::min(gap, glm::distance(tip, eye));
                toHead = za::min(toHead, glm::distance(tip, ownHead));
                // (Not the arm it is held by: its side's letter, the names' last.)
                if(!(heldLen > 0 && len > 0 && n[len - 1] == heldName[heldLen - 1] && std::strstr(heldName, "arm")))
                {
                    toHand = za::min(toHand, glm::distance(tip, at));
                }
            }
            if(!std::strncmp(n, "shin_", 5) && bone.parent > 0)
            {
                const ragdoll::Bone& thigh = r->rig->bones[bone.parent];
                const float full = (glm::length(bone.pivot - thigh.pivot) + glm::length(bone.end - bone.pivot)) * k;
                legs += glm::distance(at3(b, bone.end), at3(bone.parent, thigh.pivot)) / za::max(full, 1e-3f);
                legCount++;
            }
        }
        const int head = r->rig->head;
        const float headUp = head > 0 && head < r->count
                                 ? (b3Body_GetWorldCenter(r->body[static_cast<za::SizeT>(head)]).z - b3Body_GetWorldCenter(r->body[0]).z) * world->m2u
                                 : 0.f;
        Con_Printf("struggle hold: %d hand %d part %d (%s) lag %.2f units; bend %.0f prone %.2f head %.1f over its pelvis; "
                   "an arm %.1f from your head, %.1f from this hand; legs straight %.2f; pelvis %.1f %.1f %.1f; a hand %.1f "
                   "from its head; chest up %.2f\n",
            g.num, g.hand, g.part, r->rig->bones[g.part].name, glm::distance(at, grip) * world->m2u, bend,
            -(qc * glm::vec3{1.f, 0.f, 0.f}).z, headUp, gap * world->m2u, toHand * world->m2u,
            legCount > 0 ? legs / static_cast<float>(legCount) : 0.f, world->toU(b3Body_GetPosition(r->body[0])).x,
            world->toU(b3Body_GetPosition(r->body[0])).y, world->toU(b3Body_GetPosition(r->body[0])).z,
            toHead * world->m2u, cu.z);
    }
    // vr_struggle_status hurt <n>: the held ones' health down by n (tests: at 0 or less its struggle ends as dead).
    if(Cmd_Argc() > 2 && !q_strcasecmp(Cmd_Argv(1), "hurt"))
    {
        const float n = static_cast<float>(std::atof(Cmd_Argv(2)));
        for(const World::RagdollGrab& g : world->ragdollGrabs)
        {
            EDICT_NUM(g.num)->v.health -= n;
            Con_Printf("struggle status: %d hurt by %.0f: health %.0f\n", g.num, n, EDICT_NUM(g.num)->v.health);
        }
    }
    double ms = 0.0;
    int frames = 0, count = 0;
    ragactive::struggleCostSince(ms, frames, count);
    Con_Printf("struggle status: %d struggling; %.4f ms a frame over %d frames (%.4f ms each, %d frames' struggles); "
               "ragdolls' hash %016llx\n",
        struggling, frames > 0 ? ms / frames : 0.0, frames, count > 0 ? ms / count : 0.0, count,
        static_cast<unsigned long long>(h));
}

} // namespace box3d::detail
