
#pragma once

// rear가 있는 곳을 비워야 한다. 그래야 멀티스레드에서 안정적인 코드를 만들 수 있다.

class CRingBuffer 
{
public:
    explicit CRingBuffer(UINT64 iBufferSize = 2000);
    ~CRingBuffer();

public:
    void Resize(UINT32 size);

    UINT32 GetBufferSize() const { return capacity; } // 실제 데이터 크기 반환

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

    // 반환값으로 만들어진 WSABUF 갯수를 반환.
    int makeWSASendBuf(LPWSABUF wsaBuf);
    int makeWSARecvBuf(LPWSABUF wsaBuf);

public:
    UINT32 mod(UINT32 val) const {
        return val % capacity;       // 링버퍼의 순환 인덱스 계산
    }

    UINT32 nextPos(UINT32 pos) const {
        return mod(pos + 1);         // 다음 위치 계산
    }

    bool isFull(UINT32 r, UINT32 w) const {
        return nextPos(w) == r;      // 가득 찬 상태 확인
    }

    bool isEmpty(UINT32 r, UINT32 w) const {
        return r == w;               // 비어 있는 상태 확인
    }

private:
    char* buffer{ nullptr };           // 버퍼 저장 공간
    UINT32 readPos{ 0 };                 // 읽기 위치
    UINT32 writePos{ 0 };                // 쓰기 위치
    UINT32 capacity{ 0 };                // 버퍼 크기
};

