@echo off
setlocal

:: Project dirs
set code_dir=..\code
set lib_dir=..\code\libs

:: Build opts
set compiler=-Od -nologo -std:c11 -W4 -WX -wd4244 -wd4042 -wd4267 -wd4456 -wd4127 -wd4116 -wd4090
set defines=-DCOMPILER_MSVC=1 -D_DEBUG=1
set debug=-FC -Zi

:: Platform specific opts
set platform_includes=-I%lib_dir%
set platform_libs=shell32.lib user32.lib gdi32.lib opengl32.lib

:: Game specific opts
set game_includes=-I%lib_dir%
set game_libs=opengl32.lib

:: Common linker opts
set link=-opt:ref -incremental:no

:: Platform linker opts
set platform_link=-subsystem:windows

:: DLL linker opts
set dll_link=-EXPORT:Load -EXPORT:Update -EXPORT:Render

if not exist build mkdir build
pushd build

echo Compiler: MSVC, Target: Windows x64
echo Compiling game DLL
cl %compiler% %defines% %debug% %game_includes% %code_dir%\game.c -LD %game_libs% /link %link% %dll_link%

echo Compiling platform executable
cl %compiler% -DOS_WINDOWS=1 %defines% %debug% %platform_includes% %code_dir%\platform_windows.c %platform_libs% -Feplatform /link %link% %platform_link%

popd