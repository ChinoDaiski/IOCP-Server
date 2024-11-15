
#pragma once

#define RINGBUFFER_SIZE 10000



#include <iostream>
#include <string>

#define NOMINMAX
#include <Windows.h>


class CRingBuffer {
public:
    // 생성자: 버퍼 크기를 설정하고 버퍼를 초기화합니다.
    explicit CRingBuffer(int iBufferSize = RINGBUFFER_SIZE)
        : m_iCapacity(iBufferSize), m_iReadPos(0), m_iWritePos(0), m_iSize(0) {
        m_pBuffer = new char[m_iCapacity];

        InitializeCriticalSection(&m_cs);
    }

    // 소멸자: 동적 메모리로 할당한 버퍼를 해제합니다.
    ~CRingBuffer() {
        delete[] m_pBuffer;
        DeleteCriticalSection(&m_cs);
    }

    // 인덱스를 링 버퍼의 유효 범위 내로 감싸주는 함수
    int GetWrappedIndex(int index) const {
        return index % m_iCapacity;
    }

    // 버퍼 크기를 조정하는 함수
    void Resize(int newSize) {
        if (newSize <= 0) // 유효하지 않은 크기면 함수 종료
            return;

        // 새 버퍼 크기가 현재 데이터 크기보다 작으면 오류 발생
        if (m_iSize > newSize) {
            //DebugBreak();
            //throw std::runtime_error("Resize error: New buffer size is too small to hold existing data.");
        }

        char* newBuffer = new char[newSize]; // 새 버퍼 생성

        // 기존 데이터가 있으면 새 버퍼로 복사
        if (m_iSize > 0) {
            // 데이터가 연속된 경우
            if (m_iReadPos < m_iWritePos) {
                memcpy(newBuffer, m_pBuffer + m_iReadPos, m_iSize);
            }
            // 데이터가 나누어진 경우
            else {
                int firstPartSize = m_iCapacity - m_iReadPos;
                memcpy_s(newBuffer, firstPartSize, m_pBuffer + m_iReadPos, firstPartSize);
                memcpy_s(newBuffer + firstPartSize, m_iWritePos, m_pBuffer, m_iWritePos);
            }
        }

        delete[] m_pBuffer; // 기존 버퍼 삭제

        m_pBuffer = newBuffer; // 새 버퍼 설정
        m_iCapacity = newSize; // 버퍼 용량 업데이트

        m_iReadPos = 0; // 새로운 front 설정
        m_iWritePos = m_iSize; // 새로운 rear 설정
    }

    // 버퍼의 총 용량을 반환합니다.
    int GetBufferSize(void) {
        return m_iCapacity;
    }

    // 현재 사용 중인 버퍼의 크기를 반환합니다.
    int GetUseSize(void) {
        return m_iSize;
    }

    // 현재 사용 가능한 버퍼의 크기를 반환합니다.
    int GetFreeSize(void) {
        return m_iCapacity - m_iSize;
    }

    // 데이터를 버퍼에 추가하는 함수
    int Enqueue(const char* chpData, int iSize) {

        EnterCriticalSection(&m_cs);

        if (GetFreeSize() < iSize) {
            iSize = GetFreeSize(); // 남은 용량만큼만 데이터 추가
        }

        int firstPart = std::min(iSize, m_iCapacity - m_iWritePos);
        int secondPart = iSize - firstPart;

        memcpy(m_pBuffer + m_iWritePos, chpData, firstPart);
        memcpy(m_pBuffer, chpData + firstPart, secondPart);

        m_iWritePos = GetWrappedIndex(m_iWritePos + iSize); // 새로운 rear 위치 설정
        m_iSize += iSize; // 현재 사용 중인 버퍼 크기 업데이트

        LeaveCriticalSection(&m_cs);

        return iSize;
    }

    // 버퍼에서 데이터를 제거하는 함수
    int Dequeue(char* chpDest, int iSize) {


        if (m_iSize < iSize) {
            iSize = m_iSize; // 현재 사용 중인 용량만큼만 데이터 제거
        }

        int firstPart = std::min(iSize, m_iCapacity - m_iReadPos);
        int secondPart = iSize - firstPart;

        memcpy(chpDest, m_pBuffer + m_iReadPos, firstPart);
        memcpy(chpDest + firstPart, m_pBuffer, secondPart);

        m_iReadPos = GetWrappedIndex(m_iReadPos + iSize); // 새로운 front 위치 설정

        EnterCriticalSection(&m_cs);
        m_iSize -= iSize; // 현재 사용 중인 버퍼 크기 업데이트
        LeaveCriticalSection(&m_cs);

        return iSize;
    }

    // 데이터를 제거하지 않고 버퍼의 앞부분을 확인하는 함수
    int Peek(char* chpDest, int iSize) {
        if (m_iSize < iSize) {
            iSize = m_iSize; // 현재 사용 중인 용량만큼만 데이터 확인
        }

        int firstPart = std::min(iSize, m_iCapacity - m_iReadPos);
        int secondPart = iSize - firstPart;

        memcpy_s(chpDest, firstPart, m_pBuffer + m_iReadPos, firstPart);
        memcpy_s(chpDest + firstPart, secondPart, m_pBuffer, secondPart);

        return iSize;
    }

    // 버퍼를 비우는 함수
    void ClearBuffer(void) {
        m_iReadPos = 0;
        m_iWritePos = 0;
        m_iSize = 0;
    }

    // 현재 쓰기 위치에서 직접 추가할 수 있는 크기를 반환합니다.
    int DirectEnqueueSize(void) {
        if (m_iReadPos > m_iWritePos)
            return m_iReadPos - m_iWritePos;
        else
            return m_iCapacity - m_iWritePos;
    }

    // 현재 읽기 위치에서 직접 제거할 수 있는 크기를 반환합니다.
    int DirectDequeueSize(void) {
        if (m_iReadPos < m_iWritePos)
            return m_iWritePos - m_iReadPos;
        else
            return m_iCapacity - m_iReadPos;
    }

    // 쓰기 위치를 이동시키는 함수 (데이터를 추가)
    inline int MoveRear(int iSize) {
        int moveSize = std::min(iSize, GetFreeSize());
        m_iWritePos = GetWrappedIndex(m_iWritePos + moveSize);
        m_iSize += moveSize;
        return moveSize;
    }

    // 읽기 위치를 이동시키는 함수 (데이터를 제거)
    inline int MoveFront(int iSize) {
        int moveSize = std::min(iSize, GetUseSize());
        m_iReadPos = GetWrappedIndex(m_iReadPos + moveSize);
        m_iSize -= moveSize;
        return moveSize;
    }

    // 버퍼의 front 부분의 포인터를 반환합니다.
    char* GetFrontBufferPtr(void) {
        return m_pBuffer + m_iReadPos;
    }

    // 버퍼의 rear 부분의 포인터를 반환합니다.
    char* GetRearBufferPtr(void) {
        return m_pBuffer + m_iWritePos;
    }

    // 버퍼 포인터 반환
    char* GetBufferPtr(void)
    {
        return m_pBuffer;
    }
    // 버퍼의 크기 반환
    int GetBufferCapacity(void)
    {
        return m_iCapacity;
    }


private:
    char* m_pBuffer;    // 링버퍼의 데이터 저장 배열
    int m_iCapacity;    // 버퍼의 전체 용량
    int m_iReadPos;     // 현재 읽기 위치 (front)
    int m_iWritePos;    // 현재 쓰기 위치 (rear)
    int m_iSize;        // 현재 사용 중인 데이터의 크기

    CRITICAL_SECTION m_cs;
};
