@echo off
rem build_probe_fixed.bat — compila a sonda com vcvars64
cd /d D:\projeto\CABAL_Login\probe
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo NAO achei vcvars64.bat & exit /b 1)
ml64 /nologo /c hookstub.asm /Fohookstub.obj
if errorlevel 1 (echo FALHOU ml64 & exit /b 1)
cl /nologo /O2 /W3 /LD probe.c hookstub.obj /Fe:..\x64\Release\probe.dll /link /OUT:..\x64\Release\probe.dll
if errorlevel 1 (echo FALHOU cl & exit /b 1)
if exist ..\x64\Release\probe.dll (echo OK: ..\x64\Release\probe.dll) else (echo FALHOU & exit /b 1)