void() corpse_flayed1 = {
    precache_model("progs/corpse_flay.mdl");
    setmodel(self, "progs/corpse_flay.mdl");
    self.frame = 0;
    self.skin = 0;
};

void() corpse_flayed2 = {
    precache_model("progs/corpse_flay.mdl");
    setmodel(self, "progs/corpse_flay.mdl");
    self.frame = 0;
    self.skin = 1;
};

void() corpse_flayed3 = {
    precache_model("progs/corpse_flay.mdl");
    setmodel(self, "progs/corpse_flay.mdl");
    self.frame = 2;
    self.skin = 2;
};

void() corpse_flayed4 = {
    precache_model("progs/corpse_flay.mdl");
    setmodel(self, "progs/corpse_flay.mdl");
    self.frame = 1;
    self.skin = 1;
};

void() corpse_lynched1 = {
    precache_model("progs/corpse_lynch.mdl");
    setmodel(self, "progs/corpse_lynch.mdl");
    self.frame = 0;
    self.skin = 0;
};

void() corpse_lynched2 = {
    precache_model("progs/corpse_lynch.mdl");
    setmodel(self, "progs/corpse_lynch.mdl");
    self.frame = 0;
    self.skin = 0;
};

void() corpse_lynched3 = {
    precache_model("progs/corpse_lynch.mdl");
    setmodel(self, "progs/corpse_lynch.mdl");
    self.frame = 1;
    self.skin = 2;
};

void() corpse_crucified1 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 7;
    self.frame = 9;
    makestatic(self);
};

void() corpse_crucified2 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 8;
    self.frame = 10;
    makestatic(self);
};

void() corpse_impaled_back1 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 0;
    self.frame = 0;
    makestatic(self);
};

void() corpse_impaled_back2 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 1;
    self.frame = 0;
    makestatic(self);
};

void() corpse_impaled_back3 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 2;
    self.frame = 1;
    makestatic(self);
};

void() corpse_impaled_front1 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 0;
    self.frame = 2;
    makestatic(self);
};

void() corpse_impaled_front2 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 1;
    self.frame = 2;
    makestatic(self);
};

void() corpse_impaled_front3 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 3;
    self.frame = 3;
    makestatic(self);
};

void() corpse_impaled_horizontal1 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 0;
    self.frame = 4;
    makestatic(self);
};

void() corpse_impaled_horizontal2 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 1;
    self.frame = 4;
    makestatic(self);
};

void() corpse_impaled_horizontal3 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 4;
    self.frame = 5;
    makestatic(self);
};

void() corpse_impaled_horizontal4 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 5;
    self.frame = 6;
    makestatic(self);
};

void() corpse_impaled_vertical1 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 0;
    self.frame = 7;
    makestatic(self);
};

void() corpse_impaled_vertical2 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 1;
    self.frame = 7;
    makestatic(self);
};

void() corpse_impaled_vertical3 = {
    precache_model("progs/corpse_imp.mdl");
    setmodel(self, "progs/corpse_imp.mdl");
    self.skin = 6;
    self.frame = 8;
    makestatic(self);
};