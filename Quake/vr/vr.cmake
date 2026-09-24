# vr.cmake -- adds the Quake VR module to the ironwail target (included from the top-level CMakeLists.txt).

file(GLOB QVR_SRC CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_LIST_DIR}/*.cpp"
	"${CMAKE_CURRENT_LIST_DIR}/*.hpp"
	"${CMAKE_CURRENT_LIST_DIR}/*.h")

target_sources(ironwail PRIVATE ${QVR_SRC})
target_include_directories(ironwail PRIVATE
	"${CMAKE_CURRENT_LIST_DIR}/.."
	"${CMAKE_CURRENT_LIST_DIR}"
	"${CMAKE_CURRENT_LIST_DIR}/external")
# OpenXR: the vendored Windows loader, or an installed OpenXR SDK elsewhere.
if (WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(QVR_OPENXR_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Windows/OpenXR")
	target_include_directories(ironwail PRIVATE "${QVR_OPENXR_DIR}/include")
	target_link_libraries(ironwail PRIVATE "${QVR_OPENXR_DIR}/lib/x64/openxr_loader.lib")
	target_compile_definitions(ironwail PRIVATE QVR_HAVE_OPENXR)
	add_custom_command(TARGET ironwail POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different "${QVR_OPENXR_DIR}/lib/x64/openxr_loader.dll" $<TARGET_FILE_DIR:ironwail>)
else()
	find_package(OpenXR CONFIG QUIET)
	if (OpenXR_FOUND)
		target_link_libraries(ironwail PRIVATE OpenXR::openxr_loader)
		target_compile_definitions(ironwail PRIVATE QVR_HAVE_OPENXR)
	endif()
endif()

set_target_properties(ironwail PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED ON
	CXX_EXTENSIONS OFF)
