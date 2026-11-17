@echo off
echo desktop-stickman v4.0 - build (MinGW-w64)...
windres src\resource.rc -O coff -o src\resource.o
g++ -std=c++17 -O2 -s -Wall -Wextra src\main.cpp src\resource.o -o desktop-stickman.exe -mwindows -static -static-libgcc -static-libstdc++ -lgdiplus -ldwmapi -luser32 -lgdi32 -lshell32 -lole32 -luuid -ladvapi32
if %ERRORLEVEL% EQU 0 (
    echo Done! Run desktop-stickman.exe
) else (
    echo Build failed. Make sure MinGW-w64 g++ and windres are in PATH.
)
pause
