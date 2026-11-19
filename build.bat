@echo off
setlocal

set "QT_DIR=C:\Qt\6.12.0\mingw_64"
set "CMAKE=C:\Qt\Tools\CMake_64\bin\cmake.exe"
set "PATH=C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;%PATH%"

cd /d "%~dp0"

echo [1/3] Configure
"%CMAKE%" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/Qt/6.12.0/mingw_64
if errorlevel 1 goto fail

echo [2/3] Build
"%CMAKE%" --build build
if errorlevel 1 goto fail

echo [3/3] Deploy Qt runtime (windeployqt)
"%QT_DIR%\bin\windeployqt.exe" --release --no-translations build\WebhookSender.exe
if errorlevel 1 goto fail

echo BUILD OK: %~dp0build\WebhookSender.exe
exit /b 0

:fail
echo BUILD FAILED
exit /b 1
