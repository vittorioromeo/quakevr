// QMASTER'S QROBOT.QC
//============================================================
// This is a qoordinate axis robot with programmable points. 
// In other words, you specify the points in space that you 
// want the robot to move to, up to 50 points but not less
// than 2 points. Syncronization with multiple robots is
// achieved through target-targetname system using synctarg#'s
// where # is the current point the robot is on. More than two
// syncing robots is possible with the func_robot_controller.
// (the controller is really a super trigger_counter that handles
// multiple targetnames, those of the robots targetting it)
//
// You can also specify the duration it will take to get to
// the next point, and the wait time at that point.
//
// Points are relative to the robots current position in space.
// This means that point1 must be the (starting position - the
// last position). This might be confusing, but allows you to
// move the robot freely without worrying about having to
// change the points as you would in an absolute coordinate
// system.
//
// Reason for Existance:
// See awesome usage in my map: Subterranean Mine (engine1.bsp)
// via www.quaddicted.com   or celephais.net on Func_msgboard.
// This file is included in that package under the directory \source
// along with a test example map which just so happens to be my
// effects test box for Subterranean Mine
//
// It is my hope that this will be a much more useful func_train
// replacement. This offers the advantage that at sync points
// robots will wait indefinitely until it's mate (which might be
// blocked) arrives at it's corresponding sync point and targets
// the controller that targets both robots. This means that you can
// adjust the wait times at each point and the duration times of
// travel without ever worrying about timing to meet up with another
// robot.  Perfect harmony every time!
//
//
// This program brought to you by the letter Q
// and the company Quake Automation Inc, all rights unreserved 1996 (C) 
// but not really (C).
//
// Legal:  Do whatever you want with this.  Please credit me, Qmaster,
// aka W. Josiah Jack, but you don't have to.
//
//
//
// Parameters:
//---------------------
//.point#    vector for next point in space to move to (point1 thru point50)
//.duration#    float for time it will take to reach the next point in space (duration1 thru duration50)
//.waitpoint#    float for time to wait at the next point in space (waitpoint1 thru waitpoint50)
//.syncpoint#    float (TRUE || FALSE) used to set .syncronize value
//           I could have just used synctarg# but this lets you test with it on or off like real robots
//.poindex    float value for the next point (1-50)
//.syncronize    float (TRUE || FALSE) for whether or not to wait for input from other robot once at point
//.synctarg#    string for robot to sync with, works like target and allows seemless brush travel for infinite loops
//.syncerbot    string for holding the current robot name to sync with
//.curpoint    vector for current point to travel to (not there yet)
//.oldcurpoint    vector for current point (where we are, or where we were before we started moving)
//.homepoint    vector for the starting home point to home it out or determine starting point from initial poindex
//.wait        float for initial wait period before moving after triggered or after map spawn, if you want it to wait
//.syncent#        entity for holding targets for func_robot_controller (up to 20 robots)
//
//============================================================
void() robot_thinkgo;
void() robot_use_turnoff;

void(vector tdest, float ttime, void() func) robot_move= {
    local vector vdestdelta;

    self.think1 = func;
    self.finaldest = tdest;
    self.think = SUB_CalcMoveDone;

    if (tdest == self.origin) {
        self.velocity = VEC_ORIGIN;
        self.think();
        return;
    }
        
    vdestdelta = tdest - self.origin;
    self.nextthink = self.ltime + ttime;
    self.velocity = vdestdelta * (1/ttime); // velocity = d * 1/t = d/t but qcc won't take vector/float
};

vector(float index) robot_getpoint = {
    // I wish I had arrays and switch case statments :(
    self.syncerbot = string_null;

    if (index == 1) {
        if (self.waitpoint1)
            self.wait = self.waitpoint1;

        if (self.duration1)
            self.duration = self.duration1;

        if (self.synctarg1)
            self.syncerbot = self.synctarg1;

        return self.point1;
    }
    if (index == 2) {
        if (self.waitpoint2)
            self.wait = self.waitpoint2;

        if (self.duration2)
            self.duration = self.duration2;

        if (self.synctarg2)
            self.syncerbot = self.synctarg2;

        return self.point2;
    }
    if (index == 3) {
        if (self.waitpoint3)
            self.wait = self.waitpoint3;

        if (self.duration3)
            self.duration = self.duration3;

        if (self.synctarg3)
            self.syncerbot = self.synctarg3;

        return self.point3;
    }
    if (index == 4) {
        if (self.waitpoint4)
            self.wait = self.waitpoint4;

        if (self.duration4)
            self.duration = self.duration4;

        if (self.synctarg4)
            self.syncerbot = self.synctarg4;

        return self.point4;
    }
    if (index == 5) {
        if (self.waitpoint5)
            self.wait = self.waitpoint5;

        if (self.duration5)
            self.duration = self.duration5;

        if (self.synctarg5)
            self.syncerbot = self.synctarg5;

        return self.point5;
    }
    if (index == 6) {
        if (self.waitpoint6)
            self.wait = self.waitpoint6;

        if (self.duration6)
            self.duration = self.duration6;

        if (self.synctarg6)
            self.syncerbot = self.synctarg6;

        return self.point6;
    }
    if (index == 7) {
        if (self.waitpoint7)
            self.wait = self.waitpoint7;

        if (self.duration7)
            self.duration = self.duration7;

        if (self.synctarg7)
            self.syncerbot = self.synctarg7;

        return self.point7;
    }
    if (index == 8) {
        if (self.waitpoint8)
            self.wait = self.waitpoint8;

        if (self.duration8)
            self.duration = self.duration8;

        if (self.synctarg8)
            self.syncerbot = self.synctarg8;

        return self.point8;
    }
    if (index == 9) {
        if (self.waitpoint9)
            self.wait = self.waitpoint9;

        if (self.duration9)
            self.duration = self.duration9;

        if (self.synctarg9)
            self.syncerbot = self.synctarg9;

        return self.point9;
    }
    if (index == 10) {
        if (self.waitpoint10)
            self.wait = self.waitpoint10;

        if (self.duration10)
            self.duration = self.duration10;

        if (self.synctarg10)
            self.syncerbot = self.synctarg10;

        return self.point10;
    }
    if (index == 11) {
        if (self.waitpoint11)
            self.wait = self.waitpoint11;

        if (self.duration11)
            self.duration = self.duration11;

        if (self.synctarg11)
            self.syncerbot = self.synctarg11;

        return self.point11;
    }
    if (index == 12) {
        if (self.waitpoint12)
            self.wait = self.waitpoint12;

        if (self.duration12)
            self.duration = self.duration12;

        if (self.synctarg12)
            self.syncerbot = self.synctarg12;

        return self.point12;
    }
    if (index == 13) {
        if (self.waitpoint13)
            self.wait = self.waitpoint13;

        if (self.duration13)
            self.duration = self.duration13;

        if (self.synctarg13)
            self.syncerbot = self.synctarg13;

        return self.point13;
    }
    if (index == 14) {
        if (self.waitpoint14)
            self.wait = self.waitpoint14;

        if (self.duration14)
            self.duration = self.duration14;

        if (self.synctarg14)
            self.syncerbot = self.synctarg14;

        return self.point14;
    }
    if (index == 15) {
        if (self.waitpoint15)
            self.wait = self.waitpoint15;

        if (self.duration15)
            self.duration = self.duration15;

        if (self.synctarg15)
            self.syncerbot = self.synctarg15;

        return self.point15;
    }
    if (index == 16) {
        if (self.waitpoint16)
            self.wait = self.waitpoint16;

        if (self.duration16)
            self.duration = self.duration16;

        if (self.synctarg16)
            self.syncerbot = self.synctarg16;

        return self.point16;
    }
    if (index == 17) {
        if (self.waitpoint17)
            self.wait = self.waitpoint17;

        if (self.duration17)
            self.duration = self.duration17;

        if (self.synctarg17)
            self.syncerbot = self.synctarg17;

        return self.point17;
    }
    if (index == 18) {
        if (self.waitpoint18)
            self.wait = self.waitpoint18;

        if (self.duration18)
            self.duration = self.duration18;

        if (self.synctarg18)
            self.syncerbot = self.synctarg18;

        return self.point18;
    }
    if (index == 19) {
        if (self.waitpoint19)
            self.wait = self.waitpoint19;

        if (self.duration19)
            self.duration = self.duration19;

        if (self.synctarg19)
            self.syncerbot = self.synctarg19;

        return self.point19;
    }
    if (index == 20) {
        if (self.waitpoint20)
            self.wait = self.waitpoint20;

        if (self.duration20)
            self.duration = self.duration20;

        if (self.synctarg20)
            self.syncerbot = self.synctarg20;

        return self.point20;
    }
    if (index == 21) {
        if (self.waitpoint21)
            self.wait = self.waitpoint21;

        if (self.duration21)
            self.duration = self.duration21;

        if (self.synctarg21)
            self.syncerbot = self.synctarg21;

        return self.point21;
    }
    if (index == 22) {
        if (self.waitpoint22)
            self.wait = self.waitpoint22;

        if (self.duration22)
            self.duration = self.duration22;

        if (self.synctarg22)
            self.syncerbot = self.synctarg22;

        return self.point22;
    }
    if (index == 23) {
        if (self.waitpoint23)
            self.wait = self.waitpoint23;

        if (self.duration23)
            self.duration = self.duration23;

        if (self.synctarg23)
            self.syncerbot = self.synctarg23;

        return self.point23;
    }
    if (index == 24) {
        if (self.waitpoint24)
            self.wait = self.waitpoint24;

        if (self.duration24)
            self.duration = self.duration24;

        if (self.synctarg24)
            self.syncerbot = self.synctarg24;

        return self.point24;
    }
    if (index == 25) {
        if (self.waitpoint25)
            self.wait = self.waitpoint25;

        if (self.duration25)
            self.duration = self.duration25;

        if (self.synctarg25)
            self.syncerbot = self.synctarg25;

        return self.point25;
    }
    if (index == 26) {
        if (self.waitpoint26)
            self.wait = self.waitpoint26;

        if (self.duration26)
            self.duration = self.duration26;

        if (self.synctarg26)
            self.syncerbot = self.synctarg26;

        return self.point26;
    }
    if (index == 27) {
        if (self.waitpoint27)
            self.wait = self.waitpoint27;

        if (self.duration27)
            self.duration = self.duration27;

        if (self.synctarg27)
            self.syncerbot = self.synctarg27;

        return self.point27;
    }
    if (index == 28) {
        if (self.waitpoint28)
            self.wait = self.waitpoint28;

        if (self.duration28)
            self.duration = self.duration28;

        if (self.synctarg28)
            self.syncerbot = self.synctarg28;

        return self.point28;
    }
    if (index == 29) {
        if (self.waitpoint29)
            self.wait = self.waitpoint29;

        if (self.duration29)
            self.duration = self.duration29;

        if (self.synctarg29)
            self.syncerbot = self.synctarg29;

        return self.point29;
    }
    if (index == 30) {
        if (self.waitpoint30)
            self.wait = self.waitpoint30;

        if (self.duration30)
            self.duration = self.duration30;

        if (self.synctarg30)
            self.syncerbot = self.synctarg30;

        return self.point30;
    }
    if (index == 31) {
        if (self.waitpoint31)
            self.wait = self.waitpoint31;

        if (self.duration31)
            self.duration = self.duration31;

        if (self.synctarg31)
            self.syncerbot = self.synctarg31;

        return self.point31;
    }
    if (index == 32) {
        if (self.waitpoint32)
            self.wait = self.waitpoint32;

        if (self.duration32)
            self.duration = self.duration32;

        if (self.synctarg32)
            self.syncerbot = self.synctarg32;

        return self.point32;
    }
    if (index == 33) {
        if (self.waitpoint33)
            self.wait = self.waitpoint33;

        if (self.duration33)
            self.duration = self.duration33;

        if (self.synctarg33)
            self.syncerbot = self.synctarg33;

        return self.point33;
    }
    if (index == 34) {
        if (self.waitpoint34)
            self.wait = self.waitpoint34;

        if (self.duration34)
            self.duration = self.duration34;

        if (self.synctarg34)
            self.syncerbot = self.synctarg34;

        return self.point34;
    }
    if (index == 35) {
        if (self.waitpoint35)
            self.wait = self.waitpoint35;

        if (self.duration35)
            self.duration = self.duration35;

        if (self.synctarg35)
            self.syncerbot = self.synctarg35;

        return self.point35;
    }
    if (index == 36) {
        if (self.waitpoint36)
            self.wait = self.waitpoint36;

        if (self.duration36)
            self.duration = self.duration36;

        if (self.synctarg36)
            self.syncerbot = self.synctarg36;

        return self.point36;
    }
    if (index == 37) {
        if (self.waitpoint37)
            self.wait = self.waitpoint37;

        if (self.duration37)
            self.duration = self.duration37;

        if (self.synctarg37)
            self.syncerbot = self.synctarg37;

        return self.point37;
    }
    if (index == 38) {
        if (self.waitpoint38)
            self.wait = self.waitpoint38;

        if (self.duration38)
            self.duration = self.duration28;

        if (self.synctarg38)
            self.syncerbot = self.synctarg38;

        return self.point38;
    }
    if (index == 39) {
        if (self.waitpoint39)
            self.wait = self.waitpoint39;

        if (self.duration39)
            self.duration = self.duration39;

        if (self.synctarg39)
            self.syncerbot = self.synctarg39;

        return self.point39;
    }
    if (index == 40) {
        if (self.waitpoint40)
            self.wait = self.waitpoint40;

        if (self.duration40)
            self.duration = self.duration40;

        if (self.synctarg40)
            self.syncerbot = self.synctarg40;

        return self.point40;
    }
    if (index == 41) {
        if (self.waitpoint41)
            self.wait = self.waitpoint41;

        if (self.duration41)
            self.duration = self.duration41;

        if (self.synctarg41)
            self.syncerbot = self.synctarg41;

        return self.point41;
    }
    if (index == 42) {
        if (self.waitpoint42)
            self.wait = self.waitpoint42;

        if (self.duration42)
            self.duration = self.duration42;

        if (self.synctarg42)
            self.syncerbot = self.synctarg42;

        return self.point42;
    }
    if (index == 43) {
        if (self.waitpoint43)
            self.wait = self.waitpoint43;

        if (self.duration43)
            self.duration = self.duration43;

        if (self.synctarg43)
            self.syncerbot = self.synctarg43;

        return self.point43;
    }
    if (index == 44) {
        if (self.waitpoint44)
            self.wait = self.waitpoint44;

        if (self.duration44)
            self.duration = self.duration44;

        if (self.synctarg44)
            self.syncerbot = self.synctarg44;

        return self.point44;
    }
    if (index == 45) {
        if (self.waitpoint45)
            self.wait = self.waitpoint45;

        if (self.duration45)
            self.duration = self.duration45;

        if (self.synctarg45)
            self.syncerbot = self.synctarg45;

        return self.point45;
    }
    if (index == 46) {
        if (self.waitpoint46)
            self.wait = self.waitpoint46;

        if (self.duration46)
            self.duration = self.duration46;

        if (self.synctarg46)
            self.syncerbot = self.synctarg46;

        return self.point46;
    }
    if (index == 47) {
        if (self.waitpoint47)
            self.wait = self.waitpoint47;

        if (self.duration47)
            self.duration = self.duration47;

        if (self.synctarg47)
            self.syncerbot = self.synctarg47;

        return self.point47;
    }
    if (index == 48) {
        if (self.waitpoint48)
            self.wait = self.waitpoint48;

        if (self.duration48)
            self.duration = self.duration48;

        if (self.synctarg48)
            self.syncerbot = self.synctarg48;

        return self.point48;
    }
    if (index == 49) {
        if (self.waitpoint49)
            self.wait = self.waitpoint49;

        if (self.duration49)
            self.duration = self.duration49;

        if (self.synctarg49)
            self.syncerbot = self.synctarg49;

        return self.point49;
    }
    if (index == 50) {
        if (self.waitpoint50)
            self.wait = self.waitpoint50;

        if (self.duration50)
            self.duration = self.duration50;

        if (self.synctarg50)
            self.syncerbot = self.synctarg50;

        return self.point50;
    }

    self.wait = FALSE;
    self.duration = FL_SWIM;
    return VEC_ORIGIN;
};

void() robot_use_turnon = {
    self.use = robot_use_turnoff;
    if (self.wait) {
        self.think = robot_thinkgo;
        self.nextthink = (self.ltime + self.wait);
        self.wait = FALSE;
        return;
    }
    robot_thinkgo();
};

void() robot_use_turnoff = {
    self.use = robot_use_turnon;
    self.think = SUB_Null;
    self.nextthink = -1;
};

void() robot_sync_all_targets = {
    local string stemp;

    stemp = self.target;
    self.target = self.syncerbot;
    SUB_UseTargets();
    self.target = stemp;
    self.syncerbot = string_null;
    return;
};

void() robot_wait = {
    self.poindex = (self.poindex + 1);    

    if (self.syncerbot != string_null) {
        robot_sync_all_targets();
        self.use = robot_use_turnon;
        self.think = SUB_Null;
        return;
    }

    if (self.wait != FALSE) {
        if (self.wait == -1) {
            self.wait = FALSE;
            self.use = robot_use_turnon;
            self.think = SUB_Null;
            self.nextthink = (self.ltime + 0.100);
            return;
        }
        self.think = robot_thinkgo;
        self.nextthink = self.ltime + self.wait;
        self.wait = FALSE;
        return;
    }

    robot_thinkgo();    
};

vector(vector pnt) robot_relativecoord_get = {
    // This function steps through the program until we hit
    // the correct starting point then returns it.

    local float indexer;
    local vector vtemp;    

    for (indexer = TRUE; indexer != self.poindex; indexer = (indexer + TRUE)) {
        vtemp = robot_getpoint(indexer+TRUE);
        if (vtemp == VEC_ORIGIN)
        {
            indexer = FALSE; // reset indexer. we reached the end of the program list
            continue;      // goto end of for loop to increment indexer by 1, execute for loop again
        }
        pnt = (pnt + vtemp);
    }

    if (pnt == VEC_ORIGIN)
        pnt = (self.homepoint + robot_getpoint(TRUE));

    return pnt;
};

//==================================
//=====Main Movement Function=======
//==================================
void() robot_thinkgo = {
    self.curpoint = robot_getpoint(self.poindex);
    if (self.curpoint == VEC_ORIGIN) {
        self.poindex = TRUE;
        self.curpoint = robot_getpoint(self.poindex);
    }

    self.curpoint = (self.curpoint + self.origin);    // relative coordinates from robots current location

    if (!self.duration) {
        self.duration = TRUE;  // prevent possible glitches
    }

    self.oldcurpoint = self.curpoint;
    robot_move(self.curpoint, self.duration, robot_wait);
};

void() info_home = {};

//======Our Beloved Coordinate Axis Robot======
//---------------------------------------------
void() func_robot = {
    local entity targ;

    self.solid = SOLID_BSP;
    self.movetype = MOVETYPE_PUSH;
    self.use = robot_use_turnon;
    setmodel (self, self.model);
    setsize (self, self.mins , self.maxs);

    if (!self.poindex || self.poindex > 50)
        self.poindex = TRUE;

    if (self.homeinfo) {
        targ = find (world,targetname,self.homeinfo);
        self.homepoint = targ.origin;
    }
    self.oldcurpoint = robot_relativecoord_get(self.homepoint);
    self.curpoint = self.oldcurpoint;
    setorigin(self, self.oldcurpoint - self.mins);
    self.poindex = (self.poindex + 1);
    if (!self.point1)
        self.point1 = VEC_ORIGIN;

    if (!self.point2)
        self.point2 = '16 16 0';

    if (!self.duration1)
        self.duration1 = 1;

    if (!self.duration2)
        self.duration2 = 1;

    if (!self.targetname) {
        if (self.wait) {
            self.nextthink = self.ltime + self.wait;
            self.wait = FALSE;
        } else {
            self.nextthink = self.ltime + 0.100;
        }

        self.use = robot_use_turnoff;
        self.think = robot_thinkgo;
    }
};
//----------------------------------------------
//==============================================

void() robot_controller_use_targets = {
    local entity stemp;
    local string temptarget;
    local string targname1;
    local string targname2;
    local string targname3;
    local string targname4;
    local string targname5;
    local string targname6;
    local string targname7;
    local string targname8;
    local string targname9;
    local string targname10;
    local string targname11;
    local string targname12;
    local string targname13;
    local string targname14;
    local string targname15;
    local string targname16;
    local string targname17;
    local string targname18;
    local string targname19;
    local string targname20;
    local float tempdelay;

    if (self.syncent1) {
        self.syncent1.use = robot_use_turnon;
        targname1 = self.syncent1.targetname;
        self.syncent1.targetname = "badjoojoomagumbo";
    }

    if (self.syncent2) {
        self.syncent2.use = robot_use_turnon;
        targname2 = self.syncent2.targetname;
        self.syncent2.targetname = "badjoojoomagumbo";
    }

    if (self.syncent3) {
        self.syncent3.use = robot_use_turnon;
        targname3 = self.syncent3.targetname;
        self.syncent3.targetname = "badjoojoomagumbo";
    }

    if (self.syncent4) {
        self.syncent4.use = robot_use_turnon;
        targname4 = self.syncent4.targetname;
        self.syncent4.targetname = "badjoojoomagumbo";
    }

    if (self.syncent5) {
        self.syncent5.use = robot_use_turnon;
        targname5 = self.syncent5.targetname;
        self.syncent5.targetname = "badjoojoomagumbo";
    }

    if (self.syncent6) {
        self.syncent6.use = robot_use_turnon;
        targname6 = self.syncent6.targetname;
        self.syncent6.targetname = "badjoojoomagumbo";
    }

    if (self.syncent7) {
        self.syncent7.use = robot_use_turnon;
        targname7 = self.syncent7.targetname;
        self.syncent7.targetname = "badjoojoomagumbo";
    }

    if (self.syncent8) {
        self.syncent8.use = robot_use_turnon;
        targname8 = self.syncent8.targetname;
        self.syncent8.targetname = "badjoojoomagumbo";
    }

    if (self.syncent9) {
        self.syncent9.use = robot_use_turnon;
        targname9 = self.syncent9.targetname;
        self.syncent9.targetname = "badjoojoomagumbo";
    }

    if (self.syncent10)
    {
        self.syncent10.use = robot_use_turnon;
        targname10 = self.syncent10.targetname;
        self.syncent10.targetname = "badjoojoomagumbo";
    }

    if (self.syncent11)
    {
        self.syncent11.use = robot_use_turnon;
        targname11 = self.syncent11.targetname;
        self.syncent11.targetname = "badjoojoomagumbo";
    }

    if (self.syncent12)
    {
        self.syncent12.use = robot_use_turnon;
        targname12 = self.syncent12.targetname;
        self.syncent12.targetname = "badjoojoomagumbo";
    }

    if (self.syncent13)
    {
        self.syncent13.use = robot_use_turnon;
        targname13 = self.syncent13.targetname;
        self.syncent13.targetname = "badjoojoomagumbo";
    }

    if (self.syncent14)
    {
        self.syncent14.use = robot_use_turnon;
        targname14 = self.syncent14.targetname;
        self.syncent14.targetname = "badjoojoomagumbo";
    }

    if (self.syncent15)
    {
        self.syncent15.use = robot_use_turnon;
        targname15 = self.syncent15.targetname;
        self.syncent15.targetname = "badjoojoomagumbo";
    }

    if (self.syncent16)
    {
        self.syncent16.use = robot_use_turnon;
        targname16 = self.syncent16.targetname;
        self.syncent16.targetname = "badjoojoomagumbo";
    }

    if (self.syncent17)
    {
        self.syncent17.use = robot_use_turnon;
        targname17 = self.syncent17.targetname;
        self.syncent17.targetname = "badjoojoomagumbo";
    }

    if (self.syncent18)
    {
        self.syncent18.use = robot_use_turnon;
        targname18 = self.syncent18.targetname;
        self.syncent18.targetname = "badjoojoomagumbo";
    }

    if (self.syncent19)
    {
        self.syncent19.use = robot_use_turnon;
        targname19 = self.syncent19.targetname;
        self.syncent19.targetname = "badjoojoomagumbo";
    }

    if (self.syncent20)
    {
        self.syncent20.use = robot_use_turnon;
        targname20 = self.syncent20.targetname;
        self.syncent20.targetname = "badjoojoomagumbo";
    }


    // Syncronized start. All together now...GO!!
    temptarget = self.target;
    self.target = "badjoojoomagumbo"; // because, well, why would you use such a name!?!
    tempdelay = self.delay;
    self.delay = FALSE;
    activator = self.enemy;
    SUB_UseTargets();
    self.delay = tempdelay;
    self.target = temptarget;

    if (self.syncent1) {
        stemp = self;
        self = self.syncent1;
        self.targetname = targname1;
        self = stemp;
    }

    if (self.syncent2) {
        stemp = self;
        self = self.syncent2;
        self.targetname = targname2;
        self = stemp;
    }

    if (self.syncent3) {
        stemp = self;
        self = self.syncent3;
        self.targetname = targname3;
        self = stemp;
    }

    if (self.syncent4)
    {
        stemp = self;
        self = self.syncent4;
        self.targetname = targname4;
        self = stemp;
    }

    if (self.syncent5)
    {
        stemp = self;
        self = self.syncent5;
        self.targetname = targname5;
        self = stemp;
    }

    if (self.syncent6)
    {
        stemp = self;
        self = self.syncent6;
        self.targetname = targname6;
        self = stemp;
    }

    if (self.syncent7)
    {
        stemp = self;
        self = self.syncent7;
        self.targetname = targname7;
        self = stemp;
    }

    if (self.syncent8)
    {
        stemp = self;
        self = self.syncent8;
        self.targetname = targname8;
        self = stemp;
    }

    if (self.syncent9)
    {
        stemp = self;
        self = self.syncent9;
        self.targetname = targname9;
        self = stemp;
    }

    if (self.syncent10)
    {
        stemp = self;
        self = self.syncent10;
        self.targetname = targname10;
        self = stemp;
    }

    if (self.syncent11)
    {
        stemp = self;
        self = self.syncent11;
        self.targetname = targname11;
        self = stemp;
    }

    if (self.syncent12)
    {
        stemp = self;
        self = self.syncent12;
        self.targetname = targname12;
        self = stemp;
    }

    if (self.syncent13)
    {
        stemp = self;
        self = self.syncent13;
        self.targetname = targname13;
        self = stemp;
    }

    if (self.syncent14) {
        stemp = self;
        self = self.syncent14;
        self.targetname = targname14;
        self = stemp;
    }

    if (self.syncent15) {
        stemp = self;
        self = self.syncent15;
        self.targetname = targname15;
        self = stemp;
    }

    if (self.syncent16) {
        stemp = self;
        self = self.syncent16;
        self.targetname = targname16;
        self = stemp;
    }

    if (self.syncent17) {
        stemp = self;
        self = self.syncent17;
        self.targetname = targname17;
        self = stemp;
    }

    if (self.syncent18) {
        stemp = self;
        self = self.syncent18;
        self.targetname = targname18;
        self = stemp;
    }

    if (self.syncent19) {
        stemp = self;
        self = self.syncent19;
        self.targetname = targname19;
        self = stemp;
    }

    if (self.syncent20) {
        stemp = self;
        self = self.syncent20;
        self.targetname = targname20;
        self = stemp;
    }

    self.count = self.oldcount;
};

void() robot_controller_use = {
    self.count = (self.count - TRUE);

    if ((self.oldcount - self.count) == 1)
        self.syncent1 = other;

    if ((self.oldcount - self.count) == 2)
        self.syncent2 = other;

    if ((self.oldcount - self.count) == 3)
        self.syncent3 = other;

    if ((self.oldcount - self.count) == 4)
        self.syncent4 = other;

    if ((self.oldcount - self.count) == 5)
        self.syncent5 = other;

    if ((self.oldcount - self.count) == 6)
        self.syncent6 = other;

    if ((self.oldcount - self.count) == 7)
        self.syncent7 = other;

    if ((self.oldcount - self.count) == 8)
        self.syncent8 = other;

    if ((self.oldcount - self.count) == 9)
        self.syncent9 = other;

    if ((self.oldcount - self.count) == 10)
        self.syncent10 = other;

    if ((self.oldcount - self.count) == 11)
        self.syncent11 = other;

    if ((self.oldcount - self.count) == 12)
        self.syncent12 = other;

    if ((self.oldcount - self.count) == 13)
        self.syncent13 = other;

    if ((self.oldcount - self.count) == 14)
        self.syncent14 = other;

    if ((self.oldcount - self.count) == 15)
        self.syncent15 = other;

    if ((self.oldcount - self.count) == 16)
        self.syncent16 = other;

    if ((self.oldcount - self.count) == 17)
        self.syncent17 = other;

    if ((self.oldcount - self.count) == 18)
        self.syncent18 = other;

    if ((self.oldcount - self.count) == 19)
        self.syncent19 = other;

    if ((self.oldcount - self.count) == 20)
        self.syncent20 = other;

    if (self.count)
        return;

    if (self.count < FALSE)
        return;

    self.enemy = activator;

    if (!self.count) {
        self.think = robot_controller_use_targets;
        self.nextthink = time + self.delay;
    }    
};

void() func_robot_controller = {
    if (!self.targetname) return;

    if (!self.count) self.count = 2;
    if (!self.delay) self.delay = 0.100;
    if (!self.wait) self.wait = 0.100; // Time to reset controller before it accepts input
    self.oldcount = self.count;
    self.use = robot_controller_use;
};