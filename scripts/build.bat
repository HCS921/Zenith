@echo off

call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"

if not exist build mkdir build

pushd build

rc /fo resource.res ..\src\resource.rc
cl -nologo -FC /MT /O2 /Zi ..\src\win32_main.c resource.res /Fe:Zenith.exe user32.lib gdi32.lib d3d11.lib d3dcompiler.lib dxguid.lib shell32.lib ole32.lib windowscodecs.lib

popd
