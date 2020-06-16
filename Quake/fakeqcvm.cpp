#include "progs.hpp"
#include "areanode.hpp"
#include "server.hpp"

fake_qcvm_proxy qcvm;

[[nodiscard]] fake_qcvm& getFakeQcvm() noexcept
{
    static fake_qcvm res{sv.time, sv.num_edicts, progs, pr_edict_size,
        sv_areanodes, sv_numareanodes, sv.max_edicts, sv.worldmodel, sv.edicts,
        pr_globals, pr_argc, pr_xfunction, pr_trace, pr_functions,
        pr_statements, pr_builtins, pr_numbuiltins};

    return res;
}
