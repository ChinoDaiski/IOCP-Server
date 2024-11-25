#pragma once

#define CQSIZE 1000

#include <Windows.h>

template <typename T>
class CircularQueue {
public:
    // 생성자
    CircularQueue(UINT32 size = CQSIZE) : capacity(size), count(0){
        //queue = new T[capacity];  // 동적 배열 할당
    }

    // 소멸자
    ~CircularQueue() {
        //delete[] queue;  // 동적 배열 해제
    }

    // 큐에 데이터 추가 (enqueue)
    void enqueue(const T& data) {
        UINT32 inc = InterlockedIncrement(&count);

        UINT32 index = inc % capacity;  // 원형 배열 처리
        queue[index] = data;
    }

private:
    T queue[CQSIZE];           // 큐 배열

    UINT32 count;
    UINT32 capacity;    // 큐의 최대 크기
};
