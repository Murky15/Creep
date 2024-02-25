#ifndef COMMON_H
#define COMMON_H

#if NO_CRT && _DEBUG
int _fltused;

void *
memset(void *dest, register int val, register size_t len)
{
  register unsigned char *ptr = (unsigned char*)dest;
  while (len-- > 0)
    *ptr++ = val;
  return dest;
}

void *
memcpy(void *dest, const void *src, size_t len)
{
  char *d = dest;
  const char *s = src;
  while (len--)
    *d++ = *s++;
  return dest;
}

#else
# include <string.h>
#endif

// Basic types
#include <stdint.h>
#include <stdbool.h>
typedef int8_t s8, b8;
typedef int16_t s16, b16;
typedef int32_t s32, b32;
typedef int64_t s64, b64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef float f32;
typedef double f64;

// Keywords
#define function static
#define global static
#define local static
#define persist static

#define fallthrough

#define Bytes(n)      (n)
#define Kilobytes(n)  (n << 10)
#define Megabytes(n)  (n << 20)
#define Gigabytes(n)  (((u64)n) << 30)
#define Terabytes(n)  (((u64)n) << 40)

// Helper macros
#define Stmnt(s) do { s; } while(0)

#define Swap(type, a, b) Stmnt( type temp = a; a = b; b = temp; )
#define Unused(var) (void)(var)

#if COMPILER_MSVC
# pragma section(".roglob", read)
# define read_only static __declspec(allocate(".roglob"))
#else
# error "Read only data not implemented for this compiler!"
#endif

#if COMPILER_MSVC
# define thread_storage static __declspec(thread)
#else
# error "Thread local storage not implemented for this compiler!"
#endif

#if _DEBUG
# define Assert(c) if (!(c)) {*(int*)0 = 0;}
# define StaticAssert(c, label) u8 static_assert_##label[(c)?(1):(-1)]
#else
# define Assert(c)
# define StaticAssert(c)
#endif

// Memory management helpers

#define MemoryCopy memcpy
#define MemoryMove memmove
#define MemorySet memset
#define MemoryCompare memcmp

// StaticAssert()?
#define MemoryCopyStruct(dest, src) Stmnt( Assert(sizeof(*(dest)) == sizeof(*(src))); MemoryCopy((dest), (src), sizeof(*(dest))); )
#define MemoryCopyArray(dest, src) Stmnt( Assert(sizeof((dest)) == sizeof((src))); MemoryCopy((dest), (src), sizeof((dest))); )

#define MemoryMatch(a, b, z) (MemoryCompare((a), (b), (z)) == 0)

#define MemoryZero(ptr, size) MemorySet((ptr), 0, (size))
#define MemoryZeroStruct(ptr) MemorySet((ptr), 0, sizeof(*(ptr)))
#define MemoryZeroArray(arr) MemorySet((arr), 0, sizeof((arr)))

// Linked lists

#define DLLPushBack_NP(f, l, n, next, prev) \
((f)==0?\
((f)=(l)=(n),(n)->next=(n)->prev=0):\
((n)->prev=(l),(l)->next=(n),(l)=(n),(n)->next=0))

#define DLLRemove_NP(f, l, n, next, prev) \
((f)==(n)?\
((f)==(l)?\
((f)=(l)=(0)):\
((f)=(f)->next,(f)->prev=0)):\
(l)==(n)?\
((l)=(l)->prev,(l)->next=0):\
((n)->next->prev=(n)->prev,\
(n)->prev->next=(n)->next))

#define DLLPushBack(f, l, n) DLLPushBack_NP(f, l, n, next, prev)
#define DLLPushFront(f, l, n) DLLPushBack_NP(f, l, n, prev, next)
#define DLLRemove(f, l, n) DLLRemove_NP(f, l, n, next, prev)

#define SLLQueuePush_N(f, l, n, next) \
((f)==0?\
(f)=(l)=(n):\
((l)->next=(n),(l)=(n)),\
(n)->next=0)

#define SLLQueuePushFront_N(f, l, n, next) \
((f)==0?\
((f)=(l)=(n),(n)->next=0):\
((n)->next=(f),(f)=(n)))

#define SLLQueuePop_N(f, l, next) \
(((f)==(l))?\
((f)=(l)=0):\
((f)=(f)->next))

#define SLLQueuePush(f, l, n) SLLQueuePush_N(f, l, n, next)
#define SLLQueuePushFront(f, l, n) SLLQueuePushFront_N(f, l, n, next)
#define SLLQueuePop(f, l) SLLQueuePop_N(f, l, next)

#define SLLStackPush_N(f, n, next) \
((n)->next=(f),(f)=(n))

#define SLLStackPop_N(f, next) \
((f)==0?0:\
((f)=(f)->next))

#define SLLStackPush(f, n) SLLStackPush_N(f, n, next)
#define SLLStackPop(f) SLLStackPop_N(f, next)

// Arithmetic

#define Min(a, b) (((a)<(b)) ? (a) : (b))
#define Max(a, b) (((a)>(b)) ? (a) : (b))
#define ClampTop(x, a) Min(x,a)
#define ClampBot(a, x) Max(a,x)
#define Clamp(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

#define ArrayCount(a) (sizeof((a)) / sizeof((a)[0]))
#define IntFromPtr(p) (u64)(((u8 *)p) - 0)
#define PtrFromInt(i) (void *)(((u8 *)0) + i)
#define Member(type, member_name) ((type *)0)->member_name
#define OffsetOf(type, member_name) IntFromPtr(&Member(type, member_name))
#define BaseFromMember(type, member_name, ptr) (type *)((u8 *)(ptr) - OffsetOf(type, member_name))

#define IsPow2(x) (((x)&((x) - 1)) == 0)
#define AlignUpPow2(x, p) (((x) + (p) - 1)&~((p) - 1))
#define AlignDownPow2(x, p) ((x)&~((p) - 1))

#endif // COMMON_H