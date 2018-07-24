#include "utils.h"
#include <stdio.h>
#include <string.h>

#define BLOCK_SIZE  8
#define BLOCK_TSIZE 64

static inline int write_byte (uint8_t symbol, uint8_t size, uint8_t* buffer, int* p)
{
    int ret = 0;
    while (size > 0)
    {
        uint8_t* b = &buffer[(*p) / 8];
        int8_t shift = 8 - (size + (*p) % 8);
        if (shift >= 0)
        {
            *b |= symbol << shift;
            *p += size;
            ret += size;
            size = 0;
        }
        else // shift < 0
        {
            int8_t step = size + shift;
            *b  |= (symbol >> -shift) /*& ~(0xFFFF << step)*/;
            *p  += step;
            ret += step;
            size = -shift;
            symbol &= ~(0xFF << shift);
        }
    }
    return ret;
}

static inline uint8_t read_byte (uint8_t* buffer, int* p, uint8_t size)
{
    uint16_t v = 0;
    while (size != 0)
    {
        uint8_t b = buffer[(*p) / 8];
        int8_t shift = 8 - size - (*p) % 8;

        if (shift >= 0)
        {
            b >>= shift;
            v |= b & ~(0xFF << size);
            *p += size;
            size = 0;
        }
        else // shift < 0
        {
            b <<= -shift;
            v |= b & ((~(0xFF << (size + shift))) << -shift);
            *p += size + shift;
            size = -shift;
        }
    }
    return v;
}

inline int write_bits (uint16_t symbol, uint8_t size, uint8_t* buffer, int* p)
{
    int ret = 0;
    if (size > 8)
    {
        // write upper bits
        ret += write_byte ((uint8_t) (symbol >> 8), size - 8, buffer, p);
        size = 8;
    }
    // write lower bits
    ret += write_byte ((uint8_t) (symbol & 0x00FF), size, buffer, p);
    return ret;
}

inline uint16_t read_value (uint8_t* buffer, int* p, uint8_t size)
{
    uint16_t v = 0;
    if (size > 8)
    {
        // read upper bits
        v |= (read_byte (buffer, p, size - 8)) << 8;
        size = 8;
    }
    // read lower bits
    v |= read_byte (buffer, p, size);
    return v;
}

int load_file (const char* source, void** destination, size_t* size)
{
    FILE* fp = fopen (source, "rb");
    if (fp == NULL)
        return 1;

    // get size of source file
    fseek (fp, 0, SEEK_END);
    *size = ftell (fp) + 1;
    fseek (fp, 0, SEEK_SET);

    // allocate buffer to contain source
    *destination = malloc (*size);
    memset (*destination, 0, *size);

    // read contents and close file
    int n = fread (*destination, 1, (*size) - 1, fp);
    if (n != (*size) - 1)
        return 1;

    fclose (fp);
    return 0;
}

void print_binary (uint8_t* buffer, int p, int size)
{
    for (int i = 0; i < size; i ++)
    {
        uint8_t b = buffer[(p + i) / 8];
        b <<= (p + i) % 8;
        printf ("%s", b & 0x80 ? "1" : "0");
    }
}

void print_block_p (int16_t* block)
{
    for (int i = 0; i < BLOCK_SIZE; i ++)
    {
        for (int j = 0; j < BLOCK_SIZE; j ++)
            printf ("%6d ", block[i * BLOCK_SIZE + j]);
        printf ("\n");
    }
}

void print_block (int16_t* block[8])
{
    for (int i = 0; i < BLOCK_SIZE; i ++)
    {
        for (int j = 0; j < BLOCK_SIZE; j ++)
            printf ("%6d ", block[i][j]);
        printf ("\n");
    }
}

void print_coefs (float block[8][8])
{
    for (int i = 0; i < BLOCK_SIZE; i ++)
    {
        for (int j = 0; j < BLOCK_SIZE; j ++)
            printf ("%10.2f ", block[i][j]);
        printf ("\n");
    }
}

void print_coefs_p (float* block)
{
    for (int i = 0; i < BLOCK_SIZE; i ++)
    {
        for (int j = 0; j < BLOCK_SIZE; j ++)
            printf ("%10.2f ", block[i * BLOCK_SIZE + j]);
        printf ("\n");
    }
}
