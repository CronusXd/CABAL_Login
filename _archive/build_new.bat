@echo off
cd /d "D:\projeto\CABAL_Login"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 exit /b 1
cl /nologo /O2 /W3 /LD src\login_dll.c src\login.c src\recvwatch.c /Fe:x64\Release\CABAL_Login_new.dll /link /OUT:x64\Release\CABAL_Login_new.dll user32.lib ws2_32.lib
exit /b %errorlevel%