/*
  TODO:
  -  Move software renderer to ISPC
  -  Control remappings
  -  Fix hot reloading issue in RADDBG
  -  Use SDL_Textures
*/

// Unity build
// Headers
#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "base/base_include.h"
#include "os/os.h"
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
  GameInitFunc *init;
  GameTickFunc *tick;
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
  Bitmap framebuffer;

  // SDL handles
  SDL_Window   *window;
  SDL_Renderer *renderer;
  SDL_Texture  *framebufferTexture;
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

  Bitmap framebuffer = {0};
  framebuffer.width = windowWidth;
  framebuffer.height = windowHeight;
  framebuffer.bytesPerPixel = 4;
  framebuffer.pitch = framebuffer.width * framebuffer.bytesPerPixel;
  u64 allocSize = framebuffer.pitch * framebuffer.height;
  framebuffer.pixels = OSMemReserve(allocSize);
  OSMemCommit(framebuffer.pixels, allocSize);

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == 0) {
    SDL_Window *window = SDL_CreateWindow(
      windowTitle,
      SDL_WINDOWPOS_UNDEFINED,
      SDL_WINDOWPOS_UNDEFINED,
      windowWidth, windowHeight,
      SDL_WINDOW_RESIZABLE);
    if (window != 0) {
      SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
      SDL_RenderSetLogicalSize(renderer, windowWidth, windowHeight);
      // SDL_RenderSetIntegerScale(renderer, true);

      SDL_Texture *framebufferTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_STREAMING, windowWidth, windowHeight);

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
        .framebuffer = framebuffer,

        .window = window,
        .renderer = renderer,
        .framebufferTexture = framebufferTexture
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
    result.init = (GameInitFunc*)SDL_LoadFunction(result.handle, "GameInit");
    result.tick = (GameTickFunc*)SDL_LoadFunction(result.handle, "GameTick");
    result.valid = (result.init && result.tick);
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
mainCRTStartup(void)
{
  PlatformState app = PlatformInit();
  if (!app.window) {
    // TODO: Logging
    return 1;
  }

  // Init globals
  g_perfFrequency = SDL_GetPerformanceFrequency();

  // Load game code
  String8 srcPathString = {0};
  String8 tmpPathString = {0};
  {
    String8List srcPath = {0};
    String8List tmpPath = {0};

    TempArena temp = ArenaTempBegin(app.permanentArena);
    Str8ListPush(temp.arena, &srcPath, app.basePath);
    Str8ListPush(temp.arena, &srcPath, Str8Lit("game.dll"));
    Str8ListPush(temp.arena, &tmpPath, app.basePath);
    Str8ListPush(temp.arena, &tmpPath, Str8Lit("game_temp.dll"));
    srcPathString = Str8ListJoin(app.permanentArena, &srcPath, 0);
    tmpPathString = Str8ListJoin(app.permanentArena, &tmpPath, 0);
    ArenaTempEnd(temp);
  }

  GameHandle game = GetGameHandle(srcPathString, tmpPathString);
  if (!game.valid) {
    DebugPrint(Str8Lit("Unable to load game code!\n"));
    return 1;
  }
  game.init();

  GameMemory gameMemory = {0};
  gameMemory.size = Gigabytes(3); // TODO: How much memory do we need?
  gameMemory.mem = OSMemReserve(gameMemory.size);
  OSMemCommit(gameMemory.mem, gameMemory.size);

  GameInput input[2] = {0};
  GameInput *newInput = &input[0];
  GameInput *oldInput = &input[1];
  GameInputSource *keyboard = &newInput->sources[0];
  GameInputSource *oldKeyboard = &oldInput->sources[0];

  // Profiling
  u64 lastCounter = GetWallClock();
  int gameUpdateHz = GetWindowRefresh(app.window) / 2;
  b32 shouldSleep = true; // TODO: Overwrite with our own sleep function to get higher granularity
  f32 targetSecondsPerFrame = 1.0f / (f32)gameUpdateHz;

  GamePayload payload;
  payload.memory = gameMemory;
  payload.framebuffer = app.framebuffer;
  for (;!app.terminated;) {

    u64 newWriteTime = OSGetLastWriteTime(srcPathString);
    if (newWriteTime != game.dllLastWriteTime) {
      ReleaseGameHandle(&game);
      game = GetGameHandle(srcPathString, tmpPathString);
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
              case SDLK_w: if (isDown) --keyboard->yAxis; else ++keyboard->yAxis; break;
              case SDLK_LEFT: fallthrough
              case SDLK_a: if (isDown) --keyboard->xAxis; else ++keyboard->xAxis; break;
              case SDLK_DOWN: fallthrough
              case SDLK_s: if (isDown) ++keyboard->yAxis; else --keyboard->yAxis; break;
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
    game.tick(&payload);

    // Should this be LockTexture/UnlockTexture instead?
    SDL_UpdateTexture(app.framebufferTexture, 0, app.framebuffer.pixels, app.framebuffer.pitch);
    SDL_RenderClear(app.renderer);
    SDL_RenderCopy(app.renderer, app.framebufferTexture, 0, 0);

    u64 workCounter = GetWallClock();
    f32 secondsElapsed = GetSecondsElapsed(lastCounter, workCounter);
    if (secondsElapsed < targetSecondsPerFrame) {
      while (secondsElapsed < targetSecondsPerFrame) {
        if (shouldSleep) {
          u32 sleepTime = (u32)(1000.0f * (targetSecondsPerFrame - secondsElapsed) - 1);
          if (sleepTime)
            SDL_Delay(sleepTime);
        }
        secondsElapsed = GetSecondsElapsed(lastCounter, GetWallClock());
      }
    }
    else {
      DebugPrint(Str8Lit("Missed frame rate!\n"));
    }

    SDL_RenderPresent(app.renderer);

    Swap(GameInput*, newInput, oldInput);

    u64 endCounter = GetWallClock();
    f32 msPerFrame = 1000.0f * GetSecondsElapsed(lastCounter, endCounter);
    lastCounter = endCounter;

    f32 FPS = 0.0f;
    Unused(msPerFrame && FPS);
    // String8 timings = PushStr8F(app.frameArena, "Target: %f | ms/frame: %.02fms | frame/s: %.02f\n", targetSecondsPerFrame * 1000, msPerFrame, FPS);
    // DebugPrint(timings);

    ArenaClear(app.frameArena);
  }

  SDL_DestroyRenderer(app.renderer); // Usually I just let the OS handle this stuff for me, but SDL is being a little funny
  SDL_Quit();
  return 0;
}