#ifndef _DCT_H
#define _DCT_H

#include <stdint.h>

/**
 *  2-D Cosine Transform.
 *  Naive and very slow version.
 */
void dct  (int16_t* block, float destination[8][8]) ;

/**
 *  Inverse 2-D Cosine Transform.
 *  Naive and very slow version.
 */
void idct (int16_t* block, float destination[8][8]) ;

/**
 *  Fast Cosine Transform.
 *  Algorithm by Feig & Winograd. This is the scaled version.
 *  (not working, not sure if it needs something else to be correct)
 */
void fdct (int16_t* block, float destination[8][8]) ;

/**
 *  Fast 2-D Cosine Transform.
 *  Factorized version of the Feig & Winograd algorithm
 *  which applies 1-D transform on columns first and then on the rows.
 */
void fdct2  (int16_t* block, float destination[8][8]) ;

/**
 *  Fast Inversed 2-D Cosine Transform.
 *  Factorized version of the Feig & Winograd algorithm
 *  which applies 1-D transform on columns first and then on the rows.
 */
void ifdct2 (int16_t* block, float destination[8][8]) ;

#endif
