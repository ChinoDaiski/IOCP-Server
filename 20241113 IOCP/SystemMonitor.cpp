#include "pch.h"
#include "SystemMonitor.h"
#include <iostream>

SystemMonitor::SystemMonitor(void)
    : queryHandle(nullptr)
{
    PdhOpenQuery(NULL, NULL, &queryHandle);

    // 카운터 등록
    PdhAddCounter(queryHandle, L"\\Process(_Total)\\Private Bytes", NULL, &privateBytesCounter);
    PdhAddCounter(queryHandle, L"\\Process(_Total)\\Pool Nonpaged Bytes", NULL, &poolNonPagedCounter);
    PdhAddCounter(queryHandle, L"\\Memory\\Available MBytes", NULL, &availableMemory);
    PdhAddCounter(queryHandle, L"\\Memory\\Pool Nonpaged Bytes", NULL, &memoryPoolNonPaged);

    PdhAddCounter(queryHandle, L"\\TCPv4\\Segments Sent/sec", NULL, &TCPv4Send);
    PdhAddCounter(queryHandle, L"\\TCPv4\\Segments Retransmitted/sec", NULL, &TCPv4Retransmitted);

    PdhCollectQueryData(queryHandle);

    PdhOpenQuery(NULL, NULL, &_pdh_Query);	// 1인자에 NULL을 넣으면 실시간으로 데이터를 수집

    InitNetworkTrafficBytes();

    maxRetransmitted = 0;
    minRetransmitted = DBL_MAX;

    sumSendSegments = 0;
    sumSendSegments = 0;
}

SystemMonitor::~SystemMonitor(void)
{
    if (queryHandle) {
        PdhCloseQuery(queryHandle);
    }
}

double SystemMonitor::GetCounterValue(PDH_HCOUNTER counter)
{
    PDH_FMT_COUNTERVALUE value;
    PdhGetFormattedCounterValue(counter, PDH_FMT_DOUBLE, NULL, &value);
    return value.doubleValue;
}

bool SystemMonitor::InitNetworkTrafficBytes(void)
{
    int iCnt = 0;
    WCHAR* szCur = NULL;
    WCHAR* szCounters = NULL;
    WCHAR* szInterfaces = NULL;
    DWORD dwCounterSize = 0, dwInterfaceSize = 0;
    WCHAR szQuery[1024] = { 0, };

    _pdh_value_Network_RecvBytes = 0;
    _pdh_value_Network_SendBytes = 0;

    // PDH enum Object 를 사용하는 방법.
    // 모든 이더넷 이름이 나오지만 실제 사용중인 이더넷, 가상이더넷 등등을 확인불가 함.
    //---------------------------------------------------------------------------------------
    // PdhEnumObjectItems 을 통해서 "NetworkInterface" 항목에서 얻을 수 있는
    // 측성항목(Counters) / 인터페이스 항목(Interfaces) 를 얻음. 그런데 그 개수나 길이를 모르기 때문에
    // 먼저 버퍼의 길이를 알기 위해서 Out Buffer 인자들을 NULL 포인터로 넣어서 사이즈만 확인.
    //---------------------------------------------------------------------------------------
    PdhEnumObjectItems(NULL, NULL, L"Network Interface", NULL, &dwCounterSize, NULL, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0);
    szCounters = new WCHAR[dwCounterSize];
    szInterfaces = new WCHAR[dwInterfaceSize];

    //---------------------------------------------------------------------------------------
    // 버퍼의 동적할당 후 다시 호출!
    //
    // szCounters 와 szInterfaces 버퍼에는 여러개의 문자열이 쭉쭉쭉 들어온다. 2차원 배열도 아니고,
    // 그냥 NULL 포인터로 끝나는 문자열들이 dwCounterSize, dwInterfaceSize 길이만큼 줄줄이 들어있음.
    // 이를 문자열 단위로 끊어서 개수를 확인 해야 함. aaa\0bbb\0ccc\0ddd 이딴 식
    //---------------------------------------------------------------------------------------
    if (PdhEnumObjectItems(NULL, NULL, L"Network Interface", szCounters, &dwCounterSize, szInterfaces, &dwInterfaceSize, PERF_DETAIL_WIZARD, 0) != ERROR_SUCCESS)
    {
        delete[] szCounters;
        delete[] szInterfaces;
        return false;
    }
    iCnt = 0;
    szCur = szInterfaces;

    //---------------------------------------------------------
    // szInterfaces 에서 문자열 단위로 끊으면서, 이름을 복사받는다.
    //---------------------------------------------------------
    PDH_STATUS status;
    for (; *szCur != L'\0' && iCnt < df_PDH_ETHERNET_MAX; szCur += wcslen(szCur) + 1, iCnt++)
    {
        _EthernetStruct[iCnt]._bUse = true;
        _EthernetStruct[iCnt]._szName[0] = L'\0';
        wcscpy_s(_EthernetStruct[iCnt]._szName, szCur);

        szQuery[0] = L'\0';
        StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Received/sec", szCur);

        PdhOpenQuery(NULL, NULL, &_EthernetStruct[iCnt]._recvQuery);
        status = PdhAddCounter(_EthernetStruct[iCnt]._recvQuery, szQuery, NULL, &_EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes);
        if (status != ERROR_SUCCESS) {
            std::cerr << "Failed to add counter. Error: " << std::hex << status << std::endl;
        }


        szQuery[0] = L'\0';
        StringCbPrintf(szQuery, sizeof(WCHAR) * 1024, L"\\Network Interface(%s)\\Bytes Sent/sec", szCur);

        PdhOpenQuery(NULL, NULL, &_EthernetStruct[iCnt]._sendQuery);
        status = PdhAddCounter(_EthernetStruct[iCnt]._sendQuery, szQuery, NULL, &_EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes);
        if (status != ERROR_SUCCESS) {
            std::cerr << "Failed to add counter. Error: " << std::hex << status << std::endl;
        }
    }
}

bool SystemMonitor::MeasureNetworkTrafficBytes(void)
{
    ///////////////////////////////////////////////////////////////////////////
    /*
    위에서 만들어진
    _EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes
    _EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes
    PDH 카운터를 다른 PDH 카운터와 같은 방법으로 사용 해주면 됨.
    */
    //-----------------------------------------------------------------------------------------------
    // 이더넷 개수만큼 돌면서 총 합을 뽑음.
    //-----------------------------------------------------------------------------------------------
    PDH_STATUS Status;
    PDH_FMT_COUNTERVALUE CounterValue;

    _pdh_value_Network_RecvBytes = 0;
    _pdh_value_Network_SendBytes = 0;

    for (int iCnt = 0; iCnt < df_PDH_ETHERNET_MAX; iCnt++)
    {
        if (_EthernetStruct[iCnt]._bUse)
        {
            PdhCollectQueryData(_EthernetStruct[iCnt]._recvQuery);
            Status = PdhGetFormattedCounterValue(_EthernetStruct[iCnt]._pdh_Counter_Network_RecvBytes,
                PDH_FMT_DOUBLE, NULL, &CounterValue);
            if (Status == 0) _pdh_value_Network_RecvBytes += CounterValue.doubleValue;

            PdhCollectQueryData(_EthernetStruct[iCnt]._sendQuery);
            Status = PdhGetFormattedCounterValue(_EthernetStruct[iCnt]._pdh_Counter_Network_SendBytes,
                PDH_FMT_DOUBLE, NULL, &CounterValue);
            if (Status == 0) _pdh_value_Network_SendBytes += CounterValue.doubleValue;
        }
    }

    // 이렇게 연결된 모든 NIC에서 보내는 RecvBytes / SendBytes를 알 수 있음.
    return true;
}

void SystemMonitor::PrintMonitoringData(void)
{
    //================================================================================================================================
    // 프로세스가 시작되고 나서 흐른 시간 측정
    processTimer.PrintElapsedTime();
    //================================================================================================================================


    //================================================================================================================================
    CPUTime.UpdateCpuTime();

    wprintf(L"\nProcessor:\t%.3f%% / Process:\t%.3f%% \n", CPUTime.ProcessorTotal(), CPUTime.ProcessTotal());
    wprintf(L"ProcessorKernel:%.3f%% / ProcessKernel: %.3f%% \n", CPUTime.ProcessorKernel(), CPUTime.ProcessKernel());
    wprintf(L"ProcessorUser:\t%.3f%% / ProcessUser:\t%.3f%% \n\n", CPUTime.ProcessorUser(), CPUTime.ProcessUser());
    //================================================================================================================================


    // queryHandle 값 최신화. 
    PdhCollectQueryData(queryHandle);
    //================================================================================================================================
    std::wcout << L"User Allocated Memory : " << GetCounterValue(privateBytesCounter) / (1024 * 1024) << L" MB\n";
    std::wcout << L"Nonpaged Memory       : " << GetCounterValue(poolNonPagedCounter) / (1024 * 1024) << L" MB\n";
    std::wcout << L"Available Memory      : " << GetCounterValue(availableMemory) << L" MB\n";
    std::wcout << L"Nonpaged Pool Memory  : " << GetCounterValue(memoryPoolNonPaged) / (1024 * 1024) << L" MB\n\n";

    double send = GetCounterValue(TCPv4Send);
    double retransmitted = GetCounterValue(TCPv4Retransmitted);

    sumSendSegments += send;
    sumSendSegments += retransmitted;

    if (minRetransmitted > retransmitted)
        minRetransmitted = retransmitted;

    if (maxRetransmitted < retransmitted)
        maxRetransmitted = retransmitted;

    AvgRetransmitted = AvgRetransmitted / 8 * 7 + retransmitted / 8;

    std::wcout << L"Send Segment Count          : " << send << L" \n";
    std::wcout << L"Retransmitted Segment Count : " << retransmitted << L" \n";
    wprintf(L"Retransmitted Ratio         : %0.3f%%\n\n", retransmitted / (send + retransmitted) * 100);

    std::wcout << L"Min Retransmitted Segment Count : " << minRetransmitted << L" \n";
    std::wcout << L"Max Retransmitted Segment Count : " << maxRetransmitted << L" \n";
    std::wcout << L"Avg Retransmitted Segment Count : " << AvgRetransmitted << L" \n\n";
    //================================================================================================================================


    //================================================================================================================================
    if (MeasureNetworkTrafficBytes())
    {
        std::wcout << L"Network Recv Bytes     : " << _pdh_value_Network_RecvBytes / (1024) << L" Kbyte\n";

        double sendBytes = _pdh_value_Network_SendBytes / (1024);
        std::wcout << L"Network Send Bytes     : " << sendBytes << L" Kbyte\n";

        sumSendBytes += sendBytes;
    }
    else
    {
        std::wcout << L"Error : Cannot measure Network_RecvBytes\n";
        std::wcout << L"Error : Cannot measure Network_SendBytes\n";
    }
    //================================================================================================================================



    //================================================================================================================================
    std::wcout << L"\nSum Send Bytes          : " << 1.f * sumSendBytes / 1024 << L" Mbyte\n";
    std::wcout << L"Sum Send Segments Count : " << sumSendSegments << L"\n";
    //================================================================================================================================
}