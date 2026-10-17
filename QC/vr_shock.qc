// vr_shock.qc -- lightning's lasting shock (vr_shock_death*; the engine's vr_shock.cpp and vr_box3d.cpp, "Shocked
// ragdolls"). A monster the lightning strikes, alive or dead, keeps Quad Damage's arcs crawling over it for
// vr_shock_death_time seconds from the last bolt (bodyshockdeath: every client draws them over the body as drawn, alive,
// dying, its ragdoll or its death frames: the shock goes on through its death), its ragdoll's limbs convulse once it is
// dead, until the arcs end (.vr_shock_until: the engine drives them, vr_shock_seizure), and the killing bolt chars it
// all over (vr_shock_burns marks: woundevent's lightning burns). Every hit, alive or dead, also has its own short arcs
// and a burn where it strikes (weapons.qc VR_LightningHit, vr_wounds.qc).

.float vr_shock_until; // shocked until then (its arcs); the engine reads it: its ragdoll convulses until then (vr_box3d.cpp)
.float vr_shock_crackle; // when its body last crackled (VR_Shock_Crackle)
.float vr_shock_len;   // ... the shock's length from the last bolt (s)
.float vr_shock_bolt;  // ... the last bolt's time (a living one's shock is shorter: its death carries it on, VR_Shock_Killed)
float vr_shock_carried; // living shocks carried on through a death (the tests)
float vr_shock_sent;   // lasting shocks sent (the tests)
float vr_shock_burnt;  // burn marks sent for them (the tests)

// `n` burn marks over `t`, from all round it: from the sides at any height (standing as it starts to die), and some
// from above (lying, a corpse).
void(entity t, float n) VR_Shock_Burns =
{
    if(n <= 0 || !VR_Wound_Takes(t))
    {
        return;
    }
    vector mid = (t.absmin + t.absmax) * 0.5;
    vector size = t.absmax - t.absmin;
    float reach = max(size_x, size_y) * 0.5 + 16;
    float lying = t.vr_corpse == 2;
    float i = 0;
    while(i < n)
    {
        vector at = mid;
        vector dir = '0 0 -1';
        if(lying || random() < 0.25)
        {
            at_x = mid_x + (random() - 0.5) * size_x * 0.8;
            at_y = mid_y + (random() - 0.5) * size_y * 0.8;
            at_z = t.absmax_z + 16;
        }
        else
        {
            float a = random() * 6.2831853;
            dir_x = cos(a);
            dir_y = sin(a);
            dir_z = 0;
            at = mid - dir * reach;
            at_z = t.absmin_z + size_z * (0.15 + 0.75 * random());
        }
        woundevent(t, at, dir, QVR_WOUND_ZAP, 30 + 30 * random(), 0);
        vr_shock_burnt++;
        i++;
    }
};

// After a bolt's T_Damage to `t` (weapons.qc VR_LightningHit), struck at `xAt` going `xDir`; `xAlive`: it was alive
// before. Every bolt into a monster, alive (vr_shock_living) or dead, starts the lasting shock or keeps it fresh (sent
// again each bolt: the arcs and the convulsions end together); a killing bolt's burn marks all over, a dead one's a burn where it strikes.
// A dead body's lasting shock starting (or going on under the bolt): an electric crackle from it (misc/power.wav, the
// sound of Chthon struck and of the lightning gun's shock in water), vr_shock_sound loud; again only once the last has
// played out (1.8 s), so a beam held on it crackles on under its hum and the arcs after it are heard.
void(entity t) VR_Shock_Crackle =
{
    float vol = cvar("vr_shock_sound");
    if(vol <= 0 || time - t.vr_shock_crackle < 1.8)
    {
        return;
    }
    t.vr_shock_crackle = time;
    sound(t, CHAN_AUTO, "misc/power.wav", min(1, vol), ATTN_NORM);
    dprint("shock: "); dprint(t.classname); dprint(" crackles, volume "); dprint(ftos(min(1, vol))); dprint("\n");
};

void(entity t, float xAlive, vector xAt, vector xDir) VR_Shock_Hit =
{
    // (vr_shock_living 0: the lasting shock only on the bodies it kills or strikes dead; a living one has each hit's own
    // short arcs, weapons.qc's bodyshock.)
    if(!(t.flags & FL_MONSTER) || t.vr_gib || !cvar("vr_shock_death") || (t.health > 0 && !cvar("vr_shock_living")))
    {
        return;
    }
    float len = cvar("vr_shock_death_time");
    if(len <= 0)
    {
        return;
    }
    if(xAlive && t.health <= 0)
    {
        VR_Shock_Burns(t, floor(cvar("vr_shock_burns")));
    }
    else if(!xAlive && VR_Wound_Takes(t) && xDir != '0 0 0')
    {
        // (A corpse's damage paints no wound: a burn where the bolt strikes it, as a living one's.)
        xDir = normalize(xDir);
        woundevent(t, xAt - xDir * 8, xDir, QVR_WOUND_ZAP, 30, 0);
        vr_shock_burnt++;
    }
    // (Alive after it: vr_shock_living_time of it; its death carries it on to the whole, VR_Shock_Killed.)
    t.vr_shock_bolt = time;
    if(t.health > 0)
    {
        len = len * max(0, cvar("vr_shock_living_time"));
        if(len <= 0)
        {
            return;
        }
    }
    bodyshockdeath(t, len);
    vr_shock_sent++;
    t.vr_shock_until = time + len;
    t.vr_shock_len = len;
    if(t.health <= 0)
    {
        VR_Shock_Crackle(t);
    }
};

// `t` dies (Killed, whatever kills it): a living one's lasting shock still on (shorter: vr_shock_living_time) goes on
// to the whole vr_shock_death_time from its last bolt, as a body's; its ragdoll convulses as long. (Ended already: none.)
void(entity t) VR_Shock_Killed =
{
    if(t.vr_shock_until <= time || !(t.flags & FL_MONSTER) || !cvar("vr_shock_death"))
    {
        return;
    }
    float left = t.vr_shock_bolt + cvar("vr_shock_death_time") - time;
    if(left <= t.vr_shock_until - time)
    {
        return;
    }
    bodyshockdeath(t, left);
    vr_shock_carried++;
    t.vr_shock_until = time + left;
    t.vr_shock_len = time + left - t.vr_shock_bolt;
    VR_Shock_Crackle(t);
};
