#include "vr_openxr_vulkan.hpp"
#include <cstdio>
#if defined(QVR_HAVE_GL_VULKAN_BRIDGE) && defined(__linux__)
int main()
{
    qvr::openxr_vulkan::Bootstrap bootstrap;
    XrSession session = XR_NULL_HANDLE;
    if(bootstrap.createSession(session) || session != XR_NULL_HANDLE ||
        bootstrap.initialize(XR_NULL_HANDLE, XR_NULL_SYSTEM_ID, false) ||
        bootstrap.context().device || bootstrap.drainAfterSessionDestroy(false) ||
        !bootstrap.drainAfterSessionDestroy(true) || !bootstrap.shutdown()) return 1;
    std::puts("PASS Vulkan2 bootstrap ON CPU guards; no XR/GPU resources created");
    return 0;
}
#else
int main() { std::puts("PASS Vulkan2 bootstrap OFF; no native dependencies or resources"); }
#endif
