
#include "pch.h"
#include "RingBuffer.h"

#include "Packet.h"

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
//
//
//CPacketQueue::CPacketQueue(void)
//    : size{ -1 }
//{
//}
//
//CPacketQueue::~CPacketQueue()
//{
//}
//
//void CPacketQueue::Enqueue(CPacket* pPacket)
//{
//    pPackets[writePos] = pPacket;
//    writePos += 1;
//    writePos %= PACKET_QUEUE_SIZE;
//}
//
//int CPacketQueue::makeWSASendBuf(LPWSABUF wsaBuf)
//{
//    // r ... w 일 경우
//    if (readPos <= writePos)
//    {
//        for (int i = readPos; i < writePos; ++i)
//        {
//            wsaBuf[i - readPos].buf = pPackets[i]->GetFrontBufferPtr();
//            wsaBuf[i - readPos].len = pPackets[i]->GetDataSize();
//        }
//
//        return writePos - readPos;
//    }
//    
//    // w ... r 일 경우
//    else
//    {
//        for (int i = readPos; i < capacity; ++i)
//        {
//            wsaBuf[i - readPos].buf = pPackets[i]->GetFrontBufferPtr();
//            wsaBuf[i - readPos].len = pPackets[i]->GetDataSize();
//        }
//
//
//        for (int i = 0; i < writePos; ++i)
//        {
//            wsaBuf[i + readPos].buf = pPackets[i]->GetFrontBufferPtr();
//            wsaBuf[i + readPos].len = pPackets[i]->GetDataSize();
//        }
//
//        return capacity - readPos + writePos;
//    }
//
//    if (w == 0)
//    {
//        useSize -= 1;
//    }
//
//    // 쓰기 위치 업데이트 
//    MoveRear(iSize);
//    return iSize;
//}
//
//int CPacketQueue::makeWSARecvBuf(LPWSABUF wsaBuf)
//{
//    return 0;
//}

CPacketQueue::CPacketQueue(void)
{
    ZeroMemory(pPackets, sizeof(CPacket*) * MAX_PACKET_QUEUE_SIZE);
}

CPacketQueue::~CPacketQueue()
{
}

void CPacketQueue::Enqueue(CPacket* pPacket)
{
    UINT32 w = writePos;
    UINT32 r = readPos;

    // enq할때 이 세션이 이미 문제가 있는 세션일 수 있으니깐 한바퀴를 다 돌지 않았는지 검사해야한다.
    if (isFull(r, w))
    {
        DebugBreak();
    }

    // 0에서 시작
    pPackets[writePos] = pPacket;
    
    // writePos 위치를 1 증가
    writePos += 1;

    // 원형이기에 % 연산이 들어감
    writePos %= capacity;
}

void CPacketQueue::Dequeue(void)
{
    // 현재 제거할 패킷을 가져옴
    CPacket* pPacket = pPackets[readPos];

    // 패킷의 release 함수를 호출해서 refCounter를 1줄이고, 줄여진 값이 0인 것을 확인
    if (pPacket->ReleaseRef() == 0)
        // 0이라면 패킷을 제거
        delete pPacket;

    // readPos 위치를 1 증가
    readPos += 1;

    // 원형이기에 % 연산이 들어감
    readPos %= capacity;
}

CPacket* CPacketQueue::GetFront(void)
{
    return pPackets[readPos];
}

void CPacketQueue::ClearQueue(void)
{
    writePos = readPos = 0;
    sendCompleteDataSize = 0;
}

bool CPacketQueue::empty(void)
{
    UINT32 w = writePos;
    UINT32 r = readPos;

    return isEmpty(r, w);
}

int CPacketQueue::makeWSASendBuf(LPWSABUF wsaBuf)
{
    UINT32 w = writePos;
    UINT32 r = readPos;

    // r ... w 이렇게 있을 때
    if (w >= r)
    {
        for (int i = r; i < w; ++i)
        {
            wsaBuf[i - r].buf = pPackets[i]->GetFrontBufferPtr();
            wsaBuf[i - r].len = pPackets[i]->GetDataSize();
        }

        return w - r;
    }
    // w ... r 이렇게 있을 때
    else
    {
        // r ~ capacity까지

        for (int i = r; i < capacity; ++i)
        {
            wsaBuf[i - r].buf = pPackets[i]->GetFrontBufferPtr();
            wsaBuf[i - r].len = pPackets[i]->GetDataSize();
        }

        int size = capacity - r;

        for (int i = 0; i < w; ++i)
        {
            wsaBuf[size + i].buf = pPackets[i]->GetFrontBufferPtr();
            wsaBuf[size + i].len = pPackets[i]->GetDataSize();
        }

        return size + w;
    }
}

void CPacketQueue::RemoveSendCompletePacket(UINT32 completeSendSize)
{
    sendCompleteDataSize += completeSendSize;

    UINT32 w = writePos;
    UINT32 r = readPos;

    while (true)
    {
        int packetDataSize = pPackets[r]->GetDataSize();

        // [ front에 있는 패킷의 데이터 크기 ]보다 [ 전송 완료된 데이터의 용량 ]이 더 크다면
        if (packetDataSize <= sendCompleteDataSize)
        {
            // deq
            Dequeue();

            r += 1;
            r %= capacity;

            // deq 된 패킷의 데이터 용량만큼 전송 완료된 데이터 용량에서 제거
            sendCompleteDataSize -= packetDataSize;

            if (sendCompleteDataSize == 0)
            {
                break;
            }
        }
        else
            break;
    }
}
