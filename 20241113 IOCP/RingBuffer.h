
#pragma once

// rear�� �ִ� ���� ����� �Ѵ�. �׷��� ��Ƽ�����忡�� �������� �ڵ带 ���� �� �ִ�.

class CRingBuffer 
{
public:
    explicit CRingBuffer(UINT64 iBufferSize = 2000);
    ~CRingBuffer();

public:
    void Resize(UINT32 size);

    UINT32 GetBufferSize() const { return capacity; } // ���� ������ ũ�� ��ȯ

    UINT32 GetUseSize() const;

    UINT32 GetFreeSize() const {
        return GetBufferSize() - GetUseSize();
    }

    int Enqueue(const char* chpData, UINT32 iSize);
    int Dequeue(char* chpDest, UINT32 iSize);

    int Peek(char* chpDest, UINT32 iSize) const;
    void ClearBuffer(void);

    int DirectEnqueueSize() const;
    int DirectDequeueSize() const;
    UINT32 DirectEnqueueSize(UINT32 readPos, UINT32 writePos) const;
    UINT32 DirectDequeueSize(UINT32 readPos, UINT32 writePos) const;

    int MoveRear(int iSize);
    int MoveFront(int iSize);

    char* GetFrontBufferPtr() { return &buffer[readPos]; }
    char* GetRearBufferPtr() { return &buffer[writePos]; }
    char* GetBufferPtr() { return &buffer[0]; }
    int GetBufferCapacity() { return capacity - 1; }

    // ��ȯ������ ������� WSABUF ������ ��ȯ.
    int makeWSASendBuf(LPWSABUF wsaBuf);
    int makeWSARecvBuf(LPWSABUF wsaBuf);

    UINT32 GetReadPos(void) { return readPos; }
    UINT32 GetWritePos(void) { return writePos; }

public:
    UINT32 mod(UINT32 val) const {
        return val % capacity;       // �������� ��ȯ �ε��� ���
    }

    UINT32 nextPos(UINT32 pos) const {
        return mod(pos + 1);         // ���� ��ġ ���
    }

    bool isFull(UINT32 r, UINT32 w) const {
        return nextPos(w) == r;      // ���� �� ���� Ȯ��
    }

    bool isEmpty(UINT32 r, UINT32 w) const {
        return r == w;               // ��� �ִ� ���� Ȯ��
    }

private:
    char* buffer{ nullptr };           // ���� ���� ����
    UINT32 readPos{ 0 };                 // �б� ��ġ
    UINT32 writePos{ 0 };                // ���� ��ġ
    UINT32 capacity{ 0 };                // ���� ũ��
};


class CPacket;

#define MAX_PACKET_QUEUE_SIZE 10000

class CPacketQueue
{
public:
    explicit CPacketQueue(void);
    ~CPacketQueue();

public:
    void Enqueue(CPacket* pPacket);
    void Dequeue(void);

public:
    CPacket* GetFront(void);
    void ClearQueue(void);

public:
    bool empty(void);

    // ��ȯ������ ������� WSABUF ������ ��ȯ.
    int makeWSASendBuf(LPWSABUF wsaBuf);

    // ���ڷ� send �Ϸ��������� ���� ���� Ȯ���� ����� �޾� pPackets���� �����͸� ���� packet�� �����ϴ� �Լ�
    // ��ȯ������ empty ���θ� ��ȯ�Ѵ�.
    void RemoveSendCompletePacket(UINT32 completeSendSize);

    UINT32 GetWritePos(void) { return writePos; }
    UINT32 GetReadPos(void) { return readPos; }


public:
    UINT32 mod(UINT32 val) const {
        return val % capacity;       // �������� ��ȯ �ε��� ���
    }

    UINT32 nextPos(UINT32 pos) const {
        return mod(pos + 1);         // ���� ��ġ ���
    }

    bool isFull(UINT32 r, UINT32 w) const {
        return nextPos(w) == r;      // ���� �� ���� Ȯ��
    }

    bool isEmpty(UINT32 r, UINT32 w) const {
        return r == w;               // ��� �ִ� ���� Ȯ��
    }

private:
    CPacket* pPackets[MAX_PACKET_QUEUE_SIZE + 1];   // ť ��ü. �ϴ� 100���� ���. �Ƹ� 100���� �ѹ��� �Ѱ� �����ϴ� ��찡 ����������, �̰� ���� �����غ��鼭 �ִ밪�� �����Ͽ� ���
    UINT32 readPos{ 0 };                    // �б� ��ġ
    UINT32 writePos{ 0 };                   // ���� ��ġ
    UINT32 capacity{ MAX_PACKET_QUEUE_SIZE + 1 };   // ���� ũ��
    UINT32 sendCompleteDataSize{ 0 };   // send ���ۿ��� ������ �Ϸ��� ũ��
};
