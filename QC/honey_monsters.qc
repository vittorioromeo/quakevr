void() teleport_glitter =
{
    spawn_tdeath(self.origin, self);
    if(self.spawnflags & TRIGGER_SPAWN_TFOG)
    {
        spawn_tfog(self.origin);
    }
};

void() monster_add_to_counter =
{
    if (self.spawnflags & TRIGGER_SPAWN_MULTIPLE){
        if(self.cnt != -1){
                total_monsters = total_monsters + self.cnt + 1;
        }else{
            return;
            //Do nothing, we don't know how many will spawn, so the number will update when they actaully spawn.
        }
    }else{
        total_monsters = total_monsters + 1;
    }

}

/*
================
monster_remove_corpse

================
*/
void() monster_remove_corpse =
{
    //Screw this, just delete it, it's not visible.
    /*
    self.movetype = MOVETYPE_NOCLIP;
    self.velocity = '0 0 -18';
    self.think = SUB_Remove;
    self.nextthink = time + 2;
    */
    SUB_Remove();
}

/*
================
monster_check_remove_corpse

================
*/
void() monster_check_remove_corpse =
{
    local entity player;

    if(!(self.spawnflags & REMOVE_CORPSE)) { return; }

    player = find (world, classname, "player");
    while (player)
    {
        if( canSee(player, self) ) {
            self.think = monster_check_remove_corpse;
            self.nextthink = time + 5;
            return;
        }
        player = find (player, classname, "player");
    }

    monster_remove_corpse();


}
