#pragma once

class CPacket;

class IContent
{
public:
    virtual ~IContent() = default;

    // ������ ȣ���ϴ� �Լ�
public:
    // Ŭ���̾�Ʈ�κ��� �����͸� �������� �� ȣ��Ǵ� �Լ�
    virtual void OnRecv(UINT64 sessionID, CPacket* pPacket) = 0;

    // ������ ������ �ް�, ���ǰ� ����� ��ü�� ����� ���� ���������� ȣ���ؾ��ϴ� �Լ�
    virtual void OnAccept(UINT64 sessionID) = 0;
};