#pragma once

#include "MemoryPool.h"

template <typename T>
class concurrent_stack {
private:
    struct Node {
        T data;
        Node* next;

        Node(T _data, Node* _next) : data{ _data }, next{ _next } {}
    };

    struct AddressConverter {
        static constexpr UINT64 POINTER_MASK = 0x00007FFFFFFFFFFF; // 하위 47비트
        static constexpr UINT64 STAMP_SHIFT = 47;

        // 노드 주소에 stamp 추가
        static UINT64 AddStamp(Node* node, UINT64 stamp) {
            UINT64 pointerValue = reinterpret_cast<UINT64>(node);
            return (pointerValue & POINTER_MASK) | (stamp << STAMP_SHIFT);
        }

        // 노드 주소 추출
        static Node* ExtractNode(UINT64 taggedPointer) {
            return reinterpret_cast<Node*>(taggedPointer & POINTER_MASK);
        }
    };

    //bool CAS(UINT64* target, UINT64 expected, UINT64 desired) {
    //    return InterlockedCompareExchange64(reinterpret_cast<LONG64*>(target), desired, expected) == expected;
    //}

public:
    concurrent_stack() : top(0), stamp(0) {}

    ~concurrent_stack() {
        T value;
        while (pop(value));
    }

    void push(const T& value)
    {
        Node* newNode = pool.Alloc();
        newNode->data = value;

        Node* currentNode;

        UINT64 stValue = InterlockedIncrement(&stamp);
        UINT64 currentTop;
        UINT64 newTop;

        while (true) {
            currentTop = top;
            currentNode = AddressConverter::ExtractNode(currentTop);

            newNode->next = currentNode; // 새로운 노드의 next를 현재 top으로 설정

            newTop = AddressConverter::AddStamp(newNode, stValue);

            if (CAS(&top, currentTop, newTop)) {
                break; // 성공적으로 Push 완료
            }
        }
    }

    bool pop(T& value) {
        Node* currentNode;
        Node* nextNode = nullptr;
        UINT64 currentTop;
        UINT64 newTop;
        UINT64 stValue = InterlockedIncrement(&stamp);

        while (true) {
            currentTop = top;
            currentNode = AddressConverter::ExtractNode(currentTop);

            if (!currentNode) {
                return false; // 스택이 비어 있음
            }

            nextNode = currentNode->next;
            newTop = AddressConverter::AddStamp(nextNode, stValue);

            if (CAS(&top, currentTop, newTop)) {
                value = currentNode->data;
                pool.Free(currentNode);
                return true; // 성공적으로 Pop 완료
            }
        }
    }


public:
    UINT32 GetCurPoolCount(void) { return pool.GetCurPoolCount(); }
    UINT32 GetMaxPoolCount(void) { return pool.GetMaxPoolCount(); }

private:
    UINT64 top; // Top을 나타내는 tagged pointer
    UINT64 stamp; // 기준이 되는 stamp 값

    MemoryPool<Node, false> pool;
};