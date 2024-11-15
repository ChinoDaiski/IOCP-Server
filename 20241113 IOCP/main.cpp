#define _WINSOCK_DEPRECATED_NO_WARNINGS // 최신 VC++ 컴파일 시 경고 방지



#include <iostream>
#define NOMINMAX
#include <WinSock2.h>
#pragma comment(lib, "ws2_32")

#include "RingBuffer.h"

#include <process.h>

#include <unordered_map>

#include "Packet.h"


#pragma pack(push, 1)   

typedef struct _tagPACKET_HEADER {
    UINT16 byLen;   // 패킷 길이
}PACKET_HEADER;

#pragma pack(pop)

#define dfNETWORK_PACKET_CODE 0x89
#define dfMAX_PACKET_SIZE	10


#define SERVERPORT 6000


// 전역 배열
//스레드들이 공유하는 전역 카운터를 하나 둬서
//
//스레드들이 각자 전역 카운터를 interlock으로 1씩 증가시키면서 배열상의 index를 얻음
//
//카운터랑 index를 둘다 기록?



// 소켓 정보 저장을 위한 구조체
class CSession
{
public:
    CSession(){
        InitializeCriticalSection(&cs);
        bSending = false;
    }
    ~CSession(){
        DeleteCriticalSection(&cs);
    }

public:
    CRITICAL_SECTION cs;

    SOCKET sock;

    USHORT port;
    char IP[16];

    OVERLAPPED overlappedRecv;
    OVERLAPPED overlappedSend;

    CRingBuffer recvBuffer;
    CRingBuffer sendBuffer;

    WSABUF wsaRecvBuf[2];
    WSABUF wsaSendBuf[2];

    UINT32 IOcount;

    bool bSending;

    UINT32 iSessionID;
};

class CPlayer
{
public:
    CPlayer() {}
    ~CPlayer() {}

public:
    UINT32 m_playerID;
};



HANDLE g_hIOCP;
SOCKET g_listen_sock;
UINT32 g_SessionID = 0;


std::unordered_map<UINT32, CSession*> g_sessionMap; // key로 SessionID, value로 세션 포인터를 받아 관리하는 전역 세션맵
std::unordered_map<UINT32, CPlayer*> g_playerMap; // key로 SessionID, value로 세션 포인터를 받아 관리하는 전역 세션맵

CRITICAL_SECTION g_sessionMap_cs;


// 작업자 스레드 함수
unsigned int WINAPI WorkerThread(LPVOID arg);
unsigned int WINAPI AcceptThread(void* pArg);


void RecvPost(CSession* pSession);
void SendPost(CSession* pSession);
void SendPacket(int playerID, CPacket* pPacket);
void OnRecv(int sessionID, CPacket* pPacket);
bool ProcessMessage(CSession* pSession);


// 수동리셋 이벤트 생성
HANDLE g_hExitThreadEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

int main(void)
{
    int retval;

    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // CPU 개수 확인
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int ProcessorNum = si.dwNumberOfProcessors;

    // 입출력 완료 포트 생성
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
    if (g_hIOCP == NULL) return 1;

    // socket()
    g_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_sock == INVALID_SOCKET)
        std::cout << "INVALID_SOCKET : listen_sock\n";


    // recvbuf 크기 설정 (0으로 설정)
    int recvBufSize = 0;
    if (setsockopt(g_listen_sock, SOL_SOCKET, SO_RCVBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << std::endl;
        closesocket(g_listen_sock);
        WSACleanup();
        return 1;
    }


    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = bind(g_listen_sock, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        std::cout << "bind()\n";

    // listen()
    retval = listen(g_listen_sock, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        std::cout << "listen()\n";








    InitializeCriticalSection(&g_sessionMap_cs);






    HANDLE hAcceptThread;		// 접속요청, 끊기에 대한 처리	
    HANDLE hWorkerThread;		// 워커 스레드

    DWORD dwThreadID;

    hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);








    //WCHAR ControlKey;

    //------------------------------------------------
    // 종료 컨트롤...
    //------------------------------------------------
    while (1)
    {
        //ControlKey = _getwch();
        //if (ControlKey == L'q' || ControlKey == L'Q')
        //{
        //    //------------------------------------------------
        //    // 종료처리
        //    //------------------------------------------------

        //    SetEvent(g_hExitThreadEvent);
        //    break;
        //}
    }











    ////------------------------------------------------
    //// 스레드 종료 대기
    ////------------------------------------------------
    //HANDLE hThread[] = { hAcceptThread, hWorkerThread};
    //DWORD retVal = WaitForMultipleObjects(2, hThread, TRUE, INFINITE);
    //DWORD retError = WSAGetLastError();





    DeleteCriticalSection(&g_sessionMap_cs);






    ////------------------------------------------------
    //// 디버깅용 코드  스레드 정상종료 확인.
    ////------------------------------------------------
    //DWORD ExitCode;

    //wprintf(L"\n\n--- THREAD CHECK LOG -----------------------------\n\n");

    //GetExitCodeThread(hAcceptThread, &ExitCode);
    //if (ExitCode != 0)
    //    wprintf(L"error - Accept Thread not exit\n");

    //GetExitCodeThread(hWorkerThread, &ExitCode);
    //if (ExitCode != 0)
    //    wprintf(L"error - Accept Thread not exit\n");


    // 윈속 종료
    WSACleanup();
    return 0;
}


// recv 링버퍼에 있는 데이터를 패킷 단위로 꺼내기
bool ProcessMessage(CSession* pSession)
{
    // 1. RecvQ에 최소한의 사이즈가 있는지 확인. 조건은 [ 헤더 사이즈 이상의 데이터가 있는지 확인 ]하는 것.
    if (pSession->recvBuffer.GetUseSize() < 10)
        return false;

    //// 2. RecvQ에서 PACKET_HEADER 정보 Peek
    //PACKET_HEADER header;
    //int headerSize = sizeof(header);
    //
    //int retVal = pSession->recvBuffer.Peek(reinterpret_cast<char*>(&header), headerSize);

    //// 4. 헤더의 len값과 RecvQ의 데이터 사이즈 비교
    //if (sizeof(PACKET_HEADER) > pSession->recvBuffer.GetUseSize())
    //    return false;

    // 5. Peek 했던 헤더를 RecvQ에서 지운다.
    //pSession->recvBuffer.MoveFront(sizeof(PACKET_HEADER));

    // 6. RecvQ에서 header의 len 크기만큼 임시 패킷 버퍼를 뽑는다.
    CPacket Packet;
    int recvQDeqRetVal = pSession->recvBuffer.Dequeue(Packet.GetBufferPtr(), 10);
    Packet.MoveWritePos(10);

    if (recvQDeqRetVal != 10)
    {
        DebugBreak();
    }

    // OnRecv 호출
    OnRecv(pSession->iSessionID, &Packet);

    return true;
}


// 컨텐츠 함수
void OnRecv(int sessionID, CPacket* pPacket)
{
    
    // 여기서 컨텐츠 처리
    /*
        (*iter).second 가 플레이어이며 패킷의 타입에 따라 플레이어 조정
    */

    // 중간중간 send해야할 데이터가 있으면 SendPacket함수 호출

    // echo니깐 패킷을 sendbuffer에 바로 넣음


    SendPacket(sessionID, pPacket);
}

void SendPacket(int playerID, CPacket* pPacket)
{
    // 인자로 받은 id를 사용해 세션 검색
    EnterCriticalSection(&g_sessionMap_cs);
    auto iter = g_sessionMap.find(playerID);
    LeaveCriticalSection(&g_sessionMap_cs);

    if (iter == g_sessionMap.end())
        return;
    
    // 플레이어 ID와 세션 ID가 동일하기에 세션Map에서 같은 ID를 가진 세션을 찾아 SendRingBuffer에 패킷에 있는 데이터를 넣음.
    int retVal = iter->second->sendBuffer.Enqueue(pPacket->GetBufferPtr(), pPacket->GetDataSize());
    if (retVal == 0)
    {
        __debugbreak();
    }


    SendPost(iter->second);
}


void ReleaseSession(CSession* pSession)
{
    UINT32 sessionID = pSession->iSessionID;

    int iCnt;

    // 세션 맵에서 삭제
    EnterCriticalSection(&g_sessionMap_cs);
    EnterCriticalSection(&pSession->cs);
    LeaveCriticalSection(&pSession->cs);
    iCnt = g_sessionMap.erase(sessionID);
    LeaveCriticalSection(&g_sessionMap_cs);
    closesocket(pSession->sock);
    delete pSession;
    // 소켓 제거

    // 세션 객체 삭제
}


// 작업자 스레드 함수
unsigned int WINAPI WorkerThread(LPVOID arg)
{
    int retval;

    while (1) {
        // 비동기 입출력 완료 기다리기
        DWORD cbTransferred{};
        //SOCKET client_sock;
        CSession* pSession = nullptr;

        // GQCS의 반환값은 성공하면 TRUE, 아니면 FALSE.
        OVERLAPPED* pOverlapped;
        memset(&pOverlapped, 0, sizeof(pOverlapped));

        // GQCS로 완료통지를 가져옴.
        retval = GetQueuedCompletionStatus(g_hIOCP, &cbTransferred,
            (PULONG_PTR)&pSession, &pOverlapped, INFINITE);


        EnterCriticalSection(&pSession->cs);
        

        int wsaError = WSAGetLastError();
        int Error = GetLastError();

        //// 종료 처리
        //DWORD dwError = WaitForSingleObject(g_hExitThreadEvent, 0);
        //if (dwError != WAIT_TIMEOUT)
        //    break;

        // 예외 처리
        if (retval == FALSE || cbTransferred == 0)
        {
            ;
        }

        // recv 완료 처리
        else if (pOverlapped == &pSession->overlappedRecv) {

            // recv 호출시 wsaBuf에 recvBuffer의 포인터를 넣어놨기에 이미 데이터가 저장되어 있으므로 rear를 cbTransferred 만큼 옮겨서 링버퍼가 올바르게 작동하도록 조정
            pSession->recvBuffer.MoveRear(cbTransferred);

            // recv 링버퍼에 있는 데이터를 패킷 단위로 꺼내기
            while (ProcessMessage(pSession));
               
            // Recv를 걸어두어 비동기 입출력 시작
            RecvPost(pSession);
        }

        // send 완료 처리
        else if (pOverlapped == &pSession->overlappedSend) {
            // sendBuffer에서 전송 완료된 만큼 제거
            pSession->sendBuffer.MoveFront(cbTransferred);
            pSession->bSending = false;

            // 링버퍼가 비어 있지 않으면 send. 
            if (pSession->sendBuffer.GetUseSize() != 0)
                SendPost(pSession);
        }

        LeaveCriticalSection(&pSession->cs);

        // 완료 통지 처리가 끝났기에 카운트 1감소
        if (InterlockedDecrement(&pSession->IOcount) == 0)
        {
            ReleaseSession(pSession);
        }
    }

    return 0;
}

unsigned int WINAPI AcceptThread(void* pArg)
{
    // 데이터 통신에 사용할 변수
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;

    while (true)
    {
        // Accept
        int addrlen = sizeof(clientaddr);
        client_sock = accept(g_listen_sock, (SOCKADDR*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            std::cout << "accept()\n";
            break;
        }

        // 세션 생성
        
        // 소켓 정보 구조체 할당
        CSession* pSession = new CSession;

        if (pSession == NULL)
            break;
        
        // 소켓과 입출력 완료 포트 연결 - 새로 연견될 소켓을 key를 해당 소켓과 연결된 세션 구조체의 포인터로 하여 IOCP와 연결한다.
        CreateIoCompletionPort((HANDLE)client_sock, g_hIOCP, (ULONG_PTR)pSession, 0);



        // IP, PORT 값 추가
        memcpy(pSession->IP, inet_ntoa(clientaddr.sin_addr), sizeof(pSession->IP));
        pSession->port = ntohs(clientaddr.sin_port);

        ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
        ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));

        ZeroMemory(pSession->wsaRecvBuf, sizeof(WSABUF) * 2);

        // 링버퍼를 사용하기에 directEnq 값을 생각해서 버퍼를 만들어야한다.
        pSession->wsaRecvBuf[0].buf = pSession->recvBuffer.GetFrontBufferPtr();
        pSession->wsaRecvBuf[0].len = pSession->recvBuffer.DirectEnqueueSize();

        pSession->wsaRecvBuf[1].buf = pSession->recvBuffer.GetBufferPtr();
        pSession->wsaRecvBuf[1].len = pSession->recvBuffer.GetBufferCapacity() - pSession->recvBuffer.DirectEnqueueSize();

        pSession->sock = client_sock;
        pSession->IOcount = 0;


        // 세션 맵에 관리 대상 추가, 컨텐츠에서 g_SessionID로 세션을 검색할 것이기에 key값으로 추가
        EnterCriticalSection(&g_sessionMap_cs);
        g_sessionMap.emplace(g_SessionID, pSession);
        LeaveCriticalSection(&g_sessionMap_cs);
        pSession->iSessionID = g_SessionID;

        // 플레이어 추가
        CPlayer* pPlayer = new CPlayer;
        g_playerMap.emplace(g_SessionID, pPlayer);
        pPlayer->m_playerID = g_SessionID;

        // 세션 ID 1 증가
        g_SessionID++;


        // Recv를 걸어두어 비동기 입출력 시작
        RecvPost(pSession);
    }

    return 0;
}

void RecvPost(CSession* pSession)
{
    int retval;
    DWORD flags{};

    // 처음 IO Count ++, 이는 뒤에 할 경우 중간에 실패한 경우 ++을 못하고 있다가 IO Count가 0이 되는 경우가 있기 때문에 recv, send 호출 전에 한다.
    InterlockedIncrement(&pSession->IOcount);

    //ZeroMemory(&pSession->wsaRecvBuf, sizeof(WSABUF) * 2);
    pSession->wsaRecvBuf[0].buf = pSession->recvBuffer.GetFrontBufferPtr();
    pSession->wsaRecvBuf[0].len = pSession->recvBuffer.DirectEnqueueSize();
    pSession->wsaRecvBuf[1].buf = pSession->recvBuffer.GetBufferPtr();
    pSession->wsaRecvBuf[1].len = pSession->recvBuffer.GetBufferCapacity() - pSession->recvBuffer.DirectEnqueueSize();

    // Recv를 걸어두어 비동기 입출력 시작
    // 비동기 I/O시 I/O하나당 하나의 오버랩 구조체가 필요하다.
    retval = WSARecv(pSession->sock, pSession->wsaRecvBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);
    if (retval == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING) {
        InterlockedDecrement(&pSession->IOcount);
        //std::cout << "WSARecv() failed : " << WSAGetLastError() << "\n";
    }
}

void SendPost(CSession* pSession)
{
    // 처음 IO Count ++, 이는 뒤에 할 경우 중간에 실패한 경우 ++을 못하고 있다가 IO Count가 0이 되는 경우가 있기 때문에 recv, send 호출 전에 한다.
    InterlockedIncrement(&pSession->IOcount);

    // sendBuffer에 있는 데이터를 WSASend로 전송
    DWORD sendBytes = 0;
    DWORD flags = 0;

    if (pSession->bSending == true)
        return;

    DWORD cbTransferred = pSession->sendBuffer.GetUseSize();

    // 보낼 데이터가 sendBuffer의 directDeq 사이즈 보다 크다면
    if (cbTransferred > pSession->sendBuffer.DirectDequeueSize())
    {
        pSession->wsaSendBuf[0].buf = pSession->sendBuffer.GetFrontBufferPtr();
        pSession->wsaSendBuf[0].len = pSession->sendBuffer.DirectDequeueSize();

        pSession->wsaSendBuf[1].buf = pSession->sendBuffer.GetBufferPtr();
        pSession->wsaSendBuf[1].len = cbTransferred - pSession->sendBuffer.DirectDequeueSize();
    }
    // 보낼 데이터가 sendBuffer의 directDeq 사이즈보다 작거나 같다면
    else
    {
        pSession->wsaSendBuf[0].buf = pSession->sendBuffer.GetFrontBufferPtr();
        pSession->wsaSendBuf[0].len = cbTransferred;

        pSession->wsaSendBuf[1].buf = pSession->sendBuffer.GetBufferPtr();
        pSession->wsaSendBuf[1].len = 0;
    }

    // WSASend 호출
    pSession->bSending = true;

    int retval = WSASend(pSession->sock, pSession->wsaSendBuf, 2, nullptr, flags, &pSession->overlappedSend, NULL);
    if (retval == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING){
        InterlockedDecrement(&pSession->IOcount);
    }
}


