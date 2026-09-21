@echo off
setlocal EnableDelayedExpansion

rem Builds build\MWCheats.asi with a 32-bit MinGW-w64 g++ (C++20).

set CXX=g++
set CXXFLAGS=-m32 -std=c++20 -O2 -Wall -Wextra -fno-strict-aliasing -fno-exceptions -fno-rtti -DNOMINMAX -DWIN32_LEAN_AND_MEAN
set LDFLAGS=-shared -static -static-libgcc -static-libstdc++ -Wl,--exclude-all-symbols -Wl,-u,___mingw_SEH_error_handler -ladvapi32 -lgdi32 -lwinmm

set SOURCES=src\dllmain.cpp
for %%f in (src\NFSMW\*.cpp) do set SOURCES=!SOURCES! %%f
for %%f in (src\UI\Animation\*.cpp) do set SOURCES=!SOURCES! %%f

if not exist build mkdir build

windres -F pe-i386 -O coff -o build\UI.res.o src\UI\UI.rc
if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)

%CXX% %CXXFLAGS% %SOURCES% build\UI.res.o %LDFLAGS% -o build\MWCheats.asi
if errorlevel 1 (
    echo Build FAILED.
    exit /b 1
)

echo Built build\MWCheats.asi
echo Copy it and MWCheats.ini into your Most Wanted "scripts" folder.
endlocal
