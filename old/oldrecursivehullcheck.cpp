// TODO VR: (P0) unfortunately the QSS version seems to break item drops n
// stuff, maybe also player stuck. Compare?  Its below
bool SV_RecursiveHullCheck2(hull_t* hull, int num, float p1f, float p2f,
    const qvec3& p1, const qvec3& p2, trace_t* trace)
{
    // QSS
    // Optimize the case where this is a simple point check
    if(p1 == p2)
    {
        // points cannot cross planes, so do it faster
        const auto c = SV_HullPointContents(hull, num, p1);

        // TODO VR: (P2) restore when this is implemented
        // trace->contents = c;

        switch(c)
        {
            case CONTENTS_SOLID:
            {
                trace->startsolid = true;
                break;
            }
            case CONTENTS_EMPTY:
            {
                trace->allsolid = false;
                trace->inopen = true;
                break;
            }
            default:
            {
                trace->allsolid = false;
                trace->inwater = true;
                break;
            }
        }

        return true;
    }

#ifdef PARANOID
    // ------------------------------------------------------------------------
    // VR: Solves weird crashes, probably related to hand pos on spawn.
    if(std::isnan(p1f) || std::isnan(p2f))
    {
        return false;
    }
    // ------------------------------------------------------------------------
#endif

    // check for empty
    if(num < 0)
    {
        if(num != CONTENTS_SOLID)
        {
            trace->allsolid = false;

            if(num == CONTENTS_EMPTY)
            {
                trace->inopen = true;
            }
            else
            {
                trace->inwater = true;
            }
        }
        else
        {
            trace->startsolid = true;
        }

        return true; // empty
    }

    if(num < hull->firstclipnode || num > hull->lastclipnode)
    {
        Sys_Error("SV_RecursiveHullCheck: bad node number");
    }

    //
    // find the point distances
    //
    mclipnode_t* node = hull->clipnodes + num; // johnfitz -- was dclipnode_t
    mplane_t* plane = hull->planes + node->planenum;

    float t1;
    float t2;
    if(plane->type < 3)
    {
        t1 = p1[plane->type] - plane->dist;
        t2 = p2[plane->type] - plane->dist;
    }
    else
    {
        t1 = DoublePrecisionDotProduct(plane->normal, p1) - plane->dist;
        t2 = DoublePrecisionDotProduct(plane->normal, p2) - plane->dist;
    }

    // ------------------------------------------------------------------------
    // VR: Solves weird crashes, probably related to hand pos on spawn.
    if(std::isnan(t1) || std::isnan(t2))
    {
        return false;
    }
    // ------------------------------------------------------------------------

#if 1
    if(t1 >= 0 && t2 >= 0)
    {
        return SV_RecursiveHullCheck(
            hull, node->children[0], p1f, p2f, p1, p2, trace);
    }

    if(t1 < 0 && t2 < 0)
    {
        return SV_RecursiveHullCheck(
            hull, node->children[1], p1f, p2f, p1, p2, trace);
    }
#else
    if((t1 >= DIST_EPSILON && t2 >= DIST_EPSILON) || (t2 > t1 && t1 >= 0))
        return SV_RecursiveHullCheck(
            hull, node->children[0], p1f, p2f, p1, p2, trace);

    if((t1 <= -DIST_EPSILON && t2 <= -DIST_EPSILON) || (t2 < t1 && t1 <= 0))
        return SV_RecursiveHullCheck(
            hull, node->children[1], p1f, p2f, p1, p2, trace);
#endif

    // put the crosspoint DIST_EPSILON pixels on the near side
    float frac;

    if(t1 < 0)
    {
        frac = (t1 + DIST_EPSILON) / (t1 - t2);
    }
    else
    {
        frac = (t1 - DIST_EPSILON) / (t1 - t2);
    }

    if(frac < 0)
    {
        frac = 0;
    }
    else if(frac > 1)
    {
        frac = 1;
    }

    qvec3 mid;
    float midf = p1f + (p2f - p1f) * frac;
    for(int i = 0; i < 3; i++)
    {
        mid[i] = p1[i] + frac * (p2[i] - p1[i]);
    }

    int side = (t1 < 0);

    // move up to the node
    if(!SV_RecursiveHullCheck(
           hull, node->children[side], p1f, midf, p1, mid, trace))
    {
        return false;
    }

#if 0
#ifdef PARANOID
    if(SV_HullPointContents(sv_hullmodel, mid, node->children[side]) ==
        CONTENTS_SOLID)
    {
        Con_Printf("mid PointInHullSolid\n");
        return false;
    }
#endif
#endif

    if(SV_HullPointContents(hull, node->children[side ^ 1], mid) !=
        CONTENTS_SOLID)
    {
        // go past the node
        return SV_RecursiveHullCheck(
            hull, node->children[side ^ 1], midf, p2f, mid, p2, trace);
    }

    if(trace->allsolid)
    {
        return false; // never got out of the solid area
    }

    //==================
    // the other side of the node is solid, this is the impact point
    //==================
    if(!side)
    {
        trace->plane.normal = plane->normal;
        trace->plane.dist = plane->dist;
    }
    else
    {
        trace->plane.normal = vec3_zero - plane->normal;
        trace->plane.dist = -plane->dist;
    }

    while(
        SV_HullPointContents(hull, hull->firstclipnode, mid) == CONTENTS_SOLID)
    {
        // shouldn't really happen, but does occasionally
        frac -= 0.1;
        if(frac < 0)
        {
            trace->fraction = midf;
            trace->endpos = mid;
            Con_DPrintf("backup past 0\n");
            return false;
        }
        midf = p1f + (p2f - p1f) * frac;
        for(int i = 0; i < 3; i++)
        {
            mid[i] = p1[i] + frac * (p2[i] - p1[i]);
        }
    }

    trace->fraction = midf;
    trace->endpos = mid;

    return false;
}
