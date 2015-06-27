/*
* Created by Dr. Ace Jeangle (support@chalk-elec.com) 
* Patches EDID information in Intel HEX firmware
*
* You can compile source with standard gcc compiler: 
* gcc main.c -o patch-edid
*
* How to use:
* patch-edid ./7-of-mt.hex ./1920x1200_edid.bin
* will update EDID data in 7-of-mt.hex firmware with data from 1920x1200_edid.bin file
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char edid[256];
unsigned char* hex;
unsigned char* hexOut;
unsigned char signature[10]={0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x17,0x10};
int edidLen=0;
int hexLen=0;
int pos=0;
int cnt=0;

// "getline" implementation
size_t getline(unsigned char **lineptr, size_t *n, FILE *stream) {
    unsigned char *bufptr = NULL;
    unsigned char *p = bufptr;
    size_t size;
    int c;

    if (lineptr == NULL) {
        return -1;
    }
    if (stream == NULL) {
        return -1;
    }
    if (n == NULL) {
        return -1;
    }
    bufptr = *lineptr;
    size = *n;

    c = fgetc(stream);
    if (c == EOF) {
        return -1;
    }
    if (bufptr == NULL) {
        bufptr = malloc(128);
        if (bufptr == NULL) {
            return -1;
        }
        size = 128;
    }
    p = bufptr;
    while(c != EOF) {
        if ((p - bufptr) > (size - 1)) {
            size = size + 128;
            bufptr = realloc(bufptr, size);
            if (bufptr == NULL) {
                return -1;
            }
        }
        *p++ = c;
        if (c == '\n') {
            break;
        }
        c = fgetc(stream);
    }

    *p++ = '\0';
    *lineptr = bufptr;
    *n = size;

    return p - bufptr - 1;
}

// Prints short program description and exit
void printUsage(char* str) {
    puts(str);
    puts("EDID patch tool");
    puts("Usage: patch-edid <firmware.hex> <edid.bin>\n");
    exit(-1);
}

// Read EDID file and perform several simple checks of EDID data, returns 0 if OK
int readEdid(FILE* f) {
    int i;
    unsigned char chk;

    fseek(f, 0, SEEK_END);
    edidLen = ftell(f);
    if ((edidLen != 128) && (edidLen != 256)) printUsage("Error: EDID bin-file length must be 128 or 256 bytes");
    fseek(f, 0, SEEK_SET);
    fread(edid, edidLen, 1, f);

    // Check length of EDID data
    if ((edidLen != 128) && (edidLen != 256)) return -1;

    // Check EDID signature
    for (i=0; i<8; i++) {
        if (edid[i] != signature[i]) return -1;
    }

    // Check EDID checksum
    for (i=0,chk=0; i<edidLen; i++) {
        chk += edid[i];
    }
    // Fill second 128-bytes block with 0 if edidLen=128
    for(; i<256; i++) edid[i] = 0;
    return chk;
}

// Convert pointer to text HEX byte (pHex) to digital value (res), return 0 if OK
int hex2uchar(unsigned char* pHex, unsigned char* res) {
    *res = 0;
    if ((pHex[0] >= 'A') && (pHex[0] <= 'F')) *res = (pHex[0] - 'A' + 10) << 4;
    else if ((pHex[0] >= '0') && (pHex[0] <= '9')) *res = (pHex[0] - '0') << 4;
    else return -1;
    if ((pHex[1] >= 'A') && (pHex[1] <= 'F')) *res += (pHex[1] - 'A' + 10);
    else if ((pHex[1] >= '0') && (pHex[1] <= '9')) *res += (pHex[1] - '0');
    else return -1;
    return 0;
}

// Convert digital value (val) to text HEX byte (pHex)
void uchar2hex(unsigned char val, unsigned char* pHex) {
    unsigned char tmp;
    tmp = val >> 4;
    if (tmp > 9) pHex[0] = (tmp + 'A' - 10);
    else pHex[0] = tmp + '0';
    tmp = val & 0x0F;
    if (tmp > 9) pHex[1] = (tmp + 'A' - 10);
    else pHex[1] = tmp + '0';
}

// Find the start of the first occurrence of the EDID signature in hex array, return position or -1 if not found
int findHex() {
    int i,j;
    unsigned char chk;
    for (i=0; i < (cnt-256); i++) {
        if (hex[i] == signature[0]) {
            chk = hex[i];
            for (j=1; j<256; j++) {
                if ((j<10) && (hex[i+j] != signature[j])) break;
                chk += hex[i+j];
            }
            if ((j == 256) && (chk == 0)) return (i+1);
        }
    }
    return -1;
}

// Read hex-file and find position of old EDID, returns 0 if file format is OK
int readHex(FILE* f) {
    unsigned char* line = NULL;
    size_t len = 0;
    ssize_t read;
    unsigned char tmp;
    int i;

    // Calculate length of hex-file
    fseek(f, 0, SEEK_END);
    hexLen = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (hexLen == 0) return -1;

    hex = malloc(hexLen/2);
    hexOut = malloc(hexLen);
    cnt = 0;

    // Convert HEX to binary and store in "hex" array
    while ((read = getline(&line, &len, f)) != -1) {
        // Check line for Intel HEX format rules
        if (line[0] != ':') return -1;
        if (hex2uchar(line+1, &tmp)) return -1;
        if ((2*tmp + 11) != (read - 1)) return -1;
        for (i=9; i < (read - 4); i+=2) {
            if (hex2uchar(line+i, &tmp)) return -1;
            hex[cnt++] = tmp;
        }
    }
    pos = findHex();
    if (pos > 0) printf("Found EDID data in HEX file, patching ...\n");

    if (line) free(line);
    if (hex) free(hex);
    fseek(f, 0, SEEK_SET);
    return 0;
}

// Patch hex-file with new EDID data
void patchHex(FILE* f) {
    unsigned char* line = NULL;
    size_t len = 0;
    ssize_t read;
    unsigned char tmp,chk;
    int i,j,c;

    j = 0; c = 0;
    while ((read = getline(&line, &len, f)) != -1) {
        chk = 0;
        hexOut[j++] = ':';
        for (i=1; i < (read - 4); i+=2) {
            hex2uchar(line+i, &tmp);
            if (i >= 9) {
                c++;
                if ((c >= pos) && (c < (pos+256))) tmp = edid[c-pos];
            }
            chk += tmp;
            uchar2hex(tmp, hexOut+j);
            j += 2;
        }
        chk = ~chk + 1;
        uchar2hex(chk, hexOut+j); j += 2;
        hexOut[j++] = 0x0D; hexOut[j++] = 0x0A;
    }
    printf("patching done!\n");
    FILE* ff=fopen("log.hex","wb");
    fwrite(hexOut,1,j,ff);
    fclose(ff);

    if (line) free(line);
    if (hexOut) free(hexOut);
}


int main(int argc, char** argv) {
    FILE* hexFile;
    FILE* edidFile;

    edidLen=0;
    puts("");

    if (argc != 3) printUsage("");

    hexFile = fopen(argv[1], "r+");
    if (hexFile == NULL) printUsage("Error: can't open firmware hex-file");
    edidFile = fopen(argv[2], "rb");
    if (edidFile == NULL) printUsage("Error: can't open EDID bin-file");

    if (readEdid(edidFile) != 0) printUsage("Error: EDID bin-file does not conform to EDID standard");
    if (readHex(hexFile) != 0) printUsage("Error: firmware hex-file does not conform to Intel HEX format");
    patchHex(hexFile);

    fclose(hexFile);
    fclose(edidFile);
    return 0;
}
