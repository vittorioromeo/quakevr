void() testspin =
{
	precache_model3("progs/tree.mdl");
	setmodel(self, "progs/tree.mdl");
	
	
}


void() test_spin6 =
{
	self.movetype = MOVETYPE_NOCLIP;
	testspin();
	
	self.avelocity = '0 45 0';
}
void() test_spin7 =
{
	self.movetype = MOVETYPE_FLYMISSILE;
	testspin();
	
	self.avelocity = '0 -45 0';
}
