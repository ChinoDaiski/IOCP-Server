
#include "pch.h"

#include <process.h>
#include <unordered_map>

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

ServerContent* g_pContent = nullptr;
ServerLib* g_pServer = nullptr;




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
    g_hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 4);
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
    serverLib.RegisterIContent(&serverContent);
    serverContent.RegisterIServer(&serverLib);

    g_pContent = &serverContent;
    g_pServer = &serverLib;


    HANDLE hAcceptThread;		// ���ӿ�û, ���⿡ ���� ó��	
    HANDLE hWorkerThread;		// ��Ŀ ������

    DWORD dwThreadID;

    hAcceptThread = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, &serverLib, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
    hWorkerThread = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, &serverContent, 0, (unsigned int*)&dwThreadID);
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
    DWORD curThreadID = GetCurrentThreadId();

    if (pSession->sock != INVALID_SOCKET)
    {
        closesocket(pSession->sock);
        pSession->sock = INVALID_SOCKET;
    }

    pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RELEASE_SESSION));
    return InterlockedDecrement(&pSession->IOCount);
}


// �۾��� ������ �Լ�
unsigned int WINAPI WorkerThread(void* pArg)
{
    DWORD curThreadID = GetCurrentThreadId();

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
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_AFTER_GQCS));

        error = WSAGetLastError();

        EnterCriticalSection(&pSession->cs_session);
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_ENTER_CS));


        // PQCS�� ���� �Ϸ� ������(0, 0, NULL)�� �� ���. ��, ��(main ������)�� ���� ���� ���
        // �Ϸ������� �ִ� ���� �����带 ����.
        if ((transferredDataLen == 0 && pSession == 0 && pOverlapped == nullptr))
        {
            LeaveCriticalSection(&pSession->cs_session);
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_LEAVE_CS1));
            break;
        }

        // ���� ó�� - GQCS�� ������ ���, send,recv �ϴ��� ������ ���ܼ� closesocket�� �ؼ� �ش� ������ ����ϴ� �Ϸ������� ��� ������ ����̴�.
        // transferredDataLen�� 0�� ���(rst�� �� ���) <- �̰� send �ʿ��� 0�� ���������� ���� �� ������ send���� 0�� ������ ���� ������ ó��
        if (retval == FALSE || transferredDataLen == 0)
        {
            // ������ ��ȿ�ϴٸ� closesocket �ϰ�, ������ IO Count�� 1 �����Ѵ�. 
            IOCount = ReleaseSession(pSession);
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, (ACTION((UINT32)ACTION::IOCOUNT_0 + IOCount))));
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_LEAVE_CS2));

            // IO Count�� 0�̶�� ������ ����.
            if (IOCount == 0)
            {
                LeaveCriticalSection(&pSession->cs_session);

                // ���� ����
                g_pServer->DeleteSession(pSession);
                continue;
            }

            // IO Count�� 0�� �ƴ϶�� �Ϸ������� ���� ������ GQCS���� �Ϸ������� �ٽ� ���������� continue�Ѵ�.
            LeaveCriticalSection(&pSession->cs_session);
            continue;
        }

        // recv �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedRecv) {
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_COMPLETION_NOTICE));
            
            // WSARecv�� �����۸� ����ϱ⿡ ���ۿ� �����͸� �����ϰ�, ������� �������� �ʾҴ�. ��� ����� ������Ų��.
            pSession->recvQ.MoveRear(transferredDataLen);

            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_START_WHILE));
            while (true)
            {
                // 1. RecvQ�� �ּ����� ����� �ִ��� Ȯ��. ������ [ ��� ������ �̻��� �����Ͱ� �ִ��� Ȯ�� ]�ϴ� ��.
                if (pSession->recvQ.GetUseSize() < sizeof(PACKET_HEADER))
                {
                    pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_EXIT_WHILE1));
                    break;
                }

                // 2. RecvQ���� PACKET_HEADER ���� Peek
                PACKET_HEADER header;
                int headerSize = sizeof(header);
                int retVal = pSession->recvQ.Peek(reinterpret_cast<char*>(&header), headerSize);

                // 3. ����� len���� RecvQ�� ������ ������ ��
                if ((header.bySize + sizeof(PACKET_HEADER)) > pSession->recvQ.GetUseSize())
                {
                    pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_EXIT_WHILE2));
                    break;
                }

                // 4. RecvQ���� header�� len ũ�⸸ŭ �ӽ� ��Ŷ ���۸� �̴´�.
                CPacket Packet; // �� �Ŵ����� ���� ������ ��� ���. ����� �غ��� ������������ ���� ������ ��� ����ϰ� ����.
                int echoSendSize = header.bySize + sizeof(PACKET_HEADER);
                int recvQDeqRetVal = pSession->recvQ.Dequeue(Packet.GetBufferPtr(), echoSendSize);
                Packet.MoveWritePos(echoSendSize);

                if (recvQDeqRetVal != echoSendSize)
                {
                    DebugBreak();
                }

                // ������ �ڵ� OnRecv�� ������ id�� ��Ŷ ������ �Ѱ���
                pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_ONRECV_START)); 

                Packet.MoveReadPos(sizeof(PACKET_HEADER));  // len ���̿� ���� ������ ����, netlib���� ����ϴ� ��Ŷ�� ���̿� ���� ������ ����.
                g_pContent->OnRecv(pSession->id, &Packet);  // ���̷ε常 ���� ��Ŷ ������ �������� �ѱ�

                pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_ONRECV_AFTER));
            }

            // recv ó��
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_RECVPOST_START));
            g_pServer->RecvPost(pSession);
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_RECVPOST_AFTER));
        }

        // send �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedSend) {
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_COMPLETION_NOTICE));

            // sendFlag�� ���� ����, sendQ�� ���� �����Ͱ� �ִ��� Ȯ���Ѵ�. 
            // �ٸ� �����忡�� recv �Ϸ������� ó���� �� sendQ�� enq �ϰ�, sendFlag�� �˻��ϱ⿡
            // �� �� �ϳ��� ������ sendFlag�� �ùٸ��� ����ȴ�.
            InterlockedExchange(&pSession->sendFlag, 0);
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_FLAG0));

            // ������ ���� �Ϸ� �����Ƿ� sendQ�� �ִ� �����͸� ����
            pSession->sendQ.MoveFront(transferredDataLen);

            // ���� sendQ�� �����Ͱ� �ִٸ�
            if (pSession->sendQ.GetUseSize() != 0)
            {
                pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_HAS_DATA));

                // send ó��
                pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_SENDPOST_START));
                g_pServer->SendPost(pSession);
                pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_SENDPOST_AFTER));
            }

            // ���� sendQ�� �����Ͱ� ���ٸ�
            else
            {
                // �̹� sendFlag 0���� �ٲ����� �ƹ��͵� �� ���� ����.
                pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_HAS_NODATA)); 
            }
        }

        // �Ϸ����� ó�� ���� IO Count 1 ����
        IOCount = InterlockedDecrement(&pSession->IOCount);
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION((UINT32)ACTION::IOCOUNT_0 + IOCount)));

        LeaveCriticalSection(&pSession->cs_session);
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_LEAVE_CS3));

        // IO Count�� 0�� ���, ��� �Ϸ������� ó�������Ƿ� ������ �����ص� ����.
        if (IOCount == 0)
        {
            // ���� ����
            g_pServer->DeleteSession(pSession);
        }
    }

    return 0;
}

unsigned int WINAPI AcceptThread(void* pArg)
{
    DWORD curThreadID = GetCurrentThreadId();

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

        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_AFTER_NEW));

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
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_CONNECT_IOCP));

        // ���⼭ �������� ���� ���̳� �迭�� ������ ���
        g_pServer->RegisterSession(pSession);
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_REGISTER_SESSION));

        // recv ó��, �� ����� �����ϰ�, ���Ŀ� WSARecv�� �ϳĸ�, ��� ���� Recv �Ϸ������� �� �� ����. �׷��� ����� ������
        g_pServer->RecvPost(pSession);
        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_RECVPOST));

        // �� �������� IO Count�� �̱� �����峪 �ٸ� ���⿡ interlock���� ������ �˻� ����.
        if (pSession->IOCount == 0)
        {
            // ���� WSARecv�� �����ߴٸ� �Ϸ������� ���� �ʱ⿡ ���⼭ �ٷ� ������ ����������Ѵ�.
            g_pServer->DeleteSession(pSession);
            pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_DELETE_SESSION));
        }

        pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_EXIT));
    }

    return 0;
}


