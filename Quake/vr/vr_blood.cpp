// Blood-red peripheral feedback, shared by eye and flat-screen view passes.
#include "vr_blood.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_portals.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/Cos.hpp"
#include "Zancle/Math/Exp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/Math/Pow.hpp"
#include "Zancle/Math/Sin.hpp"
#include "Zancle/Base/Strcmp.hpp"
#include <glm/geometric.hpp>

namespace qvr::blood
{
namespace
{
struct Hit
{
    glm::vec3 direction{0.f};
    double at{-100.0};
    float strength{0.f};
};
Hit hits[8]; // Separate directions survive overlapping hits; bounded storage and alpha.
double clock{0.0}, lastTime{-1.0}, phase{0.0};
int frame{-1}, nextHit{0};
bool alive{false};

float setting(const cvar_t& v, float lo, float hi)
{
    return (v.value != v.value) ? lo : za::clamp(v.value, lo, hi);
}
struct Health
{
    float severity{0.f}, peak{0.f}, rate{0.f}, pulse{0.f};
};
Health healthLook(int health, double p)
{
    Health h;
    const float threshold = setting(vr_blood_health_threshold, 1.f, 200.f);
    if(health <= 0 || health >= threshold || vr_blood_health.value == 0.f)
        return h;
    h.severity = za::pow(za::clamp((threshold - health) / threshold, 0.f, 1.f),
        setting(vr_blood_health_curve, 0.25f, 4.f));
    h.peak = setting(vr_blood_health_intensity, 0.f, 0.8f) * h.severity;
    h.rate = setting(vr_blood_health_rate, 0.1f, 3.f) *
        (1.f + h.severity * setting(vr_blood_health_rate_scale, 0.f, 4.f));
    h.rate = za::min(h.rate, 6.f);
    // A periodic Gaussian-like beat: duration and repetition remain independent.
    // Wide pulses overlap smoothly instead of shortening their requested duration.
    const float width = setting(vr_blood_health_duration, 0.05f, 2.f) * h.rate;
    float sum = 0.f, peak = 0.f;
    const int radius = static_cast<int>(width * 3.f) + 1;
    for(int i = -radius; i <= radius; ++i)
    {
        const float d = static_cast<float>(p) - i;
        sum += za::exp(-2.77258872f * d * d / (width * width));
        peak += za::exp(-2.77258872f * i * i / (width * width));
    }
    h.pulse = za::clamp(sum / peak, 0.f, 1.f);
    return h;
}
float hitLook(const Hit& h, double age)
{
    const float duration = setting(vr_blood_hit_duration, 0.f, 5.f);
    if(vr_blood_hit.value == 0.f || duration <= 0.f || age < 0.0 || age >= duration)
        return 0.f;
    const float t = 1.f - static_cast<float>(age / duration);
    return setting(vr_blood_hit_intensity, 0.f, 0.8f) * h.strength * t * t;
}
bool playable()
{
    return cls.state == ca_connected && cls.signon == SIGNONS && cl.worldmodel &&
        !cl.intermission && cl.stats[STAT_HEALTH] > 0;
}
void advance()
{
    if(frame == host_framecount)
        return;
    frame = host_framecount;
    if(!playable())
    {
        clear();
        frame = host_framecount;
        lastTime = cl.time;
        return;
    }
    if(!alive || cl.time < lastTime - 0.1 || (cls.demoplayback && cls.demospeed < 0.f))
    {
        clear();
        frame = host_framecount;
        lastTime = cl.time;
        alive = true;
    }
    const double dt = cl.paused ? 0.0 : za::clamp(cl.time - lastTime, 0.0, 0.25);
    lastTime = cl.time;
    clock += dt;
    const Health h = healthLook(cl.stats[STAT_HEALTH], phase);
    if(h.severity == 0.f)
        phase = 0.0;
    else
        phase += dt * h.rate;
    phase -= static_cast<int>(phase); // bounded clock, identical in both eyes
    for(Hit& hit : hits)
        if(vr_blood_hit.value == 0.f)
            hit = {};
}
float encodedCoordinate(float value)
{
    // Match MSG_WriteCoord / MSG_ReadCoord, including the RMQ coordinate flags.
    if(cl.protocolflags & PRFL_FLOATCOORD) return value;
    if(cl.protocolflags & PRFL_INT32COORD) return Q_rint(value * 16.f) * (1.f / 16.f);
    if(cl.protocolflags & PRFL_24BITCOORD)
        return static_cast<short>(value) + static_cast<byte>(static_cast<int>(value * 255.f) % 255) * (1.f / 255.f);
    return static_cast<short>(Q_rint(value * 8.f)) * (1.f / 8.f);
}
bool neutralSource(const float* from, const glm::vec3& delta)
{
    if(glm::dot(delta, delta) < 24.f * 24.f || (from[0] == 0.f && from[1] == 0.f && from[2] == 0.f))
        return true;
    // A listen server can identify self/world/owned projectile damage without changing
    // svc_damage or the existing hand/view-kick hooks. Match the packet source first:
    // remote servers and demos have no ownership information and retain their direction.
    if(!sv.active || !sv.qcvm.edicts || sv.qcvm.edict_size <= 0 || cls.demoplayback ||
        cl.viewentity < 1 || cl.viewentity > svs.maxclients || cl.viewentity >= sv.qcvm.num_edicts)
        return false;
    const edict_t* player = reinterpret_cast<const edict_t*>(reinterpret_cast<const byte*>(sv.qcvm.edicts) + cl.viewentity * sv.qcvm.edict_size);
    if(player->v.dmg_inflictor < 0 || player->v.dmg_inflictor % sv.qcvm.edict_size != 0 ||
        player->v.dmg_inflictor / sv.qcvm.edict_size >= sv.qcvm.num_edicts)
        return false;
    const edict_t* source = SAVE_PROG_TO_EDICT((&sv.qcvm), player->v.dmg_inflictor);
    glm::vec3 centre;
    for(int i = 0; i < 3; ++i)
        centre[i] = encodedCoordinate(source->v.origin[i] + 0.5f * (source->v.mins[i] + source->v.maxs[i]));
    if(glm::length(centre - glm::vec3{from[0], from[1], from[2]}) > 0.0001f)
        return false;
    return source == sv.qcvm.edicts || source == player || SAVE_PROG_TO_EDICT((&sv.qcvm), source->v.owner) == player;
}
glm::vec4 projection(const float* angles)
{
    vec3_t f, r, u;
    vec3_t viewAngles{angles[0], angles[1], angles[2]};
    AngleVectors(viewAngles, f, r, u);
    float neutral = 0.f;
    glm::vec3 directional{0.f};
    const float bias = setting(vr_blood_hit_bias, 0.f, 1.f);
    // Linear directional lobes with a uniform floor; compose the sum within 0.8.
    // Front hits favour the upper edge, rear the lower edge; vertical hits use up.
    for(const Hit& h : hits)
    {
        const float strength = playable() && !portals::viewing() ? hitLook(h, clock - h.at) : 0.f;
        const float x = h.direction.x * r[0] + h.direction.y * r[1] + h.direction.z * r[2];
        const float y = h.direction.x * u[0] + h.direction.y * u[1] + h.direction.z * u[2];
        const float z = h.direction.x * f[0] + h.direction.y * f[1] + h.direction.z * f[2];
        const glm::vec2 projected{x, y + z * 0.6f};
        const float length = glm::length(projected);
        const float b = length > 0.01f ? bias : 0.f;
        neutral += strength * (1.f - b * 0.5f);
        if(length > 0.01f)
        {
            directional.x += strength * b * 0.5f * projected.x / length;
            directional.y += strength * b * 0.5f * projected.y / length;
        }
    }
    const Health h = healthLook(playable() && !portals::viewing() ? cl.stats[STAT_HEALTH] : 0, phase);
    return {neutral, directional.x, directional.y, h.peak * h.pulse};
}

void info_f()
{
    advance();
    const Health h = healthLook(cl.stats[STAT_HEALTH], phase);
    float hit = 0.f;
    for(const Hit& e : hits)
        hit = 1.f - (1.f - hit) * (1.f - hitLook(e, clock - e.at));
    Con_Printf("blood size: hit %.3f health %.3f\n", setting(vr_blood_hit_size, 0.f, 1.f), setting(vr_blood_health_size, 0.f, 1.f));
    const glm::vec4 p = projection(r_refdef.viewangles);
    const auto edge = [&p](float x, float y)
    {
        const float hit = za::clamp(p.x + p.y * x + p.z * y, 0.f, 0.8f);
        return za::min(0.8f, 1.f - (1.f - hit) * (1.f - p.w));
    };
    Con_Printf("blood edges: left %.4f right %.4f up %.4f down %.4f yaw %.1f\n",
        edge(-1.f, 0.f), edge(1.f, 0.f), edge(0.f, 1.f), edge(0.f, -1.f), r_refdef.viewangles[1]);
    Con_Printf("blood: health %d severity %.4f peak %.4f rate %.4f pulse %.4f hit %.4f clock %.4f phase %.4f paused %d\n",
        cl.stats[STAT_HEALTH], h.severity, h.peak, h.rate, h.pulse, hit, clock, phase, cl.paused);
}
void test_f()
{
    if(Cmd_Argc() < 2)
    {
        Con_Printf("vr_blood_test [packet] left|right|front|rear|up|down|neutral|armor; clear; sample <health> <phase>\n");
        return;
    }
    const bool packet = ZA_STRCMP(Cmd_Argv(1), "packet") == 0;
    const char* mode = packet && Cmd_Argc() > 2 ? Cmd_Argv(2) : Cmd_Argv(1);
    if(ZA_STRCMP(mode, "clear") == 0)
    {
        clear();
        return;
    }
    if(ZA_STRCMP(mode, "sample") == 0)
    {
        const Health h = healthLook(Cmd_Argc() > 2 ? Q_atoi(Cmd_Argv(2)) : 20,
            Cmd_Argc() > 3 ? Q_atof(Cmd_Argv(3)) : 0.0);
        Con_Printf("blood sample: severity %.4f peak %.4f rate %.4f pulse %.4f\n", h.severity, h.peak, h.rate, h.pulse);
        return;
    }
    if(!playable())
        return;
    vec3_t f, r, u, from;
    AngleVectors(r_refdef.viewangles, f, r, u);
    const float* axis = f;
    float sign = 1.f;
    if(ZA_STRCMP(mode, "left") == 0) { axis = r; sign = -1.f; }
    else if(ZA_STRCMP(mode, "right") == 0 || ZA_STRCMP(mode, "armor") == 0) axis = r;
    else if(ZA_STRCMP(mode, "rear") == 0) sign = -1.f;
    else if(ZA_STRCMP(mode, "up") == 0) axis = u;
    else if(ZA_STRCMP(mode, "down") == 0) { axis = u; sign = -1.f; }
    else if(ZA_STRCMP(mode, "neutral") == 0) sign = 0.f;
    else if(ZA_STRCMP(mode, "front") != 0) return;
    for(int i = 0; i < 3; ++i)
        from[i] = cl_entities[cl.viewentity].origin[i] + sign * axis[i] * 256.f;
    const int armor = ZA_STRCMP(mode, "armor") == 0 ? 20 : 0, damage = armor ? 0 : 20;
    if(packet)
    {
        // Round-trip the actual damage payload through V_ParseDamage, then restore
        // parser state. Also exercises legacy tint/hand feedback, for debug only.
        byte data[64];
        sizebuf_t message{};
        message.data = data;
        message.maxsize = sizeof(data);
        MSG_WriteByte(&message, armor);
        MSG_WriteByte(&message, damage);
        for(int i = 0; i < 3; ++i) MSG_WriteCoord(&message, from[i], cl.protocolflags);
        const sizebuf_t saved = net_message;
        const int read = msg_readcount;
        const qboolean bad = msg_badread;
        net_message = message;
        MSG_BeginReading();
        V_ParseDamage();
        net_message = saved;
        msg_readcount = read;
        msg_badread = bad;
    }
    else
        onDamage(armor, damage, from);
    Con_Printf("blood test: %s\n", mode);
}
}
void clear()
{
    for(Hit& h : hits) h = {};
    clock = phase = 0.0;
    lastTime = -1.0;
    nextHit = 0;
    frame = -1;
    alive = false;
}
void onDamage(int armor, int blood, const float* from)
{
    advance();
    if(!playable() || vr_blood_hit.value == 0.f || armor + blood <= 0 ||
        cl.viewentity < 1 || cl.viewentity >= cl.num_entities)
        return;
    const entity_t& player = cl_entities[cl.viewentity];
    glm::vec3 d = from ? glm::vec3{from[0] - player.origin[0], from[1] - player.origin[1], from[2] - player.origin[2]} : glm::vec3{0.f};
    if(!from || neutralSource(from, d) || d.x != d.x || d.y != d.y || d.z != d.z)
        d = {0.f, 0.f, 0.f};
    else
        d = glm::normalize(d);
    hits[nextHit] = {d, clock, za::clamp((armor + blood) / 20.f, 0.25f, 1.f)};
    nextHit = (nextHit + 1) % 8;
}
bool active()
{
    advance();
    if(!playable() || portals::viewing()) return false;
    if(setting(vr_blood_health_size, 0.f, 1.f) > 0.f && healthLook(cl.stats[STAT_HEALTH], phase).peak > 0.f) return true;
    for(const Hit& h : hits)
        if(setting(vr_blood_hit_size, 0.f, 1.f) > 0.f && hitLook(h, clock - h.at) > 0.f) return true;
    return false;
}
void bind()
{
    advance();
    const glm::vec4 p = projection(r_refdef.viewangles);
    GL_Uniform4fFunc(2, p.x, p.y, p.z, p.w);
    GL_Uniform4fFunc(4, setting(vr_blood_hit_size, 0.f, 1.f), setting(vr_blood_health_size, 0.f, 1.f), 0.f, 0.f);
    GL_Uniform4fFunc(3, static_cast<float>(glx + r_refdef.vrect.x),
        static_cast<float>(gly + glheight - r_refdef.vrect.y - r_refdef.vrect.height),
        1.f / za::max(r_refdef.vrect.width, 1), 1.f / za::max(r_refdef.vrect.height, 1));
}
void registerCommands()
{
    Cmd_AddCommand("vr_blood_info", info_f);
    Cmd_AddCommand("vr_blood_test", test_f);
}
}
extern "C" int VR_BloodActive(void) { return qvr::blood::active(); }
extern "C" void VR_BloodBind(void) { qvr::blood::bind(); }
