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

typedef struct Bitmap Bitmap;
struct Bitmap
{
  u32 width, height;
  u32 bytesPerPixel;
  u32 pitch;
  void *pixels;
};

typedef struct GameMemory GameMemory;
struct GameMemory
{
  void *mem;
  u64 size;
};

typedef struct GamePayload GamePayload;
struct GamePayload
{
  GameMemory memory;
  Bitmap framebuffer;
  GameInput input;
};

// Game function vtable
typedef void GameInitFunc(void);
typedef void GameTickFunc(GamePayload*);

typedef struct Game Game;
struct Game
{
  u8 xOffset, yOffset;
};

#endif // GAME_H