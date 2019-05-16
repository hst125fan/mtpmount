@echo off
for %%i in (*.test.h) do (
	echo #include "%%~nxi"
)