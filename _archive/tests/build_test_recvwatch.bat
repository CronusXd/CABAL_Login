@echo off
rem build_test_recvwatch.bat — compila e roda o teste do pipeline recvwatch.
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo NAO achei vcvars64.bat & exit /b 1)
cl /nologo /O2 test_recvwatch.c /Fe:test_recvwatch.exe ws2_32.lib
if errorlevel 1 (echo COMPILACAO FALHOU & exit /b 1)
"%~dp0test_recvwatch.exe"
exit /b %errorlevel%