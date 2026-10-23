//
//
//
// ----------------------------------------------------------------------------
// vr_armedgrunt_play_test.qc -- the armed grunts' gameplay tests (vr_armedgrunt_play.qc; ROUND22.md, "Armed grunts:
// gameplay"). `vr_agrunt_test N` (20..39), in an open map (vrtesthall), the player kept alive (VR_AGTest_Frame):
// - 20: ragdolls: a grunt and an armed grunt side by side, killed (corpses), then knocked down alive (active ragdolls):
//   each shot at its torso with pellets: `agplay: rag <who> <state>: traced <classname>, health <h0> -> <h1>, gib <g>`
//   and PASS when the armed grunt takes the shots as the grunt does.
// ----------------------------------------------------------------------------

entity vr_agpt_a;  // the plain grunt
entity vr_agpt_b;  // the armed grunt
float vr_agpt_h0a;
float vr_agpt_h0b;

void(string s) VR_AGPT_Print = { bprint(sprintf("agplay: %s\n", s)); };

// A plain grunt `dist` ahead, `side` units to the right, facing the player.
entity(float dist, float side) VR_AGPT_Grunt =
{
    entity p = vr_agt_player;
    makevectors(p.angles);
    vector fw = v_forward;
    fw_z = 0;
    fw = normalize(fw);
    vector rt = v_right;
    rt_z = 0;
    vector at = p.origin + fw * dist + normalize(rt) * side;
    entity oldself = self;
    self = spawn();
    setorigin(self, at);
    self.angles_y = vectoyaw(p.origin - at);
    self.classname = "monster_army";
    monster_army();
    entity g = self;
    self = oldself;
    return g;
};

// Pellets from the player's eye at e's torso (`n` of them): what the line meets first.
string(entity e, float n) VR_AGPT_Shoot =
{
    entity p = vr_agt_player;
    vector at = isragdoll(e) ? limbplace(e, 1, 3) : e.origin + '0 0 10';
    if(at == '0 0 0') { at = e.origin + '0 0 4'; }
    vector from = p.origin + p.view_ofs;
    vector dir = normalize(at - from);
    traceline(from, at + dir * 32, MOVE_HITGIBS | MOVE_HITMODEL, p);
    string what = trace_ent.classname;
    entity oldself = self;
    self = p;
    VR_SetHitKind(QVR_HITKIND_SHOT, cVR_MainHand, '0 0 0', dir);
    vr_tracer_hand = cVR_MainHand;
    vr_tracer_from = from;
    FireBulletsImpl(n, dir, '0 0 0', from, 2048, FALSE);
    VR_ClearHitKind();
    self = oldself;
    return what;
};

void() VR_AGPT_Ragdoll =
{
    entity p = vr_agt_player;
    if(vr_agt_step == 0)
    {
        VR_AGT_ClearWeapons();
        p.flags = p.flags | FL_NOTARGET;
        vr_agpt_a = VR_AGPT_Grunt(160, -50);
        makevectors(p.angles);
        p.angles_y = p.angles_y - 17;
        vr_agpt_b = VR_AGT_Spawn(WID_SHOTGUN, 168);
        p.angles_y = p.angles_y + 17;
        vr_agt_step = 1;
        vr_agt_until = time + 0.5;
        return;
    }
    if(time < vr_agt_until) { return; }
    entity a = vr_agpt_a;
    entity b = vr_agpt_b;
    if(vr_agt_step == 1 || vr_agt_step == 4) // the alive ones knocked down (4: their corpses next)
    {
        if(vr_agt_step == 1)
        {
            VR_Knockdown_Start(a);
            VR_Knockdown_Start(b);
            VR_AGPT_Print(sprintf("rag knockdown: grunt %g (rag %g), armed %g (rag %g)", a.vr_knockdown, isragdoll(a),
                b.vr_knockdown, isragdoll(b)));
        }
        else
        {
            T_Damage(a, p, p, a.health + 1);
            T_Damage(b, p, p, b.health + 1);
        }
        vr_agt_step = vr_agt_step + 1;
        vr_agt_until = time + (vr_agt_step == 2 ? 0.4 : 2.5);
        return;
    }
    if(vr_agt_step == 2 || vr_agt_step == 5)
    {
        vr_agpt_h0a = vr_agt_step == 2 ? a.health : a.vr_corpse_hp;
        vr_agpt_h0b = vr_agt_step == 2 ? b.health : b.vr_corpse_hp;
        string ta = VR_AGPT_Shoot(a, 2);
        string tb = VR_AGPT_Shoot(b, 2);
        VR_AGPT_Print(sprintf("rag shots (%s): grunt traced %s, armed traced %s", vr_agt_step == 2 ? "alive" : "corpse", ta, tb));
        vr_agt_step = vr_agt_step + 1;
        vr_agt_until = time + 0.3;
        return;
    }
    if(vr_agt_step == 3 || vr_agt_step == 6)
    {
        string st = vr_agt_step == 3 ? "alive" : "corpse";
        float da = vr_agpt_h0a - a.health;
        float db = vr_agpt_h0b - b.health;
        if(vr_agt_step == 6)
        {
            da = vr_agpt_h0a - a.vr_corpse_hp;
            db = vr_agpt_h0b - b.vr_corpse_hp;
        }
        VR_AGPT_Print(sprintf("rag %s: grunt health %g -> %g rag %g, armed %g -> %g rag %g", st, vr_agpt_h0a, a.health,
            isragdoll(a), vr_agpt_h0b, b.health, isragdoll(b)));
        VR_AGT_Check(sprintf("rag %s: the armed grunt takes the shots as the grunt (%g, %g):", st, da, db), db > 0 && da > 0);
        if(vr_agt_step == 3)
        {
            vr_agt_step = 4;
            vr_agt_until = time + 0.1;
            return;
        }
        // Gibbed: a big blast on each corpse.
        T_Damage(a, p, p, 200);
        T_Damage(b, p, p, 200);
        float ga = a.model != "progs/soldier.mdl";
        float gb = b.model != "progs/vrgrunt.mdl";
        VR_AGT_Check(sprintf("rag gib: grunt %s (%s), armed %s (%s):", ga ? "gibbed" : "whole", a.model,
            gb ? "gibbed" : "whole", b.model), ga && gb);
        p.flags = p.flags - (p.flags & FL_NOTARGET);
        cvar_set("vr_agrunt_test", "0");
    }
};

void(float t) VR_AGPT_Frame =
{
    if(t == 20) { VR_AGPT_Ragdoll(); }
};
