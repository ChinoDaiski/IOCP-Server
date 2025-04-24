#pragma once

class ContentManager;

class Managers
{
public:
    // ���� ������ ������ ������ ���� �̱���
    static Managers& GetInstance() {
        static Managers _inst;
        return _inst;
    }

public:
    // ����/�̵� ����
    Managers(const Managers&) = delete;
    Managers& operator=(const Managers&) = delete;
    Managers(Managers&&) = delete;
    Managers& operator=(Managers&&) = delete;

private:
    Managers() = default;  // ������ ����
    ~Managers() = default;

public:
    void Init(HANDLE hIOCP);

public:
    ContentManager* Content(void) { return pContentMgr; }

private:
    ContentManager* pContentMgr;
};

