cmake_minimum_required(VERSION 3.16)
project(spacewarp_ref CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# SteamVR ships the OpenXR loader (no SONAME); link it and find it at run time via RPATH.
set(XR_LOADER_DIR /opt/steamvr/bin/linuxarm64 CACHE PATH "Directory containing libopenxr_loader.so")
find_program(GLSLC glslc REQUIRED)

set(SHADERS scene.vert scene.frag motion.vert motion.frag fullscreen.vert load.frag sky_motion.frag)
set(SHADER_INCS)
foreach(s ${SHADERS})
  set(out ${CMAKE_CURRENT_BINARY_DIR}/shaders/${s}.inc)
  add_custom_command(
    OUTPUT ${out}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
    COMMAND ${GLSLC} -O --target-env=vulkan1.1 -mfmt=c -o ${out} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${s}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${s} ${CMAKE_CURRENT_SOURCE_DIR}/shaders/common.glsl
    COMMENT "glslc ${s}")
  list(APPEND SHADER_INCS ${out})
endforeach()

add_executable(spacewarp-ref src/main.cpp ${SHADER_INCS})
target_include_directories(spacewarp-ref PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/shaders)
target_compile_options(spacewarp-ref PRIVATE -Wall -Wextra -Wno-missing-field-initializers)
target_link_directories(spacewarp-ref PRIVATE ${XR_LOADER_DIR})
target_link_libraries(spacewarp-ref PRIVATE openxr_loader vulkan pthread)
set_target_properties(spacewarp-ref PROPERTIES
  BUILD_RPATH ${XR_LOADER_DIR}
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
