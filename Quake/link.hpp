#pragma once

#include <cstdint>

struct link_t
{
    link_t *prev, *next;
};

void ClearLink(link_t* l);
void RemoveLink(link_t* l);
void InsertLinkBefore(link_t* l, link_t* before);
void InsertLinkAfter(link_t* l, link_t* after);

// (type *)STRUCT_FROM_LINK(link_t *link, type, member)
// ent = STRUCT_FROM_LINK(link,entity_t,order)
// FIXME: remove this mess!
#define STRUCT_FROM_LINK(l, t, m) ((t*)((byte*)l - (intptr_t) & (((t*)0)->m)))
