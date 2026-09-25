// vr_worldtext.cpp -- see vr_worldtext.hpp.

#include "vr_worldtext.hpp"
#include "vr_engine.hpp"
#include "vr_protocol.hpp"

#include <cstring>
#include <utility>

using namespace qvr::protocol;

namespace qvr::worldtext
{
namespace
{

constexpr int maxWorldTexts = 4096;

constexpr std::size_t maxFloatTexts = 256;

std::vector<WorldText> serverTexts;
std::vector<WorldText> clientTextList;
std::vector<FloatText> clientFloatTextList;

[[nodiscard]] WorldText& serverText(int handle)
{
    if(handle < 0 || handle >= static_cast<int>(serverTexts.size()))
    {
        PR_RunError("invalid world text handle %d", handle);
    }

    return serverTexts[handle];
}

// Changes made while the map is loading are not broadcast: spawning clients get the full
// list from serverWriteAll instead.
[[nodiscard]] sizebuf_t* broadcast()
{
    return sv.state == ss_active ? &sv.reliable_datagram : nullptr;
}

void beginMessage(sizebuf_t* msg, int subcmd, int handle)
{
    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, subcmd);
    MSG_WriteShort(msg, handle);
}

void writeText(sizebuf_t* msg, int handle, const WorldText& wt)
{
    beginMessage(msg, QVR_SVC_WORLDTEXT_TEXT, handle);
    MSG_WriteString(msg, wt.text.c_str());
}

void writePos(sizebuf_t* msg, int handle, const WorldText& wt, unsigned int protocolflags)
{
    beginMessage(msg, QVR_SVC_WORLDTEXT_POS, handle);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(msg, wt.pos[i], protocolflags);
    }
}

void writeAngles(sizebuf_t* msg, int handle, const WorldText& wt)
{
    beginMessage(msg, QVR_SVC_WORLDTEXT_ANGLES, handle);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteFloat(msg, wt.angles[i]);
    }
}

void writeHAlign(sizebuf_t* msg, int handle, const WorldText& wt)
{
    beginMessage(msg, QVR_SVC_WORLDTEXT_HALIGN, handle);
    MSG_WriteByte(msg, static_cast<int>(wt.hAlign));
}

void writeScale(sizebuf_t* msg, int handle, const WorldText& wt)
{
    beginMessage(msg, QVR_SVC_WORLDTEXT_SCALE, handle);
    MSG_WriteFloat(msg, wt.scale);
}

void writeAll(sizebuf_t* msg, const std::vector<WorldText>& texts, unsigned int protocolflags)
{
    for(int handle = 0; handle < static_cast<int>(texts.size()); handle++)
    {
        const WorldText& wt = texts[handle];
        beginMessage(msg, QVR_SVC_WORLDTEXT_MAKE, handle);
        writeText(msg, handle, wt);
        writePos(msg, handle, wt, protocolflags);
        writeAngles(msg, handle, wt);
        writeHAlign(msg, handle, wt);
        writeScale(msg, handle, wt);
    }
}

[[nodiscard]] WorldText& clientText(int handle)
{
    if(handle < 0 || handle >= maxWorldTexts)
    {
        Host_Error("svc_quakevr: bad world text handle %d", handle);
    }

    if(handle >= static_cast<int>(clientTextList.size()))
    {
        clientTextList.resize(handle + 1);
    }

    return clientTextList[handle];
}

} // namespace

void serverReset()
{
    serverTexts.clear();
}

int serverMake()
{
    if(serverTexts.size() >= maxWorldTexts)
    {
        PR_RunError("too many world texts (max %d)", maxWorldTexts);
    }

    serverTexts.emplace_back();
    const int handle = static_cast<int>(serverTexts.size() - 1);
    if(sizebuf_t* msg = broadcast())
    {
        beginMessage(msg, QVR_SVC_WORLDTEXT_MAKE, handle);
    }
    return handle;
}

void serverSetText(int handle, const char* text)
{
    WorldText& wt = serverText(handle);
    wt.text = text;
    if(sizebuf_t* msg = broadcast())
    {
        writeText(msg, handle, wt);
    }
}

void serverSetPos(int handle, const glm::vec3& pos)
{
    WorldText& wt = serverText(handle);
    wt.pos = pos;
    if(sizebuf_t* msg = broadcast())
    {
        writePos(msg, handle, wt, sv.protocolflags);
    }
}

void serverSetAngles(int handle, const glm::vec3& angles)
{
    WorldText& wt = serverText(handle);
    wt.angles = angles;
    if(sizebuf_t* msg = broadcast())
    {
        writeAngles(msg, handle, wt);
    }
}

void serverSetHAlign(int handle, HAlign hAlign)
{
    WorldText& wt = serverText(handle);
    wt.hAlign = hAlign;
    if(sizebuf_t* msg = broadcast())
    {
        writeHAlign(msg, handle, wt);
    }
}

void serverSetScale(int handle, float scale)
{
    WorldText& wt = serverText(handle);
    wt.scale = scale;
    if(sizebuf_t* msg = broadcast())
    {
        writeScale(msg, handle, wt);
    }
}

void serverWriteAll(sizebuf_t* msg)
{
    writeAll(msg, serverTexts, sv.protocolflags);
}

// Unreliable, like particles: a number lost now and then does not matter.
void serverFloatText(const glm::vec3& pos, const char* text, const glm::vec3& color, float scale)
{
    sizebuf_t* msg = &sv.datagram;
    const int size = 2 + 3 * 4 + 4 + static_cast<int>(strlen(text)) + 1;
    if(sv.state != ss_active || msg->cursize + size > msg->maxsize)
    {
        return;
    }

    MSG_WriteByte(msg, svc_quakevr);
    MSG_WriteByte(msg, QVR_SVC_FLOATTEXT);
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteCoord(msg, pos[i], sv.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        MSG_WriteByte(msg, CLAMP(0, static_cast<int>(color[i] * 255.f + 0.5f), 255));
    }
    MSG_WriteByte(msg, CLAMP(1, static_cast<int>(scale * 32.f + 0.5f), 255));
    MSG_WriteString(msg, text);
}

void clientWriteAll(sizebuf_t* msg)
{
    writeAll(msg, clientTextList, cl.protocolflags);
}

void clientReset()
{
    clientTextList.clear();
    clientFloatTextList.clear();
}

void clientParseFloatText()
{
    FloatText ft;
    for(int i = 0; i < 3; i++)
    {
        ft.pos[i] = MSG_ReadCoord(cl.protocolflags);
    }
    for(int i = 0; i < 3; i++)
    {
        ft.color[i] = static_cast<float>(MSG_ReadByte()) / 255.f;
    }
    ft.scale = static_cast<float>(MSG_ReadByte()) / 32.f;
    ft.text = MSG_ReadString();
    ft.start = cl.time;

    if(clientFloatTextList.size() < maxFloatTexts)
    {
        clientFloatTextList.push_back(std::move(ft));
    }
}

const std::vector<FloatText>& clientFloatTexts(double now)
{
    // Done, or from before a jump back in time (a demo restarted).
    std::erase_if(clientFloatTextList,
        [now](const FloatText& ft) { return now > ft.start + floatTextLife || now < ft.start - 1.0; });
    return clientFloatTextList;
}

void clientParse(int subcmd)
{
    WorldText& wt = clientText(MSG_ReadShort());

    switch(subcmd)
    {
        case QVR_SVC_WORLDTEXT_MAKE: wt = WorldText{}; break;
        case QVR_SVC_WORLDTEXT_TEXT: wt.text = MSG_ReadString(); break;
        case QVR_SVC_WORLDTEXT_POS:
            for(int i = 0; i < 3; i++)
            {
                wt.pos[i] = MSG_ReadCoord(cl.protocolflags);
            }
            break;
        case QVR_SVC_WORLDTEXT_ANGLES:
            for(int i = 0; i < 3; i++)
            {
                wt.angles[i] = MSG_ReadFloat();
            }
            break;
        case QVR_SVC_WORLDTEXT_HALIGN:
            wt.hAlign = static_cast<HAlign>(MSG_ReadByte());
            break;
        case QVR_SVC_WORLDTEXT_SCALE: wt.scale = MSG_ReadFloat(); break;
        default: Host_Error("svc_quakevr: bad world text command %d", subcmd);
    }
}

const std::vector<WorldText>& clientTexts()
{
    return clientTextList;
}

} // namespace qvr::worldtext
