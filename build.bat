@echo off
setlocal EnableExtensions

set "ROOT=%~dp0"
set "QTDIR=C:\Qt\6.8.3\msvc2022_64"
if not exist "%QTDIR%\bin\qmake.exe" (
    echo Qt not found at %QTDIR%
    echo Install with: pip install aqtinstall ^&^& aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 --outputdir C:\Qt
    exit /b 1
)

if not defined VCINSTALLDIR (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
    if exist "%VSWHERE%" (
        for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -property installationPath`) do (
            call "%%i\VC\Auxiliary\Build\vcvars64.bat" >nul
        )
    )
)
if not defined VCINSTALLDIR (
    if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
        call "%ProgramFiles%\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
    )
)

where cl >nul 2>nul
if errorlevel 1 (
    echo cl.exe not found. Install Visual Studio C++ tools.
    exit /b 1
)

where cmake >nul 2>nul
if errorlevel 1 (
    echo cmake not found. Install CMake 3.19+
    exit /b 1
)

set "BUILDQT=%ROOT%build"
if not exist "%BUILDQT%" mkdir "%BUILDQT%"

pushd "%ROOT%"
cmake -S . -B build\qt -G "Ninja" -DCMAKE_PREFIX_PATH="%QTDIR%" -DCMAKE_BUILD_TYPE=Release 2>nul
if errorlevel 1 (
    echo Ninja not found, trying Visual Studio generator...
    cmake -S . -B build\qt -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QTDIR%" 2>nul
)
if errorlevel 1 (
    echo CMake configure failed.
    popd
    exit /b 1
)

cmake --build build\qt --config Release
if errorlevel 1 (
    echo Build failed.
    popd
    exit /b 1
)

echo.
echo Qt build done.
echo   build\PluginManager.exe  (Qt 6, LGPL dynamic - now at build\)
echo See THIRD_PARTY_NOTICES.md for Qt LGPL compliance.
popd
