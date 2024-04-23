@echo off
setlocal

if not exist ..\build mkdir ..\build
pushd ..\build
cl /nologo /Zi /W4 /Od /wd4201 /Zc:strictStrings- ..\code\s_main.c /link /incremental:no D3D11.lib 	User32.lib dxguid.lib d3dcompiler.lib
popd

endlocal
