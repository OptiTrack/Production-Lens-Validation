@echo off
setlocal

:: ----------------------------------------------------------------------
:: Usage
:: ----------------------------------------------------------------------
if "%~1"=="" (
    echo Usage: %~nx0 ^<CAMERA_SDK_PATH^> [--ffmpeg [path]]
    exit /b 1
)
set "CAMERA_SDK_PATH=%~1"
shift

:: Defaults
set "ENABLE_FFMPEG=OFF"
set "FFMPEG_ROOT="

:: ----------------------------------------------------------------------
:: Parse optional args
:: ----------------------------------------------------------------------
:parse_args
if "%~1"=="" goto :args_done

if /I "%~1"=="--ffmpeg" (
    set "ENABLE_FFMPEG=ON"
    shift
    if not "%~1"=="" (
        echo %~1 | findstr /B /C:"--" >nul
        if errorlevel 1 (
            set "FFMPEG_ROOT=%~1"
            shift
        )
    )
    goto :parse_args
)

for /f "tokens=1,2 delims==" %%A in ("%~1") do (
    if /I "%%A"=="--ffmpeg" (
        set "ENABLE_FFMPEG=ON"
        if not "%%B"=="" set "FFMPEG_ROOT=%%B"
        shift
        goto :parse_args
    )
)

shift
goto :parse_args

:args_done

:: ----------------------------------------------------------------------
:: Locate and init Visual Studio environment (same as before)
:: ----------------------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`
        "%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    `) do set "VS_INSTALL_DIR=%%I"
)

set "VCVARS_BATCH="
if defined VS_INSTALL_DIR (
    set "VCVARS_BATCH=%VS_INSTALL_DIR%\VC\Auxiliary\Build\vcvars64.bat"
)
if not defined VCVARS_BATCH (
    for %%P in (
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) do if exist %%~P set "VCVARS_BATCH=%%~P"
)

if not defined VCVARS_BATCH (
    echo ERROR: Could not locate vcvars64.bat>&2
    exit /b 1
)

call "%VCVARS_BATCH%" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to run "%VCVARS_BATCH%".>&2
    exit /b 1
)

:: ----------------------------------------------------------------------
:: Build directory
:: ----------------------------------------------------------------------
if exist build (
    rmdir /s /q build
)
mkdir build
pushd build

:: ----------------------------------------------------------------------
:: Configure CMake
:: ----------------------------------------------------------------------
set "CMAKE_ARGS=-DCAMERA_SDK_PATH=%CAMERA_SDK_PATH%"
if /I "%ENABLE_FFMPEG%"=="ON" (
    set "CMAKE_ARGS=%CMAKE_ARGS% -DENABLE_FFMPEG=ON"
    if defined FFMPEG_ROOT (
        set "CMAKE_ARGS=%CMAKE_ARGS% -DFFMPEG_ROOT=%FFMPEG_ROOT%"
    )
)

echo Configuring with:
echo   CAMERA_SDK_PATH = %CAMERA_SDK_PATH%
echo   ENABLE_FFMPEG   = %ENABLE_FFMPEG%
if defined FFMPEG_ROOT echo   FFMPEG_ROOT     = %FFMPEG_ROOT%

cmake .. %CMAKE_ARGS%
if errorlevel 1 (
    echo ERROR: CMake configuration failed>&2
    popd
    exit /b 1
)

:: ----------------------------------------------------------------------
:: Build
:: ----------------------------------------------------------------------
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed>&2
    popd
    exit /b 1
)

popd
echo Done.
endlocal
