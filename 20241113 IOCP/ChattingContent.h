#pragma once

#include "Content.h"

// ChattingContent
// - 채팅 기능을 구현하는 예시 컨텐츠 클래스
// - 세션 입장/퇴장 시 시스템 메시지를 브로드캐스트
// - Update()에서 수신된 CPacket(채팅 메시지)을 모든 세션으로 전송

class ChattingContent : public Content {
public:
    ChattingContent(int id, ContentManager* mgr);

    // 세션이 컨텐츠에 입장할 때 호출
    void OnSessionEnter(CSession* s) override;

    // 세션이 컨텐츠를 떠날 때 호출
    void OnSessionLeave(CSession* s) override;

    // 매 Tick마다 호출되는 본 컨텐츠 로직
    void Update() override;

    // 매 Tick마다 호출되는 컨텐츠에 쌓인 패킷을 처리하는 함수. Update 보다 먼저 호출된다.
    void ProcessPacket(CPacket* p) override;
};