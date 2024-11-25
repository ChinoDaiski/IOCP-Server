#pragma once

class CPacket;

class IServer
{
public:
    virtual ~IServer() = default;

    // �������� ȣ���ϴ� �Լ�
public:
    // �������� ���� ���� ������ �� ȣ��Ǵ� �Լ�
    virtual void SendPacket(UINT64 sessionID, CPacket* pPacket) = 0;
};
