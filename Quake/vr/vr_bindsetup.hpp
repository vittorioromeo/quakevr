#pragma once

// vr_bindsetup.hpp -- the visual binding setup screen for controllers (the headset's and flat-screen play's gamepads; his
// words 2026-11-28: "a button below called 'setup bindings' that opens up the new visual-based binding setup screen for VR
// and flatscreen controllers, or the flatscreen key bindings page for keyboard + mouse"). The main menu's bindings panel's
// Setup Bindings button calls open() (vr_bindpanel.cpp); keyboard and mouse go to Options > Controls' flat keys instead.

namespace qvr::bindsetup
{

// Opens the controller binding screen for the device in use; false when it did not open (no controller in use, or the
// screen not there yet): the caller opens the keyboard's page instead.
[[nodiscard]] bool open();

} // namespace qvr::bindsetup
