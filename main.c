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
unsigned char* hexFull;
unsigned char signature[10]={0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x17,0x10};
int edidLen=0;
int hexLen=0;
int pos=0;
int cnt=0;

// Prints short program description and exit
void printUsage(char* str) {
    puts(str);
    puts("EDID patch tool");
    puts("Usage: patch-edid <firmware.hex> <edid.bin>\n");
    if (hex) free(hex);
    if (hexFull) free(hexFull);
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
    unsigned char tmp;
    int i,j;

    // Calculate length of hex-file
    fseek(f, 0, SEEK_END);
    hexLen = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (hexLen == 0) return -1;

    hex = malloc(hexLen/2);
    hexFull = malloc(hexLen);
    fread(hexFull, 1, hexLen, f);
    cnt = 0;

    // Convert HEX to binary and store in "hex" array
    for (i = 0; i < hexLen;) {
        for (j = i; j < hexLen; j++) {
            if ((hexFull[j] == 0x0D) || (hexFull[j] == 0x0A)) break;
        }
        // Check line for Intel HEX format rules
        if (hexFull[i] != ':') return -1;
        if (hex2uchar(hexFull+i+1, &tmp)) return -1;
        if ((2*tmp + 10) != (j - i - 1)) return -1;
        for (i += 9; i < (j-2); i += 2) {
            if (hex2uchar(hexFull+i, &tmp)) return -1;
            hex[cnt++] = tmp;
        }
        i += 2;
        while(i < hexLen) {
            if ((hexFull[i] == 0x0D) || (hexFull[i] == 0x0A)) i++;
            else break;
        }
    }
    return 0;
}

// Patch hex-file with new EDID data
void patchHex(FILE* f) {
    unsigned char tmp,chk;
    int i,j,c;

    j = 0; c = 0; chk = 0;
    for (i = 0; i < hexLen;) {
        if (hexFull[i] != ':') {
            i++;
            continue;
        }
        i++;

        hex2uchar(hexFull+i,   &tmp); chk = tmp; j = tmp;
        hex2uchar(hexFull+i+2, &tmp); chk += tmp;
        hex2uchar(hexFull+i+4, &tmp); chk += tmp;
        hex2uchar(hexFull+i+6, &tmp); chk += tmp;
        i += 8;
        j = i + j*2;

        for (; i<j; i+=2) {
            hex2uchar(hexFull+i, &tmp);
            c++;
            if ((c >= pos) && (c < (pos+256))) tmp = edid[c-pos];
            chk += tmp;
            uchar2hex(tmp, hexFull+i);
        }

        chk = ~chk + 1;
        uchar2hex(chk, hexFull+j);
    }

    printf("patching done!\n");

    fwrite(hexFull,1,hexLen,f);
}


int main(int argc, char** argv) {
    FILE* hexFile;
    FILE* edidFile;

    edidLen=0;
    puts("");

    if (argc != 3) printUsage("");

    hexFile = fopen(argv[1], "rb");
    if (hexFile == NULL) printUsage("Error: can't open firmware hex-file!");
    edidFile = fopen(argv[2], "rb");
    if (edidFile == NULL) printUsage("Error: can't open EDID bin-file!");

    if (readEdid(edidFile) != 0) printUsage("Error: EDID bin-file does not conform to EDID standard!");
    if (readHex(hexFile) != 0) printUsage("Error: firmware hex-file does not conform to Intel HEX format!");
    pos = findHex();
    if (pos > 0) {
        printf("Found EDID data in HEX file, patching ...\n");
        fclose(hexFile);
        hexFile = fopen(argv[1], "wb");
        patchHex(hexFile);
    }
    else {
        printUsage("Error: Can't find EDID data in HEX file!\n");
    }

    if (hex) free(hex);
    if (hexFull) free(hexFull);

    fclose(hexFile);
    fclose(edidFile);
    return 0;
}
