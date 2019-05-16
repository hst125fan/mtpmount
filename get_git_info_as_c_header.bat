@echo off
SET CHEADERTEMPA=%CD%\batch_current_gittag
SET CHEADERTEMPB=%CD%\batch_current_gitcommit
call get_current_git_tag > %CHEADERTEMPA%
call get_current_git_commit_hash > %CHEADERTEMPB%
FOR /F %%i IN (%CHEADERTEMPA%) do set GITTAG=%%i
FOR /F %%j IN (%CHEADERTEMPB%) do set GITCOMMIT=%%j
del batch_current_gittag
del batch_current_gitcommit
echo const char* getGitTag^(^)
echo {
echo   return "%GITTAG%";
echo }
echo const char* getGitCommit^(^)
echo {
echo   return "%GITCOMMIT:~0,7%";
echo }