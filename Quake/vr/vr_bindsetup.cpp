// vr_bindsetup.cpp -- see vr_bindsetup.hpp.

#include "vr_bindsetup.hpp"

namespace qvr::bindsetup
{

bool open()
{
    // TODO(ctrlbind): the visual controller-rebinding screen (agent/ctrlbind) opens here for a controller in use (VR
    // controllers, a flat-screen gamepad) and returns true. Until it lands: false, and the Setup Bindings button opens
    // Options > Controls' flat keys (keyboard and mouse) as for a keyboard.
    return false;
}

} // namespace qvr::bindsetup
