@echo off
REM Builds snake.exe on Windows. Works with MinGW-w64 (gcc) or Visual Studio (cl).
where gcc >nul 2>nul
if %errorlevel%==0 (
    echo Building with gcc...
    gcc -std=c11 -Wall -Wextra -O2 -o snake.exe snake.c
    goto done
)
where cl >nul 2>nul
if %errorlevel%==0 (
    echo Building with MSVC...
    cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS snake.c /Fe:snake.exe
    goto done
)
echo No C compiler found. Install MinGW-w64 (e.g. "winget install BrechtSanders.WinLibs.POSIX.UCRT")
echo or open a "Developer Command Prompt for VS" and run this script again.
exit /b 1
:done
if exist snake.exe (echo. & echo Built snake.exe - run it with: snake.exe) else (echo Build failed.)
