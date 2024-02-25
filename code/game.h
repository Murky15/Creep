#ifndef GAME_H
#define GAME_H

typedef struct GameButtonState GameButtonState;
struct GameButtonState
{
  b32 isDown;
  u32 halfTransitionCount;
};
#define NUM_BUTTONS 4

typedef struct GameInputSource GameInputSource;
struct GameInputSource
{
  // b32 isConnected;

  f32 xAxis, yAxis;

  union
  {
    GameButtonState buttons[NUM_BUTTONS];

    struct
    {
      GameButtonState primary;
      GameButtonState secondary;
      GameButtonState tertiary;
      GameButtonState quaternary;
    };
  };
};

#define NUM_INPUT_SOURCES 1 // 4?
typedef struct GameInput GameInput;
struct GameInput
{
  GameInputSource sources[NUM_INPUT_SOURCES]; // Treat source 0 as keyboard
};

typedef struct GameMemory GameMemory;
struct GameMemory
{
  void *mem;
  u64 size;
};

// Function vtables (for communication between game and platform layer)
// (return, name, params...)

// Platform
#define PLATFORM_VTABLE \
  X(void, DebugPrint, String8) \

#define X(ret, name, ...) typedef ret Platform##name##Func(__VA_ARGS__);
PLATFORM_VTABLE
#undef X

typedef struct PlatformAPI PlatformAPI;
struct PlatformAPI
{
  #define X(ret, name, ...) Platform##name##Func *name;
  PLATFORM_VTABLE
  #undef X
};

// Game
#define GAME_VTABLE \
  X(void, Load, b32, PlatformAPI, GameMemory) \
  X(void, Update, GameMemory, GameInput) \
  X(void, Render, GameMemory, u64, u64) \

#define X(ret, name, ...) typedef ret Game##name##Func(__VA_ARGS__);
GAME_VTABLE
#undef X

#endif // GAME_H