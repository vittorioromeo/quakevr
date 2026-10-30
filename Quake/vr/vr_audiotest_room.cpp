// vr_audiotest_room.cpp -- vr_snd_test's occlusion, reverb, doppler, nearfield, distance and hands (see vr_audiotest.cpp).

#include "vr_audiotest_internal.hpp"

extern "C" int VR_SndSpatialize(channel_t* ch);

namespace qvr::audio::testkit
{

void addWall(Mesh& mesh, float x, float upm)
{
    const float m = upm;
    mesh.addBox(glm::vec3{(x - 0.15f) * m, -2.f * m, -1.5f * m}, glm::vec3{(x + 0.15f) * m, 2.f * m, 1.5f * m}, false,
        SurfaceMaterial::Stone, upm);
}

void makeWallSub(int, Mesh& out, void* user)
{
    addWall(out, 0.f, *static_cast<const float*>(user));
}

void testOcclusion(Result& res)
{
    Mixer m;
    Simulation sim;
    const int frame = testFrame();
    if(!m.create(testRate, frame, "") ||
        !sim.create(testRate, frame, IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, 1, 1.f, 1024))
    {
        res.check("occlusion (no Steam Audio)", false);
        return;
    }
    const float upm = units::perMetre;
    const Sound snd = makeSound(testRate, true, noise);
    const Listener lis = centred();
    Features f = baseFeatures();
    f.occlusion = za::clamp(vr_snd_occlusion.value, 0.1f, 2.f);
    SimSettings ss;
    ss.occlusion = true;
    ss.occlusionSamples = static_cast<int>(vr_snd_occlusion_samples.value);
    ss.occlusionRadius = za::clamp(vr_snd_occlusion_radius.value, 0.05f, 4.f);
    const glm::vec3 at{5.f * upm, 0.f, 0.f};
    Simulation::Source src;
    src.active = true;
    src.serial = 1;
    src.pos = toSteam(at, upm);
    const IPLCoordinateSpace3 lc = coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm);

    const auto measureWith = [&](const char* name, DirectResult& d) {
        sim.runDirectNow(lc, &src, 1, ss);
        const bool have = sim.direct(0, 1, d);
        const Render out = renderVoice(m, snd.cache(), lis, f, nullptr, 0.6f, [&](double, VoiceInput& in) {
            in.pos = at;
            in.hasDirect = have;
            in.direct = d;
        });
        writeWav(name, out);
        const int from = testRate / 5;
        return measure(out.l.data() + from, out.r.data() + from, static_cast<int>(out.l.size()) - from, testRate);
    };

    // A floor only, then a wall between.
    Mesh open;
    open.addBox(glm::vec3{-20.f * upm, -20.f * upm, -2.f * upm}, glm::vec3{20.f * upm, 20.f * upm, -1.8f * upm}, false,
        SurfaceMaterial::Stone, upm);
    Mesh walled = open;
    addWall(walled, 2.5f, upm);
    sim.buildSceneNow(ZA_MOVE(open));
    DirectResult dOpen, dWall, dMoved, dAway;
    const Levels a = measureWith("occl_open", dOpen);
    sim.buildSceneNow(ZA_MOVE(walled));
    const Levels b = measureWith("occl_wall", dWall);
    Con_Printf("snd_test occlusion: open: occlusion %.2f, %.1f dB (lows %.1f, highs %.1f); behind the wall: occlusion %.2f, "
               "transmission %.2f %.2f %.2f, %.1f dB (lows %.1f, highs %.1f); strength %.2f\n",
        dOpen.occlusion, a.rms, a.low, a.high, dWall.occlusion, dWall.transmission[0], dWall.transmission[1],
        dWall.transmission[2], b.rms, b.low, b.high, f.occlusion);
    res.check("occlusion: behind a wall quieter (> 6 dB)", a.rms - b.rms > 6.f);
    res.check("occlusion: behind a wall duller (highs lose > 3 dB more than lows)", (a.high - b.high) - (a.low - b.low) > 3.f);

    // A brush model (an instance of a sub-scene) moved in the way, then up out of it: a door opening.
    Mesh floorOnly;
    floorOnly.addBox(glm::vec3{-20.f * upm, -20.f * upm, -2.f * upm}, glm::vec3{20.f * upm, 20.f * upm, -1.8f * upm}, false,
        SurfaceMaterial::Stone, upm);
    sim.buildSceneNow(ZA_MOVE(floorOnly));
    const auto place = [&](float x, float up) {
        IPLMatrix4x4 t{};
        t.elements[0][0] = t.elements[1][1] = t.elements[2][2] = t.elements[3][3] = 1.f;
        const IPLVector3 o = toSteam(glm::vec3{x * upm, 0.f, up * upm}, upm);
        t.elements[0][3] = o.x;
        t.elements[1][3] = o.y;
        t.elements[2][3] = o.z;
        float u = upm;
        sim.beginInstances();
        sim.placeInstance(1, 1, t, makeWallSub, &u);
        sim.update(lc, &src, 0, SimSettings{}, 0.0); // (applied: nothing runs)
    };
    place(2.5f, 0.f);
    measureWith("occl_door_shut", dMoved);
    place(2.5f, 10.f);
    measureWith("occl_door_open", dAway);
    Con_Printf("snd_test occlusion: a door (instance) in the way: occlusion %.2f; moved up 10 m: %.2f\n", dMoved.occlusion,
        dAway.occlusion);
    res.check("occlusion: a moving brush model occludes, then not once moved", dMoved.occlusion < 0.3f && dAway.occlusion > 0.9f);
}

void testReverb(Result& res)
{
    const float upm = units::perMetre;
    const Listener lis = centred();
    const Sound snd = makeSound(testRate * 4, false, click);
    const IPLCoordinateSpace3 lc = coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm);
    struct Room
    {
        const char* name;
        glm::vec3 size; // metres
    };
    const Room rooms[2] = {{"small", {3.f, 3.f, 2.5f}}, {"hall", {30.f, 20.f, 12.f}}};
    const IPLReflectionEffectType types[3] = {
        IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, IPL_REFLECTIONEFFECTTYPE_HYBRID, IPL_REFLECTIONEFFECTTYPE_CONVOLUTION};
    const char* typeNames[3] = {"parametric", "hybrid", "convolution"};
    for(int t = 0; t < 3; t++)
    {
        float measured[2]{};
        float simulated[2]{};
        for(int k = 0; k < 2; k++)
        {
            Mixer m;
            Simulation sim;
            const int frame = testFrame();
            if(!m.create(testRate, frame, "") || !sim.create(testRate, frame, types[t], 1, 2.f, 4096))
            {
                res.check("reverb (no Steam Audio)", false);
                return;
            }
            m.setReverb(types[t], 1, 2.f);
            Mesh room;
            const glm::vec3 half = rooms[k].size * 0.5f * upm;
            room.addBox(-half, half, true, SurfaceMaterial::Stone, upm);
            sim.buildSceneNow(ZA_MOVE(room));
            SimSettings ss;
            ss.reverb = true;
            ss.rays = 4096;
            ss.bounces = 32;
            ss.duration = 2.f;
            ss.order = 1;
            sim.runReflectionsNow(lc, ss);
            IPLReflectionEffectParams p{};
            if(!sim.reflections(p))
            {
                res.check("reverb (no simulation)", false);
                return;
            }
            simulated[k] = p.reverbTimes[1];
            Features f = baseFeatures();
            f.reverb = 1.f;
            const glm::vec3 at = lis.fwd * upm; // 1 m ahead
            const Render out = renderVoice(m, snd.cache(), lis, f, &p, 3.f, [&](double, VoiceInput& in) { in.pos = at; });
            writeWav((za::String{"reverb_"} + typeNames[t] + "_" + rooms[k].name).cStr(), out);
            const int from = testRate / 50; // (after the dry click: 20 ms)
            measured[k] = rt60(out, from);
            // The wet energy after 20 ms against the dry click's (its first 5 ms).
            // The energy after the dry click (its first 5 ms) in three windows, against the click's.
            double dry = 0, early = 0, mid = 0, late = 0;
            for(int i = 0; i < static_cast<int>(out.l.size()); i++)
            {
                const double e = static_cast<double>(out.l[i]) * out.l[i] + static_cast<double>(out.r[i]) * out.r[i];
                const double t = static_cast<double>(i) / testRate;
                (t < 0.005 ? dry : t < 0.1 ? early : t < 0.5 ? mid : late) += e;
            }
            const auto rel = [&](double e) { return 10.0 * za::log10(za::max(e, 1e-9) / za::max(dry, 1e-9)); };
            Con_Printf("snd_test reverb %s %s (%.0f x %.0f x %.0f m): simulated RT60 %.2f %.2f %.2f s, measured %.2f s; "
                       "energy after the click, 5-100 ms %+.1f dB, 0.1-0.5 s %+.1f dB, 0.5-3 s %+.1f dB\n",
                typeNames[t], rooms[k].name, rooms[k].size.x, rooms[k].size.y, rooms[k].size.z, p.reverbTimes[0],
                p.reverbTimes[1], p.reverbTimes[2], measured[k], rel(early), rel(mid), rel(late));
        }
        char name[96];
        q_snprintf(name, sizeof name, "reverb %s: the hall rings longer than the small room (x2)", typeNames[t]);
        // (Convolution has no decay times of its own: the measured one only.)
        const bool simulatedOk = types[t] == IPL_REFLECTIONEFFECTTYPE_CONVOLUTION || simulated[1] > 2.f * simulated[0];
        res.check(name, simulatedOk && measured[1] > 2.f * za::max(measured[0], 0.01f));
    }
}

// The reverb's left and right: a room with a wall 1 m to the listener's right (the others 4-9 m away). The reverb is
// listener-centred (the same whatever a sound's direction), so its balance is the room's round the head: its first 50 ms
// (the near wall's echo; the voice itself silent) louder in the right ear; with the listener turned (the wall behind, on
// the left, ahead), balanced, in the left, balanced. (Later on the reverb is diffuse: balanced but for the HRTF's own
// lean, its ambisonic decode a little louder on the right at 2-8 kHz.)
void testReverbSide(Result& res)
{
    const float upm = units::perMetre;
    const Sound snd = makeSound(testRate, false, click);
    float side[4]{};
    for(int turn = 0; turn < 4; turn++)
    {
        // Facing +x (the wall on the right), +y (behind), -x (on the left), -y (ahead).
        Listener lis = centred();
        const float yaw = static_cast<float>(turn) * 3.14159265f * 0.5f;
        lis.fwd = glm::vec3{za::cos(yaw), za::sin(yaw), 0.f};
        lis.right = glm::vec3{za::sin(yaw), -za::cos(yaw), 0.f};
        Mixer m;
        Simulation sim;
        const int frame = testFrame();
        if(!m.create(testRate, frame, "") || !sim.create(testRate, frame, IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, 1, 1.f, 4096))
        {
            res.check("reverb side (no Steam Audio)", false);
            return;
        }
        m.setReverb(IPL_REFLECTIONEFFECTTYPE_CONVOLUTION, 1, 1.f);
        Mesh room;
        room.addBox(glm::vec3{-5.f, -1.f, -1.5f} * upm, glm::vec3{5.f, 9.f, 1.5f} * upm, true, SurfaceMaterial::Stone, upm);
        sim.buildSceneNow(ZA_MOVE(room));
        SimSettings ss;
        ss.reverb = true;
        ss.rays = 4096;
        ss.bounces = 16;
        ss.duration = 1.f;
        ss.order = 1;
        sim.runReflectionsNow(coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm), ss);
        IPLReflectionEffectParams p{};
        if(!sim.reflections(p))
        {
            res.check("reverb side (no simulation)", false);
            return;
        }
        Features f = baseFeatures();
        f.reverb = 1.f;
        f.hrtfGain = 0.f; // (the reverb alone)
        const glm::vec3 at = lis.fwd * upm;
        const Render out = renderVoice(m, snd.cache(), lis, f, &p, 0.2f, [&](double, VoiceInput& in) { in.pos = at; });
        double el = 0.0;
        double er = 0.0;
        for(int i = 0; i < testRate / 20; i++)
        {
            el += static_cast<double>(out.l[i]) * out.l[i];
            er += static_cast<double>(out.r[i]) * out.r[i];
        }
        side[turn] = static_cast<float>(10.0 * za::log10(za::max(er, 1e-9) / za::max(el, 1e-9)));
    }
    Con_Printf("snd_test reverb side: the reverb's first 50 ms, right minus left, with a wall 1 m to the right %+.1f dB, "
               "behind %+.1f, to the left %+.1f, ahead %+.1f\n",
        side[0], side[1], side[2], side[3]);
    // (Steam Audio's first-order listener-centred reverb leans only a little: about +-0.5 dB.)
    res.check("reverb side: leans to the near wall (the wall right against left > 0.5 dB, behind and ahead within 1 dB)",
        side[0] > 0.f && side[2] < 0.f && side[0] - side[2] > 0.5f && za::abs(side[1]) < 1.f && za::abs(side[3]) < 1.f);
}

void testDoppler(Result& res)
{
    Mixer m;
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("doppler (no Steam Audio)", false);
        return;
    }
    const float upm = units::perMetre;
    const Sound snd = makeSound(441, true, sine1k); // (ten periods: loops seamlessly)
    const Listener lis = centred();
    const float speed = 1000.f; // units/s: a rocket
    const float seconds = 3.f;
    const auto run = [&](float scale, double& coming, double& going) {
        Features f = baseFeatures();
        f.doppler = scale;
        const Render out = renderVoice(m, snd.cache(), lis, f, nullptr, seconds, [&](double t, VoiceInput& in) {
            const float x = -1500.f + speed * static_cast<float>(t);
            in.pos = glm::vec3{x, -1.f * upm, 0.f};
            in.vel = glm::vec3{speed, 0.f, 0.f};
        });
        if(scale > 0.f)
        {
            writeWav("doppler", out);
        }
        coming = pitch(out.l, testRate / 5, testRate / 3);
        going = pitch(out.l, static_cast<int>(out.l.size()) - testRate / 2, testRate / 3);
    };
    double coming = 0, going = 0, coming0 = 0, going0 = 0;
    run(1.f, coming, going);
    run(0.f, coming0, going0);
    const double c = 343.0 * upm;
    const double expectComing = 1000.0 * c / (c - speed * 0.99);
    const double expectGoing = 1000.0 * c / (c + speed * 0.99);
    Con_Printf("snd_test doppler: %.0f units/s (%.1f m/s): coming %.1f Hz (expected %.1f), going %.1f Hz (expected %.1f); "
               "off: %.1f, %.1f Hz\n",
        speed, speed / upm, coming, expectComing, going, expectGoing, coming0, going0);
    res.check("doppler: higher coming, lower going (within 2%)",
        za::abs(coming / expectComing - 1.0) < 0.02 && za::abs(going / expectGoing - 1.0) < 0.02);
    res.check("doppler: off leaves the pitch (within 0.5%)", za::abs(coming0 / 1000.0 - 1.0) < 0.005 &&
                                                                 za::abs(going0 / 1000.0 - 1.0) < 0.005);
}

void testNearField(Result& res)
{
    Mixer m;
    if(!m.create(testRate, testFrame(), ""))
    {
        res.check("nearfield (no Steam Audio)", false);
        return;
    }
    const float upm = units::perMetre;
    const Sound snd = makeSound(testRate, true, noise);
    const Listener lis = centred();
    const auto ild = [&](float metres, float strength) {
        Features f = baseFeatures();
        f.nearfield = strength;
        const glm::vec3 at = lis.right * (metres * upm);
        const Render out = renderVoice(m, snd.cache(), lis, f, nullptr, 0.5f, [&](double, VoiceInput& in) { in.pos = at; });
        const int from = testRate / 10;
        const Levels lv = measure(out.l.data() + from, out.r.data() + from, static_cast<int>(out.l.size()) - from, testRate);
        return lv.right - lv.left;
    };
    const float strength = za::max(0.25f, za::clamp(vr_snd_nearfield.value, 0.f, 2.f));
    const float nearOn = ild(0.2f, strength);
    const float farOn = ild(2.f, strength);
    const float nearOff = ild(0.2f, 0.f);
    Con_Printf("snd_test nearfield: ILD at 0.2 m %+.1f dB (near field off: %+.1f), at 2 m %+.1f dB\n", nearOn, nearOff, farOn);
    res.check("nearfield: a sound at the ear has a larger ILD than at 2 m (> 4 dB more)", nearOn - farOn > 4.f);
    res.check("nearfield: off, the same ILD near and far (within 1.5 dB)", za::abs(nearOff - farOn) < 1.5f);
}

// In a map: how loud a sound is by its distance from the head, Quake's panning (as vr_snd_spatial 0 mixes it) against
// the spatial mix (the settings' HRTF, occlusion from this map's own scene, the air, the near field; no reverb), each
// in dB against the sound itself. The sounds in sight of the head (the world's line of sight), round it, at three
// heights: the head's, a monster's origin (24 units over the floor), on the floor (2 units over it: an item) and half a
// unit into it (a prop's contact as it knocks: hidden by the floor, 21 dB under Quake, till outOfSolid). The author found sounds 5-10 m away too quiet (NOTES.md vrfiringrange_2026-10-01_00-29-12).
void testDistance(Result& res)
{
    if(!cl.worldmodel || cls.state != ca_connected || !shm)
    {
        Con_Printf("snd_test distance: skipped (needs a map and sound)\n");
        return;
    }
    Mixer m;
    Simulation sim;
    const int frame = testFrame();
    if(!m.create(testRate, frame, "") || !sim.create(testRate, frame, IPL_REFLECTIONEFFECTTYPE_PARAMETRIC, 1, 1.f, 1024))
    {
        res.check("distance (no Steam Audio)", false);
        return;
    }
    Features f = featuresFromCvars();
    f.reverb = 0.f;
    f.doppler = 0.f;
    const float upm = f.unitsPerMetre;
    Mesh mesh;
    appendBrushModel(cl.worldmodel, mesh, upm);
    sim.buildSceneNow(ZA_MOVE(mesh));
    Listener lis;
    lis.pos = geom::vec(listener_origin);
    lis.fwd = geom::vec(listener_forward);
    lis.right = geom::vec(listener_right);
    lis.up = geom::vec(listener_up);
    const IPLCoordinateSpace3 lc = coordinates(lis.pos, lis.fwd, lis.right, lis.up, upm);
    SimSettings ss;
    ss.occlusion = f.occlusion > 0.f;
    ss.occlusionSamples = static_cast<int>(vr_snd_occlusion_samples.value);
    ss.occlusionRadius = za::clamp(vr_snd_occlusion_radius.value, 0.05f, 4.f);
    ss.air = f.air;

    const Sound snd = makeSound(testRate, true, noise);
    const float monoDb = [&] {
        double s = 0;
        const short* d = reinterpret_cast<const short*>(snd.cache()->data);
        for(int i = 0; i < testRate; i++)
        {
            s += static_cast<double>(d[i]) * d[i];
        }
        return db(s / testRate);
    }();
    const auto clear = [&](const glm::vec3& a, const glm::vec3& b) {
        trace_t trace;
        ZA_MEMSET(&trace, 0, sizeof trace);
        trace.fraction = 1.f;
        vec3_t from{a.x, a.y, a.z};
        vec3_t to{b.x, b.y, b.z};
        SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0.f, 1.f, from, to, &trace);
        return trace.fraction >= 1.f && !trace.allsolid && !trace.startsolid;
    };
    const float distances[] = {1.f, 2.f, 3.f, 5.f, 7.f, 10.f, 15.f, 20.f};
    const char* heights[] = {"head", "monster", "floor", "contact"};
    const float distMult = 1.f / 1000.f; // ATTN_NORM
    Con_Printf("snd_test distance: from %.0f %.0f %.0f, %.2f units a metre; dB against the sound, Quake's panning | the "
               "spatial mix (without occlusion) | occlusion, the air's highs\n",
        lis.pos.x, lis.pos.y, lis.pos.z, upm);
    float worst = 0.f;
    for(int h = 0; h < 4; h++)
    {
        for(const float d : distances)
        {
            constexpr int around = 36;
            Simulation::Source src[around]{};
            glm::vec3 at[around];
            int n = 0;
            for(int k = 0; k < around; k++)
            {
                const float a = static_cast<float>(k) * 2.f * 3.14159265f / around;
                glm::vec3 p = lis.pos + d * upm * (za::cos(a) * lis.fwd + za::sin(a) * lis.right);
                if(h > 0)
                {
                    // Down to the floor under it.
                    trace_t trace;
                    ZA_MEMSET(&trace, 0, sizeof trace);
                    trace.fraction = 1.f;
                    vec3_t from{p.x, p.y, p.z};
                    vec3_t to{p.x, p.y, p.z - 512.f};
                    SV_RecursiveHullCheck(cl.worldmodel->hulls, 0, 0.f, 1.f, from, to, &trace);
                    if(trace.fraction >= 1.f || trace.startsolid)
                    {
                        continue;
                    }
                    p.z = trace.endpos[2] + (h == 1 ? 24.f : h == 2 ? 2.f : -0.5f);
                }
                if(!clear(lis.pos, h == 3 ? p + glm::vec3{0.f, 0.f, 1.f} : p) || za::abs(glm::length(p - lis.pos) / upm - d) > 0.5f * d)
                {
                    continue;
                }
                at[n] = p;
                src[n].active = true;
                src[n].serial = 1;
                src[n].pos = toSteam(outOfSolid(p, lis.pos), upm); // (as the game places it)
                n++;
            }
            if(n == 0)
            {
                Con_Printf("snd_test distance: %4.0f m, %-7s: no place in sight\n", d, heights[h]);
                continue;
            }
            sim.runDirectNow(lc, src, n, ss);
            double quake = 0, spatial = 0, open = 0, occ = 0, air = 0;
            int guessed = 0; // hidden by the world's line of sight (the stand-in till the first result)
            for(int k = 0; k < n; k++)
            {
                const glm::vec3 dir = glm::normalize(at[k] - lis.pos);
                const float dot = glm::dot(dir, lis.right);
                const float scale = za::max(0.f, 1.f - glm::length(at[k] - lis.pos) * distMult);
                const float power = 0.5f * ((1.f - dot) * (1.f - dot) + (1.f + dot) * (1.f + dot)) * scale * scale;
                quake += 10.0 * za::log10(za::max(power, 1e-12f));
                DirectResult dr;
                const bool have = sim.direct(k, 1, dr);
                for(int withOcclusion = 1; withOcclusion >= 0; withOcclusion--)
                {
                    Features fk = f;
                    fk.occlusion = withOcclusion ? f.occlusion : 0.f;
                    const Render out = renderVoice(m, snd.cache(), lis, fk, nullptr, 0.25f, [&](double, VoiceInput& in) {
                        in.pos = at[k];
                        in.gain = scale;
                        in.hasDirect = have;
                        in.direct = dr;
                    });
                    const int from = testRate / 20;
                    const Levels lv = measure(out.l.data() + from, out.r.data() + from,
                        static_cast<int>(out.l.size()) - from, testRate);
                    (withOcclusion ? spatial : open) += lv.rms - monoDb;
                }
                guessed += clear(lis.pos, outOfSolid(at[k], lis.pos)) ? 0 : 1;
                occ += have ? dr.occlusion : 1.f;
                air += have ? dr.air[2] : 1.f;
            }
            quake /= n;
            spatial /= n;
            open /= n;
            Con_Printf("snd_test distance: %4.0f m, %-7s (%2d places): Quake %+6.1f dB | spatial %+6.1f dB (%+6.1f) | "
                       "occlusion %.2f, air %.2f, %d hidden by the stand-in; spatial - Quake %+5.1f dB\n",
                d, heights[h], n, quake, spatial, open, occ / n, air / n, guessed, spatial - quake);
            worst = za::max(worst, static_cast<float>(za::abs(spatial - quake)));
        }
    }
    res.check("distance: in sight, as loud as Quake's mix (within 3 dB; a contact too)", worst < 3.f);
}

void testHands(Result& res)
{
    const hands::State& hs = hands::current();
    if(!hs.valid || !VR_IsActive() || cl.viewentity <= 0 || !shm)
    {
        Con_Printf("snd_test hands: skipped (needs a map, VR and sound)\n");
        return;
    }
    for(int k = 0; k < 4; k++)
    {
        // The weapon channels (1, 5: from the muzzle), then the hands' own (SND_CHAN_HAND, SND_CHAN_HAND2: from the hand).
        const int h = k % 2 == 0 ? HAND_MAIN : HAND_OFF;
        const bool own = k >= 2;
        channel_t ch{};
        ch.entnum = cl.viewentity;
        ch.entchannel = own ? (h == HAND_MAIN ? SND_CHAN_HAND : SND_CHAN_HAND2) : (h == HAND_MAIN ? 1 : 5);
        ch.master_vol = 255;
        ch.dist_mult = 1.f / 1000.f;
        VectorCopy(listener_origin, ch.origin);
        const int handled = VR_SndSpatialize(&ch);
        const glm::vec3 at{ch.origin[0], ch.origin[1], ch.origin[2]};
        const glm::vec3 hand = !own && hs.muzzleValid[h] ? hs.muzzle[h] : hs.pos[h];
        const glm::vec3 head{listener_origin[0], listener_origin[1], listener_origin[2]};
        const glm::vec3 right{listener_right[0], listener_right[1], listener_right[2]};
        const float side = glm::dot(hand - head, right);
        Con_Printf("snd_test hands: %s hand's channel %d: from %.0f %.0f %.0f (the hand %.1f units off it, %.1f from the "
                   "head, %.1f to the right), left %d, right %d\n",
            h == HAND_MAIN ? "main" : "off", ch.entchannel, at.x, at.y, at.z, glm::length(at - hand), glm::length(at - head),
            side, ch.leftvol, ch.rightvol);
        const bool panned = za::abs(side) < 2.f || (side > 0.f) == (ch.rightvol > ch.leftvol);
        res.check(own ? (h == HAND_MAIN ? "hands: the main hand's own channel plays from the hand" : "hands: the off hand's own from it")
                      : (h == HAND_MAIN ? "hands: the main hand's weapon channel plays from it" : "hands: the off hand's from it"),
            handled && glm::length(at - hand) < 0.5f && panned);
    }
}

} // namespace qvr::audio::testkit
