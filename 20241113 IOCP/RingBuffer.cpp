
#include "pch.h"
#include "RingBuffer.h"

CRingBuffer::CRingBuffer(UINT64 iBufferSize)
{
    Resize(iBufferSize);
}

CRingBuffer::~CRingBuffer()
{
    delete[] buffer;             // 동적 할당 해제
}

void CRingBuffer::Resize(UINT32 size)
{
    delete[] buffer;             // 기존 버퍼 삭제
    buffer = new char[size + 1]; // 새 버퍼 생성 (+1은 여유 공간)
    capacity = size + 1;
    ClearBuffer();
}

UINT32 CRingBuffer::GetUseSize() const
{
    UINT32 r = readPos;
    UINT32 w = writePos;
    return (w >= r) ? (w - r) : (capacity - r + w);
}

int CRingBuffer::Enqueue(const char* chpData, UINT32 iSize)
{
    UINT32 r = readPos;
    UINT32 w = writePos;
    UINT32 useSize = (w >= r) ? (w - r) : (capacity - r + w);

    if (w == 0)
    {
        useSize -= 1;
    }

    UINT32 freeSize = capacity - useSize - 1;

    if (iSize > freeSize)
    {
        DebugBreak();
    }

    // 첫 번째 연속 구간에 쓰기
    UINT32 firstWriteSize = std::min(iSize, capacity - w);
    std::memcpy(&buffer[w], chpData, firstWriteSize);

    // 두 번째 연속 구간에 쓰기
    UINT32 secondWriteSize = iSize - firstWriteSize;
    std::memcpy(&buffer[0], chpData + firstWriteSize, secondWriteSize);

    // 쓰기 위치 업데이트 
    MoveRear(iSize);
    return iSize;
}

int CRingBuffer::Dequeue(char* chpDest, UINT32 iSize)
{
    UINT32 r = readPos;
    UINT32 w = writePos;

    UINT32 usedSize = (w >= r) ? (w - r) : (capacity - r + w);

    if (iSize > usedSize)
    {
        DebugBreak();
    }

    // 첫 번째 연속 구간에서 읽기
    UINT32 firstReadSize = std::min(iSize, capacity - r);
    std::memcpy(chpDest, &buffer[r], firstReadSize);

    // 두 번째 연속 구간에서 읽기
    UINT32 secondReadSize = iSize - firstReadSize;
    std::memcpy(chpDest + firstReadSize, &buffer[0], secondReadSize);

    // 읽기 위치 업데이트
    MoveFront(iSize);
    return iSize;
}

int CRingBuffer::Peek(char* chpDest, UINT32 iSize) const
{
    UINT32 r = readPos;
    UINT32 w = writePos;

    UINT32 usedSize = (w >= r) ? (w - r) : (capacity - r + w);
    UINT32 toRead = std::min(iSize, usedSize);

    // 첫 번째 연속 구간에서 읽기
    UINT32 firstReadSize = std::min(toRead, capacity - r);
    UINT32 secondReadSize = toRead - firstReadSize;

    ZeroMemory(chpDest, firstReadSize + secondReadSize);

    std::memcpy(chpDest, &buffer[r], firstReadSize);

    // 두 번째 연속 구간에서 읽기
    if (secondReadSize != 0)
        std::memcpy(chpDest + firstReadSize, &buffer[0], secondReadSize);

    return toRead;
}

void CRingBuffer::ClearBuffer(void)
{
    readPos = 0;
    writePos = 0;
}

int CRingBuffer::DirectEnqueueSize() const
{
    UINT32 w = writePos;
    UINT32 r = readPos;
    return (r > w) ? (r - w - 1) : (capacity - w - (r == 0 ? 1 : 0));
}

int CRingBuffer::DirectDequeueSize() const
{
    UINT32 w = writePos;
    UINT32 r = readPos;
    return (w >= r) ? (w - r) : (capacity - r);
}

UINT32 CRingBuffer::DirectEnqueueSize(UINT32 readPos, UINT32 writePos) const
{
    return (readPos > writePos) ? (readPos - writePos - 1) : (capacity - writePos - (readPos == 0 ? 1 : 0));
}

UINT32 CRingBuffer::DirectDequeueSize(UINT32 readPos, UINT32 writePos) const
{
    return (writePos >= readPos) ? (writePos - readPos) : (capacity - readPos);
}

int CRingBuffer::MoveRear(int iSize)
{
    writePos = mod(writePos + iSize);
    return iSize;
}

int CRingBuffer::MoveFront(int iSize)
{
    readPos = mod(readPos + iSize);
    return iSize;
}

int CRingBuffer::makeWSASendBuf(LPWSABUF wsaBuf)
{
    UINT32 w = writePos;
    UINT32 r = readPos;

    // r ... w 이렇게 있을 때
    if (w >= r)
    {
        wsaBuf[0].buf = buffer + r;
        wsaBuf[0].len = w - r;

        return 1;
    }
    // w ... r 이렇게 있을 때
    else
    {
        wsaBuf[0].buf = buffer + r;
        wsaBuf[0].len = capacity - r;

        if (w == 0)
            return 1;

        wsaBuf[1].buf = buffer;
        wsaBuf[1].len = w;

        return 2;
    }
}

// enq 할때 사용
int CRingBuffer::makeWSARecvBuf(LPWSABUF wsaBuf)
{
    UINT32 w = writePos;
    UINT32 r = readPos;

    // w ... r 이렇게 있을 때
    if (r > w)
    {
        wsaBuf[0].buf = buffer + w;
        wsaBuf[0].len = r - w - 1;  // 한칸은 비워져 있어야하기 때문에 1을 뺀다. 

        return 1;
    }
    // r ... w 이렇게 있을 때
    else
    {
        UINT32 dirEnqSize = DirectEnqueueSize(r, w);

        wsaBuf[0].buf = buffer + w;
        wsaBuf[0].len = dirEnqSize;

        if (r == 0)
            return 1;

        wsaBuf[1].buf = buffer;
        wsaBuf[1].len = r - 1;

        return 2;
    }
}
