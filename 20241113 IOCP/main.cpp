
#include "pch.h"
#include "Session.h"

#include <process.h>
#include <unordered_map>

#include "ServerLib.h"
#include "Content.h"

#include "Protocol.h"

#define SERVERPORT 6000

SOCKET g_listenSocket; 
HANDLE g_hIOCP; 
UINT32 g_id = 0;

// key로 id를 받아 관리되는 세션 맵
std::unordered_map<UINT32, CSession*> g_clients;


unsigned int WINAPI MonitorThread(void* pArg);

int main(void)
{
    timeBeginPeriod(1);

    CGameServer GameServer;
    GameServer.Start(htonl(INADDR_ANY), SERVERPORT, 8, 4, true, 20000);

    // IP 입력시 inet_addr(ip); <- 원하는 IP를 문자열로 또는 htonl(INADDR_ANY)를 호출해서 모든 NIC으로부터 데이터를 받을 수 도 있다.



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
            PostQueuedCompletionStatus(g_hIOCP, 0, 0, NULL);
            break;
        }
    }

    GameServer.Stop();

    return 0;
}



void PrintTPSValue(UINT32 acceptTPS, UINT32 recvTPS, UINT32 sendTPS)
{
    std::cout << "acceptTPS : " << acceptTPS << "\n";
    std::cout << "recvMessageTPS : " << recvTPS << "\n";
    std::cout << "sendMessageTPS : " << sendTPS << "\n";
}

// 인자로 받은 서버의 TPS 및 보고 싶은 정보를 1초마다 감시하는 스레드
unsigned int WINAPI MonitorThread(void* pArg)
{
    CLanServer* pThis = (CLanServer*)pArg;

    while (true)
    {
        std::cout << "===================================\n";

        // TPS 출력
        PrintTPSValue(pThis->GetAcceptTPS(), pThis->GetRecvMessageTPS(), pThis->GetSendMessageTPS());

        std::cout << "Current SessionCount : " << pThis->GetSessionCount() << "\n";
        std::cout << "Disconnected SessionCount : " << pThis->GetDisconnectedSessionCnt() << "\n";
        std::cout << "Packet Use Count : " << CPacket::usePacketCnt << "\n";

        std::cout << "===================================\n\n";
        // value 초기화
        pThis->ClearTPSValue();

        // 1초간 Sleep
        Sleep(1000);
    }

    return 0;
}



