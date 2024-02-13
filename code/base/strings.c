function u64
CStringLength(const char *str)
{
  u64 length;
  for (length = 0; str[length]; ++length);
  return length;
}

function String8
Str8(u8 *str, u64 size)
{
  return (String8){str, size};
}

function String8
Str8Range(u8 *first, u8 *opl)
{
  return (String8){first, (u64)(opl - first)};
}

function String8
Substr8Opl(String8 string, u64 first, u64 opl)
{
  u64 oplClamped = Min(opl, string.size);
  u64 firstClamped = Min(first, oplClamped);
  return (String8){string.str + firstClamped, oplClamped - firstClamped};
}

function String8
Substr8Size(String8 string, u64 first, u64 size)
{
  return Substr8Opl(string, first, first + size);
}

function String8
Str8Skip(String8 string, u64 min)
{
  return Substr8Opl(string, min, string.size);
}

function String8
Str8Chop(String8 string, u64 nmax)
{
  return Substr8Opl(string, 0, string.size - nmax);
}

function String8
Prefix8(String8 string, u64 size)
{
  return Substr8Size(string, 0, size);
}

function String8
Suffix8(String8 string, u64 size)
{
  return Substr8Opl(string, string.size - size, string.size);
}

function b32
Str8Match(String8 a, String8 b, StringMatchFlags matchFlags)
{
  b32 result = 0;
  if (a.size == b.size) {
    result = 1;
    b32 noCase = ((matchFlags & StringMatch_CaseInsensitive) != 0);
    for (u64 i = 0; i < a.size; ++i) {
      u8 ac = a.str[i];
      u8 bc = b.str[i];
      if (noCase) {
        ac = CharToLower(ac);
        bc = CharToLower(bc);
      }
      if (ac != bc) {
        result = 0;
        break;
      }
    }
  }

  return result;
}

function String8
PushStr8Copy(Arena *arena, String8 string)
{
  String8 result;
  result.size = string.size;
  result.str = ArenaPushN(arena, u8, string.size + 1);
  MemoryCopy(result.str, string.str, string.size);
  return result;
}

function String8
PushStr8FV(Arena *arena, char *fmt, va_list args)
{
  String8 result = {0};

  va_list argsCpy;
  va_copy(argsCpy, args);

  int neededBytes = stbsp_vsnprintf(0, 0, fmt, args) + 1;
  result.str = ArenaPushN(arena, u8, neededBytes);
  result.size = neededBytes - 1;
  stbsp_vsnprintf((char*)result.str, neededBytes, fmt, argsCpy);

  va_end(argsCpy);

  return result;
}

function String8
PushStr8F(Arena *arena, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  String8 result = PushStr8FV(arena, fmt, args);
  va_end(args);

  return result;
}

function void
Str8ListPushNode(String8List *list, String8Node *node)
{
  SLLQueuePush(list->first, list->last, node);
  list->count++;
  list->totalSize += node->string.size;
}

function void
Str8ListPushNodeFront(String8List *list, String8Node *node)
{
  SLLQueuePushFront(list->first, list->last, node);
  list->count++;
  list->totalSize += node->string.size;
}

function void
Str8ListPush(Arena *arena, String8List *list, String8 string)
{
  String8Node *node = ArenaPushN(arena, String8Node, 1);
  node->string = string;
  Str8ListPushNode(list, node);
}

function void
Str8ListPushFront(Arena *arena, String8List *list, String8 string)
{
  String8Node *node = ArenaPushN(arena, String8Node, 1);
  node->string = string;
  Str8ListPushNodeFront(list, node);
}

function void
Str8ListPushF(Arena *arena, String8List *list, char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  String8 string = PushStr8FV(arena, fmt, args);
  va_end(args);
  Str8ListPush(arena, list, string);
}

function void
Str8ListConcat(String8List *list, String8List *target)
{
  if (list->first) {
    target->count += list->count;
    target->totalSize += list->totalSize;
    if (target->last == 0) {
      *target = *list;
    } else {
      target->last->next = list->first;
      target->last = list->last;
    }
  }

  MemoryZero(list, sizeof(String8List));
}

function String8List
Str8Split(Arena *arena, String8 string, u64 numSplits, u8 *splits)
{
  String8List result = {0};

  u8 *ptr = string.str;
  u8 *first = ptr;
  u8 *opl = ptr + string.size;
  for (;ptr < opl; ++ptr) {
    u8 byte = *ptr;
    b32 split = 0;
    for (u64 i = 0; i < numSplits; ++i) {
      if (byte == splits[i]) {
        split = 1;
        break;
      }
    }

    if (split) {
      if (first < ptr)
        Str8ListPush(arena, &result, Str8Range(first, ptr));
      first = ptr + 1;
    }
  }

  if (first < ptr) {
    Str8ListPush(arena, &result, Str8Range(first, ptr));
  }

  return result;
}

function String8
Str8ListJoin(Arena *arena, String8List *list, String8Join *optParams)
{
  String8Join join = {0};
  if (optParams)
    join = *optParams;

  u64 size = join.pre.size + join.post.size + (join.sep.size * (list->count - 1)) + list->totalSize;
  u8 *str = ArenaPushN(arena, u8, size + 1);
  u8 *ptr = str;

  // Pre
  MemoryCopy(ptr, join.pre.str, join.pre.size);
  ptr += join.pre.size;

  // Sep
  for (String8Node *node = list->first; node; node = node->next) {
    MemoryCopy(ptr, node->string.str, node->string.size);
    ptr += node->string.size;
    if (node != list->last) {
      MemoryCopy(ptr, join.sep.str, join.sep.size);
      ptr += join.sep.size;
    }
  }

  // Post
  MemoryCopy(ptr, join.post.str, join.post.size);
  ptr += join.post.size;

  return Str8(str, size);
}

function String8
Str8Upper(Arena *arena, String8 string)
{
  String8 result = PushStr8Copy(arena, string);
  for (u64 i = 0; i < string.size; ++i) {
    result.str[i] = CharToUpper(result.str[i]);
  }

  return result;
}

function String8
Str8Lower(Arena *arena, String8 string)
{
  String8 result = PushStr8Copy(arena, string);
  for (u64 i = 0; i < string.size; ++i) {
    result.str[i] = CharToLower(result.str[i]);
  }

  return result;
}