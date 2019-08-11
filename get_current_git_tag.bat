@echo off
set HEADCOMMIT=null
FOR /F %%i IN (%CD%\.git\HEAD) DO ( 
	set HEADCONTENT=%%i
)
IF "%HEADCONTENT%" == "ref:" (
	FOR /F "eol=; tokens=2,3* delims=, " %%i in (%CD%\.git\HEAD) do set FILENAME=%%i
	FOR /F %%k IN (%CD%\.git\%FILENAME%) DO ( set HEADCOMMIT=%%k )
) ELSE (
	set HEADCOMMIT=%HEADCONTENT%
)

set FOUND=no
for /F %%f in ('dir /b %CD%\.git\refs\tags\') do (
	for /F %%h in (%CD%\.git\refs\tags\%%f) do (
		if %HEADCOMMIT%==%%h  (
			echo %%~nxf
			set FOUND=yes
		)
	)
)
if %FOUND%==no echo untagged