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
Foundation, Inc., 59 Temple Place - Suite 430, Boston, MA  02111-1307, USA.

*/

#include "quakedef.hpp"
#include "sys.hpp"
#include "util.hpp"
#include "quakeglm.hpp"
#include "quakeglm_qvec3_togl.hpp"
#include "quakeglm_qvec4.hpp"
#include "common.hpp"
#include "console.hpp"
#include "quakedef_macros.hpp"
#include "image.hpp"
#include "shader.hpp"
#include "msg.hpp"

#include <algorithm>
#include <random>
#include <string_view>
#include <utility>
#include <array>

#define MAX_PARTICLES 4096 * 100 // default max # of particles at once
// time, per texture (TODO VR: (P2) should it be per texture?, cvar)

#define ABSOLUTE_MIN_PARTICLES \
    512 // no fewer than this no matter what's
        // on the command line

// These "ramps" below are for colors..

// Gold/brown/peach
constexpr int ramp1[8] = {111, 112, 107, 105, 103, 101, 99, 97};

// Gold/brown
constexpr int ramp2[8] = {111, 110, 109, 108, 107, 106, 104, 102};

// Gold/brown/grey
constexpr int ramp3[8] = {109, 107, 6, 5, 4, 3};

enum ptype_t : std::uint8_t
{
    pt_static,
    pt_fire,
    pt_explode,
    pt_explode2,
    pt_blob,
    pt_blob2,
    pt_txexplode,
    pt_txsmoke,
    pt_lightning,
    pt_teleport,
    pt_rock,
    pt_gunsmoke,
    pt_gunpickup,
    pt_txbigsmoke,
};

struct ParticleSOA
{
    struct Data
    {
        float _ramp;
        float _die;
        ptype_t _type;
        std::uint8_t _param0;
    };

    qvec3* _orgs;
    qvec3* _vels;
    qvec3* _accs;
    qvec4* _colors;
    float* _angles;
    float* _scales;
    int* _atlasIdxs;
    Data* _datas;
};

struct ParticleHandleSOA
{
    ParticleSOA* _soa;
    std::size_t _idx;

    [[nodiscard]] QUAKE_FORCEINLINE qvec3& org() noexcept
    {
        return _soa->_orgs[_idx];
    }

    [[nodiscard]] QUAKE_FORCEINLINE qvec3& vel() noexcept
    {
        return _soa->_vels[_idx];
    }

    [[nodiscard]] QUAKE_FORCEINLINE qvec3& acc() noexcept
    {
        return _soa->_accs[_idx];
    }

    QUAKE_FORCEINLINE void setColor(const float x) noexcept
    {
        GLubyte* c = (GLubyte*)&d_8to24table[(int)x];

        _soa->_colors[_idx].rgb =
            qvec3{c[0] / 255.f, c[1] / 255.f, c[2] / 255.f};
    }

    [[nodiscard]] QUAKE_FORCEINLINE float& ramp() noexcept
    {
        return _soa->_datas[_idx]._ramp;
    }

    [[nodiscard]] QUAKE_FORCEINLINE float& die() noexcept
    {
        return _soa->_datas[_idx]._die;
    }

    [[nodiscard]] QUAKE_FORCEINLINE float& scale() noexcept
    {
        return _soa->_scales[_idx];
    }

    QUAKE_FORCEINLINE void setAlpha(const float x) noexcept
    {
        _soa->_colors[_idx].a = x / 255.f;
    }

    QUAKE_FORCEINLINE void addAlpha(const float x, const float ft) noexcept
    {
        _soa->_colors[_idx].a += (x / 255.f) * ft;
    }

    [[nodiscard]] QUAKE_FORCEINLINE float& angle() noexcept
    {
        return _soa->_angles[_idx];
    }

    [[nodiscard]] QUAKE_FORCEINLINE int& atlasIdx() noexcept
    {
        return _soa->_atlasIdxs[_idx];
    }

    [[nodiscard]] QUAKE_FORCEINLINE ptype_t& type() noexcept
    {
        return _soa->_datas[_idx]._type;
    }

    [[nodiscard]] QUAKE_FORCEINLINE std::uint8_t& param0() noexcept
    {
        return _soa->_datas[_idx]._param0;
    }
};

template <class UnaryPredicate>
[[nodiscard]] constexpr std::size_t index_find_if(
    std::size_t first, std::size_t last, UnaryPredicate&& p)
{
    for(; first != last; ++first)
    {
        if(p(first))
        {
            return first;
        }
    }

    return last;
}

template <class UnaryPredicate, class F>
[[nodiscard]] std::size_t index_remove_if(
    std::size_t first, std::size_t last, UnaryPredicate&& p, F&& f)
{
    first = index_find_if(first, last, p);

    if(first != last)
    {
        for(std::size_t i = first; ++i != last;)
        {
            if(!p(i))
            {
                f(first, i);
                ++first;
            }
        }
    }

    return first;
}

class ParticleBufferSOA
{
private:
    ParticleSOA _pSOA;
    std::size_t _aliveCount;
    std::size_t _maxParticles;

public:
    void initialize(const std::size_t maxParticles) noexcept
    {
        _aliveCount = 0;
        _maxParticles = maxParticles;

        const auto alloc = [&](auto& field, const char* name) {
            using Type = std::remove_pointer_t<std::decay_t<decltype(field)>>;
            field = Hunk_AllocName<Type>(_maxParticles, name);
        };

        alloc(_pSOA._orgs, "psoa_orgs");
        alloc(_pSOA._vels, "psoa_vels");
        alloc(_pSOA._accs, "psoa_accs");
        alloc(_pSOA._colors, "psoa_colors");
        alloc(_pSOA._angles, "psoa_angles");
        alloc(_pSOA._scales, "psoa_scales");
        alloc(_pSOA._atlasIdxs, "psoa_atlasIdxs");
        alloc(_pSOA._datas, "psoa_datas");
    }

    void cleanup() noexcept
    {
        _aliveCount = index_remove_if(
            0, _aliveCount,
            [&](const std::size_t i) {
                return _pSOA._colors[i].a <= 0.f  //
                       || _pSOA._scales[i] <= 0.f //
                       || cl.time >= _pSOA._datas[i]._die;
            },
            [&](const std::size_t targetIdx, const std::size_t srcIdx) {
                _pSOA._orgs[targetIdx] = std::move(_pSOA._orgs[srcIdx]);
                _pSOA._vels[targetIdx] = std::move(_pSOA._vels[srcIdx]);
                _pSOA._accs[targetIdx] = std::move(_pSOA._accs[srcIdx]);
                _pSOA._colors[targetIdx] = std::move(_pSOA._colors[srcIdx]);
                _pSOA._angles[targetIdx] = std::move(_pSOA._angles[srcIdx]);
                _pSOA._scales[targetIdx] = std::move(_pSOA._scales[srcIdx]);
                _pSOA._atlasIdxs[targetIdx] =
                    std::move(_pSOA._atlasIdxs[srcIdx]);
                _pSOA._datas[targetIdx] = std::move(_pSOA._datas[srcIdx]);
            });
    }

    [[nodiscard]] QUAKE_FORCEINLINE ParticleHandleSOA create() noexcept
    {
        return {&_pSOA, _aliveCount++};
    }

    template <typename F>
    QUAKE_FORCEINLINE void forActive(F&& f) noexcept
    {
        for(std::size_t i = 0; i < _aliveCount; ++i)
        {
            f(ParticleHandleSOA{&_pSOA, i});
        }
    }

    [[nodiscard]] QUAKE_FORCEINLINE bool full() const noexcept
    {
        return _aliveCount == _maxParticles;
    }

    void clear() noexcept
    {
        _aliveCount = 0;
    }

    [[nodiscard]] QUAKE_FORCEINLINE bool empty() const noexcept
    {
        return _aliveCount == 0;
    }

    [[nodiscard]] QUAKE_FORCEINLINE std::size_t aliveCount() const noexcept
    {
        return _aliveCount;
    }

    ParticleSOA& soa() noexcept
    {
        return _pSOA;
    }
};

struct ImageData
{
    byte* data; // Hunk-allocated.
    int width;
    int height;
};

ImageData stitchImageData(const ImageData& a, const ImageData& b)
{
    const auto width = a.width + b.width;
    const auto height = std::max(a.height, b.height);
    const auto numPixels = width * height;
    const auto numBytes = numPixels * 4;

    const auto idx = [](const int width, const int height, const int depth,
                         const int x, const int y, const int z) {
        return (width * y + x) * 4 + z;
        return x + height * (y + width * z);
        return x + width * (y + depth * z);
    };

    byte* data = Hunk_Alloc<byte>(numBytes);

    const auto doPic = [&](const int xOffset, const auto& pic) {
        for(int x = 0; x < pic.width; ++x)
        {
            for(int y = 0; y < pic.height; ++y)
            {
                for(int z = 0; z < 4; ++z)
                {
                    data[idx(width, height, 4, xOffset + x, y, z)] =
                        pic.data[idx(pic.width, pic.height, 4, x, y, z)];
                }
            }
        }
    };

    doPic(0, a);
    doPic(a.width, b);

    return ImageData{data, width, height};
}

struct AtlasData
{
    ImageData _imageData;
    std::vector<float> _imageInfo; // x, y, width, height
};

template <typename... Ts>
[[nodiscard]] AtlasData stitchImages(const Ts&... images)
{
    const auto width = (images.width + ...);
    const auto height = std::max({images.height...});
    const auto numPixels = width * height;
    const auto numBytes = numPixels * 4;

    const auto idx = [](const int width, const int depth, const int x,
                         const int y,
                         const int z) { return (width * y + x) * depth + z; };

    byte* data = Hunk_Alloc<byte>(numBytes);
    std::vector<float> imageInfo;

    const auto blit = [&](const int xOffset, const auto& pic) {
        imageInfo.emplace_back(xOffset);
        imageInfo.emplace_back(0);
        imageInfo.emplace_back(pic.width);
        imageInfo.emplace_back(pic.height);

        for(int x = 0; x < pic.width; ++x)
        {
            for(int y = 0; y < pic.height; ++y)
            {
                for(int z = 0; z < 4; ++z)
                {
                    data[idx(width, 4, xOffset + x, y, z)] =
                        pic.data[idx(pic.width, 4, x, y, z)];
                }
            }
        }
    };

    int xOffset = 0;
    ((blit(xOffset, images), xOffset += images.width), ...);

    return AtlasData{ImageData{data, width, height}, std::move(imageInfo)};
}

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

    [[nodiscard]] QUAKE_FORCEINLINE std::size_t numActive() const noexcept
    {
        return _next;
    }
};

int r_numparticles;

using PBuffer = ParticleBufferSOA;
using PHandle = ParticleHandleSOA;

class ParticleManager
{
public:
    using Handle = ParticleTextureManager::Handle;

private:
    ParticleTextureManager _textureMgr;
    std::array<PBuffer, ParticleTextureManager::maxTextures> _buffers;

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

    [[nodiscard]] PBuffer& getBuffer(
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
    void forActive(F&& f) noexcept
    {
        for(std::size_t i = 0; i < _textureMgr.numActive(); ++i)
        {
            _buffers[i].forActive(f);
        }
    }

    template <typename F>
    void forBuffers(F&& f) noexcept
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

ParticleManager pMgr;

std::random_device rd;
std::mt19937 mt(rd());

[[nodiscard]] QUAKE_FORCEINLINE static float rnd(
    const float min, const float max) noexcept
{
    return std::uniform_real_distribution<float>{min, max}(mt);
}

[[nodiscard]] QUAKE_FORCEINLINE static int rndi(
    const int min, const int max) noexcept
{
    return std::uniform_int_distribution<int>{min, max - 1}(mt);
}

[[nodiscard]] QUAKE_FORCEINLINE static float rndToRad(
    const float min, const float max) noexcept
{
    return rnd(min * M_PI_DIV_180, max * M_PI_DIV_180);
}

cvar_t r_particles = {"r_particles", "1", CVAR_ARCHIVE}; // johnfitz
cvar_t r_particle_mult = {"r_particle_mult", "1", CVAR_ARCHIVE};

template <typename F>
QUAKE_FORCEINLINE void makeNParticlesI(
    const ParticleTextureManager::Handle txHandle, const int count,
    F&& f) noexcept
{
    auto& pBuffer = pMgr.getBuffer(txHandle);

    for(int i = 0; i < count * r_particle_mult.value; i++)
    {
        if(pBuffer.full())
        {
            return;
        }

        f(i, pBuffer.create());
    }
}


template <typename F>
QUAKE_FORCEINLINE void makeNParticles(
    const ParticleTextureManager::Handle txHandle, const int count,
    F&& f) noexcept
{
    makeNParticlesI(txHandle, count, [&f](const int, PHandle p) { f(p); });
}

QUAKE_FORCEINLINE void setAccGrav(PHandle p, float mult = 0.5f) noexcept
{
    extern cvar_t sv_gravity;

    p.acc()[0] = 0.f;
    p.acc()[1] = 0.f;
    p.acc()[2] = -sv_gravity.value * mult;
}

ParticleTextureManager::Handle ptxAtlas;
AtlasData ptxAtlasData;

constexpr int aiCircle = 0;
// constexpr int aiSquare = 1;
// constexpr int aiBlob = 2;
constexpr int aiExplosion = 3;
constexpr int aiSmoke = 4;
constexpr int aiBlood = 5;
constexpr int aiBloodMist = 6;
constexpr int aiLightning = 7;
constexpr int aiSpark = 8;
constexpr int aiRock = 9;
constexpr int aiGunSmoke = 10;

template <typename F>
QUAKE_FORCEINLINE void forActiveParticles(F&& f) noexcept
{
    // TODO VR: (P2) parallelize with thread pool
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
    byte* const data = Image_LoadImage(filename, &width, &height);

    if(data == nullptr)
    {
        Sys_Error("Could not load image '%s'\n", filename);
    }

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

    const auto circleImageData = [&] {
        buildCircleTexture(particle1_data);
        return ImageData{particle1_data, 64, 64};
    }();

    const auto squareImageData = [&] {
        buildSquareTexture(particle2_data);
        return ImageData{particle2_data, 2, 2};
    }();

    const auto blobImageData = [&] {
        buildBlobTexture(particle3_data);
        return ImageData{particle3_data, 64, 64};
    }();

    ptxAtlasData = stitchImages(                   //
        circleImageData,                           //
        squareImageData,                           //
        blobImageData,                             //
        loadImage("textures/particle_explosion"),  //
        loadImage("textures/particle_smoke"),      //
        loadImage("textures/particle_blood"),      //
        loadImage("textures/particle_blood_mist"), //
        loadImage("textures/particle_lightning"),  //
        loadImage("textures/particle_spark"),      //
        loadImage("textures/particle_rock"),       //
        loadImage("textures/particle_gun_smoke")   //
    );

    ptxAtlas = pMgr.createBuffer(
        makeTextureFromImageData("atlas", ptxAtlasData._imageData),
        ptxAtlasData._imageData);
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
    Cvar_RegisterVariable(&r_particle_mult);
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
qvec3 avelocities[NUMVERTEXNORMALS];
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

        qvec3 forward;
        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;

        makeNParticles(ptxAtlas, 1, [&](PHandle p) {
            p.atlasIdx() = aiCircle;
            p.angle() = rndToRad(0.f, 360.f);
            p.setAlpha(255);
            p.die() = cl.time + 0.01;
            p.setColor(0x6f);
            p.type() = pt_explode;
            p.scale() = 1.f;
            setAccGrav(p);

            constexpr float dist = 64;
            p.org()[0] = ent->origin[0] + r_avertexnormals[i][0] * dist +
                         forward[0] * beamlength;
            p.org()[1] = ent->origin[1] + r_avertexnormals[i][1] * dist +
                         forward[1] * beamlength;
            p.org()[2] = ent->origin[2] + r_avertexnormals[i][2] * dist +
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
        qvec3 org;
        const int r = fscanf(f, "%f %f %f\n", &org[0], &org[1], &org[2]);

        if(r != 3)
        {
            break;
        }

        c++;

        makeNParticles(ptxAtlas, 1, [&](PHandle p) {
            p.atlasIdx() = aiCircle;
            p.angle() = rndToRad(0.f, 360.f);
            p.setAlpha(255);
            p.die() = 99999;
            p.setColor((-c) & 15);
            p.type() = pt_static;
            p.scale() = 1.f;
            setAccGrav(p);

            p.vel() = vec3_zero;
            p.org() = org;
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
    const auto org = MSG_ReadVec3(cl.protocolflags);

    qvec3 dir;
    for(int i = 0; i < 3; i++)
    {
        dir[i] = MSG_ReadChar() * (1.0 / 16);
    }

    const int msgcount = MSG_ReadByte();
    const int color = MSG_ReadByte();

    R_RunParticleEffect_BulletPuff(org, dir, color, msgcount);
}

/*
===============
R_ParseParticle2Effect

Parse an effect out of the server message (preset-based)
===============
*/
void R_ParseParticle2Effect()
{
    const auto org = MSG_ReadVec3(cl.protocolflags);

    qvec3 dir;
    for(int i = 0; i < 3; i++)
    {
        dir[i] = MSG_ReadChar() * (1.0 / 16);
    }

    const int preset = MSG_ReadByte();
    const int msgcount = MSG_ReadShort();

    R_RunParticle2Effect(org, dir, preset, msgcount);
}

/*
===============
R_ParticleExplosion
===============
*/
void R_ParticleExplosion(const qvec3& org)
{
    makeNParticlesI(ptxAtlas, 256, [&](const int i, PHandle p) {
        p.atlasIdx() = aiCircle;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.5;
        p.setColor(ramp1[0]);
        p.ramp() = rand() & 3;
        p.scale() = rnd(0.6f, 1.2f);
        setAccGrav(p);
        p.type() = i & 1 ? pt_explode : pt_explode2;

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-16, 16);
            p.vel()[j] = rnd(-256, 256);
        }
    });

    makeNParticlesI(ptxAtlas, 64, [&](const int, PHandle p) {
        p.atlasIdx() = aiSpark;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.5;
        p.setColor(ramp1[0]);
        p.ramp() = rand() & 3;
        p.scale() = rnd(1.9f, 2.9f) * 0.55f;
        setAccGrav(p);
        p.type() = pt_rock;
        p.param0() = rndi(0, 2); // rotation direction

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-16, 16);
            p.vel()[j] = rnd(-256, 256);
        }
    });

    makeNParticlesI(ptxAtlas, 48, [&](const int, PHandle p) {
        p.atlasIdx() = aiRock;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.5;
        p.setColor(167 + (rand() & 7));
        p.ramp() = rand() & 3;
        p.scale() = rnd(0.9f, 1.9f);
        setAccGrav(p);
        p.type() = pt_rock;
        p.param0() = rndi(0, 2); // rotation direction

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-16, 16);
            p.vel()[j] = rnd(-256, 256);
        }
    });

    makeNParticles(ptxAtlas, 1, [&](PHandle p) {
        p.atlasIdx() = aiExplosion;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.5;
        p.setColor(ramp1[0]);
        p.ramp() = rand() & 3;
        p.scale() = rnd(0.5f, 2.1f) * 2.f;
        setAccGrav(p, 0.05f);
        p.type() = pt_txexplode;
        p.param0() = rndi(0, 2); // rotation direction

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-11, 11);
            p.vel()[j] = rnd(-8, 8);
        }
    });

    makeNParticles(ptxAtlas, 3, [&](PHandle p) {
        p.atlasIdx() = aiSmoke;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(225);
        p.die() = cl.time + 3.5;
        p.setColor(rand() & 7);
        p.scale() = rnd(1.2f, 1.5f);
        p.type() = pt_txsmoke;
        setAccGrav(p, -0.09f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 7) - 4);
            p.vel()[j] = rnd(-24, 24);
        }
    });
}

/*
===============
R_ParticleExplosion2
===============
*/
void R_ParticleExplosion2(const qvec3& org, int colorStart, int colorLength)
{
    int colorMod = 0;

    makeNParticlesI(ptxAtlas, 256, [&](const int, PHandle p) {
        p.atlasIdx() = aiCircle;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.5;
        p.setColor(colorStart + (colorMod % colorLength));
        p.scale() = rnd(1.6f, 3.5f);
        colorMod++;
        setAccGrav(p);
        p.type() = pt_blob;

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-16, 16);
            p.vel()[j] = rnd(-256, 256);
        }
    });

    makeNParticlesI(ptxAtlas, 64, [&](const int, PHandle p) {
        p.atlasIdx() = aiSpark;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.5;
        p.setColor(ramp1[0]);
        p.ramp() = rand() & 3;
        p.scale() = rnd(1.9f, 2.9f) * 0.55f;
        setAccGrav(p);
        p.type() = pt_rock;
        p.param0() = rndi(0, 2); // rotation direction

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-16, 16);
            p.vel()[j] = rnd(-256, 256);
        }
    });

    makeNParticlesI(ptxAtlas, 48, [&](const int, PHandle p) {
        p.atlasIdx() = aiRock;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.5;
        p.setColor(167 + (rand() & 7));
        p.ramp() = rand() & 3;
        p.scale() = rnd(0.9f, 1.9f);
        setAccGrav(p);
        p.type() = pt_rock;
        p.param0() = rndi(0, 2); // rotation direction

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-16, 16);
            p.vel()[j] = rnd(-256, 256);
        }
    });

    makeNParticles(ptxAtlas, 1, [&](PHandle p) {
        p.atlasIdx() = aiExplosion;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.5;
        p.setColor(colorStart + (colorMod % colorLength));
        p.ramp() = rand() & 3;
        p.scale() = rnd(0.5f, 2.1f) * 2.f;
        setAccGrav(p, 0.05f);
        p.type() = pt_txexplode;
        p.param0() = rndi(0, 2); // rotation direction

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-11, 11);
            p.vel()[j] = rnd(-8, 8);
        }
    });

    makeNParticles(ptxAtlas, 3, [&](PHandle p) {
        p.atlasIdx() = aiSmoke;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(225);
        p.die() = cl.time + 3.5;
        p.setColor(rand() & 7);
        p.scale() = rnd(1.2f, 1.5f);
        p.type() = pt_txsmoke;
        setAccGrav(p, -0.09f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 7) - 4);
            p.vel()[j] = rnd(-24, 24);
        }
    });
}

void R_RunParticleEffect_BulletPuff(
    const qvec3& org, const qvec3& dir, int color, int count)
{
    const auto debrisCount = count * 0.7f;
    const auto dustCount = count * 0.7f;
    const auto sparkCount = count * 0.4f;

    makeNParticles(ptxAtlas, debrisCount, [&](PHandle p) {
        p.atlasIdx() = aiRock;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 0.7 * (rand() % 5);
        p.setColor((color & ~7) + (rand() & 7));
        p.scale() = rnd(0.5f, 0.9f);
        p.type() = pt_rock;
        p.param0() = rndi(0, 2); // rotation direction
        setAccGrav(p, 0.26f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 7) - 4);
            p.vel()[j] = (dir[j] + 0.3f) * rnd(-75, 75);
        }
    });

    makeNParticles(ptxAtlas, 1, [&](PHandle p) {
        p.atlasIdx() = aiSmoke;
        p.setAlpha(45);
        p.die() = cl.time + 1.25 * (rand() % 5);
        p.setColor(rand() & 7);
        p.scale() = rnd(0.3f, 0.5f);
        p.type() = pt_txsmoke;
        setAccGrav(p, -0.09f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 4) - 2);
            p.vel()[j] = rnd(-12, 12);
        }
    });

    makeNParticles(ptxAtlas, dustCount, [&](PHandle p) {
        p.atlasIdx() = aiCircle;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 0.75 * (rand() % 5);
        p.setColor((color & ~7) + (rand() & 7));
        p.scale() = rnd(0.05f, 0.3f);
        p.type() = pt_static;
        setAccGrav(p, 0.08f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 7) - 4);
            p.vel()[j] = rnd(-24, 24);
        }

        p.vel()[2] += rnd(10, 40);
    });

    makeNParticles(ptxAtlas, sparkCount, [&](PHandle p) {
        p.atlasIdx() = aiSpark;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 1.25 * (rand() % 5);
        p.setColor(ramp3[0] + (rand() & 7));
        p.scale() = rnd(1.95f, 2.87f) * 0.35f;
        p.type() = pt_rock;
        p.param0() = rndi(0, 2); // rotation direction
        setAccGrav(p, 1.f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 7) - 4);
            p.vel()[j] = rnd(-48, 48);
        }

        p.vel()[2] = rnd(60, 360);
    });
}

void R_RunParticleEffect_Blood(const qvec3& org, const qvec3& dir, int count)
{
    constexpr int bloodColors[]{247, 248, 249, 250, 251};
    const auto pickBloodColor = [&] { return bloodColors[rndi(0, 5)]; };

    makeNParticles(ptxAtlas, count * 2, [&](PHandle p) {
        p.atlasIdx() = aiBlood;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(100);
        p.die() = cl.time + 0.7 * (rand() % 3);
        p.setColor(pickBloodColor());
        p.scale() = rnd(0.35f, 0.6f) * 6.5f;
        p.type() = pt_static;
        setAccGrav(p, 0.29f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-2, 2);
            p.vel()[j] = (dir[j] + 0.3f) * rnd(-10, 10);
        }

        p.vel()[2] += rnd(0, 40);
    });

    makeNParticles(ptxAtlas, count * 24, [&](PHandle p) {
        p.atlasIdx() = aiCircle;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(175);
        p.die() = cl.time + 0.4 * (rand() % 3);
        p.setColor(pickBloodColor());
        p.scale() = rnd(0.12f, 0.2f);
        p.type() = pt_static;
        setAccGrav(p, 0.45f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-2, 2);
            p.vel()[j] = (dir[j] + 0.3f) * rnd(-3, 3);
            p.vel()[j] *= 13.f;
        }

        p.vel()[2] += rnd(20, 60);
    });

    makeNParticles(ptxAtlas, 1, [&](PHandle p) {
        p.atlasIdx() = aiBloodMist;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(38);
        p.die() = cl.time + 2.0;
        p.setColor(225);
        p.ramp() = rand() & 3;
        p.scale() = rnd(1.1f, 2.4f) * 15.f;
        setAccGrav(p, -0.03f);
        p.type() = pt_txsmoke;

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-8, 8);
            p.vel()[j] = rnd(-4, -4);
        }
    });
}

void R_RunParticleEffect_Lightning(
    const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiLightning;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(rnd(180, 220));
        p.die() = cl.time + 1.2 * (rand() % 3);
        p.setColor(254);
        p.scale() = rnd(0.35f, 0.6f) * 6.2f;
        p.type() = pt_lightning;
        setAccGrav(p, 0.f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + rnd(-3, 3);
            p.vel()[j] = rnd(-185, 185);
        }
    });
}

void R_RunParticleEffect_Smoke(const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiSmoke;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(125);
        p.die() = cl.time + 2.5 * (rand() % 5);
        p.setColor(rand() & 7);
        p.scale() = rnd(1.2f, 1.5f) * 0.8f;
        p.type() = pt_txsmoke;
        setAccGrav(p, -0.09f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 7) - 4);
            p.vel()[j] = rnd(-24, 24);
        }
    });
}

void R_RunParticleEffect_BigSmoke(const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiSmoke;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 2.5 * (rand() % 5);
        p.setColor(rand() & 7);
        p.scale() = rnd(1.2f, 1.7f) * 2.8f;
        p.type() = pt_txbigsmoke;
        setAccGrav(p, -0.09f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 9) - 4);
            p.vel()[j] = rnd(-24, 24);
        }
    });
}

void R_RunParticleEffect_Sparks(const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiSpark;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 2.0 * (rand() % 5);
        p.setColor(rndi(102, 112));
        p.scale() = rnd(1.55f, 2.87f) * 0.45f;
        p.type() = pt_rock;
        p.param0() = rndi(0, 2); // rotation direction
        setAccGrav(p, 1.f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 7) - 4);
            p.vel()[j] = rnd(-48, 48);
        }

        p.vel()[2] = rnd(60, 360);
    });
}

void R_RunParticleEffect_GunSmoke(const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiGunSmoke;
        p.angle() = rndToRad(-10.f, 10.f) + (90 * M_PI_DIV_180);
        p.setAlpha(rnd(85, 125));
        p.die() = cl.time + 3.5;
        p.setColor(rndi(10, 16));
        p.scale() = rnd(0.9f, 1.5f) * 0.1f;
        p.type() = pt_gunsmoke;
        setAccGrav(p, -0.09f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j];
            p.vel()[j] = rnd(-3, 3);
        }

        p.org()[2] += 3.f;
    });
}

void R_RunParticleEffect_Teleport(const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiSpark;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(255);
        p.die() = cl.time + 0.6;
        p.setColor(rndi(208, 220));
        p.scale() = rnd(1.55f, 2.87f) * 0.65f;
        p.type() = pt_rock;
        p.param0() = rndi(0, 2); // rotation direction
        setAccGrav(p, 1.f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 7) - 4);
            p.vel()[j] = rnd(-48, 48);
        }

        p.vel()[2] = rnd(60, 360);
    });
}

void R_RunParticleEffect_GunPickup(
    const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiSpark;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(rnd(150, 200));
        p.die() = cl.time + 0.5;
        p.setColor(rndi(12, 16));
        p.scale() = rnd(1.55f, 2.87f) * 0.35f;
        p.type() = pt_gunpickup;
        setAccGrav(p, -0.2f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 3) - 2);
            p.vel()[j] = rnd(-16, 16);
        }

        p.vel()[2] = rnd(-10, 30);
    });
}

void R_RunParticleEffect_GunForceGrab(
    const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiSpark;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(rnd(180, 225));
        p.die() = cl.time + 0.6;
        p.setColor(rndi(106, 111));
        p.scale() = rnd(1.55f, 2.87f) * 0.35f;
        p.type() = pt_gunpickup;
        setAccGrav(p, -0.3f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + ((rand() & 4) - 2);
            p.vel()[j] = rnd(-24, 24);
        }

        p.vel()[2] = rnd(0, 40);
    });
}

void R_RunParticleEffect_LavaSpike(
    const qvec3& org, const qvec3& dir, int count)
{
    (void)dir;

    makeNParticles(ptxAtlas, count, [&](PHandle p) {
        p.atlasIdx() = aiSpark;
        p.angle() = rndToRad(0.f, 360.f);
        p.setAlpha(rnd(180, 225));
        p.die() = cl.time + 0.5;
        p.setColor(rndi(247, 254));
        p.scale() = rnd(1.55f, 2.87f) * 0.17f;
        p.type() = pt_gunpickup;
        setAccGrav(p, 0.17f);

        for(int j = 0; j < 3; j++)
        {
            p.org()[j] = org[j] + static_cast<float>(((rand() & 2) - 1)) * 0.3f;
            p.vel()[j] = rnd(-2, 2);
        }
    });
}

/*
===============
R_RunParticle2Effect
===============
*/
void R_RunParticle2Effect(
    const qvec3& org, const qvec3& dir, int preset, int count)
{
    enum class Preset : int
    {
        BulletPuff = 0,
        Blood = 1,
        Explosion = 2,
        Lightning = 3,
        Smoke = 4,
        Sparks = 5,
        GunSmoke = 6,
        Teleport = 7,
        GunPickup = 8,
        GunForceGrab = 9,
        LavaSpike = 10,
        BigSmoke = 11
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
        case Preset::Explosion:
        {
            R_ParticleExplosion(org);
            break;
        }
        case Preset::Lightning:
        {
            R_RunParticleEffect_Lightning(org, dir, count);
            break;
        }
        case Preset::Smoke:
        {
            R_RunParticleEffect_Smoke(org, dir, count);
            break;
        }
        case Preset::Sparks:
        {
            R_RunParticleEffect_Sparks(org, dir, count);
            break;
        }
        case Preset::GunSmoke:
        {
            R_RunParticleEffect_GunSmoke(org, dir, count);
            break;
        }
        case Preset::Teleport:
        {
            R_RunParticleEffect_Teleport(org, dir, count);
            break;
        }
        case Preset::GunPickup:
        {
            R_RunParticleEffect_GunPickup(org, dir, count);
            break;
        }
        case Preset::GunForceGrab:
        {
            R_RunParticleEffect_GunForceGrab(org, dir, count);
            break;
        }
        case Preset::LavaSpike:
        {
            R_RunParticleEffect_LavaSpike(org, dir, count);
            break;
        }
        case Preset::BigSmoke:
        {
            R_RunParticleEffect_BigSmoke(org, dir, count);
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
void R_LavaSplash(const qvec3& org)
{
    for(int i = -16; i < 16; i++)
    {
        for(int j = -16; j < 16; j++)
        {
            makeNParticles(ptxAtlas, 1, [&](PHandle p) {
                p.atlasIdx() = aiCircle;
                p.angle() = rndToRad(0.f, 360.f);
                p.setAlpha(255);
                p.scale() = 1.f;
                p.die() = cl.time + 2 + (rand() & 31) * 0.02;
                p.setColor(224 + (rand() & 7));
                p.type() = pt_static;
                setAccGrav(p);

                const qvec3 dir{          //
                    j * 8 + (rand() & 7), //
                    i * 8 + (rand() & 7), //
                    256};

                p.org()[0] = org[0] + dir[0];
                p.org()[1] = org[1] + dir[1];
                p.org()[2] = org[2] + (rand() & 63);

                const qfloat vel = 50 + (rand() & 63);
                p.vel() = safeNormalize(dir) * vel;
            });
        }
    }
}

/*
===============
R_TeleportSplash
===============
*/
void R_TeleportSplash(const qvec3& org)
{
    for(int i = -16; i < 16; i += 4)
    {
        for(int j = -16; j < 16; j += 4)
        {
            for(int k = -24; k < 32; k += 4)
            {
                makeNParticles(ptxAtlas, 1, [&](PHandle p) {
                    p.atlasIdx() = aiCircle;
                    p.angle() = rndToRad(0.f, 360.f);
                    p.setAlpha(rnd(150, 255));
                    p.scale() = rnd(0.6f, 1.f);
                    p.die() = cl.time + 1.2 + (rand() & 7) * 0.2;
                    p.setColor(7 + (rand() & 7));
                    p.type() = pt_teleport;
                    setAccGrav(p, 0.2f);

                    const qvec3 dir{
                        j * 8, //
                        i * 8, //
                        k * 8  //
                    };

                    p.org()[0] = org[0] + i + (rand() & 3);
                    p.org()[1] = org[1] + j + (rand() & 3);
                    p.org()[2] = org[2] + k + (rand() & 3);

                    const qfloat vel = 50 + (rand() & 63);
                    p.vel() = safeNormalize(dir) * vel;
                });
            }
        }
    }
}

static void R_SetRTRocketTrail(const qvec3& start, PHandle p)
{
    p.ramp() = (rand() & 3);
    p.setColor(ramp3[(int)p.ramp()]);
    p.type() = pt_fire;
    for(int j = 0; j < 3; j++)
    {
        p.org()[j] = start[j] + ((rand() % 6) - 3);
    }
}

static void R_SetRTBlood(const qvec3& start, PHandle p)
{
    p.type() = pt_static;
    p.setColor(67 + (rand() & 3));
    for(int j = 0; j < 3; j++)
    {
        p.org()[j] = start[j] + ((rand() % 6) - 3);
    }
}

static void R_SetRTTracer(
    const qvec3& start, const qvec3& end, PHandle p, int type)
{
    static int tracercount;

    const auto vec = end - start;

    p.die() = cl.time + 0.5;
    p.type() = pt_static;
    if(type == 3)
    {
        p.setColor(52 + ((tracercount & 4) << 1));
    }
    else
    {
        p.setColor(230 + ((tracercount & 4) << 1));
    }

    tracercount++;

    p.org() = start;
    if(tracercount & 1)
    {
        p.vel()[0] = 30 * vec[1];
        p.vel()[1] = 30 * -vec[0];
    }
    else
    {
        p.vel()[0] = 30 * -vec[1];
        p.vel()[1] = 30 * vec[0];
    }
}

static void R_SetRTSlightBlood(const qvec3& start, PHandle p)
{
    p.type() = pt_static;
    p.setColor(67 + (rand() & 3));
    for(int j = 0; j < 3; j++)
    {
        p.org()[j] = start[j] + ((rand() % 6) - 3);
    }
}

static void R_SetRTVoorTrail(const qvec3& start, PHandle p)
{
    p.setColor(9 * 16 + 8 + (rand() & 3));
    p.type() = pt_static;
    p.die() = cl.time + 0.3;
    for(int j = 0; j < 3; j++)
    {
        p.org()[j] = start[j] + ((rand() & 15) - 8);
    }
}

static void R_SetRTCommon(PHandle p)
{
    p.angle() = rndToRad(0.f, 360.f);
    p.setAlpha(255);
    p.scale() = 0.7f;
    setAccGrav(p, 0.05f);
    p.vel() = vec3_zero;
    p.die() = cl.time + 2;
}

/*
===============
R_RocketTrail

FIXME -- rename function and use #defined types instead of numbers
===============
*/
constexpr float rate = 0.1f / 9.f;
float untilNext = rate;

void R_RocketTrail(qvec3 start, const qvec3& end, int type)
{
    const auto frametime = cl.time - cl.oldtime;

    untilNext -= frametime;
    if(untilNext > 0)
    {
        return;
    }

    untilNext = rate;

    auto vec = end - start;

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

    float len = glm::length(vec);
    vec = safeNormalize(vec);

    while(len > 0)
    {
        len -= dec;

        switch(type)
        {
            case 0: // rocket trail
            {
                makeNParticles(ptxAtlas, 6, [&](PHandle p) {
                    p.atlasIdx() = aiCircle;
                    R_SetRTCommon(p);
                    R_SetRTRocketTrail(start, p);
                });

                makeNParticles(ptxAtlas, 1, [&](PHandle p) {
                    p.atlasIdx() = aiSmoke;
                    p.setAlpha(65);
                    p.die() = cl.time + 1.5 * (rand() % 5);
                    p.setColor(rand() & 7);
                    p.scale() = rnd(0.3f, 0.5f);
                    p.type() = pt_txsmoke;
                    setAccGrav(p, -0.09f);

                    for(int j = 0; j < 3; j++)
                    {
                        p.org()[j] = start[j] + ((rand() & 6) - 3);
                        p.vel()[j] = rnd(-18, 18);
                    }
                });

                break;
            }

            case 1: // smoke smoke
            {
                makeNParticles(ptxAtlas, 1, [&](PHandle p) {
                    p.atlasIdx() = aiSmoke;
                    p.angle() = rndToRad(0.f, 360.f);
                    p.setAlpha(65);
                    p.die() = cl.time + 1.5 * (rand() % 5);
                    p.setColor(rand() & 7);
                    p.scale() = rnd(0.3f, 0.5f);
                    p.type() = pt_txsmoke;
                    setAccGrav(p, -0.09f);

                    for(int j = 0; j < 3; j++)
                    {
                        p.org()[j] = start[j] + ((rand() & 6) - 3);
                        p.vel()[j] = rnd(-18, 18);
                    }
                });

                break;
            }

            case 2: // blood
            {
                makeNParticles(ptxAtlas, 6, [&](PHandle p) {
                    p.atlasIdx() = aiCircle;
                    R_SetRTCommon(p);
                    R_SetRTBlood(start, p);
                });

                makeNParticles(ptxAtlas, 1, [&](PHandle p) {
                    p.atlasIdx() = aiBloodMist;
                    p.angle() = rndToRad(0.f, 360.f);
                    p.setAlpha(32);
                    p.die() = cl.time + 3.2;
                    p.setColor(225);
                    p.ramp() = rand() & 3;
                    p.scale() = rnd(1.1f, 2.4f) * 15.f;
                    setAccGrav(p, -0.03f);
                    p.type() = pt_txsmoke;

                    for(int j = 0; j < 3; j++)
                    {
                        p.org()[j] = start[j] + rnd(-8, 8);
                        p.vel()[j] = rnd(-4, -4);
                    }
                });

                break;
            }

            case 3: [[fallthrough]];
            case 5: // tracer
            {
                makeNParticles(ptxAtlas, 6, [&](PHandle p) {
                    p.atlasIdx() = aiCircle;
                    R_SetRTCommon(p);
                    R_SetRTTracer(start, end, p, type);
                });

                break;
            }

            case 4: // slight blood
            {
                makeNParticles(ptxAtlas, 6, [&](PHandle p) {
                    p.atlasIdx() = aiCircle;
                    R_SetRTCommon(p);
                    R_SetRTSlightBlood(start, p);
                    len -= 3;
                });

                break;
            }

            case 6: // voor trail
            {
                makeNParticles(ptxAtlas, 6, [&](PHandle p) {
                    p.atlasIdx() = aiCircle;
                    R_SetRTCommon(p);
                    R_SetRTVoorTrail(start, p);
                });

                break;
            }

            case 7: // mini rocket trail
            {
                makeNParticles(ptxAtlas, 2, [&](PHandle p) {
                    p.atlasIdx() = aiCircle;
                    R_SetRTCommon(p);
                    R_SetRTRocketTrail(start, p);
                });

                makeNParticles(ptxAtlas, 1, [&](PHandle p) {
                    p.atlasIdx() = aiSmoke;
                    p.setAlpha(55);
                    p.die() = cl.time + 1.2 * (rand() % 5);
                    p.setColor(rand() & 7);
                    p.scale() = rnd(0.05f, 0.23f);
                    p.type() = pt_txsmoke;
                    setAccGrav(p, -0.09f);

                    for(int j = 0; j < 3; j++)
                    {
                        p.org()[j] = start[j] + ((rand() & 4) - 2);
                        p.vel()[j] = rnd(-18, 18);
                    }
                });

                break;
            }
        }

        start += vec;
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
    if(!r_particles.value)
    {
        return;
    }

    const qfloat frametime = cl.time - cl.oldtime;

    const float time3 = frametime * 15;
    const float time2 = frametime * 10;
    const float time1 = frametime * 5;
    const float dvel = 4 * frametime;

    pMgr.cleanup();

    pMgr.forBuffers([&](gltexture_t*, const ImageData&, PBuffer& pBuffer) {
        ParticleSOA& soa = pBuffer.soa();
        const auto pCount = pBuffer.aliveCount();

        for(std::size_t i = 0; i < pCount; ++i)
        {
            soa._vels[i] += soa._accs[i] * frametime;
            soa._orgs[i] += soa._vels[i] * frametime;
        }
    });

    forActiveParticles([&](PHandle p) {
        switch(p.type())
        {
            case pt_static:
            {
                break;
            }

            case pt_fire:
            {
                p.ramp() += time1;
                if(p.ramp() >= 6)
                {
                    p.die() = -1;
                }
                else
                {
                    p.setColor(ramp3[(int)p.ramp()]);
                }

                break;
            }

            case pt_explode:
            {
                p.ramp() += time2;

                if(p.ramp() >= 8)
                {
                    p.die() = -1;
                }
                else
                {
                    p.setColor(ramp1[(int)p.ramp()]);
                }

                for(int i = 0; i < 3; i++)
                {
                    p.vel()[i] += p.vel()[i] * dvel;
                }

                break;
            }

            case pt_explode2:
            {
                p.ramp() += time3;

                if(p.ramp() >= 8)
                {
                    p.die() = -1;
                }
                else
                {
                    p.setColor(ramp2[(int)p.ramp()]);
                }

                for(int i = 0; i < 3; i++)
                {
                    p.vel()[i] -= p.vel()[i] * frametime;
                }

                break;
            }

            case pt_blob:
            {
                p.vel() += p.vel() * dvel;

                break;
            }

            case pt_blob2:
            {
                for(int i = 0; i < 2; i++)
                {
                    p.vel()[i] -= p.vel()[i] * dvel;
                }

                break;
            }

            case pt_txexplode:
            {
                p.addAlpha(-345.f, frametime);
                p.scale() += 135.f * frametime;
                p.angle() += 0.75f * frametime * (p.param0() == 0 ? 1.f : -1.f);

                break;
            }

            case pt_txsmoke:
            {
                p.addAlpha(-70.f, frametime);
                p.scale() += 47.f * frametime;

                break;
            }

            case pt_txbigsmoke:
            {
                p.addAlpha(-35.f, frametime);
                p.scale() += 37.f * frametime;

                break;
            }

            case pt_lightning:
            {
                p.addAlpha(-84.f, frametime);
                p.scale() -= 32.f * frametime;

                break;
            }

            case pt_teleport:
            {
                p.addAlpha(-85.f, frametime);
                p.scale() -= 0.1f * frametime;

                break;
            }

            case pt_rock:
            {
                p.angle() += 25.f * frametime * (p.param0() == 0 ? 1.f : -1.f);

                break;
            }

            case pt_gunsmoke:
            {
                p.addAlpha(-110.f, frametime);
                p.scale() += 69.f * frametime;
                p.org()[2] += 18.f * frametime;

                break;
            }

            case pt_gunpickup:
            {
                p.addAlpha(-120.f, frametime);
                p.scale() -= 0.2f * frametime;

                break;
            }

            default:
            {
                break;
            }
        }
    });
}

static GLuint makeParticleShaders()
{
    using namespace std::string_view_literals;

    constexpr auto vertexShader = R"glsl(
#version 430 core

layout(location = 0) in vec3  pOrg;
layout(location = 1) in float pAngle;
layout(location = 2) in float pScale;
layout(location = 3) in vec4  pColor;
layout(location = 4) in int   pAtlasIdx;

out VS_OUT {
    float opAngle;
    float opScale;
    vec4  opColor;
    int   opAtlasIdx;
} vs_out;

void main()
{
    gl_Position = vec4(pOrg, 1.0);
    vs_out.opAngle = pAngle;
    vs_out.opScale = pScale;
    vs_out.opColor = pColor;
    vs_out.opAtlasIdx = pAtlasIdx;
}
)glsl"sv;


    constexpr auto geometryShader = R"glsl(
#version 430 core

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

in VS_OUT {
    float opAngle;
    float opScale;
    vec4  opColor;
    int   opAtlasIdx;
} gs_in[];

layout(location = 5) uniform vec3 rOrigin;
layout(location = 6) uniform vec3 rUp;
layout(location = 7) uniform vec3 rRight;
layout(location = 8) uniform mat4 mvMatrix;
layout(location = 12) uniform mat4 pjMatrix;

out vec2 texcoord;
out vec4 outVertexColor;
flat out int outAtlasIdx;

// From: http://www.neilmendoza.com/glsl-rotation-about-an-arbitrary-axis/
mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;

    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

void main()
{
    vec3 pos = gl_in[0].gl_Position.xyz;

    vec3 fwd = pos - rOrigin;
    mat4 rotMat = rotationMatrix(fwd, gs_in[0].opAngle); // This seems wrong

    vec3 up = (vec4(rUp, 1.0) * rotMat).xyz;
    vec3 right = (vec4(rRight, 1.0) * rotMat).xyz;

    float halfScale = gs_in[0].opScale / 2.0;

    vec3 left = -right;
    vec3 down = -up;

    outVertexColor = gs_in[0].opColor;
    outAtlasIdx = gs_in[0].opAtlasIdx;

    gl_Position = pjMatrix * mvMatrix * vec4(pos + halfScale * down + halfScale * left, 1.0);
    texcoord = vec2(0, 0);
    EmitVertex();

    gl_Position = pjMatrix * mvMatrix * vec4(pos + halfScale * up + halfScale * left, 1.0);
    texcoord = vec2(1, 0);
    EmitVertex();

    gl_Position = pjMatrix * mvMatrix * vec4(pos + halfScale * down + halfScale * right, 1.0);
    texcoord = vec2(0, 1);
    EmitVertex();

    gl_Position = pjMatrix * mvMatrix * vec4(pos + halfScale * up + halfScale * right, 1.0);
    texcoord = vec2(1, 1);
    EmitVertex();

    EndPrimitive();
}
)glsl"sv;

    constexpr auto fragmentShader = R"glsl(
#version 430 core

layout(location = 16) uniform vec4 atlasBuf[11]; // TODO VR: (P1) hardcoded

in vec2 texcoord;
in vec4 outVertexColor;
flat in int outAtlasIdx;

uniform sampler2D gColorMap;

out vec4 FragColor;

void main (void)
{
    ivec2 textureSize2d = textureSize(gColorMap, 0);

    vec2 imgXY = atlasBuf[outAtlasIdx].xy;
    vec2 imgWH = atlasBuf[outAtlasIdx].zw;

    vec2 uv = (imgXY / textureSize2d) + (texcoord.xy * (imgWH / textureSize2d));

    vec3 t = texture(gColorMap, uv).rgb * outVertexColor.rgb;
    FragColor = vec4(t, texture(gColorMap, uv).a * outVertexColor.a);

    if(FragColor.a < 0.01)
        discard;
}
)glsl"sv;

    return quake::make_gl_program(vertexShader, geometryShader, fragmentShader);
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

    static bool generated = false;

    static GLint particleProgramId;

    static GLuint vaoId;

    static GLuint pOrgVboId;
    static GLuint pAngleVboId;
    static GLuint pScaleVboId;
    static GLuint pColorVboId;
    static GLuint pAtlasIdxVboId;

    static GLint pOrgLocation;
    static GLint pAngleLocation;
    static GLint pScaleLocation;
    static GLint pColorLocation;
    static GLint pAtlasIdxLocation;
    static GLint rOriginLocation;
    static GLint rUpLocation;
    static GLint rRightLocation;
    static GLint mvMatrixLocation;
    static GLint pjMatrixLocation;
    static GLint atlasBufLocation;

    if(!generated)
    {
        particleProgramId = makeParticleShaders();

        glGenVertexArrays(1, &vaoId);

        glGenBuffers(1, &pOrgVboId);
        glGenBuffers(1, &pAngleVboId);
        glGenBuffers(1, &pScaleVboId);
        glGenBuffers(1, &pColorVboId);
        glGenBuffers(1, &pAtlasIdxVboId);

        pOrgLocation = 0;
        pAngleLocation = 1;
        pScaleLocation = 2;
        pColorLocation = 3;
        pAtlasIdxLocation = 4;
        rOriginLocation = 5;
        rUpLocation = 6;
        rRightLocation = 7;
        mvMatrixLocation = 8;
        pjMatrixLocation = 12;
        atlasBufLocation = 16;

        generated = true;
    }

    const auto up = vup * 1.5_qf;
    const auto right = vright * 1.5_qf;

    float mvMatrix[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, mvMatrix);

    float pjMatrix[16];
    glGetFloatv(GL_PROJECTION_MATRIX, pjMatrix);

    //
    //
    // Configuration
    glUseProgram(particleProgramId);

    glBindVertexArray(vaoId);

    glEnableVertexAttribArray(pOrgLocation);
    glEnableVertexAttribArray(pAngleLocation);
    glEnableVertexAttribArray(pScaleLocation);
    glEnableVertexAttribArray(pColorLocation);
    glEnableVertexAttribArray(pAtlasIdxLocation);

    glUniform3f(         //
        rOriginLocation, //
        r_origin[0], r_origin[1], r_origin[2]);

    glUniform3f(     //
        rUpLocation, //
        up[0], up[1], up[2]);

    glUniform3f(        //
        rRightLocation, //
        right[0], right[1], right[2]);

    glUniformMatrix4fv(   //
        mvMatrixLocation, // location
        1,                // count
        GL_FALSE,         // transpose
        mvMatrix);

    glUniformMatrix4fv(   //
        pjMatrixLocation, // location
        1,                // count
        GL_FALSE,         // transpose
        pjMatrix);

    glUniform4fv(                      //
        atlasBufLocation,              // location
        11,                            // count TODO VR: (P1) hardcoded
        ptxAtlasData._imageInfo.data() // value
    );

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glEnable(GL_BLEND);
    glDepthMask(GL_FALSE);

    glBindVertexArray(vaoId);

    pMgr.forBuffers([&](gltexture_t* texture, const ImageData& imageData,
                        PBuffer& pBuffer) {
        (void)texture;
        (void)imageData;

        const auto pCount = pBuffer.aliveCount();
        ParticleSOA& soa = pBuffer.soa();

        GL_Bind(texture);

        //
        //
        // Setup

        glBindBuffer(GL_ARRAY_BUFFER, pOrgVboId);
        glBufferData(
            GL_ARRAY_BUFFER, sizeof(qvec3) * pCount, soa._orgs, GL_STATIC_DRAW);
        glVertexAttribPointer( //
            pOrgLocation,      // location
            3,                 // number of components (vec3 = 3)
            GL_FLOAT,          // type of each component
            GL_FALSE,          // normalized
            0,                 // stride
            (void*)0           // array buffer offset
        );

        glBindBuffer(GL_ARRAY_BUFFER, pAngleVboId);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * pCount, soa._angles,
            GL_STATIC_DRAW);
        glVertexAttribPointer( //
            pAngleLocation,    // location
            1,                 // number of components
            GL_FLOAT,          // type of each component
            GL_FALSE,          // normalized
            0,                 // stride
            (void*)0           // array buffer offset
        );

        glBindBuffer(GL_ARRAY_BUFFER, pScaleVboId);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * pCount, soa._scales,
            GL_STATIC_DRAW);
        glVertexAttribPointer( //
            pScaleLocation,    // location
            1,                 // number of components
            GL_FLOAT,          // type of each component
            GL_FALSE,          // normalized
            0,                 // stride
            (void*)0           // array buffer offset
        );

        glBindBuffer(GL_ARRAY_BUFFER, pColorVboId);
        glBufferData(GL_ARRAY_BUFFER, sizeof(qvec4) * pCount, soa._colors,
            GL_STATIC_DRAW);
        glVertexAttribPointer( //
            pColorLocation,    // location
            4,                 // number of components
            GL_FLOAT,          // type of each component
            GL_FALSE,          // normalized
            0,                 // stride
            (void*)0           // array buffer offset
        );

        glBindBuffer(GL_ARRAY_BUFFER, pAtlasIdxVboId);
        glBufferData(GL_ARRAY_BUFFER, sizeof(int) * pCount, soa._atlasIdxs,
            GL_STATIC_DRAW);
        glVertexAttribIPointer( //
            pAtlasIdxLocation,  // location
            1,                  // number of components
            GL_INT,             // type of each component
            0,                  // stride
            (void*)0            // array buffer offset
        );

        //
        //
        // Draw
        glDrawArrays(GL_POINTS, 0, pBuffer.aliveCount());
    });

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor3f(1, 1, 1);

    //
    //
    // Cleanup
    glDisableVertexAttribArray(pAtlasIdxLocation);
    glDisableVertexAttribArray(pColorLocation);
    glDisableVertexAttribArray(pScaleLocation);
    glDisableVertexAttribArray(pAngleLocation);
    glDisableVertexAttribArray(pOrgLocation);

    glBindVertexArray(0);

    glUseProgram(0);
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

    // const auto up = vup * 1.5f;
    // const auto right = vright * 1.5f;

    glBegin(GL_TRIANGLES);
    forActiveParticles([&](PHandle p) {
        (void)p;

        // TODO VR: (P2) rewrite
        /*
        const float scale = p.scale;

        glVertex3fv(toGlVec(p.org));

        qvec3 p_up;
        VectorMA(p.org, scale, up, p_up);
        glVertex3fv(toGlVec(p_up));

        qvec3 p_right;
        VectorMA(p.org, scale, right, p_right);
        glVertex3fv(toGlVec(p_right));
        */
    });
    glEnd();
}
