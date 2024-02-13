/*
  TODO:
  -  Fix hot reload with Sokol
  -  Fixed point implementation
  -  Image loading
  -  TTF parsing & font baking
  -  Steamworks API (need to compile separate TU)
*/

// Headers

#include "base/base_include.h"
#include "game.h"

// Source
#define SOKOL_IMPL
#define SOKOL_GLCORE33
#include <sokol/sokol_gfx.h>
#include <sokol/sokol_gp.h>

#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

#define NK_IMPLEMENTATION
#define NK_PRIVATE
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_DEFAULT_FONT
#define NK_ASSERT Assert
#include <nuklear.h>

#define HANDMADE_MATH_NO_SSE
#include <HandmadeMath.h>

#include "base/base_include.c"

#define GAME_DATA_SIZE Kilobytes(4)
typedef struct Game Game;
struct Game
{
  Arena *permArena;
  Arena *frameArena;
};

StaticAssert(sizeof(Game) <= GAME_DATA_SIZE, check_game_size);
extern void
GameInit(GameMemory memory)
{
  Assert(memory.mem && GAME_DATA_SIZE < memory.size);
  Game *game = (Game*)memory.mem;

  // Memory management
  u64 arenaSize = memory.size / 2; // TODO: Should we divide this differently?
  void *permArenaMemory = (u8*)memory.mem + GAME_DATA_SIZE;
  void *frameArenaMemory = (u8*)permArenaMemory + arenaSize;
  game->permArena = ArenaAlloc(permArenaMemory, arenaSize);
  game->frameArena = ArenaAlloc(frameArenaMemory, arenaSize);

  // Init sokol_gfx
  sg_desc sgDesc = {0};
  sgp_desc sgpDesc = {0};
  sg_setup(&sgDesc);
  sgp_setup(&sgpDesc);
}

extern void
GameTick(GamePayload *payload)
{
  Game *gameState = (Game*)payload->memory.mem;

  GameInput input = payload->input;
  GameInputSource *keyboard = &input.sources[0];
  Unused(gameState && keyboard);

  int width = 800, height = 600; // TODO: Recieve this from platform layer
  f32 ratio = width / (f32)height;

  // Initialize
  sgp_begin(width, height);
  sgp_viewport(0, 0, width, height);
  sgp_project(-ratio, ratio, 1.f, -1.f);

  // Clear frame buffer
  sgp_set_color(0.1f, 0.1f, 0.1f, 1.f);
  sgp_clear();

  // Draw
  sgp_set_color(1.f, 1.f, 1.f, 1.f);
  sgp_draw_filled_rect(-0.5f, -0.5f, 1.f, 1.f);

  // Present
  sg_pass_action pass = {0};
  sg_begin_default_pass(&pass, width, height);
  sgp_flush();
  sgp_end();
  sg_end_pass();
  sg_commit();
}