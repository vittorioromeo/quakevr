// vr_gunsfx_test.inc -- vr_gunsfx_test (included by vr_gunsfx.cpp, after its own code: it reads its state). Run it in a
// map with the shotgun held (QC impulse 9 first): every shot's layers made and started, the variation within its
// percent (none at 0), the rooms told apart and their tails longer the bigger the room, a small room and the open sky
// both found in the map with different tails, the casings' sounds by surface, the dry click (a tick in the hand, silent
// at 0), every weapon's punch curve (a spike, a falling tail as long as its recoil's way back: Recovery 2x twice as
// long, Haptic Punch 0 none), an action's place, and nothing with vr_gunsfx 0. Every setting put back.
// One verdict line: "vr_gunsfx_test: N passed, M failed".

namespace qvr::gunsfx
{
namespace
{

struct KeepCvar
{
    cvar_t* v;
    za::String old;
    explicit KeepCvar(cvar_t* c) : v{c}, old{c->string} {}
    KeepCvar(const KeepCvar&) = delete;
    KeepCvar& operator=(const KeepCvar&) = delete;
    ~KeepCvar() { Cvar_SetQuick(v, old.cStr()); }
};

[[nodiscard]] int soundLength(const char* name)
{
    sfx_t* sfx = S_PrecacheSound(name);
    const sfxcache_t* sc = sfx ? S_LoadSound(sfx) : nullptr;
    return sc ? sc->length : 0;
}

// Channels playing `name` (just started: Quake's own channels, the voices' too).
[[nodiscard]] int playing(const char* name)
{
    int n = 0;
    for(int i = NUM_AMBIENTS; i < total_channels; i++)
    {
        n += snd_channels[i].sfx && !q_strcasecmp(snd_channels[i].sfx->name, name) ? 1 : 0;
    }
    return n;
}

void test_f()
{
    constexpr const char* N = "vr_gunsfx_test";
    testkit::Tally t;
    char what[256];
    const auto check = [&](bool ok, const char* fmt, auto... args) {
        q_snprintf(what, sizeof(what), fmt, args...);
        t.check(ok, N, what);
    };
    if(cls.state != ca_connected || !cl.worldmodel || cl.viewentity <= 0 || !shm)
    {
        check(false, "a map loaded, sound on (run.sh -Sound: the sounds are made as they load)");
        t.verdict(N);
        return;
    }
    const KeepCvar keep[] = {KeepCvar{&vr_gunsfx}, KeepCvar{&vr_gunsfx_layers}, KeepCvar{&vr_gunsfx_tail},
        KeepCvar{&vr_gunsfx_vary}, KeepCvar{&vr_gunsfx_casings}, KeepCvar{&vr_gunsfx_dryfire}, KeepCvar{&vr_gunsfx_action},
        KeepCvar{&vr_gunsfx_punch}};
    for(cvar_t* c : {&vr_gunsfx, &vr_gunsfx_layers, &vr_gunsfx_tail, &vr_gunsfx_casings, &vr_gunsfx_dryfire,
            &vr_gunsfx_action, &vr_gunsfx_punch})
    {
        Cvar_SetQuick(c, "1");
    }
    Cvar_SetQuick(&vr_gunsfx_vary, "4");
    const bool sound = true; // (checked above)
    const glm::vec3 eye = geom::vec(cl_entities[cl.viewentity].origin) + glm::vec3{0.f, 0.f, 22.f};
    const float org[3] = {eye.x, eye.y, eye.z};

    // 1. Every layer made from the game's sounds; every shot's layers started with it, from the hand that fired.
    int unmade = 0;
    for(const LayeredSound& s : gunLayers)
    {
        if(soundLength(s.name) <= 0)
        {
            unmade++;
            Con_Printf("%s: no %s\n", N, s.name);
        }
    }
    check(unmade == 0, "every layer made (%d not)", unmade);
    int missing = 0;
    for(const ShotSound& s : shots)
    {
        clear(); // (each shot's tail not held back by the last's)
        sfx_t* sfx = S_PrecacheSound(s.name);
        const int thumps = playing(thumpNames[s.kind]);
        float vol = 1.f, pitch = 1.f;
        const int before = state.lastShot.count;
        (void)VR_SndShot(cl.viewentity, 1, sfx, org, &vol, 1.f, &pitch);
        const LastShot& l = state.lastShot;
        const bool ok = l.count == before + 1 && l.thump == thumpNames[s.kind] && l.mech == s.mech && l.tail &&
                        (!sound || playing(thumpNames[s.kind]) > thumps);
        if(!ok)
        {
            missing++;
            Con_Printf("%s: %s: thump %s mech %s tail %s\n", N, s.name, l.thump ? l.thump : "-", l.mech ? l.mech : "-",
                l.tail ? l.tail : "-");
        }
    }
    check(missing == 0, "every shot's layers and tail started (%d shots short)", missing);
    {
        float vol = 1.f, pitch = 1.f;
        const int before = state.lastShot.count;
        (void)VR_SndShot(cl.viewentity, 2, S_PrecacheSound("weapons/guncock.wav"), org, &vol, 1.f, &pitch);
        check(state.lastShot.count == before, "a shot's sound on another channel (CHAN_VOICE) layers nothing");
    }

    // 2. The variation: within 4 % (the volume 2 %), not all alike; none at 0.
    float lo = 10.f, hi = 0.f, vlo = 10.f, vhi = 0.f;
    sfx_t* sg = S_PrecacheSound("weapons/guncock.wav");
    for(int i = 0; i < 40; i++)
    {
        float vol = 1.f, pitch = 1.f;
        (void)VR_SndShot(cl.viewentity, 1, sg, org, &vol, 1.f, &pitch);
        lo = za::min(lo, pitch), hi = za::max(hi, pitch), vlo = za::min(vlo, vol), vhi = za::max(vhi, vol);
    }
    check(lo >= 0.96f && hi <= 1.04f && hi - lo > 0.02f, "pitch varied within 4%% (%.3f .. %.3f)", lo, hi);
    check(vlo >= 0.98f && vhi <= 1.02f && vhi - vlo > 0.01f, "volume varied within 2%% (%.3f .. %.3f)", vlo, vhi);
    Cvar_SetQuick(&vr_gunsfx_vary, "0");
    lo = 10.f, hi = 0.f;
    for(int i = 0; i < 10; i++)
    {
        float vol = 1.f, pitch = 1.f;
        (void)VR_SndShot(cl.viewentity, 1, sg, org, &vol, 1.f, &pitch);
        lo = za::min(lo, pitch), hi = za::max(hi, pitch);
    }
    check(lo == 1.f && hi == 1.f, "no variation at 0 (%.3f .. %.3f)", lo, hi);
    Cvar_SetQuick(&vr_gunsfx_vary, "4");

    // 3. The rooms: told apart; their tails longer the bigger and opener the room; in this map a small room and the
    // open sky found round the player, their shots' tails different.
    check(classify(150.f, 0.f) == RoomSmall && classify(900.f, 0.2f) == RoomLarge && classify(300.f, 0.6f) == RoomOutdoor,
        "small, large and outdoor rooms told apart");
    int longer = 0;
    const float rate = static_cast<float>(shm->speed); // (loaded at the mix's rate)
    for(int k = 0; k < KindCount; k++)
    {
        const int a = soundLength(tailNames[k][RoomSmall]), b = soundLength(tailNames[k][RoomLarge]);
        const int c = soundLength(tailNames[k][RoomOutdoor]);
        longer += a > 0 && a < b && b < c ? 1 : 0;
        Con_Printf("%s: %s tails %.2f / %.2f / %.2f s\n", N, k == KindShot ? "shot" : k == KindLight ? "light" : "heavy",
            a / rate, b / rate, c / rate);
    }
    check(longer == KindCount, "each kind's tail longer in a large room, longest outdoors (%d of 3)", longer);
    glm::vec3 smallAt{0.f}, outAt{0.f};
    bool foundSmall = false, foundOut = false;
    for(int y = -8; y <= 8 && !(foundSmall && foundOut); y++)
    {
        for(int x = -8; x <= 8 && !(foundSmall && foundOut); x++)
        {
            const glm::vec3 p = eye + glm::vec3{x * 160.f, y * 160.f, 0.f};
            if(worldtrace::clientContents(p) != CONTENTS_EMPTY)
            {
                continue;
            }
            const RoomProbe r = probeRoom(p);
            if(r.room == RoomSmall && !foundSmall)
            {
                foundSmall = true, smallAt = p;
            }
            if(r.room == RoomOutdoor && !foundOut)
            {
                foundOut = true, outAt = p;
            }
        }
    }
    check(foundSmall && foundOut, "a small room and the open sky found round the player (%d, %d)", foundSmall ? 1 : 0,
        foundOut ? 1 : 0);
    if(foundSmall && foundOut)
    {
        const char* tails[2] = {nullptr, nullptr};
        const glm::vec3 at[2] = {smallAt, outAt};
        for(int i = 0; i < 2; i++)
        {
            clear();
            float vol = 1.f, pitch = 1.f;
            const float o[3] = {at[i].x, at[i].y, at[i].z};
            (void)VR_SndShot(cl.viewentity, 1, sg, o, &vol, 1.f, &pitch);
            tails[i] = state.lastShot.tail;
        }
        Con_Printf("%s: small room at %.0f %.0f %.0f: %s; outdoors at %.0f %.0f %.0f: %s\n", N, smallAt.x, smallAt.y,
            smallAt.z, tails[0] ? tails[0] : "-", outAt.x, outAt.y, outAt.z, tails[1] ? tails[1] : "-");
        check(tails[0] == tailNames[KindShot][RoomSmall] && tails[1] == tailNames[KindShot][RoomOutdoor],
            "the same shot's tail short in the small room, with the echo outdoors");
    }

    // 4. Casings by surface: under the player its floor's material; metal and wood their own sounds, stone the tinks.
    {
        const trace_t tr = worldtrace::world(eye, eye - glm::vec3{0.f, 0.f, 256.f}, false);
        const glm::vec3 floor = worldtrace::endPos(tr);
        const msurface_t* s = physsound::surfaceOnSegment(cl.worldmodel, floor + glm::vec3{0.f, 0.f, 2.f},
            floor - glm::vec3{0.f, 0.f, 6.f});
        const char* tex = s && s->texinfo ? cl.worldmodel->textures[s->texinfo->texnum]->name : "";
        const debris::Material m = debris::materialOf(tex);
        const char* want = m == debris::Material::Metal ? metalCasings[1] : m == debris::Material::Wood ? woodCasings[1] : nullptr;
        const char* got = casingSound(floor, glm::vec3{0.f, 0.f, 1.f}, 1);
        Con_Printf("%s: the floor's texture %s (%s): %s\n", N, tex, debris::materialName(m), got ? got : "the tinks");
        check(tr.fraction < 1.f && got == want, "a casing on the floor sounds of its material (%s)", tex);
        int casings = 0;
        for(int k = 0; k < 3; k++)
        {
            casings += soundLength(metalCasings[k]) > 0 && soundLength(woodCasings[k]) > 0 ? 1 : 0;
        }
        check(casings == 3 && debris::materialOf("metal1_1") == debris::Material::Metal &&
                debris::materialOf("wood1_1") == debris::Material::Wood,
            "metal and wood casings' sounds made (%d of 3)", casings);
        Cvar_SetQuick(&vr_gunsfx_casings, "0");
        check(casingSound(floor, glm::vec3{0.f, 0.f, 1.f}, 1) == nullptr, "casings off: the tinks everywhere");
        Cvar_SetQuick(&vr_gunsfx_casings, "1");
    }

    // 5. The dry click: a tick in the hand that clicked; silent at 0.
    {
        sfx_t* click = S_PrecacheSound(dryClickName);
        const int pulses = state.lastPulse[HAND_OFF].count;
        float vol = 1.f, pitch = 1.f;
        const int played = VR_SndShot(cl.viewentity, SND_CHAN_HAND2, click, org, &vol, 1.f, &pitch);
        check(played == 1 && state.lastDry.hand == HAND_OFF && state.lastPulse[HAND_OFF].count == pulses + 1 &&
                state.lastPulse[HAND_OFF].amplitude > 0.f,
            "an empty gun's click plays, a tick in its hand (the off hand)");
        Cvar_SetQuick(&vr_gunsfx_dryfire, "0");
        vol = 1.f, pitch = 1.f;
        check(VR_SndShot(cl.viewentity, SND_CHAN_HAND, click, org, &vol, 1.f, &pitch) == 0 && state.lastDry.silenced,
            "dry fire 0: no click");
        Cvar_SetQuick(&vr_gunsfx_dryfire, "1");
    }

    // 6. Every weapon's punch: a spike at its peak, a falling tail from half of it to nothing at its recoil's way back.
    int bad = 0, guns = 0;
    for(int slot = 0; slot < weapons::numSlots; slot++)
    {
        if(slot == weapons::fistSlot())
        {
            continue;
        }
        const float peak = peakOf(slot), tail = tailOf(slot);
        if(peak <= 0.f)
        {
            continue; // (no recoil, or its punch at 0)
        }
        guns++;
        bool ok = punchAt(slot, 0.f).amplitude == peak && punchAt(slot, 0.f).hz == spikeHz;
        float prev = peak;
        for(float tt = spikeTime; tt < spikeTime + tail; tt += 1.f / 72.f)
        {
            const float a = punchAt(slot, tt).amplitude;
            ok = ok && a <= prev && a <= peak * tailShare + 1e-6f;
            prev = a;
        }
        ok = ok && punchAt(slot, spikeTime + tail + 1e-4f).amplitude == 0.f && punchAt(slot, spikeTime + 0.5f * tail).amplitude > 0.f;
        if(!ok)
        {
            bad++;
            Con_Printf("%s: slot %d's punch: peak %.2f tail %.3f s\n", N, slot, peak, tail);
        }
    }
    check(guns > 10 && bad == 0, "every weapon's punch a spike then a falling tail (%d of %d weapons wrong)", bad, guns);
    const int held = weapons::heldSlot(HAND_MAIN);
    check(held >= 0 && held != weapons::fistSlot(), "a gun held in the main hand (slot %d)", held);
    if(held >= 0 && held != weapons::fistSlot())
    {
        const float tail0 = tailOf(held), peak0 = peakOf(held);
        {
            const KeepCvar rec{weapons::cvar(held, Key::RecoilRecovery)};
            Cvar_SetValueQuick(weapons::cvar(held, Key::RecoilRecovery), 2.f * za::max(weapons::value(held, Key::RecoilRecovery), 0.05f));
            check(za::abs(tailOf(held) - za::min(2.f * tail0, 1.5f)) < 1e-4f, "Recoil Recovery 2x: the tail twice as long (%.3f s, %.3f s)",
                tailOf(held), tail0);
        }
        {
            const KeepCvar punch{weapons::cvar(held, Key::HapticPunch)};
            Cvar_SetValueQuick(weapons::cvar(held, Key::HapticPunch), 0.f);
            check(punchAt(held, 0.f).amplitude == 0.f, "Haptic Punch 0: no punch");
        }
        state.punches[HAND_MAIN] = Punch{};
        const int pulses = state.lastPulse[HAND_MAIN].count;
        fired(HAND_MAIN);
        check(state.lastPulse[HAND_MAIN].count == pulses + 1 && state.lastPulse[HAND_MAIN].amplitude == peak0 &&
                punchOwns(HAND_MAIN),
            "a shot sends its spike at once (%.2f) and holds back the flat buzz", state.lastPulse[HAND_MAIN].amplitude);
        state.punches[HAND_MAIN] = Punch{};
        Cvar_SetQuick(&vr_gunsfx_punch, "0");
        fired(HAND_MAIN);
        check(state.punches[HAND_MAIN].slot < 0 && !punchOwns(HAND_MAIN), "punch 0: the flat buzz as before");
        Cvar_SetQuick(&vr_gunsfx_punch, "1");
    }

    // 7. An action's sound from the action; the shot from the muzzle; none with the setting off.
    const float hinge = actionPlace("vr/reload_ssg_open.wav");
    check(hinge > 0.f && hinge < 1.f && actionPlace("weapons/guncock.wav") < 0.f, "the hinge's sound along the gun (%.2f)", hinge);
    Cvar_SetQuick(&vr_gunsfx_action, "0");
    check(actionPlace("vr/reload_ssg_open.wav") < 0.f, "actions off: from the muzzle as before");

    // 8. All off: a shot as it was.
    Cvar_SetQuick(&vr_gunsfx, "0");
    {
        float vol = 1.f, pitch = 1.f;
        const int before = state.lastShot.count;
        (void)VR_SndShot(cl.viewentity, 1, sg, org, &vol, 1.f, &pitch);
        check(state.lastShot.count == before && vol == 1.f && pitch == 1.f, "vr_gunsfx 0: the shot alone, as it was");
    }
    clear();
    t.verdict(N);
}

} // namespace

void registerCommands()
{
    Cmd_AddCommand("vr_gunsfx_test", test_f);
}

} // namespace qvr::gunsfx
