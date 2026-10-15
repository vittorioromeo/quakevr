import argparse
import os
from pathlib import Path
import subprocess

# Compile the actual collection functions against small engine fixtures. The reference is the
# pre-optimization implementation; entity identities and complete ordered index streams must match.
p = argparse.ArgumentParser()
p.add_argument('--compiler', default='clang-cl')
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
source = (root / 'Quake/vr/vr_lighting.cpp').read_text()
reference = (root / 'Misc/quakevr/shadow_caster_reference.inc').read_text()

def function(text, name):
    start = text.index('void ' + name + '(') if name != 'touches' else text.index('bool touches(')
    opening = text.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

header = r'''
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "glm/glm.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
constexpr int SURF_DRAWSKY = 1, SURF_DRAWTURB = 2, mod_brush = 1, mod_alias = 2;
struct msurface_t { int flags, numedges, vbo_firstvert; float mins[3], maxs[3]; };
struct model_t { int type; bool submodel, runtime; float mins[3], maxs[3]; msurface_t* surfaces;
    int firstmodelsurface, nummodelsurfaces; };
struct entity_t { model_t* model; double msgtime; float origin[3], scale, angles[3]; };
struct mplane_t { float normal[3], dist; };
struct mnode_t { int contents; mplane_t* plane; unsigned firstsurface, numsurfaces; mnode_t* children[2]; };
struct { model_t* worldmodel; int num_entities; double mtime[2]; } cl;
entity_t cl_entities[193], copies[16];
namespace collectfx { constexpr int maxCopies = 16; entity_t* liveCopy(int i) { return copies[i].model ? &copies[i] : nullptr; } }
namespace qvr {
namespace modelmeta { enum class Trait { Submodel }; bool has(model_t* m, Trait) { return m->submodel; } }
namespace brushes { bool isRuntime(model_t* m) { return m->runtime; } }
}
float VR_EntityScale(const entity_t* e) { return e->scale; }
struct BrushCaster { entity_t* e; size_t first, count; };
using BrushCandidates = za::SmallVector<entity_t*, 64>;
'''
body = header
for name, text in [('reference', reference), ('current', source)]:
    body += '\nnamespace ' + name + ' {\nza::Vector<uint32_t> indices;\nza::Vector<BrushCaster> brushCasters;\n'
    for f in ['addSurface', 'collectWorld', 'touches', 'collectBrushes']:
        body += function(text, f) + '\n'
    if name == 'current':
        body += function(text, 'collectBrushCandidates') + '\n'
    body += '}\n'
body += r'''
uint32_t seed = 7;
uint32_t randomValue() { seed = seed * 1664525u + 1013904223u; return seed; }
float number() { return float(int(randomValue() % 1024) - 512); }
void require(bool b, const char* what) { if(!b) { std::fprintf(stderr, "FAIL %s\n", what); std::exit(1); } }
void compare()
{
    require(reference::indices.size() == current::indices.size(), "index count");
    require(reference::indices.empty() || std::memcmp(reference::indices.data(), current::indices.data(),
        reference::indices.size() * sizeof(uint32_t)) == 0, "ordered indices");
    require(reference::brushCasters.size() == current::brushCasters.size(), "caster count");
    for(size_t i=0; i<reference::brushCasters.size(); ++i) {
        const auto& a=reference::brushCasters[i]; const auto& b=current::brushCasters[i];
        require(a.e==b.e && a.first==b.first && a.count==b.count, "caster identity/order/range");
    }
}
int main()
{
    msurface_t surfaces[384]; model_t models[50]; mplane_t planes[15]; mnode_t nodes[31];
    size_t checked=0, output=0;
    for(int round=0; round<400; ++round) {
        for(auto& s: surfaces) {
            s.flags=randomValue()%8; s.numedges=int(randomValue()%130)-2;
            s.vbo_firstvert=int(randomValue()%1000000);
            for(int a=0;a<3;++a) { s.mins[a]=number(); s.maxs[a]=s.mins[a]+float(randomValue()%256); }
        }
        for(auto& m: models) {
            m.type=mod_brush; m.submodel=randomValue()%2; m.runtime=randomValue()%2;
            m.surfaces=surfaces; m.firstmodelsurface=randomValue()%370; m.nummodelsurfaces=1+randomValue()%14;
            for(int a=0;a<3;++a) { m.mins[a]=-float(randomValue()%128); m.maxs[a]=float(randomValue()%128); }
        }
        cl.worldmodel=&models[0]; cl.mtime[0]=round; cl.num_entities=193;
        for(auto& e:cl_entities) {
            e.model=randomValue()%5 ? &models[randomValue()%50] : nullptr; e.msgtime=round-(randomValue()%3==0);
            e.scale=float(randomValue()%12)*0.25f;
            for(int a=0;a<3;++a) { e.origin[a]=number(); e.angles[a]=number(); }
        }
        for(auto& e:copies) {
            e=cl_entities[randomValue()%193];
        }
        for(int i=0;i<31;++i) {
            auto& n=nodes[i]; n.contents=i<15?0:-1;
            if(i<15) { n.plane=&planes[i]; for(float& v:planes[i].normal) v=number()/512.f;
                planes[i].dist=number(); n.firstsurface=i*24; n.numsurfaces=24;
                n.children[0]=&nodes[i*2+1]; n.children[1]=&nodes[i*2+2]; }
        }
        BrushCandidates candidates; current::collectBrushCandidates(candidates);
        // A nested portal-camera invocation owns a different list; it must not overwrite this camera's candidates.
        cl.num_entities=4; BrushCandidates nested; current::collectBrushCandidates(nested); cl.num_entities=193;
        for(int light=0;light<20;++light) {
            auto& moving=cl_entities[light+1]; moving.scale+=0.1f; moving.origin[0]+=17.f;
            moving.angles[1]+=90.f; moving.msgtime=light%3 ? round : round-1;
            glm::vec3 p{number(),number(),number()}; const float radius=float(randomValue()%1200);
            reference::indices.clear(); current::indices.clear();
            reference::collectWorld(nodes,p,radius); current::collectWorld(nodes,p,radius);
            reference::collectBrushes(p,radius,light%2); current::collectBrushes(candidates,p,radius,light%2);
            compare(); output+=current::indices.size(); ++checked;
        }
        // Mutate eligibility, models, transforms, visibility and world pointers between render invocations.
        for(int i=0;i<50;++i) if(i%3==0) models[i].type=mod_alias;
        cl.worldmodel=&models[1]; cl.num_entities=37; cl.mtime[0]+=1;
        for(int i=1;i<cl.num_entities;++i) {
            cl_entities[i].model=i%3 ? &models[(i+7)%50] : nullptr; cl_entities[i].msgtime=cl.mtime[0];
        }
        BrushCandidates next; current::collectBrushCandidates(next);
        reference::indices.clear(); current::indices.clear();
        reference::collectWorld(nodes,glm::vec3{0},512); current::collectWorld(nodes,glm::vec3{0},512);
        reference::collectBrushes(glm::vec3{0},512,false); current::collectBrushes(next,glm::vec3{0},512,false);
        compare(); output+=current::indices.size(); ++checked;
    }
    // Large polygon, negative VBO base, zero-edge and sky/water guards; exercise growth from a nonempty prefix.
    for(int flags=0;flags<8;++flags) for(int edges=-2;edges<=8192;edges+=17) {
        reference::indices.clear(); current::indices.clear();
        msurface_t prefix{}; prefix.numedges=3; prefix.vbo_firstvert=42;
        reference::addSurface(&prefix); current::addSurface(&prefix);
        msurface_t s{}; s.flags=flags; s.numedges=edges; s.vbo_firstvert=-123;
        reference::addSurface(&s); current::addSurface(&s); compare(); ++checked;
    }
    std::printf("PASS %zu exact collections, %zu generated indices; ordered caster identities/ranges identical\n",checked,output);
}
'''
scratch = root / 'scratch'
scratch.mkdir(exist_ok=True)
cpp = scratch / 'shadow_casters.cpp'
exe = scratch / 'shadow_casters.exe'
cpp.write_text(body)
subprocess.run([a.compiler, '/nologo', '/std:c++latest', '/O2', '/DNDEBUG', '/DZA_STATIC', '/clang:-fno-exceptions', str(cpp),
               '/I' + str(root/'Quake/vr/external/zancle/include'), '/I' + str(root/'Quake/vr/external'),
               '/Fo' + str(scratch/'shadow_casters.obj'), '/Fe' + str(exe), '/link',
               str(root/'Windows/VisualStudio/Build-zancle/bin/x64/Release/zancle.lib')], check=True)
subprocess.run([str(exe)], check=True)