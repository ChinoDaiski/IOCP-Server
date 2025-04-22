
#include "pch.h"
#include "Protocol.h"
#include "IServer.h"
#include "Session.h"
#include "Packet.h"

void CLanServer::RecvPost(CSession* pSession)
{
    // recvMessageTPS 1 ����
    InterlockedIncrement(&recvMessageTPS);

    int retVal;
    int error;

    DWORD flags{};

    WSABUF recvBuf[2];
    int bufCnt = pSession->recvQ.makeWSARecvBuf(recvBuf);

    UINT32 IOCount = InterlockedIncrement(&pSession->IOCount);
    Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

    // �񵿱� recv ó���� ���� ���� recv�� �ɾ��. ���� �����ʹ� wsaBuf�� ����� �޸𸮿� ä���� ����.
    retVal = WSARecv(pSession->sock, recvBuf, bufCnt, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        else
        {
            Logging(pSession, ACTION::SENDPOST_WSARECV_FAIL_NOPENDING);

            // ���� �ʿ��� ���� ������ ����. ���� Ȯ��. �̶��� �Ϸ������� �߻���.
            // ��Ȯ�� �����ʿ��� ������ �����ٰ� �ѵ� ���ʿ��� ť�� �۾��� ����ߴٸ� �Ϸ������� ��.
            /*if (error == WSAECONNRESET || error == WSAECONNABORTED)
                return;*/

            // �Ϸ����� ó�� ���� IO Count 1 ����
            UINT32 IOCount = InterlockedDecrement(&pSession->IOCount);

            Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

            Logging(pSession, error);

            //std::cerr << "RecvPost : WSARecv - " << error << "\n"; // �� �κ��� ������ϴµ� � ������ ������ �𸣴ϱ� Ȯ�� �������� �־.
            // ���߿� ��Ƽ������ ������ ������ ��ġ�� �ʴ� OnError ������ �񵿱� ��¹����� ��ü ����.
        }
    }
}

// sendQ�� �����͸� �ְ�, WSASend�� ȣ��
void CLanServer::SendPost(CSession* pSession)
{
    DWORD curThreadID = GetCurrentThreadId();

    // ���� ���� ���� �� sendflag Ȯ����.
    // sending �ϴ� ���̶��
    if (InterlockedExchange(&pSession->sendFlag, 1) == 1)
    {
        // �Ѿ
        Logging(pSession, ACTION::SERVERLIB_SENDPOST_SOMEONE_SENDING);
        return;
    }
    Logging(pSession, ACTION::SERVERLIB_SENDPOST_REAL_SEND);

    // sendMessageTPS 1 ����
    InterlockedIncrement(&sendMessageTPS);

    int retval;
    int error;

    WSABUF sendBuf[MAX_PACKET_QUEUE_SIZE];
    int bufCnt = pSession->sendQ.makeWSASendBuf(sendBuf);

    // A �����尡 ����� Ȯ���ϰ� ���� interlock�� ȣ������ ���� ��Ȳ����, ���ؽ�Ʈ ����Ī. 
    // B �����尡 recvPost �����ϸ鼭 sendflag�� 1�� �ٲٰ� ����
    // C �����尡 send �Ϸ������� �޾� sendflag�� 0���� �ٲ�
    // A �����尡 �Ͼ�� interlock���� 0 ���� 1�� �ٲٰ� send ����. �ٵ� �̹� send �Ǿ���� ��Ȳ���� send 0�� ���� �� ����.
    // �̷��� �츮 �������� recv 0�϶� ������ �����ϱ⿡ �̸� �ذ��ϱ� ���ؼ� send�� 0�� ��Ȳ�� Ȯ���ؼ� ������ ���߿� ����.
    int sendLen = 0;
    for (int i = 0; i < bufCnt; ++i)
    {
        sendLen += sendBuf[i].len;
    }

    if (sendLen == 0)
    {
        Logging(pSession, ACTION::SERVERLIB_SENDPOST_SEND0);
        
        // ���� WSASend�� ���ϴϱ� sendflag�� Ǯ������Ѵ�.
        InterlockedExchange(&pSession->sendFlag, 0);
        return;
    }

    DWORD flags{};

    UINT32 IOCount = InterlockedIncrement(&pSession->IOCount);
    Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

    retval = WSASend(pSession->sock, sendBuf, bufCnt, NULL, flags, &pSession->overlappedSend, NULL);

    error = WSAGetLastError();
    if (retval == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            ;
        else
        {
            Logging(pSession, ACTION::SENDPOST_WSASEND_FAIL_NOPENDING);

            // ���� �ʿ��� ���� ������ ����. ���� Ȯ��. 
            // ��Ȯ�� �����ʿ��� ������ �����ٰ� �ѵ� ���ʿ��� ť�� �۾��� ����ߴٸ� �Ϸ������� ��.
            /*if (error == WSAECONNRESET || error == WSAECONNABORTED)
                return;*/

            // ���⼭ ������ ��� �Ϸ����� ��ü�� ���� ����.

            //// �Ϸ����� ó�� ���� IO Count 1 ����
            UINT32 IOCount = InterlockedDecrement(&pSession->IOCount);

            Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

            Logging(pSession, error);
            
            //std::cerr << "SendPost : WSASend - " << error << "\n"; // �� �κ��� ������ϴµ� � ������ ������ �𸣴ϱ� Ȯ�� �������� �־.
            // ���߿� ��Ƽ������ ������ ������ ��ġ�� �ʴ� OnError ������ �񵿱� ��¹����� ��ü ����.
        }
    }
}

bool CLanServer::AcceptPost(CSession* pSession)
{ 
    // 1) �� ���� ����
    pSession->sock = WSASocket(
        AF_INET, SOCK_STREAM, IPPROTO_TCP,
        nullptr, 0,
        WSA_FLAG_OVERLAPPED
    );

    if (pSession->sock == INVALID_SOCKET)
        return false;

    // socket�� CSession�� CompletionPort ��ü�� ����
    CreateIoCompletionPort(
        (HANDLE)pSession->sock,
        hIOCP,
        (ULONG_PTR)pSession,
        0
    );

    if (hIOCP == INVALID_HANDLE_VALUE)
    {
        printf("[����] CreateIoCompletionPort()�Լ� ����: %d\n", GetLastError());
        return false;
    }

    // 2) AcceptEx ȣ��
    DWORD recvBytes = 0;
    BOOL ok = AcceptEx(
        listenSocket,               // ��� �ִ� ���� ����
        pSession->sock,             // AcceptEx�� ���� ������(������ ����) ����
        pSession->acceptBuffer,     // ����Ʈ/���� �ּҸ� ���� ����
        0,                          // ù ������ ������ ���� �����Ƿ� 0
        sizeof(SOCKADDR_IN) + 16,   // ���� �ּ� ���� ũ��
        sizeof(SOCKADDR_IN) + 16,   // ����Ʈ �ּ� ���� ũ��
        &recvBytes,                 // ���� ����� ���ŵ� ����Ʈ, �񵿱�� ����
        &pSession->overlappedAccept // OVERLAPPED ����ü
    );

    // ok == true : AcceptEx�� ��� �Ϸ�� ���
    // ok == false && WSAGetLastError() == ERROR_IO_PENDING : �񵿱� �Ϸ� ��� ����, GQCS�� �Ϸ����� ó�� �غ� �Ϸ�

    // �� ���� ���
    if (!ok && WSAGetLastError() != ERROR_IO_PENDING)
    {
        // ������ ���� �����Ƿ� false ��ȯ
        // ���� �ݱ� �� ��ó���� returnSession���� ���� ����
        return false;
    }

    return true;
}

unsigned int __stdcall CLanServer::WorkerThread(void* pArg)
{
    CLanServer* pThis = (CLanServer*)pArg;

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
        retval = GetQueuedCompletionStatus(pThis->hIOCP, &transferredDataLen, (PULONG_PTR)&pSession, &pOverlapped, INFINITE);

        error = WSAGetLastError();
        if (pSession)
        {
            Logging(pSession, ACTION::WORKER_AFTER_GQCS);

            Logging(pSession, error);

            if (pOverlapped == &pSession->overlappedRecv)
                Logging(pSession, ACTION::WORKER_RECV_COMPLETION_NOTICE);
            else if (pOverlapped == &pSession->overlappedSend)
                Logging(pSession, ACTION::WORKER_SEND_COMPLETION_NOTICE);
        }


        // PQCS�� ���� �Ϸ� ������(0, 0, NULL)�� �� ���. ��, ��(main ������)�� ���� ���� ���
        // �Ϸ������� �ִ� ���� �����带 ����.
        if ((transferredDataLen == 0 && pSession == 0 && pOverlapped == nullptr))
        {
            break;
        }

        // ���� ó�� - GQCS�� ������ ���, send,recv �ϴ��� ������ ���ܼ� closesocket�� �ؼ� �ش� ������ ����ϴ� �Ϸ������� ��� ������ ����̴�.
        // transferredDataLen�� 0�� ���(rst�� �� ���) <- �̰� send �ʿ��� 0�� ���������� ���� �� ������ send���� 0�� ������ ���� ������ ó��
        if (retval == FALSE || transferredDataLen == 0)
        {
            // ����ʿ��� ��������� closesocket�̳� shutdown�� ȣ������ �ʰ� �����̳� ���μ����� �������Ḧ �ϴ� ��� rst�� ������ ���� �ʴ´�.
            // �̶� �����ʿ��� send/recv�� �õ��ϸ� ���� ������ ERROR_NETNAME_DELETED �̴�.
            if (error == ERROR_NETNAME_DELETED)
            {
                ;   // �ٷ� IO Count�� 1 ���ҽ�Ű�� ������ �Ѿ�� ó��.
            }

            if (pSession)
            {
                Logging(pSession, ACTION::WORKER_LEAVE_CS1);
            }
        }

        // ������ ���µ� pSeesion�� nullptr�� ���, AcceptEx�� ���� �Ϸ������̱� ������ pOverlapped�� Ȯ���Ͽ� ���ǰ��� ã�´�. 
        if (!pSession)
        {
            pSession = reinterpret_cast<CSession*>(
                reinterpret_cast<char*>(pOverlapped)
                - offsetof(CSession, overlappedAccept)
                );
        }

        // 1) AcceptEx �Ϸ� ó��
        if (pOverlapped == &pSession->overlappedAccept)
        {
            // AcceptTps 1 ����
            InterlockedIncrement(&pThis->acceptTPS);

            // �ּ� ���� ������
            sockaddr* localAddr = nullptr;
            sockaddr* remoteAddr = nullptr;
            int localLen = 0, remoteLen = 0;
            GetAcceptExSockaddrs(
                pSession->acceptBuffer,
                0,
                sizeof(SOCKADDR_IN) + 16,
                sizeof(SOCKADDR_IN) + 16,
                &localAddr, &localLen,
                &remoteAddr, &remoteLen
            );
            SOCKADDR_IN* clientaddr = reinterpret_cast<SOCKADDR_IN*>(remoteAddr);

            std::string connectedIP;
            UINT16 connectedPort;
            pThis->extractIPPort(*clientaddr, connectedIP, connectedPort);

            // ���� ��� �˻�
            if (!pThis->OnConnectionRequest(connectedIP, connectedPort))
            {
                // ���� ip�� �ƴ� �� �ش� ������ �ݰ�, ���� AcceptEx �õ�
                closesocket(pSession->sock);
                pSession->sock = INVALID_SOCKET;

                // AcceptEx ����ϰ� �ִ� ���� ���� Ȯ��
                if (pThis->IsAcceptExUnderLimit())
                {
                    // ���� AcceptEx �غ�
                    pThis->InitSessionInfo(pSession);

                    while (!pThis->AcceptPost(pSession))
                    {
                        closesocket(pSession->sock);
                        pSession->sock = INVALID_SOCKET;

                        pThis->InitSessionInfo(pSession);
                    }
                }

                // ������ ������ �ʿ� ���� ���ϸ� ���� �Ҵ�޾� �����ϸ� �Ǳ⿡ �̷��� ó��
                continue;
            }

            // �� �������� �̹� pSeesion�� Init�� �� ����

            // ���� ���� ������Ʈ & IOCP ���

            // ���ǿ� IP, Port ���� �߰�
            strcpy_s(pSession->IP, sizeof(pSession->IP), connectedIP.c_str());
            pSession->IP[sizeof(pSession->IP) - 1] = '\0';

            pSession->port = connectedPort;

            // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
            CreateIoCompletionPort(
                (HANDLE)pSession->sock,
                pThis->hIOCP,
                (ULONG_PTR)pSession,
                0
            );

            // send�� �����ϰ� recv�� �Ŵϱ� recv�� �ɱ����� �ٸ� �����忡�� send �Ϸ������� �ͼ� �����尡 �����Ǿ������ ��찡 ����.
            // �̸� �����ϱ� ���� OnAccept ȣ�� ���� IOCount�� ����, RecvPost ���� IOCount ����
            InterlockedIncrement(&pSession->IOCount);

            // OnAccept �� RecvPost ȣ��(recv ����)
            {
                // ������ accept �Լ� ���� -> ���ڰ� �ƴ� ��¥�� ���������� ���� ���� �� ��� ���.
                // OnAccept�� �����ϴ� ������ recvPost�� ���� �ɾ��� �� ��Ȱ���� �ϰ� �Ǹ� ������ �����.
                pThis->OnAccept(pSession->id);

                // recv ó��, �� ����� �����ϰ�, ���Ŀ� WSARecv�� �ϳĸ�, ��� ���� Recv �Ϸ������� �� �� ����. �׷��� ����� ������
                pThis->RecvPost(pSession);
            }

            //UINT32 IOCount = InterlockedDecrement(&pSession->IOCount);

            //// �̰� Ȯ���غ���. ������ ������� Ȯ���ؾ���. ���� ������ �����, ���⼭ ���� ��Ȱ�������.
            //// �ϴ� ������ ������ �Ǻ���. �α� �� �� Ȯ���غ����ҵ�... �Ϳ�
            //if (IOCount == 0)
            //    DebugBreak();
        }

        // recv �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedRecv) {

            // WSARecv�� �����۸� ����ϱ⿡ ���ۿ� �����͸� �����ϰ�, ������� �������� �ʾҴ�. ��� ����� ������Ų��.
            pSession->recvQ.MoveRear(transferredDataLen);

            int useSize = pSession->recvQ.GetUseSize();

            Logging(pSession, ACTION::WORKER_RECV_START_WHILE);
            while (true)
            {
                // 1. RecvQ�� �ּ����� ����� �ִ��� Ȯ��. ������ [ ��� ������ �̻��� �����Ͱ� �ִ��� Ȯ�� ]�ϴ� ��.
                int useSize = pSession->recvQ.GetUseSize();
                if (useSize < sizeof(PACKET_HEADER))
                {
                    Logging(pSession, ACTION::WORKER_RECV_EXIT_WHILE1);
                    break;
                }


                // 2. RecvQ���� PACKET_HEADER ���� Peek
                PACKET_HEADER header;
                int headerSize = sizeof(header);

                int retVal = pSession->recvQ.Peek(reinterpret_cast<char*>(&header), headerSize);

                // 3. ����� len���� RecvQ�� ������ ������ ��
                useSize = pSession->recvQ.GetUseSize();
                if ((header.bySize + sizeof(PACKET_HEADER)) > useSize)
                {
                    Logging(pSession, ACTION::WORKER_RECV_EXIT_WHILE2);
                    break;
                }

                // 4. RecvQ���� header�� len ũ�⸸ŭ �ӽ� ��Ŷ ���۸� �̴´�.
                CPacket* Packet = new CPacket; // �� �Ŵ����� ���� ������ ��� ���. ����� �غ��� ������������ ���� ������ ��� ����ϰ� ����.
                Packet->AddRef();

                int echoSendSize = header.bySize + sizeof(PACKET_HEADER);
                int recvQDeqRetVal = pSession->recvQ.Dequeue(Packet->GetFrontBufferPtr(), echoSendSize);
                Packet->MoveWritePos(echoSendSize);

                if (recvQDeqRetVal != echoSendSize)
                {
                    DebugBreak();
                }

                // ������ �ڵ� OnRecv�� ������ id�� ��Ŷ ������ �Ѱ���
                Packet->MoveReadPos(sizeof(PACKET_HEADER));  // len ���̿� ���� ������ ����, netlib���� ����ϴ� ��Ŷ�� ���̿� ���� ������ ����.
                Logging(pSession, ACTION::WORKER_RECV_ONRECV_START);
                pThis->OnRecv(pSession->id, Packet);  // ���̷ε常 ���� ��Ŷ ������ �������� �ѱ�
                Logging(pSession, ACTION::WORKER_RECV_ONRECV_AFTER);


                // ��Ŷ�� release �Լ��� ȣ���ؼ� refCounter�� 1���̰�, �ٿ��� ���� 0�� ���� Ȯ��
                if (Packet->ReleaseRef() == 0)
                {
                    // 0�̶�� ��Ŷ�� ����
                    delete Packet;
                }
            }

            // recv ó��
            Logging(pSession, ACTION::WORKER_RECV_RECVPOST_START);
            pThis->RecvPost(pSession);
            Logging(pSession, ACTION::WORKER_RECV_RECVPOST_AFTER);
        }

        // send �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedSend) {
            
            // ������ ���� �Ϸ� �����Ƿ� sendQ�� �ִ� �����͸� ����

            // ���ڷ� ���� �������� ũ�⸦ ���� �ȿ� �ִ� ��Ŷ�� pop.
            pSession->sendQ.RemoveSendCompletePacket(transferredDataLen);

            //// �� �������� read, writepos ���
            //Logging(pSession, (UINT32)ACTION::SEND_QUEUE_SENDQ_WRITEPOS + pSession->sendQ.GetWritePos());
            //Logging(pSession, (UINT32)ACTION::SEND_QUEUE_SENDQ_READPOS + pSession->sendQ.GetReadPos());


            // sendFlag�� ���� ����, sendQ�� ���� �����Ͱ� �ִ��� Ȯ���Ѵ�. 
            // �ٸ� �����忡�� recv �Ϸ������� ó���� �� sendQ�� enq �ϰ�, sendFlag�� �˻��ϱ⿡
            // �� �� �ϳ��� ������ sendFlag�� �ùٸ��� ����ȴ�.
            InterlockedExchange(&pSession->sendFlag, 0);

            

            // ���� sendQ�� �����Ͱ� �ִٸ�
            if (!pSession->sendQ.empty())
            {
                // send ó��
                Logging(pSession, ACTION::WORKER_SEND_HAS_DATA);
                Logging(pSession, ACTION::WORKER_SEND_SENDPOST_START);
                pThis->SendPost(pSession);
                Logging(pSession, ACTION::WORKER_SEND_SENDPOST_AFTER);
            }

            // ���� sendQ�� �����Ͱ� ���ٸ�
            else
            {
                Logging(pSession, ACTION::WORKER_SEND_NODATA);
                // �̹� sendFlag 0���� �ٲ����� �ƹ��͵� �� ���� ����.
                ;
            }

            //// �� �������� read, writepos ���
            //Logging(pSession, (UINT32)ACTION::SEND_QUEUE_SENDQ_WRITEPOS + pSession->sendQ.GetWritePos());
            //Logging(pSession, (UINT32)ACTION::SEND_QUEUE_SENDQ_READPOS + pSession->sendQ.GetReadPos());
        }

        // �Ϸ����� ó�� ���� IO Count 1 ����
        IOCount = InterlockedDecrement(&pSession->IOCount);
        Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

        // IO Count�� 0�� ���, ��� �Ϸ������� ó�������Ƿ� ������ �����ص� ����.
        if (IOCount == 0)
        {
            // ���� ����
            Logging(pSession, ACTION::WORKER_RELEASE_SESSION);
            pThis->returnSession(pSession);
            
            // AcceptEx ����ϰ� �ִ� ���� ���� Ȯ��
            if (pThis->IsAcceptExUnderLimit())
            {
                // AcceptEx�� ȣ���ϴ� �����̹Ƿ� curPendingSessionCnt 1 ���� ���� AcceptEx �õ�
                // ���� AcceptEx ���� �� curPendingSessionCnt �� ������Ű�� �� ���̿� ȣ�� Ƚ���� ������ �� ����
                // ���� �ٽ� ������ ����� �������� �پ��� �ϰ����� ���⼭ ������Ŵ���μ� AcceptEx ȣ�� Ƚ���� ������ ������ ����
                InterlockedIncrement(&pThis->curPendingSessionCnt);

                // ���� AcceptEx �� ���� ������ ������ ������ ���ٸ� ���� AcceptEx �غ�
                CSession* pNewSession = pThis->FetchSession();
                pThis->InitSessionInfo(pNewSession);

                while (!pThis->AcceptPost(pNewSession))
                {
                    closesocket(pNewSession->sock);
                    pNewSession->sock = INVALID_SOCKET;

                    pThis->InitSessionInfo(pNewSession);
                }
            }
        }
    }

    return 0;
}


// SOCKADDR_IN ����ü���� IP, PORT ���� �����ϴ� �Լ�
void CLanServer::extractIPPort(const SOCKADDR_IN& clientAddr, std::string& connectedIP, UINT16& connectedPort)
{
    char pAddrBuf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &clientAddr, pAddrBuf, sizeof(pAddrBuf)) == NULL) {
        std::cout << "Error : inet_ntop()\n";
        DebugBreak();
    }

    std::string strIP{ pAddrBuf };

    connectedIP = strIP;
    connectedPort = ntohs(clientAddr.sin_port);
}


unsigned int __stdcall CLanServer::AcceptThread(void* pArg)
{
    //CLanServer* pThis = (CLanServer*)pArg;

    //DWORD curThreadID = GetCurrentThreadId();

    //// ������ ��ſ� ����� ����
    //SOCKET client_sock;
    //SOCKADDR_IN clientaddr;

    //int addrlen;

    //while (true)
    //{
    //    // ��α� ť���� ���� ���� ����
    //    addrlen = sizeof(clientaddr);
    //    client_sock = accept(pThis->listenSocket, (SOCKADDR*)&clientaddr, &addrlen);
    //    if (client_sock == INVALID_SOCKET) {
    //        int error = WSAGetLastError();
    //        std::cout << "Error : accept failed - " << error << "\n";
    //        DebugBreak();
    //    }


    //    // AcceptTps 1 ����
    //    InterlockedIncrement(&pThis->acceptTPS);



    //    // ������ ������ ���� �������� IP, PORT �� ����
    //    std::string connectedIP;
    //    UINT16 connectedPort;
    //    pThis->extractIPPort(clientaddr, connectedIP, connectedPort);


    //    // ������ Ŭ���̾�Ʈ�� ���� ���� �������� ���� �Ǵ�
    //    if (!pThis->OnConnectionRequest(connectedIP, connectedPort))
    //        // ���� �Ұ��ϴٸ� continue
    //        continue;

    //    //==============================================================================================
    //    // ���� ����
    //    //==============================================================================================

    //    // ���� ���� ����ü �̾ƿ���
    //    CSession* pSession = pThis->FetchSession(); // �̾ƿ��鼭 ID�� �̹� �ο��� ����
    //    

    //    // ���ǿ� ����, IP, Port ���� �߰�
    //    pSession->sock = client_sock;

    //    strcpy_s(pSession->IP, sizeof(pSession->IP), connectedIP.c_str());
    //    pSession->IP[sizeof(pSession->IP) - 1] = '\0';

    //    pSession->port = connectedPort;


    //    Logging(pSession, ACTION::ACCEPT_AFTER_FETCH);

    //    // ���� ���� �ʱ�ȭ
    //    pThis->InitSessionInfo(pSession);

    //    Logging(pSession, ACTION::ACCEPT_AFTER_INIT);


    //    // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
    //    CreateIoCompletionPort((HANDLE)pSession->sock, pThis->hIOCP, (ULONG_PTR)pSession, 0);


    //    // send�� �����ϰ� recv�� �Ŵϱ� recv�� �ɱ����� �ٸ� �����忡�� send �Ϸ������� �ͼ� �����尡 �����Ǿ������ ��찡 ����.
    //    // �̸� �����ϱ� ���� OnAccept ȣ�� ���� IOCount�� ����, RecvPost ���� IOCount ����
    //    InterlockedIncrement(&pSession->IOCount);

    //    {
    //        // ������ accept �Լ� ���� -> ���ڰ� �ƴ� ��¥�� ���������� ���� ���� �� ��� ���.
    //        // OnAccept�� �����ϴ� ������ recvPost�� ���� �ɾ��� �� ��Ȱ���� �ϰ� �Ǹ� ������ �����.
    //        pThis->OnAccept(pSession->id);

    //        Logging(pSession, ACTION::ACCEPT_ON_ACCEPT);

    //        // recv ó��, �� ����� �����ϰ�, ���Ŀ� WSARecv�� �ϳĸ�, ��� ���� Recv �Ϸ������� �� �� ����. �׷��� ����� ������
    //        pThis->RecvPost(pSession);
    //    }

    //    UINT32 IOCount = InterlockedDecrement(&pSession->IOCount);

    //    Logging(pSession, ACTION::ACCEPT_RECVPOST);

    //    if (IOCount == 0)
    //        pThis->returnSession(pSession);
    //}

    return 0;
}

void CLanServer::ClearTPSValue(void)
{
    statusTPS.accept = InterlockedExchange(&acceptTPS, 0);
    statusTPS.recvMessage = InterlockedExchange(&recvMessageTPS, 0);
    statusTPS.sendMessage = InterlockedExchange(&sendMessageTPS, 0);

    statusTPS.CurPoolCount = stSessionIndex.GetCurPoolCount();
    statusTPS.MaxPoolCount = stSessionIndex.GetMaxPoolCount();
}

bool CLanServer::IsAcceptExUnderLimit(void)
{
    return InterlockedExchangeAdd(&curPendingSessionCnt, 0) < maxPendingSessionCnt;
}

int CLanServer::GetConnectedSessionCount()
{
    // �ش� ���� interlocked ���� �������⿡ ��Ȯ���� ����. ���ÿ뵵�� �뷫���� ������ ���
    return static_cast<int>(curSessionCnt);
}

int CLanServer::GetPendingSessionCount()
{
    // �ش� ���� interlocked ���� �������⿡ ��Ȯ���� ����. ���ÿ뵵�� �뷫���� ������ ���
    return static_cast<int>(curPendingSessionCnt);
}
