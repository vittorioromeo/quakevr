// vr_stereo.cpp -- renders each eye with Ironwail's normal pipeline.
//
// For each eye, an eye-sized set of Ironwail's framebuffers is swapped in, the view is moved
// to the eye (vr_view.cpp) with the eye's asymmetric projection (VR_OverrideProjection), the
// usual V_RenderView runs, and Ironwail's post-process pass (gamma, contrast, dithering)
// writes into the backend's eye image instead of the window. The left eye is then mirrored to
// the window, where the 2D layer is drawn as usual.

#include "vr_crosshair.hpp"
#include "vr_cvars.hpp"
#include "vr_hands.hpp"
#include "vr_lines.hpp"
#include "vr_main.hpp"
#include "vr_panel.hpp"
#include "vr_stereo.hpp"

namespace qvr::stereo
{
namespace
{

glframebufs_t eyeFramebufs{};
int eyeFramebufsWidth = 0;
int eyeFramebufsHeight = 0;
GLuint targetFbo = 0;

bool renderingEye = false;
int currentEye = 0;

void ensureEyeFramebuffers(int width, int height)
{
    if(eyeFramebufsWidth == width && eyeFramebufsHeight == height)
    {
        return;
    }

    const glframebufs_t windowFramebufs = framebufs;
    const int windowWidth = vid.width;
    const int windowHeight = vid.height;

    if(eyeFramebufsWidth)
    {
        framebufs = eyeFramebufs;
        GL_DeleteFrameBuffers();
    }

    vid.width = width;
    vid.height = height;
    GL_CreateFrameBuffers();
    eyeFramebufs = framebufs;

    framebufs = windowFramebufs;
    vid.width = windowWidth;
    vid.height = windowHeight;

    eyeFramebufsWidth = width;
    eyeFramebufsHeight = height;

    if(!targetFbo)
    {
        GL_GenFramebuffersFunc(1, &targetFbo);
    }
}

// Copies the eye just rendered into (its part of) the window, cropped to the aspect ratio.
void mirrorToWindow(int eye, GLuint windowTarget, int windowWidth, int windowHeight)
{
    const int mode = static_cast<int>(vr_mirror.value);
    if(mode <= 0 || (mode == 1 && eye != 0))
    {
        return;
    }

    int dx0 = 0, dx1 = windowWidth;
    if(mode >= 2)
    {
        dx0 = eye * windowWidth / 2;
        dx1 = dx0 + windowWidth / 2;
    }

    const float eyeAspect = static_cast<float>(eyeFramebufsWidth) / eyeFramebufsHeight;
    const float windowAspect = static_cast<float>(dx1 - dx0) / windowHeight;

    int x0 = 0, y0 = 0, x1 = eyeFramebufsWidth, y1 = eyeFramebufsHeight;
    if(windowAspect > eyeAspect)
    {
        const int h = static_cast<int>(eyeFramebufsWidth / windowAspect);
        y0 = (eyeFramebufsHeight - h) / 2;
        y1 = y0 + h;
    }
    else
    {
        const int w = static_cast<int>(eyeFramebufsHeight * windowAspect);
        x0 = (eyeFramebufsWidth - w) / 2;
        x1 = x0 + w;
    }

    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, eyeFramebufs.composite.fbo);
    GL_BindFramebufferFunc(GL_DRAW_FRAMEBUFFER, windowTarget);
    GL_BlitFramebufferFunc(x0, y0, x1, y1, dx0, 0, dx1, windowHeight, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);
}

} // namespace

bool isRenderingEye()
{
    return renderingEye;
}

int eye()
{
    return currentEye;
}

} // namespace qvr::stereo

using namespace qvr;

extern "C" int VR_RenderView()
{
    Backend* be = backend();
    const FrameState& frame = frameState();

    if(!be || !frame.shouldRender || cls.state != ca_connected || cls.signon != SIGNONS ||
        !cl.worldmodel || con_forcedup || !hands::current().valid)
    {
        return 0; // the backend ends the frame without layers
    }

    int width = 0, height = 0;
    be->eyeResolution(width, height);
    if(width <= 0 || height <= 0)
    {
        return 0;
    }

    stereo::ensureEyeFramebuffers(width, height);

    const glframebufs_t windowFramebufs = framebufs;
    const int windowWidth = vid.width, windowHeight = vid.height;
    const GLuint windowTarget = GL_NeedsPostprocess() ? framebufs.composite.fbo : 0;
    const int savedGlx = glx, savedGly = gly, savedGlwidth = glwidth, savedGlheight = glheight;
    const vrect_t savedVrect = r_refdef.vrect;
    const float savedFovX = r_refdef.fov_x, savedFovY = r_refdef.fov_y;

    crosshair::queue(hands::current());

    for(int eye = 0; eye < 2; eye++)
    {
        const unsigned image = be->acquireEyeImage(eye);
        if(!image)
        {
            continue;
        }

        GL_BindFramebufferFunc(GL_FRAMEBUFFER, stereo::targetFbo);
        GL_FramebufferTexture2DFunc(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);

        framebufs = stereo::eyeFramebufs;
        vid.width = width;
        vid.height = height;
        glx = gly = 0;
        glwidth = width;
        glheight = height;
        r_refdef.vrect.x = r_refdef.vrect.y = 0;
        r_refdef.vrect.width = width;
        r_refdef.vrect.height = height;

        stereo::renderingEye = true;
        stereo::currentEye = eye;

        V_RenderView();

        GL_BindFramebufferFunc(GL_FRAMEBUFFER, framebufs.composite.fbo);
        glViewport(0, 0, width, height);
        lines::drawInEye(hands::current().eyeOrigin[eye]);
        panel::drawInEye(hands::current());

        GL_PostProcess();

        stereo::renderingEye = false;
        be->releaseEyeImage(eye);
        stereo::mirrorToWindow(eye, windowTarget, windowWidth, windowHeight);
    }

    framebufs = windowFramebufs;
    vid.width = windowWidth;
    vid.height = windowHeight;
    glx = savedGlx;
    gly = savedGly;
    glwidth = savedGlwidth;
    glheight = savedGlheight;
    r_refdef.vrect = savedVrect;
    r_refdef.fov_x = savedFovX;
    r_refdef.fov_y = savedFovY;

    GL_BindFramebufferFunc(GL_FRAMEBUFFER, windowTarget);

    be->endFrame(true);
    panel::setStereoThisFrame(true);
    return 1;
}

extern "C" int VR_RenderingEye()
{
    return stereo::renderingEye;
}

extern "C" unsigned VR_PostProcessTarget()
{
    return stereo::renderingEye ? stereo::targetFbo : 0;
}

// Replaces the symmetric projection with the eye's asymmetric one. Ironwail's projection maps
// Quake view space (x forward, y left, z up) to clip space; only the terms producing clip x/y
// change, the (reversed-Z) depth terms are kept.
extern "C" void VR_OverrideProjection(float matrix[16])
{
    if(!stereo::renderingEye)
    {
        return;
    }

    const Fov& fov = frameState().eyes[stereo::currentEye].fov;
    const float l = std::tan(fov.left);
    const float r = std::tan(fov.right);
    const float u = std::tan(fov.up);
    const float d = std::tan(fov.down);

    matrix[1 * 4 + 0] = -2.f / (r - l);   // clip x from -y (right)
    matrix[0 * 4 + 0] = -(r + l) / (r - l); // off-axis shift, times depth (x)
    matrix[2 * 4 + 1] = 2.f / (u - d);    // clip y from z (up)
    matrix[0 * 4 + 1] = -(u + d) / (u - d);
}
