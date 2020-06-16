#include "progs.hpp"
#include "areanode.hpp"
#include "server.hpp"

fake_qcvm_proxy qcvm;

[[nodiscard]] fake_qcvm& getFakeQcvm() noexcept
{
    static char* pr_strings;
    static int pr_stringssize;
    static const char** pr_knownstrings;
    static int pr_maxknownstrings;
    static int pr_numknownstrings;
    static ddef_t* pr_fielddefs;
    static ddef_t* pr_globaldefs;
    static bool pr_trace;
    static dfunction_t* pr_xfunction;
    static int pr_xstatement;
    static int pr_argc;

    static double svtime;
    static qmodel_t* svworldmodel;

    static int svnum_edicts;
    static int svmax_edicts;
    static edict_t* svedicts; // can NOT be array indexed, because
                            // edict_t is variable sized, but can
                            // be used to reference the world ent

    static fake_qcvm res{svtime, svnum_edicts, progs, pr_edict_size,
        sv_areanodes, sv_numareanodes, svmax_edicts, svworldmodel, svedicts,
        pr_globals, pr_argc, pr_xfunction, pr_trace, pr_functions,
        pr_statements, pr_builtins, pr_numbuiltins, pr_strings, pr_stringssize,
        pr_knownstrings, pr_maxknownstrings, pr_numknownstrings, pr_fielddefs,
        pr_globaldefs};

    return res;
}
