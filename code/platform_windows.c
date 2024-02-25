// Headers
#include <windows.h>
#include <gl/gl.h>
#include <wglext.h>
#include "base/base_include.h"
#include "game.h"

// Source
#define STB_SPRINTF_IMPLEMENTATION
#include <stb/stb_sprintf.h>
#include "base/base_include.c"

typedef struct Win32GameHandle Win32GameHandle;
struct Win32GameHandle
{
  HMODULE handle;
  FILETIME lastWriteTime;

  #define X(ret, name, ...) Game##name##Func *name;
  GAME_VTABLE
  #undef X
};

typedef struct Win32Window Win32Window;
struct Win32Window
{
  HWND handle;
  RECT dim;
};

typedef struct Win32State Win32State;
struct Win32State
{
  Win32Window window;
  HDC dc;
  HGLRC glContext;
  u64 perfFrequency;
};

global Win32State globalState;
global b32 globalGameRunning = true;
global GameInput globalGameInput[2];

// Public API

extern void
DebugPrint(String8 msg)
{
  OutputDebugString((LPCSTR)msg.str);
}

// Internal functions

function void
Win32CopyFile(char *src, char *dest)
{
  CopyFile(src, dest, false);
}

function FILETIME
Win32GetLastWriteTime(String8 path)
{
  FILETIME result = {0};

  WIN32_FIND_DATA fileData;
  HANDLE fileHandle = FindFirstFile((LPCSTR)path.str, &fileData);
  if (fileHandle != INVALID_HANDLE_VALUE) {
    result = fileData.ftLastWriteTime;
    FindClose(fileHandle);
  }

  return result;
}

function void*
Win32MemReserve(u64 size)
{
  return VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
}

function void
Win32MemCommit(void *ptr, u64 size)
{
  VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
}

function void
Win32MemDecommit(void *ptr, u64 size)
{
  VirtualFree(ptr, size, MEM_DECOMMIT);
}

function void
Win32MemRelease(void *ptr, u64 size)
{
  VirtualFree(ptr, size, MEM_RELEASE);
}

function void
Win32ProcessInput(GameButtonState *oldState, GameButtonState *newState, b32 isDown)
{
  newState->isDown = isDown;
  newState->halfTransitionCount = (oldState->isDown != newState->isDown) ? 1 : 0;
}

function LRESULT CALLBACK
Win32WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg) {
    case WM_DESTROY: {
      PostQuitMessage(0);
      return 0;
    }

    case WM_SIZE: {
      globalState.window.dim.right = LOWORD(lParam);
      globalState.window.dim.bottom = HIWORD(lParam);
      return 0;
    }

    case WM_KEYUP: fallthrough
    case WM_KEYDOWN: {
      GameInput *newInput = &globalGameInput[0];
      GameInput *oldInput = &globalGameInput[1];
      GameInputSource *keyboard = &newInput->sources[0];
      GameInputSource *oldKeyboard = &oldInput->sources[0];

      b32 isDown = ((lParam & (1 << 31)) == 0);
      b32 wasDown = ((lParam & (1 << 30)) != 0);
      if (isDown != wasDown) {
        switch (wParam) {
          // TODO: I'm too stupid to add arrow key support without doubling the move speed if you press both @ same time
          case 'W': if (isDown) keyboard->yAxis++; else keyboard->yAxis--; break;
          case 'A': if (isDown) keyboard->xAxis--; else keyboard->xAxis++; break;
          case 'S': if (isDown) keyboard->yAxis--; else keyboard->yAxis++; break;
          case 'D': if (isDown) keyboard->xAxis++; else keyboard->xAxis--; break;

          case 'Q': Win32ProcessInput(&oldKeyboard->primary, &oldKeyboard->primary, isDown); break;
          case 'E': Win32ProcessInput(&oldKeyboard->secondary, &oldKeyboard->secondary, isDown); break;
          case 'R': Win32ProcessInput(&oldKeyboard->tertiary, &oldKeyboard->tertiary, isDown); break;
          case 'T': Win32ProcessInput(&oldKeyboard->quaternary, &oldKeyboard->quaternary, isDown); break;
        }
      }

      return 0;
    }
  }

  return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

function b32
Win32GetGameHandle(Win32GameHandle *result, String8 dllPath, String8 dllTempPath)
{
  b32 valid = 0;
  Win32CopyFile((char*)dllPath.str, (char*)dllTempPath.str);
  result->handle = LoadLibrary((LPCSTR)dllTempPath.str);
  result->lastWriteTime = Win32GetLastWriteTime(dllPath);
  if (result->handle) {
    #define X(ret, name, ...) \
    result->name = (Game##name##Func*)GetProcAddress(result->handle, #name); \
    valid = (result->name != 0);
    GAME_VTABLE
    #undef X
  }

  return valid;
}

function void
Win32ReleaseGameHandle(Win32GameHandle *handle)
{
  FreeLibrary(handle->handle);
  MemoryZero(handle, sizeof(Win32GameHandle));
}

function b32
Win32Init(Arena *arena, HINSTANCE hInstance, int nCmdShow)
{
  // Create main window
  read_only char *wndClassName = "Main Window Class";
  WNDCLASS wndClass = {0};
  wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  wndClass.lpfnWndProc = Win32WindowProc;
  wndClass.hInstance = hInstance;
  wndClass.style = CS_OWNDC;
  wndClass.lpszClassName = wndClassName;
  RegisterClass(&wndClass);

  RECT dim = {0, 0, 1280, 720};
  RECT dimCpy = dim;
  AdjustWindowRect(&dim, 0, false);

  HWND mainWindow = CreateWindowEx(
    0,
    wndClassName,
    "Creep (Dev build)",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    dim.right,
    dim.bottom,
    NULL,
    NULL,
    hInstance,
    NULL);
  globalState.window = (Win32Window){mainWindow, dimCpy};
  globalState.dc = GetDC(globalState.window.handle);

  if (globalState.window.handle) {
    // Obtain performance frequency
    LARGE_INTEGER perfFrequency;
    QueryPerformanceFrequency(&perfFrequency);
    globalState.perfFrequency = perfFrequency.QuadPart;

    // Create OpenGL context
    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    // TODO: Do we need these for a 2D game?
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    int pfi = ChoosePixelFormat(globalState.dc, &pfd);
    SetPixelFormat(globalState.dc, pfi, &pfd);
    globalState.glContext = wglCreateContext(globalState.dc);
    wglMakeCurrent(globalState.dc, globalState.glContext);
    PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");

    // Vsync
    b32 hasVsync = false;
    {
      TempArena temp = ArenaTempBegin(arena);
      char *extBuffer = wglGetExtensionsStringARB(globalState.dc);
      u8 splits[] = {' '};
      String8List extList = Str8Split(temp.arena, Str8C(extBuffer), 1, splits);
      for (String8Node *node = extList.first; node; node = node->next) {
        if (Str8Match(node->string, Str8Lit("WGL_EXT_swap_control"), 0)) {
          hasVsync = true;
          break;
        }
      }
      ArenaTempEnd(temp);
    }
    if (hasVsync) {
      PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
      wglSwapIntervalEXT(1);
    } else {
      DebugPrint(Str8Lit("This machine does not support vsync!\n"));
    }
  }

  ShowWindow(globalState.window.handle, nCmdShow);
  return mainWindow != NULL;
}

typedef struct Win32ThreadInfo Win32ThreadInfo;
struct Win32ThreadInfo
{
  u64 idx;
};

DWORD WINAPI
ThreadProc(LPVOID lpParameter)
{
  Win32ThreadInfo *info = lpParameter;
  Unused(info);
  for (;;) {
    Sleep(1000);
  }
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
  Unused(hPrevInstance && lpCmdLine);

  u64 backingBufferSize = Gigabytes(1);
  void *backingBuffer = Win32MemReserve(backingBufferSize);
  Win32MemCommit(backingBuffer, backingBufferSize);
  Arena *platformArena = ArenaAlloc(backingBuffer, backingBufferSize);

  if (!Win32Init(platformArena, hInstance, nCmdShow)) {
    DebugPrint(Str8Lit("Unable to initialize Win32 platform layer!\n"));
  }

  String8 gameDllPath = {0};
  String8 gameTempDllPath = {0};
  {
    TempArena temp = ArenaTempBegin(platformArena);
    char filenameCStr[MAX_PATH];
    GetModuleFileName(0, filenameCStr, sizeof(filenameCStr)); // TODO: Apparently max path is unsafe, good thing this is debug code
    String8 filename = Str8C(filenameCStr);

    u8 splits[] = { '\\' };
    String8List path = Str8Split(temp.arena, filename, 1, splits);
    String8List tmpPath = Str8Split(temp.arena, filename, 1, splits);
    path.last->string = Str8Lit("game.dll");
    tmpPath.last->string = Str8Lit("game_temp.dll");

    String8Join joinOpts = {0};
    joinOpts.sep = Str8Lit("\\");
    gameDllPath = Str8ListJoin(temp.arena, &path, &joinOpts);
    gameTempDllPath = Str8ListJoin(temp.arena, &tmpPath, &joinOpts);
    ArenaTempEnd(temp);
  }

  PlatformAPI platformAPI = {0};
  #define X(ret, name, ...) platformAPI.name = name;
  PLATFORM_VTABLE
  #undef X

  GameMemory gameMemory = {0};
  gameMemory.size = Gigabytes(4); // TODO: Change?
  gameMemory.mem = Win32MemReserve(gameMemory.size);
  Win32MemCommit(gameMemory.mem, gameMemory.size);

  GameInput *newInput = &globalGameInput[0];
  GameInput *oldInput = &globalGameInput[1];

  Win32GameHandle game;
  Assert(Win32GetGameHandle(&game, gameDllPath, gameTempDllPath));
  game.Load(true, platformAPI, gameMemory);

  LARGE_INTEGER lastCounter, endCounter;
  QueryPerformanceCounter(&lastCounter);
  for (;globalGameRunning;) {
    TempArena frameArena = ArenaTempBegin(platformArena);
    // TODO: Reload game code
    FILETIME dllWriteTime = Win32GetLastWriteTime(gameDllPath);
    if (CompareFileTime(&dllWriteTime, &game.lastWriteTime)) {
      Sleep(100); // TODO: Ugly hack
      Win32ReleaseGameHandle(&game);
      Assert(Win32GetGameHandle(&game, gameDllPath, gameTempDllPath));
      game.Load(false, platformAPI, gameMemory);
    }

    for (MSG msg; PeekMessage(&msg, 0, 0, 0, PM_REMOVE);) {
      if (msg.message == WM_QUIT)
        globalGameRunning = false;

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    game.Update(gameMemory, *newInput);
    game.Render(gameMemory, globalState.window.dim.right, globalState.window.dim.bottom);

    SwapBuffers(globalState.dc);
    Swap(GameInput*, newInput, oldInput);

    QueryPerformanceCounter(&endCounter);
#if 0
    f32 elapsedMS = (f32)(endCounter.QuadPart - lastCounter.QuadPart) * 1000.f;
    elapsedMS /= (f32)globalState.perfFrequency;
    lastCounter = endCounter;
    DebugPrint(PushStr8F(frameArena.arena, "ms/frame: %.02fms\n", elapsedMS)); // For some reason this value seems off...
#endif
    ArenaTempEnd(frameArena);
  }

  return 0;
}