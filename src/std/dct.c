#include "dct.h"
#include <string.h>

#include "utils.h"
#include <stdio.h>
#include <math.h>


#define BLOCK_SIZE  8
#define BLOCK_TSIZE 64
#define PI          3.1415926535897 // 932384626433
#define SQR2        1.4142135623731 // 0.707106781 //
#define N           0.25 // jag tycker det ska vara 0.5, men vi kör på vad alla andra säger för nu.

/**
 *  Constants.
 *  Ga(x) = cos (pi * x / 16)
 */
#define Ga1         0.980785280403230
#define Ga2         0.923879532511287
#define Ga3         0.831469612302545
#define Ga4         0.707106781186548
#define Ga5         0.555570233019602
#define Ga6         0.382683432365090
#define Ga7         0.195090322016128

#define inv_Ga1     1.019591158208319
#define inv_Ga2     1.082392200292394
#define inv_Ga3     1.202689773870091
#define inv_Ga5     1.799952446272832
#define inv_Ga6     2.613125929752751
#define inv_Ga7     5.125830895483019

#define inv_Ga1__2  0.509795579104159
#define inv_Ga2__2  0.541196100146197
#define inv_Ga3__2  0.601344886935045
#define inv_Ga5__2  0.899976223136416
#define inv_Ga6__2  1.306562964876375
#define inv_Ga7__2  2.56291544774151

#define inv_Ga1__4  0.254897789552079
#define inv_Ga2__4  0.270598050073099
#define inv_Ga3__4  0.30067244346752
#define inv_Ga5__4  0.449988111568208
#define inv_Ga6__4  0.653281482438187
#define inv_Ga7__4  1.281457723870755

#define Ga2__2      0.461939766255643
#define Ga6__2      0.191341716182545
#define Ga4__2      0.353553390593274

#define Ga4_sqr2    0.500000000000001

/**
 *  Vector operators.
 *  ADD, SUB, INV and MUL by a coefficient.
 */
#define add(col1, col2, destination) \
    for (int i = 0; i < BLOCK_SIZE; i ++) destination[i] = col1[i] + col2[i];
#define sub(col1, col2, destination) \
    for (int i = 0; i < BLOCK_SIZE; i ++) destination[i] = col1[i] - col2[i];
#define mul(col1, coef, destination) \
    for (int i = 0; i < BLOCK_SIZE; i ++) destination[i] = col1[i] * coef;
#define inv(col, destination) \
    for (int i = 0; i < BLOCK_SIZE; i ++) destination[i] = - col[i];

/* normalization vector used in DCT to make coefficients orthogonal */
static float norm_vec[BLOCK_SIZE] = { 1.0 / SQR2, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };


/**
 *  Perform discrete cosine transform on a 8x8 block.
 *
 *  Just nu är detta 4 for-loopar. Det går ju fan inte. Får titta på FFT
 *  om det finns ett snabbare sätt att göra detta.
 */
void dct (int16_t* data, float coefficients[BLOCK_SIZE][BLOCK_SIZE])
{
    float c = 0.0;
    for (int j = 0; j < BLOCK_SIZE; j ++)
    {
        for (int i = 0; i < BLOCK_SIZE; i ++)
        {
            c = 0.0;
            // sum
            for (int y = 0; y < BLOCK_SIZE; y ++)
                for (int x = 0; x < BLOCK_SIZE; x ++)
                {
                    c += data[(y << 3) + x]
                         * cos (j * PI * ((y << 1) + 1) / 16.0)
                         * cos (i * PI * ((x << 1) + 1) / 16.0);
                }
            coefficients[j][i] = N * norm_vec[j] * norm_vec[i] * c;
        }
    }
}

/**
 *  Inverse a discrete cosine transform to an 8x8 block.
 */
void idct (int16_t* coefs, float block[BLOCK_SIZE][BLOCK_SIZE])
{
    float v;
    int i, j, x, y;
    for (j = 0; j < BLOCK_SIZE; j ++)
    {
        for (i = 0; i < BLOCK_SIZE; i ++)
        {
            v = 0.0;
            for (y = 0; y < BLOCK_SIZE; y ++)
            {
                for (x = 0; x < BLOCK_SIZE; x ++)
                {
                    v += norm_vec[y] * norm_vec[x] * coefs[(y << 3) + x]
                         * cos (PI * y * ((j << 1) + 1) / 16)
                         * cos (PI * x * ((i << 1) + 1) / 16);
                }
            }
            v *= N;
            if (v > 255)
                block[j][i] = 255;
            else if (v < 0)
                block[j][i] = 0;
            else
                block[j][i] = v;
        }
    }
}

/**
 *  Weig & Finnograd Fast 1-D DCT of a row.
 *  obs: corrupts data in row parameter so backup before if needed.
 *       also it assumes that multiplication by the B3 matrix has
 *       already been done.
 */
static void inline fdct1 (float row[BLOCK_SIZE], float destination[BLOCK_SIZE])
{
    float a1, a2, a3, a4;
    // assuming B3 has already been done...

    // B2
    a1 = row[0]; a2 = row[1]; a3 = row[2]; a4 = row[3];
    row[0] = a1 + a4;
    row[1] = a2 + a3;
    row[2] = a1 - a4;
    row[3] = a2 - a3;

    // B1
    destination[0] =   row[0] + row[1];
    destination[1] =   row[0] - row[1];
    destination[2] =   row[3];
    destination[3] =   row[2];
    destination[4] = - row[6];
    destination[5] =   row[7];
    destination[6] = - row[5];
    destination[7] = - row[4];

    // K
    //  1/2 G1
    row[0] = Ga4__2 * destination[0];
    row[1] = Ga4__2 * destination[1];
    //  1/2 G2
    a1 = Ga4 * (destination[3] - destination[2]);
    row[2] = inv_Ga6__4 * (destination[2] + a1);
    row[3] = inv_Ga2__4 * (a1 - destination[2]);
    //  1/2 G4
    //      H42
    row[4] = destination[4];
    row[5] = destination[5] + destination[7];
    row[6] = destination[4] - destination[7];
    row[7] = destination[5] - destination[6];
    //      1,G1,G2
    row[5] *= Ga4;
    a1 = Ga4 * (row[7] - row[6]);
    a2 = inv_Ga6__2 * (row[6] + a1);
    a3 = inv_Ga2__2 * (a1 - row[6]);
    row[6] = a2;
    row[7] = a3;
    //      H41
    destination[4] =   row[4] + row[5] - row[6];
    destination[5] =   row[5] + row[7] - row[4];
    destination[6] = - row[4] - row[5] - row[6];
    destination[7] =   row[4] - row[5] + row[7];
    //      1/4 D4^-1
    row[4] = inv_Ga5__4 * destination[4];
    row[5] = inv_Ga1__4 * destination[5];
    row[6] = inv_Ga3__4 * destination[6];
    row[7] = inv_Ga7__4 * destination[7];

    // P
    destination[0] =   row[0];
    destination[1] = - row[4];
    destination[2] =   row[2];
    destination[3] = - row[5];
    destination[4] =   row[1];
    destination[5] = - row[7];
    destination[6] =   row[3];
    destination[7] =   row[6];
}


void fdct2 (int16_t* block, float destination[BLOCK_SIZE][BLOCK_SIZE])
{
    float row[BLOCK_SIZE];
    float tmp[BLOCK_SIZE][BLOCK_SIZE];
    int16_t* p = block;
    // transform columns
    for (int i = 0; i < BLOCK_SIZE; i ++, p ++)
    {
        // apply B3 already here
        row[0] = p[0]      + p[7 << 3];
        row[1] = p[1 << 3] + p[6 << 3];
        row[2] = p[2 << 3] + p[5 << 3];
        row[3] = p[3 << 3] + p[4 << 3];
        row[4] = p[0]      - p[7 << 3];
        row[5] = p[1 << 3] - p[6 << 3];
        row[6] = p[2 << 3] - p[5 << 3];
        row[7] = p[3 << 3] - p[4 << 3];
        // 1D transform
        fdct1 (row, tmp[i]);
    }
    // transform rows
    for (int i = 0; i < BLOCK_SIZE; i ++)
    {
        row[0] = tmp[0][i] + tmp[7][i];
        row[1] = tmp[1][i] + tmp[6][i];
        row[2] = tmp[2][i] + tmp[5][i];
        row[3] = tmp[3][i] + tmp[4][i];
        row[4] = tmp[0][i] - tmp[7][i];
        row[5] = tmp[1][i] - tmp[6][i];
        row[6] = tmp[2][i] - tmp[5][i];
        row[7] = tmp[3][i] - tmp[4][i];
        // 1D transform
        fdct1 (row, destination[i]);
    }
}

/**
 *  Weig & Finnograd, Fast Inverse 1-D DCT.
 *  obs: corrupts data in row parameter! (it uses it as a buffer)
 *       also assumes multiplication by the permutation matrix, P,
 *       has already been done prior to the row.
 */
static void inline ifdct1 (float row[BLOCK_SIZE], float destination[BLOCK_SIZE])
{
    float a1, a2, a3, a4, a5, a6, a7, a8;
    // K
    //      G1
    destination[0] = row[0] * Ga4__2;
    destination[1] = row[1] * Ga4__2;
    //      G2
    a1 = inv_Ga6__4 * row[2];
    a2 = inv_Ga2__4 * row[3];
    a3 = Ga4 * (a1 + a2);
    destination[2] = a1 - a2 - a3;
    destination[3] = a3;
    //      G4
    //          D4
    row[4] *= inv_Ga5;
    row[5] *= inv_Ga1;
    row[6] *= inv_Ga3;
    row[7] *= inv_Ga7;
    //          H42
    a1 =  row[4] - row[5] - row[6] + row[7];
    a2 =  row[4] + row[5] - row[6] - row[7];
    a3 = -row[4]          - row[6];
    a4 =           row[5]          + row[7];
    //          G1
    a2 *= Ga4;
    //          G2
    a5 = inv_Ga6__2 * a3;
    a6 = inv_Ga2__2 * a4;
    a7 = Ga4 * (a5 + a6);
    a8 = a5 - a6 - a7;
    //          1/2 x 1/2 H41
    destination[4] = .25 * (a1 + a8);
    destination[5] = .25 * (a2 + a7);
    destination[6] = .25 * -a7;
    destination[7] = .25 * (a2 - a8);

    // B1
    row[0] =   destination[0] + destination[1];
    row[1] =   destination[0] - destination[1];
    row[2] =   destination[3];
    row[3] =   destination[2];
    row[4] = - destination[7];
    row[5] = - destination[6];
    row[6] = - destination[4];
    row[7] =   destination[5];

    // B2
    a1 = row[0]; a2 = row[1]; a3 = row[2]; a4 = row[3];
    row[0] = a1 + a3;
    row[1] = a2 + a4;
    row[2] = a2 - a4;
    row[3] = a1 - a3;

    // B3
    destination[0] = row[0] + row[4];
    destination[1] = row[1] + row[5];
    destination[2] = row[2] + row[6];
    destination[3] = row[3] + row[7];
    destination[4] = row[3] - row[7];
    destination[5] = row[2] - row[6];
    destination[6] = row[1] - row[5];
    destination[7] = row[0] - row[4];
}


void ifdct2 (int16_t* block, float destination[BLOCK_SIZE][BLOCK_SIZE])
{
    float row[BLOCK_SIZE];
    float tmp[BLOCK_SIZE][BLOCK_SIZE];
    // transform columns
    int16_t* p = block;
    for (int i = 0; i < BLOCK_SIZE; i ++, p ++)
    {
        // apply permutation already here to speed it up as
        // we also apply transposing here
        row[0] =   p[0];
        row[1] =   p[4 << 3];
        row[2] =   p[2 << 3];
        row[3] =   p[6 << 3];
        row[4] = - p[1 << 3];
        row[5] = - p[3 << 3];
        row[6] =   p[7 << 3];
        row[7] = - p[5 << 3];
        ifdct1 (row, tmp[i]);
    }
    // transform rows
    for (int i = 0; i < BLOCK_SIZE; i ++)
    {
        // permutation
        row[0] =   tmp[0][i];
        row[1] =   tmp[4][i];
        row[2] =   tmp[2][i];
        row[3] =   tmp[6][i];
        row[4] = - tmp[1][i];
        row[5] = - tmp[3][i];
        row[6] =   tmp[7][i];
        row[7] = - tmp[5][i];
        ifdct1 (row, destination[i]);
    }
}


/**
 *  Multiply a 2-point vector by G2.
 */
static inline void multiply_g2 (int16_t x1, int16_t x2, float* y1, float* y2)
{
    float x = Ga4 * (x2 - x1);
    *y1 = (x1 + x)  * inv_Ga6__2;
    *y2 = (x  - x1) * inv_Ga2__2;
}

/**
 *  Multiply a column by M8.
 */
static inline void multiply_m8 (int16_t* col, float destination[8])
{
    // 1 and G1
    destination[0] = col[0];
    destination[1] = col[1];
    destination[2] = col[2];
    destination[3] = col[3] * Ga4;
    destination[4] = col[4];
    destination[5] = col[5] * Ga4;

    // G2
    multiply_g2 (col[6], col[7], &destination[6], &destination[7]);
}

/**
 *  Multiply the column by the kroener product of M8 x G1
 */
static inline void multiply_m8_g1 (int16_t* col, float destination[8])
{
    destination[0] = col[0] * Ga4;
    destination[1] = col[1] * Ga4;
    destination[2] = col[2] * Ga4;
    destination[3] = col[3] * Ga4_sqr2;
    destination[4] = col[4] * Ga4;
    destination[5] = col[5] * Ga4_sqr2;

    float x1 = Ga4 * col[6];
    float x2 = Ga4_sqr2 * (col[7] - col[6]);
    destination[6] = (x1 + x2) * inv_Ga6__2;
    destination[7] = (x2 - x1) * inv_Ga2__2;
}

/**
 *  Multiply the columns by the kroener product of M8 x G2
 *  (two columns are needed)
 */
static inline void multiply_m8_g2 (int16_t* cols, float col1[8], float col2[8])
{
    float x1, x2, x3, x4;
    // 1st, 2nd, 3rd and 5th pair are multiplied by G2
    multiply_g2 (cols[0], cols[8],  &col1[0], &col2[0]);
    multiply_g2 (cols[1], cols[9],  &col1[1], &col2[1]);
    multiply_g2 (cols[2], cols[10], &col1[2], &col2[2]);
    multiply_g2 (cols[4], cols[12], &col1[4], &col2[4]);

    // 4th and 6th pair are multiplied by ga(4) * G2
    x1 = Ga4 * cols[3];
    x2 = Ga4_sqr2 * (-cols[3] + cols[11]);
    col1[3] = ( x1 + x2) * inv_Ga6__2;
    col2[3] = (-x1 + x2) * inv_Ga2__2;

    x1 = Ga4 * cols[5];
    x2 = Ga4_sqr2 * (-cols[5] + cols[13]);
    col1[5] = ( x1 + x2) * inv_Ga6__2;
    col2[5] = (-x1 + x2) * inv_Ga2__2;

    // 7th and 8th are multiplied by G2 x G2
    x1 = Ga4__2 * (cols[6] - cols[15]);
    x2 = Ga4__2 * (cols[14] + cols[7]);
    x3 = 0.5    * (cols[6] + cols[15]);
    x4 = 0.5    * (cols[14] - cols[7]);

    col1[6] = -x1 + x2 + x3;
    col2[6] = -x1 - x2 + x4;
    col1[7] = -x1 - x2 - x4;
    col2[7] =  x1 - x2 + x3;
}

/**
 *  Multiply a column by B3.
 */
static inline void multiply_b3 (int16_t* col, int16_t* destination)
{
    destination[0] = col[0] + col[7];
    destination[1] = col[1] + col[6];
    destination[2] = col[2] + col[5];
    destination[3] = col[3] + col[4];
    destination[4] = col[0] - col[7];
    destination[5] = col[1] - col[6];
    destination[6] = col[2] - col[5];
    destination[7] = col[3] - col[4];
}

/**
 *  Multiply a column by B2.
 */
static inline void multiply_b2 (int16_t* col, int16_t* destination)
{
    destination[0] = col[0] + col[3];
    destination[1] = col[1] + col[2];
    destination[2] = col[0] - col[3];
    destination[3] = col[1] - col[2];
    destination[4] = col[4];
    destination[5] = col[5];
    destination[6] = col[6];
    destination[7] = col[7];
}

/**
 *  Multiply a column by B1.
 */
static inline void multiply_b1 (int16_t* col, int16_t* destination)
{
    destination[0] =  col[0] + col[1];
    destination[1] = -col[0] + col[1];
    destination[2] =  col[3];
    destination[3] =  col[2] - col[3];
    destination[4] = -col[6];
    destination[5] = -col[4] + col[7];
    destination[6] =  col[4] - col[6];
    destination[7] =  col[5] + col[7];
}

/**
 *  Multiply a column by R8,1.
 */
static inline void multiply_r81 (float col[BLOCK_SIZE], float destination[BLOCK_SIZE])
{
    destination[0] =  col[0];
    destination[1] =  col[1];
    destination[2] =  col[2] + col[3];
    destination[3] = -col[2] + col[3];
    destination[4] =  col[4] + col[5] - col[6];
    destination[5] = -col[4] + col[5] + col[7];
    destination[6] = -col[4] - col[5] - col[6];
    destination[7] =  col[4] - col[5] + col[7];
}

/**
 *  Multiply a column by D8.
 *  Applies permutation from P at the same time.
 */
static inline void multiply_d8 (float col[BLOCK_SIZE], float dest[BLOCK_SIZE])
{
    // we also apply permutation from P into this
    dest[0] =   col[0];
    dest[1] = - Ga5 * col[4];
    dest[2] =   Ga6 * col[2];
    dest[3] = - Ga1 * col[5];
    dest[4] =   Ga4 * col[1];
    dest[5] = - Ga7 * col[7];
    dest[6] =   Ga2 * col[3];
    dest[7] =   Ga3 * col[6];
}

void fdct (int16_t* block, float destination[BLOCK_SIZE][BLOCK_SIZE])
{
    float y[BLOCK_SIZE][BLOCK_SIZE];
    int16_t b[BLOCK_TSIZE];
    int16_t foo;

    // transpose block
    for (int i = 0; i < BLOCK_SIZE - 1; i ++)
        for (int j = i + 1; j < BLOCK_SIZE; j ++)
        {
            foo = block[i * BLOCK_SIZE + j];
            block[i * BLOCK_SIZE + j] = block[j * BLOCK_SIZE + i];
            block[j * BLOCK_SIZE + i] = foo;
        }
#ifdef VERBOSE
    printf ("transposed\n"); print_block_p (block); printf ("\n");
#endif

    // preadditions
    // B3
    for (int i = 0; i < BLOCK_TSIZE; i += BLOCK_SIZE)
        multiply_b3 (block + i, b + i);

    add (b,        (b + 56),  block);
    add ((b + 8),  (b + 48), (block + 8));
    add ((b + 16), (b + 40), (block + 16));
    add ((b + 24), (b + 32), (block + 24));
    sub (b,        (b + 56), (block + 32));
    sub ((b + 8),  (b + 48), (block + 40));
    sub ((b + 16), (b + 40), (block + 48));
    sub ((b + 24), (b + 32), (block + 56));

#ifdef VERBOSE
    printf ("B3\n"); print_block_p (block); printf ("\n");
#endif

    // B2
    for (int i = 0; i < BLOCK_TSIZE; i += BLOCK_SIZE)
        multiply_b2 (block + i, b + i);

    add (b,       (b + 24),  block);
    add ((b + 8), (b + 16), (block + 8));
    sub (b,       (b + 24), (block + 16));
    sub ((b + 8), (b + 16), (block + 24));
    memcpy ((block + 32), (b + 32), sizeof (int16_t) << 5);

#ifdef VERBOSE
    printf ("B2\n"); print_block_p (block); printf ("\n");
#endif

    // B~1
    for (int i = 0; i < BLOCK_TSIZE; i += BLOCK_SIZE)
        multiply_b1 (block + i, b + i);

    memcpy (block + 16, b + 24, sizeof (int16_t) << 3);
    add (b,        (b + 8),   block);
    sub ((b + 8),   b,       (block + 8));
    sub ((b + 16), (b + 24), (block + 24));
    inv ((b + 48),           (block + 32));
    sub ((b + 56), (b + 32), (block + 40));
    sub ((b + 32), (b + 48), (block + 48));
    add ((b + 40), (b + 56), (block + 56));

#ifdef VERBOSE
    printf ("B1\n"); print_block_p (block); printf ("\n");
#endif

    // multiply by M8
    multiply_m8    ( block,       destination[0]);
    multiply_m8    ((block + 8),  destination[1]);
    multiply_m8    ((block + 16), destination[2]);
    multiply_m8    ((block + 32), destination[4]);
    multiply_m8_g1 ((block + 24), destination[3]);
    multiply_m8_g1 ((block + 40), destination[5]);
    multiply_m8_g2 ((block + 48), destination[6],
                                  destination[7]);

#ifdef VERBOSE
    printf ("M8\n"); print_coefs (destination); printf ("\n");
#endif

    // postadditions
    for (int i = 0; i < BLOCK_SIZE; i ++)
        multiply_r81 (destination[i], y[i]);

    memcpy (destination[0], y[0], sizeof (float) << 3);
    memcpy (destination[1], y[1], sizeof (float) << 3);
    add (y[2], y[3], destination[2]);
    sub (y[3], y[2], destination[3]);
    add (y[4], y[5], destination[4]); sub (destination[4], y[6], destination[4]);
    sub (y[5], y[4], destination[5]); add (destination[5], y[7], destination[5]);
    inv (y[5], y[5]);
    sub (y[5], y[4], destination[6]); sub (destination[6], y[6], destination[6]);
    add (y[4], y[5], destination[7]); add (destination[7], y[7], destination[7]);

#ifdef VERBOSE
    printf ("R8,1\n"); print_coefs (destination); printf ("\n");
#endif

    // permute and quantize
    multiply_d8 (destination[0], y[0]);
    multiply_d8 (destination[4], y[1]);
    multiply_d8 (destination[2], y[2]);
    multiply_d8 (destination[5], y[3]);
    multiply_d8 (destination[1], y[4]);
    multiply_d8 (destination[7], y[5]);
    multiply_d8 (destination[3], y[6]);
    multiply_d8 (destination[6], y[7]);
    // inv (y[1], y[1]);
    // inv (y[3], y[3]);
    // inv (y[5], y[5]);
    mul (y[0],        0.50, y[0]);
    mul (y[1], -Ga5 * 0.25, y[1]);
    mul (y[2],  Ga6 * 0.25, y[2]);
    mul (y[3], -Ga1 * 0.25, y[3]);
    mul (y[4],  Ga4 * 0.25, y[4]);
    mul (y[5], -Ga7 * 0.25, y[5]);
    mul (y[6],  Ga2 * 0.25, y[6]);
    mul (y[7],  Ga3 * 0.25, y[7]);

    // transpose back
    for (int i = 0; i < BLOCK_SIZE; i ++)
        for (int j = 0; j < BLOCK_SIZE; j ++)
            destination[i][j] = y[j][i];
}
