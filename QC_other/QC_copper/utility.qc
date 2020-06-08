
void(string s, string ss) bprint2 = 
	{ bprint(s); bprint(ss); }
void(string s, string ss, string sss) bprint3 = 
	{ bprint(s); bprint(ss); bprint(sss); }
void(string s, string ss, string sss, string ssss) bprint4 = 
	{ bprint(s); bprint(ss); bprint(sss); bprint(ssss); }
void(string s, string ss, string sss, string ssss, string sssss) bprint5 = 
	{ bprint(s); bprint(ss); bprint(sss); bprint(ssss); bprint(sssss); }

void(string s, string ss) dprint2 = 
	{ dprint(s); dprint(ss); }
void(string s, string ss, string sss) dprint3 = 
	{ dprint(s); dprint(ss); dprint(sss); }
void(string s, string ss, string sss, string ssss) dprint4 = 
	{ dprint(s); dprint(ss); dprint(sss); dprint(ssss); }
void(string s, string ss, string sss, string ssss, string sssss) dprint5 = 
	{ dprint(s); dprint(ss); dprint(sss); dprint(ssss); dprint(sssss); }


void(string s) centerprintall =
{
	entity e;
	e = find(world, classname, "player");
		
	while (e != world)
	{
		centerprint(e, s);
		e = find(e, classname, "player");
	}
}

//============================================================================

// findradius only works on solid entities, because apparently it's only for explosions
entity(vector point, float rad, .string fld, string match) findradiusplus =
{
	entity head, prev;
	prev = world;
	head = find(world, fld, match);
	
	float found = 0;
	
	while(head)
	{
		if ( vlen(point - head.origin) <= rad )
		{
			head.chain = prev;		
			prev = head;
			found += 1;
		}
		head = find(head, fld, match);
	}
	return prev;
}


// just find every damn thing targeted by 'src' and return it as head.chain
// targets will be primarily in order by target/2/3/4, and secondarily by spawn order
// so 'target's always come before 'target2's in the chain, etc
entity(entity src) findalltargets =
{
	// WARNING: do not use this in the context of actually triggering targets.
	// an entity triggering down an active .chain of targets could trigger one
	// which itself calls findalltargets to do the same, which could blow away
	// the existing chain.
	entity head, f, prev;
	head = world;
	f = world;
	prev = world;
	
	if (src.target != string_null)
	{
		f = find(world, targetname, src.target);
		while(f)
		{
			if (!head) head = f;	// mark the start location
			if (prev)
				prev.chain = f;		// connect the end of the chain
			prev = f;
			f = find(f, targetname, src.target);
		}
	}
	if (src.target2 != string_null)
	{
		f = find(world, targetname, src.target2);
		while(f)
		{
			if (!head) head = f;
			if (prev)
				prev.chain = f;
			prev = f;
			f = find(f, targetname, src.target2);
		}
	}
	if (src.target3 != string_null)
	{
		f = find(world, targetname, src.target3);
		while(f)
		{
			if (!head) head = f;
			if (prev)
				prev.chain = f;
			prev = f;
			f = find(f, targetname, src.target3);
		}
	}
	if (src.target4 != string_null)
	{
		f = find(world, targetname, src.target4);
		while(f)
		{
			if (!head) head = f;
			if (prev)
				prev.chain = f;
			prev = f;
			f = find(f, targetname, src.target4);
		}
	}
	
	prev.chain = world;	// don't link to stale chains
	return head;
}

// findalltargets, but in reverse: returns a chain of all entities that target dst
entity(entity dst) findalltargeters =
{
	if (dst.targetname == string_null)
		return world;
	
	entity head, f, prev;
	head = world;
	f = world;
	prev = world;
	
	f = find(world, target, dst.targetname);
	while(f)
	{
		if (!head) head = f;	// mark the start location
		if (prev)
			prev.chain = f;		// connect the end of the chain
		prev = f;
		f = find(f, target, dst.targetname);
	}

	f = find(world, target2, dst.targetname);
	while(f)
	{
		if (!head) head = f;
		if (prev)
			prev.chain = f;
		prev = f;
		f = find(f, target2, dst.targetname);
	}

	f = find(world, target3, dst.targetname);
	while(f)
	{
		if (!head) head = f;
		if (prev)
			prev.chain = f;
		prev = f;
		f = find(f, target3, dst.targetname);
	}

	f = find(world, target4, dst.targetname);
	while(f)
	{
		if (!head) head = f;
		if (prev)
			prev.chain = f;
		prev = f;
		f = find(f, target4, dst.targetname);
	}

	prev.chain = world;	// don't link to stale chains
	return head;
}

entity(entity start, .string fld, string match) findunlocked =
{
	entity head;
	float found = FALSE;
	head = find(start, fld, match);
	while (head)
	{
		if (!(head.customflags & CFL_LOCKED))
			break;
		head = find(head, fld, match);
	}
	return head;
}

entity(entity start) findunlockedtarget =
{
	entity head = world;
	if (self.target != string_null)
		head = findunlocked(start, targetname, self.target);
	if (head) return head;
	
	if (self.target2 != string_null)
		head = findunlocked(start, targetname, self.target2);
	if (head) return head;
	
	if (self.target3 != string_null)
		head = findunlocked(start, targetname, self.target3);
	if (head) return head;
	
	if (self.target4 != string_null)
		return findunlocked(start, targetname, self.target4);
	return world;
}


/*
i've brought over the quoth convention of letting mappers set the 'model' key on func_*
to override an entity's model with something external, which must be precached, but that 
collides with the existing use of the model keyvalue, and also causes precache-out-of-turn
errors on maps with map hacks like mce.bsp

however, if the entity already has a modelindex, it already has a model internal to the
bsp and precache should never be called no matter what time it is.
*/
void(string s) precache_safe_model	= { if (s != string_null && !self.modelindex) precache_model2(s); }

/*
failed precaches still prevent the map from loading in most engines. put all mod-related 
precaches through these wrappers so they can be easily turned off, in the event you want 
to send small test builds to engine programmers to show them bugs without having to 
build and package your entire mod.
*/
void(string s) precache_model3	= { precache_model2(s); }
void(string s) precache_sound3	= { precache_sound2(s); }
/*
void(string s) precache_model3	= { return; }
void(string s) precache_sound3	= { return; }
*/


// shorthand for turning -1 to 0 for keyvalues for which 0 is a valid non-default selection
float(float in) zeroconvert =
{
	if (in == -1) return 0;
	return in;
}
float(float in, float def) zeroconvertdefault =
{
	if (in == -1) return 0;
	if (in == 0) return def;
	return in;
}




// ================================
//	Frustum/Box Tests
// ================================

vector(entity e) BoundsCenter =
{
	return 0.5 * ( e.absmin + e.absmax );
}

vector(entity e) BoundsBottom =
{
	vector out;
	out = 0.5 * ( e.absmin + e.absmax );
	out_z = e.absmin_z;
	return out;
}

// for measuring how large an entity is along an arbitrary vector
// FIXME: this is trash and it returns trash
float(vector v, vector s) BoundsAngleSize =
{
	v_x = fabs(v_x);
	v_y = fabs(v_y);
	v_z = fabs(v_z);
	
	// size is always + + + but this is in case I switch the parameters somewhere
	s_x = fabs(s_x);
	s_y = fabs(s_y);
	s_z = fabs(s_z);
	
	return v * s;
}

float (vector e1mins, vector e1maxs, vector e2mins, vector e2maxs, float pad) BoundsTouching =
{
	if (e1mins_x > e2maxs_x + pad) return FALSE;
	if (e1mins_y > e2maxs_y + pad) return FALSE;
	if (e1mins_z > e2maxs_z + pad) return FALSE;
	if (e1maxs_x < e2mins_x - pad) return FALSE;
	if (e1maxs_y < e2mins_y - pad) return FALSE;
	if (e1maxs_z < e2mins_z - pad) return FALSE;
	return TRUE;
}

// moved from doors
float (entity e1, entity e2) EntitiesTouching =
{
	return BoundsTouching(e1.absmin, e1.absmax, e2.absmin, e2.absmax, 0);
}

float(vector org, float hside, vector point) BoundingCubeTest =
{
	if (point_x > org_x + hside) return FALSE;
	if (point_x < org_x - hside) return FALSE;
	if (point_y > org_y + hside) return FALSE;
	if (point_y < org_y - hside) return FALSE;
	if (point_z > org_z + hside) return FALSE;
	if (point_z < org_z - hside) return FALSE;
	return TRUE;
}

float(vector planeN, float planeD, vector point) PlaneTest =
{
	return point * planeN - planeD;
}

float(vector planeN, float planeD, vector mn, vector mx) BoxPlaneTest =
{
	local float outCount;

	outCount = 0;
	
	// you know what's cool?  arrays.
	if (PlaneTest(planeN, planeD, Vector(mn_x, mn_y, mn_z)) <= 0) outCount++;
	if (PlaneTest(planeN, planeD, Vector(mx_x, mn_y, mn_z)) <= 0) outCount++;
	if (PlaneTest(planeN, planeD, Vector(mn_x, mx_y, mn_z)) <= 0) outCount++;
	if (PlaneTest(planeN, planeD, Vector(mx_x, mx_y, mn_z)) <= 0) outCount++;
	if (PlaneTest(planeN, planeD, Vector(mn_x, mn_y, mx_z)) <= 0) outCount++;
	if (PlaneTest(planeN, planeD, Vector(mx_x, mn_y, mx_z)) <= 0) outCount++;
	if (PlaneTest(planeN, planeD, Vector(mn_x, mx_y, mx_z)) <= 0) outCount++;
	if (PlaneTest(planeN, planeD, Vector(mx_x, mx_y, mx_z)) <= 0) outCount++;
	
	dprint(": ");
	dprint(ftos(outCount));
	dprint(" ");
	
	return outCount;
}

float(entity observer, float fov, vector mn, vector mx) FrustumCheck =
{
	local vector vorg;
	vorg = observer.origin + observer.view_ofs;
	makevectors(observer.v_angle);
	
	// assume vertical fov is at most 3/4 hfov, because nobody plays quake in a square window
	local float hfov, vfov;
	hfov = fov * 0.6;
	vfov = fov * 0.45; // was 0.5/0.375
	// added a 20% buffer because engines like mk5 and quakespasm have added "correction" 
	// so now hfov can't be trusted to just be the actual fucking fov anymore
	
	local float bpt, hc, hs, vc, vs;
	local vector temp, side0n, side1n, side2n, side3n, side4n;
	local float side0d, side1d, side2d, side3d, side4d;
	
	temp = SinCos(hfov);
	hs = temp_x;
	hc = temp_y;
	
	temp = SinCos(vfov);
	vs = temp_x;
	vc = temp_y;
	
	// determine frustum planes one at a time, starting with near, then left/right, then top/bottom
	// far plane isn't really necessary
	side0n = v_forward;
	side0d = vorg * side0n;
	dprint("NR");
	bpt = BoxPlaneTest(side0n, side0d, mn, mx);
	if (bpt == 8) return 0;
	
	side1n = v_forward * hs + v_right * hc;
	side1d = vorg * side1n;
	dprint("LF");
	bpt = BoxPlaneTest(side1n, side1d, mn, mx);
	if (bpt == 8) return 0;
	
	side2n = v_forward * hs - v_right * hc;
	side2d = vorg * side2n;
	dprint("RT");
	bpt = BoxPlaneTest(side2n, side2d, mn, mx);
	if (bpt == 8) return 0;
	
	side3n = v_forward * vs + v_up * vc;
	side3d = vorg * side3n;
	dprint("DN");
	bpt = BoxPlaneTest(side3n, side3d, mn, mx);
	if (bpt == 8) return 0;
	
	side4n = v_forward * vs - v_up * vc;
	side4d = vorg * side4n;
	dprint("UP");
	bpt = BoxPlaneTest(side4n, side4d, mn, mx);
	if (bpt == 8) return 0;
	
	return 1;	
}





// the following amazing pollution is by czg

/*
=============
stuffcmd_float

This is a horrible hack that I am ashamed of!
===============
*/
void stuffcmd_digit( entity client, float f) =
{
	float d;
	d = floor(f);
	d = mod(d, 10);

	//CLOSE YOUR EYES, HONEY! DON'T LOOK!
	if(d == 0)
		stuffcmd(client, "0");
	else if(d == 1)
		stuffcmd(client, "1");
	else if(d == 2)
		stuffcmd(client, "2");
	else if(d == 3)
		stuffcmd(client, "3");
	else if(d == 4)
		stuffcmd(client, "4");
	else if(d == 5)
		stuffcmd(client, "5");
	else if(d == 6)
		stuffcmd(client, "6");
	else if(d == 7)
		stuffcmd(client, "7");
	else if(d == 8)
		stuffcmd(client, "8");
	else if(d == 9)
		stuffcmd(client, "9");	
}

void stuffcmd_int( entity client, float f, float numdigits) =
{
	
	float tmp;

	
	if(f == 0)
	{
		stuffcmd( client, "0");
		return;
	}

	if(f < 0)
	{
		// Yeah sure.
		stuffcmd( client, "-");
		f = fabs(f);
	}
	
	if(numdigits <= 0)
	{
		tmp = f;
		numdigits = 1;
		while(tmp >= 1){
			tmp = tmp / 10;
			numdigits = numdigits * 10;
		}
	}
	
	//I don't know what I'm thinking here...
	//I need to do this to get zero-padding to work.

	while( numdigits > 1 )
	{
		numdigits = numdigits / 10;
		tmp = f / numdigits;
		stuffcmd_digit( client, tmp);
	}
}

void stuffcmd_float( entity client, float f) =
{
	float intpart, decpart, isNegative;
	
	isNegative = FALSE;
	
	if(f == 0)
	{
		stuffcmd( client, "0");
		return;
	}
	
	if(f < 0)
	{
		// easier this way
		isNegative = TRUE;
		f = fabs(f);
	}	
	
	// 1: stuff the integer part.
	intpart = floor(f);
	if(isNegative) 
		stuffcmd( client, "-");
	stuffcmd_int( client, intpart, 0);
	
	// 2: stuff the decimal point.
	stuffcmd( client, ".");
	
	// 3: stuff the decimal part.
	decpart = mod( f, 1);
	decpart = decpart * 10000;
	stuffcmd_int( client, decpart, 10000);
}
