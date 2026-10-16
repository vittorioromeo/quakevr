// Standalone hidden desktop GL fixture. No Quake startup, OpenXR, headset scene,
// game data or settings. Exercises the actual production GLSL strings.
#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include "vr_synthesis_shaders.hpp"
#include "vr_synthesis_math.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>
#include <algorithm>

namespace
{
constexpr int size = 64;
void require(bool ok, const char* message)
{
    if(!ok)
    {
        std::fprintf(stderr, "FAIL: %s; SDL=%s GL=0x%x\n", message, SDL_GetError(), glGetError());
        std::exit(1);
    }
}
GLuint compile(GLenum type, const char* text)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if(!ok)
    {
        char log[4096]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "%s\n", log);
    }
    require(ok, "production shader compiles");
    return shader;
}
GLuint program(const char* fragment)
{
    GLuint vs = compile(GL_VERTEX_SHADER, qvr::synthesis::shaders::fullscreen);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fragment);
    GLuint p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glLinkProgram(p);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    require(ok, "production shaders link");
    return p;
}
GLuint textureSized(GLenum format, int width, int height, int levels);
GLuint texture(GLenum format, int levels)
{
    return textureSized(format, size, size, levels);
}
GLuint textureSized(GLenum format, int width, int height, int levels)
{
    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexStorage2D(GL_TEXTURE_2D, levels, format, width, height);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, levels > 1 ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, levels - 1);
    return id;
}
void bind(int unit, GLuint id)
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id);
}
void target(GLuint fbo, GLuint color, GLuint depth = 0, int level = 0)
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, level);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "test framebuffer complete");
}
void image(GLuint id, int shiftX, int shiftY)
{
    std::vector<float> pixels(size * size * 4);
    for(int y = 0; y < size; ++y)
    for(int x = 0; x < size; ++x)
    {
        float color = 0.12f + 0.04f * std::sin(x * 0.17f + y * 0.11f);
        if(x >= 16 + shiftX && x < 48 + shiftX && y >= 16 + shiftY && y < 48 + shiftY)
        {
            const float u = static_cast<float>(x - 16 - shiftX), v = static_cast<float>(y - 16 - shiftY);
            color = 0.5f + 0.2f * std::sin(u * 0.29f + v * 0.31f) + 0.2f * std::cos(u * 0.37f - v * 0.23f);
        }
        const int at = (y * size + x) * 4;
        pixels[at] = pixels[at + 1] = pixels[at + 2] = color;
        pixels[at + 3] = 1.f;
    }
    bind(0, id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_FLOAT, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
}
void depthFixture(GLuint fbo, GLuint depthProgram, bool reversed)
{
    GLuint source = texture(GL_DEPTH_COMPONENT32F, 1), seed = texture(GL_RGBA16F, 1), output = texture(GL_DEPTH_COMPONENT32F, 1);
    float projection[16]{};
    projection[3] = 1.f;
    qvr::synthesis::overrideEyeProjection(projection, -1.f, 1.f, 1.f, -1.f, 1.f, 4096.f, reversed);
    const glm::mat4 previous = glm::make_mat4(projection);
    const glm::mat4 current = previous * glm::translate(glm::mat4{1.f}, {0.f, -2.f, 0.f});
    const glm::mat4 inverse = glm::inverse(current);
    const glm::vec4 clip = current * glm::vec4{16.f, 0.f, 0.f, 1.f};
    const float value = reversed ? clip.z / clip.w : clip.z / clip.w * 0.5f + 0.5f;
    std::vector<float> pixels(size * size, value);
    bind(0, source);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());
    target(fbo, seed, output);
    glViewport(0, 0, size, size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(depthProgram);
    glUniform2f(0, size, size);
    glUniform4f(1, 0.f, 0.f, 1.f, 1.f);
    glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(inverse));
    glUniformMatrix4fv(6, 1, GL_FALSE, glm::value_ptr(previous));
    glUniform3f(10, 0.f, 2.f, 0.f);
    glUniform1i(11, reversed);
    glUniform1i(12, 1);
    bind(0, source);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    float motion[4]{}, depth = 0.f;
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_FLOAT, motion);
    glReadPixels(32, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    std::printf("depth_fixture reversed=%d source=%.8f captured=%.8f seed=(%.8f,%.8f)\n", reversed, value, depth, motion[0], motion[1]);
    require(std::abs(motion[0] + 0.0625f) < 8e-5f, "actual depth shader computes camera seed within half-float quantization");
    require(std::abs(depth - value) < 1e-6f, "actual depth shader preserves conventional/reversed depth");
    std::fill(pixels.begin(), pixels.end(), reversed ? 0.f : 1.f);
    bind(0, source);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_FLOAT, motion);
    require(std::abs(motion[0]) < 1e-5f, "sky seed ignores camera translation");
    require(glGetError() == GL_NO_ERROR, "depth capture leaves no GL error");
    GLuint textures[]{source, seed, output};
    glDeleteTextures(3, textures);
}

void multisampleFixture(GLuint fbo, GLuint depthProgram, bool reversed)
{
    GLuint source = 0, sourceFbo = 0;
    glGenTextures(1, &source);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, source);
    glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH_COMPONENT32F, size, size, GL_TRUE);
    glGenFramebuffers(1, &sourceFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, sourceFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, source, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "MSAA source framebuffer");
    const char* fill = "#version 430\nlayout(location=0) uniform float Depth; void main(){gl_FragDepth=Depth;}";
    GLuint fillProgram = program(fill);
    glUseProgram(fillProgram);
    glViewport(0, 0, size, size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glEnable(GL_SAMPLE_MASK);
    const float values[]{0.25f, 0.5f, 0.75f, 0.6f};
    for(int s = 0; s < 4; ++s)
    {
        glSampleMaski(0, 1u << s);
        glUniform1f(0, values[s]);
        glDrawArrays(GL_TRIANGLES, 0, 3);
    }
    glDisable(GL_SAMPLE_MASK);
    glSampleMaski(0, ~0u);
    GLuint seed = texture(GL_RGBA16F, 1), output = texture(GL_DEPTH_COMPONENT32F, 1);
    target(fbo, seed, output);
    glUseProgram(depthProgram);
    glUniform2f(0, size, size);
    glUniform4f(1, 0.f, 0.f, 1.f, 1.f);
    const glm::mat4 identity{1.f};
    glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(6, 1, GL_FALSE, glm::value_ptr(identity));
    glUniform3f(10, 0.f, 0.f, 0.f);
    glUniform1i(11, reversed);
    glUniform1i(12, 4);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, source);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    float depth = 0.f;
    glReadPixels(32, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    require(std::abs(depth - (reversed ? 0.75f : 0.25f)) < 1e-6f, "MSAA resolves nearest surface in both Z conventions");
    require(glGetError() == GL_NO_ERROR, "MSAA depth capture leaves no GL error");
    GLuint textures[]{source, seed, output};
    glDeleteTextures(3, textures);
    glDeleteFramebuffers(1, &sourceFbo);
    glDeleteProgram(fillProgram);
}
void finiteGuardFixture(GLuint fbo, GLuint depthProgram, GLuint flowProgram)
{
    constexpr int width = 65, height = 61;
    GLuint sourceDepth = textureSized(GL_DEPTH_COMPONENT32F, width, height, 1);
    GLuint seed = textureSized(GL_RGBA16F, width, height, 1);
    GLuint outputDepth = textureSized(GL_DEPTH_COMPONENT32F, width, height, 1);
    std::vector<float> depthPixels(width * height, 0.5f);
    bind(0, sourceDepth);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depthPixels.data());
    float inverse[16]{}, previous[16]{};
    inverse[0] = inverse[5] = inverse[10] = inverse[15] = 1.f;
    previous[0] = std::numeric_limits<float>::quiet_NaN();
    previous[5] = previous[10] = previous[15] = 1.f;
    target(fbo, seed, outputDepth);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(depthProgram);
    glUniform2f(0, width, height);
    glUniform4f(1, 0.f, 0.f, 1.f, 1.f);
    glUniformMatrix4fv(2, 1, GL_FALSE, inverse);
    glUniformMatrix4fv(6, 1, GL_FALSE, previous);
    glUniform3f(10, 0.f, 0.f, 0.f);
    glUniform1i(11, 0);
    glUniform1i(12, 1);
    bind(0, sourceDepth);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    std::vector<float> seedPixels(width * height * 4);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, seedPixels.data());
    bool seedFinite = true;
    for(size_t i = 0; i < seedPixels.size(); i += 4)
        seedFinite &= std::isfinite(seedPixels[i]) && std::isfinite(seedPixels[i + 1]);
    require(seedFinite, "depth reprojection falls back to current UV when previous clip coordinates are non-finite");

    GLuint current = textureSized(GL_RGBA8, width, height, 1);
    GLuint old = textureSized(GL_RGBA8, width, height, 1);
    GLuint flow = textureSized(GL_RGBA16F, width, height, 1);
    std::vector<unsigned char> colorPixels(width * height * 4, 64);
    for(size_t i = 3; i < colorPixels.size(); i += 4) colorPixels[i] = 255;
    bind(0, current);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, colorPixels.data());
    bind(0, old);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, colorPixels.data());
    std::fill(seedPixels.begin(), seedPixels.end(), 0.f);
    seedPixels[0] = std::numeric_limits<float>::quiet_NaN();
    seedPixels[1] = std::numeric_limits<float>::infinity();
    bind(0, seed);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_FLOAT, seedPixels.data());
    target(fbo, flow);
    glViewport(0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(flowProgram);
    bind(0, current);
    bind(1, old);
    bind(2, seed);
    glUniform2f(0, width, height);
    glUniform1i(1, 0);
    glUniform1i(2, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    std::vector<float> flowPixels(width * height * 4);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, flowPixels.data());
    bool flowFinite = true;
    for(size_t i = 0; i < flowPixels.size(); i += 4)
        flowFinite &= std::isfinite(flowPixels[i]) && std::isfinite(flowPixels[i + 1]);
    require(flowFinite, "Lucas-Kanade flow sanitizes non-finite camera seeds across odd image edges");
    require(glGetError() == GL_NO_ERROR, "non-finite depth and flow guards leave no GL error");
    GLuint textures[]{sourceDepth, seed, outputDepth, current, old, flow};
    glDeleteTextures(6, textures);
}
void dynamicFixture(GLuint fbo, GLuint flowProgram, GLuint exportProgram, int eye, int shiftX, int shiftY)
{
    GLuint current = texture(GL_RGBA8, 3), previous = texture(GL_RGBA8, 3);
    image(current, shiftX, shiftY);
    image(previous, 0, 0);
    GLuint seed = texture(GL_RGBA16F, 1);
    target(fbo, seed);
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT); // camera is stationary; object moves independently
    GLuint flow[2]{texture(GL_RGBA16F, 3), texture(GL_RGBA16F, 3)};
    glDisable(GL_DEPTH_TEST);
    glUseProgram(flowProgram);
    bind(0, current);
    bind(1, previous);
    int last = 0;
    for(int level = 2; level >= 0; --level)
    for(int iteration = 0; iteration < qvr::synthesis::shaders::flowIterations; ++iteration)
    {
        const int out = last ^ 1;
        target(fbo, flow[out], 0, level);
        glViewport(0, 0, size >> level, size >> level);
        const bool first = level == 2 && iteration == 0;
        bind(2, first ? seed : flow[last]);
        glUniform2f(0, size >> level, size >> level);
        glUniform1i(1, level);
        glUniform1i(2, first ? 0 : iteration == 0 ? level + 1 : level);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        last = out;
    }
    std::vector<float> pixels(size * size * 4);
    glReadPixels(0, 0, size, size, GL_RGBA, GL_FLOAT, pixels.data());
    double error = 0.0, background = 0.0;
    for(int y = 28 + shiftY; y < 36 + shiftY; ++y)
    for(int x = 28 + shiftX; x < 36 + shiftX; ++x)
    {
        const int at = (y * size + x) * 4;
        error += std::abs(pixels[at] * size + shiftX) + std::abs(pixels[at + 1] * size + shiftY);
    }
    for(int y = 3; y < 8; ++y)
    for(int x = 3; x < 8; ++x)
    {
        const int at = (y * size + x) * 4;
        background += std::abs(pixels[at] * size) + std::abs(pixels[at + 1] * size);
    }
    error /= 64.0;
    background /= 25.0;
    std::printf("eye=%d independent_foreground_shift=(%d,%d) mean_flow_error=%.4f px stationary_background_error=%.4f px\n",
        eye, shiftX, shiftY, error, background);
    require(error < 0.75, "dynamic foreground XY correspondence within subpixel fixture tolerance");
    require(background < 0.25, "stationary background does not acquire object motion");
    GLuint depth = texture(GL_DEPTH_COMPONENT32F, 1), motion = texture(GL_RGBA16F, 1), outputDepth = texture(GL_DEPTH_COMPONENT32F, 1);
    std::vector<float> depths(size * size, 0.625f);
    bind(0, depth);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_DEPTH_COMPONENT, GL_FLOAT, depths.data());
    target(fbo, motion, outputDepth);
    glViewport(0, 0, size, size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(exportProgram);
    bind(0, flow[last]);
    bind(1, depth);
    bind(2, seed);
    glUniform2f(0, size, size);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    float vector[4]{}, z = 0.f;
    glReadPixels(32 + shiftX, 32 + shiftY, 1, 1, GL_RGBA, GL_FLOAT, vector);
    glReadPixels(32 + shiftX, 32 + shiftY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &z);
    require(std::abs(vector[0] - 2.f * shiftX / size) < 0.025f, "EXT X vector sign and NDC scale");
    require(std::abs(vector[1] + 2.f * shiftY / size) < 0.025f, "EXT Vulkan-down Y sign and NDC scale");
    require(std::abs(z - 0.625f) < 1e-6f, "export writes actual depth, not a clear/placeholder");
    require(glGetError() == GL_NO_ERROR, "dynamic flow/export leaves no GL error");
    GLuint textures[]{current, previous, seed, flow[0], flow[1], depth, motion, outputDepth};
    glDeleteTextures(8, textures);
}

void srgbMotionFixture(GLuint fbo, GLuint exportProgram)
{
    GLuint flow = texture(GL_RGBA16F, 1), seed = texture(GL_RGBA16F, 1), depth = texture(GL_DEPTH_COMPONENT32F, 1);
    GLuint motion = texture(GL_SRGB8_ALPHA8, 1), decoded = texture(GL_RGBA32F, 1), decodedDepth = texture(GL_DEPTH_COMPONENT32F, 1);
    std::vector<float> flowPixels(size * size * 4), depthPixels(size * size, 0.75f);
    std::vector<float> seedPixels(size * size * 4);
    for(int pixel = 0; pixel < size * size; ++pixel)
    {
        flowPixels[pixel * 4] = -0.015625f; // one pixel of previousUV-currentUV
        flowPixels[pixel * 4 + 1] = 0.0078125f;
        flowPixels[pixel * 4 + 3] = 1.f;
        seedPixels[pixel * 4] = -0.0078125f;
        seedPixels[pixel * 4 + 1] = 0.00390625f;
        seedPixels[pixel * 4 + 3] = 1.f;
    }
    bind(0, flow);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_FLOAT, flowPixels.data());
    bind(0, depth);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_DEPTH_COMPONENT, GL_FLOAT, depthPixels.data());
    bind(0, seed);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_FLOAT, seedPixels.data());

    qvr::synthesis::Srgb8MotionEncoding encoding;
    require(qvr::synthesis::srgb8MotionEncoding(0.0625f, encoding), "sRGB8 fixture encoding created");
    target(fbo, motion, decodedDepth);
    glViewport(0, 0, size, size);
    glDisable(GL_FRAMEBUFFER_SRGB); // shader explicitly encodes before the sRGB attachment write
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(exportProgram);
    bind(0, flow);
    bind(1, depth);
    bind(2, seed);
    glUniform2f(0, size, size);
    glUniform2f(3, encoding.scale, encoding.scale);
    glUniform2f(4, encoding.offset, encoding.offset);
    glUniform1i(5, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    const char* readback = "#version 430\nlayout(binding=0) uniform sampler2D Image; layout(location=7) uniform ivec2 At; layout(location=0) out vec4 Value; void main(){Value=texelFetch(Image, At, 0);}";
    GLuint readbackProgram = program(readback);
    target(fbo, decoded);
    glDisable(GL_DEPTH_TEST);
    glUseProgram(readbackProgram);
    bind(0, motion);
    glUniform2i(7, 32, 32);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    float value[4]{};
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_FLOAT, value);
    const float x = value[0] * encoding.scale + encoding.offset;
    const float y = value[1] * encoding.scale + encoding.offset;
    std::printf("srgb8_motion samples=(%.6f,%.6f) decoded_ndc=(%.6f,%.6f) error_pixels=(%.3f,%.3f)\n",
        value[0], value[1], x, y, (x - 0.03125f) * size * 0.5f, (y - 0.015625f) * size * 0.5f);
    require(std::abs(x - 0.03125f) < 0.0025f && std::abs(y - 0.015625f) < 0.0025f,
        "sRGB8 attachment and sampler preserve signed X/Y motion within one pixel at fixture width");
    require(value[3] > 0.99f, "valid motion is marked as unmodified in the ignored alpha channel");
    const auto decodeAt = [&](int px, int py, float result[4])
    {
        target(fbo, decoded);
        glUseProgram(readbackProgram);
        bind(0, motion);
        glUniform2i(7, px, py);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glReadPixels(32, 32, 1, 1, GL_RGBA, GL_FLOAT, result);
    };
    // A large finite vector and a non-finite vector both take the camera seed
    // path. The encoded value remains in the representable interval, with
    // alpha=0 marking the quality fallback for diagnostics.
    flowPixels[(12 * size + 12) * 4] = -0.25f;
    flowPixels[(20 * size + 20) * 4] = std::numeric_limits<float>::quiet_NaN();
    flowPixels[(30 * size + 30) * 4] = std::numeric_limits<float>::infinity();
    seedPixels[(30 * size + 30) * 4] = 0.25f; // also outside range: must become neutral motion
    bind(0, flow);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_FLOAT, flowPixels.data());
    bind(0, seed);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_FLOAT, seedPixels.data());
    target(fbo, motion, decodedDepth);
    glUseProgram(exportProgram);
    bind(0, flow);
    bind(1, depth);
    bind(2, seed);
    glUniform2f(0, size, size);
    glUniform2f(3, encoding.scale, encoding.scale);
    glUniform2f(4, encoding.offset, encoding.offset);
    glUniform1i(5, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    float fallback[4]{};
    decodeAt(12, 12, fallback);
    require(fallback[3] < 0.01f &&
        std::abs(fallback[0] * encoding.scale + encoding.offset - 0.015625f) < 0.0025f &&
        std::abs(fallback[1] * encoding.scale + encoding.offset - 0.0078125f) < 0.0025f,
        "out-of-range motion uses the finite, in-range camera seed instead of saturating");
    decodeAt(20, 20, fallback);
    require(fallback[3] < 0.01f &&
        std::abs(fallback[0] * encoding.scale + encoding.offset - 0.015625f) < 0.0025f,
        "non-finite motion uses the finite camera seed instead of propagating NaN");
    decodeAt(30, 30, fallback);
    require(fallback[3] < 0.01f &&
        std::abs(fallback[0] * encoding.scale + encoding.offset) < 0.0025f &&
        std::abs(fallback[1] * encoding.scale + encoding.offset) < 0.0025f,
        "out-of-range camera seed falls back to representable zero motion");
    require(glGetError() == GL_NO_ERROR, "sRGB8 motion render and decode leave no GL error");
    glDeleteProgram(readbackProgram);
    GLuint textures[]{flow, seed, depth, motion, decoded, decodedDepth};
    glDeleteTextures(6, textures);
}

// The lenses' hidden area is drawn at the near plane (depth 1 reversed, 0 conventional): no surface, so no seed, no
// motion and the far depth in the export, while the visible side keeps what it had (vr_xr_frame_synthesis_mask).
void hiddenAreaFixture(GLuint fbo, GLuint depthProgram, GLuint exportProgram, bool reversed)
{
    const float hiddenDepth = reversed ? 1.f : 0.f, farDepth = reversed ? 0.f : 1.f;
    GLuint source = texture(GL_DEPTH_COMPONENT32F, 1), seed = texture(GL_RGBA16F, 1), output = texture(GL_DEPTH_COMPONENT32F, 1);
    float projection[16]{};
    projection[3] = 1.f;
    qvr::synthesis::overrideEyeProjection(projection, -1.f, 1.f, 1.f, -1.f, 1.f, 4096.f, reversed);
    const glm::mat4 previous = glm::make_mat4(projection);
    const glm::mat4 current = previous * glm::translate(glm::mat4{1.f}, {0.f, -2.f, 0.f});
    const glm::mat4 inverse = glm::inverse(current);
    const glm::vec4 clip = current * glm::vec4{16.f, 0.f, 0.f, 1.f};
    const float value = reversed ? clip.z / clip.w : clip.z / clip.w * 0.5f + 0.5f;
    std::vector<float> pixels(size * size, value);
    for(int y = 0; y < size; ++y)
    for(int x = 0; x < size / 2; ++x) pixels[y * size + x] = hiddenDepth; // left half hidden, right half a surface
    bind(0, source);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());
    target(fbo, seed, output);
    glViewport(0, 0, size, size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(depthProgram);
    glUniform2f(0, size, size);
    glUniform4f(1, 0.f, 0.f, 1.f, 1.f);
    glUniformMatrix4fv(2, 1, GL_FALSE, glm::value_ptr(inverse));
    glUniformMatrix4fv(6, 1, GL_FALSE, glm::value_ptr(previous));
    glUniform3f(10, 0.f, 2.f, 0.f);
    glUniform1i(11, reversed);
    glUniform1i(12, 1);
    bind(0, source);
    float hidden[4]{}, visible[4]{};
    glUniform1i(13, 1);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(16, 32, 1, 1, GL_RGBA, GL_FLOAT, hidden);
    glReadPixels(48, 32, 1, 1, GL_RGBA, GL_FLOAT, visible);
    std::printf("hidden_area_fixture reversed=%d seed hidden=(%.6f,%.6f,%.6f) visible=(%.6f,%.6f)\n", reversed,
        hidden[0], hidden[1], hidden[2], visible[0], visible[1]);
    require(std::abs(hidden[0]) < 1e-5f && std::abs(hidden[1]) < 1e-5f && std::abs(hidden[2]) < 1e-5f,
        "hidden area has a zero camera seed");
    require(std::abs(visible[0] + 0.0625f) < 8e-5f, "the visible side keeps its camera seed beside a hidden area");
    glUniform1i(13, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glReadPixels(16, 32, 1, 1, GL_RGBA, GL_FLOAT, hidden);
    require(std::abs(hidden[0]) > 1e-3f, "mask 0 leaves the near-plane reprojection as it was");

    GLuint flow = texture(GL_RGBA16F, 1), motion = texture(GL_RGBA16F, 1);
    std::vector<float> flowPixels(size * size * 4);
    for(int pixel = 0; pixel < size * size; ++pixel)
    {
        flowPixels[pixel * 4] = -0.015625f; // one pixel of previousUV-currentUV
        flowPixels[pixel * 4 + 1] = 0.0078125f;
        flowPixels[pixel * 4 + 3] = 1.f;
    }
    bind(0, flow);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_FLOAT, flowPixels.data());
    target(fbo, motion, output);
    glViewport(0, 0, size, size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(exportProgram);
    bind(0, flow);
    bind(1, source);
    bind(2, seed);
    glUniform2f(0, size, size);
    glUniform1i(5, 0); // the program keeps its uniforms: the sRGB8 fixture left its encoding on
    glUniform1i(6, 0);
    glUniform1i(7, 0);
    glUniform1i(8, reversed);
    for(int mask = 1; mask >= 0; --mask)
    {
        glUniform1i(9, mask);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        float hiddenVector[4]{}, visibleVector[4]{}, hiddenZ = 0.f, visibleZ = 0.f;
        glReadPixels(16, 32, 1, 1, GL_RGBA, GL_FLOAT, hiddenVector);
        glReadPixels(16, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &hiddenZ);
        glReadPixels(48, 32, 1, 1, GL_RGBA, GL_FLOAT, visibleVector);
        glReadPixels(48, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &visibleZ);
        std::printf("hidden_area_export reversed=%d mask=%d hidden=(%.5f,%.5f,a=%.2f,z=%.4f) visible=(%.5f,%.5f,a=%.2f,z=%.4f)\n",
            reversed, mask, hiddenVector[0], hiddenVector[1], hiddenVector[3], hiddenZ, visibleVector[0], visibleVector[1],
            visibleVector[3], visibleZ);
        if(mask)
        {
            require(std::abs(hiddenVector[0]) < 1e-5f && std::abs(hiddenVector[1]) < 1e-5f && hiddenVector[3] < 0.01f,
                "hidden area exports no motion (alpha 0: not estimated)");
            require(std::abs(hiddenZ - farDepth) < 1e-6f, "hidden area exports the far depth, not the near plane");
        }
        else
        {
            require(std::abs(hiddenVector[0] - 0.03125f) < 0.002f && std::abs(hiddenZ - hiddenDepth) < 1e-6f,
                "mask 0 exports the hidden area as drawn");
        }
        require(std::abs(visibleVector[0] - 0.03125f) < 0.002f && std::abs(visibleVector[1] - 0.015625f) < 0.002f &&
                visibleVector[3] > 0.99f && std::abs(visibleZ - value) < 1e-6f,
            "visible side exports its motion and depth unchanged beside a hidden area");
    }
    require(glGetError() == GL_NO_ERROR, "hidden area fixture leaves no GL error");
    GLuint textures[]{source, seed, output, flow, motion};
    glDeleteTextures(5, textures);
}

// A flow texel the flow pass never wrote (alpha 0 from the clear, however finite its xy) is no estimate: the export
// takes the camera seed there and marks it, while its covered neighbour keeps the flow.
void uncoveredFlowFixture(GLuint fbo, GLuint exportProgram)
{
    GLuint flow = texture(GL_RGBA16F, 1), seed = texture(GL_RGBA16F, 1), depth = texture(GL_DEPTH_COMPONENT32F, 1);
    GLuint motion = texture(GL_RGBA16F, 1), outputDepth = texture(GL_DEPTH_COMPONENT32F, 1);
    std::vector<float> flowPixels(size * size * 4), seedPixels(size * size * 4), depthPixels(size * size, 0.75f);
    for(int pixel = 0; pixel < size * size; ++pixel)
    {
        flowPixels[pixel * 4] = -0.015625f;
        flowPixels[pixel * 4 + 1] = 0.0078125f;
        flowPixels[pixel * 4 + 3] = 1.f;
        seedPixels[pixel * 4] = -0.0078125f;
        seedPixels[pixel * 4 + 1] = 0.00390625f;
        seedPixels[pixel * 4 + 3] = 1.f;
    }
    flowPixels[(10 * size + 10) * 4] = -0.25f; // finite, plausible-looking, but never written: alpha 0
    flowPixels[(10 * size + 10) * 4 + 1] = 0.25f;
    flowPixels[(10 * size + 10) * 4 + 3] = 0.f;
    flowPixels[(20 * size + 20) * 4 + 3] = std::numeric_limits<float>::quiet_NaN(); // garbage alpha
    bind(0, flow);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_FLOAT, flowPixels.data());
    bind(0, seed);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_RGBA, GL_FLOAT, seedPixels.data());
    bind(0, depth);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, size, size, GL_DEPTH_COMPONENT, GL_FLOAT, depthPixels.data());
    target(fbo, motion, outputDepth);
    glViewport(0, 0, size, size);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    glDepthMask(GL_TRUE);
    glUseProgram(exportProgram);
    bind(0, flow);
    bind(1, depth);
    bind(2, seed);
    glUniform2f(0, size, size);
    glUniform1i(5, 0);
    glUniform1i(6, 0);
    glUniform1i(7, 0);
    glUniform1i(8, 0);
    glUniform1i(9, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    float gap[4]{}, garbage[4]{}, covered[4]{};
    glReadPixels(10, 10, 1, 1, GL_RGBA, GL_FLOAT, gap);
    glReadPixels(20, 20, 1, 1, GL_RGBA, GL_FLOAT, garbage);
    glReadPixels(11, 10, 1, 1, GL_RGBA, GL_FLOAT, covered);
    std::printf("uncovered_flow gap=(%.5f,%.5f,a=%.2f) garbage_alpha=(%.5f,%.5f,a=%.2f) covered=(%.5f,%.5f,a=%.2f)\n",
        gap[0], gap[1], gap[3], garbage[0], garbage[1], garbage[3], covered[0], covered[1], covered[3]);
    require(std::abs(gap[0] - 0.015625f) < 1e-3f && std::abs(gap[1] - 0.0078125f) < 1e-3f && gap[3] < 0.01f,
        "an unwritten flow texel exports the camera seed, marked with alpha 0");
    require(std::abs(garbage[0] - 0.015625f) < 1e-3f && garbage[3] < 0.01f,
        "a flow texel with NaN alpha exports the camera seed, marked with alpha 0");
    require(std::abs(covered[0] - 0.03125f) < 2e-3f && std::abs(covered[1] - 0.015625f) < 2e-3f && covered[3] > 0.99f,
        "a covered flow texel exports its flow");
    require(glGetError() == GL_NO_ERROR, "uncovered flow fixture leaves no GL error");
    GLuint textures[]{flow, seed, depth, motion, outputDepth};
    glDeleteTextures(5, textures);
}

} // namespace

int main()
{
    require(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL video initialized");
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = SDL_CreateWindow("QVR offscreen synthesis test", 0, 0, size, size, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    require(window, "hidden desktop test window");
    SDL_GLContext context = SDL_GL_CreateContext(window);
    require(context, "desktop GL context");
    std::printf("renderer=%s version=%s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    GLuint vao = 0, fbo = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenFramebuffers(1, &fbo);
    GLuint depthProgram = program(qvr::synthesis::shaders::depth);
    GLuint flowProgram = program(qvr::synthesis::shaders::flow);
    GLuint exportProgram = program(qvr::synthesis::shaders::exportImages);
    depthFixture(fbo, depthProgram, false);
    depthFixture(fbo, depthProgram, true);
    multisampleFixture(fbo, depthProgram, false);
    multisampleFixture(fbo, depthProgram, true);
    finiteGuardFixture(fbo, depthProgram, flowProgram);
    dynamicFixture(fbo, flowProgram, exportProgram, 0, 4, 0);
    dynamicFixture(fbo, flowProgram, exportProgram, 1, -3, 0);
    dynamicFixture(fbo, flowProgram, exportProgram, 2, 0, 3);
    srgbMotionFixture(fbo, exportProgram);
    hiddenAreaFixture(fbo, depthProgram, exportProgram, false);
    hiddenAreaFixture(fbo, depthProgram, exportProgram, true);
    uncoveredFlowFixture(fbo, exportProgram);
    glDeleteProgram(depthProgram);
    glDeleteProgram(flowProgram);
    glDeleteProgram(exportProgram);
    glDeleteFramebuffers(1, &fbo);
    glDeleteVertexArrays(1, &vao);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::puts("PASS: actual GLSL compile/link, depth/MSAA/camera reprojection, dynamic stereo flow, sRGB8 round-trip, bounded NaN/Inf/out-of-range fallback to camera or neutral motion, the lenses' hidden area exported as no motion at the far depth and unwritten flow texels exported as the camera seed; no OpenXR session created");
}
