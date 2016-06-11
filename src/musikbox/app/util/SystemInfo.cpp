//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2007-2016 musikcube team
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the author nor the names of other contributors may
//      be used to endorse or promote products derived from this software
//      without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "SystemInfo.h"

using namespace musik::box;

#ifdef WIN32

#include "windows.h"
#include "psapi.h"
#include "pdh.h"

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
    diff = std::max((ULONGLONG) 1, diff);

    percent /= diff;
    percent /= processorCount;

    lastCpu = now;
    lastUserCpu = user;
    lastSysCpu = sys;

    return (percent * 100.0f);
}
#endif
