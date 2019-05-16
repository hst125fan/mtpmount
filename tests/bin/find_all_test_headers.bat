@echo off
del all_test_headers.h
bin\find_all_test_headers_helper.bat > all_test_headers.h
rem for %%i in (*.test.h) do (
rem	echo #include "%%~nxi" > all_test_headers.h
rem )