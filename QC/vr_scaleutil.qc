//
//
//
// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

vector(entity xEnt) VR_ScaleUtil_GetScale =
{
    return xEnt.scale + '1 1 1';
}

void(entity xEnt, vector xScale) VR_ScaleUtil_SetScale =
{
    xEnt.scale = xScale - '1 1 1';
}

vector(float xFactor) VR_ScaleUtil_GetUniformScaleVector =
{
    vector factorVector;
    factorVector.x = xFactor;
    factorVector.y = xFactor;
    factorVector.z = xFactor;
    return factorVector;
}
