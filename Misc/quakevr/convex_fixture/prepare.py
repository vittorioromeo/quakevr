import argparse
from pathlib import Path
import subprocess
import shutil

p = argparse.ArgumentParser()
p.add_argument("--baseline", default="a381daa432260157d4ddfd59d65f0f81298e2337")
p.add_argument("--out", type=Path, default=Path("scratch/gunhullopt"))
a = p.parse_args()
a.out.mkdir(parents=True, exist_ok=True)
header = Path("Quake/vr/vr_convex.hpp").read_text().replace('#include "vr_engine.hpp"', '''#include <glm/glm.hpp>
#include <cstdio>
#include <cstring>
#include <cmath>
#define CLAMP(a,b,c) ((b)<(a)?(a):(b)>(c)?(c):(b))
extern "C" double Sys_DoubleTime();
extern "C" void Con_Printf(const char*, ...);''')
(a.out / "vr_convex.hpp").write_text(header)
baseline = subprocess.check_output(["git", "show", a.baseline + ":Quake/vr/vr_convex.cpp"], text=True)
optimized = Path("Quake/vr/vr_convex.cpp").read_text()
for text, filename, current in [(baseline, "baseline_fixture.cpp", False), (optimized, "optimized_fixture.cpp", True)]:
    i = text.index("namespace qvr::convex")
    text = text[:i] + '''b3HullData* fixtureHull(const b3Vec3*,int,int);
void fixtureVisit(int);
void fixtureRetained(size_t);
void fixtureVoxel(const int*, const void*, const float*, size_t);
void fixtureSample(const glm::vec3&);
void fixtureVoxelTime(double);
''' + text[i:]
    text = text.replace("b3CreateHull(", "fixtureHull(")
    text = text.replace('    QVR_TRACY_SCOPE(tracyScope, "convex voxelise");', '    QVR_TRACY_SCOPE(tracyScope, "convex voxelise");\n    const double fixtureVoxelStart = Sys_DoubleTime();', 1)
    text = text.replace("                const int k = g.cellOf(p);", "#ifndef FIXTURE_BENCHMARK\n                fixtureSample(p);\n#endif\n                const int k = g.cellOf(p);", 1)
    needle = "    pass(-1);"
    assert needle in text
    text = text.replace(needle, needle + "\n    fixtureVoxelTime(Sys_DoubleTime() - fixtureVoxelStart);\n    fixtureVoxel(g.n, g.state.data(), g.dist.data(), count);", 1)
    for sig, stage in [("void regionPoints(", 0), ("[[nodiscard]] float gapOf(", 1)]:
        i = text.index(sig)
        j = text.index("{", i)
        text = text[:j+1] + f"\n    fixtureVisit({stage});" + text[j+1:]
    if current:
        needle = "        for(int c = 0; c < foundCount; c++)\n        {\n            if(oks"
        assert needle in text
        text = text.replace(needle, '''        size_t retained = 0;
        for(const ScoredCut& cut : geometry)
        {
            for(int half=0;half<2;++half)
            {
                retained += cut.points[half].capacity()*sizeof(b3Vec3);
                if(cut.hulls[half])retained += cut.hulls[half]->byteCount;
            }
        }
        fixtureRetained(retained);
''' + needle)
    (a.out / filename).write_text(text)
shutil.copy2(Path(__file__).with_name("fixture.cpp"), a.out / "fixture.cpp")
print("prepared private baseline/current convex fixture")
