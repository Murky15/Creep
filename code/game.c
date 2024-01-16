/*
  TODO:
  -  Steamworks API (need to compile separate TU)
*/

// Headers

#include "base/base_include.h"
#include "game.h"

// Source
// #define STB_IMAGE_IMPLEMENTATION
// #define STB_TRUETYPE_IMPLEMENTATION
// #include <stb/stb_image.h>
// #include <stb/stb_truetype.h>
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>

#define HANDMADE_MATH_NO_SSE
#include <HandmadeMath.h>

#include "base/base_include.c"

#if NO_CRT
#include <windows.h>
// Um... can we do this?
BOOL WINAPI DllMain (
    HINSTANCE const instance,  // handle to DLL module
    DWORD     const reason,    // reason for calling function
    LPVOID    const reserved)  // reserved
{
    Unused(instance && reason && reserved);
    return TRUE;  // Successful DLL_PROCESS_ATTACH.
}
#endif

function void
RenderTestGradient(Bitmap bitmap, u8 xOffset, u8 yOffset)
{
  for (u32 y = 0; y < bitmap.height; ++y) {
    for (u32 x = 0; x < bitmap.width; ++x) {
      u8 red = x + xOffset;
      u8 green = y + yOffset;
      u8 blue = 0;
      ((u32*)bitmap.pixels)[y * bitmap.width + x] = (red << 16 | green << 8 | blue);
    }
  }
}

function void
RenderTestScene(Bitmap bitmap)
{
  Unused(bitmap);
}

extern void
GameInit(void)
{
  // It's quiet here... (Do I even need this?)
}

extern void
GameTick(GamePayload *payload)
{
  Game *gameState = (Game*)payload->memory.mem;
  GameInput input = payload->input;

  GameInputSource *keyboard = &input.sources[0];

  gameState->xOffset += (keyboard->xAxis * 3);
  gameState->yOffset += (keyboard->yAxis * 3);

  RenderTestGradient(payload->framebuffer, gameState->xOffset, gameState->yOffset);
  // RenderTestScene(bitmap);
}