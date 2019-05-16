@echo off
SET TEMPFILENAME="%CD%\batch_current_commit_temp"
call get_current_git_commit_hash > %TEMPFILENAME%
FOR /F "eol=; tokens=1 delims=, " %%i in (%TEMPFILENAME%) do (
	set HEADCOMMIT=%%i
)
del batch_current_commit_temp
set FOUND=no
for /R %%f in (%CD%\.git\refs\tags\*) do (
	for /F %%h in (%%f) do (
		if %HEADCOMMIT%==%%h  (
			echo %%~nf
			set FOUND=yes
		)
	)
)
if %FOUND%==no echo untagged