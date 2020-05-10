// ===================
//	MATHS.
// ===================

float mathlib_accuracy; //accuracy of sqrt & pow

// frikqcc doesn't like it when i swizzle directly
// TODO: see if fteqcc cares
vector(float x, float y, float z) Vector =
{
	local vector swiz;
	swiz_x = x;
	swiz_y = y;
	swiz_z = z;
	return swiz;
}

float (vector a, vector b) vdist =
{
	return vlen(b-a);
}
float (vector a, vector b) hdist =
{
	vector c = b - a;
	c_z = 0;
	return vlen(c);
}

float (vector v) vlensqr =
{
	return v_x * v_x + v_y * v_y + v_z * v_z;
}

vector(float num) SinCos =
{
	local vector angle, vf, vu, vr, result;
	
	vf = v_forward;
	vu = v_up;
	vr = v_right;
	
	angle = '0 1 0' * num;
	makevectors(angle);
	result = Vector(v_forward_y,v_forward_x,0);
	
	v_forward = vf;
	v_up = vu;
	v_right = vr;
	
	return result;
}

float(float num) sin =
{
	vector temp;
	temp = SinCos(num);
	return temp_x;
}

float(float num) cos =
{
	vector temp;
	temp = SinCos(num);
	return temp_y;
}

float(float num) tan =
{
	vector temp;
	temp = SinCos(num);
	return temp_x / temp_y;
}
 
float(float y, float x) atan2 =
{
	local vector ang; //temporary used to calculate trig values
	ang = '0 0 0';
	ang_x = x;
	ang_y = y;
	return vectoyaw(ang);
}

vector(vector a, vector b) CrossProduct =
{
	vector c;
	
	c_x = a_y * b_z - a_z * b_y;
	c_y = a_z * b_x - a_x * b_z;
	c_z = a_x * b_y - a_y * b_x;
	
	return c;
}

float(vector a, vector b) DotProduct =
{
//	return (a_x * b_x) + (a_y * b_y) + (a_z * b_z);
	return a * b;	// you idiot
}

float(float a, float b, float mix) lerp =
{
	if (mix <= 0) return a;
	if (mix >= 1) return b;
	return (b * mix + a * ( 1 - mix ) );
}

vector(vector a, vector b, float mix) lerpVector =
{
	if (mix <= 0) return a;
	if (mix >= 1) return b;
	return (b * mix + a * ( 1 - mix ) );
}

// for a relaxing lerp: hermite lerp.
float(float a, float b, float mix) lerpHermite =
{
	if (mix <= 0) return a;
	if (mix >= 1) return b;
	
	local float h01;
	
	h01 = mix * mix;
	h01 *= 3 - 2 * mix;
	
	return (b * h01 + a * ( 1 - h01 ) );
}

vector(vector a, vector b, float mix) lerpVectorHermite =
{
	if (mix <= 0) return a;
	if (mix >= 1) return b;

	local float h01;
	
	h01 = mix * mix;
	h01 *= 3 - 2 * mix;
	
	return (b * h01 + a * ( 1 - h01 ) );
}

// even more smooth:
float(float a, float b, float mix) lerpCosine =
{
	if (mix <= 0) return a;
	if (mix >= 1) return b;
	
	local float cmx;
	cmx = (1 - cos(mix * 180)) / 2;
	
	return (b * cmx + a * ( 1 - cmx ) );
}

vector(vector a, vector b, float mix) lerpVectorCosine =
{
	if (mix <= 0) return a;
	if (mix >= 1) return b;

	local float cmx;
	cmx = (1 - cos(mix * 180)) / 2;
	
	return (b * cmx + a * ( 1 - cmx ) );
}


// rotates B around A
vector(vector b, vector a, float r) rotateVectorAround =
{
	float sin, cos, dot;
	vector cross, temp;
	
	cross = CrossProduct(a, b);
	dot = a*b;
	
	temp = SinCos(r);
	sin = temp_x;
	cos = temp_y;
	
	return b*cos + cross*sin + a*dot*(1-cos);
}

float() crandom =
{
	return 2*(random() - 0.5);
}

vector(vector a, vector b) vec_scale =
{
	return Vector(a_x * b_x, a_y * b_y, a_z * b_z);
}

float(float a, float b) min = { if (a <= b) return a; return b; }
float(float a, float b) max = { if (a >= b) return a; return b; }
float(float a, float mn, float mx) clamp = { if (a >= mx) return mx; if (a <= mn) return mn; return a; }
float(float a) saturate = { return clamp(a, 0, 1); }

float(float a, float b) xor = { return (a | b) - (a & b); }
float(float a, float b) not = { return a - (a & b); }

float(float a, float limit) cycle =
{
	a += 1;
	if (a >= limit) return 0;
	return a;
}

// ================================
//	Z-Aware Projectile Maths
// ================================
float(vector dest, float speed, float th) BallisticTheta =
{
	float a, b, c, y, z;
	float theta, tan_theta, oldtheta;

	theta = th;
	
	//calculate how far we are firing
	z = dest_z;
	dest_z = 0;
	y = vlen(dest);

	//find the coefficients of the quadratic in tan(theta)
	a = 0.5 * world.gravity * y * y / (speed * speed);
	b = -y;
	c = a + z;
	
	if(b*b >= 4*a*c)	//check if the destination is too far to reach
	{
		y = 0;
		// newton raphson to within 1 degree, no more than 4x
		while (fabs(oldtheta - theta) > 1 && y < 4)
		{
			oldtheta = theta;
			tan_theta = tan(theta);
			theta = atan2(a*tan_theta*tan_theta - c, 2*a*tan_theta + b);
			y += 1;
		}

		//constrain the values to stop anything too mad happening
		while(theta > 90)
			theta = theta - 180;
	}
	
	return theta;
}


// ================================


float(float base, float exp) ipow =
{
	float x, out;
	
	x = exp - 1;
	out = base;
	
	while (x)
	{
		out *= base;
		x -= 1;
	}
	return out;
}




// the following are by any one of the following people:
// frikac, preach, kashua, lordhavoc, sock

float(float a, float b) mod =
{
	return a - (floor(a / b) * b);
}

//faster version of id's anglemod
float(float v) anglemod =
{
	return v - floor(v/360) * 360;
}

// mathlib_sqrt
float(float num) sqrt =
{
	local float apr;

	if(mathlib_accuracy <= 0)
		mathlib_accuracy = 0.001;   //this sets a level of accuracy, it's a global float

	if (num < mathlib_accuracy)
		return 0;
	if (num>1)
		apr = num;
	else
		apr = 1;
	do
	{
		apr = (num + (apr * apr)) / (2 * apr);
	}
	while (fabs((apr * apr) - num) > (num * mathlib_accuracy));
	return apr;
}




