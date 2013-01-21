@echo off
if "%1" = "clean" goto clean

MAKE -C hchlb release
MAKE -C hanlib release
MAKE -C hst release
cd kime
dmake
cd ..
copy kime\kime.exe .
copy kime\kimehook.dll .
copy kime\except.dat .
goto end

:clean
MAKE -C hchlb clean
MAKE -C hanlib clean
MAKE -C hst clean
cd kime
dmake clean
cd ..

:end
