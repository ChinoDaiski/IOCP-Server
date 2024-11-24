#pragma once

#define CQSIZE 50

#include <Windows.h>

template <typename T>
class CircularQueue {
public:
    // ������
    CircularQueue(UINT32 size = CQSIZE) : rear(0), capacity(size), threadID(0){
        //queue = new T[capacity];  // ���� �迭 �Ҵ�
    }

    // �Ҹ���
    ~CircularQueue() {
        //delete[] queue;  // ���� �迭 ����
    }

    // ť�� ������ �߰� (enqueue)
    void enqueue(const T& data) {
        UINT32 inc = InterlockedIncrement(&rear);
        rear = inc % capacity;  // ���� �迭 ó��
        queue[rear] = data;
    }

private:
    T queue[CQSIZE];           // ť �迭
    UINT32 rear;        // ť�� �հ� �� �ε���
    UINT32 capacity;    // ť�� �ִ� ũ��

public:
    DWORD threadID;
};