#pragma once

struct bonepose_t;

struct lerpdata_t
{
    short pose1;
    short pose2;
    float blend;
    qvec3 origin;
    qvec3 angles;
    bonepose_t* bonestate;
};
