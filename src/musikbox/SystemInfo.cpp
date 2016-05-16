#include "stdafx.h"

#include "SystemInfo.h"

#include "windows.h"
#include "psapi.h"
#include "pdh.h"

#ifdef WIN32
class WindowsSystemInfo : public SystemInfo {
    public:
        WindowsSystemInfo();
        virtual ~WindowsSystemInfo();

        virtual int64 GetTotalVirtualMemory();
        virtual int64 GetUsedVirtualMemory();
        virtual int64 GetTotalPhysicalMemory();
        virtual int64 GetUsedPhysicalMemory();
        virtual double GetCpuUsage();

    private:
        ULARGE_INTEGER  lastCpu, lastSysCpu, lastUserCpu;
        int processorCount;
        HANDLE self;
        PDH_HQUERY cpuQuery;
        PDH_HCOUNTER cpuTotal;
};
#endif

SystemInfo* SystemInfo::Create() {
#ifdef WIN32
    return new WindowsSystemInfo();
#else
    return new SystemInfo();
#endif
}

SystemInfo::SystemInfo() {

}

SystemInfo::~SystemInfo() {

}

int64 SystemInfo::GetTotalVirtualMemory() {
    return 0;
}

int64 SystemInfo::GetUsedVirtualMemory() {
    return 0;
}

int64 SystemInfo::GetTotalPhysicalMemory() {
    return 0;
}

int64 SystemInfo::GetUsedPhysicalMemory() {
    return 0;
}

double SystemInfo::GetCpuUsage() {
    return 0;
}


#ifdef WIN32

#define GET_MEMORY_STATUS \
    MEMORYSTATUSEX memInfo; \
    memInfo.dwLength = sizeof(MEMORYSTATUSEX); \
    GlobalMemoryStatusEx(&memInfo); \

#define GET_PROCESS_MEMORY_COUNTERS \
    PROCESS_MEMORY_COUNTERS_EX pmc; \
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)); \

WindowsSystemInfo::WindowsSystemInfo() {
    PdhOpenQuery(NULL, NULL, &cpuQuery);
    PdhAddCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", NULL, &cpuTotal);
    PdhCollectQueryData(cpuQuery);

    SYSTEM_INFO sysInfo;
    FILETIME ftime, fsys, fuser;

    GetSystemInfo(&sysInfo);
    processorCount = sysInfo.dwNumberOfProcessors;

    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastCpu, &ftime, sizeof(FILETIME));

    self = GetCurrentProcess();
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&lastSysCpu, &fsys, sizeof(FILETIME));
    memcpy(&lastUserCpu, &fuser, sizeof(FILETIME));
}

WindowsSystemInfo::~WindowsSystemInfo() {
    PdhCloseQuery(cpuQuery);
}

int64 WindowsSystemInfo::GetTotalVirtualMemory() {
    GET_MEMORY_STATUS
    return memInfo.ullTotalPageFile;
}

int64 WindowsSystemInfo::GetUsedVirtualMemory() {
    GET_PROCESS_MEMORY_COUNTERS
    return pmc.PrivateUsage;
}

int64 WindowsSystemInfo::GetTotalPhysicalMemory() {
    GET_MEMORY_STATUS
    return memInfo.ullTotalPhys;
}

int64 WindowsSystemInfo::GetUsedPhysicalMemory() {
    GET_PROCESS_MEMORY_COUNTERS
    return pmc.WorkingSetSize;
}

double WindowsSystemInfo::GetCpuUsage() {
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    double percent;
    
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));
    
    GetProcessTimes(self, &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));
    
    percent = 
        (double) (sys.QuadPart - lastSysCpu.QuadPart) +
        (double) (user.QuadPart - lastUserCpu.QuadPart);

    ULONGLONG diff = now.QuadPart - lastCpu.QuadPart;
    diff = max(1, diff);

    percent /= diff;
    percent /= processorCount;
    
    lastCpu = now;
    lastUserCpu = user;
    lastSysCpu = sys;
    
    return (percent * 100.0f);
}
#endif