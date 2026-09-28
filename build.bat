@echo off
setlocal
cd /d "%~dp0"

REM Always pass this value, including OFF, to override any old CMake cache.
set "POSER_ENABLE_LAYERED_OVERLAY=0"
echo [build.bat] layered overlay=%POSER_ENABLE_LAYERED_OVERLAY%

REM Prefer cmake + MSVC (Visual Studio 17 2022 generator).
REM NOTE: The WinGet cmake is a MinGW build; its default generator may pick
REM Ninja/g++, which cannot compile the plugin source (MSVC __try/__except).
REM So we must force the VS generator, and fall back to build_msvc.ps1.
where cmake >nul 2>&1
if %errorlevel%==0 (
  cmake -S . -B build\cmake-vs2022 -G "Visual Studio 17 2022" -A x64 -DPOSER_ENABLE_LAYERED_OVERLAY=%POSER_ENABLE_LAYERED_OVERLAY% || goto fallback
  cmake --build build\cmake-vs2022 --config Release || goto fallback
  goto ok
)
:fallback
echo [build.bat] cmake/VS generator unavailable - falling back to tools\build_msvc.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_msvc.ps1 -EnableLayeredOverlay %POSER_ENABLE_LAYERED_OVERLAY% && goto ok || exit /b 1
:ok
if not exist plugin mkdir plugin
echo Build OK. plugin\ folder contains:
echo   - poser.dll             (Endfield Poser plugin)
echo   - d3dcompiler_47.dll    (DX proxy loader, built locally)
echo   - vulkan-1.dll         (Vulkan proxy loader, built locally)
echo Install with the installation wizard at the repository root.
