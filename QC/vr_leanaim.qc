// QVR: monsters see and aim at a leaning player's visible parts, with a cover advantage (vr_monster_lean; ROUND26.md,
// "leanaim"). Quake's monsters see a player by one point, his origin + view_ofs, and aim at his origin: leaning out
// from cover (his head, shoulders and hands past the edge, his origin behind it) he was never seen nor aimed at. Now:
// - Sight (visible(), ai.qc): when that point is hidden, a few points of his drawn body (the engine's playerbodypoint,
//   the shape shots meet: head, neck, torso, shoulders, hands) are tried; any showing, he is seen, but only after
//   vr_monster_lean_react seconds of a new exposure (a lean after over VR_LEAN_GAP s of none).
// - Clear shot (the CheckAttack family's "trace_ent != targ"): VR_Lean_Clear, seen that way this think, counts.
// - Aim (every ranged monster's shot through VR_AimCone_Bend; armed grunts' own aim, VR_AG_AimDir): with his eyes' point
//   still hidden, at the biggest part showing (the torso, then the head or neck, the shoulders, the hands), with extra
//   spread vr_monster_lean_spread times how much of him is hidden; lobbed shots only turned level (their arc kept).
// The advantage: the delay, the smaller target and the spread (vr_sphit_test 2's DPS lines: open > lean > hidden 0).

float VR_LEAN_POINTS = 7;   // playerbodypoint's points: 0 head, 1 neck, 2 torso, 3 4 shoulders, 5 6 hands
float VR_LEAN_GAP = 1;      // seconds unseen by a lean after which a lean is a new exposure (its reaction delay again)
float VR_LEAN_RANGE = 2000; // farther: not tried (beyond every ranged attack)

.float vr_lean_seen;  // when this monster last saw its enemy only by his body's points (time)
.float vr_lean_since; // ... since when, this exposure
.float vr_lean_expo;  // ... how much of him showed (0..1, by the points' weights)

vector vr_lean_best;   // VR_Lean_Scan's biggest part showing (world)
float vr_lean_reaims;  // shots turned onto a part showing (the test)

// A point's weight: about its part's size seen from ahead (the torso 6, head and neck 3, shoulders 2, hands 1).
float(float i) VR_Lean_Weight =
{
    if(i == 2)
    {
        return 6;
    }
    if(i <= 1)
    {
        return 3;
    }
    return i <= 4 ? 2 : 1;
};

// How much of player t's drawn body shows from `eye` (0..1; 0: none, or no shape); vr_lean_best its biggest part showing.
// Up to VR_LEAN_POINTS traces (the world and doors, lifts, walls block; monsters do not).
float(entity m, entity t, vector eye) VR_Lean_Scan =
{
    float seen = 0;
    float total = 0;
    float bestw = 0;
    for(float i = 0; i < VR_LEAN_POINTS; i++)
    {
        vector p = playerbodypoint(t, i);
        if(p == '0 0 0')
        {
            return 0;
        }
        float w = VR_Lean_Weight(i);
        total = total + w;
        traceline(eye, p, TRUE, m);
        if(trace_fraction == 1 && !trace_startsolid)
        {
            seen = seen + w;
            if(w > bestw)
            {
                bestw = w;
                vr_lean_best = p;
            }
        }
    }
    return total > 0 ? seen / total : 0;
};

// visible()'s second look (self a monster, its eyes' line to `client`'s point blocked): a player's body points. TRUE:
// seen (after the reaction delay of a new exposure).
float(entity client, vector spot1) VR_Lean_Sight =
{
    if(!cvar_hget(cvarh_vr_monster_lean) || !(self.flags & FL_MONSTER) || client.classname != "player" ||
       client.health <= 0 || vlen(client.origin - self.origin) > VR_LEAN_RANGE)
    {
        return FALSE;
    }
    float expo = VR_Lean_Scan(self, client, spot1);
    if(expo <= 0)
    {
        return FALSE;
    }
    if(time - self.vr_lean_seen > VR_LEAN_GAP)
    {
        self.vr_lean_since = time; // (a new exposure)
    }
    self.vr_lean_seen = time;
    self.vr_lean_expo = expo;
    return time - self.vr_lean_since >= cvar_hget(cvarh_vr_monster_lean_react);
};

// A clear shot at targ by a lean (the CheckAttack family, after visible() this think): seen by its body's points now.
float(entity targ) VR_Lean_Clear =
{
    return targ == self.enemy && self.vr_lean_seen == time && time - self.vr_lean_since >= cvar_hget(cvarh_vr_monster_lean_react);
};

// Monster m's extra spread at its enemy now (Quake's spread units; 0: not a lean, he shows from its eyes).
float(entity m) VR_Lean_Spread =
{
    if(!cvar_hget(cvarh_vr_monster_lean) || time - m.vr_lean_seen > VR_LEAN_GAP)
    {
        return 0;
    }
    return max(0, cvar_hget(cvarh_vr_monster_lean_spread)) * (1 - m.vr_lean_expo);
};

// A shot's way (a direction or a velocity) from monster m at its enemy, leaning from cover (seen by his body's points
// lately, his eyes' point still hidden): at the biggest part showing now, with the extra spread; `lobbed`: only turned
// level (its length and arc kept). Else, or nothing of him showing: as given (into the cover). VR_AimCone_Bend calls it.
vector(entity m, vector dir, float lobbed) VR_Lean_Aim =
{
    entity t = m.enemy;
    if(!cvar_hget(cvarh_vr_monster_lean) || !t || t.classname != "player" || time - m.vr_lean_seen > VR_LEAN_GAP ||
       m.vr_ai_gate > 0)
    {
        return dir;
    }
    vector eye = m.origin + m.view_ofs;
    traceline(eye, t.origin + t.view_ofs, TRUE, m);
    if(trace_fraction == 1)
    {
        return dir; // (out in the open now: as aimed)
    }
    float expo = VR_Lean_Scan(m, t, eye);
    if(expo <= 0)
    {
        return dir;
    }
    m.vr_lean_expo = expo;
    vr_lean_reaims = vr_lean_reaims + 1;
    vector to = normalize(vr_lean_best - eye);
    float s = VR_Lean_Spread(m);
    if(s > 0)
    {
        vector right;
        right_x = to_y;
        right_y = -to_x;
        right_z = 0;
        right = vlen(right) > 0.0001 ? normalize(right) : '0 1 0';
        vector up; // (right x to)
        up_x = right_y * to_z;
        up_y = -right_x * to_z;
        up_z = right_x * to_y - right_y * to_x;
        to = normalize(to + right * crandom() * s + up * crandom() * s);
    }
    float len = vlen(dir);
    if(lobbed)
    {
        float flat = sqrt(dir_x * dir_x + dir_y * dir_y);
        float tf = sqrt(to_x * to_x + to_y * to_y);
        if(tf > 0.0001)
        {
            dir_x = to_x / tf * flat;
            dir_y = to_y / tf * flat;
        }
        return dir;
    }
    return to * len;
};
