
#include "pch.h"
#include "Session.h"

#include <process.h>
#include <unordered_map>

#include "ServerLib.h"
#include "Content.h"

#include "Protocol.h"

#include "SystemMonitor.h"
#include "Timer.h"

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

int main(void)
{
    CGameServer GameServer;

    // IP 입력시 inet_addr(ip); <- 원하는 IP를 문자열로 또는 htonl(INADDR_ANY)를 호출해서 모든 NIC으로부터 데이터를 받을 수 도 있다.
    GameServer.Start(htonl(INADDR_ANY), SERVERPORT, 8, 4, true, 20000);

    // 모니터 스레드 생성
    HANDLE hMonitorThread;   // 모니터 스레드 핸들값
    DWORD dwThreadID;
    hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, &GameServer, 0, (unsigned int*)&dwThreadID);



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
    Timer timer;

    while (true)
    {
        MoveCursorToTop();

        // value 초기화
        pThis->ClearTPSValue();
        const FrameStats& status = pThis->GetStatusTPS();

        std::wcout << L"======================================================" << std::endl;

        // 서버 시간 출력
        timer.PrintElapsedTime();

        std::cout << "\n\n";

        // TPS 출력
        PrintTPSValue(status.accept, status.recvMessage, status.sendMessage);

        // 메모리 풀 관련 출력
        PrintMemoryPoolValue(status.CurPoolCount, status.MaxPoolCount);

        std::cout << "\nCurrent SessionCount : " << pThis->GetSessionCount() << "\n";
        std::cout << "Disconnected SessionCount : " << pThis->GetDisconnectedSessionCnt() << "\n";
        std::cout << "Packet Use Count : " << CPacket::usePacketCnt << "\n\n";

        monitor.PrintMonitoringData();

        std::wcout << L"======================================================" << std::endl;

        // 1초간 Sleep
        Sleep(1000);
    }

    return 0;
}



