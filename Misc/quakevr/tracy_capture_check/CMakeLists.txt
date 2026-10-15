cmake_minimum_required(VERSION 3.20)
project(qvr_tracy_capture_check LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
# Test-only official matching source/dependencies must already exist. No fetching/installing.
foreach(path TRACY_SOURCE CAPSTONE_SOURCE ZSTD_SOURCE PPQSORT_SOURCE)
  if(NOT IS_DIRECTORY "${${path}}")
    message(FATAL_ERROR "Set ${path} to an existing source directory")
  endif()
endforeach()
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_AARCH64_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_LEGACY_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTEST OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory("${CAPSTONE_SOURCE}" capstone)
add_subdirectory("${ZSTD_SOURCE}/build/cmake" zstd)
set(common "${TRACY_SOURCE}/public/common")
set(server "${TRACY_SOURCE}/server")
add_executable(qvr_tracy_capture_check check.cpp
  "${common}/tracy_lz4.cpp" "${common}/tracy_lz4hc.cpp" "${common}/TracySocket.cpp"
  "${common}/TracyStackFrames.cpp" "${common}/TracySystem.cpp"
  "${server}/TracyBroadcast.cpp" "${server}/TracyMemory.cpp" "${server}/TracyMmap.cpp"
  "${server}/TracyPrint.cpp" "${server}/TracySysUtil.cpp" "${server}/TracyTaskDispatch.cpp"
  "${server}/TracyTextureCompression.cpp" "${server}/TracyThreadCompress.cpp" "${server}/TracyWorker.cpp")
target_include_directories(qvr_tracy_capture_check PRIVATE "${common}" "${server}"
  "${CAPSTONE_SOURCE}/include/capstone" "${ZSTD_SOURCE}/lib" "${PPQSORT_SOURCE}/include")
target_compile_definitions(qvr_tracy_capture_check PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(qvr_tracy_capture_check PRIVATE capstone_static libzstd_static ws2_32 dbghelp)
