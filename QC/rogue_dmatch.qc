// dmatch - general deathmatch additions

// =======================================
// =======================================
void() tag_token_respawn =
{
    entity spawnPoint;
    entity tagToken;

    tagToken = find(world, classname, "dmatch_tag_token");
    if(tagToken == world)
        return;

    spawnPoint = SelectSpawnPoint();
    setorigin(tagToken, spawnPoint.origin);
    tag_token_owner = world;
    tagToken.solid = SOLID_TRIGGER;
    tagToken.touch = tag_token_touch;
    tagToken.think = SUB_Null;
    tagToken.owner = world;
    tagToken.tag_frags = 0;
    droptofloor();
};

// =======================================
// =======================================
void() tag_token_fall =
{
    self.tag_frags = 0;
    self.think = tag_token_respawn;
    self.nextthink = time + 30;
    droptofloor();
};

// =======================================
// =======================================
void() tag_token_drop =
{
    entity tagToken;

    tagToken = find(world, classname, "dmatch_tag_token");
    if(tagToken == world)
        return;

    bprint(tagToken.owner.netname);
    bprint(" lost the tag token!\n");
    tagToken.tag_frags = 0;
    tagToken.solid = SOLID_TRIGGER;
    tagToken.owner = world;
    tagToken.touch = tag_token_touch;
    tagToken.think = tag_token_fall;
    tagToken.nextthink = time + 0.1;
};

// =======================================
// =======================================
void() tag_token_think =
{
    if(self.owner.health > 0)
    {
        if(self.tag_message_time < time)
        {
            bprint(self.owner.netname);
            bprint(" has the tag token!\n");
            self.tag_message_time = time + 30;
        }

        setorigin(self, self.owner.origin + '0 0 48');
        self.think = tag_token_think;
        self.nextthink = time + 0.1;
    }
    else
    {
        tag_token_drop();
    }
};

// =======================================
// =======================================
void() tag_token_touch =
{
    if(other.classname != "player")
        return;

    tag_token_owner = other;
    self.tag_frags = 0;

    sound(self, CHAN_AUTO, "runes/end1.wav", 1, ATTN_NORM);

    bprint(other.netname);
    bprint(" got the tag token!\n");
    self.tag_message_time = time + 30;

    self.owner = other;
    self.solid = SOLID_NOT;
    self.touch = SUB_Null;
    self.think = tag_token_think;
    self.nextthink = time + 0.1;
};

/*QUAKED dmatch_tag_token (1 1 0) (-16 -16 -16) (16 16 16)
The deathmatch tag token.
*/
void() dmatch_tag_token =
{
    if(cvar_hget(cvarh_teamplay) != TEAM_DMATCH_TAG)
    {
        remove(self);
        return;
    }

    precache_model("progs/sphere.mdl");
    precache_sound("runes/end1.wav");
    setmodel(self, "progs/sphere.mdl");
    self.skin = 1;
    setsize(self, '-16 -16 -16', '16 16 16');
    self.touch = tag_token_touch;
    self.effects = self.effects | EF_DIMLIGHT;
    StartItem();
};

// =======================================
// =======================================
void(entity targ, entity attacker) dmatch_score =
{
    entity tagToken;

    if(teamplay == TEAM_DMATCH_TAG)
    {
        tagToken = find(world, classname, "dmatch_tag_token");
        if(tagToken == world)
        {
            attacker.frags = attacker.frags + 1;
            return;
        }

        if(attacker == tag_token_owner)
        {
            tagToken.tag_frags = tagToken.tag_frags + 1;
            attacker.frags = attacker.frags + 3;
            if(tagToken.tag_frags == 5)
            {
                sprint(attacker, "You got the Quad Damage\n");
                VR_ItemUtil_EntAddItem(attacker, IID_QUAD);
                stuffcmd (attacker, "bf\n");
                sound(attacker,CHAN_VOICE,"items/damage.wav", 1, ATTN_NORM);
                attacker.super_time = 1;
                attacker.super_damage_finished = time + 30;
            }
            else if(tagToken.tag_frags == 10)
            {
                bprint(attacker.netname);
                bprint(" lost the tag token\n");
                tagToken.solid = SOLID_TRIGGER;
                tagToken.tag_frags = 0;
                tagToken.touch = tag_token_touch;
                tag_token_respawn();
            }
        }
        else
        {
            if(targ == tag_token_owner)
            {
                attacker.frags = attacker.frags + 5;
                sound(self, CHAN_AUTO, "runes/end1.wav", 1, ATTN_NORM);
                if(attacker.classname == "player")
                {
                    tag_token_owner = attacker;

                    tagToken.tag_frags = 0;
                    tagToken.tag_message_time = time + 0.5;
                    tagToken.owner = attacker;
                    tagToken.solid = SOLID_NOT;
                    tagToken.touch = SUB_Null;
                    tagToken.think = tag_token_think;
                    tagToken.nextthink = time + 0.1;
                }
            }
            else
                attacker.frags = attacker.frags + 1;
        }
    }
};


