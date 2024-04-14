@echo off
setlocal

if not exist ..\build mkdir ..\build
pushd ..\build
cl /nologo /Zi /W4 /Od /Zc:strictStrings- ..\code\s_main.c /link /incremental:no
popd

endlocal