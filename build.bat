@echo off
setlocal

set "MSYS2_SHELL=C:\msys64\msys2_shell.cmd"
set "UCRT_BIN=C:\msys64\ucrt64\bin"
set "PROJECT_DIR=%~dp0"
set "SRC=main.cpp"
set "OUT=game.exe"
set "CXXFLAGS=-std=c++17 -O2 -Wall -Wextra"
set "INCLUDE=-IC:/msys64/ucrt64/include"
set "LIBDIR=-LC:/msys64/ucrt64/lib"
set "LIBS=-lraylib -lopengl32 -lgdi32 -lwinmm"

if "%PROJECT_DIR:~-1%"=="\" set "PROJECT_DIR=%PROJECT_DIR:~0,-1%"
set "PROJECT_DIR_UNIX=%PROJECT_DIR:\=/%"
set "COMPILE_CMD=cd '%PROJECT_DIR_UNIX%' && g++ %SRC% -o %OUT% %CXXFLAGS% %INCLUDE% %LIBDIR% %LIBS%"
"%MSYS2_SHELL%" -defterm -no-start -ucrt64 -c "%COMPILE_CMD%"
if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

copy /Y "%UCRT_BIN%\libraylib.dll" "%PROJECT_DIR%\" >nul || (
    echo Failed to copy libraylib.dll
    exit /b 1
)
copy /Y "%UCRT_BIN%\glfw3.dll" "%PROJECT_DIR%\" >nul || (
    echo Failed to copy glfw3.dll
    exit /b 1
)
copy /Y "%UCRT_BIN%\libstdc++-6.dll" "%PROJECT_DIR%\" >nul || (
    echo Failed to copy libstdc++-6.dll
    exit /b 1
)
copy /Y "%UCRT_BIN%\libgcc_s_seh-1.dll" "%PROJECT_DIR%\" >nul || (
    echo Failed to copy libgcc_s_seh-1.dll
    exit /b 1
)
copy /Y "%UCRT_BIN%\libwinpthread-1.dll" "%PROJECT_DIR%\" >nul || (
    echo Failed to copy libwinpthread-1.dll
    exit /b 1
)

echo Build succeeded: %OUT%
endlocal
