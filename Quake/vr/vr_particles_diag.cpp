// vr_particles_diag.cpp -- part of vr_particles.cpp (see vr_particles.hpp, vr_particles_internal.hpp): the test and
// report commands (vr_particle_mist_test, the blood spray test, shore foam info, the light and waste reports).

#include "vr_particles_internal.hpp"

namespace qvr::particles
{
using namespace detail;


void mistTest_f()
{
    vec3_t fwd, right, up;
    AngleVectors(r_refdef.viewangles, fwd, right, up);
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const float dist = Cmd_Argc() > 1 ? static_cast<float>(atof(Cmd_Argv(1))) : 64.f;
    const int before = liveCount();
    bloodMist(eye + geom::vec(fwd) * dist, geom::vec(right), 20);
    Con_Printf("vr_gore_mist_test: %d clouds %.0f units ahead\n", liveCount() - before, static_cast<double>(dist));
}

void bloodSprayTest_f()
{
    vec3_t fwd, right, up;
    AngleVectors(r_refdef.viewangles, fwd, right, up);
    const glm::vec3 eye{r_refdef.vieworg[0], r_refdef.vieworg[1], r_refdef.vieworg[2]};
    const float dist = Cmd_Argc() > 1 ? static_cast<float>(atof(Cmd_Argv(1))) : 64.f;
    const int count = Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : 30;
    const glm::vec3 at = eye + geom::vec(fwd) * dist;
    const int before = liveCount();
    (void)spawn(at, geom::vec(right), Preset::Blood, count);
    Con_Printf("vr_water_blood_spray: %d particles %.0f units ahead, %s\n", liveCount() - before, static_cast<double>(dist),
        water::bloodInLiquid(at) ? "in a liquid" : "in the air");
}

void shoreFoamInfo_f()
{
    unsigned generation = 0;
    const za::SizeT total = water::shoreSegments(generation).size();
    Con_Printf("vr_water_foam_info: %d shore segments, %d near; last frame %d spawned, %.0f alive asked (cap %.0f), "
               "%.3f ms; %d particles live\n",
        static_cast<int>(total), static_cast<int>(shoreNear.list.size()), shoreNear.spawned, shoreNear.expectedLive,
        kShoreMaxLive, shoreNear.costMs, static_cast<int>(pool.size()));
}

// vr_particle_light_report: the last frame's lit particles, their light and its cost.
void lightReport_f()
{
    const LightStats& st = lightStats;
    // Their mean light (1: Quake's full), its lightmap's share (after its contrast) and the rest (the dynamic lights'),
    // and their colour's luma as drawn and as it would be unlit.
    double light = 0.0, baked = 0.0, colour = 0.0, base = 0.0;
    int n = 0;
    for(za::SizeT i = 0; i < za::min(lightCount, pool.size()); i++)
    {
        const Particle& p = pool[i];
        if(p.lighting != Lighting::Lit)
        {
            continue;
        }
        float k[3] = {static_cast<float>(p.lightmap[0]), static_cast<float>(p.lightmap[1]), static_cast<float>(p.lightmap[2])};
        lighting::lightCurve(k);
        const glm::vec3 f = lightOf[i];
        const float y = 0.299f * p.color.r + 0.587f * p.color.g + 0.114f * p.color.b;
        light += (f.x + f.y + f.z) / 3.f;
        baked += za::min((k[0] + k[1] + k[2]) / (3.f * 128.f), 2.f);
        base += y;
        colour += 0.299f * p.color.r * f.x + 0.587f * p.color.g * f.y + 0.114f * p.color.b * f.z;
        n++;
    }
    const double m = za::max(n, 1);
    Con_Printf("vr_particle_light %g: %d lit, %d emissive, %d dynamic lights\n", vr_particle_light.value, st.lit,
        st.emissive, st.lights);
    Con_Printf("  lit: light %.3f (lightmap %.3f, dynamic %.3f; 1 = Quake's full), colour luma %.3f (unlit %.3f)\n",
        light / m, baked / m, (light - baked) / m, colour / m, base / m);
    Con_Printf("  this frame: %llu light reaches (a dynamic light within a lit particle's reach), %d traces, %d shared, %d flicker reads, %d stale; %.3f ms (%.3f ms a frame over %d, with %.0f traces and %.0f lit)\n",
        static_cast<unsigned long long>(st.reached), st.traces, st.shared, st.refreshed, st.stale, st.ms, lightMsSum / za::max(lightFrames, 1), lightFrames,
        static_cast<double>(lightTraceSum) / za::max(lightFrames, 1),
        static_cast<double>(lightLitSum) / za::max(lightFrames, 1));
    lightMsSum = 0.0;
    lightFrames = 0;
    lightTraceSum = lightLitSum = 0;
}

constexpr const char* cellNames[CellCount] = {"circle", "explosion", "smoke", "blood", "blood mist", "lightning",
    "spark", "rock", "gun smoke", "glow", "ring", "drop", "spray", "foam", "fire"};

// vr_particle_waste_report: the fragments the particles shaded since the last report (vr_debug_particle_waste 1), and
// how many showed nothing (under half a level of 255) or next to nothing (under vr_debug_particle_waste_alpha), by
// atlas cell, for all of them and for the big ones (covering vr_debug_particle_waste_big of the view or more).
void wasteReport_f()
{
    drainWaste();
    if(!wasteCounted)
    {
        Con_Printf("vr_particle_waste_report: nothing counted (set vr_debug_particle_waste 1, with particles in view)\n");
        return;
    }
    const za::U64* counts = wasteTotals;
    wasteCounted = false;
    const auto line = [](const char* name, double shaded, double visible, double strong) {
        if(shaded <= 0.0)
        {
            return;
        }
        Con_Printf("  %-12s %10.0f shaded, %5.1f%% invisible, %5.1f%% faint (%5.1f%% wasted)\n", name, shaded,
            100.0 * (shaded - visible) / shaded, 100.0 * (visible - strong) / shaded, 100.0 * (shaded - strong) / shaded);
    };
    for(int big = 0; big < 3; big++)
    {
        // 0 every one, 1 the small, 2 the big
        double total[3] = {};
        Con_Printf("particle waste, %s (faint: under %g):\n", big == 0 ? "all" : big == 1 ? "small" : "big",
            vr_debug_particle_waste_alpha.value);
        for(int cell = 0; cell < CellCount; cell++)
        {
            double c[3] = {};
            for(int b = 0; b < 2; b++)
            {
                if(big != 0 && b != big - 1)
                {
                    continue;
                }
                for(int k = 0; k < 3; k++) c[k] += counts[(cell * 2 + b) * 3 + k];
            }
            line(cellNames[cell], c[0], c[1], c[2]);
            for(int k = 0; k < 3; k++) total[k] += c[k];
        }
        line("total", total[0], total[1], total[2]);
    }
    for(za::U64& c : wasteTotals) c = 0;
}
} // namespace qvr::particles
