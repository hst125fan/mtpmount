@echo off
FOR /F %%i IN (%CD%\.git\HEAD) DO ( 
	set HEADCONTENT=%%i
)
IF "%HEADCONTENT%" == "ref:" (
	FOR /F "eol=; tokens=2,3* delims=, " %%i in (%CD%\.git\HEAD) do set FILENAME=%%i
	FOR /F %%k IN (%CD%\.git\%FILENAME%) DO ( echo %%k )
) ELSE (
	echo %HEADCONTENT%
)	