
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
        // 1) 먼저 나갈 세션을 기록
        outgoingSessions.push_back(std::make_pair(id, s));

        // 2) swap-and-pop으로 managedSessions에서 제거
        *it = managedSessions.back();
        managedSessions.pop_back();
    }
}

void Content::Tick()
{
    // loop 요청이 하나 더 들어왔다고만 알림
    InterlockedIncrement(&pendingLoops);

    // 이전 값이 0(작업 중이 아님)이면 1로 바꾸고 진입,
    // 이전 값이 1(이미 작업 중)이면 그대로 1이고, 진입하지 않음
    if (InterlockedCompareExchange(&isUpdating, 1, 0) != 0) {
        // 이미 다른 스레드가 업데이트 중. 반환 
        return;
    }

    while (true)
    {
        // 1. 들어온 세션 처리 - 이미 Update를 돌리며 ContentManager의 transferSession을 호출하며 내부적으로 AddSession이 호출되어 incomingSessions가 채워져 있음.
        for (auto s : incomingSessions) {
            managedSessions.push_back(s);
            OnSessionEnter(s);
        }
        incomingSessions.clear();

        // 2. 쌓인 패킷 처리
        UINT32 qSize = InterlockedCompareExchange(&packetSize, 0, 0);  // 쌓인 패킷의 갯수를 원자적으로 가져옴

        CPacket* pPacket;
        for (UINT32 i = 0; i < qSize; ++i)
        {
            // 패킷을 1개 꺼낼 때 까지 반복
            while (!packetQueue.try_pop(pPacket));

            // 패킷을 꺼냈으니 패킷 사이즈 -1
            InterlockedDecrement(&packetSize);

            // ProcessPacket 함수 호출
            ProcessPacket(pPacket);

            // 패킷 처리가 끝났으니 refCnt 1 감소
            pPacket->ReleaseRef();
        }

        // 3. 컨텐츠 고유 로직 => 이 단계에서 outgoingSessions를 채움
        Update();

        // 4. 나갈 세션 처리
        for (auto& session : outgoingSessions) {
            OnSessionLeave(session.second);

            // 세션이 다른 컨텐츠로 가지 않는다면, 세션을 종료
            if (session.first == NO_TRANSFER_SESSION)
                session.second->networkAlive = 0;
            // 만약 다른 컨텐츠로 간다면, 해당 컨텐츠로 세션을 이동
            else
                pContentManager->transferSession(session.first, session.second);

        }
        outgoingSessions.clear();

        // 5. 놓친 루프 여부 검사 후 없다면 마무리
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
