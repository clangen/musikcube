#include "stdafx.h"

class SystemInfo {
    public:
        static SystemInfo* Create();

        virtual ~SystemInfo();

    protected:
        SystemInfo();

    public:
       virtual int64 GetTotalVirtualMemory();
       virtual int64 GetUsedVirtualMemory();
       virtual int64 GetTotalPhysicalMemory();
       virtual int64 GetUsedPhysicalMemory();
       virtual double GetCpuUsage();
};