/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2020-2020 Vittorio Romeo

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "quakedef.hpp"

#include <algorithm>
#include <random>
#include <utility>
#include <array>

#define MAX_PARTICLES \
    65536 // default max # of particles at one
          //  time
#define ABSOLUTE_MIN_PARTICLES \
    1024 // no fewer than this no matter what's
         //  on the command line

// These "ramps" below are for colors..
constexpr int ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
constexpr int ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
constexpr int ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

class ParticleBuffer
{
private:
    particle_t* _particles;
    particle_t* _aliveEnd;
    particle_t* _end;

public:
    void initialize(const std::size_t maxParticles) noexcept
    {
        _particles = (particle_t*)Hunk_AllocName(
            maxParticles * sizeof(particle_t), "particles");
        _aliveEnd = _particles;
        _end = _particles + maxParticles;
    }

    void cleanup() noexcept
    {
        _aliveEnd = std::remove_if(_particles, _aliveEnd,
            [](const particle_t& p) { return cl.time >= p.die; });
    }

    [[nodiscard]] particle_t& create() noexcept
    {
        return *_aliveEnd++;
    }

    template <typename F>
    void forActive(F&& f) noexcept
    {
        for(auto p = _particles; p != _aliveEnd; ++p)
        {
            f(*p);
        }
    }

    [[nodiscard]] bool full() const noexcept
    {
        return _aliveEnd == _end;
    }

    void clear() noexcept
    {
        _aliveEnd = _particles;
    }

    [[nodiscard]] bool empty() const noexcept
    {
        return _aliveEnd == _particles;
    }
};

class ParticleTextureManager
{
public:
    using Handle = std::uint8_t;

private:
    static constexpr std::size_t maxTextures = 32;
    std::array<gltexture_t*, maxTextures> _textures;
    Handle _next = 0;

public:
    [[nodiscard]] Handle put(gltexture_t* const texture) noexcept
    {
        assert(_next < maxTextures);
        _textures[_next] = texture;
        return _next++;
    }

    [[nodiscard]] gltexture_t* get(const Handle handle) noexcept
    {
        assert(handle < _next);
        return _textures[handle];
    }
};

std::random_device rd;
std::mt19937 mt(rd());

[[nodiscard]] static float rnd(const float min, const float max) noexcept
{
    return std::uniform_real_distribution<float>{min, max}(mt);
}

[[nodiscard]] static int rndi(const int min, const int max) noexcept
{
    return std::uniform_int_distribution<int>{min, max - 1}(mt);
}

template <typename F>
void makeParticle(F&& f)
{
    if(pBuffer.full())
    {
        return;
    }

    f(pBuffer.create());
}

template <typename F>
void makeNParticles(const int count, F&& f)
{
    for(int i = 0; i < count; i++)
    {
        makeParticle(f);
    }
}

template <typename F>
void makeNParticlesI(const int count, F&& f)
{
    for(int i = 0; i < count; i++)
    {
        if(pBuffer.full())
        {
            return;
        }

        f(i, pBuffer.create());
    }
}

void setAccGrav(particle_t& p, float mult = 0.5f)
{
    extern cvar_t sv_gravity;

    p.acc[0] = 0.f;
    p.acc[1] = 0.f;
    p.acc[2] = -sv_gravity.value * mult;
}


ParticleBuffer pBuffer;
ParticleTextureManager pTextureMgr;
int r_numparticles;

gltexture_t* default_particletexture;
gltexture_t* particletexture1;
gltexture_t* particletexture2;
gltexture_t* particletexture3;
gltexture_t* particletexture4;
gltexture_t* testtx;      // johnfitz
float texturescalefactor; // johnfitz -- compensate for apparent size of
                          // different particle textures

cvar_t r_particles = {"r_particles", "1", CVAR_ARCHIVE}; // johnfitz

template <typename F>
void forActiveParticles(F&& f)
{
    pBuffer.forActive(std::forward<F>(f));
}

/*
===============
R_ParticleTextureLookup -- johnfitz -- generate nice antialiased 32x32 circle
for particles
===============
*/
int R_ParticleTextureLookup(int x, int y, int sharpness)
{
    x -= 16;
    y -= 16;

    // distance from point x,y to circle origin, squared
    int r = x * x + y * y;
    r = r > 255 ? 255 : r;

    // alpha value to return
    int a = sharpness * (255 - r);
    a = q_min(a, 255);

    return a;
}



static void buildCircleTexture(byte* dst) noexcept
{
    for(int x = 0; x < 64; x++)
    {
        for(int y = 0; y < 64; y++)
        {
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = R_ParticleTextureLookup(x, y, 8);
        }
    }
}

static void buildSquareTexture(byte* dst) noexcept
{
    for(int x = 0; x < 2; x++)
    {
        for(int y = 0; y < 2; y++)
        {
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = x || y ? 0 : 255;
        }
    }
}

static void buildBlobTexture(byte* dst) noexcept
{
    for(int x = 0; x < 64; x++)
    {
        for(int y = 0; y < 64; y++)
        {
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = R_ParticleTextureLookup(x, y, 2);
        }
    }
}

[[nodiscard]] gltexture_t* makeTextureFromDataBuffer(
    const char* name, int width, int height, byte* data) noexcept
{
    return TexMgr_LoadImage(nullptr, name, width, height, SRC_RGBA, data, "",
        (src_offset_t)data, TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_LINEAR);
}

struct ImageData
{
    byte* data; // Hunk-allocated.
    int width;
    int height;
};

[[nodiscard]] ImageData loadImage(const char* filename)
{
    char filenameBuf[128];
    q_snprintf(filenameBuf, sizeof(filenameBuf), filename);

    int width;
    int height;
    byte* data = Image_LoadImage(filename, &width, &height);

    return {data, width, height};
}

[[nodiscard]] gltexture_t* makeTextureFromImageData(
    const char* name, const ImageData& imageData) noexcept
{
    return makeTextureFromDataBuffer(
        name, imageData.width, imageData.height, imageData.data);
}


/*
===============
R_InitParticleTextures -- johnfitz -- rewritten
===============
*/
void R_InitParticleTextures()
{
    static byte particle1_data[64 * 64 * 4];
    static byte particle2_data[2 * 2 * 4];
    static byte particle3_data[64 * 64 * 4];

    // particle texture 1 -- circle
    buildCircleTexture(particle1_data);
    particletexture1 =
        makeTextureFromDataBuffer("particle1", 64, 64, particle1_data);

    // particle texture 2 -- square
    buildSquareTexture(particle2_data);
    particletexture1 =
        makeTextureFromDataBuffer("particle2", 64, 64, particle2_data);

    // particle texture 3 -- blob
    buildBlobTexture(particle3_data);
    particletexture1 =
        makeTextureFromDataBuffer("particle3", 64, 64, particle3_data);

    // particle texture 3 -- explosion
    testtx = makeTextureFromImageData(
        "particle4", loadImage("textures/particle_explosion"));

    // set default
    default_particletexture = particletexture1;
    texturescalefactor = 1.27;
}

/*
===============
R_SetParticleTexture_f -- johnfitz
===============
*/
static void R_SetParticleTexture_f(cvar_t* var)
{
    (void)var;

    switch((int)(r_particles.value))
    {
        case 1:
            default_particletexture = particletexture1;
            texturescalefactor = 1.27;
            break;
        case 2:
            default_particletexture = particletexture2;
            texturescalefactor = 1.0;
            break;
            //	case 3:
            //		default_particletexture = particletexture3;
            //		texturescalefactor = 1.5;
            //		break;
    }
}

static void R_InitRNumParticles()
{
    const int i = COM_CheckParm("-particles");

    if(i)
    {
        r_numparticles = (int)(Q_atoi(com_argv[i + 1]));
        if(r_numparticles < ABSOLUTE_MIN_PARTICLES)
        {
            r_numparticles = ABSOLUTE_MIN_PARTICLES;
        }
    }
    else
    {
        r_numparticles = MAX_PARTICLES;
    }
}

static void R_InitParticleCVars()
{
    Cvar_RegisterVariable(&r_particles); // johnfitz
    Cvar_SetCallback(&r_particles, R_SetParticleTexture_f);
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles()
{
    R_InitRNumParticles();

    pBuffer.initialize(r_numparticles);

    R_InitParticleCVars();
    R_InitParticleTextures(); // johnfitz
}

/*
===============
R_EntityParticles
===============
*/
#define NUMVERTEXNORMALS 162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];
vec3_t avelocities[NUMVERTEXNORMALS];
float beamlength = 16;

void R_EntityParticles(entity_t* ent)
{
    if(!avelocities[0][0])
    {
        for(int i = 0; i < NUMVERTEXNORMALS; i++)
        {
            avelocities[i][0] = (rand() & 255) * 0.01;
            avelocities[i][1] = (rand() & 255) * 0.01;
            avelocities[i][2] = (rand() & 255) * 0.01;
        }
    }

    for(int i = 0; i < NUMVERTEXNORMALS; i++)
    {
        float angle = cl.time * avelocities[i][0];
        float sy = sin(angle);
        float cy = cos(angle);

        angle = cl.time * avelocities[i][1];
        float sp = sin(angle);
        float cp = cos(angle);

        vec3_t forward;
        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;

        if(pBuffer.full())
        {
            return;
        }

        particle_t& p = pBuffer.create();

        p.die = cl.time + 0.01;
        p.color = 0x6f;
        p.type = pt_explode;
        p.scale = 1.f;
        setAccGrav(p);

        constexpr float dist = 64;
        p.org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist +
                   forward[0] * beamlength;
        p.org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist +
                   forward[1] * beamlength;
        p.org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist +
                   forward[2] * beamlength;
    }
}

/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles()
{
    pBuffer.clear();
}

/*
===============
R_ReadPointFile_f
===============
*/
void R_ReadPointFile_f()
{
    if(cls.state != ca_connected)
    {
        return; // need an active map.
    }

    char name[MAX_QPATH];
    q_snprintf(name, sizeof(name), "maps/%s.pts", cl.mapname);

    FILE* f;
    COM_FOpenFile(name, &f, nullptr);
    if(!f)
    {
        Con_Printf("couldn't open %s\n", name);
        return;
    }

    Con_Printf("Reading %s...\n", name);

    int c = 0;
    while(true)
    {
        vec3_t org;
        const int r = fscanf(f, "%f %f %f\n", &org[0], &org[1], &org[2]);

        if(r != 3)
        {
            break;
        }

        c++;

        if(pBuffer.full())
        {
            Con_Printf("Not enough free particles\n");
            break;
        }

        particle_t& p = pBuffer.create();

        p.die = 99999;
        p.color = (-c) & 15;
        p.type = pt_static;
        p.scale = 1.f;
        setAccGrav(p);

        VectorCopy(vec3_origin, p.vel);
        VectorCopy(org, p.org);
    }

    fclose(f);
    Con_Printf("%i points read\n", c);
}

/*
===============
R_ParseParticleEffect

Parse an effect out of the server message
===============
*/
void R_ParseParticleEffect()
{
    vec3_t org;
    for(int i = 0; i < 3; i++)
    {
        org[i] = MSG_ReadCoord(cl.protocolflags);
    }

    vec3_t dir;
    for(int i = 0; i < 3; i++)
    {
        dir[i] = MSG_ReadChar() * (1.0 / 16);
    }

    const int msgcount = MSG_ReadByte();
    const int color = MSG_ReadByte();

    // TODO VR:
    // const int count = msgcount == 255 ? 1024 : msgcount;

    R_RunParticleEffect(org, dir, color, msgcount);
}

// TODO VR:
/*
===============
R_ParseParticle2Effect

Parse an effect out of the server message
===============
*/
void R_ParseParticle2Effect()
{
    vec3_t org;
    for(int i = 0; i < 3; i++)
    {
        org[i] = MSG_ReadCoord(cl.protocolflags);
    }

    vec3_t dir;
    for(int i = 0; i < 3; i++)
    {
        dir[i] = MSG_ReadChar() * (1.0 / 16);
    }

    const int preset = MSG_ReadByte();
    const int msgcount = MSG_ReadByte();

    R_RunParticle2Effect(org, dir, preset, msgcount);
}

/*
===============
R_ParticleExplosion
===============
*/
void R_ParticleExplosion(vec3_t org)
{
    makeNParticlesI(1024, [&](const int i, particle_t& p) {
        p.die = cl.time + 5;
        p.color = ramp1[0];
        p.ramp = rand() & 3;
        p.scale = rnd(0.9f, 2.3f);
        setAccGrav(p);
        p.type = i & 1 ? pt_explode : pt_explode2;

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + rnd(-16, 16);
            p.vel[j] = rnd(-256, 256);
        }
    });

    makeNParticles(512, [&](particle_t& p) {
        p.die = cl.time + 3.5 * (rand() % 5);
        p.color = rand() & 7;
        p.scale = rnd(0.2f, 0.5f);
        p.type = pt_static;
        setAccGrav(p, -0.08f);

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + ((rand() & 7) - 4);
            p.vel[j] = rnd(-24, 24);
        }

        p.vel[2] += rnd(10, 40);
    });
}

/*
===============
R_ParticleExplosion2
===============
*/
void R_ParticleExplosion2(vec3_t org, int colorStart, int colorLength)
{
    int colorMod = 0;

    for(int i = 0; i < 512; i++)
    {
        if(pBuffer.full())
        {
            return;
        }

        particle_t& p = pBuffer.create();

        p.die = cl.time + 0.3;
        p.color = colorStart + (colorMod % colorLength);
        p.scale = 1.f;
        colorMod++;
        setAccGrav(p);

        p.type = pt_blob;
        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + rnd(-16, 16);
            p.vel[j] = rnd(-256, 256);
        }
    }
}

/*
===============
R_BlobExplosion
===============
*/
void R_BlobExplosion(vec3_t org)
{
    for(int i = 0; i < 1024; i++)
    {
        if(pBuffer.full())
        {
            return;
        }

        particle_t& p = pBuffer.create();

        p.die = cl.time + 1 + (rand() & 8) * 0.05;
        p.scale = 1.f;
        setAccGrav(p);

        if(i & 1)
        {
            p.type = pt_blob;
            p.color = 66 + rand() % 6;
        }
        else
        {
            p.type = pt_blob2;
            p.color = 150 + rand() % 6;
        }

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + rnd(-16, 16);
            p.vel[j] = rnd(-256, 256);
        }
    }
}

void R_RunParticleEffect_BulletPuff(
    vec3_t org, vec3_t dir, int color, int count)
{
    const auto debrisCount = count * 0.7f;
    const auto dustCount = count * 0.7f;
    const auto sparkCount = count * 0.4f;

    makeNParticles(debrisCount, [&](particle_t& p) {
        p.die = cl.time + 0.7 * (rand() % 5);
        p.color = (color & ~7) + (rand() & 7);
        p.scale = rnd(0.5f, 0.9f);
        p.type = pt_static;
        setAccGrav(p, 0.26f);

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + ((rand() & 7) - 4);
            p.vel[j] = dir[j] * rnd(5, 90);
        }
    });

    makeNParticles(dustCount, [&](particle_t& p) {
        p.die = cl.time + 1.5 * (rand() % 5);
        p.color = (color & ~7) + (rand() & 7);
        p.scale = rnd(0.2f, 0.5f);
        p.type = pt_static;
        setAccGrav(p, 0.08f);

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + ((rand() & 7) - 4);
            p.vel[j] = rnd(-24, 24);
        }

        p.vel[2] += rnd(10, 40);
    });

    makeNParticles(sparkCount, [&](particle_t& p) {
        p.die = cl.time + 1.6 * (rand() % 5);
        p.color = ramp3[0] + (rand() & 7);
        p.scale = rnd(0.15f, 0.35f);
        p.type = pt_static;
        setAccGrav(p, 1.f);

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + ((rand() & 7) - 4);
            p.vel[j] = rnd(-48, 48);
        }

        p.vel[2] = rnd(60, 360);
    });
}

void R_RunParticleEffect_Blood(vec3_t org, vec3_t dir, int count)
{
    constexpr int bloodColors[]{247, 248, 249, 250, 251};
    const auto pickBloodColor = [&] { return bloodColors[rndi(0, 5)]; };

    makeNParticles(count * 5, [&](particle_t& p) {
        p.die = cl.time + 0.7 * (rand() % 5);
        p.color = pickBloodColor();
        p.scale = rnd(0.35f, 0.6f);
        p.type = pt_static;
        setAccGrav(p, 0.33f);

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + rnd(-2, 2);
            p.vel[j] = (dir[j] + 0.3f) * rnd(-10, 10);
        }
    });

    makeNParticles(count * 15, [&](particle_t& p) {
        p.die = cl.time + 0.4 * (rand() % 5);
        p.color = pickBloodColor();
        p.scale = rnd(0.15f, 0.4f);
        p.type = pt_static;
        setAccGrav(p, 0.45f);

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + rnd(-2, 2);
            p.vel[j] = (dir[j] + 0.3f) * rnd(-3, 3);
            p.vel[j] *= 13.f;
        }

        p.vel[2] += rnd(20, 60);
    });
}

/*
===============
R_RunParticleEffect
===============
*/
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
    // TODO VR: add way to change types
    R_RunParticleEffect_BulletPuff(org, dir, color, count);
}

// TODO VR:
/*
===============
R_RunParticle2Effect
===============
*/
void R_RunParticle2Effect(vec3_t org, vec3_t dir, int preset, int count)
{
    enum class Preset : int
    {
        BulletPuff = 0,
        Blood = 1
    };

    switch(static_cast<Preset>(preset))
    {
        case Preset::BulletPuff:
        {
            R_RunParticleEffect_BulletPuff(org, dir, 0, count);
            break;
        }
        case Preset::Blood:
        {
            R_RunParticleEffect_Blood(org, dir, count);
            break;
        }
        default:
        {
            assert(false);
            break;
        }
    }
}

/*
===============
R_LavaSplash
===============
*/
void R_LavaSplash(vec3_t org)
{
    for(int i = -16; i < 16; i++)
    {
        for(int j = -16; j < 16; j++)
        {
            makeNParticles(1, [&](particle_t& p) {
                p.scale = 1.f;
                p.die = cl.time + 2 + (rand() & 31) * 0.02;
                p.color = 224 + (rand() & 7);
                p.type = pt_static;
                setAccGrav(p);

                vec3_t dir;
                dir[0] = j * 8 + (rand() & 7);
                dir[1] = i * 8 + (rand() & 7);
                dir[2] = 256;

                p.org[0] = org[0] + dir[0];
                p.org[1] = org[1] + dir[1];
                p.org[2] = org[2] + (rand() & 63);

                VectorNormalize(dir);
                const float vel = 50 + (rand() & 63);
                VectorScale(dir, vel, p.vel);
            });
        }
    }
}

/*
===============
R_TeleportSplash
===============
*/
void R_TeleportSplash(vec3_t org)
{
    for(int i = -16; i < 16; i += 4)
    {
        for(int j = -16; j < 16; j += 4)
        {
            for(int k = -24; k < 32; k += 4)
            {
                if(pBuffer.full())
                {
                    return;
                }

                particle_t& p = pBuffer.create();

                p.scale = 1.f;
                p.die = cl.time + 1.2 + (rand() & 7) * 0.2;
                p.color = 7 + (rand() & 7);
                p.type = pt_static;
                setAccGrav(p, 0.2f);

                vec3_t dir;
                dir[0] = j * 8;
                dir[1] = i * 8;
                dir[2] = k * 8;

                p.org[0] = org[0] + i + (rand() & 3);
                p.org[1] = org[1] + j + (rand() & 3);
                p.org[2] = org[2] + k + (rand() & 3);

                VectorNormalize(dir);
                const float vel = 50 + (rand() & 63);
                VectorScale(dir, vel, p.vel);
            }
        }
    }
}

/*
===============
R_RocketTrail

FIXME -- rename function and use #defined types instead of numbers
===============
*/
void R_RocketTrail(vec3_t start, vec3_t end, int type)
{
    static int tracercount;

    vec3_t vec;
    VectorSubtract(end, start, vec);

    int dec;
    if(type < 128)
    {
        dec = 3;
    }
    else
    {
        dec = 1;
        type -= 128;
    }

    float len = VectorNormalize(vec);
    while(len > 0)
    {
        len -= dec;

        if(pBuffer.full())
        {
            return;
        }

        particle_t& p = pBuffer.create();
        p.scale = 0.7f;
        setAccGrav(p, 0.05f);

        VectorCopy(vec3_origin, p.vel);
        p.die = cl.time + 2;

        switch(type)
        {
            case 0: // rocket trail
                p.ramp = (rand() & 3);
                p.color = ramp3[(int)p.ramp];
                p.type = pt_fire;
                for(int j = 0; j < 3; j++)
                {
                    p.org[j] = start[j] + ((rand() % 6) - 3);
                }
                break;

            case 1: // smoke smoke
                p.ramp = (rand() & 3) + 2;
                p.color = ramp3[(int)p.ramp];
                p.type = pt_fire;
                for(int j = 0; j < 3; j++)
                {
                    p.org[j] = start[j] + ((rand() % 6) - 3);
                }
                break;

            case 2: // blood
                p.type = pt_static;
                p.color = 67 + (rand() & 3);
                for(int j = 0; j < 3; j++)
                {
                    p.org[j] = start[j] + ((rand() % 6) - 3);
                }
                break;

            case 3:
            case 5: // tracer
                p.die = cl.time + 0.5;
                p.type = pt_static;
                if(type == 3)
                {
                    p.color = 52 + ((tracercount & 4) << 1);
                }
                else
                {
                    p.color = 230 + ((tracercount & 4) << 1);
                }

                tracercount++;

                VectorCopy(start, p.org);
                if(tracercount & 1)
                {
                    p.vel[0] = 30 * vec[1];
                    p.vel[1] = 30 * -vec[0];
                }
                else
                {
                    p.vel[0] = 30 * -vec[1];
                    p.vel[1] = 30 * vec[0];
                }
                break;

            case 4: // slight blood
                p.type = pt_static;
                p.color = 67 + (rand() & 3);
                for(int j = 0; j < 3; j++)
                {
                    p.org[j] = start[j] + ((rand() % 6) - 3);
                }
                len -= 3;
                break;

            case 6: // voor trail
                p.color = 9 * 16 + 8 + (rand() & 3);
                p.type = pt_static;
                p.die = cl.time + 0.3;
                for(int j = 0; j < 3; j++)
                {
                    p.org[j] = start[j] + ((rand() & 15) - 8);
                }
                break;
        }

        VectorAdd(start, vec, start);
    }
}

/*
===============
CL_RunParticles -- johnfitz -- all the particle behavior, separated from
R_DrawParticles
===============
*/
void CL_RunParticles()
{
    const float frametime = cl.time - cl.oldtime;
    const float time3 = frametime * 15;
    const float time2 = frametime * 10;
    const float time1 = frametime * 5;
    const float dvel = 4 * frametime;

    pBuffer.cleanup();

    forActiveParticles([&](particle_t& p) {
        p.vel[0] += p.acc[0] * frametime;
        p.vel[1] += p.acc[1] * frametime;
        p.vel[2] += p.acc[2] * frametime;

        p.org[0] += p.vel[0] * frametime;
        p.org[1] += p.vel[1] * frametime;
        p.org[2] += p.vel[2] * frametime;

        switch(p.type)
        {
            case pt_static:
            {
                break;
            }

            case pt_fire:
            {
                p.ramp += time1;
                if(p.ramp >= 6)
                {
                    p.die = -1;
                }
                else
                {
                    p.color = ramp3[(int)p.ramp];
                }
                break;
            }

            case pt_explode:
            {
                p.ramp += time2;
                if(p.ramp >= 8)
                {
                    p.die = -1;
                }
                else
                {
                    p.color = ramp1[(int)p.ramp];
                }
                for(int i = 0; i < 3; i++)
                {
                    p.vel[i] += p.vel[i] * dvel;
                }
                break;
            }

            case pt_explode2:
            {
                p.ramp += time3;
                if(p.ramp >= 8)
                {
                    p.die = -1;
                }
                else
                {
                    p.color = ramp2[(int)p.ramp];
                }
                for(int i = 0; i < 3; i++)
                {
                    p.vel[i] -= p.vel[i] * frametime;
                }
                break;
            }

            case pt_blob:
            {
                for(int i = 0; i < 3; i++)
                {
                    p.vel[i] += p.vel[i] * dvel;
                }
                break;
            }

            case pt_blob2:
            {
                for(int i = 0; i < 2; i++)
                {
                    p.vel[i] -= p.vel[i] * dvel;
                }
                break;
            }

            default:
            {
                break;
            }
        }
    });
}

/*
===============
R_DrawParticles -- johnfitz -- moved all non-drawing code to
CL_RunParticles
===============
*/
void R_DrawParticles()
{
    GLubyte color[4];

    GLubyte* c; // johnfitz -- particle transparency
    // johnfitz
    // float			alpha; //johnfitz -- particle transparency

    if(!r_particles.value || pBuffer.empty())
    {
        return;
    }

    vec3_t up;
    VectorScale(vup, 1.5, up);

    vec3_t right;
    VectorScale(vright, 1.5, right);

    // TODO VR:
    // GL_Bind(default_particletexture);
    GL_Bind(testtx);
    glEnable(GL_BLEND);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDepthMask(GL_FALSE); // johnfitz -- fix for particle z-buffer bug

    glBegin(GL_TRIANGLES);
    forActiveParticles([&](particle_t& p) {
        const float scale = texturescalefactor * p.scale * 85.f;

        // johnfitz -- particle transparency and fade out
        c = (GLubyte*)&d_8to24table[(int)p.color];
        color[0] = c[0];
        color[1] = c[1];
        color[2] = c[2];
        // alpha = CLAMP(0, p.die + 0.5 - cl.time, 1);
        color[3] = 255; //(int)(alpha * 255);
        glColor4ubv(color);
        // johnfitz

        glTexCoord2f(0, 0);
        glVertex3fv(p.org);

        glTexCoord2f(1, 0);
        vec3_t p_up;
        VectorMA(p.org, scale, up, p_up);
        glVertex3fv(p_up);

        glTexCoord2f(0, 1);
        vec3_t p_right;
        VectorMA(p.org, scale, right, p_right);
        glVertex3fv(p_right);

        rs_particles++; // johnfitz //FIXME: just use r_numparticles
    });
    glEnd();

    glDepthMask(GL_TRUE); // johnfitz -- fix for particle z-buffer bug
    glDisable(GL_BLEND);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1, 1, 1);
}


/*
===============
R_DrawParticles_ShowTris -- johnfitz
===============
*/
void R_DrawParticles_ShowTris()
{
    if(!r_particles.value)
    {
        return;
    }

    vec3_t up;
    VectorScale(vup, 1.5, up);

    vec3_t right;
    VectorScale(vright, 1.5, right);

    glBegin(GL_TRIANGLES);
    forActiveParticles([&](particle_t& p) {
        const float scale = texturescalefactor * p.scale;

        glVertex3fv(p.org);

        vec3_t p_up;
        VectorMA(p.org, scale, up, p_up);
        glVertex3fv(p_up);

        vec3_t p_right;
        VectorMA(p.org, scale, right, p_right);
        glVertex3fv(p_right);
    });

    glEnd();
}
