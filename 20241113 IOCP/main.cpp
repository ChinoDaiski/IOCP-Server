
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

// key�� id�� �޾� �����Ǵ� ���� ��
std::unordered_map<UINT32, CSession*> g_clients;




void MoveCursorToTop() {

    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE); // �ܼ� �ڵ� ��������
    COORD coord = { 0, 0 }; // Ŀ���� �ܼ� �� ������ �̵�
    SetConsoleCursorPosition(hConsole, coord); // Ŀ�� ��ġ ����

    for (int i = 0; i < 30; ++i)
    {
        std::cout << "                                                     \n";
    }

    SetConsoleCursorPosition(hConsole, coord); // Ŀ�� ��ġ ����
}


unsigned int WINAPI MonitorThread(void* pArg);
unsigned int WINAPI TimerThread(void* pArg);

int main(void)
{
    //=================================================================================================================
    // ������ ���� - ������ ������ �Ŵ����� �������� �˾Ƽ� ��ϵ�
    //=================================================================================================================
    ChattingContent chatting((int)ContentType::Chatting, Managers::GetInstance().Content());



    //=================================================================================================================
    // ���� ����
    //=================================================================================================================
    CGameServer GameServer;

    // IP �Է½� inet_addr(ip); <- ���ϴ� IP�� ���ڿ��� �Ǵ� htonl(INADDR_ANY)�� ȣ���ؼ� ��� NIC���κ��� �����͸� ���� �� �� �ִ�.
    GameServer.Start(htonl(INADDR_ANY), SERVERPORT, 8, 4, true, 20000, 100);


    //=================================================================================================================
    // ������ ����
    //=================================================================================================================
    DWORD dwThreadID;

    // Ÿ�̸� ������ ����
    HANDLE hTimerThread;   // Ÿ�̸� ������ �ڵ鰪
    int initialServerFrame = 25;
    hTimerThread = (HANDLE)_beginthreadex(NULL, 0, TimerThread, &initialServerFrame, 0, (unsigned int*)&dwThreadID);

    // ����� ������ ����
    HANDLE hMonitorThread;   // ����� ������ �ڵ鰪
    hMonitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, &GameServer, 0, (unsigned int*)&dwThreadID);



    WCHAR ControlKey;

    //=================================================================================================================
    // ���� ��Ʈ��
    //=================================================================================================================
    while (1)
    {
        ControlKey = _getwch();
        if (ControlKey == L'q' || ControlKey == L'Q')
        {
            //=================================================================================================================
            // ����ó��
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

// ���ڷ� ���� ������ TPS �� ���� ���� ������ 1�ʸ��� �����ϴ� ������
unsigned int WINAPI MonitorThread(void* pArg)
{
    CLanServer* pThis = (CLanServer*)pArg;

    SystemMonitor monitor;

    while (true)
    {
        //MoveCursorToTop();

        // value �ʱ�ȭ
        pThis->ClearTPSValue();

        std::wcout << L"======================================================" << std::endl;

        monitor.PrintMonitoringData();
        std::cout << "\n";

        const FrameStats& status = pThis->GetStatusTPS();

        // TPS ���
        PrintTPSValue(status.accept, status.recvMessage, status.sendMessage);

        // �޸� Ǯ ���� ���
        PrintMemoryPoolValue(status.CurPoolCount, status.MaxPoolCount);

        std::cout << "\Pending SessionCount : " << pThis->GetPendingSessionCount() << "\n";
        std::cout << "Current SessionCount : " << pThis->GetConnectedSessionCount() << "\n";
        std::cout << "Disconnected SessionCount : " << pThis->GetDisconnectedSessionCnt() << "\n";
        std::cout << "Packet Use Count : " << CPacket::usePacketCnt << "\n\n";

        std::wcout << L"======================================================" << std::endl;

        // 1�ʰ� Sleep
        Sleep(1000);
    }

    return 0;
}

unsigned int WINAPI TimerThread(void* pArg)
{
    int targetFrame = *(int*)pArg;

    Managers::GetInstance().Timer()->InitTimer(targetFrame);

    // ���� �ֱ⸶�� �������� ������ PostQueuedCompletionStatus �� �־� ȣ��
    while (true)
    {
        int sleepDurationMs = Managers::GetInstance().Timer()->CheckFrame();

        if (sleepDurationMs != 0)
            Sleep(sleepDurationMs);

        Managers::GetInstance().Content()->Tick();
    }

    return 0;
}
