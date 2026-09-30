@echo off
setlocal

rem ============================================================
rem  FoolChat one-click launcher: start 4 services
rem  Order: StatusServer -> ChatServer1 -> ChatServer2 -> GateServer
rem  Each service runs in its own window, cwd = exe dir (reads config.ini there)
rem  Requires: MySQL / Redis / VarifyServer already running
rem ============================================================

set "ROOT=%~dp0"
set "MISSING=0"

call :run "StatusServer" "%ROOT%StatusServer\x64\Debug"    "StatusServer.exe"
call :run "ChatServer1"  "%ROOT%ChatServer\x64\Debug"      "ChatServer.exe"
call :run "ChatServer2"  "%ROOT%ChatServer2\x64\Debug"     "ChatServer.exe"
call :run "GateServer"   "%ROOT%GateServer\x64\Debug"      "GateServer.exe"

echo.
if "%MISSING%"=="1" (
    echo [HINT] Some services missing, see MISSING lines above. Build them first.
    pause
) else (
    echo [DONE] 4 services started. You can close this window.
    timeout /t 3 >nul
)
exit /b 0

:run
set "LABEL=%~1"
set "DIR=%~2"
set "EXE=%~3"
if not exist "%DIR%\%EXE%" (
    echo [MISSING] %LABEL%: "%DIR%\%EXE%"
    set "MISSING=1"
    exit /b 1
)
pushd "%DIR%"
start "FoolChat - %LABEL%" "%EXE%"
popd
echo [OK] %LABEL%
exit /b 0
