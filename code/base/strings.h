#ifndef STRINGS_H
#define STRINGS_H
// This would not be possible without Allen 4th and Ryan Fleury

// String types

typedef struct String8 String8;
struct String8
{
  u8 *str;
  u64 size;
};

// String lists and arrays

typedef struct String8Node String8Node;
struct String8Node
{
  String8Node *next;
  String8 string;
};

typedef struct String8List String8List;
struct String8List
{
  String8Node *first;
  String8Node *last;
  u64 count;
  u64 totalSize;
};

typedef struct String8Array String8Array;
struct String8Array
{
  u64 count;
  String8 *strings;
};

// String op types

typedef u32 StringMatchFlags;
enum
{
  StringMatch_CaseInsensitive = (1 << 0),
};

typedef struct String8Join String8Join;
struct String8Join
{
  String8 pre, sep, post;
};

// ASCII helpers

function inline b32
CharIsAlphaUpper(u8 c)
{
  return c >= 'A' && c <= 'Z';
}

function inline b32
CharIsAlphaLower(u8 c)
{
  return c >= 'a' && c <= 'z';
}

function inline b32
CharIsAlpha(u8 c)
{
  return CharIsAlphaUpper(c) || CharIsAlphaLower(c);
}

function inline b32
CharIsNumeric(u8 c)
{
  return c >= '0' && c <= '9';
}

function inline b32
CharIsSymbol(u8 c)
{
  return (c >= '!' && c <= '/') ||
    (c >= ':' && c <= '@') ||
    (c >= '[' && c <= '`') ||
    (c >= '{' && c <= '~');
}

function inline b32
CharIsSpace(u8 c)
{
  return (c == ' ') || (c >= '\a' && c <= '\r');
}

function inline u8
CharToUpper(u8 c)
{
    return CharIsAlphaLower(c) ? ('A' + (c - 'a')) : c;
}

function inline u8
CharToLower(u8 c)
{
    return CharIsAlphaUpper(c) ? ('a' + (c - 'A')) : c;
}

function inline u8
CharToForwardSlash(u8 c)
{
    return c == '\\' ? '/' : c;
}

function u64 CStringLength(const char *str);

// String functions

// For use in printf-like functions
#define Str8Expand(s) (int)(s).size, (s).str

// Constructors
function String8 Str8(u8 *str, u64 size);
#define Str8C(cstr) Str8((u8*)(cstr), CStringLength(cstr))
#define Str8Lit(s) Str8((u8*)(s), sizeof(s)-1)
function String8 Str8Range(u8 *first, u8 *opl);

// Substrings
function String8 Substr8Opl(String8 string, u64 first, u64 opl);
function String8 Substr8Size(String8 string, u64 first, u64 size);
function String8 Str8Skip(String8 string, u64 min);
function String8 Str8Chop(String8 string, u64 nmax);
function String8 Prefix8(String8 string, u64 size);
function String8 Suffix8(String8 string, u64 size);

// Matching
function b32 Str8Match(String8 a, String8 b, StringMatchFlags matchFlags);

// Allocation
function String8 PushStr8Copy(Arena *arena, String8 string);
function String8 PushStr8FV(Arena *arena, char *fmt, va_list args);
function String8 PushStr8F(Arena *arena, char *fmt, ...);
#define Str8VArg(s) (int)(s).size, (s).str

// Lists
function void Str8ListPushNode(String8List *list, String8Node *node);
function void Str8ListPushNodeFront(String8List *list, String8Node *node);
function void Str8ListPush(Arena *arena, String8List *list, String8 string);
function void Str8ListPushFront(Arena *arena, String8List *list, String8 string);
function void Str8ListPushF(Arena *arena, String8List *list, char *fmt, ...);
function void Str8ListConcat(String8List *list, String8List *target);
function String8List Str8Split(Arena *arena, String8 string, u64 numSplits, u8 *splits);
function String8 Str8ListJoin(Arena *arena, String8List *list, String8Join *optParams);

// Styling
function String8 Str8Upper(Arena *arena, String8 string);
function String8 Str8Lower(Arena *arena, String8 string);

#endif