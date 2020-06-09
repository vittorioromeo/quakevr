#include "link.hpp"

// ClearLink is used for new headnodes
void ClearLink(link_t* l)
{
    l->prev = l->next = l;
}

void RemoveLink(link_t* l)
{
    l->next->prev = l->prev;
    l->prev->next = l->next;
}

void InsertLinkBefore(link_t* l, link_t* before)
{
    l->next = before;
    l->prev = before->prev;
    l->prev->next = l;
    l->next->prev = l;
}

void InsertLinkAfter(link_t* l, link_t* after)
{
    l->next = after->next;
    l->prev = after;
    l->prev->next = l;
    l->next->prev = l;
}
