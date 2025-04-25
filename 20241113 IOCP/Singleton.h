

//------------------------------------------------------------------------------  
// 윈도우 Interlocked 함수만 사용한 락-프리 더블 체크(singleton) 구현  
// T 타입은 기본 생성자를 public 으로 가지며 복사/대입이 금지되어야 한다.
// 초기화 중인 스레드는 다른 스레드가 대기했다가 초기화 완료 후 인스턴스를 반환한다.  
//------------------------------------------------------------------------------  
#include <Windows.h>

template <typename T>
class Singleton {
public:
    /*
        싱글턴 인스턴스 반환
        최초 호출 시에만 인스턴스 생성 및 Initialize() 호출
        다른 스레드는 초기화 완료 시까지 대기했다가 인스턴스를 반환
     */
    static T& GetInstance() {
        // 이미 초기화가 완료되어 있으면 즉시 반환
        if (InterlockedCompareExchange(&initState, initState, 2) == 2) {
            return *instancePtr;
        }

        // initState: 0=미초기화, 1=초기화 중, 2=초기화 완료
        // 0에서 1로 전환하는 스레드가 단 하나만 초기화 담당
        LONG prevState = InterlockedCompareExchange(&initState, 1, 0);
        if (prevState == 0) {
            // 이 스레드가 최초 초기화 담당
            T* newObj = new T();
            newObj->Initialize();            // 초기화 콜백 (한 번만)

            // 인스턴스 포인터를 등록 (단순 대입)
            instancePtr = newObj;

            // InterlockedExchange 호출로 메모리 배리어 역할 수행
            // 이전 쓰기(store) 연산이 캐시에 플러시된 후 initState가 2로 설정됨
            InterlockedExchange(&initState, 2);
        }
        else {
            // 다른 스레드가 초기화 중 -> 완료될 때까지 대기
            while (InterlockedCompareExchange(&initState, initState, 1) != 2) {
                // 다른 스레드에 실행 기회를 주기 위해 0ms만큼 대기
                Sleep(0);
            }
        }
        return *instancePtr;
    }

    // 복사 생성자 및 대입 연산자 금지  
    Singleton(const Singleton&) = delete;
    Singleton& operator=(const Singleton&) = delete;
    Singleton(Singleton&&) = delete;
    Singleton& operator=(Singleton&&) = delete;

protected:
    // 생성자/소멸자는 protected로 두어 외부에서 직접 생성/삭제 금지  
    Singleton() = default;
    ~Singleton() = default;

private:
    // singleton 인스턴스 포인터 (volatile로 선언하여 메모리 순서 보장), 사실 최적화 옵션을 사용하지 않기에 굳이라는 생각이 들지만, 혹시 몰라 추가함
    static T* volatile instancePtr;
    // 초기화 상태 변수: 0=미초기화, 1=초기화 중, 2=초기화 완료
    static UINT32 initState;
};

//------------------------------------------------------------------------------  
// 정적 멤버 변수 정의  
//------------------------------------------------------------------------------  

template <typename T>
T* volatile Singleton<T>::instancePtr = nullptr;

template <typename T>
UINT32 Singleton<T>::initState = 0;

//-----------------------------------------------------------------------------  
// 사용 예시  
//-----------------------------------------------------------------------------  
// class MyManager : public Singleton<MyManager>  
// {   
//     friend class Singleton<MyManager>;  // 싱글턴 생성 허용  
//  
// private:  
//     MyManager() { /* 초기화 코드 */ }  
//     void Initialize() { /* 초기화 로직 */ }
// };  