
#include "pch.h"
#include "ServerLib.h"

#include <process.h>

#include "Session.h"
#include "Protocol.h"

bool CGameServer::Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, UINT16 maxSessionCount)
{
    //=================================================================================================================
    // 1. 서버 소켓 초기화 및 IOCP 연결
    //=================================================================================================================

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
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, runningThreadCount);
    if (hIOCP == NULL)
    {
        std::cerr << "INVALID_HANDLE : hIOCP\n";
        return false;
    }

    // socket()
    listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket == INVALID_SOCKET)
    {
        std::cerr << "INVALID_SOCKET : listen_sock\n";
        return false;
    }


    // sendbuf 크기 설정 (송신 버퍼의 크기를 0으로 설정해 중간 단계 버퍼링 없이 메모리에 직접 direct I/O를 유도)
    int recvBufSize = 0;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR) {
        std::cerr << "setsockopt failed with error: " << WSAGetLastError() << std::endl;
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        WSACleanup();
        return false;
    }


    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = ip; //inet_addr(ip);   htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    retval = bind(listenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
    if (retval == SOCKET_ERROR)
        std::cout << "bind()\n";

    // listen()
    retval = listen(listenSocket, SOMAXCONN);
    if (retval == SOCKET_ERROR)
        std::cout << "listen()\n";



    //=================================================================================================================
    // 2. 세션 관리 초기화
    //=================================================================================================================
    
    // 여기서 세션에 관련된 모든 정보들을 초기화. 만약 풀이 필요하다면 관련 코드 삽입
    maxConnections = maxSessionCount;
    sessions = new CSession[maxConnections];


    

    //=================================================================================================================
    // 3. 서버 가동에 필요한 정보를 로드
    //=================================================================================================================
    isServerMaintrenanceMode = true;

    InitializeCriticalSection(&cs_sessionID);

    // stack 방식으로 세션 ID의 배열 정보를 저장( 0 ~ maxConnections 사이의 값을 인덱스로 사용 )
    for (int i = maxConnections - 1; i >= 0; --i)
    {
        stSessionIndex.push(i);
    }




    //=================================================================================================================
    // 4. 스레드 풀 생성
    //=================================================================================================================
    threadCnt = workerThreadCount;
    hThreads = new HANDLE[threadCnt + 1];

    DWORD dwThreadID;

    // Accept Thread 호출
    hThreads[0] = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, (unsigned int*)&dwThreadID);

    // Worker Threads 호출
    for (UINT8 i = 0; i < workerThreadCount; ++i)
    {
        hThreads[i + 1] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, this, 0, (unsigned int*)&dwThreadID);
    }

    return true;  // 성공 시 true 반환
}

void CGameServer::Stop(void)
{
    // 모든 스레드 종료 및 리소스 정리

    // ===========================================================================
    // 1. 모든 스레드가 종료되기를 대기
    // ===========================================================================
    DWORD retVal = WaitForMultipleObjects(threadCnt, hThreads, TRUE, INFINITE);
    DWORD retError = GetLastError();

    // ===========================================================================
    // 2. 리소스 정리
    // ===========================================================================
    Release();

    // ===========================================================================
    // 3. 스레드가 정상 종료되었는지 검사
    // ===========================================================================
    DWORD ExitCode;

    wprintf(L"\n\n--- THREAD CHECK LOG -----------------------------\n\n");

    GetExitCodeThread(hThreads[0], &ExitCode);
    if (ExitCode != 0)
        wprintf(L"error - Accept Thread not exit\n");

    for (UINT8 i = 1; i < threadCnt; ++i)
    {
        GetExitCodeThread(hThreads[i], &ExitCode);
        if (ExitCode != 0)
            wprintf(L"error - IO Thread not exit\n");
    }

    // 윈속 종료
    WSACleanup();
}

void CGameServer::Release(void)
{
    // 세션 삭제
    delete[] sessions;

    DeleteCriticalSection(&cs_sessionID);
}

bool CGameServer::Disconnect(UINT64 sessionID)
{
    // 세션 종료 처리, 컨텐츠 쪽에서 먼저 호출하는 함수로 컨텐츠 객체가 소멸되고 나서 서버에 알려주는 형식.
    // 서버에선 완료통지가 모두 와야 삭제되므로 우선 삭제 예정인 flag가 올려두고 IO Count가 0이 되면 closesocket과 삭제를 한다.
    
    

    return true;
}

void CGameServer::SendPacket(UINT64 sessionID, CPacket* pPacket)
{
    DWORD curThreadID = GetCurrentThreadId();

    // 세션 검색
    UINT16 index = static_cast<UINT16>(sessionID);
    CSession* pSession = &sessions[index];

    // 세션을 찾았다면
    if (pSession->id == sessionID)
    {
        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_FIND));

        // 패킷 선언
        CPacket packet;

        // 헤더 정보 삽입
        PACKET_HEADER header;
        header.bySize = pPacket->GetDataSize();
        packet.PutData((char*)&header, sizeof(PACKET_HEADER));

        // 인자로 받은 패킷의 데이터 삽입
        packet.PutData(pPacket->GetFrontBufferPtr(), pPacket->GetDataSize());

        // sendQ에 넣음
        pSession->sendQ.Enqueue(packet.GetFrontBufferPtr(), packet.GetDataSize());

        //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_ENQ));
    }
    // 아니라면 재활용되면서 이상한 값이 나올 수 있다.
    else
    {
        DebugBreak();
    }

    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_START_SENDPOST));

    SendPost(pSession);

    //pSession->debugQueue.enqueue(std::make_pair(curThreadID, ACTION::SENDPACKET_AFTER_SENDPOST));
}

// 세션이 만들어지기 전에 허락을 받는 함수. 서버 테스트나 점검 등이 필요할 때 사내 IP만 접속할 수 있도록 인원을 제한하는 방식.
bool CGameServer::OnConnectionRequest(const std::string& ip, int port)
{
    // 지금은 테스트를 위해 일단 true를 반환
    return true;

    // 인자로 접속하는 클라이언트의 IP와 PORT를 넘기고, 이 함수 안에서 특정 IP의 접속에 대한 것을 처리한다.

    // 처음 서버에 접속했을 경우
    if (isServerMaintrenanceMode)
    {
        // 접속한 IP가 allowedIPs에 등록된 IP일 경우 접속을 허용, 아닐 경우 차단
        const auto& iter = allowedIPs.find(ip);

        if (iter != allowedIPs.end())
            return true;
    }
    // 서버 점검이 끝난 경우
    else
    {
        // 접속한 IP가 blockedIPs에 등록된 IP가 아닐 경우 접속을 허용, 아닐 경우 차단
        const auto& iter = blockedIPs.find(ip);

        if (iter == blockedIPs.end())
            return true;
    }

    return false;
}

void CGameServer::OnAccept(UINT64 sessionID)
{
    // 더미 테스트를 위해 처음 접속할 때 정해진 data를 클라에 전송.
    CPacket packet;
    UINT64 data = 0x7fffffffffffffff;
    packet.PutData((char*)&data, sizeof(data));

    // 패킷을 보내는 함수 
    SendPacket(sessionID, &packet);
}

void CGameServer::OnRelease(UINT64 sessionID)
{
    // 해당 세션 id를 가진 플레이어를 검색

    // 해당 플레이어를 삭제

    // 여기선 에코 테스트 서버니깐 아무것도 하지 않음.
}

void CGameServer::OnRecv(UINT64 sessionID, CPacket* pPacket)
{
    // 여기서 컨텐츠 처리가 들어감.
    // ~ 컨텐츠 ~
    // 컨텐츠 처리를 하면서 패킷을 만들어 SendPacket를 호출
    // sendQ에 데이터를 삽입. + sending 여부를 판단해 sendQ에 넣은 데이터를 보낼 수 있다면 보낸다.

    SendPacket(sessionID, pPacket);
}

CSession* CGameServer::FetchSession(void)
{
    UINT64 sessionID = 0;

    // 상위 6byte는 접속마다 1씩 증가하는 uniqueSessionID 값을 넣는다.

    // 전체 64bit중 상위 48bit를 사용
    sessionID = uniqueSessionID;
    sessionID <<= 16;

    // 사용한 uniqueSessionID는 중복 방지를 위해 1증가
    uniqueSessionID++;

    EnterCriticalSection(&cs_sessionID);

    // 사용하지 않는 배열 인덱스를 찾음
    UINT16 sessionIndex = stSessionIndex.top();
    stSessionIndex.pop();

    LeaveCriticalSection(&cs_sessionID);

    // 세션접속 수 1 증가
    InterlockedIncrement(&curSessionCnt);

    sessionID |= sessionIndex;
    sessions[sessionIndex].id = sessionID;

    return &sessions[sessionIndex];
}

void CGameServer::returnSession(CSession* pSession)
{
    // 소켓 close
    closesocket(pSession->sock);
    pSession->sock = INVALID_SOCKET;

    EnterCriticalSection(&cs_sessionID);

    // 인자로 받은 세션이 위치한 배열 인덱스를 반환
    stSessionIndex.push(static_cast<UINT16>(pSession->id));

    LeaveCriticalSection(&cs_sessionID);

    // 세션접속 수 1 감소
    InterlockedDecrement(&curSessionCnt);

    InterlockedIncrement(&disconnectedSessionCnt);

    // 세션을 삭제했으니 이후에 콘텐츠에게 세션이 삭제되었음을 알리는 코드가 들어감
    OnRelease(pSession->id);
}

void CGameServer::InitSessionInfo(CSession* pSession)
{
    // send/recv용 오버랩 구조체 초기화
    ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
    ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));

    // 삭제를 위한 IO Count 부여, 초기화 할 시기로 아직 등록도 전이니 0으로 설정
    pSession->IOCount = 0;

    // send를 1회 제한하기 위한 flag 변수
    pSession->sendFlag = 0;

    // 링버퍼 초기화
    pSession->sendQ.ClearBuffer();
    pSession->recvQ.ClearBuffer(); 
    
    // 해당 세션이 살아있는지 여부, 첫 값은 TRUE로 컨텐츠 쪽에서 끊었을 경우에만 0으로 변경한다.
    pSession->isAlive = 1;
}

void CGameServer::loadIPList(const std::string& filePath, std::unordered_set<std::string>& IPList)
{
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open file " << filePath << std::endl;
        return;
    }

    std::string line;
    while (std::getline(inputFile, line)) {
        if (!line.empty()) {
            IPList.emplace(line);
        }
    }

    inputFile.close();

    return;
}
