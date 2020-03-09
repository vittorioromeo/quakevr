/* Counter QuickC program
   By Jim Dose'  9/13/96
   Copyright (c)1996 Hipnotic Interactive, Inc.
   All rights reserved.
   Do not distribute.
*/

float COUNTER_TOGGLE = 1;
float COUNTER_LOOP = 2;
float COUNTER_STEP = 4;
float COUNTER_RESET = 8;
float COUNTER_RANDOM = 16;
float COUNTER_FINISHCOUNT = 32;
float COUNTER_START_ON = 64;

void() counter_on_use;
void() counter_off_use;

void() counter_think =
	{
	self.cnt = self.cnt + 1;
	if ( self.spawnflags & COUNTER_RANDOM )
		{
		self.state = random() * self.count;
		self.state = floor( self.state ) + 1;
		}
	else
		{
		self.state = self.cnt;
		}

	activator = other;
	SUB_UseTargets();
	self.nextthink = time + self.wait;

	if ( self.spawnflags & COUNTER_STEP )
		{
		counter_on_use();
		}

	if ( self.cnt >= self.count )
		{
		self.cnt = 0;
		if ( ( self.aflag ) || !( self.spawnflags & COUNTER_LOOP ) )
			{
			if (self.spawnflags & COUNTER_TOGGLE)
				{
				counter_on_use();
				}
			else
				{
				remove (self);
				}
			}
		}
	};

void() counter_on_use =
	{
	if ( ( self.cnt != 0 ) && ( self.spawnflags & COUNTER_FINISHCOUNT ) )
		{
		self.aflag = TRUE;
		return;
		}

	self.use = counter_off_use;
	self.think = SUB_Null;
	self.aflag = FALSE;
	};

void() counter_off_use =
	{
	self.aflag = FALSE;
	if (self.spawnflags & COUNTER_TOGGLE)
		{
		self.use = counter_on_use;
		}
	else
		{
		self.use = SUB_Null;
		}

	if ( self.spawnflags & COUNTER_RESET )
		{
		self.cnt = 0;
		self.state = 0;
		}
	self.think = counter_think;
	if (self.delay)
		{
		self.nextthink = time + self.delay;
		}
	else
		{
		counter_think();
		}
	};

float( entity counter ) counter_GetCount =
	{
	local float value;

	if ( counter.classname == "counter" )
		{
		return counter.state;
		}
	return 0;
	};

/*QUAKED func_counter (0 0 0.5) (0 0 0) (32 32 32) TOGGLE LOOP STEP RESET RANDOM FINISHCOUNT START_ON
TOGGLE causes the counter to switch between an on and off state
each time the counter is triggered.

LOOP causes the counter to repeat infinitly.  The count resets to zero
after reaching the value in "count".

STEP causes the counter to only increment when triggered.  Effectively,
this turns the counter into a relay with counting abilities.

RESET causes the counter to reset to 0 when restarted.

RANDOM causes the counter to generate random values in the range 1 to "count"
at the specified interval.

FINISHCOUNT causes the counter to continue counting until it reaches "count"
before shutting down even after being set to an off state.

START_ON causes the counter to be on when the level starts.

"count" specifies how many times to repeat the event.  If LOOP is set,
it specifies how high to count before reseting to zero.  Default is 10.

"wait"  the length of time between each trigger event. Default is 1 second.

"delay" how much time to wait before firing after being switched on.
*/

void() func_counter =
	{
	if ( !self.wait )
	   {
	   self.wait = 1;
	   }

	self.count = floor( self.count );
	if ( self.count <= 0 )
	   {
	   self.count = 10;
	   }
	self.cnt = 0;
	self.state = 0;

	self.classname = "counter";
	self.use = counter_off_use;
	self.think = SUB_Null;
   if ( self.spawnflags & COUNTER_START_ON )
      {
      self.think = counter_off_use;
      self.nextthink = time + 0.1;
      }
	};

void() oncount_use =
	{
	if ( counter_GetCount( other ) == self.count )
		{
		activator = other;
		SUB_UseTargets();
		}
	};

/*QUAKED func_oncount (0 0 0.5) (0 0 0) (16 16 16)
Must be used as the target for func_counter.  When the counter
reaches the value set by count, func_oncount triggers its targets.

"count" specifies the value to trigger on.  Default is 1.

"delay" how much time to wait before firing after being triggered.
*/

void() func_oncount =
	{
	self.count = floor( self.count );
	if ( self.count <= 0 )
	   {
	   self.count = 1;
	   }

	self.classname = "oncount";
	self.use = oncount_use;
	self.think = SUB_Null;
	};
