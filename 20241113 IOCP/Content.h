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

#define NO_TRANSFER_SESSION -1  // �ٸ� �������� �̵����� �ʴ� ������ ��ȣ

class Content abstract {
public:
    Content(int id, ContentManager* contentMgr);

public:
    virtual ~Content() = default;
    virtual void OnSessionEnter(CSession* s) = 0;   // ������ �������� ���� �� ȣ��Ǵ� �Լ�
    virtual void OnSessionLeave(CSession* s) = 0;   // ������ ���������� ���� �� ȣ��Ǵ� �Լ�
    virtual void Update() = 0;  // �� Tick ���� ȣ��Ǵ� �Լ�. �������� �������� ó���ϴ� �κ�
    virtual void ProcessPacket(CPacket* p) = 0;  // �� Tick ���� ȣ��Ǵ� �Լ�. ���� ��Ŷ�� ó���ϴ� �Լ�

    // ���� ����
    void AddSession(CSession* s);
    void RequestSessionDelete(CSession* s);    // ������ isAlive�� false �� ���� IOCount�� 0�� �Ǿ� ��Ʈ��ũ �ʿ��� ������ ��û�� �� ���
    void ExitSession(CSession* s, int id = NO_TRANSFER_SESSION);    // id ���� �ָ� �ٸ� �������� ������ �߰�, ���� �ٸ� �������� �̵����� �ʰ� ������ �����ϰ� �ʹٸ� id�� ���� ���� ������ ��

    // ������ �̺�Ʈ ó��. ���ο��� OnSessionEnter, OnSessionLeave, Update ȣ���. ��Ŀ������� �� Tick �Լ��� ó����.
    void Tick();

    // ��Ŷ�� packetQueue�� �߰��ϴ� �Լ�
    void EnqeuePacket(CPacket* pPacket);

protected:
    // �ڽ��� ���� ���
    std::vector<CSession*> managedSessions;

private:
    // �ӽ� ����: ���� ���� ����, ���� ����
    std::vector<CSession*> incomingSessions;
    std::vector<std::pair<int, CSession*>> outgoingSessions;

    // ���� ��û�� ���� ����
    std::vector<CSession*> requestDeleteSessions;

    // ��Ŷ ť
    tbb::concurrent_queue<CPacket*> packetQueue;
    UINT32 packetSize = 0;

    // ������Ʈ ���� ����
    UINT32 isUpdating = 0;
    UINT32 pendingLoops = 0;

    int contentID; 
    
private:
    ContentManager* pContentManager;
};
