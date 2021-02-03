@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsall.bat" amd64
set BASEDIR=%~dp0\
set path=%BASEDIR%;%path%
