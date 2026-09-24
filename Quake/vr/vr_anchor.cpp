// vr_anchor.cpp -- see vr_anchor.hpp.

#include "vr_anchor.hpp"

#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace qvr::anchor
{
namespace
{

struct Triangle
{
    int facesfront;
    int vertindex[3];
};

// ----------------------------------------------------------------------------
// QuakeSpasm's BuildTris strip/fan search (gl_mesh.c before QuakeSpasm 0.93), reduced to
// computing the vertex order. Kept structurally identical so the order matches exactly.

class StripBuilder
{
public:
    explicit StripBuilder(const std::vector<Triangle>& tris) : triangles(tris), used(tris.size(), 0)
    {
    }

    [[nodiscard]] std::vector<int> buildVertexOrder()
    {
        std::vector<int> order;
        const int numtris = static_cast<int>(triangles.size());

        for(int i = 0; i < numtris; i++)
        {
            if(used[i])
            {
                continue;
            }

            int bestlen = 0;
            int bestverts[1024];
            int besttris[1024];

            for(int type = 0; type < 2; type++)
            {
                for(int startv = 0; startv < 3; startv++)
                {
                    const int len = type == 1 ? stripLength(i, startv) : fanLength(i, startv);
                    if(len > bestlen)
                    {
                        bestlen = len;
                        for(int j = 0; j < bestlen + 2; j++)
                        {
                            bestverts[j] = stripverts[j];
                        }
                        for(int j = 0; j < bestlen; j++)
                        {
                            besttris[j] = striptris[j];
                        }
                    }
                }
            }

            for(int j = 0; j < bestlen; j++)
            {
                used[besttris[j]] = 1;
            }

            for(int j = 0; j < bestlen + 2; j++)
            {
                order.push_back(bestverts[j]);
            }
        }

        return order;
    }

private:
    const std::vector<Triangle>& triangles;
    std::vector<int> used;
    int stripverts[128];
    int striptris[128];
    int stripcount{0};

    void clearTempUsed(int starttri)
    {
        for(int j = starttri + 1; j < static_cast<int>(triangles.size()); j++)
        {
            if(used[j] == 2)
            {
                used[j] = 0;
            }
        }
    }

    [[nodiscard]] int stripLength(int starttri, int startv)
    {
        used[starttri] = 2;
        const Triangle& last = triangles[starttri];

        stripverts[0] = last.vertindex[startv % 3];
        stripverts[1] = last.vertindex[(startv + 1) % 3];
        stripverts[2] = last.vertindex[(startv + 2) % 3];
        striptris[0] = starttri;
        stripcount = 1;

        int m1 = last.vertindex[(startv + 2) % 3];
        int m2 = last.vertindex[(startv + 1) % 3];

        bool extended = true;
        while(extended && stripcount < 126)
        {
            extended = false;
            for(int j = starttri + 1; j < static_cast<int>(triangles.size()) && !extended; j++)
            {
                const Triangle& check = triangles[j];
                if(check.facesfront != last.facesfront)
                {
                    continue;
                }

                for(int k = 0; k < 3; k++)
                {
                    if(check.vertindex[k] != m1 || check.vertindex[(k + 1) % 3] != m2)
                    {
                        continue;
                    }

                    if(used[j])
                    {
                        clearTempUsed(starttri);
                        return stripcount;
                    }

                    if(stripcount & 1)
                    {
                        m2 = check.vertindex[(k + 2) % 3];
                    }
                    else
                    {
                        m1 = check.vertindex[(k + 2) % 3];
                    }

                    stripverts[stripcount + 2] = check.vertindex[(k + 2) % 3];
                    striptris[stripcount] = j;
                    stripcount++;
                    used[j] = 2;
                    extended = true;
                    break;
                }
            }
        }

        clearTempUsed(starttri);
        return stripcount;
    }

    [[nodiscard]] int fanLength(int starttri, int startv)
    {
        used[starttri] = 2;
        const Triangle& last = triangles[starttri];

        stripverts[0] = last.vertindex[startv % 3];
        stripverts[1] = last.vertindex[(startv + 1) % 3];
        stripverts[2] = last.vertindex[(startv + 2) % 3];
        striptris[0] = starttri;
        stripcount = 1;

        const int m1 = last.vertindex[startv % 3];
        int m2 = last.vertindex[(startv + 2) % 3];

        bool extended = true;
        while(extended && stripcount < 126)
        {
            extended = false;
            for(int j = starttri + 1; j < static_cast<int>(triangles.size()) && !extended; j++)
            {
                const Triangle& check = triangles[j];
                if(check.facesfront != last.facesfront)
                {
                    continue;
                }

                for(int k = 0; k < 3; k++)
                {
                    if(check.vertindex[k] != m1 || check.vertindex[(k + 1) % 3] != m2)
                    {
                        continue;
                    }

                    if(used[j])
                    {
                        clearTempUsed(starttri);
                        return stripcount;
                    }

                    m2 = check.vertindex[(k + 2) % 3];
                    stripverts[stripcount + 2] = m2;
                    striptris[stripcount] = j;
                    stripcount++;
                    used[j] = 2;
                    extended = true;
                    break;
                }
            }
        }

        clearTempUsed(starttri);
        return stripcount;
    }
};

// Reads the triangle list of an MDL file (the in-memory model no longer has it).
[[nodiscard]] std::vector<Triangle> loadTriangles(const char* modelName)
{
    std::vector<Triangle> tris;

    int size = 0;
    byte* data = COM_LoadMallocFile(modelName, nullptr);
    if(!data)
    {
        return tris;
    }
    size = com_filesize;

    const auto readInt = [&](int offset) {
        int v;
        memcpy(&v, data + offset, sizeof(v));
        return LittleLong(v);
    };

    // mdl_t: ident, version, scale[3], scale_origin[3], boundingradius, eyeposition[3],
    // numskins, skinwidth, skinheight, numverts, numtris, numframes, synctype, flags, size.
    constexpr int headerSize = 84;
    if(size < headerSize || readInt(0) != IDPOLYHEADER)
    {
        free(data);
        return tris;
    }

    const int numskins = readInt(48);
    const int skinwidth = readInt(52);
    const int skinheight = readInt(56);
    const int numverts = readInt(60);
    const int numtris = readInt(64);
    const int skinsize = skinwidth * skinheight;

    int ofs = headerSize;
    for(int i = 0; i < numskins && ofs < size; i++)
    {
        const int type = readInt(ofs);
        ofs += 4;
        if(type == 0)
        {
            ofs += skinsize;
        }
        else
        {
            const int groupSkins = readInt(ofs);
            ofs += 4 + groupSkins * 4 + groupSkins * skinsize;
        }
    }

    ofs += numverts * 12; // stverts: onseam, s, t

    if(ofs + numtris * 16 > size)
    {
        free(data);
        return tris;
    }

    tris.resize(numtris);
    for(int i = 0; i < numtris; i++, ofs += 16)
    {
        tris[i].facesfront = readInt(ofs);
        for(int k = 0; k < 3; k++)
        {
            tris[i].vertindex[k] = readInt(ofs + 4 + k * 4);
        }
    }

    free(data);
    return tris;
}

// Strip-order index -> original vertex index, per model name.
std::unordered_map<std::string, std::vector<int>> vertexOrders;

[[nodiscard]] const std::vector<int>& vertexOrder(const qmodel_t* model)
{
    auto it = vertexOrders.find(model->name);
    if(it == vertexOrders.end())
    {
        const std::vector<Triangle> tris = loadTriangles(model->name);
        it = vertexOrders.emplace(model->name, StripBuilder{tris}.buildVertexOrder()).first;
    }

    return it->second;
}

[[nodiscard]] int animatedPose(const maliasframedesc_t& frame)
{
    if(frame.numposes <= 1)
    {
        return frame.firstpose;
    }

    const float interval = frame.interval > 0.f ? frame.interval : 0.1f;
    return frame.firstpose + static_cast<int>(cl.time / interval) % frame.numposes;
}

[[nodiscard]] glm::vec3 poseVertex(const aliashdr_t* hdr, int pose, int vertex)
{
    const trivertx_t* verts = reinterpret_cast<const trivertx_t*>(
        reinterpret_cast<const byte*>(hdr) + hdr->vertexes);
    const trivertx_t& v = verts[pose * hdr->numverts + vertex];
    return {v.v[0], v.v[1], v.v[2]};
}

} // namespace

int zeroPose(const aliashdr_t* hdr)
{
    const maliasframedesc_t& frame = hdr->frames[0];
    if(frame.numposes <= 1)
    {
        return frame.firstpose;
    }

    // The old engine animated zero-pose groups at a fixed 10 fps.
    return frame.firstpose + static_cast<int>(cl.time / 0.1) % frame.numposes;
}

int currentPose(const entity_t& ent, const aliashdr_t* hdr)
{
    int frame = ent.frame;
    if(frame < 0 || frame >= hdr->numframes)
    {
        frame = 0;
    }

    return animatedPose(hdr->frames[frame]);
}

namespace
{

// The vertex as the renderer will draw it this frame: R_SetupAliasFrame blends from
// .previouspose to .currentpose over .lerptime, and a frame change only starts a new blend
// (from the pose shown so far) when the entity is drawn. Reads the entity without changing it.
[[nodiscard]] glm::vec3 drawnVertex(const entity_t& e, const aliashdr_t* hdr, int vertex)
{
    const int pose = currentPose(e, hdr);
    const bool lerping = r_lerpmodels.value && !(e.model->flags & MOD_NOLERP && r_lerpmodels.value != 2);
    if(!lerping || (e.lerpflags & (LERP_RESETANIM | LERP_RESETANIM2)))
    {
        return poseVertex(hdr, pose, vertex);
    }

    if(pose != e.currentpose)
    {
        return poseVertex(hdr, e.currentpose, vertex); // a new blend starts from here
    }

    float blend = 1.f;
    if(e.lerpflags & LERP_FINISH && e.lerpfinish > e.lerpstart)
    {
        blend = static_cast<float>((cl.time - e.lerpstart) / (e.lerpfinish - e.lerpstart));
    }
    else if(e.lerptime > 0.f)
    {
        blend = static_cast<float>((cl.time - e.lerpstart) / e.lerptime);
    }
    blend = CLAMP(0.f, blend, 1.f);

    return glm::mix(poseVertex(hdr, e.previouspose, vertex), poseVertex(hdr, e.currentpose, vertex), blend);
}

} // namespace

glm::vec3 posedVertex(const entity_t& ent, int anchorIndex, float zeroBlend)
{
    if(!ent.model || ent.model->type != mod_alias)
    {
        return glm::vec3{0.f};
    }

    const aliashdr_t* hdr = static_cast<const aliashdr_t*>(Mod_Extradata(ent.model));
    if(hdr->poseverttype != aliashdr_t::PV_QUAKE1 || !hdr->vertexes)
    {
        return glm::vec3{0.f};
    }

    const std::vector<int>& order = vertexOrder(ent.model);
    int vertex = 0;
    if(!order.empty())
    {
        vertex = order[CLAMP(0, anchorIndex, static_cast<int>(order.size()) - 1)];
    }
    vertex = CLAMP(0, vertex, hdr->numverts - 1);

    const glm::vec3 posed = drawnVertex(ent, hdr, vertex);
    if(zeroBlend <= 0.f)
    {
        return posed;
    }

    return glm::mix(posed, poseVertex(hdr, zeroPose(hdr), vertex), zeroBlend);
}

} // namespace qvr::anchor
