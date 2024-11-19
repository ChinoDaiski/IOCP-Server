
#include <iostream>

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

#include <process.h>
#include <unordered_map>

#include "Session.h"
#include "RingBuffer.h"

#include "ServerLib.h"
#include "Content.h"

#define SERVERPORT 6000


SOCKET g_listenSocket; 
HANDLE g_hIOCP; 
UINT32 g_id = 0;

// key로 id를 받아 관리되는 세션 맵
std::unordered_map<UINT32, CSession*> g_clients;

// 작업자 스레드 함수
unsigned int WINAPI WorkerThread(void* pArg);
unsigned int WINAPI AcceptThread(void* pArg);

void SendPost(CSession* pSession);
void RecvPost(CSession* pSession);






int main(void)
{
    int retval;

    // 윈속 초기화
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // CPU 개수 확인
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    // int ProcessorNum = si.dwNumberOfProcessors;
    // 프로세서 갯수 미만으로 IOCP Running 스레드의 갯수를 제한하면서 테스트 해볼 것.
    
    // 입출력 완료 포트 생성
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
    if (g_hIOCP == NULL)
    {
        std::cerr << "INVALID_HANDLE : g_hIOCP\n";
        return 1;
    }

    // socket()
    g_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listenSocket == INVALID_SOCKET)
    {
        std::cerr << "INVALID_SOCKET : listen_sock\n";
        return 1;
    }


    // sendbuf 크기 설정 (송신 버퍼의 크기를 0으로 설정해 중간 단계 버퍼링 없이 메모리에 직접 direct I/O를 유도)
    int recvBufSize = 0;
    if (setsockopt(g_listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << std::endl;
        closesocket(g_listenSocket);
        WSACleanup();
        return 1;
    }


    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serveraddr.sin_port = htons(SERVERPORT);
    retval = bind(g_listenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        std::cout << "bind()\n";

    // listen()
    retval = listen(g_listenSocket, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        std::cout << "listen()\n";



    
    ServerLib serverLib;        // IOCP 서버 라이브러리 인스턴스
    ServerContent serverContent; // 서버 콘텐츠 인스턴스



    HANDLE hAcceptThread;		// 접속요청, 끊기에 대한 처리	
    HANDLE hWorkerThread;		// 워커 스레드

    DWORD dwThreadID;

    hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, &serverLib, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);








    WCHAR ControlKey;

    //------------------------------------------------
    // 종료 컨트롤...
    //------------------------------------------------
    while (1)
    {
        ControlKey = _getwch();
        if (ControlKey == L'q' || ControlKey == L'Q')
        {
            //------------------------------------------------
            // 종료처리
            //------------------------------------------------
            PostQueuedCompletionStatus(g_hIOCP, 0, 0, NULL);
            break;
        }

        if (ControlKey == L'w' || ControlKey == L'W')
        {
            //------------------------------------------------
            // 세션하나 선택해서 WSASend로 send 0을 했을 때 완료통지가 오는지 확인
            //------------------------------------------------
            auto iter = g_clients.find(0);
            WSABUF wsaBuf;
            wsaBuf.buf = iter->second->sendQ.GetFrontBufferPtr();
            wsaBuf.len = 0;

            DWORD flags{};
            WSASend(iter->second->sock, &wsaBuf, 1, NULL, flags, &iter->second->overlappedSend, NULL);
            break;
        }
    }






    // 윈속 종료
    WSACleanup();
    return 0;
}

UINT32 ReleaseSession(CSession* pSession)
{
    if (pSession->sock != INVALID_SOCKET)
    {
        closesocket(pSession->sock);
        pSession->sock = INVALID_SOCKET;
    }

    return InterlockedDecrement(&pSession->IOCount);
}


// 작업자 스레드 함수
unsigned int WINAPI WorkerThread(void* pArg)
{
    ServerContent* pContent = reinterpret_cast<ServerContent*>(pArg);

    int retval;
    int error;
    UINT32 IOCount;

    // 비동기 입출력 완료 기다리기
    DWORD transferredDataLen;

    CSession* pSession;

    // GQCS의 반환값은 성공하면 TRUE, 아니면 FALSE.
    OVERLAPPED* pOverlapped;

    while (1) {
        transferredDataLen = 0;
        memset(&pOverlapped, 0, sizeof(pOverlapped));

        // GQCS로 IOCP 핸들로 온 완료통지를 가져옴.
        // 반환값으로 몇 바이트를 받았는지, 어떤 키인지, 어떤 오버랩 구조체를 사용했는지를 알려준다.
        retval = GetQueuedCompletionStatus(g_hIOCP, &transferredDataLen, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

        error = WSAGetLastError();

        EnterCriticalSection(&pSession->cs);


        // PQCS로 넣은 완료 통지값(0, 0, NULL)이 온 경우. 즉, 내(main 스레드)가 먼저 끊은 경우
        // 완료통지가 있던 말던 스레드를 종료.
        if ((transferredDataLen == 0 && pSession == 0 && pOverlapped == nullptr))
            break;

        // 예외 처리 - GQCS가 실패한 경우, send,recv 하던중 문제가 생겨서 closesocket을 해서 해당 소켓을 사용하는 완료통지가 모두 실패한 경우이다.
        // transferredDataLen이 0인 경우(rst가 온 경우) <- 이건 send 쪽에서 0을 정상적으로 보낼 수 있으니 send에서 0을 보내는 경우는 막도록 처리
        if (retval == FALSE || transferredDataLen == 0)
        {
            // 소켓이 유효하다면 closesocket 하고, 세션의 IO Count를 1 감소한다. 
            IOCount = ReleaseSession(pSession);

            // IO Count가 0이라면 세션을 지움.
            if (IOCount == 0)
                delete pSession;

            // IO Count가 0이 아니라면 완료통지가 남아 있으니 GQCS에서 완료통지를 다시 가져오도록 continue한다.
            LeaveCriticalSection(&pSession->cs);
            continue;
        }

        // recv 완료 통지가 온 경우
        else if (pOverlapped == &pSession->overlappedRecv) {

            // WSARecv는 링버퍼를 사용하기에 버퍼에 데이터만 존재하고, 사이즈는 증가하지 않았다. 고로 사이즈를 증가시킨다.
            pSession->recvQ.MoveRear(transferredDataLen);

            // 에코형식으로 만들 것이기에 받은 데이터를 뽑아내어 sendQ에 삽입
            char temp[512];
            pSession->recvQ.Dequeue(temp, transferredDataLen);


            while (true)
            {
                if (pSession->recvQ.GetUseSize() < 10)
                    break;
                
                CPacket Packet;
                int recvQDeqRetVal = pSession->recvQ.Dequeue(Packet.GetBufferPtr(), 10);
                Packet.MoveWritePos(10);

                if (recvQDeqRetVal != 10)
                {
                    DebugBreak();
                }

                // 콘텐츠 코드 OnRecv에 세션의 id와 패킷 정보를 넘겨줌
                pContent->OnRecv(pSession->id, &Packet);
            }
            




            // 여기서 컨텐츠 처리가 들어감.
            // 컨텐츠 처리를 하면서 sendQ에 데이터를 삽입. SendPost 함수 호출시 sendQ에 넣은 데이터를 보낼 수 있다면 보낸다.
            pSession->sendQ.Enqueue(temp, transferredDataLen);
               
            // 보낼 것이 있을 때 sendflag 확인함.
            // sending 하는 중이 아니라면
            if (InterlockedExchange(&pSession->sendFlag, 1) == 0)
            {
                // send 처리
                SendPost(pSession);
            }

            // recv 처리
            RecvPost(pSession);
        }

        // send 완료 통지가 온 경우
        else if (pOverlapped == &pSession->overlappedSend) {
            // sendFlag를 먼저 놓고, sendQ에 보낼 데이터가 있는지 확인한다. 
            // 다른 스레드에서 recv 완료통지를 처리할 때 sendQ에 enq 하고, sendFlag를 검사하기에
            // 둘 중 하나는 무조건 sendFlag가 올바르게 적용된다.
            InterlockedExchange(&pSession->sendFlag, 0);

            // 보내는 것을 완료 했으므로 sendQ에 있는 데이터를 제거
            pSession->sendQ.MoveFront(transferredDataLen);

            // 만약 sendQ에 데이터가 있다면
            if (pSession->sendQ.GetUseSize() != 0)
            {
                if (InterlockedExchange(&pSession->sendFlag, 1) == 0)
                {
                    // send 처리
                    SendPost(pSession);
                }
            }

            // 만약 sendQ에 데이터가 없다면
            else
                ; // 이미 sendFlag 0으로 바꿨으니 아무것도 할 것이 없다.
        }

        LeaveCriticalSection(&pSession->cs);

        // 완료통지 처리 이후 IO Count 1 감소
        IOCount = InterlockedDecrement(&pSession->IOCount);

        // IO Count가 0인 경우, 모든 완료통지를 처리했으므로 세션을 삭제해도 무방.
        if (IOCount == 0)
        {
            // 세션 삭제
            delete pSession;
        }
    }

    return 0;
}

unsigned int WINAPI AcceptThread(void* pArg)
{
    ServerLib* pServer = reinterpret_cast<ServerLib*>(pArg);

    // 데이터 통신에 사용할 변수
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;

    int addrlen;

    while (true)
    {
        // Accept
        addrlen = sizeof(clientaddr);
        client_sock = accept(g_listenSocket, (SOCKADDR*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            std::cout << "accept()\n";
            break;
        }

        // 세션 생성
        
        // 소켓 정보 구조체 할당
        CSession* pSession = new CSession;

        if (pSession == NULL)
            break;

        // IP, PORT 값 추가
        char pAddrBuf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &clientaddr, pAddrBuf, sizeof(pAddrBuf)) == NULL) {
            std::cout << "Error : inet_ntop()\n";
            DebugBreak();
        }

        memcpy(pSession->IP, pAddrBuf, sizeof(char) * 16);
        pSession->IP[16] = '\0';

        pSession->port = ntohs(clientaddr.sin_port);

        // send/recv용 오버랩 구조체 초기화
        ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
        ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));

        // client 소켓 연결
        pSession->sock = client_sock;

        // client ID 부여
        pSession->id = g_id;
        g_id++;

        // IO Count 부여, 초기화 할 시기로 아직 등록도 전이니 0으로 설정
        pSession->IOCount = 0;

        // send를 1회 제한하기 위한 flag 변수
        pSession->sendFlag = 0;

        pSession->SendingCount = 0;


        // 소켓과 입출력 완료 포트 연결 - 새로 연견될 소켓을 key를 해당 소켓과 연결된 세션 구조체의 포인터로 하여 IOCP와 연결한다.
        CreateIoCompletionPort((HANDLE)client_sock, g_hIOCP, (ULONG_PTR)pSession, 0);


        // recv 처리
        RecvPost(pSession);

        // 이 시점에선 IO Count가 싱글 스레드나 다름 없기에 interlock없이 생으로 검사 가능.
        if (pSession->IOCount == 0)
        {
            // 만약 WSARecv가 실패했다면 완료통지도 오지 않기에 여기서 바로 세션을 삭제해줘야한다.
            delete pSession;
            continue;
        }

        // 여기서 전역적인 세션 맵이나 배열에 세션을 등록
        pServer->RegisterSession(pSession);
    }

    return 0;
}

// sendQ에 데이터를 넣고, WSASend를 호출
void SendPost(CSession* pSession)
{
    UINT32 a = InterlockedIncrement(&pSession->SendingCount);

    if (a == 2)
        DebugBreak();

    int retval;
    int error;

    WSABUF wsaBuf[2];
    wsaBuf[0].buf = pSession->sendQ.GetFrontBufferPtr();
    wsaBuf[0].len = pSession->sendQ.DirectDequeueSize();

    wsaBuf[1].buf = pSession->sendQ.GetBufferPtr();

    int transferredDataLen = pSession->sendQ.GetUseSize();

    // 보내려는 데이터의 길이가 sendQ에서 뺄 수 있는 데이터의 길이보다 크다면
    if (transferredDataLen > pSession->sendQ.DirectDequeueSize())
    {
        wsaBuf[1].len = transferredDataLen - wsaBuf[0].len;
    }
    // 보내려는 데이터의 길이가 sendQ에서 뺄 수 있는 데이터의 길이와 비교했을 때 작거나 같다면
    else
    {
        wsaBuf[1].len = 0;
    }

    // send하는 데이터가 0이라면
    if ((wsaBuf[0].len + wsaBuf[1].len) == 0)
        // return 할 것.
        return;

    DWORD flags{};

    InterlockedIncrement(&pSession->IOCount);

    retval = WSASend(pSession->sock, wsaBuf, 2, NULL, flags, &pSession->overlappedSend, NULL);

    error = WSAGetLastError();
    if (retval == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            ;
        else
            DebugBreak();
    }

    InterlockedDecrement(&pSession->SendingCount);
}

void RecvPost(CSession* pSession)
{
    int retVal;
    int error;

    DWORD flags{};

    WSABUF recvBuf[2];
    recvBuf[0].buf = pSession->recvQ.GetFrontBufferPtr();
    recvBuf[0].len = pSession->recvQ.DirectEnqueueSize();
    recvBuf[1].buf = pSession->recvQ.GetBufferPtr();
    recvBuf[1].len = pSession->recvQ.GetBufferCapacity() - pSession->recvQ.DirectEnqueueSize();
   
    InterlockedIncrement(&pSession->IOCount);

    // 비동기 recv 처리를 위해 먼저 recv를 걸어둠. 받은 데이터는 wsaBuf에 등록한 메모리에 채워질 예정.
    retVal = WSARecv(pSession->sock, recvBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        InterlockedDecrement(&pSession->IOCount);
        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // 이 부분은 없어야하는데 어떤 오류가 생길지 모르니깐 확인 차원에서 넣어봄.
                                                                // 나중엔 멀티스레드 로직에 영향을 미치지 않는 OnError 등으로 비동기 출력문으로 교체 예정.
    }
}