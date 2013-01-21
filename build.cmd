@echo off
MAKE -C hanlib release
MAKE -C hchlb release
cd kime
dmake
cd ..
copy kime\kime.exe .
copy kime\kimehook.dll .
