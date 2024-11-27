#pragma once

#include <Windows.h>
#include <unordered_map>

class CPacket;
class CSession;

class IServer
{
public:
    virtual ~IServer() = default;

    // 컨텐츠가 호출하는 함수
public:
    // 콘텐츠에 보낼 것이 생겼을 때 호출되는 함수
    virtual void SendPacket(UINT64 sessionID, CPacket* pPacket) = 0;
};


class CLanServer { 
public:
    virtual ~CLanServer() {}

    // 서버 시작/종료
    virtual bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount,
        bool nagleOption, UINT16 maxSessionCount) = 0;
    virtual void Stop() = 0;

    // 세션 관리
    virtual int GetSessionCount() const = 0;
    virtual bool Disconnect(UINT32 sessionID) = 0;
    virtual bool SendPacket(UINT32 sessionID, CPacket* packet) = 0;


    virtual CSession* FetchSession(void) = 0;   // sessions에서 세션을 하나 가져오는 함수
    virtual void returnSession(CSession* pSession) = 0; // sessions에 세션을 반환하는 함수
    virtual CSession* InitSessionInfo(CSession* pSession) = 0;



    // 이벤트 처리 (순수 가상 함수)
    virtual bool OnConnectionRequest(const std::string& ip, int port) = 0;
    virtual void OnAccept(UINT32 sessionID) = 0;
    virtual void OnRelease(UINT32 sessionID) = 0;
    virtual void OnRecv(UINT32 sessionID, CPacket* packet) = 0;
    virtual void OnError(int errorCode, const wchar_t* errorMessage) = 0;

    // 모니터링
    virtual int GetAcceptTPS() const = 0;
    virtual int GetRecvMessageTPS() const = 0;
    virtual int GetSendMessageTPS() const = 0;

    void RecvPost(CSession* pSession);
    void SendPost(CSession* pSession);

    // 워커 스레드, 인자로 
    static unsigned int WINAPI WorkerThread(void* pArg);
    static unsigned int WINAPI AcceptThread(void* pArg);

protected:
    HANDLE hIOCP;  // IOCP 핸들
    SOCKET listenSocket; // listen 소켓
    CSession* sessions;  // 세션 관리
    
    UINT8 threadCnt;    // 생성할 스레드 갯수
    HANDLE* hThreads;
};