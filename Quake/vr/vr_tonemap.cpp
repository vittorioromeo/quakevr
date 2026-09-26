// vr_tonemap.cpp -- the eyes' tone curve, colour grade and last dither (see vr_tonemap.h), and vr_eyeshot.

#include "vr_tonemap.hpp"
#include "vr_cvars.hpp"
#include "vr_engine.hpp"
#include "vr_stereo.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace qvr::tonemap
{
namespace
{

// The curve (QvrTonemap): unchanged up to the knee, 1 at the white point. Quake's brightest colours (fullbright
// lamps, lit sky) are about 1 and most of a frame is under 0.6, so a knee of 0.8 leaves nearly all of it as tuned;
// a white point of 4 (the brightest lightmaps doubled, coloured dynamic lights up to 4) keeps what they add apart.
constexpr float knee = 0.8f;
constexpr float white = 4.f;
// What the world and models may write into the float scene (SceneTone.x): past the white point is white anyway.
constexpr float sceneMax = 8.f;

// The grades: quakevr/gfx/vr/grade_<name>.png (Misc/quakevr/make_grades.py), loaded when first used.
struct Grade
{
    const char* name;
    GLuint tex;
    bool failed;
};
Grade grades[] = {{"film", 0, false}, {"cold", 0, false}, {"warm", 0, false}, {"moss", 0, false}, {"violet", 0, false}};
enum GradeIndex
{
    Film,
    Cold,
    Warm,
    Moss,
    Violet
};

// vr_grade 4: by episode (the map's name: e1m* ... e4m*), others (start, end, mission packs, mods) Quake VR's.
[[nodiscard]] int episodeGrade()
{
    const char* m = cl.mapname;
    if(m[0] == 'e' && m[1] >= '1' && m[1] <= '4' && m[2] == 'm')
    {
        constexpr int byEpisode[] = {Cold, Moss, Warm, Violet};
        return byEpisode[m[1] - '1'];
    }
    return Film;
}

[[nodiscard]] GLuint gradeTexture(Grade& g)
{
    if(g.tex || g.failed)
    {
        return g.tex;
    }
    const int mark = Hunk_LowMark();
    int w = 0, h = 0;
    enum srcformat fmt;
    const byte* data = Image_LoadImage(va("gfx/vr/grade_%s", g.name), &w, &h, &fmt);
    if(!data || h < 2 || w != h * h)
    {
        Hunk_FreeToLowMark(mark);
        Con_Warning("VR: colour grade gfx/vr/grade_%s.png missing or not an N*N x N strip\n", g.name);
        g.failed = true;
        return 0;
    }
    // The strip's texel (x = b * n + r, y = g) to the table's (r, g, b).
    const int n = h;
    std::vector<byte> texels(static_cast<std::size_t>(n) * n * n * 4);
    for(int b = 0; b < n; b++)
    {
        for(int gr = 0; gr < n; gr++)
        {
            for(int r = 0; r < n; r++)
            {
                const byte* src = data + (static_cast<std::size_t>(gr) * w + b * n + r) * 4;
                byte* dst = texels.data() + ((static_cast<std::size_t>(b) * n + gr) * n + r) * 4;
                std::memcpy(dst, src, 4);
            }
        }
    }
    Hunk_FreeToLowMark(mark);

    glGenTextures(1, &g.tex);
    GL_BindNative(GL_TEXTURE3, GL_TEXTURE_3D, g.tex);
    GL_ObjectLabelFunc(GL_TEXTURE, g.tex, -1, va("vr grade %s", g.name));
    GL_TexImage3DFunc(GL_TEXTURE_3D, 0, GL_RGBA8, n, n, n, 0, GL_RGBA, GL_UNSIGNED_BYTE, texels.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    return g.tex;
}

// vr_eyeshot: a number for this session's shots.
int eyeshotCount = 0;

void writePfm(const std::string& path, const std::vector<float>& rgb, int width, int height)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    if(!f)
    {
        return;
    }
    std::fprintf(f, "PF\n%d %d\n-1.0\n", width, height); // bottom row first, as GL reads
    std::fwrite(rgb.data(), sizeof(float), rgb.size(), f);
    std::fclose(f);
}

} // namespace

bool active()
{
    return vr_tonemap.value > 0.f;
}

unsigned sceneFormat()
{
    if(!active())
    {
        return GL_RGB10_A2;
    }
    return vr_tonemap.value >= 2.f ? GL_R11F_G11F_B10F : GL_RGBA16F; // 2: the cheaper float format (coarser)
}

glm::vec4 bind(unsigned unit)
{
    glm::vec4 p{0.f, knee, white, 0.f};
    if(active())
    {
        p.x = std::clamp(vr_exposure.value, 0.1f, 4.f);
    }
    const int choice = static_cast<int>(vr_grade.value);
    const float strength = std::clamp(vr_grade_strength.value, 0.f, 1.f);
    if(choice >= 1 && choice <= 4 && strength > 0.f)
    {
        constexpr int byChoice[] = {Film, Cold, Warm};
        const GLuint tex = gradeTexture(grades[choice == 4 ? episodeGrade() : byChoice[choice - 1]]);
        if(tex)
        {
            GL_BindNative(GL_TEXTURE0 + unit, GL_TEXTURE_3D, tex);
            p.w = strength;
        }
    }
    return p;
}

void eyeshot(int eye, unsigned imageFbo, unsigned sceneFbo, int width, int height)
{
    if(vr_eyeshot.value <= 0.f)
    {
        return;
    }
    const std::string dir = std::string{com_gamedir} + "/eyeshots";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    const char* side = eye == 0 ? "L" : "R";
    const std::string base = va("eyeshots/%s_%03d_%s", cl.mapname[0] ? cl.mapname : "none", eyeshotCount, side);

    std::vector<byte> pixels(static_cast<std::size_t>(width) * height * 3);
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, imageFbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    Image_WritePNG((base + ".png").c_str(), pixels.data(), width, height, 24, false);

    if(vr_eyeshot.value >= 2.f)
    {
        std::vector<float> scene(static_cast<std::size_t>(width) * height * 3);
        GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, sceneFbo);
        glReadPixels(0, 0, width, height, GL_RGB, GL_FLOAT, scene.data());
        writePfm(std::string{com_gamedir} + "/" + base + ".pfm", scene, width, height);
    }
    GL_BindFramebufferFunc(GL_READ_FRAMEBUFFER, 0);
    Con_Printf("Wrote %s.png\n", base.c_str());
    if(eye == 1)
    {
        ++eyeshotCount;
        Cvar_SetQuick(&vr_eyeshot, "0");
    }
}

} // namespace qvr::tonemap

using namespace qvr;

extern "C" float VR_SceneTone(void)
{
    return stereo::isRenderingEye() && tonemap::active() ? tonemap::sceneMax : 1.f;
}

extern "C" float VR_SceneDither(float dither)
{
    return stereo::isRenderingEye() && vr_dither.value > 0.f ? 0.f : dither;
}

// Uniforms 7 (Tone) and 8 (Dither) of the non-palettized post-process; zero for the window.
extern "C" void VR_PostProcessTone(void)
{
    if(!stereo::isRenderingEye())
    {
        GL_Uniform4fFunc(7, 0.f, 0.f, 0.f, 0.f);
        GL_Uniform4fFunc(8, 0.f, 0.f, 0.f, 0.f);
        return;
    }
    const glm::vec4 tone = tonemap::bind(3);
    GL_Uniform4fFunc(7, tone.x, tone.y, tone.z, tone.w);
    const int mode = static_cast<int>(vr_dither.value);
    const float frame = mode == 2 ? static_cast<float>(host_framecount % 64) : 0.f;
    const float otherEye = mode != 3 && stereo::eye() == 1 ? 1.f : 0.f;
    GL_Uniform4fFunc(8, mode > 0 ? 1.f / 255.f : 0.f, otherEye, frame, 0.f);
}
