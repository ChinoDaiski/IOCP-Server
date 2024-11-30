
#include "pch.h"
#include "Packet.h"
#include <iostream>

CPacket::CPacket(int iBufferSize)
{
	m_iBufferSize = iBufferSize;
	m_iFront = m_iRear = 0;

	m_chpBuffer = new char[m_iBufferSize];
	ZeroMemory(m_chpBuffer, m_iBufferSize);

	m_refCount = 0; 
	
	// 지금은 처음 생성하자마자 사용하니깐 refcount를 1 증가시킨다.
	// 나중엔 메모리 풀을 사용해서 가져올 때 가져가는 측에서 AddRef를 호출할 것.
	AddRef();
}

CPacket::~CPacket()
{
	Clear();

	delete m_chpBuffer;
}

void CPacket::Clear(void)
{
	if (m_iFront > m_iRear)
		DebugBreak();

	m_iFront = m_iRear = 0;
}

int CPacket::MoveWritePos(int iSize)
{
	m_iRear += iSize;

	return m_iRear;
}

int CPacket::MoveReadPos(int iSize)
{
	m_iFront += iSize;

	return m_iFront;
}

CPacket& CPacket::operator=(CPacket& clSrcPacket)
{
	m_iFront = clSrcPacket.m_iFront;
	m_iRear = clSrcPacket.m_iRear;
	memcpy(m_chpBuffer, clSrcPacket.m_chpBuffer, m_iRear);

	return *this;
}

int CPacket::GetData(char* chpDest, int iSize)
{
	std::memcpy(chpDest, m_chpBuffer + m_iFront, iSize);
	MoveReadPos(iSize);

	return 0;
}

int CPacket::PutData(char* chpSrc, int iSrcSize)
{
	if ((m_iRear + iSrcSize) > m_iBufferSize)
	{
		// 패킷의 최대 사이즈 보다 더 많은 값이 들어갈 예정.
		DebugBreak();
	}

	std::memcpy(m_chpBuffer + m_iRear, chpSrc, iSrcSize);
	MoveWritePos(iSrcSize);

	return 0;
}

UINT16 CPacket::AddRef(void)
{
	return InterlockedIncrement16(&m_refCount);
}

UINT16 CPacket::ReleaseRef(void)
{
	return InterlockedDecrement16(&m_refCount);
}
