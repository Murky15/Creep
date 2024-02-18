/*
  TODO:
  -  We have a lot of uneeded overhead from SDL. Create individual platform layers
  -  Multithreading (https://www.rfleury.com/p/main-loops-refresh-rates-and-determinism)
  -  Control remappings
*/

// Unity build

// Headers
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_opengl.h>

#include "base/base_include.h"
// #include "os/os.h"
#include "game.h"

// Source
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>
#include "base/base_include.c"

#if OS_WINDOWS
# include "os/os_windows.c"
#elif OS_MAC || OS_LINUX
# error "Missing implementation for Unix-like systems!"
#endif // OS detection

// Globals
global u64 g_perfFrequency;

typedef struct GameHandle GameHandle;
struct GameHandle
{
  b32 valid;

  u64 dllLastWriteTime;

  void *handle;
  #define X(ret, name, ...) Game##name##Func *name;
  GAME_VTABLE
  #undef X
};

typedef struct PlatformState PlatformState;
struct PlatformState
{
  b32 terminated;

  // Memory management
  Arena *permanentArena;
  Arena *frameArena;

  // Misc
  String8 basePath;

  // SDL handles
  SDL_Window   *window;
  SDL_GLContext glContext;
};

function u64
GetWallClock(void)
{
  return SDL_GetPerformanceCounter();
}

function f32
GetSecondsElapsed(u64 oldCounter, u64 newCounter)
{
  return (f32)(newCounter - oldCounter) / (f32)g_perfFrequency;
}

function int
GetWindowRefresh(SDL_Window *window)
{
  SDL_DisplayMode dpyMode = {0};
  int dpyIndex = SDL_GetWindowDisplayIndex(window);
  SDL_GetDesktopDisplayMode(dpyIndex, &dpyMode);
  int result = dpyMode.refresh_rate == 0 ? 60 : dpyMode.refresh_rate; // 60 is default

  return result;
}

function void
ProcessInput(GameButtonState *oldState, GameButtonState *newState, b32 isDown)
{
  newState->isDown = isDown;
  newState->halfTransitionCount = (oldState->isDown != newState->isDown) ? 1 : 0;
}

function PlatformState
PlatformInit(void)
{
  read_only char *windowTitle = "Game";
  read_only u32 windowWidth = 800, windowHeight = 600;

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {
    SDL_Window *window = SDL_CreateWindow(
      windowTitle,
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      windowWidth, windowHeight,
      SDL_WINDOW_OPENGL);
    if (window != 0) {
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
      SDL_GLContext glContext = SDL_GL_CreateContext(window);
      SDL_GL_SetSwapInterval(0);

      u64 platformMemorySize = Gigabytes(1);
      void *platformMemory = OSMemReserve(platformMemorySize);
      OSMemCommit(platformMemory, platformMemorySize);

      u64 arenaSize = platformMemorySize / 2;
      void *permArenaMemory = platformMemory;
      void *frameArenaMemory = (u8*)platformMemory + arenaSize;

      char *basePath = SDL_GetBasePath();
      return (PlatformState) {
        .permanentArena = ArenaAlloc(permArenaMemory, arenaSize),
        .frameArena = ArenaAlloc(frameArenaMemory, arenaSize),

        .basePath = Str8C(basePath),

        .window = window,
        .glContext = glContext,
        };
    }
  }

  return (PlatformState){0};
}

function GameHandle
GetGameHandle(String8 source, String8 temp)
{
  GameHandle result = {0};

  result.dllLastWriteTime = OSGetLastWriteTime(source);

  OSCopyFile((char*)source.str, (char*)temp.str);
  result.handle = SDL_LoadObject((char*)temp.str);
  if (result.handle) {
    #define X(ret, name, ...) \
    result.name = (Game##name##Func*)SDL_LoadFunction(result.handle, #name); \
    result.valid = (result.name != 0);
    GAME_VTABLE
    #undef X
  }
  DebugPrint(Str8Lit("Loaded game code!\n"));

  return result;
}

function void
ReleaseGameHandle(GameHandle *handle)
{
  if (handle->valid) {
    SDL_UnloadObject(handle->handle);
    *handle = (GameHandle){0};
  }
}

int
main(void)
{
  PlatformState app = PlatformInit();
  if (!app.window) {
    // TODO: Logging
    return 1;
  }

  // Init globals
  g_perfFrequency = SDL_GetPerformanceFrequency();

  // Determine game path
  String8 srcPathString = {0};
  String8 tmpPathString = {0};
  {
    TempArena temp = ArenaTempBegin(app.permanentArena);
    String8List srcPath = {0};
    String8List tmpPath = {0};
    Str8ListPush(temp.arena, &srcPath, app.basePath);
    Str8ListPush(temp.arena, &srcPath, Str8Lit("game.dll"));
    Str8ListPush(temp.arena, &tmpPath, app.basePath);
    Str8ListPush(temp.arena, &tmpPath, Str8Lit("game_temp.dll"));
    srcPathString = Str8ListJoin(app.permanentArena, &srcPath, 0);
    tmpPathString = Str8ListJoin(app.permanentArena, &tmpPath, 0);
    ArenaTempEnd(temp);
  }

  GameMemory gameMemory = {0};
  gameMemory.size = Gigabytes(4); // TODO: How much memory do we need?
  gameMemory.mem = OSMemReserve(gameMemory.size);
  OSMemCommit(gameMemory.mem, gameMemory.size);

  GameInput input[2] = {0};
  GameInput *newInput = &input[0];
  GameInput *oldInput = &input[1];
  GameInputSource *keyboard = &newInput->sources[0];
  GameInputSource *oldKeyboard = &oldInput->sources[0];

  GamePayload payload;
  payload.memory = gameMemory;

  GameHandle game = GetGameHandle(srcPathString, tmpPathString);
  if (!game.valid) {
    DebugPrint(Str8Lit("Unable to load game code!\n"));
    return 1;
  }
  game.Load(true, gameMemory);

  for (;!app.terminated;) {
    u64 newWriteTime = OSGetLastWriteTime(srcPathString);
    if (newWriteTime != game.dllLastWriteTime) {
      ReleaseGameHandle(&game);
      game = GetGameHandle(srcPathString, tmpPathString);
      game.Load(false, gameMemory);
    }

    // Poll events
    for (SDL_Event event; SDL_PollEvent(&event);) {
      switch (event.type) {

        case SDL_KEYUP: fallthrough
        case SDL_KEYDOWN: {
          SDL_KeyboardEvent *keyEvent = (SDL_KeyboardEvent*)&event;
          if (!keyEvent->repeat) {
            b32 isDown = (keyEvent->state == SDL_PRESSED);
            switch (keyEvent->keysym.sym) {
              // TODO: Pretty hacky?
              case SDLK_UP: fallthrough
              case SDLK_w: if (isDown) ++keyboard->yAxis; else --keyboard->yAxis; break;
              case SDLK_LEFT: fallthrough
              case SDLK_a: if (isDown) --keyboard->xAxis; else ++keyboard->xAxis; break;
              case SDLK_DOWN: fallthrough
              case SDLK_s: if (isDown) --keyboard->yAxis; else ++keyboard->yAxis; break;
              case SDLK_RIGHT: fallthrough
              case SDLK_d: if (isDown) ++keyboard->xAxis; else --keyboard->xAxis; break;

              case SDLK_q: ProcessInput(&oldKeyboard->primary, &keyboard->primary, isDown); break;
              case SDLK_e: ProcessInput(&oldKeyboard->secondary, &keyboard->secondary, isDown); break;
              case SDLK_r: ProcessInput(&oldKeyboard->tertiary, &keyboard->tertiary, isDown); break;
              case SDLK_t: ProcessInput(&oldKeyboard->quaternary, &keyboard->quaternary, isDown); break;
            }
          }
        } break;

        case SDL_QUIT: {
          app.terminated = true;
        } break;
      }
    }

    payload.input = *newInput;
    game.Update(&payload);
    game.Render(&payload);

    // Present renderer
    SDL_GL_SwapWindow(app.window);

    Swap(GameInput*, newInput, oldInput);
    ArenaClear(app.frameArena);
  }
  SDL_Quit();
  return 0;
}