@echo off
setlocal

REM Compile DLL
cl /LD umcgi\bus.c /link /OUT:umcgi\bus.dll

REM Clean up build artifacts
del /Q /F *.obj *.exp *.lib

REM Start PHP server
php -S %1

endlocal
