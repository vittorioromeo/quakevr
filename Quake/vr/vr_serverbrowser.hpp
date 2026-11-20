// vr_serverbrowser.hpp -- the server browser (vr_serverbrowser.cpp; its C side: vr_api.h's VR_Browser*).

#pragma once

namespace qvr::browser
{

void init();                         // vr_mp_browse, vr_mp_favourite
void noteConnected(const char* host); // a server joined: first in the recent list (VR_MpNoteConnect)

} // namespace qvr::browser
