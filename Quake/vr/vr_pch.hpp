#pragma once

// vr_pch.hpp -- the VR module's precompiled header: the libraries and the engine headers nearly every Quake/vr file
// includes, parsed once per build instead of in each of them (most of a file's parsing was these: glm and the
// standard headers under it, the Windows SDK's intrin.h under <cwchar>, quakedef.h). Windows: quakevr.props (created by
// vr_pch.cpp, forced into every Quake/vr .cpp); Linux: vr.cmake (target_precompile_headers).
//
// Forced in, it would hide a missing include: every file still includes what it uses, and the build without it checks
// that (Misc/quakevr/check_nopch.sh: MSBuild -p:QvrNoPch=1, CMake -DQVR_PCH=OFF).
//
// Only headers that rarely change belong here: a change to any of them rebuilds every VR file (the module's own
// headers, vr_cvars.hpp and its list among them, stay out). In the order vr_engine.hpp needs: the C++ libraries before
// the engine's function-like macros.

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Zancle/Algorithm/Sort.hpp"
#include "Zancle/Base/IntTypes.hpp"
#include "Zancle/Base/SizeT.hpp"
#include "Zancle/Base/Swap.hpp"
#include "Zancle/Container/AnkerlUnorderedDense.hpp"
#include "Zancle/Container/Array.hpp"
#include "Zancle/Container/InPlaceVector.hpp"
#include "Zancle/Container/SmallVector.hpp"
#include "Zancle/Container/Vector.hpp"
#include "Zancle/Math/Atan2.hpp"
#include "Zancle/Math/Clamp.hpp"
#include "Zancle/Math/MinMax.hpp"
#include "Zancle/String/String.hpp"
#include "Zancle/String/StringView.hpp"
#include "Zancle/Vocabulary/FunctionRef.hpp"
#include "Zancle/Vocabulary/UniquePtr.hpp"

#include "vr_engine.hpp"
