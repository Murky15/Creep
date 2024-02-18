// Headers
#include <windows.h>
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
};

global Win32State globalState;
global b32 globalGameRunning = true;
global GameInput globalGameInput[2];

function void
DebugPrint(String8 msg)
{
  OutputDebugString((LPCSTR)msg.str);
}

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
          case VK_UP: fallthrough
          case 'W': if (isDown) ++keyboard->yAxis; else --keyboard->yAxis; break;
          case VK_LEFT: fallthrough
          case 'A': if (isDown) --keyboard->xAxis; else ++keyboard->xAxis; break;
          case VK_DOWN: fallthrough
          case 'S': if (isDown) --keyboard->yAxis; else ++keyboard->yAxis; break;
          case VK_RIGHT: fallthrough
          case 'D': if (isDown) ++keyboard->xAxis; else --keyboard->xAxis; break;

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
Win32GetGameHandle(Win32GameHandle *result, String8 dllPath)
{
  b32 valid = 0;
  result->handle = LoadLibrary((LPCSTR)dllPath.str);
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
}

function b32
Win32Init(HINSTANCE hInstance, int nCmdShow)
{
  // Create main window
  read_only char *wndClassName = "Main Window Class";
  WNDCLASS wndClass = {0};
  wndClass.lpfnWndProc = Win32WindowProc;
  wndClass.hInstance = hInstance;
  wndClass.style = CS_OWNDC;
  wndClass.lpszClassName = wndClassName;
  RegisterClass(&wndClass);

  RECT dim = {0, 0, 800, 600};
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
  }

  ShowWindow(globalState.window.handle, nCmdShow);
  return mainWindow != NULL;
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
  Unused(hPrevInstance && lpCmdLine);

  if (!Win32Init(hInstance, nCmdShow)) {
    DebugPrint(Str8Lit("Unable to initialize Win32 platform layer!\n"));
  }

  u64 backingBufferSize = Gigabytes(1);
  void *backingBuffer = Win32MemReserve(backingBufferSize);
  Win32MemCommit(backingBuffer, backingBufferSize);
  Arena *platformArena = ArenaAlloc(backingBuffer, backingBufferSize);

  String8 gameDllPath = {0};
  {
    TempArena temp = ArenaTempBegin(platformArena);
    char filenameCStr[MAX_PATH];
    GetModuleFileName(0, filenameCStr, sizeof(filenameCStr)); // TODO: Apparently max path is unsafe, good thing this is debug code
    String8 filename = Str8C(filenameCStr);

    u8 splits[] = { '\\' };
    String8List path = Str8Split(temp.arena, filename, 1, splits);
    path.last->string = Str8Lit("game.dll");

    String8Join joinOpts = {0};
    joinOpts.sep = Str8Lit("\\");
    gameDllPath = Str8ListJoin(temp.arena, &path, &joinOpts);
    ArenaTempEnd(temp);
  }

  GameMemory gameMemory = {0};
  gameMemory.size = Gigabytes(4); // TODO: Change?
  gameMemory.mem = Win32MemReserve(gameMemory.size);
  Win32MemCommit(gameMemory.mem, gameMemory.size);

  GameInput *newInput = &globalGameInput[0];
  GameInput *oldInput = &globalGameInput[1];

  Win32GameHandle game;
  if (!Win32GetGameHandle(&game, gameDllPath)) {
    DebugPrint(Str8Lit("Unable to load game code!\n"));
  }
  game.Load(true, gameMemory);

  for (;globalGameRunning;) {
    // TODO: Reload game code

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
  }

  return 0;
}