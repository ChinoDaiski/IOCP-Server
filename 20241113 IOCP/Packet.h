#pragma once

#define NOMINMAX
#include <Windows.h>
#include <cstring>
#include <type_traits>

class CPacket
{
public:
	enum class en_PACKET : UINT32
	{
		eBUFFER_DEFAULT = 100		// 패킷의 기본 버퍼 사이즈.
	};

	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Return:
	//////////////////////////////////////////////////////////////////////////
	explicit CPacket(int iBufferSize = static_cast<UINT32>(en_PACKET::eBUFFER_DEFAULT));
	virtual ~CPacket();

	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void	Clear(void);

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	int	GetBufferSize(void) { return m_iBufferSize; }

	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		GetDataSize(void) { return m_iRear - m_iFront; }

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	char* GetBufferPtr(void) { return m_chpBuffer + m_iFront; }

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);






	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	CPacket& operator = (CPacket& clSrcPacket);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	template<typename T>
	CPacket& operator<<(const T& value) {
		// 패킷에 할당되어 있는 최대 사이즈 보다 더 많은 값이 들어가는지 검사
		if ((m_iRear + sizeof(T)) > m_iBufferSize)
		{
			//StackTraceLogger sw;
			//sw.ShowCallstack();
			//throw std::runtime_error("패킷에 할당되어 있는 최대 사이즈 보다 더 많은 값이 들어가는지 검사");
		}

		if (!std::is_arithmetic<T>::value)
			return *this;

		// 데이터 복사 및 포인터 이동
		*reinterpret_cast<T*>(m_chpBuffer + m_iRear) = value;
		MoveWritePos(sizeof(T));

		return *this;
	}

	//CPacket& operator << (unsigned char byValue);
	//CPacket& operator << (char chValue);

	//CPacket& operator << (short shValue);
	//CPacket& operator << (unsigned short wValue);

	//CPacket& operator << (int iValue);
	//CPacket& operator << (long lValue);
	//CPacket& operator << (float fValue);

	//CPacket& operator << (__int64 iValue);
	//CPacket& operator << (double dValue);


	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////

	template<typename T>
	CPacket& operator>>(T& value) {
		// 넣어진 값 이상으로 뺴려고 시도
		if ((m_iFront + sizeof(T)) > m_iRear)
		{
			//StackTraceLogger sw;
			//sw.ShowCallstack();
			//throw std::runtime_error("넣어진 값 이상으로 뺴려고 시도");
		}

		if constexpr (!std::is_arithmetic<T>::value)
		{
			DebugBreak();
			return *this;
		}

		// 데이터 복사 및 포인터 이동
		value = *reinterpret_cast<T*>(m_chpBuffer + m_iFront);
		MoveReadPos(sizeof(T));

		return *this;
	}

	//CPacket& operator >> (BYTE& byValue);
	//CPacket& operator >> (char& chValue);

	//CPacket& operator >> (short& shValue);
	//CPacket& operator >> (WORD& wValue);

	//CPacket& operator >> (int& iValue);
	//CPacket& operator >> (DWORD& dwValue);
	//CPacket& operator >> (float& fValue);

	//CPacket& operator >> (__int64& iValue);
	//CPacket& operator >> (double& dValue);




	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		GetData(char* chpDest, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		PutData(char* chpSrc, int iSrcSize);


protected:
	char* m_chpBuffer;      // 데이터 저장 배열
	int m_iBufferSize;      // 버퍼의 전체 용량
	int m_iFront;           // 현재 읽기 위치 (front)
	int m_iRear;            // 현재 쓰기 위치 (rear)
};