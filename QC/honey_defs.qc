#include "vr_macros.qc"

//
//
//
// ----------------------------------------------------------------------------
// Zombie axe hack
// ----------------------------------------------------------------------------

string ZOMBIE_ONGROUND = "ZOMBIE_ONGROUND";

//
//
//
// ----------------------------------------------------------------------------
// Fields
// ----------------------------------------------------------------------------

.float alpha;

.float damage_weapon; //cg: used to indicate what weapon caused the damage.
                      //    so far reliable only for player weapons

.float rangescale;
