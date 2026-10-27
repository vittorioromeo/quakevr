#ifdef QVR_TESTS
// vr_armedgrunt_posecat_test.qc -- test 83: the gun pose categories (vr_agpose.hpp Class; vr_agholds.inc QVR_AGGUN;
// ROUND22.md, "Gun pose categories"). For each armed kind and each weapon of the Weapon Holds' table: the class its
// poses take (VR_AGK_Class on a stand-in holder, its weapon by id; the AD upgrades by their code through the Weapon
// Holds' answer) against the table below; a Pistol Aim key changed (its right place 5 forward): only the pistols'
// keys laid on move, by exactly that (vr_agrunt_pose_key: set, then reset); a Pose Category set (the grunt's nail
// pistol Rifle): that kind's alone changes,
// back at Auto. A few frames (the console commands' turns); ends on "agtest: test 83 over".

// The category weapon `code` should take by Auto (melee: its melee class).
float(float code) VR_AGPC_Want =
{
    if(code == WID_AXE || code == WID_SWORD || code == WID_CROWBAR) { return VR_AGK_MELEE; }
    if(code == WID_MJOLNIR || code == WID_SUPERAXE) { return VR_AGK_MELEE2; }
    if(code == WID_CHAINSAW) { return VR_AGK_SAW; }
    if(code == WID_NAILPISTOL || code == WID_REVOLVER || code == WID_FLAREGUN) { return VR_AGK_PISTOL; }
    if(code == WID_ROCKET_LAUNCHER || code == WID_ADROCKET) { return VR_AGK_PISTOL + 1; } // a shoulder launcher
    if(code == WID_ADPYRO || code == WID_ADFUMIGATOR) { return VR_AGK_PISTOL + 2; }       // a sprayer
    if(code == WID_LASER_CANNON) { return VR_AGK_PISTOL + 3; }
    if(code == VR_AG_CODE_WIDOWMAKER) { return VR_AGK_WIDOWMAKER; }
    return VR_AGK_GUN; // (the rifles)
};

// The weapons the test asks (the Weapon Holds' rows: their ids, the AD upgrades by code; i 0 .. VR_AGPC_N - 1).
float VR_AGPC_N = 30;
float(float i) VR_AGPC_Code =
{
    if(i < 2) { return i == 0 ? WID_AXE : WID_MJOLNIR; }
    if(i <= 15) { return WID_SHOTGUN + (i - 2); }        // 4..17: shotgun .. crowbar
    if(i == 16) { return WID_SUPERAXE; }
    if(i <= 20) { return WID_ADROCKET + (i - 17); }      // 20..23
    if(i <= 27) { return WID_ADCENTURION + (i - 21); }   // 25..31
    if(i == 28) { return VR_AG_CODE_WIDOWMAKER; }
    if(i == 29) { return VR_AG_CODE_PLASMAGUN; }
    return -1;
};

string(float code) VR_AGPC_Name =
{
    if(code == VR_AG_CODE_WIDOWMAKER) { return "Widowmaker"; }
    if(code == VR_AG_CODE_PLASMAGUN) { return "Plasma Gun"; }
    return WeaponNameOf(code, 0);
};

entity vr_agpc_g;
entity vr_agpc_p;

// The class kind `kind` takes with weapon `code` (a stand-in holder and weapon; an upgrade's through the holds' answer).
float(float kind, float code) VR_AGPC_Class =
{
    vr_agpc_g.vr_ag_kind = kind;
    if(code > 100)
    {
        vector h = weapondrawnpose(world, kind * 1000 + code, 24);
        return h_z ? h_z : VR_AGK_GUN;
    }
    vr_agpc_p.weapon = code;
    return VR_AGK_Class(vr_agpc_g, vr_agpc_p);
};

// The right place the keys lay on kind `kind` with weapon `code`, aimed (VR_AGK_Keys: the Aim stance alone).
vector(float kind, float code) VR_AGPC_Rp =
{
    vr_agpc_g.vr_ag_kind = kind;
    vr_agpc_g.vr_ag_aimw = 1;
    vr_agpc_p.weapon = code > 100 ? WID_SUPER_SHOTGUN : code; // (an upgrade: its class asked by code below)
    float cls = VR_AGPC_Class(kind, code);
    if(code > 100)
    {
        return weapondrawnpose(world, kind * VR_AGK_KIND + cls * 1000 + 10, 11); // (the Aim key's own)
    }
    VR_AGK_Keys(vr_agpc_g, vr_agpc_p);
    return vr_agk_rp;
};

// "vr_agrunt_pose_key <code> <its 21 numbers>" for key `code` as it is now, its right place moved by `d`.
string(float code, vector d) VR_AGPC_KeyCmd =
{
    string s = sprintf("vr_agrunt_pose_key %g", code);
    for(float part = 11; part <= 17; part++)
    {
        vector v = weapondrawnpose(world, code, part);
        if(part == 11) { v = v + d; }
        s = strcat(s, sprintf(" %g %g %g", v_x, v_y, v_z));
    }
    return strcat(s, "\n");
};

float vr_agpc_step;
float vr_agpc_wait;
vector vr_agpc_rp0[64]; // (kind * 32 + i: the right place the keys laid on before)
vector vr_agpc_want;    // the Pistol Aim key's right place once set

// Kind `kind`'s Pistol Aim code.
float(float kind) VR_AGPC_PistolAim = { return kind * VR_AGK_KIND + VR_AGK_PISTOL * 1000 + 10; };

void() VR_AGPC_Frame =
{
    float k;
    float i;
    float code;
    if(vr_agpc_step == 0)
    {
        vr_agpc_g = spawn();
        vr_agpc_p = spawn();
        vr_agpc_p.owner = vr_agpc_g;
        // 1. the table
        float n = 0;
        for(k = 0; k < VR_AM_COUNT; k++)
        {
            for(i = 0; i < VR_AGPC_N; i++)
            {
                code = VR_AGPC_Code(i);
                float c = VR_AGPC_Class(k, code);
                VR_AGT_Check(sprintf("agcat %s %s (%g): class %g, want %g", k ? "enforcer" : "grunt", VR_AGPC_Name(code),
                    code, c, VR_AGPC_Want(code)), c == VR_AGPC_Want(code));
                n++;
                vr_agpc_rp0[k * 32 + i] = VR_AGPC_Rp(k, code);
            }
        }
        VR_AGT_Print(sprintf("agcat: %g weapons x kinds", n));
        // 2. a Pistol Aim key 5 forward (the console's turn: set next frame)
        vr_agpc_want = weapondrawnpose(world, VR_AGPC_PistolAim(0), 11) + '5 0 0';
        for(k = 0; k < VR_AM_COUNT; k++)
        {
            localcmd(VR_AGPC_KeyCmd(VR_AGPC_PistolAim(k), '5 0 0'));
        }
        vr_agpc_wait = 0;
        vr_agpc_step = 1;
        return;
    }
    if(vr_agpc_step == 1)
    {
        if(vlen(weapondrawnpose(world, VR_AGPC_PistolAim(0), 11) - vr_agpc_want) > 0.01)
        {
            if(++vr_agpc_wait > 30)
            {
                VR_AGT_Check("agcat key: the Pistol Aim key never set (timed out)", FALSE);
                vr_agpc_step = 3;
            }
            return;
        }
        float moved = 0;
        float wrong = 0;
        for(k = 0; k < VR_AM_COUNT; k++)
        {
            for(i = 0; i < VR_AGPC_N; i++)
            {
                code = VR_AGPC_Code(i);
                vector d = VR_AGPC_Rp(k, code) - vr_agpc_rp0[k * 32 + i];
                float pistol = VR_AGPC_Want(code) == VR_AGK_PISTOL;
                float ok = pistol ? vlen(d - '5 0 0') < 0.01 : vlen(d) < 0.001;
                moved = moved + (vlen(d) > 0.001);
                if(!ok)
                {
                    wrong++;
                    VR_AGT_Print(sprintf("agcat key: %s %s moved %v (pistol %g)", k ? "enforcer" : "grunt",
                        VR_AGPC_Name(code), d, pistol));
                }
            }
        }
        VR_AGT_Check(sprintf("agcat key: the Pistol Aim key 5 forward: %g holders moved (want %g, the pistols), %g wrong",
            moved, 3 * VR_AM_COUNT, wrong), moved == 3 * VR_AM_COUNT && !wrong);
        for(k = 0; k < VR_AM_COUNT; k++)
        {
            localcmd(sprintf("vr_agrunt_pose_key %g reset\n", VR_AGPC_PistolAim(k)));
        }
        vr_agpc_wait = 0;
        vr_agpc_step = 2;
        return;
    }
    if(vr_agpc_step == 2)
    {
        vector now = VR_AGPC_Rp(0, WID_NAILPISTOL); // (the nail pistol: i 25)
        if(vlen(now - vr_agpc_rp0[25]) > 0.001 && ++vr_agpc_wait <= 30)
        {
            return;
        }
        VR_AGT_Check(sprintf("agcat key: reset, the grunt's nail pistol %v (was %v)", now, vr_agpc_rp0[25]),
            vlen(now - vr_agpc_rp0[25]) < 0.001);
        vr_agpc_step = 3;
    }
    // 3. a Pose Category set: that kind's alone
    cvar_set("vr_agrunt_posecat_grunt_nailpistol", "1");
    float g1 = VR_AGPC_Class(0, WID_NAILPISTOL);
    float e1 = VR_AGPC_Class(1, WID_NAILPISTOL);
    cvar_set("vr_agrunt_posecat_grunt_nailpistol", "0");
    float g0 = VR_AGPC_Class(0, WID_NAILPISTOL);
    cvar_set("vr_agrunt_posecat_enforcer_rocket", "4"); // (a sprayer)
    float e2 = VR_AGPC_Class(1, WID_ROCKET_LAUNCHER);
    float g2 = VR_AGPC_Class(0, WID_ROCKET_LAUNCHER);
    cvar_set("vr_agrunt_posecat_enforcer_rocket", "0");
    VR_AGT_Check(sprintf("agcat override: grunt nail pistol Rifle -> %g (enforcer's %g), Auto -> %g; enforcer rocket "
        "Sprayer -> %g (grunt's %g)", g1, e1, g0, e2, g2), g1 == VR_AGK_GUN && e1 == VR_AGK_PISTOL && g0 == VR_AGK_PISTOL
        && e2 == VR_AGK_PISTOL + 2 && g2 == VR_AGK_PISTOL + 1);
    remove(vr_agpc_g);
    remove(vr_agpc_p);
    vr_agpc_step = 0;
    cvar_set("vr_agrunt_test", "0");
};
#endif
