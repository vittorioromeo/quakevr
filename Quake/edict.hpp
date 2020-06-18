#pragma once

#include "protocol.hpp"
#include "progdefs.hpp"
#include "link.hpp"

#define MAX_ENT_LEAFS 32

struct edict_t
{
    bool free;
    link_t area; /* linked to a division node or leaf */

    unsigned int num_leafs; // QSS
    int leafnums[MAX_ENT_LEAFS];

    entity_state_t baseline;
    unsigned char alpha; /* johnfitz -- hack to support alpha since it's not
                            part of entvars_t */
    bool sendinterval;   /* johnfitz -- send time until nextthink to client
                                for better lerp timing */
    bool onladder; /* spike -- content_ladder stuff */ // QSS

    float freetime; /* qcvm->time when the object was freed */
    entvars_t v;    /* C exported fields from progs */

    /* other fields from progs come immediately after */
};

#define EDICT_FROM_AREA(l) STRUCT_FROM_LINK(l, edict_t, area)
