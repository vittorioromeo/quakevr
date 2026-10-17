// vr_propthink.hpp -- a force-grabbable thing's think in the engine: QC forcegrabbable_think_impl's work (water, dust,
// settling after a throw) as the propthink builtin. See
// vr_propthink.cpp.

#pragma once

namespace qvr::propthink
{

void PF_propthink();      // float(float particles) propthink (QC builtins.qc)

} // namespace qvr::propthink
