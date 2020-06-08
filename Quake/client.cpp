#include "client.hpp"
#include "msg.hpp"

[[nodiscard]] bool client_state_t::isValidWorldTextHandle(
    const WorldTextHandle wth) const noexcept
{
    return (worldTexts.size() < maxWorldTextInstances) &&
           (static_cast<WorldTextHandle>(worldTexts.size()) > wth);
}

void client_state_t::OnMsg_WorldTextHMake() noexcept
{
    const WorldTextHandle wth = MSG_ReadShort();

    cl.worldTexts.resize(wth + 1);
    assert(isValidWorldTextHandle(wth));
}

void client_state_t::OnMsg_WorldTextHSetText() noexcept
{
    const WorldTextHandle wth = MSG_ReadShort();
    assert(isValidWorldTextHandle(wth));

    const char* str = MSG_ReadString();

    if(isValidWorldTextHandle(wth))
    {
        cl.worldTexts[wth]._text = str;
    }
}

void client_state_t::OnMsg_WorldTextHSetPos() noexcept
{
    const WorldTextHandle wth = MSG_ReadShort();
    assert(isValidWorldTextHandle(wth));

    const qvec3 v = MSG_ReadVec3(cl.protocolflags);

    if(isValidWorldTextHandle(wth))
    {
        cl.worldTexts[wth]._pos = v;
    }
}

void client_state_t::OnMsg_WorldTextHSetAngles() noexcept
{
    const WorldTextHandle wth = MSG_ReadShort();
    assert(isValidWorldTextHandle(wth));

    const qvec3 v = MSG_ReadVec3(cl.protocolflags);

    if(isValidWorldTextHandle(wth))
    {
        cl.worldTexts[wth]._angles = v;
    }
}

void client_state_t::OnMsg_WorldTextHSetHAlign() noexcept
{
    const WorldTextHandle wth = MSG_ReadShort();
    assert(isValidWorldTextHandle(wth));

    const auto b = MSG_ReadByte();
    const auto v = static_cast<WorldText::HAlign>(b);

    if(isValidWorldTextHandle(wth))
    {
        cl.worldTexts[wth]._hAlign = v;
    }
}
