#pragma once

class CSession;
class CPacket;

// �⺻ CObject Ŭ���� ����
class CObject {
public:
    explicit CObject() noexcept;
    virtual ~CObject() = default;

    // �޸� Ǯ���� ����� �뵵
    virtual void Init(void);

    // ������Ʈ �Լ� (���� �������� ������Ʈ ���¸� �����ϴ� �� ���)
    virtual void Update(void);
    virtual void LateUpdate(void);

public:
    inline bool isDead(void) { return m_bDead; }
    void SetDead(void) { m_bDead = true; }
    
private:
    void CheckTimeout(void);    // �߱������� Timeout�� Ȯ���ϱ� ���� �ҷ��ִ� �Լ�

private:
    CSession* m_pSession;   // ������Ʈ�� ����� ����
    bool m_bDead;           // �׾����� ����

protected:
    UINT32 m_ID;            // ID

private:
    // Timeout ����
    std::chrono::steady_clock::time_point m_lastTimeoutCheckTime;     // ���������� ��Ŷ�� ������ �ð�

private:
    static UINT32 g_ID; // ID
};