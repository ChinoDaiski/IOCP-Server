
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

    int MoveRear(int iSize);
    int MoveFront(int iSize);

    char* GetFrontBufferPtr() { return &buffer[readPos]; }
    char* GetRearBufferPtr() { return &buffer[writePos]; }
    char* GetBufferPtr() { return &buffer[0]; }
    int GetBufferCapacity() { return capacity - 1; }

    // ��ȯ������ ������� WSABUF ������ ��ȯ.
    int makeWSASendBuf(LPWSABUF wsaBuf);
    int makeWSARecvBuf(LPWSABUF wsaBuf);

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

