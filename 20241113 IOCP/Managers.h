#pragma once

class ContentManager;

class Managers
{
public:
    // 가장 간단한 형태의 스레드 안전 싱글턴
    static Managers& GetInstance() {
        static Managers _inst;
        return _inst;
    }

public:
    // 복사/이동 금지
    Managers(const Managers&) = delete;
    Managers& operator=(const Managers&) = delete;
    Managers(Managers&&) = delete;
    Managers& operator=(Managers&&) = delete;

private:
    Managers() = default;  // 생성자 은닉
    ~Managers() = default;

public:
    void Init(HANDLE hIOCP);

public:
    ContentManager* Content(void) { return pContentMgr; }

private:
    ContentManager* pContentMgr;
};

