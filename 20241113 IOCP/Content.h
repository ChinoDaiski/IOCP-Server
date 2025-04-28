#pragma once

#include "tbb/concurrent_queue.h"
#include "Packet.h"

//class CPlayer
//{
//public:
//    CPlayer();
//    ~CPlayer();
//
//public:
//    UINT32 sessionID;
//    UINT32 playerID;
//
//private:
//    static UINT32 g_ID;
//};

class CSession;
class ContentManager;

#define NO_TRANSFER_SESSION -1  // 다른 컨텐츠로 이동하지 않는 컨텐츠 번호

class Content abstract {
public:
    Content(int id, ContentManager* contentMgr);

public:
    virtual ~Content() = default;
    virtual void OnSessionEnter(CSession* s) = 0;   // 세션이 컨텐츠에 들어올 때 호출되는 함수
    virtual void OnSessionLeave(CSession* s) = 0;   // 세션이 컨텐츠에서 나갈 때 호출되는 함수
    virtual void Update() = 0;  // 매 Tick 에서 호출되는 함수. 실질적인 컨텐츠를 처리하는 부분
    virtual void ProcessPacket(CPacket* p) = 0;  // 매 Tick 에서 호출되는 함수. 쌓인 패킷을 처리하는 함수

    // 세션 관리
    void AddSession(CSession* s);
    void RequestSessionDelete(CSession* s);    // 세션의 isAlive가 false 된 이후 IOCount가 0이 되어 네트워크 쪽에서 삭제를 요청할 때 사용
    void ExitSession(CSession* s, int id = NO_TRANSFER_SESSION);    // id 값을 주면 다른 컨텐츠에 세션을 추가, 만약 다른 컨텐츠로 이동하지 않고 세션을 종료하고 싶다면 id에 값을 주지 않으면 됨

    // 프레임 이벤트 처리. 내부에서 OnSessionEnter, OnSessionLeave, Update 호출됨. 워커스레드는 이 Tick 함수를 처리함.
    void Tick();

    // 패킷을 packetQueue에 추가하는 함수
    void EnqeuePacket(CPacket* pPacket);

protected:
    // 자신의 세션 목록
    std::vector<CSession*> managedSessions;

private:
    // 임시 저장: 새로 들어온 세션, 나갈 세션
    std::vector<CSession*> incomingSessions;
    std::vector<std::pair<int, CSession*>> outgoingSessions;

    // 삭제 요청이 들어온 세션
    std::vector<CSession*> requestDeleteSessions;

    // 패킷 큐
    tbb::concurrent_queue<CPacket*> packetQueue;
    UINT32 packetSize = 0;

    // 업데이트 제어 변수
    UINT32 isUpdating = 0;
    UINT32 pendingLoops = 0;

    int contentID; 
    
private:
    ContentManager* pContentManager;
};
