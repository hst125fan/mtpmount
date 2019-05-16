@echo off
SET JUMPBACKDIR=%cd%
cd %1
call get_git_info_as_c_header.bat > %2
cd %JUMPBACKDIR%