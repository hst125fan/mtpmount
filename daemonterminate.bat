@echo off
IF NOT EXIST %1 exit 0
call %1 daemonctl terminate
if %ERRORLEVEL% == 0 exit 0
if %ERRORLEVEL% == -3 exit 0
exit %ERRORLEVEL%