@echo off
rem build.bat — compila a DLL CABAL_Login (x64, Release).
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo NAO achei vcvars64.bat & exit /b 1)
if not exist x64\Release mkdir x64\Release
cl /nologo /O2 /W3 /LD src\login_dll.c src\login.c src\recvwatch.c /Fe:x64\Release\CABAL_Login.dll /link /OUT:x64\Release\CABAL_Login.dll user32.lib ws2_32.lib
if errorlevel 1 (echo FALHOU: linker nao gerou a DLL & exit /b 1)
if exist x64\Release\CABAL_Login.dll (echo OK: x64\Release\CABAL_Login.dll) else (echo FALHOU & exit /b 1)
