@echo off
rem build_test_crypto.bat — compila e roda o harness da crypto.
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo NAO achei vcvars64.bat & exit /b 1)
cl /nologo /O2 test_crypto.c /Fe:test_crypto.exe
if errorlevel 1 (echo COMPILACAO FALHOU & exit /b 1)
"%~dp0test_crypto.exe"
exit /b %errorlevel%