# qvr_buildinfo.cmake -- writes qvr_buildver.h (vr_crash.cpp: VR_BuildInfo, VR_BuildNumber), at build time:
#   cmake -DQVR_REPO=<repo> -DQVR_OUT=<header> -P qvr_buildinfo.cmake
# The build number is the commit count (git rev-list --count HEAD, always increasing), then the short hash ("+dirty"
# with uncommitted changes to tracked files) and the local time to the minute: "1.1.0-dev build 4321 (abc1234) 2026-10-17
# 14:05". Without git (a source zip): number 0, "unknown" and the time of the build. The header is rewritten only when its
# text changes, so a build that changed nothing compiles nothing.

find_package(Git QUIET)
set(number 0)
set(stamp "unknown")
if(GIT_FOUND)
	execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${QVR_REPO}" rev-list --count HEAD
		OUTPUT_VARIABLE count OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE rc ERROR_QUIET)
	if(rc EQUAL 0 AND NOT count STREQUAL "")
		set(number "${count}")
		execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${QVR_REPO}" rev-parse --short=7 HEAD
			OUTPUT_VARIABLE stamp OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
		execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${QVR_REPO}" --no-optional-locks status --porcelain --untracked-files=no
			OUTPUT_VARIABLE changes OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
		if(NOT changes STREQUAL "")
			set(stamp "${stamp}+dirty")
		endif()
	endif()
endif()
# A build from a copy without .git (the Flatpak: Misc/linux/flatpak) reads them from .qvr-buildinfo, "<number> <hash>",
# written by whoever had the checkout (.github/workflows/flatpak.yml).
if(number EQUAL 0 AND EXISTS "${QVR_REPO}/.qvr-buildinfo")
	file(STRINGS "${QVR_REPO}/.qvr-buildinfo" info LIMIT_COUNT 1)
	if(info MATCHES "^([0-9]+) ([0-9a-zA-Z+]+)")
		set(number "${CMAKE_MATCH_1}")
		set(stamp "${CMAKE_MATCH_2}")
	endif()
endif()
file(STRINGS "${QVR_REPO}/VERSION" semver LIMIT_COUNT 1)
string(STRIP "${semver}" semver)
string(TIMESTAMP date "%Y-%m-%d %H:%M")
set(text "#define QVR_BUILD_NUMBER ${number}\n#define QVR_BUILD_INFO \"${semver}-dev build ${number} (${stamp}) ${date}\"\n")
set(old "")
if(EXISTS "${QVR_OUT}")
	file(READ "${QVR_OUT}" old)
endif()
if(NOT old STREQUAL text)
	get_filename_component(dir "${QVR_OUT}" DIRECTORY)
	file(MAKE_DIRECTORY "${dir}")
	file(WRITE "${QVR_OUT}" "${text}")
endif()
message(STATUS "Quake VR ${semver}-dev build ${number} (${stamp}) ${date}")
