#pragma once

namespace qvr::frame_synthesis
{
void beginFrame();
// Capture each eye's actual scene depth before the shared scene targets are reused.
void captureDepth(int eye, int width, int height, unsigned depthFormat);
// Generate dynamic image motion from final color, export acquired runtime images.
void render(int eye, unsigned colorFramebuffer, int width, int height);
// Commit both eyes' history only after successful stereo submission.
void submitted(bool stereoSubmitted);
void reset();
void shutdown();
} // namespace qvr::frame_synthesis
