#include <windows.h>

function void
DebugPrint(String8 msg)
{
  OutputDebugString((LPCSTR)msg.str);
}

function void 
OSCopyFile(char *src, char *dest)
{
  CopyFile(src, dest, false);
}

function u64
OSGetLastWriteTime(String8 path)
{
  ULARGE_INTEGER result = {0};

  WIN32_FIND_DATA fileData;
  HANDLE fileHandle = FindFirstFile((LPCSTR)path.str, &fileData);
  if (fileHandle != INVALID_HANDLE_VALUE) {
    FILETIME lastWriteFileTime = fileData.ftLastWriteTime;
    result.u.LowPart = lastWriteFileTime.dwLowDateTime;
    result.u.HighPart = lastWriteFileTime.dwHighDateTime;
    FindClose(fileHandle);
  }
  
  return (u64)result.QuadPart;
}

function void*
OSMemReserve(u64 size)
{
  return VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
}

function void
OSMemCommit(void *ptr, u64 size)
{
  VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

function void 
OSMemDecommit(void *ptr, u64 size)
{
  VirtualFree(ptr, size, MEM_DECOMMIT);
}

function void 
OSMemRelease(void *ptr, u64 size)
{
  VirtualFree(ptr, size, MEM_RELEASE);
}
