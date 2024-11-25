#pragma once

class CPacket;

class IContent
{
public:
    virtual ~IContent() = default;

    // 서버가 호출하는 함수
public:
    // 클라이언트로부터 데이터를 수신했을 때 호출되는 함수
    virtual void OnRecv(UINT64 sessionID, CPacket* pPacket) = 0;

    // 서버가 세션을 받고, 세션과 연결된 객체를 만들기 위해 컨텐츠에서 호출해야하는 함수
    virtual void OnAccept(UINT64 sessionID) = 0;
};