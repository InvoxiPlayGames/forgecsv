// Forge binary CSV file ImHex pattern
// by Emma / InvoxiPlayGames, 2026
// 
// Changelog:
//    v0 - Initial pattern.

#pragma author InvoxiPlayGames
#pragma description Forge binary CSV file
#pragma magic [ 01 00 00 00 02 00 00 00 ] @ 0x00
#pragma endian little
//change to #pragma endian big for BE platforms

struct CsvResourceHeader {
  u32 mRevision;
  u32 mUnk;
  u8 mSeperator;
  u32 mStringTableLength;
};

struct CsvRow {
  u32 mColumns;
  u32 mColumnStrings[mColumns];
};

struct CsvResource {
  CsvResourceHeader mResourceHeader;
  char mStringTable[mResourceHeader.mStringTableLength];
  CsvRow mHeaderRow;
  u32 mNumRows;
  CsvRow mRows[mNumRows];
};

CsvResource csv @ 0x0;

