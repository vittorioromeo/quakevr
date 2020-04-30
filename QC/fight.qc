
/*

A monster is in fight mode if it thinks it can effectively attack its
enemy.

When it decides it can't attack, it goes into hunt mode.

*/

float(float v) anglemod;

void() knight_atk1;
void() knight_runatk1;
void() ogre_smash1;
void() ogre_swing1;

void() sham_smash1;
void() sham_swingr1;
void() sham_swingl1;

void(float side)    Demon_Melee;

void(vector dest) ChooseTurn;



float    enemy_vis, enemy_infront, enemy_range;
float    enemy_yaw;


void() knight_attack =
{
    local float        len;

// decide if now is a good swing time
    len = vlen(self.enemy.origin+self.enemy.view_ofs - (self.origin+self.view_ofs));

    if(len<80)
        knight_atk1();
    else
        knight_runatk1();
};

//=============================================================================
