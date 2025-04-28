
#pragma once

#include "pch.h"
#include "Object.h"
#include "Session.h"

#include "Packet.h"
#include "Managers.h"
#include "TimerManager.h"

UINT32 CObject::g_ID = 1;

CObject::CObject() noexcept
	: m_pSession(nullptr), m_bDead(false)
{
	m_ID = g_ID;
	g_ID++;
}

void CObject::Init(void)
{
	m_lastTimeoutCheckTime = Managers::GetInstance().Timer()->GetCurrServerTime();

	m_bDead = false;
}

void CObject::Update(void)
{
	CheckTimeout();
}

void CObject::LateUpdate(void)
{
}

void CObject::CheckTimeout(void)
{
	TimerManager::TimePoint currSeverTime = Managers::GetInstance().Timer()->GetCurrServerTime();

	// dfNETWORK_PACKET_RECV_TIMEOUT (ms ����) �̻� �����ٸ�
	if (dfNETWORK_PACKET_RECV_TIMEOUT < std::chrono::duration_cast<std::chrono::milliseconds>(currSeverTime - m_lastTimeoutCheckTime).count()) {
		// Timeout���� ���� ���� ����
		m_bDead = true;
	}
	// ������ �ʾҴٸ�
	else
		// ������ üũ �ð� ����
		m_lastTimeoutCheckTime = currSeverTime;
}