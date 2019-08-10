@echo off
echo collecting dokan dependencies...
mkdir "%1"
mkdir "%2"
mkdir "%3"
set dokanlib=notfound
for /d %%f in ("C:\Program Files\Dokan\Dokan Library-*") do (
	set dokanlib=%%f
)
if "%dokanlib%"=="notfound" exit /b 1
xcopy /s /y "%dokanlib%\include\*" "%1"
xcopy /s /y "%dokanlib%\lib\*" "%2"
xcopy /s /y "%dokanlib%\x86\lib\*" "%3"