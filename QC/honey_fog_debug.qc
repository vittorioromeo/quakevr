void() fdbg_redUp;
void() fdbg_redDown;
void() fdbg_greenUp;
void() fdbg_greenDown;
void() fdbg_blueUp;
void() fdbg_blueDown;
void() fdbg_densityUp;
void() fdbg_densityDown;

float STEP = 0.001;

float() getCurrentFogDensity =
{
    if(self.fog_override_owner) return self.fog_override_density;
    else return self.fog_local_density;
}

vector() getCurrentFogColor =
{
    if(self.fog_override_owner) return self.fog_override_color;
    else return self.fog_local_color;
}

void() fdbg_redUp = {
    vector c;
    float d;
    
    d = getCurrentFogDensity();
    c = getCurrentFogColor();
    
    c_x = clamp(c_x + STEP, 0, 1);
    
    
    
    fog_setOverride (self, c, d, 0, 0.05);
    dprint_str_v_str("Fog color: ",c," ");
    dprint_str_f_str("Fog density: ",d,"\n");
}

void() fdbg_redDown = {
    vector c;
    float d;
    
    d = getCurrentFogDensity();
    c = getCurrentFogColor();
    
    c_x = clamp(c_x - STEP, 0, 1);
    
    fog_setOverride (self, c, d, 0, 0.05);
    dprint_str_v_str("Fog color: ",c," ");
    dprint_str_f_str("Fog density: ",d,"\n");
}

void() fdbg_greenUp = {
    vector c;
    float d;
    
    d = getCurrentFogDensity();
    c = getCurrentFogColor();
    
    c_y = clamp(c_y + STEP, 0, 1);
    
    
    
    fog_setOverride (self, c, d, 0, 0.05);
    dprint_str_v_str("Fog color: ",c," ");
    dprint_str_f_str("Fog density: ",d,"\n");
}

void() fdbg_greenDown = {
    vector c;
    float d;
    
    d = getCurrentFogDensity();
    c = getCurrentFogColor();
    
    c_y = clamp(c_y - STEP, 0, 1);
    
    fog_setOverride (self, c, d, 0, 0.05);
    dprint_str_v_str("Fog color: ",c," ");
    dprint_str_f_str("Fog density: ",d,"\n");
}

void() fdbg_blueUp = {
    vector c;
    float d;
    
    d = getCurrentFogDensity();
    c = getCurrentFogColor();
    
    c_z = clamp(c_z + STEP, 0, 1);
    
    
    
    fog_setOverride (self, c, d, 0, 0.05);
    dprint_str_v_str("Fog color: ",c," ");
    dprint_str_f_str("Fog density: ",d,"\n");
}

void() fdbg_blueDown = {
    vector c;
    float d;
    
    d = getCurrentFogDensity();
    c = getCurrentFogColor();
    
    c_z = clamp(c_z - STEP, 0, 1);
    
    fog_setOverride (self, c, d, 0, 0.05);
    dprint_str_v_str("Fog color: ",c," ");
    dprint_str_f_str("Fog density: ",d,"\n");
}

void() fdbg_densityUp = {
    vector c;
    float d;
    
    d = getCurrentFogDensity();
    c = getCurrentFogColor();
    
    d = clamp(d + STEP, 0, 1);
    
    
    
    fog_setOverride (self, c, d, 0, 0.05);
    dprint_str_v_str("Fog color: ",c," ");
    dprint_str_f_str("Fog density: ",d,"\n");
}

void() fdbg_densityDown = {
    vector c;
    float d;
    
    d = getCurrentFogDensity();
    c = getCurrentFogColor();
    
    d = clamp(d - STEP, 0, 1);
    
    fog_setOverride (self, c, d, 0, 0.05);
    dprint_str_v_str("Fog color: ",c," ");
    dprint_str_f_str("Fog density: ",d,"\n");
}
