// vr_steamaudio.hpp -- Valve's Steam Audio (Quake/vr/external/steamaudio/README.md), loaded at run time: phonon.dll
// (libphonon.so) next to the executable, found on first use. Without it (a dedicated server, a build without the
// DLL, an incompatible version) api() is null and the spatial audio (vr_audio.cpp) leaves Quake's mixer as it was.
//
// Every function the game calls is in the table under its own name (api()->iplContextCreate(...)), typed from the
// SDK's header: nothing links against phonon.lib.
#pragma once

#include "steamaudio/include/phonon.h"

namespace qvr::steamaudio
{

#define QVR_IPL_FUNCTIONS(X)                                                                                            \
    X(iplContextCreate)                                                                                                 \
    X(iplContextRelease)                                                                                                \
    X(iplHRTFCreate)                                                                                                    \
    X(iplHRTFRelease)                                                                                                   \
    X(iplBinauralEffectCreate)                                                                                          \
    X(iplBinauralEffectRelease)                                                                                         \
    X(iplBinauralEffectReset)                                                                                           \
    X(iplBinauralEffectApply)                                                                                           \
    X(iplDirectEffectCreate)                                                                                            \
    X(iplDirectEffectRelease)                                                                                           \
    X(iplDirectEffectReset)                                                                                             \
    X(iplDirectEffectApply)                                                                                             \
    X(iplReflectionEffectCreate)                                                                                        \
    X(iplReflectionEffectRelease)                                                                                       \
    X(iplReflectionEffectReset)                                                                                         \
    X(iplReflectionEffectApply)                                                                                         \
    X(iplAmbisonicsDecodeEffectCreate)                                                                                  \
    X(iplAmbisonicsDecodeEffectRelease)                                                                                 \
    X(iplAmbisonicsDecodeEffectReset)                                                                                   \
    X(iplAmbisonicsDecodeEffectApply)                                                                                   \
    X(iplSceneCreate)                                                                                                   \
    X(iplSceneRelease)                                                                                                  \
    X(iplSceneCommit)                                                                                                   \
    X(iplSceneSaveOBJ)                                                                                                  \
    X(iplStaticMeshCreate)                                                                                              \
    X(iplStaticMeshRelease)                                                                                             \
    X(iplStaticMeshAdd)                                                                                                 \
    X(iplInstancedMeshCreate)                                                                                           \
    X(iplInstancedMeshRelease)                                                                                          \
    X(iplInstancedMeshAdd)                                                                                              \
    X(iplInstancedMeshRemove)                                                                                           \
    X(iplInstancedMeshUpdateTransform)                                                                                  \
    X(iplSimulatorCreate)                                                                                               \
    X(iplSimulatorRelease)                                                                                              \
    X(iplSimulatorSetScene)                                                                                             \
    X(iplSimulatorCommit)                                                                                               \
    X(iplSimulatorSetSharedInputs)                                                                                      \
    X(iplSimulatorRunDirect)                                                                                            \
    X(iplSimulatorRunReflections)                                                                                       \
    X(iplSourceCreate)                                                                                                  \
    X(iplSourceRelease)                                                                                                 \
    X(iplSourceAdd)                                                                                                     \
    X(iplSourceRemove)                                                                                                  \
    X(iplSourceSetInputs)                                                                                               \
    X(iplSourceGetOutputs)

struct Api
{
#define QVR_IPL_MEMBER(name) decltype(&::name) name{nullptr};
    QVR_IPL_FUNCTIONS(QVR_IPL_MEMBER)
#undef QVR_IPL_MEMBER
};

// The library and its context (made once, on the first call; the main thread's). Null if it is missing or refused.
[[nodiscard]] const Api* api();
[[nodiscard]] IPLContext context();

// What happened when it was looked for (vr_snd_info): "not tried", "loaded 4.8.1", "phonon.dll not found"...
[[nodiscard]] const char* status();

// Messages Steam Audio logged (from any thread), printed on the main thread (vr_audio.cpp's frame).
void flushLog();

// VR_Shutdown: the context released, the library unloaded (after every object made from it was released).
void shutdown();

} // namespace qvr::steamaudio
