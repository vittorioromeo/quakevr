# vr.mk -- the Quake VR module in the Makefile builds (Makefile and Makefile.w64 include it after their
# OBJS, before their rules; each adds its thread library for Zancle). C++23: any file may include Zancle. No OpenXR in
# these builds: mock backend only. The engine links as C++.

LINKER = $(CXX)

VROBJS := $(patsubst %.cpp,%.o,$(wildcard vr/*.cpp))
OBJS += $(VROBJS)
OBJDEPS += $(VROBJS:%.o=%.d)
# No exceptions (docs/vr-port/CODE_STYLE.md): the module and Zancle are built without them.
VR_CXXFLAGS = $(filter-out -std=%,$(CFLAGS)) -std=c++23 -fno-exceptions -DZA_STATIC -I. -Ivr -Ivr/external -Ivr/external/zancle/include
# The version (vr_crash.cpp, VR_Version): the repository's VERSION file; a dev build (docs/vr-port/RELEASING.md).
QVR_VERSION := $(strip $(shell cat ../VERSION 2>/dev/null))
ifneq ($(QVR_VERSION),)
VR_CXXFLAGS += -DQVR_VERSION='"$(QVR_VERSION)"'
endif
# The build number and date (vr/qvr_buildver.h, as vr/qvr_buildinfo.cmake): the commit count (0 without git), the short
# hash ("+dirty" with uncommitted changes to tracked files), the local time to the minute: "1.1.0-dev build 4321
# (abc1234) 2026-10-17 14:05". Written at every make run, only when its text changes. Without git the file is left out
# and vr_crash.cpp has its fallback (number 0, the compiler's date).
QVR_BUILD_COUNT := $(strip $(shell git -C .. rev-list --count HEAD 2>/dev/null))
ifneq ($(QVR_BUILD_COUNT),)
QVR_BUILD_HASH := $(strip $(shell git -C .. rev-parse --short=7 HEAD 2>/dev/null))
ifneq ($(strip $(shell git -C .. --no-optional-locks status --porcelain --untracked-files=no 2>/dev/null)),)
QVR_BUILD_HASH := $(QVR_BUILD_HASH)+dirty
endif
QVR_HASH := \#
QVR_BUILD_INFO := $(QVR_VERSION)-dev build $(QVR_BUILD_COUNT) ($(QVR_BUILD_HASH)) $(shell date '+%Y-%m-%d %H:%M')
$(shell printf '$(QVR_HASH)define QVR_BUILD_NUMBER %s\n$(QVR_HASH)define QVR_BUILD_INFO "%s"\n' '$(QVR_BUILD_COUNT)' '$(QVR_BUILD_INFO)' > vr/qvr_buildver.tmp; cmp -s vr/qvr_buildver.tmp vr/qvr_buildver.h || mv -f vr/qvr_buildver.tmp vr/qvr_buildver.h)
endif

# Zancle (vr/external/zancle/README.md), the modules the Quake VR code uses: C++23, optimised and without Zancle's
# asserts (NDEBUG); with DEBUG=1 and QVR_ZANCLE_DEBUG=1 (the default while the migration settles; 0 to switch it off)
# built as the engine's debug build with its asserts on (their handler: vr_zancle.cpp; QVR_ZANCLE_DEBUG tells that file
# the library has its own assert function).
QVR_ZANCLE_DEBUG ?= 1
ZANCLE_DIR = vr/external/zancle
ZANCLEOBJS := $(patsubst %.cpp,%.o,$(wildcard $(ZANCLE_DIR)/src/Zancle/*/*.cpp))
OBJS += $(ZANCLEOBJS)
OBJDEPS += $(ZANCLEOBJS:%.o=%.d)
ifeq ($(DEBUG)$(QVR_ZANCLE_DEBUG),11)
VR_CXXFLAGS += -DQVR_ZANCLE_DEBUG
ZANCLE_CXXFLAGS = $(filter-out -std=%,$(CFLAGS)) -std=c++23 -fno-exceptions -DZA_STATIC -I$(ZANCLE_DIR)/include \
	-I$(ZANCLE_DIR)/src -I$(ZANCLE_DIR)/extlibs/moodycamel
else
ZANCLE_CXXFLAGS = $(filter-out -std=% -O%,$(CFLAGS)) -std=c++23 -fno-exceptions -O2 -DNDEBUG -DZA_STATIC -I$(ZANCLE_DIR)/include \
	-I$(ZANCLE_DIR)/src -I$(ZANCLE_DIR)/extlibs/moodycamel
endif

QVR_CLEAN = vr/*.o vr/*.d $(ZANCLEOBJS) $(ZANCLEOBJS:%.o=%.d)

vr/%.o:	vr/%.cpp $(MAKEFILE)
	@echo "Compiling $<" && \
	$(CXX) $(DFLAGS) -c $(VR_CXXFLAGS) $(SDL_CFLAGS) -MMD -MP -o $@ $<

$(ZANCLE_DIR)/%.o:	$(ZANCLE_DIR)/%.cpp $(MAKEFILE)
	@echo "Compiling $<" && \
	$(CXX) -c $(ZANCLE_CXXFLAGS) -MMD -MP -o $@ $<
