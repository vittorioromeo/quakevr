#include "qcvm.hpp"
#include "sys.hpp"
#include "progs.hpp"

#include <cassert>

qcvm_t* qcvm;

void PR_SwitchQCVM(qcvm_t* nvm)
{
    if(qcvm != nullptr && nvm != nullptr)
    {
        Sys_Error("PR_SwitchQCVM: A qcvm was already active");
    }

    qcvm = nvm;

    if(qcvm != nullptr)
    {
        pr_global_struct = (globalvars_t*)qcvm->globals;
    }
    else
    {
        pr_global_struct = nullptr;
    }
}
