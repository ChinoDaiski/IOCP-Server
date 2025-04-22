
#include "pch.h"
#include "Protocol.h"
#include "IServer.h"
#include "Session.h"
#include "Packet.h"

void CLanServer::RecvPost(CSession* pSession)
{
    // recvMessageTPS 1 증가
    InterlockedIncrement(&recvMessageTPS);

    int retVal;
    int error;

    DWORD flags{};

    WSABUF recvBuf[2];
    int bufCnt = pSession->recvQ.makeWSARecvBuf(recvBuf);

    UINT32 IOCount = InterlockedIncrement(&pSession->IOCount);
    Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

    // 비동기 recv 처리를 위해 먼저 recv를 걸어둠. 받은 데이터는 wsaBuf에 등록한 메모리에 채워질 예정.
    retVal = WSARecv(pSession->sock, recvBuf, bufCnt, NULL, &flags, &pSession->overlappedRecv, NULL);

    error = WSAGetLastError();

    if (retVal == SOCKET_ERROR)
    {
        if (error == ERROR_IO_PENDING)
            return;

        else
        {
            Logging(pSession, ACTION::SENDPOST_WSARECV_FAIL_NOPENDING);

            // 상대방 쪽에서 먼저 연결을 끊음. 이유 확실. 이때도 완료통지는 발생함.
            // 정확힌 상대방쪽에서 연결을 끊었다고 한들 내쪽에서 큐에 작업을 등록했다면 완료통지가 옴.
            /*if (error == WSAECONNRESET || error == WSAECONNABORTED)
                return;*/

            // 완료통지 처리 이후 IO Count 1 감소
            UINT32 IOCount = InterlockedDecrement(&pSession->IOCount);

            Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

            Logging(pSession, error);

            //std::cerr << "RecvPost : WSARecv - " << error << "\n"; // 이 부분은 없어야하는데 어떤 오류가 생길지 모르니깐 확인 차원에서 넣어봄.
            // 나중엔 멀티스레드 로직에 영향을 미치지 않는 OnError 등으로 비동기 출력문으로 교체 예정.
        }
    }
}

// sendQ에 데이터를 넣고, WSASend를 호출
void CLanServer::SendPost(CSession* pSession)
{
    DWORD curThreadID = GetCurrentThreadId();

    // 보낼 것이 있을 때 sendflag 확인함.
    // sending 하는 중이라면
    if (InterlockedExchange(&pSession->sendFlag, 1) == 1)
    {
        // 넘어감
        Logging(pSession, ACTION::SERVERLIB_SENDPOST_SOMEONE_SENDING);
        return;
    }
    Logging(pSession, ACTION::SERVERLIB_SENDPOST_REAL_SEND);

    // sendMessageTPS 1 증가
    InterlockedIncrement(&sendMessageTPS);

    int retval;
    int error;

    WSABUF sendBuf[MAX_PACKET_QUEUE_SIZE];
    int bufCnt = pSession->sendQ.makeWSASendBuf(sendBuf);

    // A 스레드가 사이즈를 확인하고 아직 interlock을 호출하지 않은 상황에서, 컨텍스트 스위칭. 
    // B 스레드가 recvPost 진행하면서 sendflag를 1로 바꾸고 진행
    // C 스레드가 send 완료통지를 받아 sendflag를 0으로 바꿈
    // A 스레드가 일어나서 interlock으로 0 에서 1로 바꾸고 send 진행. 근데 이미 send 되어버린 상황에서 send 0이 나올 수 있음.
    // 이러면 우리 구조에서 recv 0일때 소켓을 제거하기에 이를 해결하기 위해서 send시 0인 상황을 확인해서 진행을 도중에 멈춤.
    int sendLen = 0;
    for (int i = 0; i < bufCnt; ++i)
    {
        sendLen += sendBuf[i].len;
    }

    if (sendLen == 0)
    {
        Logging(pSession, ACTION::SERVERLIB_SENDPOST_SEND0);
        
        // 실제 WSASend를 안하니깐 sendflag를 풀어줘야한다.
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

            // 상대방 쪽에서 먼저 연결을 끊음. 이유 확실. 
            // 정확힌 상대방쪽에서 연결을 끊었다고 한들 내쪽에서 큐에 작업을 등록했다면 완료통지가 옴.
            /*if (error == WSAECONNRESET || error == WSAECONNABORTED)
                return;*/

            // 여기서 실패할 경우 완료통지 자체가 오지 않음.

            //// 완료통지 처리 이후 IO Count 1 감소
            UINT32 IOCount = InterlockedDecrement(&pSession->IOCount);

            Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

            Logging(pSession, error);
            
            //std::cerr << "SendPost : WSASend - " << error << "\n"; // 이 부분은 없어야하는데 어떤 오류가 생길지 모르니깐 확인 차원에서 넣어봄.
            // 나중엔 멀티스레드 로직에 영향을 미치지 않는 OnError 등으로 비동기 출력문으로 교체 예정.
        }
    }
}

bool CLanServer::AcceptPost(CSession* pSession)
{ 
    // 1) 새 소켓 생성
    pSession->sock = WSASocket(
        AF_INET, SOCK_STREAM, IPPROTO_TCP,
        nullptr, 0,
        WSA_FLAG_OVERLAPPED
    );

    if (pSession->sock == INVALID_SOCKET)
        return false;

    // socket과 CSession을 CompletionPort 객체와 연결
    CreateIoCompletionPort(
        (HANDLE)pSession->sock,
        hIOCP,
        (ULONG_PTR)pSession,
        0
    );

    if (hIOCP == INVALID_HANDLE_VALUE)
    {
        printf("[에러] CreateIoCompletionPort()함수 실패: %d\n", GetLastError());
        return false;
    }

    // 2) AcceptEx 호출
    DWORD recvBytes = 0;
    BOOL ok = AcceptEx(
        listenSocket,               // 듣고 있던 리슨 소켓
        pSession->sock,             // AcceptEx가 새로 생성한(위에서 만든) 소켓
        pSession->acceptBuffer,     // 리모트/로컬 주소를 받을 버퍼
        0,                          // 첫 데이터 수신을 하지 않으므로 0
        sizeof(SOCKADDR_IN) + 16,   // 로컬 주소 공간 크기
        sizeof(SOCKADDR_IN) + 16,   // 리모트 주소 공간 크기
        &recvBytes,                 // 동기 모드라면 수신된 바이트, 비동기면 무시
        &pSession->overlappedAccept // OVERLAPPED 구조체
    );

    // ok == true : AcceptEx가 즉시 완료된 경우
    // ok == false && WSAGetLastError() == ERROR_IO_PENDING : 비동기 완료 대기 상태, GQCS로 완료통지 처리 준비 완료

    // 그 외의 경우
    if (!ok && WSAGetLastError() != ERROR_IO_PENDING)
    {
        // 연결이 실패 했으므로 false 반환
        // 소켓 닫기 및 뒤처리는 returnSession에서 진행 예정
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


        // PQCS로 넣은 완료 통지값(0, 0, NULL)이 온 경우. 즉, 내(main 스레드)가 먼저 끊은 경우
        // 완료통지가 있던 말던 스레드를 종료.
        if ((transferredDataLen == 0 && pSession == 0 && pOverlapped == nullptr))
        {
            break;
        }

        // 예외 처리 - GQCS가 실패한 경우, send,recv 하던중 문제가 생겨서 closesocket을 해서 해당 소켓을 사용하는 완료통지가 모두 실패한 경우이다.
        // transferredDataLen이 0인 경우(rst가 온 경우) <- 이건 send 쪽에서 0을 정상적으로 보낼 수 있으니 send에서 0을 보내는 경우는 막도록 처리
        if (retval == FALSE || transferredDataLen == 0)
        {
            // 상대쪽에서 명시적으로 closesocket이나 shutdown을 호출하지 않고 랜뽑이나 프로세스의 강제종료를 하는 경우 rst가 서버로 오지 않는다.
            // 이때 서버쪽에서 send/recv를 시도하면 나는 에러가 ERROR_NETNAME_DELETED 이다.
            if (error == ERROR_NETNAME_DELETED)
            {
                ;   // 바로 IO Count를 1 감소시키는 쪽으로 넘어가서 처리.
            }

            if (pSession)
            {
                Logging(pSession, ACTION::WORKER_LEAVE_CS1);
            }
        }

        // 문제가 없는데 pSeesion이 nullptr인 경우, AcceptEx로 인한 완료통지이기 때문에 pOverlapped를 확인하여 세션값을 찾는다. 
        if (!pSession)
        {
            pSession = reinterpret_cast<CSession*>(
                reinterpret_cast<char*>(pOverlapped)
                - offsetof(CSession, overlappedAccept)
                );
        }

        // 1) AcceptEx 완료 처리
        if (pOverlapped == &pSession->overlappedAccept)
        {
            // AcceptTps 1 증가
            InterlockedIncrement(&pThis->acceptTPS);

            // 주소 정보 꺼내기
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

            // 연결 허용 검사
            if (!pThis->OnConnectionRequest(connectedIP, connectedPort))
            {
                // 허용된 ip가 아닐 시 해당 소켓을 닫고, 새로 AcceptEx 시도
                closesocket(pSession->sock);
                pSession->sock = INVALID_SOCKET;

                // AcceptEx 대기하고 있는 소켓 갯수 확인
                if (pThis->IsAcceptExUnderLimit())
                {
                    // 다음 AcceptEx 준비
                    pThis->InitSessionInfo(pSession);

                    while (!pThis->AcceptPost(pSession))
                    {
                        closesocket(pSession->sock);
                        pSession->sock = INVALID_SOCKET;

                        pThis->InitSessionInfo(pSession);
                    }
                }

                // 세션을 삭제할 필요 없이 소켓만 새로 할당받아 재사용하면 되기에 이렇게 처리
                continue;
            }

            // 이 시점에선 이미 pSeesion은 Init이 된 상태

            // 세션 정보 업데이트 & IOCP 등록

            // 세션에 IP, Port 정보 추가
            strcpy_s(pSession->IP, sizeof(pSession->IP), connectedIP.c_str());
            pSession->IP[sizeof(pSession->IP) - 1] = '\0';

            pSession->port = connectedPort;

            // 소켓과 입출력 완료 포트 연결 - 새로 연견될 소켓을 key를 해당 소켓과 연결된 세션 구조체의 포인터로 하여 IOCP와 연결한다.
            CreateIoCompletionPort(
                (HANDLE)pSession->sock,
                pThis->hIOCP,
                (ULONG_PTR)pSession,
                0
            );

            // send를 먼저하고 recv를 거니깐 recv를 걸기전에 다른 스레드에서 send 완료통지가 와서 스레드가 삭제되어버리는 경우가 있음.
            // 이를 방지하기 위해 OnAccept 호출 전에 IOCount를 증가, RecvPost 이후 IOCount 감소
            InterlockedIncrement(&pSession->IOCount);

            // OnAccept 및 RecvPost 호출(recv 예약)
            {
                // 콘텐츠 accept 함수 생성 -> 에코가 아닌 진짜로 콘텐츠에서 뭔가 만들 때 등록 요망.
                // OnAccept를 먼저하는 이유가 recvPost를 먼저 걸었을 때 재활용을 하게 되면 문제가 생긴다.
                pThis->OnAccept(pSession->id);

                // recv 처리, 왜 등록을 먼저하고, 이후에 WSARecv를 하냐면, 등록 전에 Recv 완료통지가 올 수 있음. 그래서 등록을 먼저함
                pThis->RecvPost(pSession);
            }

            //UINT32 IOCount = InterlockedDecrement(&pSession->IOCount);

            //// 이거 확인해보자. 가능한 경우인지 확인해야함. 만약 가능한 경우라면, 여기서 세션 재활용들어가야함.
            //// 일단 가능한 것으로 판별됨. 로그 다 찍어서 확인해봐야할듯... 와우
            //if (IOCount == 0)
            //    DebugBreak();
        }

        // recv 완료 통지가 온 경우
        else if (pOverlapped == &pSession->overlappedRecv) {

            // WSARecv는 링버퍼를 사용하기에 버퍼에 데이터만 존재하고, 사이즈는 증가하지 않았다. 고로 사이즈를 증가시킨다.
            pSession->recvQ.MoveRear(transferredDataLen);

            int useSize = pSession->recvQ.GetUseSize();

            Logging(pSession, ACTION::WORKER_RECV_START_WHILE);
            while (true)
            {
                // 1. RecvQ에 최소한의 사이즈가 있는지 확인. 조건은 [ 헤더 사이즈 이상의 데이터가 있는지 확인 ]하는 것.
                int useSize = pSession->recvQ.GetUseSize();
                if (useSize < sizeof(PACKET_HEADER))
                {
                    Logging(pSession, ACTION::WORKER_RECV_EXIT_WHILE1);
                    break;
                }


                // 2. RecvQ에서 PACKET_HEADER 정보 Peek
                PACKET_HEADER header;
                int headerSize = sizeof(header);

                int retVal = pSession->recvQ.Peek(reinterpret_cast<char*>(&header), headerSize);

                // 3. 헤더의 len값과 RecvQ의 데이터 사이즈 비교
                useSize = pSession->recvQ.GetUseSize();
                if ((header.bySize + sizeof(PACKET_HEADER)) > useSize)
                {
                    Logging(pSession, ACTION::WORKER_RECV_EXIT_WHILE2);
                    break;
                }

                // 4. RecvQ에서 header의 len 크기만큼 임시 패킷 버퍼를 뽑는다.
                CPacket* Packet = new CPacket; // 힙 매니저가 같은 공간을 계속 사용. 디버깅 해보니 지역변수지만 같은 영역을 계속 사용하고 있음.
                Packet->AddRef();

                int echoSendSize = header.bySize + sizeof(PACKET_HEADER);
                int recvQDeqRetVal = pSession->recvQ.Dequeue(Packet->GetFrontBufferPtr(), echoSendSize);
                Packet->MoveWritePos(echoSendSize);

                if (recvQDeqRetVal != echoSendSize)
                {
                    DebugBreak();
                }

                // 콘텐츠 코드 OnRecv에 세션의 id와 패킷 정보를 넘겨줌
                Packet->MoveReadPos(sizeof(PACKET_HEADER));  // len 길이에 대한 정보를 지움, netlib에서 사용하는 패킷의 길이에 대한 정보를 날림.
                Logging(pSession, ACTION::WORKER_RECV_ONRECV_START);
                pThis->OnRecv(pSession->id, Packet);  // 페이로드만 남은 패킷 정보를 컨텐츠에 넘김
                Logging(pSession, ACTION::WORKER_RECV_ONRECV_AFTER);


                // 패킷의 release 함수를 호출해서 refCounter를 1줄이고, 줄여진 값이 0인 것을 확인
                if (Packet->ReleaseRef() == 0)
                {
                    // 0이라면 패킷을 제거
                    delete Packet;
                }
            }

            // recv 처리
            Logging(pSession, ACTION::WORKER_RECV_RECVPOST_START);
            pThis->RecvPost(pSession);
            Logging(pSession, ACTION::WORKER_RECV_RECVPOST_AFTER);
        }

        // send 완료 통지가 온 경우
        else if (pOverlapped == &pSession->overlappedSend) {
            
            // 보내는 것을 완료 했으므로 sendQ에 있는 데이터를 제거

            // 인자로 받은 데이터의 크기를 보고 안에 있는 패킷을 pop.
            pSession->sendQ.RemoveSendCompletePacket(transferredDataLen);

            //// 이 시점에서 read, writepos 기록
            //Logging(pSession, (UINT32)ACTION::SEND_QUEUE_SENDQ_WRITEPOS + pSession->sendQ.GetWritePos());
            //Logging(pSession, (UINT32)ACTION::SEND_QUEUE_SENDQ_READPOS + pSession->sendQ.GetReadPos());


            // sendFlag를 먼저 놓고, sendQ에 보낼 데이터가 있는지 확인한다. 
            // 다른 스레드에서 recv 완료통지를 처리할 때 sendQ에 enq 하고, sendFlag를 검사하기에
            // 둘 중 하나는 무조건 sendFlag가 올바르게 적용된다.
            InterlockedExchange(&pSession->sendFlag, 0);

            

            // 만약 sendQ에 데이터가 있다면
            if (!pSession->sendQ.empty())
            {
                // send 처리
                Logging(pSession, ACTION::WORKER_SEND_HAS_DATA);
                Logging(pSession, ACTION::WORKER_SEND_SENDPOST_START);
                pThis->SendPost(pSession);
                Logging(pSession, ACTION::WORKER_SEND_SENDPOST_AFTER);
            }

            // 만약 sendQ에 데이터가 없다면
            else
            {
                Logging(pSession, ACTION::WORKER_SEND_NODATA);
                // 이미 sendFlag 0으로 바꿨으니 아무것도 할 것이 없다.
                ;
            }

            //// 이 시점에서 read, writepos 기록
            //Logging(pSession, (UINT32)ACTION::SEND_QUEUE_SENDQ_WRITEPOS + pSession->sendQ.GetWritePos());
            //Logging(pSession, (UINT32)ACTION::SEND_QUEUE_SENDQ_READPOS + pSession->sendQ.GetReadPos());
        }

        // 완료통지 처리 이후 IO Count 1 감소
        IOCount = InterlockedDecrement(&pSession->IOCount);
        Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

        // IO Count가 0인 경우, 모든 완료통지를 처리했으므로 세션을 삭제해도 무방.
        if (IOCount == 0)
        {
            // 세션 삭제
            Logging(pSession, ACTION::WORKER_RELEASE_SESSION);
            pThis->returnSession(pSession);
            
            // AcceptEx 대기하고 있는 소켓 갯수 확인
            if (pThis->IsAcceptExUnderLimit())
            {
                // AcceptEx를 호출하는 과정이므로 curPendingSessionCnt 1 증가 이후 AcceptEx 시도
                // 만약 AcceptEx 성공 후 curPendingSessionCnt 을 증가시키면 그 사이에 호출 횟수가 증가할 수 있음
                // 물론 다시 세션이 끊기는 과정에서 줄어들긴 하겠지만 여기서 증가시킴으로서 AcceptEx 호출 횟수가 증가할 위험을 줄임
                InterlockedIncrement(&pThis->curPendingSessionCnt);

                // 만약 AcceptEx 건 소켓 갯수가 설정한 값보다 적다면 다음 AcceptEx 준비
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


// SOCKADDR_IN 구조체에서 IP, PORT 값을 추출하는 함수
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

    //// 데이터 통신에 사용할 변수
    //SOCKET client_sock;
    //SOCKADDR_IN clientaddr;

    //int addrlen;

    //while (true)
    //{
    //    // 백로그 큐에서 소켓 정보 추출
    //    addrlen = sizeof(clientaddr);
    //    client_sock = accept(pThis->listenSocket, (SOCKADDR*)&clientaddr, &addrlen);
    //    if (client_sock == INVALID_SOCKET) {
    //        int error = WSAGetLastError();
    //        std::cout << "Error : accept failed - " << error << "\n";
    //        DebugBreak();
    //    }


    //    // AcceptTps 1 증가
    //    InterlockedIncrement(&pThis->acceptTPS);



    //    // 추출한 소켓의 접속 정보에서 IP, PORT 값 추출
    //    std::string connectedIP;
    //    UINT16 connectedPort;
    //    pThis->extractIPPort(clientaddr, connectedIP, connectedPort);


    //    // 접속한 클라이언트가 현재 접속 가능한지 여부 판단
    //    if (!pThis->OnConnectionRequest(connectedIP, connectedPort))
    //        // 접속 불가하다면 continue
    //        continue;

    //    //==============================================================================================
    //    // 세션 생성
    //    //==============================================================================================

    //    // 소켓 정보 구조체 뽑아오기
    //    CSession* pSession = pThis->FetchSession(); // 뽑아오면서 ID는 이미 부여된 상태
    //    

    //    // 세션에 소켓, IP, Port 정보 추가
    //    pSession->sock = client_sock;

    //    strcpy_s(pSession->IP, sizeof(pSession->IP), connectedIP.c_str());
    //    pSession->IP[sizeof(pSession->IP) - 1] = '\0';

    //    pSession->port = connectedPort;


    //    Logging(pSession, ACTION::ACCEPT_AFTER_FETCH);

    //    // 세션 정보 초기화
    //    pThis->InitSessionInfo(pSession);

    //    Logging(pSession, ACTION::ACCEPT_AFTER_INIT);


    //    // 소켓과 입출력 완료 포트 연결 - 새로 연견될 소켓을 key를 해당 소켓과 연결된 세션 구조체의 포인터로 하여 IOCP와 연결한다.
    //    CreateIoCompletionPort((HANDLE)pSession->sock, pThis->hIOCP, (ULONG_PTR)pSession, 0);


    //    // send를 먼저하고 recv를 거니깐 recv를 걸기전에 다른 스레드에서 send 완료통지가 와서 스레드가 삭제되어버리는 경우가 있음.
    //    // 이를 방지하기 위해 OnAccept 호출 전에 IOCount를 증가, RecvPost 이후 IOCount 감소
    //    InterlockedIncrement(&pSession->IOCount);

    //    {
    //        // 콘텐츠 accept 함수 생성 -> 에코가 아닌 진짜로 콘텐츠에서 뭔가 만들 때 등록 요망.
    //        // OnAccept를 먼저하는 이유가 recvPost를 먼저 걸었을 때 재활용을 하게 되면 문제가 생긴다.
    //        pThis->OnAccept(pSession->id);

    //        Logging(pSession, ACTION::ACCEPT_ON_ACCEPT);

    //        // recv 처리, 왜 등록을 먼저하고, 이후에 WSARecv를 하냐면, 등록 전에 Recv 완료통지가 올 수 있음. 그래서 등록을 먼저함
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
    // 해당 값을 interlocked 없이 가져오기에 정확하지 않음. 감시용도로 대략적인 측정시 사용
    return static_cast<int>(curSessionCnt);
}

int CLanServer::GetPendingSessionCount()
{
    // 해당 값을 interlocked 없이 가져오기에 정확하지 않음. 감시용도로 대략적인 측정시 사용
    return static_cast<int>(curPendingSessionCnt);
}
