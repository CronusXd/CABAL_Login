@echo off
rem build_probe.bat — compila a sonda de observacao do dispatch de UI (x64).
rem Saida: x64\Release\probe.dll
cd /d "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (echo NAO achei vcvars64.bat & exit /b 1)
if not exist ..\x64\Release mkdir ..\x64\Release
ml64 /nologo /c hookstub.asm /Fohookstub.obj
if errorlevel 1 (echo FALHOU ml64 & exit /b 1)
cl /nologo /O2 /W3 /LD probe.c hookstub.obj /Fe:..\x64\Release\probe.dll /link /OUT:..\x64\Release\probe.dll
if exist ..\x64\Release\probe.dll (echo OK: ..\x64\Release\probe.dll) else (echo FALHOU & exit /b 1)
