@echo off
setlocal

:: -- Input check ----------------------------------------------------------
if "%~1"=="" (
    echo Usage: %~nx0 ^<CAMERA_SDK_PATH^>
    exit /b 1
)
set "CAMERA_SDK_PATH=%~1"

:: -- Locate vswhere.exe --------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_INSTALL_DIR=%%I"
    )
) else (
    echo WARNING: vswhere.exe not found at "%VSWHERE%". Will attempt fallback paths.
)

:: -- Determine path to vcvars64.bat ------------------------------------
set "VCVARS_BATCH="

if defined VS_INSTALL_DIR (
    set "VCVARS_BATCH=%VS_INSTALL_DIR%\VC\Auxiliary\Build\vcvars64.bat"
    if not exist "%VCVARS_BATCH%" (
        set "VCVARS_BATCH="
    )
)

if not defined VCVARS_BATCH (
    :: Fallback: try common installations (you can extend this list if needed)
    for %%P in (
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
        "C:\Program Files\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
    ) do (
        if exist %%~P (
            set "VCVARS_BATCH=%%~P"
            goto :found_vcvars
        )
    )
)

:found_vcvars
if not defined VCVARS_BATCH (
    echo ERROR: Could not locate vcvars64.bat. Please install Visual Studio with C++ build tools or provide its location.>&2
    exit /b 1
)

echo Using vcvars64.bat at "%VCVARS_BATCH%"

:: Call the environment setup
call "%VCVARS_BATCH%" >nul 2>&1
if errorlevel 1 (
    echo ERROR: Failed to run "%VCVARS_BATCH%".&2
    exit /b 1
)

:: -- Setup build directory ----------------------------------------------
if exist build (
    echo WARNING: This will delete the existing build directory and all its contents.
    choice /C YN /N /M "Are you sure you want to delete it? [Y/N] "
    if errorlevel 2 (
        echo Aborted.
        exit /b 1
    )
    echo Deleting existing build directory...
    rmdir /s /q build
)
mkdir build
pushd build

:: -- Invoke CMake -------------------------------------------------------
cmake .. -DCAMERA_SDK_PATH=%CAMERA_SDK_PATH%
if errorlevel 1 (
    echo ERROR: CMake configuration failed.&2
    popd
    exit /b 1
)

cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed.&2
    popd
    exit /b 1
)

popd
echo Done.
endlocal
