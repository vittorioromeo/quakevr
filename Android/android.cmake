# android.cmake -- the Android (Quest standalone) build's settings, included by the top-level CMakeLists.txt when the
# NDK's toolchain is in use (ANDROID). docs/vr-port/QUEST_PLAN.md, C3; Android/README.md says how to build.
#
# The engine is a shared library (libquakevr.so) that SDL2's Java activity loads and calls (SDL_main); SDL2 is built
# from its source (QVR_SDL2_DIR: SDL2's source folder, 2.30.x) into libSDL2.so. OpenGL ES 3.2 through EGL (SDL's
# context), the log through logcat (liblog), WAV and Ogg Vorbis music (stb_vorbis: CMakeLists.txt, vr.cmake), no
# curl; OpenXR through Khronos' Android loader (C4a, QVR_OPENXR_ANDROID_DIR).

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
set(QVR_OPENXR OFF CACHE BOOL "" FORCE) # (vr.cmake's Linux loader and GLX: not these; OpenXR below)

# OpenXR (C4): Khronos' loader for Android (its AAR, unpacked: jni/arm64-v8a/libopenxr_loader.so, which the APK
# carries), the engine's own OpenXR headers (Windows/OpenXR/include, newer than the loader's: the same API); the ES
# binding and the loader's Android init (vr_backend_openxr*.cpp, __ANDROID__). Without the folder: the mock backend.
set(QVR_OPENXR_ANDROID_DIR "$ENV{QVR_OPENXR_ANDROID_DIR}" CACHE PATH
	"Khronos' OpenXR loader for Android, its AAR unpacked (jni/arm64-v8a/libopenxr_loader.so); empty: no OpenXR")
set(QVR_OPENXR_ANDROID_LOADER "${QVR_OPENXR_ANDROID_DIR}/jni/arm64-v8a/libopenxr_loader.so")
set(QVR_OPENXR_INCLUDE "${CMAKE_CURRENT_LIST_DIR}/../Windows/OpenXR/include") # (here: the macro's is the caller's)

# ironwail's target is a shared library here (the top-level CMakeLists.txt); these are its Android settings.
macro(qvr_android_target)
	set_target_properties(ironwail PROPERTIES OUTPUT_NAME quakevr)
	target_link_libraries(ironwail PRIVATE GLESv3 EGL android log)
	# A missing symbol fails the link, not the app's start (dlopen).
	target_link_options(ironwail PRIVATE "-Wl,--no-undefined")
	# The menus' giant file at -Os (C5g, QUEST_PLAN.md 6zr): the NDK's clang 18 took ~18 minutes on it at -O3 and more
	# than 10 at -O1, 17 seconds at -Os; menu code, nothing a frame waits on.
	set_source_files_properties("${CMAKE_SOURCE_DIR}/Quake/vr/vr_menu.cpp" PROPERTIES COMPILE_OPTIONS "-Os")
	if (QVR_OPENXR_ANDROID_DIR AND EXISTS "${QVR_OPENXR_ANDROID_LOADER}")
		add_library(qvr_openxr_loader SHARED IMPORTED)
		set_target_properties(qvr_openxr_loader PROPERTIES IMPORTED_LOCATION "${QVR_OPENXR_ANDROID_LOADER}")
		target_link_libraries(ironwail PRIVATE qvr_openxr_loader)
		target_include_directories(ironwail PRIVATE "${QVR_OPENXR_INCLUDE}")
		target_compile_definitions(ironwail PRIVATE QVR_HAVE_OPENXR)
		# Beside libquakevr.so, for Android/package_apk.sh.
		add_custom_command(TARGET ironwail POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QVR_OPENXR_ANDROID_LOADER}" $<TARGET_FILE_DIR:ironwail>)
		message(STATUS "Quake VR (Android): OpenXR backend on (${QVR_OPENXR_ANDROID_LOADER})")
	else()
		message(STATUS "Quake VR (Android): OpenXR backend off (QVR_OPENXR_ANDROID_DIR: no jni/arm64-v8a/libopenxr_loader.so): the mock backend alone")
	endif()
endmacro()
