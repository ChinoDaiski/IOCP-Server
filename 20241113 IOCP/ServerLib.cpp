
#include "pch.h"
#include "ServerLib.h"

#include <process.h>

#include "Session.h"
#include "Protocol.h"
#include "Packet.h"

#include "Managers.h"
#include "ContentManager.h"

bool CGameServer::Start(unsigned long ip, int port, int workerThreadCount, int runningThreadCount, bool nagleOption, int maxSessionCount, int pendingAcceptCount)
{
    // 타이머 인터벌을 1ms로 줄임. 서버의 퀀텀이 길기에 타이머 인터벌을 줄여 비동기 작업의 지연이 퀀텀이 길어서 확인되까지 걸리는 시간을 줄임으로서 낭비하는 시간이 없어져 성능이 올라간다고 추측.
    // 아무튼 안했을 때 보다 했을 때 서버에서 성능이 30%이상 올라가는 것을 확인. 물론 에코인 경우지만...
    timeBeginPeriod(1);

    //=================================================================================================================
    // 1. 서버 소켓 초기화 및 IOCP 연결
    //=================================================================================================================

    // winSock 초기화
    InitWinSock();

    // listenSocket 생성
    CreateListenSocket(PROTOCOL_TYPE::TCP_IP);

    // IOCP handle 생성
    CreateIOCP(runningThreadCount);

    //=================================================================================================================
    // 2. 서버 소켓 옵션 설정 및 bind / listen 
    //=================================================================================================================
    
    // 소켓 옵션 조정
    SetOptions(nagleOption);

    // listen 소켓에 서버에 접속할 NIC, PORT 번호 bind
    Bind(ip, port);

    // listen 소켓 생성
    Listen();

    //=================================================================================================================
    // 3. 서버 가동에 필요한 정보를 로드
    //=================================================================================================================

    InitResource(maxSessionCount);

    //=================================================================================================================
    // 4. 스레드 풀 생성
    //=================================================================================================================

    CreateThreadPool(workerThreadCount);

    //=================================================================================================================
    // 5. pendingAcceptCount 갯수만큼 소켓 생성 후 acceptex 호출
    //=================================================================================================================

    ReserveAcceptSocket(pendingAcceptCount);

    return true;  // 성공 시 true 반환
}

void CGameServer::Stop(void)
{
    // 모든 스레드 종료 및 리소스 정리

    // 이 함수를 호출한 스레드가 먼저 accept를 끊고, 기존의 세션들의 연결을 끊음.
    // 세션의 연결이 다 끊어졌음을 확인하고, PQCS를 워커 스레드 갯수만큼 호출, 이후 WFMOS를 호출하고 기다림

    // 모든 워커 스레드가 끊어졌다면 리소스를 정리하고 스레드 오류가 있는지 확인 후 없으면 종료.
    for (int i = 0; i < threadCnt; ++i)
        PostQueuedCompletionStatus(hIOCP, 0, 0, NULL);

    // ===========================================================================
    // 1. 모든 스레드가 종료되기를 대기
    // ===========================================================================
    DWORD retVal = WaitForMultipleObjects(threadCnt + 1, hThreads, TRUE, INFINITE);     // accept 스레드 갯수 1 추가해서 검사
    DWORD retError = GetLastError();

    // ===========================================================================
    // 2. 리소스 정리
    // ===========================================================================
    ReleaseResource();

    // ===========================================================================
    // 3. 스레드가 정상 종료되었는지 검사
    // ===========================================================================
    DWORD ExitCode;

    // 이 아래쪽 코드는 뜨면 안됨. 뜬다는 것은 main이 종료되었음에도 스레드가 살아있다는 의미.
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

void CGameServer::ReleaseResource(void)
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
        // 패킷 할당
        CPacket* pSendPacket = new CPacket;

        // 헤더 정보 삽입
        PACKET_HEADER header;
        header.bySize = pPacket->GetDataSize();
        pSendPacket->PutData((char*)&header, sizeof(PACKET_HEADER));

        // 인자로 받은 패킷의 데이터 삽입
        pSendPacket->PutData(pPacket->GetFrontBufferPtr(), pPacket->GetDataSize());

        // sendQ에 직렬화 버퍼 자체를 넣음
        pSession->sendQ.Enqueue(pSendPacket);
    }
    // 아니라면 재활용되면서 이상한 값이 나올 수 있다.
    else
    {
        DebugBreak();
    }

    SendPost(pSession);
}

// 세션이 만들어지기 전에 허락을 받는 함수. 서버 테스트나 점검 등이 필요할 때 사내 IP만 접속할 수 있도록 인원을 제한하는 방식.
bool CGameServer::OnConnectionRequest(const std::string& ip, int port)
{
    std::string str{ "1.0.8.0" };
    if (ip == str)
        return false;


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

void CGameServer::InitWinSock(void) noexcept
{
    // 윈도우 소켓 초기화
    WSADATA wsaData;

    int retVal = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (retVal != 0) {
        std::cout << "Error : WSAStartup failed " << retVal << ", " << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::CreateListenSocket(PROTOCOL_TYPE type)
{
    switch (type)
    {
    case PROTOCOL_TYPE::TCP_IP:
        // 이미 2인자에서 TCP냐 UDP냐 결정이 났기 때문에 3인자에 넣지 않아도 무방하다.
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET)
        {
            std::cerr << "Error : TCP CreateListenSocket()" << WSAGetLastError() << "\n";
            DebugBreak();
        }
        break;
    case PROTOCOL_TYPE::UDP:
        listenSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (listenSocket == INVALID_SOCKET)
        {
            std::cerr << "Error : UDP CreateListenSocket()" << WSAGetLastError() << "\n";
            DebugBreak();
        }
        break;
    default:
        std::cerr << "Error : default socket()" << WSAGetLastError() << "\n";
        DebugBreak();
        break;
    }
}

void CGameServer::CreateIOCP(int runningThreadCount)
{
    // CPU 개수 확인
    //SYSTEM_INFO si;
    //GetSystemInfo(&si);
    // int ProcessorNum = si.dwNumberOfProcessors;
    // 프로세서 갯수 미만으로 IOCP Running 스레드의 갯수를 제한하면서 테스트 해볼 것.

    // 입출력 완료 포트 생성
    hIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, runningThreadCount);
    if (hIOCP == NULL)
    {
        std::cerr << "INVALID_HANDLE : hIOCP, Error : " << WSAGetLastError() << "\n";
        DebugBreak();
    }

    Managers::GetInstance().Content()->InitWithIOCP(hIOCP);
}

void CGameServer::Bind(unsigned long ip, UINT16 port)
{
    // bind()
    SOCKADDR_IN serveraddr;
    ZeroMemory(&serveraddr, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = ip; //inet_addr(ip);   htonl(INADDR_ANY);
    serveraddr.sin_port = htons(port);
    int retval = bind(listenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));

    if (retval == SOCKET_ERROR)
    {
        std::cout << "Error : Bind(), " << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::Listen(void)
{
    // listen()
    int retval = listen(listenSocket, SOMAXCONN_HINT(65535));
    if (retval == SOCKET_ERROR)
    {
        std::cout << "Error : Listen(), " << WSAGetLastError() << "\n";
        DebugBreak();
    }

    // 리슨 소켓을 IOCP에 등록. AcceptEx를 사용하기 위해선 리슨 소켓을 IOCP에 등록해줘야함
    HANDLE hResult = CreateIoCompletionPort(
        (HANDLE)listenSocket,  // listen() 한 소켓 핸들
        hIOCP,                 // 기존 hIOCP 핸들에 연결
        (ULONG_PTR)0,          // CompletionKey (0 혹은 식별용 값)
        0                      // 스레드 풀 크기 지정 (0 으로 두면 기본값)
    );

    // 리턴값 검사
    if (hResult == NULL) {
        DWORD err = GetLastError();
        std::cout << "[에러] CreateIoCompletionPort 실패: " << err << "\n";
        // 필요 시 프로그램 종료 또는 복구 로직

        DebugBreak();
    }
}

void CGameServer::InitResource(int maxSessionCount)
{
    // 여기서 세션에 관련된 모든 정보들을 초기화. 만약 풀이 필요하다면 관련 코드 삽입
    maxConnections = maxSessionCount;
    sessions = new CSession[maxConnections];

    // stack 방식으로 세션 ID의 배열 정보를 저장( 0 ~ maxConnections 사이의 값을 인덱스로 사용 )
    for (int i = maxConnections - 1; i >= 0; --i)
    {
        stSessionIndex.push(i);
    }

    isServerMaintrenanceMode = true;

    InitializeCriticalSection(&cs_sessionID);
}

void CGameServer::CreateThreadPool(int workerThreadCount)
{
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
}

void CGameServer::ReserveAcceptSocket(int pendingAcceptCount)
{
    maxPendingSessionCnt = pendingAcceptCount;

    // 초기 동시 대기 세션 수만큼 PostAccept
    for (int i = 0; i < pendingAcceptCount; ++i)
    {
        CSession* pSession = FetchSession();
        InitSessionInfo(pSession);   // 필요한 초기화만

        // 중간에 AcceptEx 실패시
        if (!AcceptPost(pSession))
        {
            // 정리 후 다시 시도
            returnSession(pSession);
            --i;

            continue;
        }

        // AcceptEx 호출에 성공했으므로 IOCount를 1 증가. Accept 완료통지 처리 이후 차감될 예정
        //InterlockedIncrement(&pSession->IOCount);

        // 이 시점에서 curPendingSessionCnt는 워커스레드에서 재접이 일어날 수 있기에 interlocked를 사용
        // 해당 값은 data race 우려가 있으므로 ineterlock 사용
        InterlockedIncrement(&curPendingSessionCnt);
    }
}

void CGameServer::SetTCPSendBufSize(SOCKET socket, UINT32 size)
{
    int recvBufSize = size;
    if (setsockopt(listenSocket, SOL_SOCKET, SO_SNDBUF, (const char*)&recvBufSize, sizeof(recvBufSize)) == SOCKET_ERROR)
    {
        std::cerr << "setsockopt failed with error : " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        listenSocket = INVALID_SOCKET;
        WSACleanup();
        DebugBreak();
    }
}

void CGameServer::SetNonBlockingMode(SOCKET socket, bool bNonBlocking)
{
    u_long mode = bNonBlocking; // 1: Non-blocking, 0: Blocking
    if (ioctlsocket(listenSocket, FIONBIO, &mode) != 0) {
        std::cerr << "Failed to set non-blocking mode. Error : " << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::DisableNagleAlgorithm(SOCKET socket, bool bNagleOn)
{
    int flag = !bNagleOn; // 1: Nagle 비활성화, 0: 기본이 0으로 Nagle이 켜져 있는 상황
    // Nagle 옵션이 true라면 Nagle을 키는 상황이고, flag == 0, flag 값이 0이여야 킴
    // Nagle 옵션이 false라면 Nagle을 끄는 상황이다. flag == 1, flag 값이 1이면 끔
    if (setsockopt(listenSocket, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(flag)) != 0) {
        std::cerr << "Failed to disable Nagle algorithm. Error : " << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::EnableRSTOnClose(SOCKET socket)
{
    // 기본값이 l_onoff == 0 이라 설정 안해도 RST가 날아간다. 명시적으로 호출함 해보고 싶었음.
    struct linger lingerOpt = { 0, 0 }; // l_onoff = 1, l_linger = 0
    if (setsockopt(listenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(lingerOpt)) != 0) {
        std::cerr << "Failed to enable RST on close. Error :" << WSAGetLastError() << "\n";
        DebugBreak();
    }
}

void CGameServer::SetOptions(bool bNagleOn)
{
    // sendbuf 크기 설정 (송신 버퍼의 크기를 0으로 설정해 중간 단계 버퍼링 없이 메모리에 직접 direct I/O를 유도)
    SetTCPSendBufSize(listenSocket, 0);

    // blocking, non-blocking 소켓 모드 전환
    // accept 스레드는 일이 있을 때만 일어나는게 효율적이기에 listen 소켓을 blokcing 소켓으로 설정.
    SetNonBlockingMode(listenSocket, false);

    // RST 활성, 기본적으로 활성되어 있기에 그냥 호출해보는 것
    EnableRSTOnClose(listenSocket);

    // nagle 옵션 활성/비활성. bNagleOn이 true면 켜지는것. 기본이 켜져 있기에 사실 false만이 유의미한 변화가 있음.
    DisableNagleAlgorithm(listenSocket, bNagleOn);
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
    UINT16 sessionIndex;
    while (!stSessionIndex.pop(sessionIndex));

    LeaveCriticalSection(&cs_sessionID);

    // 세션접속 수 1 증가
    InterlockedIncrement(&curSessionCnt);

    sessionID |= sessionIndex;
    sessions[sessionIndex].id = sessionID;

    return &sessions[sessionIndex];
}

void CGameServer::returnSession(CSession* pSession)
{
    EnterCriticalSection(&cs_sessionID);

    // sendQ에 아직 처리되지 못한 패킷들 정리
    pSession->sendQ.ClearQueue();

    // 소켓 close
    closesocket(pSession->sock);
    pSession->sock = INVALID_SOCKET;

    // 인자로 받은 세션이 위치한 배열 인덱스를 반환
    stSessionIndex.push(static_cast<UINT16>(pSession->id));

    LeaveCriticalSection(&cs_sessionID);

    // 세션접속 수 1 감소
    InterlockedDecrement(&curSessionCnt);

    // 연결이 끊어진 수 1 증가
    InterlockedIncrement(&disconnectedSessionCnt);

    // 세션을 삭제했으니 이후에 콘텐츠에게 세션이 삭제되었음을 알리는 코드가 들어감
    OnRelease(pSession->id);

    // 세션이 삭제되었으므로 curPendingSessionCnt 1 감소
    InterlockedDecrement(&curPendingSessionCnt);
}

void CGameServer::InitSessionInfo(CSession* pSession)
{
    // send/recv용 오버랩 구조체 초기화
    ZeroMemory(&pSession->overlappedRecv, sizeof(pSession->overlappedRecv));
    ZeroMemory(&pSession->overlappedSend, sizeof(pSession->overlappedSend));
    ZeroMemory(&pSession->overlappedAccept, sizeof(pSession->overlappedAccept));

    ZeroMemory(&pSession->acceptBuffer, sizeof(pSession->acceptBuffer));

    // 삭제를 위한 IO Count 부여, 초기화 할 시기로 아직 등록도 전이니 0으로 설정
    UINT32 IOCount = InterlockedExchange(&pSession->IOCount, 0);
    Logging(pSession, (UINT32)ACTION::IOCOUNT_0 + IOCount);

    if (IOCount != 0)
        DebugBreak();

    // send를 1회 제한하기 위한 flag 변수
    pSession->sendFlag = 0;

    // 링버퍼 초기화
    pSession->sendQ.ClearQueue();
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
