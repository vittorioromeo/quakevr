//function to help monsters not shoot each other
float (entity targ) clean_shot =
{
    local vector    spot1, spot2;
    
    spot1 = self.origin + self.view_ofs;
    spot2 = targ.origin + targ.view_ofs;
    traceline (spot1, spot2, FALSE, self);    // try not to shoot through other monsters
    if (trace_ent != targ)
        return FALSE;
    return TRUE;
};