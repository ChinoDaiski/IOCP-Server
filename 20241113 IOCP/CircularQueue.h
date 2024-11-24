#pragma once

#define CQSIZE 50

#include <Windows.h>

template <typename T>
class CircularQueue {
public:
    // 생성자
    CircularQueue(UINT32 size = CQSIZE) : rear(0), capacity(size), threadID(0){
        //queue = new T[capacity];  // 동적 배열 할당
    }

    // 소멸자
    ~CircularQueue() {
        //delete[] queue;  // 동적 배열 해제
    }

    // 큐에 데이터 추가 (enqueue)
    void enqueue(const T& data) {
        UINT32 inc = InterlockedIncrement(&rear);
        rear = inc % capacity;  // 원형 배열 처리
        queue[rear] = data;
    }

private:
    T queue[CQSIZE];           // 큐 배열
    UINT32 rear;        // 큐의 앞과 뒤 인덱스
    UINT32 capacity;    // 큐의 최대 크기

public:
    DWORD threadID;
};