#pragma once

class CPacket;

class IServer
{
public:
    virtual ~IServer() = default;

    // 컨텐츠가 호출하는 함수
public:
    // 콘텐츠에 보낼 것이 생겼을 때 호출되는 함수
    virtual void SendPacket(UINT64 sessionID, CPacket* pPacket) = 0;
};
