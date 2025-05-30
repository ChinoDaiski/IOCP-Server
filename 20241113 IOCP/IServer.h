#pragma once

#include <Windows.h>
#include <stack>
#include "concurrent_stack.h"

class CPacket;
class CSession;

// 각 행동이 초당 얼마나 일어나는지 확인하는 변수 
struct FrameStats
{
    UINT32 accept;
    UINT32 recvMessage;
    UINT32 sendMessage;

    // 메모리 풀 관련. 세션 index 값을 관리하는 concurrent_stack이 가지고 있는 메모리 풀의 사용량.
    UINT32 CurPoolCount;
    UINT32 MaxPoolCount;
};

class CLanServer { 
public:
    virtual ~CLanServer() {}

    // 서버 시작/종료
    virtual bool Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount,
        bool nagleOption, int maxSessionCount, int pendingAcceptCount) = 0;     // maxSessionCount은 최대 세션 갯수, pendingAcceptCount는 accept를 대기하는 acceptex 호출 세션 갯수
    virtual void Stop() = 0;

    // 모니터링 관련
    int GetConnectedSessionCount();   // 현재 서버에 접속된 세션의 갯수를 반환
    int GetPendingSessionCount();     // 현재 AcceptEx를 호출한 세션의 갯수를 반환

    // 세션 관리
    virtual bool Disconnect(UINT64 sessionID) = 0;  // 컨텐츠에서 먼저 삭제하고 netlib에 세션을 삭제해야할 때 호출하는 함수
    virtual void SendPacket(UINT64 sessionID, CPacket* packet) = 0; // 특정 세션에 직렬화 버퍼를 보낼 때 사용하는 함수

    virtual CSession* FetchSession(void) = 0;   // sessions에서 세션을 하나 가져오는 함수
    virtual void returnSession(CSession* pSession) = 0; // sessions에 세션을 반환하는 함수
    virtual void InitSessionInfo(CSession* pSession) = 0;   // 세션을 초기화 하는 함수

    // 이벤트 처리 (순수 가상 함수)
    virtual bool OnConnectionRequest(const std::string& ip, int port) = 0;  // 처음 백로그 큐에서 데이터를 빼서 접속 가능한지 여부를 판단하는 함수. 성공시 세션을 만들고, 아니면 그대로 continue
    virtual void OnAccept(UINT64 sessionID) = 0;    // 세션이 접속하고 컨텐츠 쪽에서 해야하는 일을 정의한 함수.
    virtual void OnRelease(UINT64 sessionID) = 0;   // 세션이 netlib쪽에서 먼저 삭제될 때 컨텐츠쪽에 알리는 함수. 만약 Disconnect 함수로 컨텐츠에서 먼저 끊었다면 continue
    virtual void OnRecv(UINT64 sessionID, CPacket* packet) = 0; // netlib에서 패킷을 받았을 때 패킷의 처리를 위해 호출하는 함수. 컨텐츠 쪽에서 정의한대로 패킷을 처리한다.
    virtual void OnError(int errorCode, const wchar_t* errorMessage) = 0;   // 에러가 있을 경우 로그를 남기기 위한 함수. 나중에 동기, 비동기 에러함수 2개로 나눌 예정

    // recv, send 를 실제로 하는 함수
    void RecvPost(CSession* pSession);
    void SendPost(CSession* pSession);
    bool AcceptPost(CSession* pSession);

    // SOCKADDR_IN 로 부터 IP, Port를 추출하는 함수
    void extractIPPort(const SOCKADDR_IN& clientAddr, std::string& connectedIP, UINT16& connectedPort);

    // 억셉트/워커 스레드, static 함수이기 떄문에 인자로 객체를 집어넣음. 함수 자체는 일종의 틀로 보면 됨.
    static unsigned int WINAPI WorkerThread(void* pArg);
    static unsigned int WINAPI AcceptThread(void* pArg);

    // 모니터링 관련
    const FrameStats& GetStatusTPS(void) const { return statusTPS; }
    void ClearTPSValue(void);   // TPS 측정관련 변수들을 모두 0으로 초기화 하면서 statusTPS에 값을 넣는 함수.
    UINT32 GetDisconnectedSessionCnt(void) const { return disconnectedSessionCnt; }

    // AcceptEx 시도 여부 확인 함수
    bool IsAcceptExUnderLimit(void);

private:
    // 각 행동이 초당 얼마나 일어나는지 확인하는 변수 
    UINT32 acceptTPS = 0;
    UINT32 recvMessageTPS = 0;
    UINT32 sendMessageTPS = 0;

    FrameStats statusTPS;

protected:
    HANDLE hIOCP;  // IOCP 핸들
    SOCKET listenSocket; // listen 소켓
    CSession* sessions;  // 세션 관리
    
    UINT8 threadCnt;    // 생성할 스레드 갯수
    HANDLE* hThreads;   // 실제 스레드의 핸들값. 이 값으로 서버 종료시 스레드의 종료 여부를 판단함. WaitForMultipleObjects 호출에 사용.

protected:
    concurrent_stack<UINT16> stSessionIndex; // 스레드 인덱스들을 보관하는 스택
    CRITICAL_SECTION cs_sessionID;

protected:
    UINT64 uniqueSessionID = 0; // 세션이 접속할 때 마다 1씩 증가하는 변수
    UINT16 maxConnections = 0;  // 최대 동접 세션 수

protected:
    UINT32 curSessionCnt = 0;   // 현재 접속중인 세션 수
    UINT32 disconnectedSessionCnt = 0;  // 중간에 끊어진 세션수

protected:
    UINT32 maxPendingSessionCnt;     // 최대 Accept 예약한 세션 갯수
    UINT32 curPendingSessionCnt;     // 현재 Accept 예약한 세션 갯수
};