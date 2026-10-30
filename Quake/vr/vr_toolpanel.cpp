// vr_toolpanel.cpp -- the in-world tool panels' shared pieces (vr_toolpanel.hpp).

#include "vr_toolpanel.hpp"

#include "vr_engine.hpp"

#include "Zancle/Math/MinMax.hpp"

#include <string.h>

namespace qvr::toolpanel
{

void say(za::String& line, double& until, const char* tool, const za::String& text, double seconds)
{
    line = text;
    until = realtime + seconds;
    Con_Printf("%s: %s\n", tool, text.cStr());
}

void fill(const Rect& r, const glm::vec4& c)
{
    const float rgb[3]{c.r, c.g, c.b};
    Draw_FillEx(r.x0, r.y0, r.w(), r.h(), rgb, c.a);
}

void text(float x, float y, float dim, const char* s, bool gold)
{
    char buf[96];
    int n = 0;
    for(; s[n] && n < static_cast<int>(sizeof(buf)) - 1; n++)
    {
        buf[n] = gold ? static_cast<char>(static_cast<unsigned char>(s[n]) | 0x80) : s[n];
    }
    buf[n] = '\0';
    Draw_StringEx(x, y, dim, buf);
}

void textFit(float cx, float y, float width, float dim, const char* s, bool gold)
{
    const float len = static_cast<float>(strlen(s));
    const float d = len > 0.f ? za::min(dim, width / len) : dim;
    text(cx - len * d * 0.5f, y + (dim - d) * 0.5f, d, s, gold);
}

} // namespace qvr::toolpanel
