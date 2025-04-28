#pragma once

class CSession;
class CPacket;

// 기본 CObject 클래스 정의
class CObject {
public:
    explicit CObject() noexcept;
    virtual ~CObject() = default;

    // 메모리 풀에서 사용할 용도
    virtual void Init(void);

    // 업데이트 함수 (서버 로직에서 오브젝트 상태를 갱신하는 데 사용)
    virtual void Update(void);
    virtual void LateUpdate(void);

public:
    inline bool isDead(void) { return m_bDead; }
    void SetDead(void) { m_bDead = true; }
    
private:
    void CheckTimeout(void);    // 추기적으로 Timeout을 확인하기 위해 불러주는 함수

private:
    CSession* m_pSession;   // 오브젝트와 연결된 세션
    bool m_bDead;           // 죽었는지 여부

protected:
    UINT32 m_ID;            // ID

private:
    // Timeout 관련
    std::chrono::steady_clock::time_point m_lastTimeoutCheckTime;     // 마지막으로 패킷을 갱신한 시간

private:
    static UINT32 g_ID; // ID
};