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
    if (offset >= current_string_table_sz) {
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

    return 0;
}

int txtrow_to_binrow(CsvHandle csv, char *row, FILE *bin) {
    // TODO(Emma)
    fprintf(stderr, "%s: function not implemented.\n", __func__);
    return -1;
}

int csv_to_bincsv(const char *csv, const char *bincsv) {
    // TODO(Emma)
    fprintf(stderr, "%s: function not implemented.\n", __func__);
    return -1;
}

int print_usage(const char *filename) {
    printf("usage:\n");
    printf("  %s bin2csv /path/to/input.csv[_xb1|_ps4|_pc] /path/to/output.csv\n", filename);
    //printf("  %s csv2bin /path/to/input.csv /path/to/output.csv[_xb1|_ps4|_pc]\n", filename);
    return -1;
}

int main(int argc, const char **argv) {
    if (argc < 4)
        return print_usage(argv[0]);

    const char *verb = argv[1];
    if (strcasecmp(verb, "bin2csv") == 0) {
        return bincsv_to_csv(argv[2], argv[3]);
    } else if (strcasecmp(verb, "csv2bin") == 0) {
        return bincsv_to_csv(argv[2], argv[3]);
    } else {
        fprintf(stderr, "invalid verb '%s'\n", verb);
        return print_usage(argv[0]);
    }
}
