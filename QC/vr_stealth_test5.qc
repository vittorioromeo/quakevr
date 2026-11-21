#ifdef QVR_TESTS
// vr_stealth_test5.qc -- `vr_stealth_test 130` on vralert (Misc/quakevr/make_vralert_map.py): the spread of an alarm
// from one monster to the others (docs/vr-port/STEALTH.md, "Hostile and Alert propagation"; the notes of 2026-11-21).
// The map's six grunts round V: N near V in the open, S farther facing it, A in ambush facing it, W behind a wall
// within earshot, F in a sealed room far off. The player is put in a sealed booth (out of everyone's sight, dark).
// Three scenes, each from all six Idle at their marks: V hurt by him, V alerted by him, V killed by him; each grunt's
// state 1.5 s on against what it should be. Prints `stealthtest: alarm_<scene>_<grunt> PASS|FAIL`, then
// `stealthtest: alarm done, N failed`. stealth_tests.sh alarm runs it.

entity stt5_v;
entity stt5_n;
entity stt5_s;
entity stt5_a;
entity stt5_w;
entity stt5_f;
float stt5_scene; // 0 hurt, 1 alerted, 2 killed

vector stt5_booth = '-1312 224 24';

// Each grunt's mark and yaw (as make_vralert_map.py places them): 0 V, 1 N, 2 S, 3 A, 4 W, 5 F.
vector(float i) VR_STT5_Home =
{
    if(i == 0) { return '0 128 24'; }
    if(i == 1) { return '-176 128 24'; }
    if(i == 2) { return '-640 128 24'; }
    if(i == 3) { return '0 320 24'; }
    if(i == 4) { return '256 -96 24'; }
    return '1152 232 24';
};

float(float i) VR_STT5_Yaw =
{
    if(i == 1) { return 180; }
    if(i == 3) { return 270; }
    if(i == 4) { return 90; }
    if(i == 5) { return 180; }
    return 0;
};

entity(float i) VR_STT5_Get =
{
    if(i == 0) { return stt5_v; }
    if(i == 1) { return stt5_n; }
    if(i == 2) { return stt5_s; }
    if(i == 3) { return stt5_a; }
    if(i == 4) { return stt5_w; }
    return stt5_f;
};

// The map's grunt nearest a mark (within 64), world if none.
entity(vector at) VR_STT5_Find =
{
    entity best = world;
    float bd = 64;
    for(entity e = findflags(world, flags, FL_MONSTER); e; e = findflags(e, flags, FL_MONSTER))
    {
        float d = vlen(e.origin - at);
        if(e.health > 0 && d < bd)
        {
            bd = d;
            best = e;
        }
    }
    return best;
};

// All six back on their marks, Idle; the player in the booth.
void() VR_STT5_Reset =
{
    for(float i = 0; i < 6; i++)
    {
        entity m = VR_STT5_Get(i);
        setorigin(m, VR_STT5_Home(i));
        VR_STT2_Calm(m, VR_STT5_Yaw(i));
        m.stl_heard = 0;
        m.stl_alert_time = -100;
    }
    stt2_p.velocity = '0 0 0';
    setorigin(stt2_p, stt5_booth);
    sight_entity = world;
    sight_entity_time = -100;
};

// 0 Idle, 1 Alert, 2 Hostile at him.
float(entity m) VR_STT5_State =
{
    if(m.enemy == stt2_p) { return 2; }
    if(m.enemy == world && m.stl_state == STL_ALERT) { return 1; }
    if(m.enemy == world) { return 0; }
    return 3; // (after something else)
};

// xWant: 0 Idle, 1 Alert, 2 Hostile, 4 Alert or Hostile, 5 the ambusher at its post (Idle or Alert), -1 only shown.
void(string xScene, string xName, entity m, float xWant) VR_STT5_Check =
{
    float s = VR_STT5_State(m);
    vector cry = VR_STT5_Home(0) + '0 0 25';
    string info = sprintf("state %g (0 idle, 1 alert, 2 hostile), heard %g kind %g, %.0f units from V (V's cry's way x%.2f)", s,
        m.stl_heard > 0, m.stl_heard_kind, vlen(m.origin - VR_STT5_Home(0)), VR_Stealth_NoisePath(cry, m, stt5_v));
    if(xWant < 0)
    {
        bprint(sprintf("stealthtest:   alarm_%s_%s (info) %s\n", xScene, xName, info));
        return;
    }
    float pass = (xWant == 4) ? (s == 1 || s == 2) : (s == xWant);
    if(xWant == 5)
    {
        // (the ambusher: never Hostile, never off its post; Alert (turned to look at a body it sees) allowed)
        float off = vlen(m.origin - VR_STT5_Home(3));
        pass = ((s == 0 || s == 1) && (off < 24));
        info = sprintf("%s, %.0f units off its post", info, off);
    }
    VR_STT2_Report(sprintf("alarm_%s_%s", xScene, xName), pass, info);
};

void() VR_STT5_Frame =
{
    entity p = stt2_p;
    float t = time - stt2_t0;
    if(stt2_stage == 0)
    {
        stt5_v = VR_STT5_Find(VR_STT5_Home(0));
        stt5_n = VR_STT5_Find(VR_STT5_Home(1));
        stt5_s = VR_STT5_Find(VR_STT5_Home(2));
        stt5_a = VR_STT5_Find(VR_STT5_Home(3));
        stt5_w = VR_STT5_Find(VR_STT5_Home(4));
        stt5_f = VR_STT5_Find(VR_STT5_Home(5));
        if(!stt5_v || !stt5_n || !stt5_s || !stt5_a || !stt5_w || !stt5_f)
        {
            VR_STT2_Report("alarm", FALSE, "not on vralert (a grunt missing from its mark)");
            bprint("stealthtest: alarm done, 1 failed\n");
            VR_STT2_End();
            return;
        }
        cvar_set("vr_stealth_light_force", "0");
        stt5_scene = 0;
        VR_STT5_Reset();
        VR_STT2_Stage(1);
        return;
    }
    if(stt2_stage == 1 && t >= 0.5)
    {
        // (the event: by him, from his booth)
        if(stt5_scene == 0)
        {
            T_Damage(stt5_v, p, p, 10);
        }
        else if(stt5_scene == 1)
        {
            VR_Stealth_Alert(stt5_v, p.origin, STL_A_GLIMPSE, 0.5, TRUE);
        }
        else
        {
            T_Damage(stt5_v, p, p, stt5_v.health + 100);
        }
        VR_STT2_Stage(2);
        return;
    }
    if(stt2_stage == 2 && t >= 1.5)
    {
        string sc = stt5_scene == 0 ? "hurt" : (stt5_scene == 1 ? "alerted" : "killed");
        if(stt5_scene == 1)
        {
            VR_STT5_Check(sc, "V", stt5_v, 1);
            VR_STT5_Check(sc, "N", stt5_n, 1);
            VR_STT5_Check(sc, "S", stt5_s, -1);
            VR_STT5_Check(sc, "A", stt5_a, 5);
            VR_STT5_Check(sc, "W", stt5_w, -1);
            VR_STT5_Check(sc, "F", stt5_f, 0);
        }
        else
        {
            VR_STT5_Check(sc, "N", stt5_n, 2);
            VR_STT5_Check(sc, "S", stt5_s, 2);
            VR_STT5_Check(sc, "A", stt5_a, 5);
            VR_STT5_Check(sc, "W", stt5_w, 4);
            VR_STT5_Check(sc, "F", stt5_f, 0);
        }
        stt5_scene = stt5_scene + 1;
        if(stt5_scene > 2)
        {
            bprint(sprintf("stealthtest: alarm done, %g failed\n", stt2_fails));
            VR_STT2_End();
            return;
        }
        VR_STT5_Reset();
        VR_STT2_Stage(1);
    }
};
#endif
