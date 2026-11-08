// vr_playerplates.qc -- the armour you wear, drawn as plates on your body (vr_player_plates; ROUND24.md, "The player's armour
// plates"; the plates: vr_agarmor.cpp's generator on progs/vrbody, the view: vr_view_body.cpp setupPlates).
//
// A look alone: .armorvalue, .armortype and T_Damage's rules are as they ever were; this file only reads them, each
// frame, whatever changed them (armor_touch, a shard, Arcane Dimensions' armour, a hit). The pieces shown go by the fraction
// left of the worn tier's most (100, 150, 200: a damaged red and a damaged green lose them at the same pace), the
// tier sets the colour. A piece lost: limbs first, the one nearest the hit if a hit took it, the cuirass only when the
// armour is gone; it falls off as the humanoids' plates do (vr_agarmor.qc's debris and crack) with a pulse in the hands. A
// piece gained: the view morphs it in. Kept in .vr_pp_mask, so a save keeps them; parm75 carries them to the next level.

float VR_PP_NUM = 10;  // all pieces (VR_AR_PIECES)
float vr_pp_pops;      // the tests: pieces shed so far
float vr_pp_last_pop;  // ... the last one
float vr_pp_pulses;    // ... the hands' pulses
float vr_pp_cracks;    // ... the cracks

float(entity p) VR_PP_Tier =
{
    if(p.armorvalue <= 0)
    {
        return 0;
    }
    return p.armortype >= 0.75 ? 3 : p.armortype >= 0.55 ? 2 : 1;
};

// How many pieces `value` left of tier `tier`'s most shows: all at its most, one at the very least, none without armour.
float(float tier, float value) VR_PP_Count =
{
    if(tier <= 0 || value <= 0)
    {
        return 0;
    }
    return 1 + floor(min(1, value / (50 + 50 * tier)) * (VR_PP_NUM - 1));
};

// The pieces in the order they are put back (the cuirass first, the last one on the first off, with no hit to choose by).
float(float k) VR_PP_Order =
{
    if(k == 0) return 0; // the cuirass
    if(k == 1) return 2; // the pauldrons
    if(k == 2) return 3;
    if(k == 3) return 1; // the helmet
    if(k == 4) return 6; // the thighs
    if(k == 5) return 7;
    if(k == 6) return 4; // the bracers
    if(k == 7) return 5;
    if(k == 8) return 8; // the shins
    return 9;
};

float(float mask) VR_PP_CountOf =
{
    float n = 0;
    for(float i = 0; i < VR_PP_NUM; i++)
    {
        n = n + ((mask & VR_AgArmor_Bit(i)) ? 1 : 0);
    }
    return n;
};

// Where piece `i` is: as your body draws it, else (no body drawn, another's) where it would be on someone standing there.
vector(entity p, float i) VR_PP_Where =
{
    vector at = limbpiece(p, i, 10);
    if(at != '0 0 0')
    {
        return at;
    }
    makevectors([0, p.angles_y, 0]);
    vector o = i == 0 ? '0 0 12' : i == 1 ? '0 0 24' : i == 2 ? '0 10 16' : i == 3 ? '0 -10 16' : i == 4 ? '8 14 4' :
        i == 5 ? '8 -14 4' : i == 6 ? '0 5 -8' : i == 7 ? '0 -5 -8' : i == 8 ? '0 5 -18' : '0 -5 -18';
    return p.origin + v_forward * o_x - v_right * o_y + v_up * o_z;
};

// The piece to shed: the limb (or helmet) nearest the last hit if it just came, else the last one on; the cuirass last.
float(entity p) VR_PP_NextOff =
{
    float best = -1;
    float bestDist = 999999;
    float hit = time - p.vr_pp_hit_time < 0.25 && p.vr_pp_hit_time > 0;
    for(float k = VR_PP_NUM - 1; k >= 1; k--)
    {
        float i = VR_PP_Order(k);
        if(p.vr_pp_mask & VR_AgArmor_Bit(i))
        {
            if(!hit)
            {
                return i;
            }
            float d = vlen(VR_PP_Where(p, i) - p.vr_pp_hit);
            if(d < bestDist)
            {
                bestDist = d;
                best = i;
            }
        }
    }
    return best >= 0 ? best : 0;
};

void(float hand) VR_PP_Pulse =
{
    haptic(hand, 0, 0.12, 110, 0.6);
    vr_pp_pulses = vr_pp_pulses + 1;
};

// Piece `i` falls off: its crack (`crack`: once a frame, however many go), a pulse, its debris (the helmet's never: it
// would be round your eyes; solo and coop only, as the humanoids' plates are).
void(entity p, float i, float tier, float crack) VR_PP_Shed =
{
    p.vr_pp_mask = p.vr_pp_mask - (p.vr_pp_mask & VR_AgArmor_Bit(i));
    vr_pp_pops = vr_pp_pops + 1;
    vr_pp_last_pop = i;
    if(crack)
    {
        precache_sound(VR_AR_BREAK_SOUND);
        sound(p, CHAN_AUTO, VR_AR_BREAK_SOUND, max(0, min(4, cvar_hget(cvarh_vr_agrunt_armor_crack_volume))),
            max(0, cvar_hget(cvarh_vr_agrunt_armor_crack_attn)));
        vr_pp_cracks = vr_pp_cracks + 1;
        if(p.ishuman)
        {
            VR_Haptic_Both(VR_PP_Pulse);
        }
    }
    if(i != 1 && !deathmatch)
    {
        vector at = limbpiece(p, i, 10);
        vector out = at - p.origin;
        out_z = 0;
        // (in two halves: vr_agarmor_split)
        VR_AgArmor_DebrisBroken(p, VR_AgArmor_ModelOf("progs/vrbody.mdl", i), tier, i, at, limbpiece(p, i, 11),
            p.velocity + normalize(out) * (60 + 40 * random()) + '0 0 90');
    }
};

// Each frame: the pieces shown follow the armour left.
void(entity p) VR_PP_Frame =
{
    if(!cvar_hget(cvarh_vr_player_plates))
    {
        p.vr_pp_type = 0;
        p.vr_pp_mask = 0;
        return;
    }
    float tier = VR_PP_Tier(p);
    float want = VR_PP_Count(tier, p.armorvalue);
    float have = VR_PP_CountOf(p.vr_pp_mask);
    // (the colour the pieces falling now were: the armour's gone from under them, its type with it)
    float shedTier = p.vr_pp_type > 0 ? p.vr_pp_type : max(tier, 1);
    float crack = TRUE;
    while(have > want)
    {
        VR_PP_Shed(p, VR_PP_NextOff(p), shedTier, crack);
        crack = FALSE;
        have = have - 1;
    }
    for(float k = 0; k < VR_PP_NUM && have < want; k++)
    {
        float b = VR_AgArmor_Bit(VR_PP_Order(k));
        if(!(p.vr_pp_mask & b))
        {
            p.vr_pp_mask = p.vr_pp_mask + b;
            have = have + 1;
        }
    }
    // (as long as any armour shows, the cuirass is on: a mask from elsewhere, an old save's)
    if(want > 0 && !(p.vr_pp_mask & 1))
    {
        float last = VR_PP_NextOff(p);
        p.vr_pp_mask = p.vr_pp_mask - (p.vr_pp_mask & VR_AgArmor_Bit(last)) + 1;
    }
    p.vr_pp_type = tier;
};

// Worldspawn: the debris models made ahead (a plate's first break would hitch: vr_agarmor.qc VR_AgArmor_PrecacheOf).
void() VR_PP_Precache =
{
    if(cvar_hget(cvarh_vr_player_plates))
    {
        VR_AgArmor_PrecacheOf("progs/vrbody.mdl");
    }
};
