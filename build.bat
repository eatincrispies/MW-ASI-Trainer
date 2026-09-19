@echo off
setlocal

rem Builds build\PursuitCheats.asi with a 32-bit MinGW-w64 g++ (C++20).

set CXX=g++
set CXXFLAGS=-m32 -std=c++20 -O2 -Wall -Wextra -fno-strict-aliasing -fno-exceptions -fno-rtti -DNOMINMAX -DWIN32_LEAN_AND_MEAN
set LDFLAGS=-shared -static -static-libgcc -static-libstdc++ -Wl,--exclude-all-symbols -Wl,-u,___mingw_SEH_error_handler -ladvapi32

if not exist build mkdir build

%CXX% %CXXFLAGS% src\dllmain.cpp src\NFSMW\Pursuit.cpp %LDFLAGS% -o build\PursuitCheats.asi
if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)

echo Built build\PursuitCheats.asi
echo Copy it and PursuitCheats.ini into your Most Wanted "scripts" folder.
endlocal
