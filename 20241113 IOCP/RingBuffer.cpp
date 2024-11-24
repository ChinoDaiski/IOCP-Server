
#include "pch.h"
#include "RingBuffer.h"

CRingBuffer::CRingBuffer(UINT64 iBufferSize)
{
    Resize(iBufferSize);
}

CRingBuffer::~CRingBuffer()
{
    delete[] buffer;             // ���� �Ҵ� ����
}

void CRingBuffer::Resize(UINT32 size)
{
    delete[] buffer;             // ���� ���� ����
    buffer = new char[size + 1]; // �� ���� ���� (+1�� ���� ����)
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

    // ù ��° ���� ������ ����
    UINT32 firstWriteSize = std::min(iSize, capacity - w);
    std::memcpy(&buffer[w], chpData, firstWriteSize);

    // �� ��° ���� ������ ����
    UINT32 secondWriteSize = iSize - firstWriteSize;
    std::memcpy(&buffer[0], chpData + firstWriteSize, secondWriteSize);

    // ���� ��ġ ������Ʈ 
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

    // ù ��° ���� �������� �б�
    UINT32 firstReadSize = std::min(iSize, capacity - r);
    std::memcpy(chpDest, &buffer[r], firstReadSize);

    // �� ��° ���� �������� �б�
    UINT32 secondReadSize = iSize - firstReadSize;
    std::memcpy(chpDest + firstReadSize, &buffer[0], secondReadSize);

    // �б� ��ġ ������Ʈ
    MoveFront(iSize);
    return iSize;
}

int CRingBuffer::Peek(char* chpDest, UINT32 iSize) const
{
    UINT32 r = readPos;
    UINT32 w = writePos;

    UINT32 usedSize = (w >= r) ? (w - r) : (capacity - r + w);
    UINT32 toRead = std::min(iSize, usedSize);

    // ù ��° ���� �������� �б�
    UINT32 firstReadSize = std::min(toRead, capacity - r);
    UINT32 secondReadSize = toRead - firstReadSize;

    ZeroMemory(chpDest, firstReadSize + secondReadSize);

    std::memcpy(chpDest, &buffer[r], firstReadSize);

    // �� ��° ���� �������� �б�
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

    // r ... w �̷��� ���� ��
    if (w >= r)
    {
        wsaBuf[0].buf = buffer + r;
        wsaBuf[0].len = w - r;

        return 1;
    }
    // w ... r �̷��� ���� ��
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

// enq �Ҷ� ���
int CRingBuffer::makeWSARecvBuf(LPWSABUF wsaBuf)
{
    UINT32 w = writePos;
    UINT32 r = readPos;

    // w ... r �̷��� ���� ��
    if (r > w)
    {
        wsaBuf[0].buf = buffer + w;
        wsaBuf[0].len = r - w - 1;  // ��ĭ�� ����� �־���ϱ� ������ 1�� ����. 

        return 1;
    }
    // r ... w �̷��� ���� ��
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
