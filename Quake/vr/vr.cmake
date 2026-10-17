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
# OpenXR: WGL on Windows, GLX on Linux; both retain the existing OpenGL renderer.
if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(QVR_OPENXR_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Windows/OpenXR")
	target_include_directories(ironwail PRIVATE "${QVR_OPENXR_DIR}/include")
	target_link_libraries(ironwail PRIVATE "${QVR_OPENXR_DIR}/lib/x64/openxr_loader.lib")
	target_compile_definitions(ironwail PRIVATE QVR_HAVE_OPENXR)
	add_custom_command(TARGET ironwail POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QVR_OPENXR_DIR}/lib/x64/openxr_loader.dll" $<TARGET_FILE_DIR:ironwail>)
elseif (UNIX AND NOT APPLE)
	find_package(X11 REQUIRED)
	find_path(QVR_OPENXR_INCLUDE_DIR openxr/openxr.h REQUIRED)
	find_library(QVR_OPENXR_LIBRARY NAMES openxr_loader HINTS /opt/steamvr/bin/linuxarm64 REQUIRED)
	target_include_directories(ironwail PRIVATE "${QVR_OPENXR_INCLUDE_DIR}" ${X11_INCLUDE_DIR})
	target_link_libraries(ironwail PRIVATE "${QVR_OPENXR_LIBRARY}" OpenGL::GLX ${X11_LIBRARIES})
	target_compile_definitions(ironwail PRIVATE QVR_HAVE_OPENXR)
endif()

# Experimental OpenGL/Zink -> OpenXR Vulkan2 bridge. Transport and frame
# synthesis are separate opt-ins; compositor-generated frames remain unverified.
option(QVR_ENABLE_FRAME_SYNTHESIS_GL_VULKAN_BRIDGE "Build the experimental Linux GL/Vulkan frame-synthesis bridge" OFF)
if (QVR_ENABLE_FRAME_SYNTHESIS_GL_VULKAN_BRIDGE)
	if (NOT UNIX OR APPLE)
		message(FATAL_ERROR "QVR_ENABLE_FRAME_SYNTHESIS_GL_VULKAN_BRIDGE requires Linux OPAQUE_FD interop")
	endif()
	find_package(Vulkan REQUIRED)
	target_link_libraries(ironwail PRIVATE Vulkan::Vulkan)
	target_compile_definitions(ironwail PRIVATE QVR_HAVE_GL_VULKAN_BRIDGE)
endif()

# Compile the native Vulkan BSP buffer/draw component into the engine. This is
# renderer infrastructure only; OpenXR Vulkan session creation and the live
# Quake stereo dispatch remain separate integration work.
option(QVR_ENABLE_NATIVE_VULKAN_BSP "Compile Quake VR's experimental native Vulkan BSP renderer component" OFF)
if (QVR_ENABLE_NATIVE_VULKAN_BSP)
	find_package(Vulkan REQUIRED)
	target_link_libraries(ironwail PRIVATE Vulkan::Vulkan)
	target_compile_definitions(ironwail PRIVATE QVR_HAVE_VULKAN)
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

# The AO bake and audio kernels preserve the scalar/SSE operation order.
# ARM compilers must not fuse their multiply/add pairs into FMA operations.
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
	set_property(SOURCE "${CMAKE_CURRENT_LIST_DIR}/vr_ao.cpp" "${CMAKE_CURRENT_LIST_DIR}/vr_audio.cpp"
		APPEND PROPERTY COMPILE_OPTIONS -ffp-contract=off)
endif()

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

# Opt-in CPU-only fixtures for backend-independent renderer inputs. Keep these
# out of player builds unless the developer is running CTest.
option(QVR_BUILD_CPU_TESTS "Build Quake VR CPU-only tests" OFF)
if (QVR_BUILD_CPU_TESTS)
	include(CTest)
	# Pure metadata tests need only core declarations, including on platforms
	# without a native OpenXR backend. Prefer the production SDK where present.
	if (QVR_OPENXR_INCLUDE_DIR)
		set(QVR_TEST_OPENXR_INCLUDE_DIR "${QVR_OPENXR_INCLUDE_DIR}")
	else()
		set(QVR_TEST_OPENXR_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Windows/OpenXR/include")
	endif()
	foreach(fixture synthesis_math synthesis_submission)
		add_executable(qvr_${fixture}_test "${CMAKE_CURRENT_LIST_DIR}/../../Misc/quakevr/tests/vr_${fixture}_test.cpp")
		target_include_directories(qvr_${fixture}_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${CMAKE_CURRENT_LIST_DIR}/external")
		target_include_directories(qvr_${fixture}_test PRIVATE "${QVR_TEST_OPENXR_INCLUDE_DIR}")
		set_target_properties(qvr_${fixture}_test PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
		add_test(NAME qvr_${fixture} COMMAND qvr_${fixture}_test)
	endforeach()
	add_executable(qvr_bsp_mesh_test
		"${CMAKE_CURRENT_LIST_DIR}/vr_bsp_mesh.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/../../Misc/quakevr/tests/vr_bsp_mesh_test.cpp")
	target_include_directories(qvr_bsp_mesh_test PRIVATE
		"${CMAKE_CURRENT_LIST_DIR}/.."
		"${CMAKE_CURRENT_LIST_DIR}"
		"${CMAKE_CURRENT_LIST_DIR}/external/zancle/include")
	target_compile_definitions(qvr_bsp_mesh_test PRIVATE ZA_STATIC USE_SDL2)
	set_target_properties(qvr_bsp_mesh_test PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
	target_link_libraries(qvr_bsp_mesh_test PRIVATE qvr_zancle SDL2::SDL2)
	add_test(NAME qvr_bsp_mesh COMMAND qvr_bsp_mesh_test)
	if (QVR_ENABLE_FRAME_SYNTHESIS_GL_VULKAN_BRIDGE)
		add_executable(qvr_gl_vulkan_bridge_test
			"${CMAKE_CURRENT_LIST_DIR}/vr_gl_vulkan_bridge.cpp"
			"${CMAKE_CURRENT_LIST_DIR}/../../Misc/quakevr/tests/vr_gl_vulkan_bridge_test.cpp")
		target_include_directories(qvr_gl_vulkan_bridge_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${QVR_OPENXR_INCLUDE_DIR}")
		target_compile_definitions(qvr_gl_vulkan_bridge_test PRIVATE QVR_HAVE_GL_VULKAN_BRIDGE)
		target_link_libraries(qvr_gl_vulkan_bridge_test PRIVATE Vulkan::Vulkan SDL2::SDL2 OpenGL::GL)
		set_target_properties(qvr_gl_vulkan_bridge_test PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
		add_test(NAME qvr_gl_vulkan_bridge COMMAND qvr_gl_vulkan_bridge_test)
	endif()
endif()

# Standalone, device-dependent OpenXR/Vulkan frame-synthesis fixture. Keep it
# explicitly opt-in and out of CTest: it requires a configured OpenXR runtime
# (and normally a Steam Frame headset) to run meaningfully.
option(QVR_BUILD_OPENXR_VULKAN_FIXTURE "Build the standalone OpenXR/Vulkan frame-synthesis fixture" OFF)
if (QVR_BUILD_OPENXR_VULKAN_FIXTURE)
	find_package(Vulkan REQUIRED)
	find_program(QVR_GLSLC NAMES glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
	if (NOT QVR_GLSLC)
		message(FATAL_ERROR "QVR_BUILD_OPENXR_VULKAN_FIXTURE requires glslc to compile its Vulkan test shaders")
	endif()
	if (WIN32)
		set(QVR_FIXTURE_OPENXR_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Windows/OpenXR/include")
		set(QVR_FIXTURE_OPENXR_LOADER "${CMAKE_CURRENT_LIST_DIR}/../../Windows/OpenXR/lib/x64/openxr_loader.lib")
	else()
		set(QVR_FIXTURE_OPENXR_INCLUDE_DIR "${QVR_OPENXR_INCLUDE_DIR}")
		set(QVR_FIXTURE_OPENXR_LOADER "${QVR_OPENXR_LIBRARY}")
	endif()
	if (NOT EXISTS "${QVR_FIXTURE_OPENXR_INCLUDE_DIR}/openxr/openxr.h")
		message(FATAL_ERROR "QVR_BUILD_OPENXR_VULKAN_FIXTURE requires OpenXR headers; expected openxr/openxr.h under ${QVR_FIXTURE_OPENXR_INCLUDE_DIR}")
	endif()
	if (NOT EXISTS "${QVR_FIXTURE_OPENXR_LOADER}")
		message(FATAL_ERROR "QVR_BUILD_OPENXR_VULKAN_FIXTURE requires the OpenXR loader library; got ${QVR_FIXTURE_OPENXR_LOADER}")
	endif()
	add_executable(qvr_openxr_vulkan_frame_loop
		"${CMAKE_CURRENT_LIST_DIR}/../../Misc/quakevr/tests/vr_openxr_vulkan_frame_loop.cpp"
		"${CMAKE_CURRENT_LIST_DIR}/vr_vulkan_bsp.cpp")
	set(QVR_VULKAN_TEST_SHADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/qvr_vulkan_test_shaders")
	set(QVR_VULKAN_TEST_VERT "${QVR_VULKAN_TEST_SHADER_DIR}/bsp_test.vert.spv")
	set(QVR_VULKAN_TEST_FRAG "${QVR_VULKAN_TEST_SHADER_DIR}/bsp_test.frag.spv")
	add_custom_command(OUTPUT "${QVR_VULKAN_TEST_VERT}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${QVR_VULKAN_TEST_SHADER_DIR}"
		COMMAND "${QVR_GLSLC}" --target-env=vulkan1.0 -o "${QVR_VULKAN_TEST_VERT}"
			"${CMAKE_CURRENT_LIST_DIR}/../../Misc/quakevr/tests/shaders/bsp_test.vert"
		DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../../Misc/quakevr/tests/shaders/bsp_test.vert"
		VERBATIM)
	add_custom_command(OUTPUT "${QVR_VULKAN_TEST_FRAG}"
		COMMAND ${CMAKE_COMMAND} -E make_directory "${QVR_VULKAN_TEST_SHADER_DIR}"
		COMMAND "${QVR_GLSLC}" --target-env=vulkan1.0 -o "${QVR_VULKAN_TEST_FRAG}"
			"${CMAKE_CURRENT_LIST_DIR}/../../Misc/quakevr/tests/shaders/bsp_test.frag"
		DEPENDS "${CMAKE_CURRENT_LIST_DIR}/../../Misc/quakevr/tests/shaders/bsp_test.frag"
		VERBATIM)
	add_custom_target(qvr_openxr_vulkan_test_shaders DEPENDS "${QVR_VULKAN_TEST_VERT}" "${QVR_VULKAN_TEST_FRAG}")
	add_dependencies(qvr_openxr_vulkan_frame_loop qvr_openxr_vulkan_test_shaders)
	target_include_directories(qvr_openxr_vulkan_frame_loop PRIVATE
		"${QVR_FIXTURE_OPENXR_INCLUDE_DIR}"
		"${CMAKE_CURRENT_LIST_DIR}"
		"${CMAKE_CURRENT_LIST_DIR}/external/zancle/include")
	target_compile_definitions(qvr_openxr_vulkan_frame_loop PRIVATE ZA_STATIC QVR_HAVE_VULKAN
		"QVR_VULKAN_TEST_SHADER_DIR=\"${QVR_VULKAN_TEST_SHADER_DIR}\"")
	target_link_libraries(qvr_openxr_vulkan_frame_loop PRIVATE Vulkan::Vulkan SDL2::SDL2 OpenGL::GL
		"${QVR_FIXTURE_OPENXR_LOADER}" qvr_zancle)
	set_target_properties(qvr_openxr_vulkan_frame_loop PROPERTIES
		CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
endif()

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
		set_source_files_properties(${QVR_MIMALLOC_SRC} PROPERTIES COMPILE_DEFINITIONS "MI_MALLOC_OVERRIDE;QVR_CUSTOM_CPP_OPERATORS"
			COMPILE_OPTIONS "-fno-builtin-malloc;-fno-builtin-free;-fno-builtin-calloc;-fno-builtin-realloc")
	endif()
endif()
