forgecsv converts between regular CSV files and the binary CSV format as used
in Harmonix Forge engine games (RB4, RBVR, etc)

usage:
  ./forgecsv bin2csv /path/to/input.csv[_xb1|_ps4|_pc] /path/to/output.csv
  ./forgecsv csv2bin /path/to/input.csv /path/to/output.csv[_xb1|_ps4|_pc]

compiling:
  cc forgecsv.c csv.c -o forgecsv

platform notes:
  - requires Mingw64 on Windows
  - csv.c (thus re-encoding) does not work on macOS

licensed under MIT (see LICENSE.txt)

uses csv-fast-reader (https://github.com/jandoczy/csv-fast-reader), licensed
 under the MIT license
