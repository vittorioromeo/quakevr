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
# OpenXR: the vendored Windows loader. The backend's graphics binding is OpenGL on Windows (WGL)
# only, so elsewhere the build has the mock backend alone until it gets a GLX/EGL binding.
if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(QVR_OPENXR_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Windows/OpenXR")
	target_include_directories(ironwail PRIVATE "${QVR_OPENXR_DIR}/include")
	target_link_libraries(ironwail PRIVATE "${QVR_OPENXR_DIR}/lib/x64/openxr_loader.lib")
	target_compile_definitions(ironwail PRIVATE QVR_HAVE_OPENXR)
	add_custom_command(TARGET ironwail POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QVR_OPENXR_DIR}/lib/x64/openxr_loader.dll" $<TARGET_FILE_DIR:ironwail>)
endif()

set_target_properties(ironwail PROPERTIES
	CXX_STANDARD 23
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF)
# clang-cl: C++23 itself (CMake asks for it with /std:c++latest, C++26 in clang-cl), on the C++ files alone (Visual
# Studio's generators give a target's $<COMPILE_LANGUAGE:CXX> options to its C files too).
if (CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	set(QVR_CPP_SRC ${QVR_SRC})
	list(FILTER QVR_CPP_SRC INCLUDE REGEX "\\.cpp$")
	set_source_files_properties(${QVR_CPP_SRC} PROPERTIES COMPILE_OPTIONS "/clang:-std=c++23")
endif()

# Box3D (external/box3d/README.md): the rigid-body physics library (vr_box3d.cpp), C17, single-threaded.
# No FMA contraction on gcc and clang: Box3D's cross-platform determinism relies on it.
file(GLOB QVR_BOX3D_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/external/box3d/src/*.c")
add_library(qvr_box3d STATIC ${QVR_BOX3D_SRC})
target_include_directories(qvr_box3d PUBLIC "${CMAKE_CURRENT_LIST_DIR}/external/box3d/include")
set_target_properties(qvr_box3d PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED ON C_EXTENSIONS ON)
if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
	target_compile_options(qvr_box3d PRIVATE /clang:-ffp-contract=off)
elseif (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	target_compile_options(qvr_box3d PRIVATE -ffp-contract=off)
endif()
if (UNIX AND NOT APPLE)
	target_link_libraries(qvr_box3d PUBLIC m)
endif()
target_link_libraries(ironwail PRIVATE qvr_box3d)

# Zancle (external/zancle/README.md): its concurrency module, a static library with its own settings: C++23, optimised
# and without Zancle's asserts (NDEBUG) in every configuration. (Zancle's asserts in the engine's own files, where NDEBUG
# isn't defined, call vr_zancle.cpp's handler.)
file(GLOB_RECURSE QVR_ZANCLE_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/external/zancle/src/*.cpp")
add_library(qvr_zancle STATIC ${QVR_ZANCLE_SRC})
target_include_directories(qvr_zancle PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/external/zancle/include"
	"${CMAKE_CURRENT_LIST_DIR}/external/zancle/src"
	"${CMAKE_CURRENT_LIST_DIR}/external/zancle/extlibs/moodycamel")
target_compile_definitions(qvr_zancle PRIVATE NDEBUG ZA_STATIC)
set_target_properties(qvr_zancle PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
if (MSVC)
	target_compile_options(qvr_zancle PRIVATE /O2 /Ob2 /RTC- "/clang:-std=c++23")
else()
	target_compile_options(qvr_zancle PRIVATE -O2)
endif()
find_package(Threads REQUIRED)
target_link_libraries(qvr_zancle PUBLIC Threads::Threads)
if (WIN32)
	target_link_libraries(qvr_zancle PUBLIC synchronization winmm)
endif()
target_link_libraries(ironwail PRIVATE qvr_zancle)
