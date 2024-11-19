
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

// key�� id�� �޾� �����Ǵ� ���� ��
std::unordered_map<UINT32, CSession*> g_clients;

// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(void* pArg);
unsigned int WINAPI AcceptThread(void* pArg);

void SendPost(CSession* pSession);
void RecvPost(CSession* pSession);






int main(void)
{
    int retval;

    // ���� �ʱ�ȭ
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;

    // CPU ���� Ȯ��
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    // int ProcessorNum = si.dwNumberOfProcessors;
    // ���μ��� ���� �̸����� IOCP Running �������� ������ �����ϸ鼭 �׽�Ʈ �غ� ��.
    
    // ����� �Ϸ� ��Ʈ ����
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


    // sendbuf ũ�� ���� (�۽� ������ ũ�⸦ 0���� ������ �߰� �ܰ� ���۸� ���� �޸𸮿� ���� direct I/O�� ����)
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



    
    ServerLib serverLib;        // IOCP ���� ���̺귯�� �ν��Ͻ�
    ServerContent serverContent; // ���� ������ �ν��Ͻ�



    HANDLE hAcceptThread;		// ���ӿ�û, ���⿡ ���� ó��	
    HANDLE hWorkerThread;		// ��Ŀ ������

    DWORD dwThreadID;

    hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, &serverLib, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, 0, (unsigned int*)&dwThreadID);
    //hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, (LPVOID)0, 0, (unsigned int*)&dwThreadID);








    WCHAR ControlKey;

    //------------------------------------------------
    // ���� ��Ʈ��...
    //------------------------------------------------
    while (1)
    {
        ControlKey = _getwch();
        if (ControlKey == L'q' || ControlKey == L'Q')
        {
            //------------------------------------------------
            // ����ó��
            //------------------------------------------------
            PostQueuedCompletionStatus(g_hIOCP, 0, 0, NULL);
            break;
        }

        if (ControlKey == L'w' || ControlKey == L'W')
        {
            //------------------------------------------------
            // �����ϳ� �����ؼ� WSASend�� send 0�� ���� �� �Ϸ������� ������ Ȯ��
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






    // ���� ����
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


// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(void* pArg)
{
    ServerContent* pContent = reinterpret_cast<ServerContent*>(pArg);

    int retval;
    int error;
    UINT32 IOCount;

    // �񵿱� ����� �Ϸ� ��ٸ���
    DWORD transferredDataLen;

    CSession* pSession;

    // GQCS�� ��ȯ���� �����ϸ� TRUE, �ƴϸ� FALSE.
    OVERLAPPED* pOverlapped;

    while (1) {
        transferredDataLen = 0;
        memset(&pOverlapped, 0, sizeof(pOverlapped));

        // GQCS�� IOCP �ڵ�� �� �Ϸ������� ������.
        // ��ȯ������ �� ����Ʈ�� �޾Ҵ���, � Ű����, � ������ ����ü�� ����ߴ����� �˷��ش�.
        retval = GetQueuedCompletionStatus(g_hIOCP, &transferredDataLen, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

        error = WSAGetLastError();

        EnterCriticalSection(&pSession->cs);


        // PQCS�� ���� �Ϸ� ������(0, 0, NULL)�� �� ���. ��, ��(main ������)�� ���� ���� ���
        // �Ϸ������� �ִ� ���� �����带 ����.
        if ((transferredDataLen == 0 && pSession == 0 && pOverlapped == nullptr))
            break;

        // ���� ó�� - GQCS�� ������ ���, send,recv �ϴ��� ������ ���ܼ� closesocket�� �ؼ� �ش� ������ ����ϴ� �Ϸ������� ��� ������ ����̴�.
        // transferredDataLen�� 0�� ���(rst�� �� ���) <- �̰� send �ʿ��� 0�� ���������� ���� �� ������ send���� 0�� ������ ���� ������ ó��
        if (retval == FALSE || transferredDataLen == 0)
        {
            // ������ ��ȿ�ϴٸ� closesocket �ϰ�, ������ IO Count�� 1 �����Ѵ�. 
            IOCount = ReleaseSession(pSession);

            // IO Count�� 0�̶�� ������ ����.
            if (IOCount == 0)
                delete pSession;

            // IO Count�� 0�� �ƴ϶�� �Ϸ������� ���� ������ GQCS���� �Ϸ������� �ٽ� ���������� continue�Ѵ�.
            LeaveCriticalSection(&pSession->cs);
            continue;
        }

        // recv �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedRecv) {

            // WSARecv�� �����۸� ����ϱ⿡ ���ۿ� �����͸� �����ϰ�, ������� �������� �ʾҴ�. ���� ����� ������Ų��.
            pSession->recvQ.MoveRear(transferredDataLen);

            // ������������ ���� ���̱⿡ ���� �����͸� �̾Ƴ��� sendQ�� ����
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

                // ������ �ڵ� OnRecv�� ������ id�� ��Ŷ ������ �Ѱ���
                pContent->OnRecv(pSession->id, &Packet);
            }
            




            // ���⼭ ������ ó���� ��.
            // ������ ó���� �ϸ鼭 sendQ�� �����͸� ����. SendPost �Լ� ȣ��� sendQ�� ���� �����͸� ���� �� �ִٸ� ������.
            pSession->sendQ.Enqueue(temp, transferredDataLen);
               
            // ���� ���� ���� �� sendflag Ȯ����.
            // sending �ϴ� ���� �ƴ϶��
            if (InterlockedExchange(&pSession->sendFlag, 1) == 0)
            {
                // send ó��
                SendPost(pSession);
            }

            // recv ó��
            RecvPost(pSession);
        }

        // send �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedSend) {
            // sendFlag�� ���� ����, sendQ�� ���� �����Ͱ� �ִ��� Ȯ���Ѵ�. 
            // �ٸ� �����忡�� recv �Ϸ������� ó���� �� sendQ�� enq �ϰ�, sendFlag�� �˻��ϱ⿡
            // �� �� �ϳ��� ������ sendFlag�� �ùٸ��� ����ȴ�.
            InterlockedExchange(&pSession->sendFlag, 0);

            // ������ ���� �Ϸ� �����Ƿ� sendQ�� �ִ� �����͸� ����
            pSession->sendQ.MoveFront(transferredDataLen);

            // ���� sendQ�� �����Ͱ� �ִٸ�
            if (pSession->sendQ.GetUseSize() != 0)
            {
                if (InterlockedExchange(&pSession->sendFlag, 1) == 0)
                {
                    // send ó��
                    SendPost(pSession);
                }
            }

            // ���� sendQ�� �����Ͱ� ���ٸ�
            else
                ; // �̹� sendFlag 0���� �ٲ����� �ƹ��͵� �� ���� ����.
        }

        LeaveCriticalSection(&pSession->cs);

        // �Ϸ����� ó�� ���� IO Count 1 ����
        IOCount = InterlockedDecrement(&pSession->IOCount);

        // IO Count�� 0�� ���, ��� �Ϸ������� ó�������Ƿ� ������ �����ص� ����.
        if (IOCount == 0)
        {
            // ���� ����
            delete pSession;
        }
    }

    return 0;
}

unsigned int WINAPI AcceptThread(void* pArg)
{
    ServerLib* pServer = reinterpret_cast<ServerLib*>(pArg);

    // ������ ��ſ� ����� ����
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

        // ���� ����
        
        // ���� ���� ����ü �Ҵ�
        CSession* pSession = new CSession;

        if (pSession == NULL)
            break;

        // IP, PORT �� �߰�
        char pAddrBuf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &clientaddr, pAddrBuf, sizeof(pAddrBuf)) == NULL) {
            std::cout << "Error : inet_ntop()\n";
            DebugBreak();
        }

        memcpy(pSession->IP, pAddrBuf, sizeof(char) * 16);
        pSession->IP[16] = '\0';

        pSession->port = ntohs(clientaddr.sin_port);

        // send/recv�� ������ ����ü �ʱ�ȭ
        ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
        ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));

        // client ���� ����
        pSession->sock = client_sock;

        // client ID �ο�
        pSession->id = g_id;
        g_id++;

        // IO Count �ο�, �ʱ�ȭ �� �ñ�� ���� ��ϵ� ���̴� 0���� ����
        pSession->IOCount = 0;

        // send�� 1ȸ �����ϱ� ���� flag ����
        pSession->sendFlag = 0;

        pSession->SendingCount = 0;


        // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
        CreateIoCompletionPort((HANDLE)client_sock, g_hIOCP, (ULONG_PTR)pSession, 0);


        // recv ó��
        RecvPost(pSession);

        // �� �������� IO Count�� �̱� �����峪 �ٸ� ���⿡ interlock���� ������ �˻� ����.
        if (pSession->IOCount == 0)
        {
            // ���� WSARecv�� �����ߴٸ� �Ϸ������� ���� �ʱ⿡ ���⼭ �ٷ� ������ ����������Ѵ�.
            delete pSession;
            continue;
        }

        // ���⼭ �������� ���� ���̳� �迭�� ������ ���
        pServer->RegisterSession(pSession);
    }

    return 0;
}

// sendQ�� �����͸� �ְ�, WSASend�� ȣ��
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

    // �������� �������� ���̰� sendQ���� �� �� �ִ� �������� ���̺��� ũ�ٸ�
    if (transferredDataLen > pSession->sendQ.DirectDequeueSize())
    {
        wsaBuf[1].len = transferredDataLen - wsaBuf[0].len;
    }
    // �������� �������� ���̰� sendQ���� �� �� �ִ� �������� ���̿� ������ �� �۰ų� ���ٸ�
    else
    {
        wsaBuf[1].len = 0;
    }

    // send�ϴ� �����Ͱ� 0�̶��
    if ((wsaBuf[0].len + wsaBuf[1].len) == 0)
        // return �� ��.
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

    // �񵿱� recv ó���� ���� ���� recv�� �ɾ��. ���� �����ʹ� wsaBuf�� ����� �޸𸮿� ä���� ����.
    retVal = WSARecv(pSession->sock, recvBuf, 2, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        InterlockedDecrement(&pSession->IOCount);
        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // �� �κ��� ������ϴµ� � ������ ������ �𸣴ϱ� Ȯ�� �������� �־.
                                                                // ���߿� ��Ƽ������ ������ ������ ��ġ�� �ʴ� OnError ������ �񵿱� ��¹����� ��ü ����.
    }
}