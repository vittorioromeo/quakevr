# vr.cmake -- adds the Quake VR module to the ironwail target (included from the top-level CMakeLists.txt).

# The module is C++23 and any of its files may include Zancle (external/zancle/README.md), which is written against the
# GCC/Clang builtins: GCC or Clang, on Windows clang-cl (e.g. Visual Studio's generator with -T ClangCL), not MSVC's cl.
if (MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
	message(FATAL_ERROR "Quake VR builds with clang-cl on Windows, not MSVC's cl (Zancle, Quake/vr/external/zancle, needs it): configure with -T ClangCL (or -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl)")
endif()

file(GLOB QVR_SRC CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_LIST_DIR}/*.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/*.hpp"
	"${CMAKE_CURRENT_LIST_DIR}/*.h")

target_sources(ironwail PRIVATE ${QVR_SRC})
target_include_directories(ironwail PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/.."
	"${CMAKE_CURRENT_LIST_DIR}"
	"${CMAKE_CURRENT_LIST_DIR}/external"
	"${CMAKE_CURRENT_LIST_DIR}/external/zancle/include")
target_compile_definitions(ironwail PRIVATE ZA_STATIC)
# The version (vr_crash.cpp, VR_Version): the repository's VERSION file; a dev build (docs/vr-port/RELEASING.md).
file(STRINGS "${CMAKE_CURRENT_LIST_DIR}/../../VERSION" QVR_VERSION LIMIT_COUNT 1)
string(STRIP "${QVR_VERSION}" QVR_VERSION)
target_compile_definitions(ironwail PRIVATE "QVR_VERSION=\"${QVR_VERSION}\"")
# The build number and date (qvr_buildinfo.cmake: qvr_buildver.h, rewritten only when its text changes, before every build).
set(QVR_GEN_DIR "${CMAKE_BINARY_DIR}/qvr_gen")
add_custom_target(qvr_buildinfo ALL
	COMMAND "${CMAKE_COMMAND}" "-DQVR_REPO=${CMAKE_CURRENT_LIST_DIR}/../.." "-DQVR_OUT=${QVR_GEN_DIR}/qvr_buildver.h"
		-P "${CMAKE_CURRENT_LIST_DIR}/qvr_buildinfo.cmake"
	BYPRODUCTS "${QVR_GEN_DIR}/qvr_buildver.h"
	COMMENT "Quake VR build number"
	VERBATIM)
add_dependencies(ironwail qvr_buildinfo)
target_include_directories(ironwail PRIVATE "${QVR_GEN_DIR}")
# OpenXR: the vendored Windows loader (the WGL binding); on Linux the system's loader and headers (pkg-config openxr:
# Arch's openxr, Debian's libopenxr-dev and libopenxr-loader1) and the GLX (Xlib) binding (docs/vr-port/LINUX.md).
# Without them (or QVR_OPENXR OFF) the Linux build has the mock backend alone.
option(QVR_OPENXR "Linux: the OpenXR backend (needs the OpenXR loader, pkg-config openxr, and X11/GLX)" ON)
if (UNIX AND NOT APPLE AND QVR_OPENXR)
	find_package(PkgConfig)
	if (PKG_CONFIG_FOUND)
		pkg_check_modules(QVR_OPENXR_PC IMPORTED_TARGET openxr)
	endif()
	find_package(X11)
	if (QVR_OPENXR_PC_FOUND AND X11_FOUND)
		target_link_libraries(ironwail PRIVATE PkgConfig::QVR_OPENXR_PC ${X11_LIBRARIES})
		target_include_directories(ironwail PRIVATE ${X11_INCLUDE_DIR})
		target_compile_definitions(ironwail PRIVATE QVR_HAVE_OPENXR)
		if (TARGET OpenGL::GLX)
			target_link_libraries(ironwail PRIVATE OpenGL::GLX)
		endif()
		message(STATUS "Quake VR: OpenXR backend on (loader ${QVR_OPENXR_PC_VERSION}, GLX binding)")
	else()
		message(STATUS "Quake VR: OpenXR backend off (no pkg-config openxr or X11): the mock backend alone")
	endif()
elseif (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(QVR_OPENXR_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Windows/OpenXR")
	target_include_directories(ironwail PRIVATE "${QVR_OPENXR_DIR}/include")
	target_link_libraries(ironwail PRIVATE "${QVR_OPENXR_DIR}/lib/x64/openxr_loader.lib")
	target_compile_definitions(ironwail PRIVATE QVR_HAVE_OPENXR)
	add_custom_command(TARGET ironwail POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QVR_OPENXR_DIR}/lib/x64/openxr_loader.dll" $<TARGET_FILE_DIR:ironwail>)
endif()

# No exceptions (docs/vr-port/CODE_STYLE.md): the module and Zancle are built without them (clang-cl /EHs-c-, after
# CMake's /EHsc: the later wins; elsewhere -fno-exceptions).
set_target_properties(ironwail PROPERTIES
	CXX_STANDARD 23
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF)
# clang-cl: C++23 itself (CMake asks for it with /std:c++latest, C++26 in clang-cl), on the C++ files alone (Visual
# Studio's generators give a target's $<COMPILE_LANGUAGE:CXX> options to its C files too).
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	set(QVR_CPP_SRC ${QVR_SRC})
	list(FILTER QVR_CPP_SRC INCLUDE REGEX "\\.cpp$")
	set_source_files_properties(${QVR_CPP_SRC} PROPERTIES COMPILE_OPTIONS "/clang:-std=c++23;/EHs-c-")
else()
	target_compile_options(ironwail PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>")
endif()
# The precompiled header (vr_pch.hpp: the libraries and engine headers nearly every VR file includes), forced into the
# target's C++ files (the VR module's; the engine is C). -DQVR_PCH=OFF builds without it: a file that uses a header it
# doesn't include fails there (Misc/quakevr/check_nopch.sh).
option(QVR_PCH "Quake VR: the VR module's precompiled header (Quake/vr/vr_pch.hpp)" ON)
if (QVR_PCH AND NOT CMAKE_VERSION VERSION_LESS 3.16)
	target_precompile_headers(ironwail PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_LIST_DIR}/vr_pch.hpp>")
endif()
# The menu (vr_menu.cpp and its page groups, vr_menu_pages_*.cpp): only what is marked inline is inlined (clang-cl /Ob1,
# clang -finline-hint-functions, gcc -fno-inline-functions). Its thousands of page builders and helpers were the
# inliner's work and the build's long pole; it is UI code (quakevr.props, the same).
file(GLOB QVR_MENU_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/vr_menu.cpp" "${CMAKE_CURRENT_LIST_DIR}/vr_menu_pages_*.cpp")
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	set_property(SOURCE ${QVR_MENU_SRC} APPEND PROPERTY COMPILE_OPTIONS "/Ob1")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	set_property(SOURCE ${QVR_MENU_SRC} APPEND PROPERTY COMPILE_OPTIONS "-finline-hint-functions")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set_property(SOURCE ${QVR_MENU_SRC} APPEND PROPERTY COMPILE_OPTIONS "-fno-inline-functions")
endif()
# They don't use the precompiled header: it is built with the usual inlining, and clang refuses a PCH whose inlining
# differs ("Inlining differs in precompiled file").
set_source_files_properties(${QVR_MENU_SRC} PROPERTIES SKIP_PRECOMPILE_HEADERS ON)

# Box3D (external/box3d/README.md): the rigid-body physics library (vr_box3d.cpp), C17, run on the engine's job pool.
# No FMA contraction on gcc and clang: Box3D's cross-platform determinism relies on it.
# wide8.c holds its AVX2 kernels, used only on a CPU that has AVX2 (src/simd.c, run-time detection; wide4.c is the
# SSE2 copy): gcc and clang compile them for AVX2 by a pragma (src/core.h B3_AVX2_BEGIN), MSVC's cl needs /arch:AVX2 on
# that file alone (as upstream's CMake). clang-cl gets intrin.h first: verstable.h includes it inside that pragma, where
# clang refuses its declarations ("attribute 'target' multiversioning").
file(GLOB QVR_BOX3D_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/external/box3d/src/*.c")
add_library(qvr_box3d STATIC ${QVR_BOX3D_SRC})
target_include_directories(qvr_box3d PUBLIC "${CMAKE_CURRENT_LIST_DIR}/external/box3d/include")
set_target_properties(qvr_box3d PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
set(QVR_BOX3D_WIDE8 "${CMAKE_CURRENT_LIST_DIR}/external/box3d/src/wide8.c")
if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC" AND CMAKE_C_COMPILER_ID STREQUAL "Clang")
	target_compile_options(qvr_box3d PRIVATE /clang:-ffp-contract=off)
	set_source_files_properties(${QVR_BOX3D_WIDE8} PROPERTIES COMPILE_OPTIONS "/FIintrin.h")
elseif (MSVC)
	if (CMAKE_C_COMPILER_ARCHITECTURE_ID MATCHES "^(x64|X86)$")
		set_source_files_properties(${QVR_BOX3D_WIDE8} PROPERTIES COMPILE_OPTIONS "/arch:AVX2")
	endif()
elseif (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(qvr_box3d PRIVATE -ffp-contract=off)
endif()
if (UNIX AND NOT APPLE)
	target_link_libraries(qvr_box3d PUBLIC m)
endif()
target_link_libraries(ironwail PRIVATE qvr_box3d)

# Zancle (external/zancle/README.md): the modules the Quake VR code uses, a static library with its own settings: C++23;
# optimised and without Zancle's asserts (NDEBUG) in Release; in Debug by QVR_ZANCLE_DEBUG: ON, built as the engine's
# Debug with its asserts on (a failure calls vr_zancle.cpp's handler, installed by za::setAssertHandler, as in the
# engine's own files); OFF, as in Release (the engine's Debug then defines the assert function the library has not).
option(QVR_ZANCLE_DEBUG "Zancle's library in Debug: unoptimised, with its asserts (OFF: optimised, asserts off)" ON)
file(GLOB_RECURSE QVR_ZANCLE_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/external/zancle/src/*.cpp")
add_library(qvr_zancle STATIC ${QVR_ZANCLE_SRC})
target_include_directories(qvr_zancle PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/external/zancle/include"
	"${CMAKE_CURRENT_LIST_DIR}/external/zancle/src"
	"${CMAKE_CURRENT_LIST_DIR}/external/zancle/extlibs/moodycamel")
set_target_properties(qvr_zancle PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if (QVR_ZANCLE_DEBUG)
	# Debug: the engine's own flags, and the asserts (their handler: vr_zancle.cpp); other configurations optimised.
	# QVR_ZANCLE_DEBUG tells the engine's vr_zancle.cpp that the library has its own assert function.
	set(QVR_ZANCLE_OPTIMISED "$<NOT:$<CONFIG:Debug>>")
	target_compile_definitions(qvr_zancle PRIVATE ZA_STATIC "$<${QVR_ZANCLE_OPTIMISED}:NDEBUG>")
	target_compile_definitions(ironwail PRIVATE "$<$<CONFIG:Debug>:QVR_ZANCLE_DEBUG>")
else()
	set(QVR_ZANCLE_OPTIMISED 1)
	target_compile_definitions(qvr_zancle PRIVATE NDEBUG ZA_STATIC)
endif()
if (MSVC)
	target_compile_options(qvr_zancle PRIVATE "$<${QVR_ZANCLE_OPTIMISED}:/O2;/Ob2;/RTC->" "/clang:-std=c++23" "/EHs-c-")
else()
	target_compile_options(qvr_zancle PRIVATE "$<${QVR_ZANCLE_OPTIMISED}:-O2>" -fno-exceptions)
endif()
find_package(Threads REQUIRED)
target_link_libraries(qvr_zancle PUBLIC Threads::Threads)
if (WIN32)
	target_link_libraries(qvr_zancle PUBLIC synchronization winmm)
endif()
target_link_libraries(ironwail PRIVATE qvr_zancle)

# mimalloc (external/mimalloc/README.md): the process's C heap, malloc/free and new/delete (vr_crtheap.c; new/delete
# call malloc/free, vr_alloccount.cpp). One C file, src/static.c, includes the others; it is compiled into the
# executable itself (an object, not an archive: its definitions must win over the C library's).
# Windows (clang-cl, the DLL C runtime): vr_crtheap.c defines malloc and the rest and their import pointers (__imp_*), on
# 64-bit only. Elsewhere: mimalloc's own override (MI_MALLOC_OVERRIDE: malloc and the rest defined in the executable,
# which the shared libraries' calls bind to as well); not on macOS (its static override does not take there).
# OFF: the C library's heap, untouched; at run time on Windows -nomimalloc or QVR_MIMALLOC=0 does the same.
if (APPLE OR (WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8))
	set(QVR_MIMALLOC_DEFAULT OFF)
else()
	set(QVR_MIMALLOC_DEFAULT ON)
endif()
option(QVR_MIMALLOC "mimalloc as the process's heap (malloc/free, new/delete)" ${QVR_MIMALLOC_DEFAULT})
set(QVR_CRTHEAP_SRC "${CMAKE_CURRENT_LIST_DIR}/vr_crtheap.c" "${CMAKE_CURRENT_LIST_DIR}/vr_crtheap_info.c")
target_sources(ironwail PRIVATE ${QVR_CRTHEAP_SRC})
set_source_files_properties(${QVR_CRTHEAP_SRC} PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/external/mimalloc/include")
if (QVR_MIMALLOC)
	set(QVR_MIMALLOC_SRC "${CMAKE_CURRENT_LIST_DIR}/external/mimalloc/src/static.c")
	target_sources(ironwail PRIVATE ${QVR_MIMALLOC_SRC})
	set_source_files_properties(${QVR_MIMALLOC_SRC} PROPERTIES INCLUDE_DIRECTORIES "${CMAKE_CURRENT_LIST_DIR}/external/mimalloc/include")
	set_source_files_properties(${QVR_CRTHEAP_SRC} PROPERTIES COMPILE_DEFINITIONS QVR_MIMALLOC)
	if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
		set_source_files_properties("${CMAKE_CURRENT_LIST_DIR}/vr_crtheap.c" PROPERTIES COMPILE_OPTIONS "/clang:-fno-builtin")
	else()
		# gcc/clang: mimalloc's override also defines the C++ operators, by their mangled names (alloc-override.c),
		# which collide with vr_alloccount.cpp's (counted, then malloc: mimalloc's all the same): renamed away here.
		set(QVR_MI_CXX_OPS _Znwm _Znam _Znwj _Znaj _ZdlPv _ZdaPv _ZdlPvm _ZdaPvm)
		foreach(op _Znwm _Znam _Znwj _Znaj)
			list(APPEND QVR_MI_CXX_OPS ${op}RKSt9nothrow_t ${op}St11align_val_t ${op}St11align_val_tRKSt9nothrow_t)
		endforeach()
		foreach(op _ZdlPv _ZdaPv)
			list(APPEND QVR_MI_CXX_OPS ${op}RKSt9nothrow_t ${op}St11align_val_t ${op}St11align_val_tRKSt9nothrow_t ${op}mSt11align_val_t)
		endforeach()
		list(TRANSFORM QVR_MI_CXX_OPS REPLACE "^(.+)$" "\\1=qvr_mimalloc_unused\\1")
		set_source_files_properties(${QVR_MIMALLOC_SRC} PROPERTIES COMPILE_DEFINITIONS "MI_MALLOC_OVERRIDE;${QVR_MI_CXX_OPS}"
			COMPILE_OPTIONS "-fno-builtin-malloc;-fno-builtin-free;-fno-builtin-calloc;-fno-builtin-realloc")
	endif()
endif()
