# QuakeC.cmake -- the QuakeC (QC/, into quakevr/progs.dat) as part of the CMake build, outside Windows (included from the
# top-level CMakeLists.txt). On Windows the Visual Studio build compiles it (ironwail.vcxproj's QvrCompileQC target).
#
# The compiler is FTEQCC with the fte32 flag (Tools/fteqcc/README.md): stock FTEQCC writes a 16-bit progs.dat, which
# QC/build.sh's format check fails. By default it is built here from the source pinned in Misc/linux/flatpak/fteqcc.json
# (the Flatpak's fteqcc module: its URL, checksum and patch are read from it, the one place they are written),
# downloaded once into the build folder (needs network on the first build, CMake 3.19, make, patch and a C compiler
# with zlib). -DQVR_FTEQCC=<path> uses a compiler of your own instead (built with Tools/fteqcc/fte32.patch; the
# Flatpak passes the one its module built); -DQVR_QC=OFF leaves progs.dat alone.
#
# The compile is QC/build.sh (the flags and the checks live there), run when a QC source or the compiler changes. Its
# whole output goes to <build>/qc-build.log; the build shows its last lines, or the end of the log when it fails.
# -DQVR_QC_RELEASE=ON compiles the release flavour (-DQVR_NO_TESTS: no QC tests), as the packages ship it.

if (WIN32)
	set(QVR_QC_DEFAULT OFF)
else()
	set(QVR_QC_DEFAULT ON)
endif()
option(QVR_QC "Compile the QuakeC (QC/ into quakevr/progs.dat) with the build" ${QVR_QC_DEFAULT})
option(QVR_QC_RELEASE "The QuakeC's release flavour (-DQVR_NO_TESTS), as the packages ship it" OFF)
set(QVR_FTEQCC "" CACHE FILEPATH "FTEQCC with the fte32 flag (empty: built from Misc/linux/flatpak/fteqcc.json's source)")

if (NOT QVR_QC)
	message(STATUS "Quake VR: QuakeC off (QVR_QC=OFF): quakevr/progs.dat is not built")
	return()
endif()
if (WIN32)
	message(STATUS "Quake VR: QuakeC skipped on Windows (the Visual Studio build or QC\\build.bat compiles it)")
	return()
endif()

get_filename_component(QVR_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

if (QVR_FTEQCC)
	if (NOT EXISTS "${QVR_FTEQCC}")
		message(FATAL_ERROR "Quake VR: QVR_FTEQCC=${QVR_FTEQCC} does not exist")
	endif()
	set(QVR_FTEQCC_BIN "${QVR_FTEQCC}")
	message(STATUS "Quake VR: QuakeC with ${QVR_FTEQCC}")
else()
	if (CMAKE_VERSION VERSION_LESS 3.19)
		message(FATAL_ERROR "Quake VR: building FTEQCC reads Misc/linux/flatpak/fteqcc.json, which needs CMake 3.19 (or pass -DQVR_FTEQCC=<fteqcc with fte32>, or -DQVR_QC=OFF)")
	endif()
	# The pin: the module's first source (the archive) and second (the patch, a path relative to the module's file).
	set(QVR_FTEQCC_PIN "${QVR_ROOT}/Misc/linux/flatpak/fteqcc.json")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${QVR_FTEQCC_PIN}")
	file(READ "${QVR_FTEQCC_PIN}" QVR_FTEQCC_JSON)
	string(JSON QVR_FTEQCC_URL GET "${QVR_FTEQCC_JSON}" sources 0 url)
	string(JSON QVR_FTEQCC_SHA256 GET "${QVR_FTEQCC_JSON}" sources 0 sha256)
	string(JSON QVR_FTEQCC_PATCH_REL GET "${QVR_FTEQCC_JSON}" sources 1 path)
	get_filename_component(QVR_FTEQCC_PATCH "${QVR_ROOT}/Misc/linux/flatpak/${QVR_FTEQCC_PATCH_REL}" ABSOLUTE)

	include(ExternalProject)
	find_program(QVR_MAKE NAMES gmake make)
	find_program(QVR_PATCH NAMES patch)
	if (NOT QVR_MAKE OR NOT QVR_PATCH)
		message(FATAL_ERROR "Quake VR: building FTEQCC needs make and patch (or pass -DQVR_FTEQCC=<fteqcc with fte32>, or -DQVR_QC=OFF)")
	endif()
	set(QVR_FTEQCC_EXTRA "")
	if (NOT CMAKE_VERSION VERSION_LESS 3.24)
		list(APPEND QVR_FTEQCC_EXTRA DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
	endif()
	ExternalProject_Add(qvr_fteqcc
		URL "${QVR_FTEQCC_URL}"
		URL_HASH "SHA256=${QVR_FTEQCC_SHA256}"
		PREFIX "${CMAKE_BINARY_DIR}/fteqcc"
		PATCH_COMMAND "${QVR_PATCH}" -p1 -i "${QVR_FTEQCC_PATCH}"
		CONFIGURE_COMMAND ""
		BUILD_COMMAND "${QVR_MAKE}" -C engine/qclib qcc "CC=${CMAKE_C_COMPILER}"
		BUILD_IN_SOURCE TRUE
		INSTALL_COMMAND ""
		BUILD_BYPRODUCTS "<SOURCE_DIR>/engine/qclib/fteqcc.bin"
		LOG_DOWNLOAD TRUE LOG_BUILD TRUE LOG_OUTPUT_ON_FAILURE TRUE
		${QVR_FTEQCC_EXTRA})
	ExternalProject_Get_Property(qvr_fteqcc SOURCE_DIR)
	set(QVR_FTEQCC_BIN "${SOURCE_DIR}/engine/qclib/fteqcc.bin")
	message(STATUS "Quake VR: QuakeC with FTEQCC built from Misc/linux/flatpak/fteqcc.json's source")
endif()

if (QVR_QC_RELEASE)
	set(QVR_QC_FLAVOUR 1)
	message(STATUS "Quake VR: QuakeC release flavour (QVR_NO_TESTS)")
else()
	set(QVR_QC_FLAVOUR "")
endif()
# The flavour as a file, rewritten only when it changes: switching QVR_QC_RELEASE compiles again.
set(QVR_QC_FLAVOUR_FILE "${CMAKE_BINARY_DIR}/qc-flavour.txt")
if (QVR_QC_FLAVOUR)
	set(QVR_QC_FLAVOUR_TEXT "release")
else()
	set(QVR_QC_FLAVOUR_TEXT "tests")
endif()
if (EXISTS "${QVR_QC_FLAVOUR_FILE}")
	file(READ "${QVR_QC_FLAVOUR_FILE}" QVR_QC_FLAVOUR_OLD)
else()
	set(QVR_QC_FLAVOUR_OLD "")
endif()
if (NOT QVR_QC_FLAVOUR_OLD STREQUAL QVR_QC_FLAVOUR_TEXT)
	file(WRITE "${QVR_QC_FLAVOUR_FILE}" "${QVR_QC_FLAVOUR_TEXT}")
endif()

file(GLOB_RECURSE QVR_QC_SRC CONFIGURE_DEPENDS "${QVR_ROOT}/QC/*.qc" "${QVR_ROOT}/QC/*.qh")
list(APPEND QVR_QC_SRC "${QVR_ROOT}/QC/progs.src" "${QVR_ROOT}/QC/build.sh" "${QVR_ROOT}/QC/fteqcc.ini")
# The engine's headers the QC includes (#include "../Quake/vr/..."): the weapon ids and the *_shared.h tables. A change to
# one compiles the QC again, or the engine and progs.dat disagree (a weapon's id, a material's number).
file(GLOB QVR_QC_SHARED CONFIGURE_DEPENDS "${QVR_ROOT}/Quake/vr/*_shared.h")
list(APPEND QVR_QC_SRC ${QVR_QC_SHARED} "${QVR_ROOT}/Quake/vr/vr_weapon_ids.h")
set(QVR_PROGS "${QVR_ROOT}/quakevr/progs.dat")
set(QVR_QC_LOG "${CMAKE_BINARY_DIR}/qc-build.log")
add_custom_command(OUTPUT "${QVR_PROGS}"
	COMMAND sh -c "QVR_QC_RELEASE=\"$4\" FTEQCC=\"$1\" sh \"$2\" > \"$3\" 2>&1 || { tail -n 40 \"$3\"; echo \"QuakeC failed: the whole log is $3\"; exit 1; }; tail -n 4 \"$3\""
		sh "${QVR_FTEQCC_BIN}" "${QVR_ROOT}/QC/build.sh" "${QVR_QC_LOG}" "${QVR_QC_FLAVOUR}"
	DEPENDS ${QVR_QC_SRC} "${QVR_FTEQCC_BIN}" "${QVR_QC_FLAVOUR_FILE}"
	COMMENT "Quake VR: QuakeC (QC/build.sh, ${QVR_QC_FLAVOUR_TEXT} flavour) into quakevr/progs.dat"
	VERBATIM)
add_custom_target(qvr_progs ALL DEPENDS "${QVR_PROGS}")
if (TARGET qvr_fteqcc)
	add_dependencies(qvr_progs qvr_fteqcc)
endif()
