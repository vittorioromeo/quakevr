# vr.mk -- the Quake VR module in the Makefile builds (Makefile, Makefile.w32 and Makefile.w64 include it after their
# OBJS, before their rules; each adds its thread library for Zancle). C++23: any file may include Zancle. No OpenXR in
# these builds: mock backend only. The engine links as C++.

LINKER = $(CXX)

VROBJS := $(patsubst %.cpp,%.o,$(wildcard vr/*.cpp))
OBJS += $(VROBJS)
OBJDEPS += $(VROBJS:%.o=%.d)
VR_CXXFLAGS = $(filter-out -std=%,$(CFLAGS)) -std=c++23 -DZA_STATIC -I. -Ivr -Ivr/external -Ivr/external/zancle/include

# Zancle (vr/external/zancle/README.md), its concurrency module: C++23, optimised and without Zancle's asserts
# (NDEBUG) in every build, DEBUG=1 too.
ZANCLE_DIR = vr/external/zancle
ZANCLEOBJS := $(patsubst %.cpp,%.o,$(wildcard $(ZANCLE_DIR)/src/Zancle/*/*.cpp))
OBJS += $(ZANCLEOBJS)
OBJDEPS += $(ZANCLEOBJS:%.o=%.d)
ZANCLE_CXXFLAGS = $(filter-out -std=% -O%,$(CFLAGS)) -std=c++23 -O2 -DNDEBUG -DZA_STATIC -I$(ZANCLE_DIR)/include \
	-I$(ZANCLE_DIR)/src -I$(ZANCLE_DIR)/extlibs/moodycamel

QVR_CLEAN = vr/*.o vr/*.d $(ZANCLEOBJS) $(ZANCLEOBJS:%.o=%.d)

vr/%.o:	vr/%.cpp $(MAKEFILE)
	@echo "Compiling $<" && \
	$(CXX) $(DFLAGS) -c $(VR_CXXFLAGS) $(SDL_CFLAGS) -MMD -MP -o $@ $<

$(ZANCLE_DIR)/%.o:	$(ZANCLE_DIR)/%.cpp $(MAKEFILE)
	@echo "Compiling $<" && \
	$(CXX) -c $(ZANCLE_CXXFLAGS) -MMD -MP -o $@ $<
