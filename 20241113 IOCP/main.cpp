
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

// key�� id�� �޾� �����Ǵ� ���� ��
std::unordered_map<UINT32, CSession*> g_clients;


unsigned int WINAPI MonitorThread(void* pArg);

int main(void)
{
    timeBeginPeriod(1);

    CGameServer GameServer;
    GameServer.Start(htonl(INADDR_ANY), SERVERPORT, 8, 4, true, 20000);

    // IP �Է½� inet_addr(ip); <- ���ϴ� IP�� ���ڿ��� �Ǵ� htonl(INADDR_ANY)�� ȣ���ؼ� ��� NIC���κ��� �����͸� ���� �� �� �ִ�.



    // ����� ������ ����
    HANDLE hMonitorThread;   // ����� ������ �ڵ鰪
    DWORD dwThreadID;
    hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, &GameServer, 0, (unsigned int*)&dwThreadID);



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

// ���ڷ� ���� ������ TPS �� ���� ���� ������ 1�ʸ��� �����ϴ� ������
unsigned int WINAPI MonitorThread(void* pArg)
{
    CLanServer* pThis = (CLanServer*)pArg;

    while (true)
    {
        std::cout << "===================================\n";

        // TPS ���
        PrintTPSValue(pThis->GetAcceptTPS(), pThis->GetRecvMessageTPS(), pThis->GetSendMessageTPS());

        std::cout << "Current SessionCount : " << pThis->GetSessionCount() << "\n";
        std::cout << "Disconnected SessionCount : " << pThis->GetDisconnectedSessionCnt() << "\n";
        std::cout << "Packet Use Count : " << CPacket::usePacketCnt << "\n";

        std::cout << "===================================\n\n";
        // value �ʱ�ȭ
        pThis->ClearTPSValue();

        // 1�ʰ� Sleep
        Sleep(1000);
    }

    return 0;
}



