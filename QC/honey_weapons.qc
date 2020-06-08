void() splashy =
{
    spawn_tfog(self.origin);
    self.think = splashy;
    self.nextthink = time + 1;
}

void(string lol) poopy =
{
    entity rnde;
    rnde = find(world, classname, lol);
    while(rnde){

        if(rnde.health > 0){
            eprint(rnde);
            rnde.think = splashy;
            rnde.nextthink = time + 0.1;
        }
        rnde = find(rnde, classname, lol);
    }
}
