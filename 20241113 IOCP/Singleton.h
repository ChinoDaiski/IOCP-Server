

//------------------------------------------------------------------------------  
// ������ Interlocked �Լ��� ����� ��-���� ���� üũ(singleton) ����  
// T Ÿ���� �⺻ �����ڸ� public ���� ������ ����/������ �����Ǿ�� �Ѵ�.
// �ʱ�ȭ ���� ������� �ٸ� �����尡 ����ߴٰ� �ʱ�ȭ �Ϸ� �� �ν��Ͻ��� ��ȯ�Ѵ�.  
//------------------------------------------------------------------------------  
#include <Windows.h>

template <typename T>
class Singleton {
public:
    /*
        �̱��� �ν��Ͻ� ��ȯ
        ���� ȣ�� �ÿ��� �ν��Ͻ� ���� �� Initialize() ȣ��
        �ٸ� ������� �ʱ�ȭ �Ϸ� �ñ��� ����ߴٰ� �ν��Ͻ��� ��ȯ
     */
    static T& GetInstance() {
        // �̹� �ʱ�ȭ�� �Ϸ�Ǿ� ������ ��� ��ȯ
        if (InterlockedCompareExchange(&initState, initState, 2) == 2) {
            return *instancePtr;
        }

        // initState: 0=���ʱ�ȭ, 1=�ʱ�ȭ ��, 2=�ʱ�ȭ �Ϸ�
        // 0���� 1�� ��ȯ�ϴ� �����尡 �� �ϳ��� �ʱ�ȭ ���
        LONG prevState = InterlockedCompareExchange(&initState, 1, 0);
        if (prevState == 0) {
            // �� �����尡 ���� �ʱ�ȭ ���
            T* newObj = new T();
            newObj->Initialize();            // �ʱ�ȭ �ݹ� (�� ����)

            // �ν��Ͻ� �����͸� ��� (�ܼ� ����)
            instancePtr = newObj;

            // InterlockedExchange ȣ��� �޸� �踮�� ���� ����
            // ���� ����(store) ������ ĳ�ÿ� �÷��õ� �� initState�� 2�� ������
            InterlockedExchange(&initState, 2);
        }
        else {
            // �ٸ� �����尡 �ʱ�ȭ �� -> �Ϸ�� ������ ���
            while (InterlockedCompareExchange(&initState, initState, 1) != 2) {
                // �ٸ� �����忡 ���� ��ȸ�� �ֱ� ���� 0ms��ŭ ���
                Sleep(0);
            }
        }
        return *instancePtr;
    }

    // ���� ������ �� ���� ������ ����  
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

protected:
    // ������/�Ҹ��ڴ� protected�� �ξ� �ܺο��� ���� ����/���� ����  
    Singleton() = default;
    ~Singleton() = default;

private:
    // singleton �ν��Ͻ� ������ (volatile�� �����Ͽ� �޸� ���� ����), ��� ����ȭ �ɼ��� ������� �ʱ⿡ ���̶�� ������ ������, Ȥ�� ���� �߰���
    static T* volatile instancePtr;
    // �ʱ�ȭ ���� ����: 0=���ʱ�ȭ, 1=�ʱ�ȭ ��, 2=�ʱ�ȭ �Ϸ�
    static UINT32 initState;
};

//------------------------------------------------------------------------------  
// ���� ��� ���� ����  
//------------------------------------------------------------------------------  

template <typename T>
T* volatile Singleton<T>::instancePtr = nullptr;

template <typename T>
UINT32 Singleton<T>::initState = 0;

//-----------------------------------------------------------------------------  
// ��� ����  
//-----------------------------------------------------------------------------  
// class MyManager : public Singleton<MyManager>  
// {   
//     friend class Singleton<MyManager>;  // �̱��� ���� ���  
//  
// private:  
//     MyManager() { /* �ʱ�ȭ �ڵ� */ }  
//     void Initialize() { /* �ʱ�ȭ ���� */ }
// };  