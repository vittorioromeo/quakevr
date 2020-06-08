/*
=============
point_visible

returns 1 if the entity is visible to self's origin, even if not infront ()
=============
*/

float (vector spot1, vector spot2) point_visible =
{
    // see through other monsters
    traceline(spot1, spot2, TRUE, self);

    if(trace_inopen && trace_inwater)
    {
        // sight line crossed contents
        return FALSE;
    }

    //MED 11/21/96
    if(trace_fraction == 1)
    {
        visible_distance = vlen(spot2 - spot1);
        return TRUE;
    }

    return FALSE;
}

/*
=============
bbox_visible

returns 1 if the entity is visible to self's bounding box, even if not infront ()
=============
*/

float (entity client, vector spot2) bbox_visible =
{
    vector spot1, bboxspot;
    float res;
    float bbox_scale = 1.25;

//    dprint_str_v_str("player is at ",spot2, "\n");

    spot1 = client.origin + client.view_ofs;

    bboxspot_x = spot1_x + client.maxs_x * bbox_scale;
    bboxspot_y = spot1_y + client.maxs_y * bbox_scale;
    bboxspot_z = spot1_z;
//    dprint_str_v_str("1: Testing visibility to ",bboxspot, "\n");
    res = point_visible(bboxspot, spot2);
    if(res) return res;

    bboxspot_x = spot1_x + client.mins_x * bbox_scale;
    bboxspot_y = spot1_y + client.maxs_y * bbox_scale;
    bboxspot_z = spot1_z;
//    dprint_str_v_str("2: Testing visibility to ",bboxspot, "\n");
    res = point_visible(bboxspot, spot2);
    if(res) return res;

    bboxspot_x = spot1_x + client.maxs_x * bbox_scale;
    bboxspot_y = spot1_y + client.mins_y * bbox_scale;
    bboxspot_z = spot1_z;
//    dprint_str_v_str("3: Testing visibility to ",bboxspot, "\n");
    res = point_visible(bboxspot, spot2);
    if(res) return res;

    bboxspot_x = spot1_x + client.mins_x * bbox_scale;
    bboxspot_y = spot1_y + client.mins_y * bbox_scale;
    bboxspot_z = spot1_z;
//    dprint_str_v_str("4: Testing visibility to ",bboxspot, "\n");
    res = point_visible(bboxspot, spot2);
    if(res) return res;

//    dprint("Not visible.\n");

    return FALSE;
}

float(vector targ) infront_v =
{
    vector vec;
    float  dot;

    makevectors(self.angles);
    vec = normalize(targ - self.origin);
    dot = vec * v_forward;

    if(dot > 0.4)
    {
        return TRUE;
    }

    return FALSE;
}

/*
=============
canSee

returns 1 if the entity is in front (in sight) of self and in line of sight
=============
*/
float(entity src, entity targ) canSee =
{
    float	res;

    entity oself;
    oself = self;
    res = FALSE;
    self = src;

    if(infront(targ))
        if(bbox_visible(targ, src.origin + src.view_ofs))
            res = TRUE;

    self = oself;
    return res;
}
