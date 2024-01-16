function Arena* 
ArenaAlloc(void *backingBuffer, u64 backingBufferSize)
{
  Assert(IsPow2(backingBufferSize));
  Assert(sizeof(Arena) <= backingBufferSize);
  
  Arena *arena = (Arena*)backingBuffer;
  arena->pos = sizeof(Arena);
  arena->cap = backingBufferSize;
  
  return arena;
}

function void*  
ArenaPushNoZero(Arena *arena, u64 size, u64 align)
{
  Assert(IsPow2(align));

  void *result = 0;
  if (arena->pos + size <= arena->cap) {
    u8 *base = (u8*)arena;
    u64 alignedPos = AlignUpPow2(arena->pos, align);
    result = base + alignedPos;
    arena->pos = alignedPos + size;
  } else {
    // TODO: Logging
  }
  
  return result;
}

function void*  
ArenaPush(Arena *arena, u64 size, u64 align)
{
  void *result = ArenaPushNoZero(arena, size, align);
  MemoryZero(result, size);
  
  return result;
}

function void   
ArenaPopTo(Arena *arena, u64 pos)
{
  u64 minPos = sizeof(Arena);
  u64 newPos = Max(minPos, pos);
  arena->pos = newPos;
}

function void
ArenaPop(Arena *arena, u64 amount)
{
  u64 amountClamped = Min(amount, arena->pos);
  u64 newPos = arena->pos - amountClamped;
  ArenaPopTo(arena, newPos);
}

function void   
ArenaClear(Arena *arena)
{
  ArenaPopTo(arena, 0);
}

function u64    
PosFromArena(Arena *arena)
{
  return arena->pos;
}

function TempArena 
ArenaTempBegin(Arena *arena)
{
  TempArena result = {0};
  result.arena = arena;
  result.pos = arena->pos;
  
  return result;
}

function void 
ArenaTempEnd(TempArena temp)
{
  ArenaPopTo(temp.arena, temp.pos);
}
