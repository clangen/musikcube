#include "stdafx.h"

namespace musik {
    namespace box {
        class SystemInfo {
            public:
                static SystemInfo* Create();
                virtual ~SystemInfo();

                virtual int64 GetTotalVirtualMemory();
                virtual int64 GetUsedVirtualMemory();
                virtual int64 GetTotalPhysicalMemory();
                virtual int64 GetUsedPhysicalMemory();
                virtual double GetCpuUsage();

            protected:
                SystemInfo();
        };
    }
}
