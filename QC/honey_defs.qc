//
//
//
// ----------------------------------------------------------------------------
// Implementation utilities
// ----------------------------------------------------------------------------

// TODO VR: (P2) move
#define VRUTIL_POWER_OF_TWO(xExponent) (1 << xExponent)

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
