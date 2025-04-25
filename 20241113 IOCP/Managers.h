#pragma once

#include "Singleton.h"

class ContentManager;

class Managers : public Singleton<Managers>
{
private:
    friend class Singleton<Managers>;  // �̱��� ���� ���  

private:
    Managers();
    void Initialize();

public:
    ContentManager* Content(void) { return pContentMgr; }

private:
    ContentManager* pContentMgr;
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