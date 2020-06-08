// State flags stored in defskeep.qc
// Client parms decoding and encoding save global state

void () envglobal_use = {
    globalstate = (globalstate | self.stateflags); // Activate global state bit
};

void () env_global = {
    if (self.stateflags <= 0) return; // do nothing, go idle
    self.use = envglobal_use;
};

void () logic_auto_think = {
    if (self.use) self.use();
    SUB_UseTargets();
    self.think = self.use = SUB_Null; // go idle
    self.nextthink = time + 0.100;
};

void () logic_auto = {
    self.think = logic_auto_think;
    self.nextthink = time + 0.6 + self.delay; // Give at least a little more than 0.5s to start all other entities before firing targets
};