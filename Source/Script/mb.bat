@echo off
rem set bison_simple=parser.cpp
set bison_simple=dummy
bison\bison.exe -o nul c_gram.cpp

