#ifndef OS_H
#define OS_H

// For the stuff that SDL does not provide

// IO functions
function void DebugPrint(String8 msg);
function void OSCopyFile(char *src, char *dest);
function u64  OSGetLastWriteTime(String8 path);

// Virtual memory functions
function void *OSMemReserve(u64 size);
function void  OSMemCommit(void *ptr, u64 size);
function void  OSMemDecommit(void *ptr, u64 size);
function void  OSMemRelease(void *ptr, u64 size);

#endif