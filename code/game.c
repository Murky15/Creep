/*
  TODO:
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
#define SOKOL_DEBUG
#define SOKOL_GLCORE33
#include <sokol/sokol_log.h>
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
  // Memory management
  Arena *permArena;
  Arena *frameArena;

  // Sokol handles
  _sg_state_t *sgState;
  _sgp_context *sgpState;
};

StaticAssert(sizeof(Game) <= GAME_DATA_SIZE, check_game_size);
extern void
GameLoaded(b32 first, GameMemory memory)
{
  Assert(memory.mem && GAME_DATA_SIZE < memory.size);
  Game *game = (Game*)memory.mem;

  if (first) {
    // Memory management
    u64 arenaSize = memory.size / 2; // TODO: Should we divide this differently?
    void *permArenaMemory = (u8*)memory.mem + GAME_DATA_SIZE;
    void *frameArenaMemory = (u8*)permArenaMemory + arenaSize;
    game->permArena = ArenaAlloc(permArenaMemory, arenaSize);
    game->frameArena = ArenaAlloc(frameArenaMemory, arenaSize);

    game->sgState = ArenaPushN(game->permArena, _sg_state_t, 1);
    game->sgpState = ArenaPushN(game->permArena, _sgp_context, 1);
    // Super hacky
    _sg = game->sgState;
    _sgp = game->sgpState;

    // Init sokol_gfx
    sg_desc sgDesc = {0};
    sgp_desc sgpDesc = {0};
    sgDesc.logger.func = slog_func;
    sg_setup(&sgDesc);
    sgp_setup(&sgpDesc);

  } else {
    _sg = game->sgState;
    _sgp = game->sgpState;
    // Refresh OpenGL context (it wouldn't be game development without crazy hacks)
    _sg_discard_backend();
    _sg_setup_backend(&_sg->desc);
  }
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
  Unused(ratio);

  // Initialize
  sgp_begin(width, height);
  sgp_viewport(0, 0, width, height);
  sgp_project(0, 500, 500, 0);

  // Clear frame buffer
  sgp_set_color(0.1f, 0.1f, 0.1f, 1.f);
  sgp_clear();

  // Draw
  sgp_set_color(1.f, 0.f, 0.f, 1.f);
  sgp_draw_filled_rect(0, 0, 100.f, 100.f);

  // Present
  sg_pass_action pass = {0};
  sg_begin_default_pass(&pass, width, height);
  sgp_flush();
  sgp_end();
  sg_end_pass();
  sg_commit();
}