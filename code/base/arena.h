#ifndef ARENA_H
#define ARENA_H

/* 
  TODO: 
  - Arena growth
  - Scratch arenas
*/

#define ARENA_COMMIT_GRANULARITY Kilobytes(4)
#define ARENA_DECOMMIT_THRESHOLD Megabytes(64)

typedef struct Arena Arena;
struct Arena
{
  u64 pos;
  u64 cap;
};

typedef struct TempArena TempArena;
struct TempArena
{
  Arena *arena;
  u64 pos;
};

// Arena functions

function Arena* ArenaAlloc(void *backingBuffer, u64 backingBufferSize);
function void   ArenaRelease(Arena *arena);
function void*  ArenaPushNoZero(Arena *arena, u64 size, u64 align);
function void*  ArenaPush(Arena *arena, u64 size, u64 align);
#define ArenaPushN(arena, type, count) ArenaPush((arena), sizeof(type) * (count), _Alignof(type))
function void   ArenaPopTo(Arena *arena, u64 pos);
function void   ArenaPop(Arena *arena, u64 amount);
function void   ArenaClear(Arena *arena);
function u64    PosFromArena(Arena *arena);

// Temp arena functions

function TempArena ArenaTempBegin(Arena *arena);
function void      ArenaTempEnd(TempArena temp);

#endif