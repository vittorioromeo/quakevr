#include "server.hpp"
#include "msg.hpp"

#include <cassert>

[[nodiscard]] bool server_t::isValidWorldTextHandle(
    const WorldTextHandle wth) const noexcept
{
    return static_cast<int>(wth) < static_cast<int>(worldTexts.size());
}

[[nodiscard]] bool server_t::hasAnyFreeWorldTextHandle() const noexcept
{
    return !freeWorldTextHandles.empty();
}

[[nodiscard]] WorldTextHandle server_t::makeWorldTextHandle() noexcept
{
    assert(hasAnyFreeWorldTextHandle());

    const WorldTextHandle wth = freeWorldTextHandles.back();
    freeWorldTextHandles.pop_back();
    worldTexts.resize(wth + 1);

    assert(isValidWorldTextHandle(wth));
    return wth;
}

[[nodiscard]] WorldText& server_t::getWorldText(
    const WorldTextHandle wth) noexcept
{
    assert(isValidWorldTextHandle(wth));
    return worldTexts[wth];
}

void server_t::initializeWorldTexts()
{
    worldTexts.clear();
    for(std::size_t i = 0; i < maxWorldTextInstances; ++i)
    {
        freeWorldTextHandles.emplace_back(maxWorldTextInstances - 1 - i);
    }
}

void server_t::SendMsg_WorldTextHMake(
    client_t& client, const WorldTextHandle wth) noexcept
{
    MSG_WriteByte(&client.message, svc_worldtext_hmake);
    MSG_WriteShort(&client.message, wth);
}

void server_t::SendMsg_WorldTextHSetText(client_t& client,
    const WorldTextHandle wth, const char* const text) noexcept
{
    MSG_WriteByte(&client.message, svc_worldtext_hsettext);
    MSG_WriteShort(&client.message, wth);
    MSG_WriteString(&client.message, text);
}

void server_t::SendMsg_WorldTextHSetPos(
    client_t& client, const WorldTextHandle wth, const qvec3& pos) noexcept
{
    MSG_WriteByte(&client.message, svc_worldtext_hsetpos);
    MSG_WriteShort(&client.message, wth);
    MSG_WriteVec3(&client.message, pos, sv.protocolflags);
}

void server_t::SendMsg_WorldTextHSetAngles(
    client_t& client, const WorldTextHandle wth, const qvec3& angles) noexcept
{
    MSG_WriteByte(&client.message, svc_worldtext_hsetangles);
    MSG_WriteShort(&client.message, wth);
    MSG_WriteVec3(&client.message, angles, sv.protocolflags);
}

void server_t::SendMsg_WorldTextHSetHAlign(client_t& client,
    const WorldTextHandle wth, const WorldText::HAlign hAlign) noexcept
{
    MSG_WriteByte(&client.message, svc_worldtext_hsethalign);
    MSG_WriteShort(&client.message, wth);
    MSG_WriteByte(&client.message, static_cast<int>(hAlign));
}
