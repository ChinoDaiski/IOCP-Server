
#include "pch.h"
#include "Session.h"

#include <process.h>
#include <unordered_map>

#include "ServerLib.h"
#include "Content.h"

#include "Protocol.h"

#include "Packet.h"

#pragma comment(lib, "PDH_DLL.lib")
#include "SystemMonitor.h"


// Content
#include "ContentType.h"
#include "ContentManager.h"
#include "ChattingContent.h"

// Timer
#include "TimerManager.h"

#include "Managers.h"







#define SERVERPORT 6000

SOCKET g_listenSocket; 
UINT32 g_id = 0;

// key로 id를 받아 관리되는 세션 맵
std::unordered_map<UINT32, CSession*> g_clients;




void MoveCursorToTop() {

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // 콘솔 핸들 가져오기
    COORD coord = { 0, 0 }; // 커서를 콘솔 맨 앞으로 이동
    SetConsoleCursorPosition(hConsole, coord); // 커서 위치 설정

    for (int i = 0; i < 30; ++i)
    {
        std::cout << "                                                     \n";
    }

    SetConsoleCursorPosition(hConsole, coord); // 커서 위치 설정
}


unsigned int WINAPI MonitorThread(void* pArg);
unsigned int WINAPI TimerThread(void* pArg);

int main(void)
{
    //=================================================================================================================
    // 컨텐츠 생성 - 생성시 컨텐츠 매니저에 컨텐츠가 알아서 등록됨
    //=================================================================================================================
    ChattingContent chatting((int)ContentType::Chatting, Managers::GetInstance().Content());



    //=================================================================================================================
    // 서버 시작
    //=================================================================================================================
    CGameServer GameServer;

    // IP 입력시 inet_addr(ip); <- 원하는 IP를 문자열로 또는 htonl(INADDR_ANY)를 호출해서 모든 NIC으로부터 데이터를 받을 수 도 있다.
    GameServer.Start(htonl(INADDR_ANY), SERVERPORT, 8, 4, true, 20000, 100);


    //=================================================================================================================
    // 스레드 생성
    //=================================================================================================================
    DWORD dwThreadID;

    // 타이머 스레드 생성
    HANDLE hTimerThread;   // 타이머 스레드 핸들값
    int initialServerFrame = 25;
    hTimerThread = (HANDLE)_beginthreadex(NULL, 0, TimerThread, &initialServerFrame, 0, (unsigned int*)&dwThreadID);

    // 모니터 스레드 생성
    HANDLE hMonitorThread;   // 모니터 스레드 핸들값
    hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, &GameServer, 0, (unsigned int*)&dwThreadID);



    WCHAR ControlKey;

    //=================================================================================================================
    // 종료 컨트롤
    //=================================================================================================================
    while (1)
    {
        ControlKey = _getwch();
        if (ControlKey == L'q' || ControlKey == L'Q')
        {
            //=================================================================================================================
            // 종료처리
            //=================================================================================================================
            GameServer.Stop();
            break;
        }
    }

    return 0;
}



void PrintTPSValue(UINT32 acceptTPS, UINT32 recvTPS, UINT32 sendTPS)
{
    std::cout << "acceptTPS : " << acceptTPS << "\n";
    std::cout << "recvMessageTPS : " << recvTPS << "\n";
    std::cout << "sendMessageTPS : " << sendTPS << "\n";
}

void PrintMemoryPoolValue(UINT32 curPoolCount, UINT32 maxPoolCount)
{
    std::cout << "curPoolCount : " << curPoolCount << "\n";
    std::cout << "maxPoolCount : " << maxPoolCount << "\n";
}

// 인자로 받은 서버의 TPS 및 보고 싶은 정보를 1초마다 감시하는 스레드
unsigned int WINAPI MonitorThread(void* pArg)
{
    CLanServer* pThis = (CLanServer*)pArg;

    SystemMonitor monitor;

    while (true)
    {
        //MoveCursorToTop();

        // value 초기화
        pThis->ClearTPSValue();

        std::wcout << L"======================================================" << std::endl;

        monitor.PrintMonitoringData();
        std::cout << "\n";

        const FrameStats& status = pThis->GetStatusTPS();

        // TPS 출력
        PrintTPSValue(status.accept, status.recvMessage, status.sendMessage);

        // 메모리 풀 관련 출력
        PrintMemoryPoolValue(status.CurPoolCount, status.MaxPoolCount);

        std::cout << "\Pending SessionCount : " << pThis->GetPendingSessionCount() << "\n";
        std::cout << "Current SessionCount : " << pThis->GetConnectedSessionCount() << "\n";
        std::cout << "Disconnected SessionCount : " << pThis->GetDisconnectedSessionCnt() << "\n";
        std::cout << "Packet Use Count : " << CPacket::usePacketCnt << "\n\n";

        std::wcout << L"======================================================" << std::endl;

        // 1초간 Sleep
        Sleep(1000);
    }

    return 0;
}

unsigned int WINAPI TimerThread(void* pArg)
{
    int targetFrame = *(int*)pArg;

    Managers::GetInstance().Timer()->InitTimer(targetFrame);

    // 일정 주기마다 컨텐츠의 정보를 PostQueuedCompletionStatus 에 넣어 호출
    while (true)
    {
        int sleepDurationMs = Managers::GetInstance().Timer()->CheckFrame();

        if (sleepDurationMs != 0)
            Sleep(sleepDurationMs);

        Managers::GetInstance().Content()->Tick();
    }

    return 0;
}
