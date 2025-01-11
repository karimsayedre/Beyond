@echo off
pushd %~dp0
call %Beyond_DIR%\vendor\bin\premake5.exe vs2022 --verbose
popd
