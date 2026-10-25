// vr_pch.cpp -- creates the VR module's precompiled header (vr_pch.hpp) in the Visual Studio build (quakevr.props: /Yc
// here, /Yu and /FI in every other Quake/vr .cpp). Nothing else: elsewhere (CMake, or MSBuild -p:QvrNoPch=1) an empty file.

#include "vr_pch.hpp"
