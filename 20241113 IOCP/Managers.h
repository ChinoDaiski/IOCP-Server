#pragma once

#include "Singleton.h"

class ContentManager;
class TimerManager;

class Managers : public Singleton<Managers>
{
private:
    friend class Singleton<Managers>;  // �̱��� ���� ���  

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