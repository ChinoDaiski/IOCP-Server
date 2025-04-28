#pragma once

#include "Singleton.h"

class ContentManager;
class TimerManager;

class Managers : public Singleton<Managers>
{
private:
    friend class Singleton<Managers>;  // 싱글턴 생성 허용  

private:
    Managers();
    void Initialize();

public:
    ContentManager* Content(void) { return pContentMgr; }
    TimerManager* Timer(void) { return pTimerMgr; }

private:
    ContentManager* pContentMgr;
    TimerManager* pTimerMgr;
};


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