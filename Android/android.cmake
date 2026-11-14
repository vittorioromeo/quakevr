# android.cmake -- the Android (Quest standalone) build's settings, included by the top-level CMakeLists.txt when the
# NDK's toolchain is in use (ANDROID). docs/vr-port/QUEST_PLAN.md, C3; Android/README.md says how to build.
#
# The engine is a shared library (libquakevr.so) that SDL2's Java activity loads and calls (SDL_main); SDL2 is built
# from its source (QVR_SDL2_DIR: SDL2's source folder, 2.30.x) into libSDL2.so. OpenGL ES 3.2 through EGL (SDL's
# context), the log through logcat (liblog), no codecs or curl yet, no OpenXR yet (C4: the mock backend alone).

if (NOT ANDROID)
	message(FATAL_ERROR "Android/android.cmake is for the NDK's toolchain (ANDROID)")
endif()
if (NOT CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" AND NOT ANDROID_ABI STREQUAL "arm64-v8a")
	message(WARNING "Quake VR: Android builds are arm64-v8a (the Quest); this one is ${ANDROID_ABI}${CMAKE_ANDROID_ARCH_ABI}")
endif()

# No floating-point contraction (FMA) anywhere: the NDK's clang contracts by default (-ffp-contract=on) and AArch64 has
# FMA, which would change results the PC build gets without it (the AO bake's reference bits, QuakeC physics, Box3D's
# determinism). Every target, SDL2's too (QUEST_PLAN.md section 2.2).
add_compile_options(-ffp-contract=off)

set(QVR_SDL2_DIR "$ENV{QVR_SDL2_DIR}" CACHE PATH "SDL2's source folder (2.30.x): built here as libSDL2.so")
if (NOT EXISTS "${QVR_SDL2_DIR}/CMakeLists.txt")
	message(FATAL_ERROR "Quake VR (Android): QVR_SDL2_DIR must name SDL2's source folder (got '${QVR_SDL2_DIR}')")
endif()
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)
add_subdirectory("${QVR_SDL2_DIR}" sdl2 EXCLUDE_FROM_ALL)

# mimalloc stays off: overriding malloc from a library the app loads late is not what bionic expects (vr.cmake's
# QVR_MIMALLOC; C6 can measure it). No user directories ($HOME): the base folder is the app's external files folder.
set(QVR_MIMALLOC OFF CACHE BOOL "" FORCE)
set(ENABLE_USERDIRS FALSE CACHE STRING "" FORCE)
set(QVR_OPENXR OFF CACHE BOOL "" FORCE)

# ironwail's target is a shared library here (the top-level CMakeLists.txt); these are its Android settings.
macro(qvr_android_target)
	set_target_properties(ironwail PROPERTIES OUTPUT_NAME quakevr)
	target_link_libraries(ironwail PRIVATE GLESv3 EGL android log)
	# A missing symbol fails the link, not the app's start (dlopen).
	target_link_options(ironwail PRIVATE "-Wl,--no-undefined")
endmacro()
