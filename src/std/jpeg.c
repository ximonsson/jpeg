#include "jpeg/jpeg.h"
#include "utils.h"
#include "huffman.h"
#include "dct.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#ifdef MULTITHREAD
    #include "thread_pool.h"
#endif

#define BLOCK_TSIZE 64
#define EOB          0 // End of Block
#define MEMALIGN    16
#define ROUND_TO_BYTE(x) x > 255 ? 255 : x < 0 ? 0 : x;


static int width,
           height;

// luminance and color channels for raw data
static int16_t* Y;
static int16_t* U;
static int16_t* V;

static uint8_t* buffer;

static size_t block_byte_size = BLOCK_TSIZE * sizeof (int16_t);

#ifdef MULTITHREAD
static int          running = 1;
static thread_pool  jobs;
static thread_pool  finished_jobs;
static pthread_t    thread_1, thread_2, thread_3, thread_4;
#endif

/* standard quantization matrix ~ 50% */
// static float quantization_matrix_50[JPEG_BLOCK_SIZE][JPEG_BLOCK_SIZE] =
// {
//     {  16,  11,  10,  16,  24,  40,  51,  61 },
//     {  12,  12,  14,  19,  26,  58,  60,  55 },
//     {  14,  13,  16,  24,  40,  57,  69,  56 },
//     {  14,  17,  22,  29,  51,  87,  80,  62 },
//     {  18,  22,  37,  56,  68, 109, 103,  77 },
//     {  24,  35,  55,  64,  81, 104, 113,  92 },
//     {  49,  64,  78,  87, 103, 121, 120, 101 },
//     {  72,  92,  95,  90, 112, 100, 103,  99 }
// };
static float quantization_matrix_95[JPEG_BLOCK_SIZE][JPEG_BLOCK_SIZE] =
{
    {   8,   5,   5,   8,  12,  20,  25,  30 },
    {   6,   6,   7,   9,  13,  29,  30,  27 },
    {   7,   6,   8,  12,  20,  28,  34,  28 },
    {   7,   8,  11,  14,  25,  43,  40,  31 },
    {   9,  11,  18,  28,  34,  54,  51,  38 },
    {  12,  17,  27,  32,  40,  52,  56,  46 },
    {  24,  32,  39,  43,  51,  60,  60,  50 },
    {  36,  46,  47,  45,  56,  50,  51,  49 }
};

/* specify zig zag order to do entropy encoding in */
static uint8_t zigzag[JPEG_BLOCK_SIZE * JPEG_BLOCK_SIZE][2] =
{
    { 0, 0 }, { 1, 0 }, { 0, 1 }, { 0, 2 }, { 1, 1 }, { 2, 0 }, { 3, 0 }, { 2, 1 },
    { 1, 2 }, { 0, 3 }, { 0, 4 }, { 1, 3 }, { 2, 2 }, { 3, 1 }, { 4, 0 }, { 5, 0 },
    { 4, 1 }, { 3, 2 }, { 2, 3 }, { 1, 4 }, { 0, 5 }, { 0, 6 }, { 1, 5 }, { 2, 4 },
    { 3, 3 }, { 4, 2 }, { 5, 1 }, { 6, 0 }, { 7, 0 }, { 6, 1 }, { 5, 2 }, { 4, 3 },
    { 3, 4 }, { 2, 5 }, { 1, 6 }, { 0, 7 }, { 1, 7 }, { 2, 6 }, { 3, 5 }, { 4, 4 },
    { 5, 3 }, { 6, 2 }, { 7, 1 }, { 7, 2 }, { 6, 3 }, { 5, 4 }, { 4, 5 }, { 3, 6 },
    { 2, 7 }, { 3, 7 }, { 4, 6 }, { 5, 5 }, { 6, 4 }, { 7, 3 }, { 7, 4 }, { 6, 5 },
    { 5, 6 }, { 4, 7 }, { 5, 7 }, { 6, 6 }, { 7, 5 }, { 7, 6 }, { 6, 7 }, { 7, 7 }
};

#define DECODE_HUFFMAN_AC(buffer, p) decode_huffman_value (huffman_ac_tree, buffer, p)
#define DECODE_HUFFMAN_DC(buffer, p) decode_huffman_value (huffman_dc_tree, buffer, p)

static struct huffman_tree_node* huffman_ac_tree;
static struct huffman_tree_node* huffman_dc_tree;


/**
 *  Init Huffman trees for decoding.
 */
static void huffman_init ()
{
    huffman_ac_tree = malloc (sizeof (struct huffman_tree_node));
    huffman_dc_tree = malloc (sizeof (struct huffman_tree_node));
    huffman_ac_tree->leaf = 0;
    huffman_dc_tree->leaf = 0;
    huffman_ac_tree->children[0] = huffman_ac_tree->children[1] = NULL;
    huffman_dc_tree->children[0] = huffman_dc_tree->children[1] = NULL;
    create_huffman_ac_tree (huffman_ac_tree);
    create_huffman_dc_tree (huffman_dc_tree);
}

/**
 *  Apply quantization to data with q as quantization parameter.
 */
static void quantize (float block[JPEG_BLOCK_SIZE][JPEG_BLOCK_SIZE], int16_t* destination)
{
    for (int j = 0; j < JPEG_BLOCK_SIZE; j ++)
        for (int i = 0; i < JPEG_BLOCK_SIZE; i ++)
            destination[(j << 3) + i] = block[j][i] / quantization_matrix_95[j][i];
}

/**
 *  Multiply by quantization matrix.
 */
static void dequantize (int16_t* block)
{
    for (int i = 0; i < JPEG_BLOCK_SIZE; i ++)
        for (int j = 0; j < JPEG_BLOCK_SIZE; j ++)
            block[(i << 3) + j] *= quantization_matrix_95[i][j];
}

/**
 *  Perform entropy encoding on a block of data.
 */
static void encode (int16_t* block, uint8_t* destination, int* p)
{
    uint8_t run_length = 0;
    uint16_t amplitude;

    // encode DC coefficient
    amplitude = block[0];
    // write code for symbol 2 (amplitude)
    encode_huffman_dc_value (amplitude, destination, p);

    // encode AC coefficients
    for (int i = 1; i < BLOCK_TSIZE; i ++)
    {
        amplitude = block[(zigzag[i][1] << 3) + zigzag[i][0]];
        if (amplitude == 0)
        {
            run_length ++;
            continue;
        }
        else
        {
            // if run_length > 15 fill out with special symbol for >15 zero run.
            for ( ; run_length > MAX_RUN_LEN; run_length -= MAX_RUN_LEN)
                encode_huffman_ac_value (0, MAX_RUN_LEN, destination, p);
            // encode to buffer
            encode_huffman_ac_value (amplitude, run_length, destination, p);
            run_length = 0;
        }
    }
    // write EOB (end of block)
    encode_huffman_ac_value (0, 0, destination, p);
}

/**
 *  Decode entropy data.
 */
static void decode (uint8_t* data, int* p, int16_t* block)
{
    // decode DC coefficient
    uint8_t  symbol    = DECODE_HUFFMAN_DC (data, p);
    uint16_t amplitude = read_value (data, p, symbol);
    // check if it was a negative value
    if ((amplitude & (1 << (symbol - 1))) == 0)
        amplitude = ~((~amplitude) & ~(0xFFFF << symbol)) + 1;
    block[0] = amplitude;

    // fill rest of block
    int i = 1;
    uint8_t run, size;
    while ((symbol = DECODE_HUFFMAN_AC (data, p)) != EOB)
    {
        run  = symbol >> 4;
        size = symbol & 0xF;
        i   += run; // skip zeroes
        if (size > 0)
        {
            amplitude = read_value (data, p, size);
            if ((amplitude & (1 << (size - 1))) == 0) // handle negative values
                amplitude = ~((~amplitude) & ~(0xFFFF << size)) + 1;
            block[(zigzag[i][1] << 3) + zigzag[i][0]] = amplitude;
            i ++;
        }
    }
}


static inline void compress_block (int16_t* block,
                                   int16_t* previous_dc,
                                   float coefficients[JPEG_BLOCK_SIZE][JPEG_BLOCK_SIZE],
                                   uint8_t* destination,
                                   int* bufferp)
{
    // transform
    fdct2 (block, coefficients);
    // quantize
    coefficients[0][0] -= 1024;
    quantize (coefficients, block);
    // remove previous dc
    int16_t dc = *previous_dc;
    *previous_dc = block[0];
    block[0] -= dc;
    // encode
    encode (block, destination, bufferp);
}


static inline void decompress_block (uint8_t* data, int* p, int16_t* block, int16_t* dc, float decompressed[JPEG_BLOCK_SIZE][JPEG_BLOCK_SIZE])
{
    // decode
    decode (data, p, block);
    block[0] += *dc;
    *dc = block[0];

    // dequantize
    dequantize (block);
    block[0] += 1024;

    // inverse DCT
    ifdct2 (block, decompressed);
}


int jpeg_compress (unsigned char* data, unsigned char* destination)
{
    int y, x, i;
    int w = width << 1;
    float coefficients[JPEG_BLOCK_SIZE][JPEG_BLOCK_SIZE];
    int16_t dc = 0;
    int bufferp = JPEG_HEADER_SIZE << 3;

    //                          Prepare data:
    // separate data from the different channels.
    // @TODO: it assumes data is packed as UYVY.
    //        if the data could be captured with separate channels it
    //        would go a lot faster to skip this crap.
    // To compute which block we are doing following:
    // ((y / 8 * width / 8) + (x / 8)) * 64 + (y % 8) * 8 + x % 8
    i = width >> 3;
    for (y = 0; y < height; y ++)
        for (x = 0; x < width; x ++)
            Y[(((y >> 3) * i + (x >> 3)) << 6) + ((y & 0x7) << 3) + (x & 0x7)] = data[y * w + (x << 1) + 1];
    i >>= 1;
    for (y = 0; y < height; y ++)
        for (x = 0; x < width >> 1; x ++)
        {
            U[(((y >> 3) * i + (x >> 3)) << 6) + ((y & 0x7) << 3) + (x & 0x7)] = data[y * w + (x << 2)];
            V[(((y >> 3) * i + (x >> 3)) << 6) + ((y & 0x7) << 3) + (x & 0x7)] = data[y * w + (x << 2) + 2];
        }

    w  = width >> 1;

    //                          Compress color channels:
    // compress luminance
    for (i = 0; i < width * height; i += BLOCK_TSIZE)
        compress_block (Y + i, &dc, coefficients, destination, &bufferp);
    // store size in bits of luminance data
    memcpy (destination, &bufferp, sizeof (int));

    dc = 0;
    for ( ; i < width * height; i += BLOCK_TSIZE)
        compress_block (Y + i, &dc, coefficients, destination, &bufferp);
    // store size in bits of luminance data
    memcpy (destination + sizeof (int), &bufferp, sizeof (int));

    // compress chroma blue
    dc = 0;
    for (i = 0; i < w * height; i += BLOCK_TSIZE)
        compress_block (U + i, &dc, coefficients, destination, &bufferp);
    // store size of blue data
    memcpy (destination + 2 * sizeof (int), &bufferp, sizeof (int));

    // compress chroma red
    dc = 0;
    for (i = 0; i < w * height; i += BLOCK_TSIZE)
        compress_block (V + i, &dc, coefficients, destination, &bufferp);
    // store size of red data
    memcpy (destination + 3 * sizeof (int), &bufferp, sizeof (int));

    return (bufferp << 3) + 1;
}


void jpeg_deinit ()
{
    // free temporary buffers
    free (Y);
    free (U);
    free (V);
    free (buffer);
    // free huffman trees
    destroy_huffman_tree (huffman_ac_tree);
    destroy_huffman_tree (huffman_dc_tree);

#ifdef MULTITHREAD
    running = 0;
    thread_pool_destroy (&jobs);
    thread_pool_destroy (&finished_jobs);
#endif
}


int jpeg_decompress_to_texture (uint8_t* data, size_t size, GLuint tex)
{
    jpeg_decompress (data, size, buffer);
    load_texture (buffer, width, height);
    return 0;
}


#ifdef MULTITHREAD

static void* decompress_thread (void* __not_used__)
{
    int p = 0;
    int x, y, i, j;
    int w = width << 1;
    int16_t dc;

    float tmp[JPEG_BLOCK_SIZE][JPEG_BLOCK_SIZE];

    int offset;
    uint8_t step;

    int16_t* block;
    uint8_t* data = NULL;
    uint8_t* destination = NULL;

    thread_args args;

    if (posix_memalign ((void**) &block, MEMALIGN, block_byte_size) != 0)
    {
        fprintf (stderr, "error allocating memory\n");
        return NULL;
    }

    while (running)
    {
        // pop new job
        if (thread_pool_pop (&jobs, &args) == THREAD_POOL_EMPTY)
            continue;

        data        = args.source;
        destination = args.destination;
        step        = args.step;
        offset      = args.offset;
        p           = args.bitp;
        dc          = 0;

        for (y = 0; y < height; y += JPEG_BLOCK_SIZE)
        {
            for (x = 0; x < (width >> 1); x += JPEG_BLOCK_SIZE)
            {
                memset (block, 0, block_byte_size);
                decompress_block (data, &p, block, &dc, tmp);
                // copy it to buffer
                for (i = 0; i < JPEG_BLOCK_SIZE; i ++)
                    for (j = 0; j < JPEG_BLOCK_SIZE; j ++)
                        destination[(y + i) * w + ((x + j) << step) + offset] = ROUND_TO_BYTE (tmp[i][j]);
            }
        }
        // push to finished jobs
        thread_pool_push (&finished_jobs, data, destination, offset, step, args.bitp);
    }

    free (block);
    return NULL;
}


int jpeg_decompress (unsigned char* data, size_t size, unsigned char* destination)
{
    int bitp = JPEG_HEADER_SIZE << 3;
    thread_pool_push (&jobs, data, destination, 1, 1, bitp);

    memcpy (&bitp, data, sizeof (int));
    thread_pool_push (&jobs, data, destination, 1 + width, 1, bitp);

    memcpy (&bitp, data + sizeof (int), sizeof (int));
    thread_pool_push (&jobs, data, destination, 0, 2, bitp);

    memcpy (&bitp, data + 2 * sizeof (int), sizeof (int));
    thread_pool_push (&jobs, data, destination, 2, 2, bitp);

    thread_args finito;
    thread_pool_pop (&finished_jobs, &finito);
    thread_pool_pop (&finished_jobs, &finito);
    thread_pool_pop (&finished_jobs, &finito);
    thread_pool_pop (&finished_jobs, &finito);

    return 0;
}

#else /* if not MULTITHREAD : single thread */

int jpeg_decompress (unsigned char* data, size_t size, unsigned char* destination)
{
    int16_t* block;
    float    tmp[JPEG_BLOCK_SIZE][JPEG_BLOCK_SIZE];
    int      x, y, i, j;
    int      p       = JPEG_HEADER_SIZE << 3;
    int16_t  dc      = 0;
    int      nblocks = 0;
    int      w       = width >> 1;

    if (posix_memalign ((void**) &block, MEMALIGN, block_byte_size) != 0)
    {
        fprintf (stderr, "error allocating memory\n");
        return 1;
    }

    // decode luminance
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (x = 0; x < w; x += JPEG_BLOCK_SIZE, nblocks ++)
        {
            memset (block, 0, block_byte_size);
            decompress_block (data, &p, block, &dc, tmp);
            // copy it to buffer
            for (i = 0; i < JPEG_BLOCK_SIZE; i ++)
                for (j = 0; j < JPEG_BLOCK_SIZE; j ++)
                    destination[(y + i) * (width << 1) + ((x + j) << 1) + 1] = ROUND_TO_BYTE (tmp[i][j]);
        }
    }

    // decode luminance
    dc = 0;
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (x = 0; x < w; x += JPEG_BLOCK_SIZE, nblocks ++)
        {
            memset (block, 0, block_byte_size);
            decompress_block (data, &p, block, &dc, tmp);
            // copy it to buffer
            for (i = 0; i < JPEG_BLOCK_SIZE; i ++)
                for (j = 0; j < JPEG_BLOCK_SIZE; j ++)
                    destination[(y + i) * (width << 1) + ((x + j) << 1) + 1 + width] = ROUND_TO_BYTE (tmp[i][j]);
        }
    }

    dc = 0;
    // decode chrominance
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (x = 0; x < w; x += JPEG_BLOCK_SIZE, nblocks ++)
        {
            memset (block, 0, block_byte_size);
            decompress_block (data, &p, block, &dc, tmp);
            // copy it to buffer
            for (i = 0; i < JPEG_BLOCK_SIZE; i ++)
                for (j = 0; j < JPEG_BLOCK_SIZE; j ++)
                    destination[(y + i) * (width << 1) + ((x + j) << 2)] = ROUND_TO_BYTE (tmp[i][j]);
        }
    }

    dc = 0;
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (x = 0; x < w; x += JPEG_BLOCK_SIZE, nblocks ++)
        {
            memset (block, 0, block_byte_size);
            decompress_block (data, &p, block, &dc, tmp);
            // copy it to buffer
            for (i = 0; i < JPEG_BLOCK_SIZE; i ++)
                for (j = 0; j < JPEG_BLOCK_SIZE; j ++)
                    destination[(y + i) * (width << 1) + ((x + j) << 2) + 2] = ROUND_TO_BYTE (tmp[i][j]);
        }
    }

    free (block);
    return 0;
}

#endif /* MULTITHREAD */


int jpeg_init (int w, int h, GLuint text)
{
    width  = w;
    height = h;

    if (posix_memalign ((void**) &Y, MEMALIGN, width * height     * sizeof (int16_t)) != 0 ||
        posix_memalign ((void**) &U, MEMALIGN, width * height / 2 * sizeof (int16_t)) != 0 ||
        posix_memalign ((void**) &V, MEMALIGN, width * height / 2 * sizeof (int16_t)) != 0)
    {
        fprintf (stderr, "error allocating init memory\n");
        return 1;
    }
    memset (Y, 0, width * height     * sizeof (int16_t));
    memset (U, 0, width * height / 2 * sizeof (int16_t));
    memset (V, 0, width * height / 2 * sizeof (int16_t));

    buffer = malloc (width * height * 2);
    memset (buffer, 0, width * height * 2);

    huffman_init ();

#ifdef MULTITHREAD
    // init thread pools
    thread_pool_init (&jobs);
    thread_pool_init (&finished_jobs);
    // start decoder threads
    pthread_create (&thread_1, NULL, decompress_thread, NULL);
    pthread_create (&thread_2, NULL, decompress_thread, NULL);
    pthread_create (&thread_3, NULL, decompress_thread, NULL);
    pthread_create (&thread_4, NULL, decompress_thread, NULL);
#endif

    return 0;
}
