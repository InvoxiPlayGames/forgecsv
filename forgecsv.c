#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "csv.h"

#define DEFAULT_SEPERATOR ','

char *current_string_table = NULL;
uint32_t current_string_table_sz = 0;
uint32_t current_string_table_ptr = 0;
char current_seperator = DEFAULT_SEPERATOR;
static char quote = '"';

typedef struct _CsvResourceHeader {
    uint32_t mRevision;
    uint32_t mUnk;
    uint8_t mSeperator;
    uint32_t mStringTableLength;
} __attribute__((packed)) CsvResourceHeader;

const char *get_string_from_table(uint32_t offset) {
    if (current_string_table == NULL) {
        fprintf(stderr, "%s: no string table loaded.\n", __func__);
        return NULL;
    }
    if (offset > current_string_table_sz) {
        fprintf(stderr, "%s: offset (0x%x) larger than string table (0x%x).\n", __func__, offset, current_string_table_sz);
        return NULL;
    }
    return current_string_table + offset;
}

int binrow_to_txtrow(FILE *bin, FILE *txt) {
    char newln[] = { '\r', '\n' };
    int r = 0;
    uint32_t num_cols = 0;
    uint32_t *columns = NULL;

    // if we don't have a string table, bail out
    if (current_string_table == NULL) {
        fprintf(stderr, "%s: no string table loaded.\n", __func__);
        return -1;
    }

    // read the number of columns from the file
    num_cols = 0;
    r = fread(&num_cols, sizeof(int), 1, bin);
    if (r != 1 || num_cols < 0) { // sanity check
        fprintf(stderr, "%s: failed to read number of columns.\n", __func__);
        return -1;
    }

    // allocate enough room for the columns in memory temporarily
    columns = malloc(sizeof(uint32_t) * num_cols);
    if (columns == NULL) {
        fprintf(stderr, "%s: failed to allocate memory for columns.\n", __func__);
        return -1;
    }
    r = fread(columns, sizeof(uint32_t), num_cols, bin);
    if (r != num_cols) {
        fprintf(stderr, "%s: failed to read columns.\n", __func__);
        free(columns);
        return -1;
    }

    // write the column text data out to the text file
    for (int i = 0; i < num_cols; i++) {
        int str_esc_len = 0;
        const char *str = NULL;
        char *str_esc = NULL;
        int j = 0;
        int k = 0;

        str = get_string_from_table(columns[i]);
        if (str == NULL) {
            fprintf(stderr, "%s: failed to read column from string table.\n", __func__);
            free(columns);
            return -1;
        }

        // get the size of the string when escaped
        for (j=0; str[j]; str[j]==quote ? str_esc_len += 2 : str_esc_len++, j++);
        // and allocate a buffer for it
        str_esc = malloc(str_esc_len + 1);
        if (str_esc == NULL) {
            fprintf(stderr, "%s: failed to allocate buffer for escaped string.\n", __func__);
            free(columns);
            return -1;
        }
        // and copy the string, escaping it
        k = 0;
        for (j = 0; str[j]; j++) {
            if (str[j] == quote)
                str_esc[k++] = quote;
            str_esc[k++] = str[j];
        }
        str_esc[k] = '\0';

        // TODO(Emma): check if the writes fail here
        fwrite(&quote, 1, sizeof(char), txt);
        fwrite(str_esc, 1, strlen(str_esc), txt);
        fwrite(&quote, 1, sizeof(char), txt);
        // write the seperator if it isn't
        if (i < (num_cols - 1))
            fwrite(&current_seperator, 1, sizeof(char), txt);
        
        free(str_esc);
    }

    // write a CRLF newline
    fwrite(newln, sizeof(newln), 1, txt);

    // successfully read and output - free the buffer we made before
    free(columns);
    return 0;
}

int bincsv_to_csv(const char *bincsv_file, const char *csv_file) {
    FILE *bincsv = NULL;
    FILE *csv = NULL;
    CsvResourceHeader resHdr = {0};
    int r = 0;
    int num_rows = 0;

    // open the files
    bincsv = fopen(bincsv_file, "rb");
    if (bincsv == NULL) {
        fprintf(stderr, "%s: failed to read from '%s'.\n", __func__, bincsv_file);
        return -1;
    }
    csv = fopen(csv_file, "wb"); // load as binary; even tho it's text data, i don't trust it
    if (csv == NULL) {
        fprintf(stderr, "%s: failed to open '%s' for writing.\n", __func__, csv_file);
        fclose(bincsv);
        return -1;
    }

    // read the file header
    r = fread(&resHdr, sizeof(CsvResourceHeader), 1, bincsv);
    if (r != 1) {
        fprintf(stderr, "%s: failed to read CsvResource header.\n", __func__);
        fclose(bincsv);
        fclose(csv);
        return -1;
    }
    // check the version
    if (resHdr.mRevision != 0x1 || resHdr.mUnk != 0x2) {
        fprintf(stderr, "%s: unknown CsvResource type (0x%x, 0x%x)\n", __func__, resHdr.mRevision, resHdr.mUnk);
        fclose(bincsv);
        fclose(csv);
        return -1;
    }
    current_seperator = resHdr.mSeperator;

    // read the string table
    current_string_table_sz = resHdr.mStringTableLength;
    current_string_table = malloc(resHdr.mStringTableLength);
    if (current_string_table == NULL) {
        fprintf(stderr, "%s: failed to allocate string table of size 0x%x\n", __func__, current_string_table_sz);
        fclose(bincsv);
        fclose(csv);
        return -1;
    }
    r = fread(current_string_table, 1, current_string_table_sz, bincsv);
    if (r != current_string_table_sz) {
        fprintf(stderr, "%s: failed to read string table from file.\n", __func__);
        fclose(bincsv);
        fclose(csv);
        free(current_string_table);
        current_string_table = NULL;
        return -1;
    }

    // read the header row
    if (binrow_to_txtrow(bincsv, csv) < 0) {
        fprintf(stderr, "%s: failed to read header row from file.\n", __func__);
        fclose(bincsv);
        fclose(csv);
        free(current_string_table);
        current_string_table = NULL;
        return -1;
    }

    // read out all the rowsnum_cols = 0;
    r = fread(&num_rows, sizeof(int), 1, bincsv);
    if (r != 1 || num_rows < 0) { // sanity check
        fprintf(stderr, "%s: failed to read number of rows.\n", __func__);
        return -1;
    }
    for (int i = 0; i < num_rows; i++) {
        if (binrow_to_txtrow(bincsv, csv) < 0) {
            fprintf(stderr, "%s: failed to read row %i from file.\n", __func__, i);
            fclose(bincsv);
            fclose(csv);
            free(current_string_table);
            current_string_table = NULL;
            return -1;
        }
    }

    fclose(bincsv);
    fclose(csv);
    free(current_string_table);
    current_string_table = NULL;
    return 0;
}

uint32_t add_string_to_table(const char *string) {
    if ((current_string_table_ptr + strlen(string) + 1) > current_string_table_sz) {
        fprintf(stderr, "%s: bounds check failed adding string of length 0x%lx (ptr 0x%x, sz 0x%x)\n", __func__, strlen(string) + 1, current_string_table_ptr, current_string_table_sz);
        return 0xFFFFFFFF;
    }

    uint32_t ptr = current_string_table_ptr;
    strcpy(current_string_table + current_string_table_ptr, string);
    current_string_table_ptr += strlen(string) + 1;
    return ptr;
}

#define MAX_COLS 16
int txtrow_to_binrow(CsvHandle csv, char *row, FILE *bin) {
#ifndef __APPLE__
    int num_cols = 0;
    uint32_t cols[MAX_COLS] = {0}; // max 16 columns isn't great, but fuck it
    const char *col = NULL;
    while (col = CsvReadNextCol(row, csv)) {
        cols[num_cols] = add_string_to_table(col);
        if (cols[num_cols] == 0xFFFFFFFF) {
            fprintf(stderr, "%s: failed to add string to table!\n", __func__);
            return -1;
        }
        num_cols++;
        if (num_cols >= MAX_COLS) {
            fprintf(stderr, "%s: exceeded maximum columns limit.\n", __func__);
            return -1;
        }
    }
    // TODO(Emma): check for errors when writing
    fwrite(&num_cols, sizeof(int), 1, bin);
    fwrite(cols, sizeof(uint32_t), num_cols, bin);
    return 0;
#else
    // TODO(Emma): csv.c doesn't work on macOS
    fprintf(stderr, "%s: function not implemented on this platform.\n", __func__);
    return -1;
#endif
}

int csv_to_bincsv(const char *csv_file, const char *bincsv_file) {
#ifndef __APPLE__
    int r = 0;
    // This sucks - we read the CSV in its entirety twice to get the size of symbol table,
    //  and number of rows, and then to actually insert those entries to the table.
    char *row = NULL;
    int num_rows = 0;
    uint32_t str_table_size = 0;
    CsvHandle csv = CsvOpen2(csv_file, current_seperator, '"', '\\');
    if (csv == NULL) {
        fprintf(stderr, "%s: failed to open csv file from '%s'.\n", __func__, csv_file);
        return -1;
    }
    while (row = CsvReadNextRow(csv)) {
        const char *col = NULL;
        num_rows++;
        while (col = CsvReadNextCol(row, csv))
            str_table_size += strlen(col) + 1;
    }
    CsvClose(csv);
    
    // actually allocate the string table
    current_string_table_sz = str_table_size;
    current_string_table = malloc(str_table_size);
    if (current_string_table == NULL) {
        fprintf(stderr, "%s: failed to allocate string table.\n", __func__);
        return -1;
    }
    memset(current_string_table, 0, str_table_size);

    FILE *bincsv = fopen(bincsv_file, "wb");
    if (bincsv == NULL) {
        fprintf(stderr, "%s: failed to open '%s' for writing.\n", __func__, bincsv_file);
        free(current_string_table);
        current_string_table = NULL;
        return -1;
    }

    // build the resource header and write it to the file
    CsvResourceHeader resHdr = {0};
    resHdr.mRevision = 0x1;
    resHdr.mUnk = 0x2;
    resHdr.mSeperator = current_seperator;
    resHdr.mStringTableLength = str_table_size;
    r = fwrite(&resHdr, sizeof(CsvResourceHeader), 1, bincsv);
    if (r != 1) {
        fprintf(stderr, "%s: failed to write header to file.\n", __func__);
        fclose(bincsv);
        free(current_string_table);
        current_string_table = NULL;
        return -1;
    }
    // skip past the size of the string table - we'll write it later
    fseek(bincsv, str_table_size, SEEK_CUR);

    // read the CSV again and write out each of the lines
    int num_rows_done = 0;
    csv = CsvOpen2(csv_file, current_seperator, '"', '\\');
    if (csv == NULL) {
        fprintf(stderr, "%s: failed to open csv file from '%s'.\n", __func__, csv_file);
        fclose(bincsv);
        free(current_string_table);
        current_string_table = NULL;
        return -1;
    }
    while (row = CsvReadNextRow(csv)) {
        // TODO(Emma): properly check for errors
        if (txtrow_to_binrow(csv, row, bincsv) < 0) {
            fprintf(stderr, "%s: failed on row %i ('%s')\n", __func__, num_rows_done, row);
            CsvClose(csv);
            fclose(bincsv);
            free(current_string_table);
            current_string_table = NULL;
            return -1;
        }
        // after the header row, there's a count of the rest of the rows
        if (num_rows_done == 0) {
            int num_rows_file = num_rows - 1; // don't include header row
            fwrite(&num_rows_file, sizeof(int), 1, bincsv);
        }
        num_rows_done++;
    }
    CsvClose(csv);

    // write the string table to the file
    fseek(bincsv, sizeof(CsvResourceHeader), SEEK_SET);
    r = fwrite(current_string_table, 1, current_string_table_sz, bincsv);
    if (r != current_string_table_sz) {
        fprintf(stderr, "%s: failed to write string table to file.\n", __func__);
        fclose(bincsv);
        free(current_string_table);
        current_string_table = NULL;
        return -1;
    }

    free(current_string_table);
    fclose(bincsv);
    current_string_table = NULL;
    return 0;
#else
    // TODO(Emma): csv.c doesn't work on macOS
    fprintf(stderr, "%s: function not implemented on this platform.\n", __func__);
    return -1;
#endif
}

int print_usage(const char *filename) {
    printf("usage:\n");
    printf("  %s bin2csv /path/to/input.csv[_xb1|_ps4|_pc] /path/to/output.csv\n", filename);
#ifndef __APPLE__
    // TODO(Emma): csv.c doesn't work on macOS
    printf("  %s csv2bin /path/to/input.csv /path/to/output.csv[_xb1|_ps4|_pc]\n", filename);
#endif
    return -1;
}

int main(int argc, const char **argv) {
    if (argc < 4)
        return print_usage(argv[0]);

    const char *verb = argv[1];
    if (strcasecmp(verb, "bin2csv") == 0) {
        return bincsv_to_csv(argv[2], argv[3]);
    } else if (strcasecmp(verb, "csv2bin") == 0) {
        return csv_to_bincsv(argv[2], argv[3]);
    } else {
        fprintf(stderr, "invalid verb '%s'\n", verb);
        return print_usage(argv[0]);
    }
}
