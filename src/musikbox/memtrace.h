// Adepted from http://www.flipcode.com/archives/How_To_Find_Memory_Leaks.shtml

#include <list>
#include <sstream>

using namespace std;
typedef struct 
{
    DWORD   address;
    DWORD   size;
    tchar   file[64];
    DWORD   line;
} ALLOC_INFO;

typedef std::list<ALLOC_INFO*> AllocList;

AllocList   *allocList;

void AddTrack(DWORD addr, DWORD asize, const tchar* fname, DWORD lnum)
{
    ALLOC_INFO *info;

    if(!allocList) 
    {
        allocList = new(AllocList);
    }

    info = new(ALLOC_INFO);
    info->address = addr;

    wcsncpy(info->file, fname, 63);
    
    info->line = lnum;
    info->size = asize;
    allocList->insert(allocList->begin(), info);
};

void RemoveTrack(DWORD addr)
{
    AllocList::iterator i;

    if(!allocList)
        return;

    for(i = allocList->begin(); i != allocList->end(); i++)
    {
        if((*i)->address == addr)
        {
            allocList->remove((*i));
            break;
        }
    }
};

void DumpUnfreed()
{
    AllocList::iterator i;
    DWORD totalSize = 0;

    tostringstream os;

    if(!allocList)
        return;

    for(i = allocList->begin(); i != allocList->end(); i++) 
    {
        os << (*i)->address << "\t\tLINE " << (*i)->line << "\t\tADDRESS " << (*i)->address << "\t" << (*i)->size << "unfreed\n";
        OutputDebugString(os.str().c_str());

        totalSize += (*i)->size;
    }
    
    OutputDebugString(_T("-------------------------------------------------\n"));
    
    os.str(tstring());
    os << "Total Unfreed: " << totalSize << " bytes\n";
};
            
#ifdef _DEBUG
inline void * __cdecl operator new(unsigned int size, const tchar *file, int line)
{
    void *ptr = (void *)malloc(size);
    AddTrack((DWORD)ptr, size, file, line);
    return(ptr);
};

inline void __cdecl operator delete(void *p)
{
    RemoveTrack((DWORD)p);
    free(p);
};

inline void * __cdecl operator new[](unsigned int size, const tchar *file, int line)
{
    void *ptr = (void *)malloc(size);
    AddTrack((DWORD)ptr, size, file, line);
    return(ptr);
};

inline void __cdecl operator delete[](void *p)
{
    RemoveTrack((DWORD)p);
    free(p);
};
#endif

#ifdef _DEBUG
#define DEBUG_NEW new(__FILE__, __LINE__)
#else
#define DEBUG_NEW new
#endif
#define new DEBUG_NEW