// vr_backend_openxr_panel.cpp -- the 2D panel (vr_backend_openxr.cpp): its swapchain, its images and its place (a quad
// layer).

#include "vr_backend_openxr_internal.hpp"

#ifdef QVR_HAVE_OPENXR

namespace qvr::oxr
{

unsigned OpenXrBackend::acquirePanelImage(int width, int height)
{
    if(!frameBegun || panelPending || width <= 0 || height <= 0 || !ensurePanelSwapchain(width, height))
    {
        return 0;
    }
    return acquireImage(panel);
}

void OpenXrBackend::releasePanelImage()
{
    panelPending = releaseImage(panel);
}

bool OpenXrBackend::runtimePanel(Pose& pose, glm::vec2& size) const
{
    if(!panelShown)
    {
        return false;
    }
    pose = panelPlaced;
    size = panelSize();
    return true;
}

glm::vec2 OpenXrBackend::panelSize() const
{
    return {runtimePanelWidth, panel.width > 0 ? runtimePanelWidth * static_cast<float>(panel.height) / static_cast<float>(panel.width) : 0.f};
}

void OpenXrBackend::placePanel()
{
    panelPlaced = placeRuntimePanel(lastHead);
    const glm::quat& q = panelPlaced.orientation;
    panelPose.position = {panelPlaced.position.x, panelPlaced.position.y, panelPlaced.position.z};
    panelPose.orientation = {q.x, q.y, q.z, q.w};
}

bool OpenXrBackend::ensurePanelSwapchain(int width, int height)
{
    if(panel.handle != XR_NULL_HANDLE && panel.width == width && panel.height == height)
    {
        return true;
    }
    if(panel.handle != XR_NULL_HANDLE)
    {
        const XrSwapchain handle = panel.handle;
        guarded("xrDestroySwapchain", [handle] { return xrDestroySwapchain(handle); });
        panel = Swapchain{};
    }
    return createSwapchain(panel, width, height,
        XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT, "xrCreateSwapchain (panel)");
}

} // namespace qvr::oxr

#endif // QVR_HAVE_OPENXR
