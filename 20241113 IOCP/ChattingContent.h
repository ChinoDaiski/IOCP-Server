#pragma once

#include "Content.h"

// ChattingContent
// - ä�� ����� �����ϴ� ���� ������ Ŭ����
// - ���� ����/���� �� �ý��� �޽����� ��ε�ĳ��Ʈ
// - Update()���� ���ŵ� CPacket(ä�� �޽���)�� ��� �������� ����

class ChattingContent : public Content {
public:
    ChattingContent(int id, ContentManager* mgr);

    // ������ �������� ������ �� ȣ��
    void OnSessionEnter(CSession* s) override;

    // ������ �������� ���� �� ȣ��
    void OnSessionLeave(CSession* s) override;

    // �� Tick���� ȣ��Ǵ� �� ������ ����
    void Update() override;

    // �� Tick���� ȣ��Ǵ� �������� ���� ��Ŷ�� ó���ϴ� �Լ�. Update ���� ���� ȣ��ȴ�.
    void ProcessPacket(CPacket* p) override;
};