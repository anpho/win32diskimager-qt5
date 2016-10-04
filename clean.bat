@echo off
cd src
mingw32-make.exe release-distclean debug-distclean
cd ..
pause
@echo on
