
#include <iostream>

#define NOMINMAX
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment(lib, "ws2_32")

#include <process.h>
#include <unordered_map>

#include "RingBuffer.h"


#define SERVERPORT 6000

// 소켓 정보 저장을 위한 구조체
class CSession
{
public:
    CSession(){
    }
    ~CSession(){
    }

public:
    SOCKET sock;

    USHORT port;
    char IP[17];

    CRingBuffer recvQ;
    CRingBuffer sendQ;

    OVERLAPPED overlappedRecv;
    OVERLAPPED overlappedSend;
};

class CPlayer
{
public:
    CPlayer() {}
    ~CPlayer() {}

public:
    UINT32 m_playerID;
};

SOCKET g_listenSocket; 
HANDLE g_hIOCP;

// 작업자 스레드 함수
unsigned int WINAPI WorkerThread(void* arg);
unsigned int WINAPI AcceptThread(void* pArg);

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
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 3);
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


    // recvbuf 크기 설정 (송신 버퍼의 크기를 0으로 설정해 중간 단계 버퍼링 없이 메모리에 직접 direct I/O를 유도)
    int recvBufSize = 0;
    if (setsockopt(g_listenSocket, SOL_SOCKET, SO_RCVBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR) {
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






    HANDLE hAcceptThread;		// 접속요청, 끊기에 대한 처리	
    HANDLE hWorkerThread;		// 워커 스레드

    DWORD dwThreadID;

    hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, NULL, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
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
    }






    // 윈속 종료
    WSACleanup();
    return 0;
}



// 작업자 스레드 함수
unsigned int WINAPI WorkerThread(LPVOID arg)
{
    int retval;

    while (1) {
        // 비동기 입출력 완료 기다리기
        DWORD transferredDataLen{};

        CSession* pSession = nullptr;

        // GQCS의 반환값은 성공하면 TRUE, 아니면 FALSE.
        OVERLAPPED* pOverlapped;
        memset(&pOverlapped, 0, sizeof(pOverlapped));

        // GQCS로 IOCP 핸들로 온 완료통지를 가져옴.
        // 반환값으로 몇 바이트를 받았는지, 어떤 키인지, 어떤 오버랩 구조체를 사용했는지를 알려준다.
        retval = GetQueuedCompletionStatus(g_hIOCP, &transferredDataLen, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

        int wsaError = WSAGetLastError();
        int Error = GetLastError();

        // 예외 처리 - GQCS가 실패했거나, cbTransferred이 0인 경우(rst가 온 경우)
        if (retval == FALSE || transferredDataLen == 0 )
        {
            // PQCS로 넣은 완료 통지값(0, 0, NULL)이 온 경우. 즉, 내가 먼저 끊은 경우
            if (pSession == 0 && pOverlapped == nullptr)
            {
                // while문을 빠져나간다.
                break;
            }

            // 상대방에서 먼저 끊어서 이쪽에서 확인해보니 네트워크가 더 이상 없는 경우
            if (wsaError == ERROR_NETNAME_DELETED)
            {
                continue;
            }

            // 상대 측에서 먼저 끊은 경우나 진짜 에러일 것이다. 확인 필요.
            DebugBreak();
        }

        // recv 완료 통지가 온 경우
        else if (pOverlapped == &pSession->overlappedRecv) {
            // 링버퍼를 사용하기에 버퍼에 데이터만 존재하고, 사이즈는 증가하지 않았다. 고로 사이즈를 증가시킨다.
            pSession->recvQ.MoveRear(transferredDataLen);

            // 에코형식으로 만들 것이기에 받은 데이터를 뽑아내어 sendQ에 삽입
            char temp[512];
            pSession->recvQ.Dequeue(temp, transferredDataLen);

            pSession->sendQ.Enqueue(temp, transferredDataLen);

            WSABUF wsaBuf[2];
            wsaBuf[0].buf = pSession->sendQ.GetFrontBufferPtr();
            wsaBuf[0].len = pSession->sendQ.DirectDequeueSize();

            wsaBuf[1].buf = pSession->sendQ.GetBufferPtr();

            // 보내려는 데이터의 길이가 sendQ에서 뺄 수 있는 데이터의 길이보다 크다면
            if (transferredDataLen > (DWORD)pSession->sendQ.DirectDequeueSize())
            {
                wsaBuf[1].len = transferredDataLen - wsaBuf[0].len;
            }
            // 보내려는 데이터의 길이가 sendQ에서 뺄 수 있는 데이터의 길이와 비교했을 때 작거나 같다면
            else
            {
                wsaBuf[1].len = 0;
            }

            DWORD flags{};
            WSASend(pSession->sock, wsaBuf, 2, NULL, flags, &pSession->overlappedSend, NULL);



            // 다시 recv를 걸어둬서 비동기적으로 recv가 처리되도록 함
            wsaBuf[0].buf = pSession->recvQ.GetFrontBufferPtr();
            wsaBuf[0].len = pSession->recvQ.DirectEnqueueSize();
            wsaBuf[1].buf = pSession->recvQ.GetBufferPtr();
            wsaBuf[1].len = pSession->recvQ.GetBufferCapacity() - pSession->recvQ.DirectEnqueueSize();

            flags = 0;
            WSARecv(pSession->sock, wsaBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);
        }

        // send 완료 통지가 온 경우
        else if (pOverlapped == &pSession->overlappedSend) {
            // 보내는 것을 완료 했으므로 sendQ에 있는 데이터를 제거
            pSession->sendQ.MoveFront(transferredDataLen);
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


        // 소켓과 입출력 완료 포트 연결 - 새로 연견될 소켓을 key를 해당 소켓과 연결된 세션 구조체의 포인터로 하여 IOCP와 연결한다.
        CreateIoCompletionPort((HANDLE)client_sock, g_hIOCP, (ULONG_PTR)pSession, 0);

        DWORD flags{};

        WSABUF recvBuf;
        recvBuf.buf = pSession->recvQ.GetFrontBufferPtr();
        recvBuf.len = pSession->recvQ.GetBufferCapacity();

        // 비동기 recv 처리를 위해 먼저 recv를 걸어둠. 받은 데이터는 wsaBuf에 등록한 메모리에 채워질 예정.
        WSARecv(pSession->sock, &recvBuf, 1, NULL, &flags, &pSession->overlappedRecv, NULL);
    }

    return 0;
}
