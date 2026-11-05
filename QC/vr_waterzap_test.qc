#ifdef QVR_TESTS
// vr_waterzap_test.qc -- lightning into water electrifies its surface (VR_LGWater_Zap, weapons.qc; the arcs: engine
// vr_shock.cpp drawSurface). `map vrfiringrange; vr_waterzap_test 1` (Debug > Tools > Test Effects > Lightning into Water;
// any map with open water does), over the first water on the map: each source fired by its own function. What counts is
// the client having drawn the surface's effect (watershockdrawn), not the server's having sent it. One line a case, then
// the verdict `waterzap: <n> passed, <m> failed` (run.sh -QuitOn "waterzap: .* failed").

float vr_wz_step;     // the next case (0: not running)
string vr_wz_name;    // the case being awaited ("": none)
float vr_wz_drawn;    // watershockdrawn when last looked at
float vr_wz_frames;   // frames it has been awaited, or (no case) frames the effects have been quiet

float VR_WZ_GIVEUP = 60; // frames (0.8 s): a safety net: a surface effect is drawn within a few frames of its bolt
float VR_WZ_QUIET = 3;   // frames with nothing drawn: the last case's effect (0.3 s) has gone out
vector vr_wz_surface; // a point of the water's surface, with room over it (VR_WZ_FindWater)

void(entity p, float xWid) VR_WZ_Give =
{
    VRSetEntWeapon(p, cVR_MainHand, xWid);
    VRSetEntWeaponFlags(p, cVR_MainHand, 0);
    VRSetEntWeaponInst(p, cVR_MainHand, WeaponInst_New(0));
    VRSetEntWeaponClip(p, cVR_MainHand, WeaponIdToBaseClipSize(xWid));
    W_SetCurrentAmmoFor(cVR_MainHand);
    p.ammo_cells = 200;
    p.health = 100;
};

// Whether a point's column has open water at `xAt`: liquid under it, open over it.
float(vector xAt) VR_WZ_WaterAt =
{
    return VR_LGWater_IsLiquid(pointcontents(xAt - '0 0 8')) && pointcontents(xAt + '0 0 8') == CONTENT_EMPTY;
};

// The first water, over the map's columns, a good hand's reach across and with room over it for the sources.
float() VR_WZ_FindWater =
{
    for(float x = -4032; x <= 4032; x = x + 64)
    {
        for(float y = -4032; y <= 4032; y = y + 64)
        {
            vector top = '0 0 4032';
            top_x = x;
            top_y = y;
            vector at = liquidentry(top, top - '0 0 8064');
            if(at == top || !VR_WZ_WaterAt(at) || !VR_WZ_WaterAt(at + '24 0 0') || !VR_WZ_WaterAt(at - '24 0 0')
                || !VR_WZ_WaterAt(at + '0 24 0') || !VR_WZ_WaterAt(at - '0 24 0'))
            {
                continue;
            }
            traceline(at, at + '0 0 170', TRUE, world);
            if(trace_fraction == 1)
            {
                vr_wz_surface = at;
                return TRUE;
            }
        }
    }
    return FALSE;
};

// A blow of Mjolnir's at `xAt`, as HIP_FireMjolnir strikes: the trace it struck by, then its lightning.
void(entity p, vector xAt) VR_WZ_Mjolnir =
{
    VR_WZ_Give(p, WID_MJOLNIR);
    trace_endpos = xAt;
    trace_ent = world;
    HIP_FireMjolnirLightning(cVR_MainHand, xAt);
};

// A shambler's bolt (the real cast) from `xFrom`, at the player at `xTarget`.
void(entity p, vector xFrom, vector xTarget) VR_WZ_Shambler =
{
    setorigin(p, xTarget);
    entity m = spawn();
    m.classname = "monster_shambler";
    m.flags = FL_MONSTER;
    m.solid = SOLID_NOT;
    setorigin(m, xFrom);
    m.enemy = p;
    m.think = SUB_Remove;
    m.nextthink = time + 0.5;
    self = m;
    CastLightning();
    self = p;
};

void() VR_WaterZap_TestFrame =
{
    float ask = cvar_hget(cvarh_vr_waterzap_test);
    if(ask)
    {
        cvar_set("vr_waterzap_test", "0");
        vr_wz_step = 1;
        vr_wz_frames = 0;
        VR_Test_BeginFlags("waterzap", VR_TEST_LOUD);
        vr_wz_name = "";
        vr_wz_drawn = watershockdrawn();
        if(!VR_WZ_FindWater())
        {
            vr_wz_step = 0;
            VR_Test_Check(FALSE, "open water on this map");
            VR_Test_Verdict();
            return;
        }
        bprint(sprintf("waterzap: the water's surface at %v\n", vr_wz_surface));
    }
    if(!vr_wz_step)
    {
        return;
    }

    float drawn = watershockdrawn();
    float more = drawn > vr_wz_drawn;
    vr_wz_drawn = drawn;
    vr_wz_frames = more ? 0 : vr_wz_frames + 1;

    if(vr_wz_name != "")
    {
        if(!more && vr_wz_frames < VR_WZ_GIVEUP)
        {
            return;
        }
        VR_Test_Check(more, sprintf("%s: surface effect %s", vr_wz_name, more ? "drawn" : "never drawn"));
        vr_wz_name = "";
        vr_wz_frames = 0;
        return;
    }
    if(vr_wz_frames < VR_WZ_QUIET)
    {
        return; // (and the hand's aim reaching it, before the first)
    }

    entity p = VR_Player();
    if(p == world)
    {
        return;
    }
    entity oself = self;
    self = p;
    p.flags = p.flags | FL_GODMODE;
    p.movetype = MOVETYPE_NOCLIP;
    p.velocity = '0 0 0';
    vector above = vr_wz_surface + '0 0 94';
    setorigin(p, above);

    vr_wz_frames = 0;
    float step = vr_wz_step;
    vr_wz_step = step + 1;
    if(step == 1)
    {
        // (A frame for the player to be over the pool, its hands with it.)
        vr_wz_frames = VR_WZ_QUIET - 2;
    }
    else if(step == 2)
    {
        vr_wz_name = "thunderbolt into the water";
        VR_WZ_Give(p, WID_LIGHTNING);
        p.shotrot = '89 0 0'; // straight down
        W_FireLightning(cVR_MainHand);
    }
    else if(step == 3)
    {
        vr_wz_name = "Mjolnir striking in the water";
        VR_WZ_Mjolnir(p, vr_wz_surface - '0 0 6');
    }
    else if(step == 4)
    {
        vr_wz_name = "Mjolnir striking just over the water";
        VR_WZ_Mjolnir(p, vr_wz_surface + '0 0 12');
    }
    else if(step == 5)
    {
        vr_wz_name = "Shambler's bolt into the water";
        VR_WZ_Shambler(p, above + '0 0 30', vr_wz_surface - '0 0 64');
    }
    else
    {
        vr_wz_step = 0;
        VR_Test_Verdict();
    }
    self = oself;
};
#endif
