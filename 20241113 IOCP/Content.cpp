
#include "pch.h"

#include "Content.h"
#include "Session.h"
#include "ContentManager.h"

//UINT32 CPlayer::g_ID = 0;
//
//CPlayer::CPlayer()
//{
//	playerID = g_ID;
//	g_ID++;
//}
//
//CPlayer::~CPlayer()
//{
//}

Content::Content(int id, ContentManager* contentMgr)
{
    contentID = id;
    pContentManager = contentMgr;

    contentMgr->registerContent(contentID, this);
}

void Content::AddSession(CSession* s)
{
    incomingSessions.push_back(s);
}

void Content::RequestSessionDelete(CSession* s)
{
    requestDeleteSessions.push_back(s);
}

void Content::ExitSession(CSession* s, int id)
{
    auto it = std::find(managedSessions.begin(), managedSessions.end(), s);

    if (it != managedSessions.end()) {
        // 1) ���� ���� ������ ���
        outgoingSessions.push_back(std::make_pair(id, s));

        // 2) swap-and-pop���� managedSessions���� ����
        *it = managedSessions.back();
        managedSessions.pop_back();
    }
}

void Content::Tick()
{
    // loop ��û�� �ϳ� �� ���Դٰ� �˸�
    InterlockedIncrement(&pendingLoops);

    // ���� ���� 0(�۾� ���� �ƴ�)�̸� 1�� �ٲٰ� ����,
    // ���� ���� 1(�̹� �۾� ��)�̸� �״�� 1�̰�, �������� ����
    if (InterlockedCompareExchange(&isUpdating, 1, 0) != 0) {
        // �̹� �ٸ� �����尡 ������Ʈ ��. ��ȯ 
        return;
    }

    while (true)
    {
        // 1. ���� ���� ó�� - �̹� Update�� ������ ContentManager�� transferSession�� ȣ���ϸ� ���������� AddSession�� ȣ��Ǿ� incomingSessions�� ä���� ����.
        for (auto s : incomingSessions) {
            managedSessions.push_back(s);
            OnSessionEnter(s);
        }
        incomingSessions.clear();

        // 2. ���� ��Ŷ ó��
        UINT32 qSize = InterlockedCompareExchange(&packetSize, 0, 0);  // ���� ��Ŷ�� ������ ���������� ������

        CPacket* pPacket;
        for (UINT32 i = 0; i < qSize; ++i)
        {
            // ��Ŷ�� 1�� ���� �� ���� �ݺ�
            while (!packetQueue.try_pop(pPacket));

            // ��Ŷ�� �������� ��Ŷ ������ -1
            InterlockedDecrement(&packetSize);

            // ProcessPacket �Լ� ȣ��
            ProcessPacket(pPacket);

            // ��Ŷ ó���� �������� refCnt 1 ����
            pPacket->ReleaseRef();
        }

        // 3. ������ ���� ���� => �� �ܰ迡�� outgoingSessions�� ä��
        Update();

        // 4. ���� ���� ó��
        for (auto& session : outgoingSessions) {
            OnSessionLeave(session.second);

            // ������ �ٸ� �������� ���� �ʴ´ٸ�, ������ ����
            if (session.first == NO_TRANSFER_SESSION)
                session.second->networkAlive = 0;
            // ���� �ٸ� �������� ���ٸ�, �ش� �������� ������ �̵�
            else
                pContentManager->transferSession(session.first, session.second);

        }
        outgoingSessions.clear();

        // 5. ��ģ ���� ���� �˻� �� ���ٸ� ������
        if (InterlockedDecrement(&pendingLoops) == 0)
            break;
    }

    InterlockedExchange(&isUpdating, 0);
}

void Content::EnqeuePacket(CPacket* pPacket)
{
    packetQueue.push(pPacket);
    InterlockedIncrement(&packetSize);
}
