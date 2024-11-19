#pragma once

class CPacket;

class IServer
{
public:
    virtual ~IServer() = default;

    // �������� ȣ���ϴ� �Լ�
public:
    // �������� ���� ���� ������ �� ȣ��Ǵ� �Լ�
    virtual void SendPacket(int sessionID, CPacket* pPacket) = 0;
};
