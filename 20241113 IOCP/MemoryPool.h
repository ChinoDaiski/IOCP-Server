#pragma once

#include <iostream>
#include <chrono>
#include <vector>
#include <new>
#include <Windows.h>

//#include "Profile.h"

#include "CircularQueue.h"

#define GUARD_VALUE 0xAAAABBBBCCCCDDDD

// Node 구조체 정의

#ifdef _DEBUG
#pragma pack(1)
#endif // _DEBUG


template<typename T>
struct Node
{
    // x64 환경에서 빌드될것을 고려해서 UINT64 형을 사용
#ifdef _DEBUG

    UINT64 BUFFER_GUARD_FRONT;
    T data;
    UINT64 BUFFER_GUARD_END;
    ULONG_PTR POOL_INSTANCE_VALUE;
    Node<T>* next;

#endif // _DEBUG
#ifndef _DEBUG

    T data;
    Node<T>* next;

#endif // !_DEBUG
};

#ifdef _DEBUG
#pragma pop
#endif // _DEBUG

template<typename T>
struct AddressConverter {
    static constexpr UINT64 POINTER_MASK = 0x00007FFFFFFFFFFF; // 하위 47비트
    static constexpr UINT64 STAMP_SHIFT = 47;

    // 노드 주소에 stamp 추가
    static UINT64 AddStamp(Node<T>* node, UINT64 stamp) {
        UINT64 pointerValue = reinterpret_cast<UINT64>(node);
        return (pointerValue & POINTER_MASK) | (stamp << STAMP_SHIFT);
    }

    // 노드 주소 추출
    static Node<T>* ExtractNode(UINT64 taggedPointer) {
        return reinterpret_cast<Node<T>*>(taggedPointer & POINTER_MASK);
    }
};

//bool CAS(UINT64* target, UINT64 expected, UINT64 desired) {
//    return InterlockedCompareExchange64(reinterpret_cast<LONG64*>(target), desired, expected) == expected;
//}

#define CAS(target, expected, desired) (InterlockedCompareExchange64(reinterpret_cast<LONG64*>(target), desired, expected) == (expected))


//// 디버깅 정보 기록
//queue.enqueue(DebugNode{
//    reinterpret_cast<long long>(pNode),
//    reinterpret_cast<long long>(pNewNode),
//    ACTION::PUSH,
//    GetCurrentThreadId(),
//    casSuccess,
//    GetTickCount64()
//    });

//enum class ACTION {
//    ALLOC,
//    FREE,
//    END
//};

//typedef struct _tagDebugNode {
//    long long pNode;        // 이전 노드 포인터
//    long long pNext;        // 다음 노드 포인터
//    ACTION action;          // 동작 유형 (PUSH, POP)
//    DWORD threadId;         // 스레드 ID
//    bool casSuccess;        // CAS 성공 여부
//    unsigned long long timestamp;    // 타임스탬프 (밀리초)
//
//    _tagDebugNode() {}
//    _tagDebugNode(long long a, long long b, ACTION _action, DWORD _threadId, bool _success, unsigned long long _timestamp)
//        : pNode{ a }, pNext{ b }, action{ _action }, threadId{ _threadId }, casSuccess{ _success }, timestamp{ _timestamp } {}
//} DebugNode, * PDebugNode;


// MemoryPool 클래스 정의
template<typename T, bool bPlacementNew>
class MemoryPool
{
public:
    // 생성자
    MemoryPool(UINT32 sizeInitialize = 0);

    // 소멸자
    virtual ~MemoryPool(void);
    
    // 풀에 있는 객체를 넘겨주거나 새로 할당해 넘김
    T* Alloc(void);
    
    // 객체를 풀에 반환
    bool Free(T* ptr);
   
public:
    UINT32 GetCurPoolCount(void){ return InterlockedCompareExchange(&m_curPoolCount, 0, 0); }
    UINT32 GetMaxPoolCount(void){ return InterlockedCompareExchange(&m_maxPoolCount, 0, 0); }

private:
    //Node<T>* m_freeNode;
    UINT32 m_curPoolCount; // 풀에서 사용하는 노드 갯수, Alloc되면 1 감소, Free되면 1 증가
    UINT32 m_maxPoolCount; // 풀에서 사용하는 최대 노드 갯수

private:
    UINT64 top; // Top을 나타내는 tagged pointer, Node<T>* m_freeNode가 바뀐 형태
    UINT64 stamp; // 기준이 되는 stamp 값

//public:
//    CircularQueue<DebugNode> debugQueue;
};

template<typename T, bool bPlacementNew>
inline MemoryPool<T, bPlacementNew>::MemoryPool(UINT32 sizeInitialize)
{
    top = 0;
    m_curPoolCount = 0;
    m_maxPoolCount = 0;
    stamp = 0;

    T** pArr = new T * [sizeInitialize];

    // 초기 메모리 공간 준비
    if (sizeInitialize > 0)
    {
        for (UINT32 i = 0; i < sizeInitialize; i++)
        {
            pArr[i] = Alloc();
        }
        
        for (UINT32 i = 0; i < sizeInitialize; i++)
        {
            Free(pArr[i]);
        }
    }

    delete[] pArr;
}

template<typename T, bool bPlacementNew>
inline MemoryPool<T, bPlacementNew>::~MemoryPool(void)
{
    Node<T>* currentNode;
    Node<T>* nextNode = nullptr;
    UINT64 currentTop;
    UINT64 newTop;
    UINT64 stValue = InterlockedIncrement(&stamp);

    while (true) {
        currentTop = top;
        currentNode = AddressConverter<T>::ExtractNode(currentTop);

        if (!currentNode) {
            // 스택이 비어 있음
            break;
        }

        nextNode = currentNode->next;
        newTop = AddressConverter<T>::AddStamp(nextNode, stValue);

        if (CAS(&top, currentTop, newTop)) {
            delete currentNode;
            InterlockedDecrement(&m_maxPoolCount);
        }
    }

    // 만약 할당 해제가 전부 완료되지 않았다면
    if (m_maxPoolCount != 0)
    {
        // 음... 어떻게할지 나중에 정하자구.
    }
}

// 만약 비어있다면 할당, 있다면 pop하고 반환인데...
template<typename T, bool bPlacementNew>
inline T* MemoryPool<T, bPlacementNew>::Alloc(void)
{
    Node<T>* currentNode;
    Node<T>* nextNode = nullptr;
    UINT64 currentTop;
    UINT64 newTop;
    UINT64 stValue = InterlockedIncrement(&stamp);

    while (true) {
        currentTop = top;
        currentNode = AddressConverter<T>::ExtractNode(currentTop);

        // 스택이 비어 있다면 새로 노드를 생성해서 반환
        if (!currentNode) {
            // m_freeNode가 nullptr이라면 풀에 객체가 존재하지 않는다는 의미이므로 새로운 객체 할당

    // 새 노드 할당
            Node<T>* newNode = (Node<T>*)malloc(sizeof(Node<T>));
            ZeroMemory(newNode, sizeof(T));

#ifdef _DEBUG
            // 디버깅용 가드. 가드 값도 확인하고, 반환되는 풀의 정보가 올바른지 확인하기 위해 사용
            newNode->BUFFER_GUARD_FRONT = GUARD_VALUE;
            newNode->BUFFER_GUARD_END = GUARD_VALUE;

            newNode->POOL_INSTANCE_VALUE = reinterpret_cast<ULONG_PTR>(this);
#endif // _DEBUG

            newNode->next = nullptr;    // 정확힌 m_freeNode를 대입해도 된다. 근데 이 자체가 nullptr이니 보기 쉽게 nullptr 넣음.

            // placement New 옵션이 켜져있다면 생성자 호출
            if constexpr (bPlacementNew)
            {
                //new (reinterpret_cast<char*>(newNode) + offsetof(Node<T>, data)) T();
                new (&(newNode->data)) T();
            }

            // 풀에서 사용하는 최대 노드 갯수를 1 증가
            InterlockedIncrement(&m_maxPoolCount);

            // 객체의 T타입 데이터 반환
            return reinterpret_cast<T*>(reinterpret_cast<char*>(newNode) + offsetof(Node<T>, data));
        }

        nextNode = currentNode->next;
        newTop = AddressConverter<T>::AddStamp(nextNode, stValue);

        if (CAS(&top, currentTop, newTop)) {

            // placement New 옵션이 켜져있다면 생성자 호출
            if constexpr (bPlacementNew)
            {
                //new (reinterpret_cast<char*>(returnNode) + offsetof(Node<T>, data)) T();
                new (&(currentNode->data)) T();
            }

            // 풀에 존재하는 노드 갯수를 1 감소
            InterlockedDecrement(&m_curPoolCount);

            // 객체의 T타입 데이터 반환
            return &currentNode->data;
        }
    }
}

template<typename T, bool bPlacementNew>
inline bool MemoryPool<T, bPlacementNew>::Free(T* ptr)
{
#ifdef _DEBUG
    // 반환하는 값이 존재하지 않는다면
    if (ptr == nullptr)
    {
        // 실패
        return false;
    }
#endif // _DEBUG

    // offsetof(Node<T>, data)) => Node 구조체의 data 변수와 Node 구조체와의 주소 차이값을 계산
    // 여기선 debug 모드일때 가드가 있으므로 4, release일 경우 0으로 처리
    Node<T>* pNode = reinterpret_cast<Node<T>*>(reinterpret_cast<char*>(ptr) - offsetof(Node<T>, data));

#ifdef _DEBUG 
    // 스택 오버, 언더 플로우 감지
    if (
        pNode->BUFFER_GUARD_FRONT != GUARD_VALUE ||
        pNode->BUFFER_GUARD_END != GUARD_VALUE
        )
    {
        // 만약 다르다면 false 반환
        return false;
    }

    //  풀 반환이 올바른지 검사
    if (pNode->POOL_INSTANCE_VALUE != reinterpret_cast<ULONG_PTR>(this))
    {
        // 이 시점에서 어떻게 할지... 나중에 결정.


        // 만약 다르다면 false 반환
        return false;
    }
#endif // _DEBUG

    // 만약 placement new 옵션이 켜져 있다면, 생성을 placement new로 했을 것이므로 해당 객체의 소멸자를 수동으로 불러줌
    if constexpr (bPlacementNew)
    {
        pNode->data.~T();
    }

    Node<T>* currentNode;

    UINT64 stValue = InterlockedIncrement(&stamp);
    UINT64 currentTop;
    UINT64 newTop;

    while (true) {
        currentTop = top;
        currentNode = AddressConverter<T>::ExtractNode(currentTop);

        pNode->next = currentNode; // 새로운 노드의 next를 현재 top으로 설정

        newTop = AddressConverter<T>::AddStamp(pNode, stValue);

        if (CAS(&top, currentTop, newTop)) {
            break; // 성공적으로 Push 완료
        }
    }

    // 풀에 존재하는 노드 갯수를 1 증가
    InterlockedIncrement(&m_curPoolCount);

    // 반환 성공
    return true;
}