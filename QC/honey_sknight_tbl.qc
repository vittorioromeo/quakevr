


//This is a terrible hack
//See sknight_debug.qc: D_SK_printspears();

void() sknight_spearpos_init =
{
    //pos1 = offset from origin
    //pos2 = angles

    if(self.cnt == 0){
        self.pos1 = '-19.9 -17.3  34.3';
        self.pos2 = '-42.2  33.8   0.0';
        return;

    }else if(self.cnt == 1){
        self.pos1 = '-18.7   5.1  34.3';
        self.pos2 = '-54.8 -12.7   0.0';
        return;

    }else if(self.cnt == 2){
        self.pos1 = '-3.9   9.6  12.8';
        self.pos2 = '-57.7 -39.4   0.0';
        return;

    }else if(self.cnt == 3){
        self.pos1 = '-7.3   2.0  15.0';
        self.pos2 = '-54.8  -7.0   0.0';
        return;

    }else if(self.cnt == 4){
        self.pos1 = '-10.1  -9.8  13.3';
        self.pos2 = '-40.8  29.5   0.0';
        return;

    }else if(self.cnt == 5){
        self.pos1 = '-12.6  16.0  17.8';
        self.pos2 = '-40.8 -42.2   0.0';
        return;

    }else if(self.cnt == 6){
        self.pos1 = '-15.3   3.0  25.8';
        self.pos2 = '-64.7 -11.3   0.0';
        return;

    }else if(self.cnt == 7){
        self.pos1 = '-3.2   5.2  11.3';
        self.pos2 = '-56.3 -52.0   0.0';
        return;

    }else if(self.cnt == 8){
        self.pos1 = '-1.4  15.3  14.0';
        self.pos2 = '-60.5 -50.6   0.0';
        return;

    }else if(self.cnt == 9){
        self.pos1 = '-6.1   6.0   6.6';
        self.pos2 = '-57.7 -52.0   0.0';
        return;

    }else if(self.cnt == 10){
        self.pos1 = '-11.0  14.1  21.6';
        self.pos2 = '-54.8 -39.4   0.0';
        return;

    }else if(self.cnt == 11){
        self.pos1 = '-17.2   8.0  27.5';
        self.pos2 = '-47.8 -15.5   0.0';
        return;

    }else if(self.cnt == 12){
        self.pos1 = '-16.1  -5.6  20.9';
        self.pos2 = '-47.8  12.7   0.0';
        return;

    }else if(self.cnt == 13){
        self.pos1 = '-25.7  -8.3  36.7';
        self.pos2 = '-42.2  12.7   0.0';
        return;

    }else if(self.cnt == 14){
        self.pos1 = '-14.5  21.2  31.4';
        self.pos2 = '-45.0 -39.4   0.0';
        return;

    }else if(self.cnt == 15){
        self.pos1 = '-9.9  13.7  27.7';
        self.pos2 = '-40.8 -46.4   0.0';
        return;

    }else if(self.cnt == 16){
        self.pos1 = '-8.4  15.9  20.3';
        self.pos2 = '-46.4 -57.7   0.0';
        return;

    }else if(self.cnt == 17){
        self.pos1 = '-19.5  -6.1  27.1';
        self.pos2 = '-40.8  18.3   0.0';
        return;

    }
    
    //Default:
    else{
        dprint("WARNING: Unaccounted for spear: #");
        dprint(ftos(self.cnt));
        dprint(".\n");
        
        self.pos1 = '0 0 0';
        self.pos2 = '0 0 0';
        return;
    }
}



