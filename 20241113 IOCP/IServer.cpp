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

    InterlockedIncrement(&pSession->IOCount);

    // �񵿱� recv ó���� ���� ���� recv�� �ɾ��. ���� �����ʹ� wsaBuf�� ����� �޸𸮿� ä���� ����.
    retVal = WSARecv(pSession->sock, recvBuf, bufCnt, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        InterlockedDecrement(&pSession->IOCount);

        // ���� �ʿ��� ���� ������ ����. 
        if (error == WSAECONNRESET || error == WSAECONNABORTED)
            return;

        std::cerr << "RecvPost : WSARecv - " << error << "\n"; // �� �κ��� ������ϴµ� � ������ ������ �𸣴ϱ� Ȯ�� �������� �־.
        // ���߿� ��Ƽ������ ������ ������ ��ġ�� �ʴ� OnError ������ �񵿱� ��¹����� ��ü ����.
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
        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_SOMEONE_SENDING));
        // �Ѿ
        return;
    }

    // sendMessageTPS 1 ����
    InterlockedIncrement(&sendMessageTPS);

    int retval;
    int error;

    WSABUF sendBuf[2];
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
        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_SEND0));

        // ���� WSASend�� ���ϴϱ� sendflag�� Ǯ������Ѵ�.
        InterlockedExchange(&pSession->sendFlag, 0);
        return;
    }


    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SERVERLIB_SENDPOST_REAL_SEND));


    DWORD flags{};

    InterlockedIncrement(&pSession->IOCount);

    retval = WSASend(pSession->sock, sendBuf, bufCnt, NULL, flags, &pSession->overlappedSend, NULL);

    error = WSAGetLastError();
    if (retval == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            ;
        else
        {
            // �����ʿ��� ���� ����.
            if (error == WSAECONNRESET || error == WSAECONNABORTED)
            {
                InterlockedDecrement(&pSession->IOCount);
            }
            else
                DebugBreak();
        }
    }
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
        ////pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_AFTER_GQCS));

        error = WSAGetLastError();

        ////pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_ENTER_CS));


        // PQCS�� ���� �Ϸ� ������(0, 0, NULL)�� �� ���. ��, ��(main ������)�� ���� ���� ���
        // �Ϸ������� �ִ� ���� �����带 ����.
        if ((transferredDataLen == 0 && pSession == 0 && pOverlapped == nullptr))
        {
            ////pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_LEAVE_CS1));
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
                //int a = 10;
            }
        }

        // recv �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedRecv) {

            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_COMPLETION_NOTICE));

            // WSARecv�� �����۸� ����ϱ⿡ ���ۿ� �����͸� �����ϰ�, ������� �������� �ʾҴ�. ��� ����� ������Ų��.
            pSession->recvQ.MoveRear(transferredDataLen);

            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_START_WHILE));
            int useSize = pSession->recvQ.GetUseSize();
            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION((UINT32)ACTION::RECV_QUEUE_USESIZE + useSize)));

            while (true)
            {
                // 1. RecvQ�� �ּ����� ����� �ִ��� Ȯ��. ������ [ ��� ������ �̻��� �����Ͱ� �ִ��� Ȯ�� ]�ϴ� ��.
                int useSize = pSession->recvQ.GetUseSize();
                if (useSize < sizeof(PACKET_HEADER))
                {
                    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_EXIT_WHILE1));
                    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION((UINT32)ACTION::RECV_QUEUE_USESIZE + useSize)));
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
                    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_EXIT_WHILE2));
                    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION((UINT32)ACTION::RECV_QUEUE_USESIZE + useSize)));
                    break;
                }

                // 4. RecvQ���� header�� len ũ�⸸ŭ �ӽ� ��Ŷ ���۸� �̴´�.
                CPacket Packet; // �� �Ŵ����� ���� ������ ��� ���. ����� �غ��� ������������ ���� ������ ��� ����ϰ� ����.
                int echoSendSize = header.bySize + sizeof(PACKET_HEADER);
                int recvQDeqRetVal = pSession->recvQ.Dequeue(Packet.GetFrontBufferPtr(), echoSendSize);
                Packet.MoveWritePos(echoSendSize);

                if (recvQDeqRetVal != echoSendSize)
                {
                    DebugBreak();
                }


                // ������ �ڵ� OnRecv�� ������ id�� ��Ŷ ������ �Ѱ���
                //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_ONRECV_START));

                Packet.MoveReadPos(sizeof(PACKET_HEADER));  // len ���̿� ���� ������ ����, netlib���� ����ϴ� ��Ŷ�� ���̿� ���� ������ ����.
                pThis->OnRecv(pSession->id, &Packet);  // ���̷ε常 ���� ��Ŷ ������ �������� �ѱ�

                //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_ONRECV_AFTER));
            }

            // recv ó��
            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_RECVPOST_START));
            pThis->RecvPost(pSession);
            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_RECV_RECVPOST_AFTER));
        }

        // send �Ϸ� ������ �� ���
        else if (pOverlapped == &pSession->overlappedSend) {
            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_COMPLETION_NOTICE));

            // sendFlag�� ���� ����, sendQ�� ���� �����Ͱ� �ִ��� Ȯ���Ѵ�. 
            // �ٸ� �����忡�� recv �Ϸ������� ó���� �� sendQ�� enq �ϰ�, sendFlag�� �˻��ϱ⿡
            // �� �� �ϳ��� ������ sendFlag�� �ùٸ��� ����ȴ�.

            // ������ ���� �Ϸ� �����Ƿ� sendQ�� �ִ� �����͸� ����
            pSession->sendQ.MoveFront(transferredDataLen);
            InterlockedExchange(&pSession->sendFlag, 0);
            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_FLAG0));


            // ���� sendQ�� �����Ͱ� �ִٸ�
            if (pSession->sendQ.GetUseSize() != 0)
            {
                //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_HAS_DATA));

                // send ó��
                //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_SENDPOST_START));
                pThis->SendPost(pSession);
                //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_SENDPOST_AFTER));
            }

            // ���� sendQ�� �����Ͱ� ���ٸ�
            else
            {
                // �̹� sendFlag 0���� �ٲ����� �ƹ��͵� �� ���� ����.
                //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_SEND_HAS_NODATA));
            }
        }

        // �Ϸ����� ó�� ���� IO Count 1 ����
        IOCount = InterlockedDecrement(&pSession->IOCount);
        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION((UINT32)ACTION::IOCOUNT_0 + IOCount)));

        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_LEAVE_CS3));

        // IO Count�� 0�� ���, ��� �Ϸ������� ó�������Ƿ� ������ �����ص� ����.
        if (IOCount == 0)
        {
            // ���� ����
            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_CALL_DELETE_SESSION2));
            pThis->returnSession(pSession);
        }
    }

    return 0;
}


// SOCKADDR_IN ����ü���� IP, PORT ���� �����ϴ� �Լ�
void extractIPPort(const SOCKADDR_IN& clientAddr, std::string& connectedIP, UINT16& connectedPort)
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
    CLanServer* pThis = (CLanServer*)pArg;

    DWORD curThreadID = GetCurrentThreadId();

    // ������ ��ſ� ����� ����
    SOCKET client_sock;
    SOCKADDR_IN clientaddr;

    int addrlen;

    while (true)
    {
        // ��α� ť���� ���� ���� ����
        addrlen = sizeof(clientaddr);
        client_sock = accept(pThis->listenSocket, (SOCKADDR*)&clientaddr, &addrlen);
        if (client_sock == INVALID_SOCKET) {
            std::cout << "accept()\n";
            break;
        }


        // AcceptTps 1 ����
        InterlockedIncrement(&pThis->acceptTPS);



        // ������ ������ ���� �������� IP, PORT �� ����
        std::string connectedIP;
        UINT16 connectedPort;
        extractIPPort(clientaddr, connectedIP, connectedPort);


        // ������ Ŭ���̾�Ʈ�� ���� ���� �������� ���� �Ǵ�
        if (!pThis->OnConnectionRequest(connectedIP, connectedPort))
            // ���� �Ұ��ϴٸ� continue
            continue;

        //==============================================================================================
        // ���� ����
        //==============================================================================================

        // ���� ���� ����ü �̾ƿ���
        CSession* pSession = pThis->FetchSession(); // �̾ƿ��鼭 ID�� �̹� �ο��� ����

        // ���ǿ� ����, IP, Port ���� �߰�
        pSession->sock = client_sock;
        pSession->IP = connectedIP;
        pSession->port = connectedPort;

        // ���� ���� �ʱ�ȭ
        pThis->InitSessionInfo(pSession);


        // ���ϰ� ����� �Ϸ� ��Ʈ ���� - ���� ���ߵ� ������ key�� �ش� ���ϰ� ����� ���� ����ü�� �����ͷ� �Ͽ� IOCP�� �����Ѵ�.
        CreateIoCompletionPort((HANDLE)pSession->sock, pThis->hIOCP, (ULONG_PTR)pSession, 0);


        // recv ó��, �� ����� �����ϰ�, ���Ŀ� WSARecv�� �ϳĸ�, ��� ���� Recv �Ϸ������� �� �� ����. �׷��� ����� ������
        pThis->RecvPost(pSession);


        // ������ accept �Լ� ���� -> ���ڰ� �ƴ� ��¥�� ���������� ���� ���� �� ��� ���.
        pThis->OnAccept(pSession->id);



        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_AFTER_NEW));

        
        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_CONNECT_IOCP));


        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_RECVPOST));


       

        // �� �������� IO Count�� �̱� �����峪 �ٸ� ���⿡ interlock���� ������ �˻� ����.
        if (pSession->IOCount == 0)
        {
            // ���� WSARecv�� �����ߴٸ� �Ϸ������� ���� �ʱ⿡ ���⼭ �ٷ� ������ ����������Ѵ�.
            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::WORKER_CALL_DELETE_SESSION3));

            // ������ ����, �������� ������ ���� ������ �˸�
            pThis->returnSession(pSession);

            //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_DELETE_SESSION));
        }

        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::ACCEPT_EXIT));
    }

    return 0;
}

void CLanServer::ClearTPSValue(void)
{
    InterlockedExchange(&acceptTPS, 0);
    InterlockedExchange(&recvMessageTPS, 0);
    InterlockedExchange(&sendMessageTPS, 0);
}
