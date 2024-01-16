@echo off

:: Project dirs
set code_dir=..\code
set lib_dir=..\code\libs

:: Build opts
set compiler=-GS- -Gs1000000000 -nologo -std:c11 -W4 -WX -wd4244 -wd4042
set defines=-DNO_CRT=1 -DCOMPILER_MSVC=1 -DBUILD_INTERNAL=1 -DBUILD_SLOW=1
set debug=-FC -Zi

:: Platform specific opts
set platform_includes=-I%lib_dir% -I%lib_dir%\SDL2\include
set platform_libs=%lib_dir%\SDL2\lib\x64\SDL2.lib %lib_dir%\SDL2\lib\x64\SDL2.lib shell32.lib kernel32.lib

:: Game specific opts
set game_includes=-I%lib_dir% -I%lib_dir%\Steamworks\sdk\public\
set game_libs=-I%lib_dir%\Steamworks\sdk\redistributable_bin\win64\steam_api64.lib

:: Common linker opts
set link=-opt:ref -incremental:no -stack:0x200000,200000 -nodefaultlib

:: Platform linker opts
set platform_link=-subsystem:windows -entry:mainCRTStartup

:: DLL linker opts
set dll_link=-EXPORT:GameInit -EXPORT:GameTick -entry:DllMain

if not exist build mkdir build
pushd build

echo Compiler: MSVC, Target: Windows x64
echo Compiling game DLL
cl -Od %compiler% %defines% %debug% %game_includes% %code_dir%\game.c -LD %game_libs% /link %link% %dll_link%

echo Compiling platform executable
cl -Od %compiler% -DOS_WINDOWS=1 %defines% %debug% %platform_includes% %code_dir%\platform.c %platform_libs% /link %link% %platform_link%

popd