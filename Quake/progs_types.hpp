#pragma once

#include "q_stdinc.hpp"

#define MAX_PARMS 8

using func_t = int;
using string_t = int;

enum etype_t
{
    ev_bad = -1,
    ev_void = 0,
    ev_string,
    ev_float,
    ev_vector,
    ev_entity,
    ev_field,
    ev_function,
    ev_pointer,

    ev_ext_integer // QSS
};

struct dstatement_t
{
    unsigned short op;
    short a, b, c;
};

struct ddef_t
{
    unsigned short type; // if DEF_SAVEGLOBAL bit is set
                         // the variable needs to be saved in savegames
    unsigned short ofs;
    int s_name;
};

struct dfunction_t
{
    int first_statement; // negative numbers are builtins
    int parm_start;
    int locals; // total ints of parms + locals

    int profile; // runtime

    int s_name;
    int s_file; // source file defined in

    int numparms;
    byte parm_size[MAX_PARMS];
};

struct dprograms_t
{
    int version;
    int crc; // check of header file

    int ofs_statements;
    int numstatements; // statement 0 is an error

    int ofs_globaldefs;
    int numglobaldefs;

    int ofs_fielddefs;
    int numfielddefs;

    int ofs_functions;
    int numfunctions; // function 0 is an empty

    int ofs_strings;
    int numstrings; // first string is a null string

    int ofs_globals;
    int numglobals;

    int entityfields;
};

struct prstack_t
{
    int s;
    dfunction_t* f;
};

using builtin_t = void (*)();


struct pr_extfuncs_s
{ // various global qc entry points that might be called by the engine, if set.
    func_t EndFrame;
    func_t SV_ParseClientCommand;

    // csqc-specific entry points
    func_t CSQC_Init;
    func_t CSQC_DrawHud;    // for the simple hud-only csqc interface.
    func_t CSQC_DrawScores; //(optional) for the simple hud-only csqc interface.
    func_t CSQC_InputEvent;
    func_t CSQC_ConsoleCommand;
    func_t CSQC_Parse_Event;
    func_t CSQC_Parse_Damage;
    // todo...
    //	func_t		CSQC_Parse_CenterPrint;
    //	func_t		CSQC_Parse_Print;

    //	func_t		CSQC_Parse_TempEntity;	//evil... This is the bane of all
    // protocol compatibility. Die.
    //	func_t		CSQC_Parse_StuffCmd; //not in simple. Too easy to make
    // cheats by ignoring server messages.
};

struct pr_extglobals_s
{
    // csqc-specific globals...
    float* cltime;
    float* maxclients;
    float* intermission;
    float* intermission_time;
    float* player_localnum;
    float* player_localentnum;

    // float	*clientcommandframe;	//we don't have prediction.
    // float	*servercommandframe;	//we don't have prediction.
};

struct pr_extfields_s
{ // various fields that might be wanted by the engine. -1 == invalid
    // I should probably use preprocessor magic for this list or something
    int items2;                // float
    int gravity;               // float
    int alpha;                 // float
    int movement;              // vector
    int viewmodelforclient;    // entity
    int exteriormodeltoclient; // entity
    int traileffectnum;        // float
    int emiteffectnum;         // float
    int scale;                 // float
    int colormod;              // vector
    int tag_entity;            // entity
    int tag_index;             // float
    int button3;               // float
    int button4;               // float
    int button5;               // float
    int button6;               // float
    int button7;               // float
    int button8;               // float
    int viewzoom;              // float
    int modelflags;            // float, the upper 8 bits of .effects
    // REMEMBER TO ADD THESE TO qsextensions.qc AND pr_edict.c
};
