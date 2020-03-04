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
    4096 // default max # of particles at one
         //  time
#define ABSOLUTE_MIN_PARTICLES \
    512 // no fewer than this no matter what's
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
        _aliveEnd =
            std::remove_if(_particles, _aliveEnd, [](const particle_t& p) {
                return p.alpha <= 0.f || p.scale <= 0.f || cl.time >= p.die;
            });
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

struct ImageData
{
    byte* data; // Hunk-allocated.
    int width;
    int height;
};

class ParticleTextureManager
{
public:
    using Handle = std::uint8_t;
    static constexpr std::size_t maxTextures = 32;

private:
    std::array<gltexture_t*, maxTextures> _textures;
    std::array<ImageData, maxTextures> _imageData;
    Handle _next = 0;

public:
    [[nodiscard]] Handle put(
        gltexture_t* const texture, const ImageData& imageData) noexcept
    {
        assert(_next < maxTextures);
        _textures[_next] = texture;
        _imageData[_next] = imageData;
        return _next++;
    }

    [[nodiscard]] gltexture_t* get(const Handle handle) const noexcept
    {
        assert(handle < _next);
        return _textures[handle];
    }

    [[nodiscard]] const ImageData& getImageData(
        const Handle handle) const noexcept
    {
        assert(handle < _next);
        return _imageData[handle];
    }

    [[nodiscard]] std::size_t numActive() const noexcept
    {
        return _next;
    }
};

int r_numparticles;

class ParticleManager
{
public:
    using Handle = ParticleTextureManager::Handle;

private:
    ParticleTextureManager _textureMgr;
    std::array<ParticleBuffer, ParticleTextureManager::maxTextures> _buffers;

public:
    [[nodiscard]] Handle createBuffer(
        gltexture_t* const texture, const ImageData& imageData) noexcept
    {
        const Handle h = _textureMgr.put(texture, imageData);
        _buffers[h].initialize(r_numparticles);
        return h;
    }

    [[nodiscard]] gltexture_t* getTexture(const Handle handle) const noexcept
    {
        return _textureMgr.get(handle);
    }

    [[nodiscard]] const ImageData& getImageData(
        const Handle handle) const noexcept
    {
        return _textureMgr.getImageData(handle);
    }

    [[nodiscard]] ParticleBuffer& getBuffer(
        const ParticleTextureManager::Handle txHandle) noexcept
    {
        assert(txHandle < _textureMgr.numActive());
        return _buffers[txHandle];
    }

    void clear() noexcept
    {
        for(std::size_t i = 0; i < _textureMgr.numActive(); ++i)
        {
            _buffers[i].clear();
        }
    }

    void cleanup() noexcept
    {
        for(std::size_t i = 0; i < _textureMgr.numActive(); ++i)
        {
            _buffers[i].cleanup();
        }
    }

    template <typename F>
    void forActive(F&& f)
    {
        for(std::size_t i = 0; i < _textureMgr.numActive(); ++i)
        {
            _buffers[i].forActive(f);
        }
    }

    template <typename F>
    void forBuffers(F&& f)
    {
        for(std::size_t i = 0; i < _textureMgr.numActive(); ++i)
        {
            if(!_buffers[i].empty())
            {
                f(getTexture(i), getImageData(i), _buffers[i]);
            }
        }
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
void makeParticle(const ParticleTextureManager::Handle txHandle, F&& f)
{
    auto& pBuffer = pMgr.getBuffer(txHandle);

    if(pBuffer.full())
    {
        return;
    }

    f(pBuffer.create());
}

template <typename F>
void makeNParticles(
    const ParticleTextureManager::Handle txHandle, const int count, F&& f)
{
    for(int i = 0; i < count; i++)
    {
        makeParticle(txHandle, f);
    }
}

template <typename F>
void makeNParticlesI(
    const ParticleTextureManager::Handle txHandle, const int count, F&& f)
{
    auto& pBuffer = pMgr.getBuffer(txHandle);

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

ParticleManager pMgr;

ParticleTextureManager::Handle ptxCircle;
ParticleTextureManager::Handle ptxSquare;
ParticleTextureManager::Handle ptxBlob;
ParticleTextureManager::Handle ptxExplosion;
ParticleTextureManager::Handle ptxSmoke;
ParticleTextureManager::Handle ptxBlood;
ParticleTextureManager::Handle ptxBloodMist;

cvar_t r_particles = {"r_particles", "1", CVAR_ARCHIVE}; // johnfitz

template <typename F>
void forActiveParticles(F&& f)
{
    pMgr.forActive(std::forward<F>(f));
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

    {
        buildCircleTexture(particle1_data);
        const ImageData imageData{particle1_data, 64, 64};
        ptxCircle = pMgr.createBuffer(
            makeTextureFromImageData("particle1", imageData), imageData);
    }

    {
        buildSquareTexture(particle2_data);
        const ImageData imageData{particle2_data, 64, 64};
        ptxSquare = pMgr.createBuffer(
            makeTextureFromImageData("particle2", imageData), imageData);
    }

    {
        buildBlobTexture(particle3_data);
        const ImageData imageData{particle3_data, 64, 64};
        ptxBlob = pMgr.createBuffer(
            makeTextureFromImageData("particle3", imageData), imageData);
    }

    const auto load = [&](ParticleTextureManager::Handle& target,
                          const char* name) {
        const auto imageData = loadImage(name);
        target = pMgr.createBuffer(
            makeTextureFromImageData(name, imageData), imageData);
    };

    load(ptxExplosion, "textures/particle_explosion");
    load(ptxSmoke, "textures/particle_smoke");
    load(ptxBlood, "textures/particle_blood");
    load(ptxBloodMist, "textures/particle_blood_mist");
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
}

/*
===============
R_InitParticles
===============
*/
void R_InitParticles()
{
    R_InitRNumParticles();
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

        makeParticle(ptxCircle, [&](particle_t& p) {
            p.alpha = 255;
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
        });
    }
}

/*
===============
R_ClearParticles
===============
*/
void R_ClearParticles()
{
    pMgr.clear();
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

        makeParticle(ptxCircle, [&](particle_t& p) {
            p.alpha = 255;
            p.die = 99999;
            p.color = (-c) & 15;
            p.type = pt_static;
            p.scale = 1.f;
            setAccGrav(p);

            VectorCopy(vec3_origin, p.vel);
            VectorCopy(org, p.org);
        });
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
    makeNParticlesI(ptxCircle, 256, [&](const int i, particle_t& p) {
        p.alpha = 255;
        p.die = cl.time + 4;
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

    makeNParticles(ptxExplosion, 6, [&](particle_t& p) {
        p.alpha = 136;
        p.die = cl.time + 4 * rnd(0.5f, 1.5f);
        p.color = ramp1[0];
        p.ramp = rand() & 3;
        p.scale = rnd(0.5f, 2.1f) * 37.f;
        setAccGrav(p, 0.05f);
        p.type = pt_txexplode;

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + rnd(-14, 14);
            p.vel[j] = rnd(-8, 8);
        }
    });

    makeNParticles(ptxSmoke, 6, [&](particle_t& p) {
        p.alpha = 185;
        p.die = cl.time + 3.5 * (rand() % 5);
        p.color = rand() & 7;
        p.scale = rnd(1.2f, 1.5f);
        p.type = pt_txsmoke;
        setAccGrav(p, -0.09f);

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + ((rand() & 7) - 4);
            p.vel[j] = rnd(-24, 24);
        }
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

    makeNParticles(ptxCircle, 512, [&](particle_t& p) {
        p.alpha = 255;
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
    });
}

/*
===============
R_BlobExplosion
===============
*/
void R_BlobExplosion(vec3_t org)
{
    makeNParticlesI(ptxCircle, 1024, [&](const int i, particle_t& p) {
        p.alpha = 255;
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
    });
}

void R_RunParticleEffect_BulletPuff(
    vec3_t org, vec3_t dir, int color, int count)
{
    const auto debrisCount = count * 0.7f;
    const auto dustCount = count * 0.7f;
    const auto sparkCount = count * 0.4f;

    makeNParticles(ptxCircle, debrisCount, [&](particle_t& p) {
        p.alpha = 255;
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

    makeNParticles(ptxCircle, dustCount, [&](particle_t& p) {
        p.alpha = 255;
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

    makeNParticles(ptxCircle, sparkCount, [&](particle_t& p) {
        p.alpha = 255;
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

    makeNParticles(ptxBlood, count * 2, [&](particle_t& p) {
        p.alpha = 100;
        p.die = cl.time + 0.7 * (rand() % 3);
        p.color = pickBloodColor();
        p.scale = rnd(0.35f, 0.6f) * 6.5f;
        p.type = pt_static;
        setAccGrav(p, 0.29f);

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + rnd(-2, 2);
            p.vel[j] = (dir[j] + 0.3f) * rnd(-10, 10);
        }

        p.vel[2] += rnd(0, 40);
    });

    makeNParticles(ptxCircle, count * 12, [&](particle_t& p) {
        p.alpha = 175;
        p.die = cl.time + 0.4 * (rand() % 3);
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

    makeNParticles(ptxBloodMist, 1, [&](particle_t& p) {
        p.alpha = 38;
        p.die = cl.time + 3.2;
        p.color = 225;
        p.ramp = rand() & 3;
        p.scale = rnd(1.1f, 2.4f) * 15.f;
        setAccGrav(p, -0.03f);
        p.type = pt_txsmoke;

        for(int j = 0; j < 3; j++)
        {
            p.org[j] = org[j] + rnd(-8, 8);
            p.vel[j] = rnd(-4, -4);
        }
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
            makeParticle(ptxCircle, [&](particle_t& p) {
                p.alpha = 255;
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
                makeParticle(ptxCircle, [&](particle_t& p) {
                    p.alpha = 255;
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
                });
            }
        }
    }
}

static void R_SetRTRocketTrail(vec3_t start, particle_t& p)
{
    p.ramp = (rand() & 3);
    p.color = ramp3[(int)p.ramp];
    p.type = pt_fire;
    for(int j = 0; j < 3; j++)
    {
        p.org[j] = start[j] + ((rand() % 6) - 3);
    }
}

static void R_SetRTBlood(vec3_t start, particle_t& p)
{
    p.type = pt_static;
    p.color = 67 + (rand() & 3);
    for(int j = 0; j < 3; j++)
    {
        p.org[j] = start[j] + ((rand() % 6) - 3);
    }
}

static void R_SetRTTracer(vec3_t start, vec3_t end, particle_t& p, int type)
{
    static int tracercount;

    vec3_t vec;
    VectorSubtract(end, start, vec);

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
}

static void R_SetRTSlightBlood(vec3_t start, particle_t& p)
{
    p.type = pt_static;
    p.color = 67 + (rand() & 3);
    for(int j = 0; j < 3; j++)
    {
        p.org[j] = start[j] + ((rand() % 6) - 3);
    }
}

static void R_SetRTVoorTrail(vec3_t start, particle_t& p)
{
    p.color = 9 * 16 + 8 + (rand() & 3);
    p.type = pt_static;
    p.die = cl.time + 0.3;
    for(int j = 0; j < 3; j++)
    {
        p.org[j] = start[j] + ((rand() & 15) - 8);
    }
}

static void R_SetRTCommon(particle_t& p)
{
    p.alpha = 255;
    p.scale = 0.7f;
    setAccGrav(p, 0.05f);
    VectorCopy(vec3_origin, p.vel);
    p.die = cl.time + 2;
}

/*
===============
R_RocketTrail

FIXME -- rename function and use #defined types instead of numbers
===============
*/
void R_RocketTrail(vec3_t start, vec3_t end, int type)
{
    constexpr float rate = 0.1f / 9.f;
    static float untilNext = rate;
    const float frametime = cl.time - cl.oldtime;

    untilNext -= frametime;
    if(untilNext > 0)
    {
        return;
    }

    untilNext = rate;

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

        switch(type)
        {
            case 0: // rocket trail
            {
                makeParticle(ptxCircle, [&](particle_t& p) {
                    R_SetRTCommon(p);
                    R_SetRTRocketTrail(start, p);
                });

                makeNParticles(ptxSmoke, 1, [&](particle_t& p) {
                    p.alpha = 65;
                    p.die = cl.time + 1.5 * (rand() % 5);
                    p.color = rand() & 7;
                    p.scale = rnd(0.3f, 0.5f);
                    p.type = pt_txsmoke;
                    setAccGrav(p, -0.09f);

                    for(int j = 0; j < 3; j++)
                    {
                        p.org[j] = start[j] + ((rand() & 6) - 3);
                        p.vel[j] = rnd(-18, 18);
                    }
                });

                break;
            }

            case 1: // smoke smoke
            {
                makeNParticles(ptxSmoke, 1, [&](particle_t& p) {
                    p.alpha = 65;
                    p.die = cl.time + 1.5 * (rand() % 5);
                    p.color = rand() & 7;
                    p.scale = rnd(0.3f, 0.5f);
                    p.type = pt_txsmoke;
                    setAccGrav(p, -0.09f);

                    for(int j = 0; j < 3; j++)
                    {
                        p.org[j] = start[j] + ((rand() & 6) - 3);
                        p.vel[j] = rnd(-18, 18);
                    }
                });

                break;
            }

            case 2: // blood
            {
                makeParticle(ptxCircle, [&](particle_t& p) {
                    R_SetRTCommon(p);
                    R_SetRTBlood(start, p);
                });

                makeParticle(ptxBloodMist, [&](particle_t& p) {
                    p.alpha = 32;
                    p.die = cl.time + 3.2;
                    p.color = 225;
                    p.ramp = rand() & 3;
                    p.scale = rnd(1.1f, 2.4f) * 15.f;
                    setAccGrav(p, -0.03f);
                    p.type = pt_txsmoke;

                    for(int j = 0; j < 3; j++)
                    {
                        p.org[j] = start[j] + rnd(-8, 8);
                        p.vel[j] = rnd(-4, -4);
                    }
                });

                break;
            }

            case 3: [[fallthrough]];
            case 5: // tracer
            {
                makeParticle(ptxCircle, [&](particle_t& p) {
                    R_SetRTCommon(p);
                    R_SetRTTracer(start, end, p, type);
                });

                break;
            }

            case 4: // slight blood
            {
                makeParticle(ptxCircle, [&](particle_t& p) {
                    R_SetRTCommon(p);
                    R_SetRTSlightBlood(start, p);
                    len -= 3;
                });

                break;
            }

            case 6: // voor trail
            {
                makeParticle(ptxCircle, [&](particle_t& p) {
                    R_SetRTCommon(p);
                    R_SetRTVoorTrail(start, p);
                });

                break;
            }
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

    pMgr.cleanup();

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

            case pt_txexplode:
            {
                if(p.alpha > 0.f)
                {
                    p.alpha -= 90.f * frametime;
                }

                if(p.scale > 0.f)
                {
                    p.scale += 45.f * frametime;
                }

                break;
            }

            case pt_txsmoke:
            {
                if(p.alpha > 0.f)
                {
                    p.alpha -= 75.f * frametime;
                }

                if(p.scale > 0.f)
                {
                    p.scale += 47.f * frametime;
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
    if(!r_particles.value)
    {
        return;
    }

    vec3_t up;
    VectorScale(vup, 1.5, up);

    vec3_t right;
    VectorScale(vright, 1.5, right);

    vec3_t down;
    down[0] = up[0] * -1.f;
    down[1] = up[1] * -1.f;
    down[2] = up[2] * -1.f;

    vec3_t left;
    left[0] = right[0] * -1.f;
    left[1] = right[1] * -1.f;
    left[2] = right[2] * -1.f;

    pMgr.forBuffers([&](gltexture_t* texture, const ImageData& imageData,
                        ParticleBuffer& pBuffer) {
        (void)imageData;

        // TODO VR:
        GL_Bind(texture);
        glEnable(GL_BLEND);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        glDepthMask(GL_FALSE); // johnfitz -- fix for particle z-buffer bug

        glBegin(GL_QUADS);
        pBuffer.forActive([&](particle_t& p) {
            const float scale = p.scale;

            // johnfitz -- particle transparency and fade out
            GLubyte* c = (GLubyte*)&d_8to24table[(int)p.color];

            GLubyte color[4];
            color[0] = c[0];
            color[1] = c[1];
            color[2] = c[2];
            color[3] = p.alpha;

            glColor4ubv(color);
            // johnfitz

            vec3_t p_upleft;
            VectorMA(p.org, scale / 2.f, up, p_upleft);
            VectorMA(p_upleft, scale / 2.f, left, p_upleft);

            vec3_t p_upright;
            VectorMA(p.org, scale / 2.f, up, p_upright);
            VectorMA(p_upright, scale / 2.f, right, p_upright);

            vec3_t p_downleft;
            VectorMA(p.org, scale / 2.f, down, p_downleft);
            VectorMA(p_downleft, scale / 2.f, left, p_downleft);

            vec3_t p_downright;
            VectorMA(p.org, scale / 2.f, down, p_downright);
            VectorMA(p_downright, scale / 2.f, right, p_downright);

            glTexCoord2f(0, 0);
            glVertex3fv(p_downleft);

            glTexCoord2f(1, 0);
            glVertex3fv(p_upleft);

            glTexCoord2f(1, 1);
            glVertex3fv(p_upright);

            glTexCoord2f(0, 1);
            glVertex3fv(p_downright);
        });
        glEnd();

        glDepthMask(GL_TRUE); // johnfitz -- fix for particle z-buffer bug
        glDisable(GL_BLEND);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glColor3f(1, 1, 1);
    });
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
        const float scale = p.scale;

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
