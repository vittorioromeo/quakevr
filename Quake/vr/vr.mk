# vr.mk -- the Quake VR module in the Makefile builds (Makefile, Makefile.w32 and Makefile.w64 include it after their
# OBJS, before their rules; each adds its thread library for Zancle). C++23: any file may include Zancle. No OpenXR in
# these builds: mock backend only. The engine links as C++.

LINKER = $(CXX)

VROBJS := $(patsubst %.cpp,%.o,$(wildcard vr/*.cpp))
OBJS += $(VROBJS)
OBJDEPS += $(VROBJS:%.o=%.d)
# No exceptions (docs/vr-port/CODE_STYLE.md): the module and Zancle are built without them.
VR_CXXFLAGS = $(filter-out -std=%,$(CFLAGS)) -std=c++23 -fno-exceptions -DZA_STATIC -I. -Ivr -Ivr/external -Ivr/external/zancle/include

# Zancle (vr/external/zancle/README.md), the modules the Quake VR code uses: C++23, optimised and without Zancle's
# asserts (NDEBUG); with DEBUG=1 and QVR_ZANCLE_DEBUG=1 (the default while the migration settles; 0 to switch it off)
# built as the engine's debug build with its asserts on (their handler: vr_zancle.cpp).
QVR_ZANCLE_DEBUG ?= 1
ZANCLE_DIR = vr/external/zancle
ZANCLEOBJS := $(patsubst %.cpp,%.o,$(wildcard $(ZANCLE_DIR)/src/Zancle/*/*.cpp))
OBJS += $(ZANCLEOBJS)
OBJDEPS += $(ZANCLEOBJS:%.o=%.d)
ifeq ($(DEBUG)$(QVR_ZANCLE_DEBUG),11)
ZANCLE_CXXFLAGS = $(filter-out -std=%,$(CFLAGS)) -std=c++23 -fno-exceptions -DQVR_ZANCLE_DEBUG -DZA_STATIC -I$(ZANCLE_DIR)/include \
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
