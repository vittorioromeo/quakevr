// vr_copper_defs.qc -- Copper's rules: the bridge's declarations, early (vr_copper_bridge.qc, after Copper's imported
// files, defines them; DWELL.md P1b). Copper's code (QC/copper, cu_<name>) runs the world logic of a map played by
// Copper's rules (its triggers, doors, trains, targets: vr_rules.cpp); our monsters, items and weapons stay ours.

// The map plays by Copper's rules (the Dwell campaign, a package made for Copper or Dwell, or its signals).
float() CU_Rules =
{
    return VR_Ruleset() == VR_RULES_COPPER;
};

float(entity e) CU_Run;     // Copper's code owns this entity's damage and death (its breakables, shootable doors)
void() CU_WorldSpawn;       // worldspawn: Copper's world setup after ours, in a Copper map
void() CU_Frame;            // StartFrame: what Copper's own StartFrame did, in a Copper map
// combat.qc T_DamageImpl: our damage on one of Copper's entities, by Copper's T_Damage. TRUE: done.
float(entity targ, entity inflictor, entity attacker, float damage) CU_Damage;
// Copper's T_Damage (a reviewed patch, copper_import.py): on one of ours, our T_Damage takes it. TRUE: done.
float(entity targ, entity inflictor, entity attacker, float damage) VR_CU_DamageOurs;
// Copper's target_autosave (a reviewed patch): our autosave, as our level starts make it.
void() VR_CU_Autosave;
void(entity xBlocker) VR_CU_ShaftBlocked; // Copper's plats, doors and trains blocked: lift shafts kept clear
void(entity xSpot) CU_PlayerSpawnedAt; // PutClientInServer: a Copper spawn point's targets fired
