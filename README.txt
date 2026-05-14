forgecsv converts between regular CSV files and the binary CSV format as used
in Harmonix Forge engine games (RB4, RBVR, etc)

usage:
  ./forgecsv bin2csv /path/to/input.csv[_xb1|_ps4|_pc] /path/to/output.csv
  ./forgecsv csv2bin /path/to/input.csv /path/to/output.csv[_xb1|_ps4|_pc]

compiling (Linux, macOS):
  cc forgecsv.c csv.c -o forgecsv
compiling (Windows/Mingw64):
  cc -D_WIN32_WINNT=0x0603 forgecsv.c csv.c -o forgecsv.exe

licensed under MIT (see LICENSE.txt)

uses csv-fast-reader (https://github.com/jandoczy/csv-fast-reader), licensed
 under the MIT license
