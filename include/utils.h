#ifndef _UTILS_H
#define _UTILS_H

#include <stdint.h>
#include <stdlib.h>

/**
 *  Write size bits from symbol to buffer at position p.
 *  Increments p to size when done.
 *  The function assumes that the bits are right aligned in symbol.
 */
int write_bits (uint16_t symbol, uint8_t size, uint8_t* buffer, int* p) ;

/**
 *  Read a value that has is the next size bits in buffer starting
 *  from bit pointer p;
 *  Increments pointer p past size bits.
 */
uint16_t read_value (uint8_t* buffer, int* p, uint8_t size) ;

/**
 *  Print binary representation of buffer for size bits starting from
 *  bit p.
 *  Used for debugging.
 */
void print_binary (uint8_t* buffer, int p, int size) ;

/**
 *  Print block.
 */
void print_block_p (int16_t* block) ;
void print_block   (int16_t* block[8]) ;

/**
 *  Print coefs.
 */
void print_coefs (float block[8][8]) ;
void print_coefs_p (float* block);

/**
 *  Load a file contents into buffer.
 *  The buffer needs to manually be freed later.
 */
int load_file (const char* /* file path */, void** /* destination */, size_t* /* size */ );

#endif /* _UTILS_H */
