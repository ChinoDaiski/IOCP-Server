#pragma once

#include <Pdh.h>
#include <PdhMsg.h>
#include <strsafe.h>

#pragma comment(lib, "Pdh.lib")

#include "cpuUsage.h"

#define df_PDH_ETHERNET_MAX 8
//--------------------------------------------------------------
// 이더넷 하나에 대한 Send,Recv PDH 쿼리 정보.
//--------------------------------------------------------------
struct st_ETHERNET
{
    bool _bUse;
    WCHAR _szName[128];
    PDH_HCOUNTER _pdh_Counter_Network_RecvBytes;
    PDH_HCOUNTER _pdh_Counter_Network_SendBytes;

    PDH_HQUERY _sendQuery;
    PDH_HQUERY _recvQuery;
};

class SystemMonitor {
public:
    explicit SystemMonitor(void);
    ~SystemMonitor(void);

public:
    double GetCounterValue(PDH_HCOUNTER counter);
    bool InitNetworkTrafficBytes(void);
    bool MeasureNetworkTrafficBytes(void);
    void PrintMonitoringData(void);

private:
    PDH_HQUERY queryHandle;            // PDH 쿼리 핸들

    // CPU 전체 사용률                   // 이 2개의 항목은 성능 모니터에서 코드로 측정하면 정확한 값이 나오지 않으므로 다른 방식(CPU_UsageChekcing 프로젝트)으로 측정
    // 프로세스 CPU 사용률

    PDH_HCOUNTER privateBytesCounter;  // 프로세스 유저 할당 메모리
    PDH_HCOUNTER poolNonPagedCounter;  // 프로세스 논페이지 메모리
    PDH_HCOUNTER availableMemory;      // 사용 가능 메모리
    PDH_HCOUNTER memoryPoolNonPaged;   // 논페이지드 메모리




    PDH_HQUERY _pdh_Query;

    st_ETHERNET _EthernetStruct[df_PDH_ETHERNET_MAX]; // 랜카드 별 PDH 정보
    double _pdh_value_Network_RecvBytes; // 총 Recv Bytes 모든 이더넷의 Recv 수치 합산
    double _pdh_value_Network_SendBytes; // 총 Send Bytes 모든 이더넷의 Send 수치 합산


    // CPU 전체 사용률 및 프로세스 CPU 사용률 측정
    CCpuUsage CPUTime; // CPUTime(hProcess)
};