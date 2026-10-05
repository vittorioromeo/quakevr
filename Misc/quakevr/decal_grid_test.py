"""Compile the actual decal grid builders and compare every GPU grid word.

Uses the pre-optimization implementation from Git as the reference, and the
vendored GLM/Zancle types. Generated files stay under build-cmake. Run on Windows
with --vcvars pointing to Visual Studio's vcvars64.bat.
"""
import argparse
from pathlib import Path
import subprocess


def builder(source, namespace):
    structure = source[source.index("struct WorldDecal\n"):source.index("static_assert(sizeof(WorldDecal)")]
    constants = source[source.index("constexpr float worldCell"):source.index("gfx::StorageBuffer worldDecalBuffer")]
    buckets = source[source.index("void worldBuckets("):source.index("// The marks and their grid made again")]
    grid = source[source.index("    // Buckets: twice"):source.index('    QVR_PROFILE("decal buffers upload")')]
    return f"namespace {namespace} {{\n{structure}{constants}{buckets}\nvoid build() {{\n{grid}\n}}\n}}\n"


HARNESS = r'''
#include <glm/glm.hpp>
#include <Zancle/Base/IntTypes.hpp>
#include <Zancle/Container/Vector.hpp>
#include <Zancle/Math/MinMax.hpp>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#define QVR_PROFILE(x) ((void)0)
@BUILDERS@
int cases = 0;
void check() {
    reference::build();
    optimized::build();
    if(reference::worldGrid.size() != optimized::worldGrid.size() ||
       !std::equal(reference::worldGrid.begin(), reference::worldGrid.end(), optimized::worldGrid.begin())) {
        std::fprintf(stderr, "Grid differs in case %d\n", cases);
        std::exit(1);
    }
    // Also require identical first-cell membership order, even for capped buckets.
    for(size_t k = 0; k < reference::worldDecals.size(); ++k) {
        reference::worldBuckets(reference::worldDecals[k], reference::worldGrid[0]);
        auto begin = optimized::worldMembershipOffsets[k];
        auto end = optimized::worldMembershipOffsets[k + 1];
        if(end - begin != reference::worldMarkBuckets.size() ||
           !std::equal(reference::worldMarkBuckets.begin(), reference::worldMarkBuckets.end(),
                       optimized::worldMemberships.begin() + begin)) {
            std::fprintf(stderr, "Membership order differs in case %d, mark %zu\n", cases, k);
            std::exit(1);
        }
    }
    ++cases;
}
void add(glm::vec3 c, glm::vec3 u, glm::vec3 v, glm::vec3 n, float size, float depth) {
    reference::WorldDecal a{glm::vec4(c, 0), glm::vec4(u, size), glm::vec4(v, size),
                            glm::vec4(n, depth), glm::vec4(0)};
    optimized::WorldDecal b{a.centre, a.u, a.v, a.n, a.time};
    reference::worldDecals.pushBack(a);
    optimized::worldDecals.pushBack(b);
}
void clear() { reference::worldDecals.clear(); optimized::worldDecals.clear(); }
int main() {
    std::mt19937 rng(20261005);
    std::uniform_real_distribution<float> pos(-4096, 4096), dir(-1, 1), size(.001f, 256);
    check(); // empty grid
    for(int t = 0; t < 160; ++t) {
        clear();
        int count = t % 4 == 0 ? 1 : 20 + rng() % 150;
        for(int k = 0; k < count; ++k) {
            glm::vec3 n = glm::normalize(glm::vec3(dir(rng), dir(rng), dir(rng)));
            glm::vec3 u = glm::normalize(glm::cross(n, glm::vec3(0, 1, 0)));
            auto v = glm::cross(n, u);
            add({pos(rng), pos(rng), pos(rng)}, u, v, n, size(rng), 1 + rng() % 32);
        }
        check();
    }
    clear();
    // Dense, colliding cells with >64 marks per bucket; maximum bucket table.
    for(int k = 0; k < 4096; ++k)
        add({float((k % 64) * 6 - 192), float((k / 64) * 6 - 192), -32},
            {1,0,0}, {0,1,0}, {0,0,1}, 128, 12);
    check();
    std::printf("Dense-case scratch capacity: stamps %zu bytes, memberships %zu bytes, offsets %zu bytes\n",
        optimized::worldBucketStamp.capacity() * sizeof(za::U32),
        optimized::worldMemberships.capacity() * sizeof(za::U32),
        optimized::worldMembershipOffsets.capacity() * sizeof(za::SizeT));
    // Force stamp rollover without resizing, with both old generation 1 stamps
    // and a freshly stamped UINT32_MAX mark present.
    std::fill(optimized::worldBucketStamp.begin(), optimized::worldBucketStamp.end(), 1u);
    optimized::worldStamp = UINT32_MAX - 1;
    check();
    clear(); // table shrinks back to minimum
    add({-32,-64,32}, {1,0,0}, {0,1,0}, {0,0,1}, .001f, 1);
    check();
    check(); // unchanged membership with reused scratch
    clear();
    check();
    std::printf("PASS: %d exact grid and membership comparisons (random rotated marks, collisions, newest-64, resize, empty, rollover)\n", cases);
}
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--vcvars", required=True)
    parser.add_argument("--reference", default="5d8fae73")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    out = root / "build-cmake/decal-opt-20261005/grid-test"
    out.mkdir(parents=True, exist_ok=True)
    old = subprocess.check_output(["git", "-c", f"safe.directory={root.as_posix()}", "show",
                                   f"{args.reference}:Quake/vr/vr_decals.cpp"], cwd=root, text=True)
    new = (root / "Quake/vr/vr_decals.cpp").read_text()
    code = HARNESS.replace("@BUILDERS@", builder(old, "reference") + builder(new, "optimized"))
    (out / "grid_test.cpp").write_text(code)
    # Fixed batch file avoids quoting nested command strings through PowerShell.
    batch = f'''@echo off
call "{Path(args.vcvars).resolve()}" >nul
if errorlevel 1 exit /b 1
cl /nologo /std:c++latest /EHsc /O2 /DZA_STATIC /DNDEBUG /I"{root / 'Quake/vr/external'}" /I"{root / 'Quake/vr/external/zancle/include'}" grid_test.cpp /Fe:grid_test.exe
if errorlevel 1 exit /b 1
grid_test.exe
'''
    (out / "run.bat").write_text(batch)
    subprocess.run(["cmd.exe", "/c", str(out / "run.bat")], cwd=out, check=True)


if __name__ == "__main__":
    main()

