/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske
Copyright (C) 2010-2014 QuakeSpasm developers

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

#define MAX_PARTICLES \
    65536 // default max # of particles at one
          //  time
#define ABSOLUTE_MIN_PARTICLES \
    512 // no fewer than this no matter what's
        //  on the command line

// These "ramps" below are for colors..
constexpr int ramp1[8] = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
constexpr int ramp2[8] = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
constexpr int ramp3[8] = {0x6d, 0x6b, 6, 5, 4, 3};

particle_t* active_particles;
particle_t* free_particles;
particle_t* particles;

int r_numparticles;

gltexture_t* default_particletexture;
gltexture_t* particletexture1;
gltexture_t* particletexture2;
gltexture_t* particletexture3;
gltexture_t* particletexture4; // johnfitz
float texturescalefactor;      // johnfitz -- compensate for apparent size of
                               // different particle textures

cvar_t r_particles = {"r_particles", "1", CVAR_ARCHIVE};         // johnfitz
cvar_t r_quadparticles = {"r_quadparticles", "1", CVAR_ARCHIVE}; // johnfitz

[[nodiscard]] static particle_t* getFreeParticle() noexcept
{
    // Pop from free particles list
    particle_t* const p = free_particles;
    free_particles = p->next;

    // Push into active particles list
    p->next = active_particles;
    active_particles = p;

    return p;
}

[[nodiscard]] static particle_t* killParticle(particle_t* p) noexcept
{
    particle_t* const result = p->next;
    p->next = free_particles;
    free_particles = p;

    return result;
}

template <typename F>
void forActiveParticles(F&& f)
{
    for(particle_t* p = active_particles; p; p = p->next)
    {
        f(*p);
    }
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

/*
===============
R_InitParticleTextures -- johnfitz -- rewritten
===============
*/
void R_InitParticleTextures()
{
    int x;

    int y;
    static byte particle1_data[64 * 64 * 4];
    static byte particle2_data[2 * 2 * 4];
    static byte particle3_data[64 * 64 * 4];
    byte* dst;

    // particle texture 1 -- circle
    dst = particle1_data;
    for(x = 0; x < 64; x++)
    {
        for(y = 0; y < 64; y++)
        {
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = R_ParticleTextureLookup(x, y, 8);
        }
    }
    particletexture1 = TexMgr_LoadImage(nullptr, "particle1", 64, 64, SRC_RGBA,
        particle1_data, "", (src_offset_t)particle1_data,
        TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_LINEAR);

    // particle texture 2 -- square
    dst = particle2_data;
    for(x = 0; x < 2; x++)
    {
        for(y = 0; y < 2; y++)
        {
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = x || y ? 0 : 255;
        }
    }
    particletexture2 = TexMgr_LoadImage(nullptr, "particle2", 2, 2, SRC_RGBA,
        particle2_data, "", (src_offset_t)particle2_data,
        TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_NEAREST);

    // particle texture 3 -- blob
    dst = particle3_data;
    for(x = 0; x < 64; x++)
    {
        for(y = 0; y < 64; y++)
        {
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = 255;
            *dst++ = R_ParticleTextureLookup(x, y, 2);
        }
    }
    particletexture3 = TexMgr_LoadImage(nullptr, "particle3", 64, 64, SRC_RGBA,
        particle3_data, "", (src_offset_t)particle3_data,
        TEXPREF_PERSIST | TEXPREF_ALPHA | TEXPREF_LINEAR);

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

/*
===============
R_InitParticles
===============
*/
void R_InitParticles()
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

    particles = (particle_t*)Hunk_AllocName(
        r_numparticles * sizeof(particle_t), "particles");

    Cvar_RegisterVariable(&r_particles); // johnfitz
    Cvar_SetCallback(&r_particles, R_SetParticleTexture_f);
    Cvar_RegisterVariable(&r_quadparticles); // johnfitz

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
vec3_t avelocity = {23, 7, 3};
float partstep = 0.01;
float timescale = 0.01;

void R_EntityParticles(entity_t* ent)
{
    float angle;
    float sp;

    float sy;

    float cp;

    float cy;
    //	float		sr, cr;
    //	int		count;
    vec3_t forward;

    const float dist = 64;
    //	count = 50;

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
        angle = cl.time * avelocities[i][0];
        sy = sin(angle);
        cy = cos(angle);
        angle = cl.time * avelocities[i][1];
        sp = sin(angle);
        cp = cos(angle);
        angle = cl.time * avelocities[i][2];
        //	sr = sin(angle);
        //	cr = cos(angle);

        forward[0] = cp * cy;
        forward[1] = cp * sy;
        forward[2] = -sp;

        if(!free_particles)
        {
            return;
        }

        particle_t* const p = getFreeParticle();

        p->die = cl.time + 0.01;
        p->color = 0x6f;
        p->type = pt_explode;

        p->org[0] = ent->origin[0] + r_avertexnormals[i][0] * dist +
                    forward[0] * beamlength;
        p->org[1] = ent->origin[1] + r_avertexnormals[i][1] * dist +
                    forward[1] * beamlength;
        p->org[2] = ent->origin[2] + r_avertexnormals[i][2] * dist +
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
    free_particles = &particles[0];
    active_particles = nullptr;

    for(int i = 0; i < r_numparticles; i++)
    {
        particles[i].next = &particles[i + 1];
    }

    particles[r_numparticles - 1].next = nullptr;
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

        if(!free_particles)
        {
            Con_Printf("Not enough free particles\n");
            break;
        }

        particle_t* const p = getFreeParticle();

        p->die = 99999;
        p->color = (-c) & 15;
        p->type = pt_static;

        VectorCopy(vec3_origin, p->vel);
        VectorCopy(org, p->org);
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
    const int count = msgcount == 255 ? 1024 : msgcount;

    R_RunParticleEffect(org, dir, color, count);
}

/*
===============
R_ParticleExplosion
===============
*/
void R_ParticleExplosion(vec3_t org)
{
    for(int i = 0; i < 1024; i++)
    {
        if(!free_particles)
        {
            return;
        }

        particle_t* const p = getFreeParticle();

        p->die = cl.time + 5;
        p->color = ramp1[0];
        p->ramp = rand() & 3;

        if(i & 1)
        {
            p->type = pt_explode;
            for(int j = 0; j < 3; j++)
            {
                p->org[j] = org[j] + ((rand() % 32) - 16);
                p->vel[j] = (rand() % 512) - 256;
            }
        }
        else
        {
            p->type = pt_explode2;
            for(int j = 0; j < 3; j++)
            {
                p->org[j] = org[j] + ((rand() % 32) - 16);
                p->vel[j] = (rand() % 512) - 256;
            }
        }
    }
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
        if(!free_particles)
        {
            return;
        }

        particle_t* const p = getFreeParticle();

        p->die = cl.time + 0.3;
        p->color = colorStart + (colorMod % colorLength);
        colorMod++;

        p->type = pt_blob;
        for(int j = 0; j < 3; j++)
        {
            p->org[j] = org[j] + ((rand() % 32) - 16);
            p->vel[j] = (rand() % 512) - 256;
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
        if(!free_particles)
        {
            return;
        }

        particle_t* const p = getFreeParticle();

        p->die = cl.time + 1 + (rand() & 8) * 0.05;

        if(i & 1)
        {
            p->type = pt_blob;
            p->color = 66 + rand() % 6;
            for(int j = 0; j < 3; j++)
            {
                p->org[j] = org[j] + ((rand() % 32) - 16);
                p->vel[j] = (rand() % 512) - 256;
            }
        }
        else
        {
            p->type = pt_blob2;
            p->color = 150 + rand() % 6;
            for(int j = 0; j < 3; j++)
            {
                p->org[j] = org[j] + ((rand() % 32) - 16);
                p->vel[j] = (rand() % 512) - 256;
            }
        }
    }
}

/*
===============
R_RunParticleEffect
===============
*/
void R_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count)
{
    for(int i = 0; i < count; i++)
    {
        if(!free_particles)
        {
            return;
        }

        particle_t* const p = getFreeParticle();

        if(count == 1024)
        { // rocket explosion
            p->die = cl.time + 5;
            p->color = ramp1[0];
            p->ramp = rand() & 3;
            if(i & 1)
            {
                p->type = pt_explode;
                for(int j = 0; j < 3; j++)
                {
                    p->org[j] = org[j] + ((rand() % 32) - 16);
                    p->vel[j] = (rand() % 512) - 256;
                }
            }
            else
            {
                p->type = pt_explode2;
                for(int j = 0; j < 3; j++)
                {
                    p->org[j] = org[j] + ((rand() % 32) - 16);
                    p->vel[j] = (rand() % 512) - 256;
                }
            }
        }
        else
        {
            p->die = cl.time + 0.1 * (rand() % 5);
            p->color = (color & ~7) + (rand() & 7);
            p->type = pt_slowgrav;
            for(int j = 0; j < 3; j++)
            {
                // TODO VR: bullet puff
                p->org[j] = org[j] + ((rand() & 7) - 4);
                p->vel[j] = dir[j] * 15 + (rand() % 6) - 3;
            }
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
    float vel;
    vec3_t dir;

    for(int i = -16; i < 16; i++)
    {
        for(int j = -16; j < 16; j++)
        {
            for(int k = 0; k < 1; k++)
            {
                if(!free_particles)
                {
                    return;
                }

                particle_t* const p = getFreeParticle();

                p->die = cl.time + 2 + (rand() & 31) * 0.02;
                p->color = 224 + (rand() & 7);
                p->type = pt_slowgrav;

                dir[0] = j * 8 + (rand() & 7);
                dir[1] = i * 8 + (rand() & 7);
                dir[2] = 256;

                p->org[0] = org[0] + dir[0];
                p->org[1] = org[1] + dir[1];
                p->org[2] = org[2] + (rand() & 63);

                VectorNormalize(dir);
                vel = 50 + (rand() & 63);
                VectorScale(dir, vel, p->vel);
            }
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
    vec3_t dir;

    for(int i = -16; i < 16; i += 1)
    {
        for(int j = -16; j < 16; j += 1)
        {
            for(int k = -24; k < 32; k += 1)
            {
                if(!free_particles)
                {
                    return;
                }

                particle_t* const p = getFreeParticle();

                p->die = cl.time + 0.2 + (rand() & 7) * 0.02;
                p->color = 7 + (rand() & 7);
                p->type = pt_slowgrav;

                dir[0] = j * 8;
                dir[1] = i * 8;
                dir[2] = k * 8;

                p->org[0] = org[0] + i + (rand() & 3);
                p->org[1] = org[1] + j + (rand() & 3);
                p->org[2] = org[2] + k + (rand() & 3);

                VectorNormalize(dir);
                const float vel = 50 + (rand() & 63);
                VectorScale(dir, vel, p->vel);
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
    vec3_t vec;
    float len;
    int dec;
    static int tracercount;

    VectorSubtract(end, start, vec);
    len = VectorNormalize(vec);
    if(type < 128)
    {
        dec = 3;
    }
    else
    {
        dec = 1;
        type -= 128;
    }

    while(len > 0)
    {
        len -= dec;

        if(!free_particles)
        {
            return;
        }

        particle_t* const p = getFreeParticle();

        VectorCopy(vec3_origin, p->vel);
        p->die = cl.time + 2;

        switch(type)
        {
            case 0: // rocket trail
                p->ramp = (rand() & 3);
                p->color = ramp3[(int)p->ramp];
                p->type = pt_fire;
                for(int j = 0; j < 3; j++)
                {
                    p->org[j] = start[j] + ((rand() % 6) - 3);
                }
                break;

            case 1: // smoke smoke
                p->ramp = (rand() & 3) + 2;
                p->color = ramp3[(int)p->ramp];
                p->type = pt_fire;
                for(int j = 0; j < 3; j++)
                {
                    p->org[j] = start[j] + ((rand() % 6) - 3);
                }
                break;

            case 2: // blood
                p->type = pt_grav;
                p->color = 67 + (rand() & 3);
                for(int j = 0; j < 3; j++)
                {
                    p->org[j] = start[j] + ((rand() % 6) - 3);
                }
                break;

            case 3:
            case 5: // tracer
                p->die = cl.time + 0.5;
                p->type = pt_static;
                if(type == 3)
                {
                    p->color = 52 + ((tracercount & 4) << 1);
                }
                else
                {
                    p->color = 230 + ((tracercount & 4) << 1);
                }

                tracercount++;

                VectorCopy(start, p->org);
                if(tracercount & 1)
                {
                    p->vel[0] = 30 * vec[1];
                    p->vel[1] = 30 * -vec[0];
                }
                else
                {
                    p->vel[0] = 30 * -vec[1];
                    p->vel[1] = 30 * vec[0];
                }
                break;

            case 4: // slight blood
                p->type = pt_grav;
                p->color = 67 + (rand() & 3);
                for(int j = 0; j < 3; j++)
                {
                    p->org[j] = start[j] + ((rand() % 6) - 3);
                }
                len -= 3;
                break;

            case 6: // voor trail
                p->color = 9 * 16 + 8 + (rand() & 3);
                p->type = pt_static;
                p->die = cl.time + 0.3;
                for(int j = 0; j < 3; j++)
                {
                    p->org[j] = start[j] + ((rand() & 15) - 8);
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
    extern cvar_t sv_gravity;

    const float frametime = cl.time - cl.oldtime;
    const float time3 = frametime * 15;
    const float time2 = frametime * 10;
    const float time1 = frametime * 5;
    const float grav = frametime * sv_gravity.value * 0.05;
    const float dvel = 4 * frametime;

    while(true)
    {
        if(particle_t* const pk = active_particles; pk && pk->die < cl.time)
        {
            active_particles = killParticle(pk);
            continue;
        }

        break;
    }

    forActiveParticles([&](particle_t& p) {
        while(true)
        {
            if(particle_t* const pk = p.next; pk && pk->die < cl.time)
            {
                p.next = killParticle(pk);
                continue;
            }

            break;
        }

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
                p.vel[2] += grav;
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
                p.vel[2] -= grav;
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
                p.vel[2] -= grav;
                break;
            }

            case pt_blob:
            {
                for(int i = 0; i < 3; i++)
                {
                    p.vel[i] += p.vel[i] * dvel;
                }
                p.vel[2] -= grav;
                break;
            }

            case pt_blob2:
            {
                for(int i = 0; i < 2; i++)
                {
                    p.vel[i] -= p.vel[i] * dvel;
                }
                p.vel[2] -= grav;
                break;
            }

            case pt_grav: [[fallthrough]];
            case pt_slowgrav:
            {
                p.vel[2] -= grav;
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

    if(!r_particles.value)
    {
        return;
    }

    // ericw -- avoid empty glBegin(),glEnd() pair below; causes issues
    // on AMD
    if(!active_particles)
    {
        return;
    }

    vec3_t up;
    VectorScale(vup, 1.5, up);

    vec3_t right;
    VectorScale(vright, 1.5, right);

    GL_Bind(default_particletexture);
    glEnable(GL_BLEND);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glDepthMask(GL_FALSE); // johnfitz -- fix for particle z-buffer bug

    if(r_quadparticles.value) // johnitz -- quads save fillrate
    {
        glBegin(GL_QUADS);
        forActiveParticles([&](particle_t& p) {
            // hack a scale up to keep particles from disapearing
            float scale = (p.org[0] - r_origin[0]) * vpn[0] +
                          (p.org[1] - r_origin[1]) * vpn[1] +
                          (p.org[2] - r_origin[2]) * vpn[2];
            if(scale < 20)
            {
                scale = 1 + 0.08; // johnfitz -- added .08 to be consistent
            }
            else
            {
                scale = 1 + scale * 0.004;
            }

            scale /= 2.0; // quad is half the size of triangle

            scale *= texturescalefactor; // johnfitz -- compensate for
                                         // apparent size of different
                                         // particle textures

            // TODO VR: global particle scale
            if(p.type == ptype_t::pt_explode || p.type == ptype_t::pt_explode2)
            {
                scale *= 1.2f;
            }
            else
            {
                scale *= 0.6f;
            }

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

            glTexCoord2f(0.5, 0);
            vec3_t p_up;
            VectorMA(p.org, scale, up, p_up);
            glVertex3fv(p_up);

            glTexCoord2f(0.5, 0.5);
            vec3_t p_upright; // johnfitz -- p_ vectors
            VectorMA(p_up, scale, right, p_upright);
            glVertex3fv(p_upright);

            glTexCoord2f(0, 0.5);
            vec3_t p_right;
            VectorMA(p.org, scale, right, p_right);
            glVertex3fv(p_right);

            rs_particles++; // johnfitz //FIXME: just use r_numparticles
        });
        glEnd();
    }
    else // johnitz --  triangles save verts
    {
        glBegin(GL_TRIANGLES);
        forActiveParticles([&](particle_t& p) {
            // hack a scale up to keep particles from disapearing
            float scale = (p.org[0] - r_origin[0]) * vpn[0] +
                          (p.org[1] - r_origin[1]) * vpn[1] +
                          (p.org[2] - r_origin[2]) * vpn[2];
            if(scale < 20)
            {
                scale = 1 + 0.08; // johnfitz -- added .08 to be consistent
            }
            else
            {
                scale = 1 + scale * 0.004;
            }

            scale *= texturescalefactor; // johnfitz -- compensate for
                                         // apparent size of different
                                         // particle textures

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
    }

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

    if(r_quadparticles.value)
    {
        forActiveParticles([&](particle_t& p) {
            glBegin(GL_TRIANGLE_FAN);

            // hack a scale up to keep particles from disapearing
            float scale = (p.org[0] - r_origin[0]) * vpn[0] +
                          (p.org[1] - r_origin[1]) * vpn[1] +
                          (p.org[2] - r_origin[2]) * vpn[2];
            if(scale < 20)
            {
                scale = 1 + 0.08; // johnfitz -- added .08 to be consistent
            }
            else
            {
                scale = 1 + scale * 0.004;
            }

            scale /= 2.0; // quad is half the size of triangle

            scale *= texturescalefactor; // compensate for apparent size of
                                         // different particle textures

            glVertex3fv(p.org);

            vec3_t p_up;
            VectorMA(p.org, scale, up, p_up);
            glVertex3fv(p_up);

            vec3_t p_upright;
            VectorMA(p_up, scale, right, p_upright);
            glVertex3fv(p_upright);

            vec3_t p_right;
            VectorMA(p.org, scale, right, p_right);
            glVertex3fv(p_right);

            glEnd();
        });
    }
    else
    {
        glBegin(GL_TRIANGLES);
        forActiveParticles([&](particle_t& p) {
            // hack a scale up to keep particles from disapearing
            float scale = (p.org[0] - r_origin[0]) * vpn[0] +
                          (p.org[1] - r_origin[1]) * vpn[1] +
                          (p.org[2] - r_origin[2]) * vpn[2];

            if(scale < 20)
            {
                scale = 1 + 0.08; // johnfitz -- added .08 to be consistent
            }
            else
            {
                scale = 1 + scale * 0.004;
            }

            scale *= texturescalefactor; // compensate for apparent size of
                                         // different particle textures

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
}
