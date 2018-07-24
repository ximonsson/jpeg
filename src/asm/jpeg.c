#include "jpeg/jpeg.h"
#include "huffman.h"
#include "utils.h"
#include "ui.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#ifdef MULTITHREAD
#include "thread_pool.h"
#include <unistd.h>
#endif


#define BLOCK_TSIZE     64
#define EOB             0 // End of Block
#define Y_STRIDE        16
#define UV_STRIDE       32

#define DECODE_HUFFMAN_AC(buffer, p) decode_huffman_value (huffman_ac_tree, buffer, p)
#define DECODE_HUFFMAN_DC(buffer, p) decode_huffman_value (huffman_dc_tree, buffer, p)


#define DECOMPRESS(func, ptr, block) \
    memset (block, 0, block_byte_size);\
    decode (data, &p, block);\
    block[0] += prev_dc;\
    prev_dc = block[0];\
    func (compressed_block, destination + y * w + x);


#define COMPRESS(func, ptr, block) \
    func (ptr, block);\
    prev_dc_tmp = block[0];\
    block[0] -= prev_dc;\
    prev_dc = prev_dc_tmp;\
    encode (block, destination, &p);\


static struct huffman_tree_node* huffman_ac_tree;
static struct huffman_tree_node* huffman_dc_tree;

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

static int      width,
                height;

static uint8_t* buffer;


/**
 *  Assembly functions
 */
extern void setup                (int, int, float*, float*);
extern void reset_dc             ();

extern void compress_luminance   (uint8_t*, int16_t*);
extern void compress_red         (uint8_t*, int16_t*);
extern void compress_blue        (uint8_t*, int16_t*);

extern void decompress_luminance (int16_t*, uint8_t*);
extern void decompress_red       (int16_t*, uint8_t*);
extern void decompress_blue      (int16_t*, uint8_t*);


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
 *  Destroy Huffman trees.
 */
static void huffman_deinit ()
{
    destroy_huffman_tree (huffman_ac_tree);
    destroy_huffman_tree (huffman_dc_tree);
}

/**
 *  Perform entropy encoding on a block of data.
 */
static inline void encode (int16_t* block, uint8_t* destination, int* p)
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
    encode_huffman_ac_value (EOB, 0, destination, p);
}

/**
 *  Decode entropy data.
 */
static inline void decode (uint8_t* data, int* p, int16_t* block)
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


#ifndef MULTITHREAD /* single threaded version */

static float*   block_coefs,
            *   block_coefs_transformed;
static int16_t* compressed_block;

int jpeg_init (int w, int h, GLuint texbuf)
{
    width  = w;
    height = h;
    buffer = malloc (width * height * 2);

    // allocate buffers
    if (posix_memalign ((void**) &compressed_block, 16, BLOCK_TSIZE * sizeof (int16_t)) != 0)
    {
        fprintf (stderr, "error allocating buffer\n");
        return 1;
    }
    if (posix_memalign ((void**) &block_coefs, 16, BLOCK_TSIZE * sizeof (float)) != 0)
    {
        fprintf (stderr, "error allocating buffer\n");
        return 1;
    }
    if (posix_memalign ((void**) &block_coefs_transformed, 16, BLOCK_TSIZE * sizeof (float)) != 0)
    {
        fprintf (stderr, "error allocating buffer\n");
        return 1;
    }

    // setup assembly
    setup (w, h, block_coefs, block_coefs_transformed);
    // initialize huffman trees
    huffman_init ();
    return 0;
}


void jpeg_deinit ()
{
    // free huffman trees
    huffman_deinit ();
    free (buffer);
    free (block_coefs);
    free (block_coefs_transformed);
    free (compressed_block);
}


int jpeg_compress (unsigned char* data, unsigned char* destination)
{
    uint8_t* ptr          = data;
    int      p            = JPEG_HEADER_SIZE << 3; // bit pointer
    int      w            = width << 1;
    int      block_stride = (JPEG_BLOCK_SIZE - 1) * w;
    int16_t  prev_dc      = 0;
    int16_t  prev_dc_tmp  = 0;
    int y, x;

    memset (compressed_block, 0, BLOCK_TSIZE * sizeof (int16_t));

    // compress Y
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE, ptr += block_stride)
    {
        for (x = 0; x < w / 2; x += Y_STRIDE, ptr += Y_STRIDE)
        {
            COMPRESS (compress_luminance, ptr, compressed_block)
        }
        ptr += w - w / 2 - ((w / 2) % Y_STRIDE);
    }
    // reset and write to header size in bits of encoded y blocks
    memcpy (destination, &p, sizeof (int));
    ptr = data + w / 2 - ((w / 2) % Y_STRIDE);
    prev_dc = 0;

    for (y = 0 ; y < height; y += JPEG_BLOCK_SIZE, ptr += block_stride)
    {
        for (x = w / 2 - ((w / 2) % Y_STRIDE); x < w; x += Y_STRIDE, ptr += Y_STRIDE)
        {
            COMPRESS (compress_luminance, ptr, compressed_block)
        }
        ptr += w / 2 - ((w / 2) % Y_STRIDE);
    }
    // reset and write to header size in bits of encoded y blocks
    memcpy (destination + sizeof (int), &p, sizeof (int));
    ptr = data;
    prev_dc = 0;

    // compress U
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE, ptr += block_stride)
    {
        for (x = 0; x < w; x += UV_STRIDE, ptr += UV_STRIDE)
        {
            COMPRESS (compress_blue, ptr, compressed_block)
        }
    }
    // reset and write to header size in bits of encoded u blocks
    memcpy (destination + 2 * sizeof (int), &p, sizeof (int));
    ptr = data;
    prev_dc = 0;

    // compress V
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE, ptr += block_stride)
    {
        for (x = 0; x < w; x += UV_STRIDE, ptr += UV_STRIDE)
        {
            COMPRESS (compress_red, ptr, compressed_block)
        }
    }
    // write to header size in bits of encoded v blocks
    memcpy (destination + 3 * sizeof (int), &p, sizeof (int));
    return (p >> 3) + 1;
}


int jpeg_decompress (unsigned char* data, size_t size, unsigned char* destination)
{
    int     w               = width << 1;
    int     p               = JPEG_HEADER_SIZE << 3;
    size_t  block_byte_size = BLOCK_TSIZE * sizeof (int16_t);
    int16_t prev_dc         = 0;
    int y, x;

    for (y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (x = 0; x < w / 2; x += Y_STRIDE)
        {
            DECOMPRESS (decompress_luminance, ptr, compressed_block);
        }
    }
    prev_dc = 0;
    for (y = 0 ; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (x = w / 2 - ((w / 2) % Y_STRIDE); x < w; x += Y_STRIDE)
        {
            DECOMPRESS (decompress_luminance, ptr, compressed_block);
        }
    }
    prev_dc = 0;
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (x = 0; x < w; x += UV_STRIDE)
        {
            DECOMPRESS (decompress_blue, ptr, compressed_block);
        }
    }
    prev_dc = 0;
    for (y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (x = 0; x < w; x += UV_STRIDE)
        {
            DECOMPRESS (decompress_red, ptr, compressed_block);
        }
    }
    return 0;
}


int jpeg_decompress_to_texture (uint8_t* data, size_t size, GLuint tex)
{
    jpeg_decompress (data, size, buffer);
    load_texture (buffer, width, height);
    return 0;
}

#endif /* if not defined MULTITHREAD */


/**
 *  Multi-threaded version of the JPEG codec starts here.
 */
#ifdef MULTITHREAD

#define THREAD_SETUP \
    if (posix_memalign ((void**) &compressed_block, 16, block_byte_size) != 0) \
    {\
        fprintf (stderr, "error allocating buffer in thread\n");\
        return NULL;\
    }\
    if (posix_memalign ((void**) &block_coefs, 16, BLOCK_TSIZE * sizeof (float)) != 0)\
    {\
        fprintf (stderr, "error allocating buffer\n");\
        return NULL;\
    }\
    if (posix_memalign ((void**) &block_coefs_transformed, 16, BLOCK_TSIZE * sizeof (float)) != 0)\
    {\
        fprintf (stderr, "error allocating buffer\n");\
        return NULL;\
    }\
    setup (width, height, block_coefs, block_coefs_transformed);
/* end THREAD_SETUP */


static pthread_t        thread_y;
static pthread_t        thread_uv;

static int              running = 1;

static thread_pool      y_jobs;
static thread_pool      finished_y_jobs;
static thread_pool      uv_jobs;
static thread_pool      finished_uv_jobs;

/**
 *  Threaded function decompressing Y values.
 */
static void* decompress_y (void* __not_used__)
{
    uint8_t* data               = NULL;
    uint8_t* destination        = NULL;
    int      w                  = width << 1;
    int      p                  = JPEG_HEADER_SIZE << 3;
    size_t   block_byte_size    = BLOCK_TSIZE * sizeof (int16_t);
    int16_t  prev_dc            = 0;
    int16_t* compressed_block   = NULL;
    float*   block_coefs,
         *   block_coefs_transformed;
    thread_args args;

    THREAD_SETUP

    while (running)
    {
        // pop new job
        if (thread_pool_pop (&y_jobs, &args) == THREAD_POOL_EMPTY)
            continue;

        p = JPEG_HEADER_SIZE << 3;
        data = args.source;
        destination = args.destination;
        prev_dc = 0;
        // decompress
        for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
            for (int x = 0; x < w; x += Y_STRIDE)
            {
                DECOMPRESS (decompress_luminance, ptr, compressed_block);
            }
        // push to finished jobs
        thread_pool_push (&finished_y_jobs, data, destination);
    }

    free (compressed_block);
    free (block_coefs);
    free (block_coefs_transformed);
    return NULL;
}

/**
 *  Thread function decompressing UV values.
 */
static void* decompress_uv (void* __not_used__)
{
    uint8_t* data               = NULL;
    uint8_t* destination        = NULL;
    int      w                  = width << 1;
    int      p                  = 0;
    size_t   block_byte_size    = BLOCK_TSIZE * sizeof (int16_t);
    int16_t  prev_dc            = 0;
    int16_t* compressed_block   = NULL;
    float*   block_coefs,
         *   block_coefs_transformed;

    thread_args args;

    THREAD_SETUP

    while (running)
    {
        // pop new job
        if (thread_pool_pop (&uv_jobs, &args) == THREAD_POOL_EMPTY)
            continue;

        data = args.source;
        destination = args.destination;
        memcpy (&p, data, sizeof (int));
        prev_dc = 0;

        // decompress
        for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
            for (int x = 0; x < w; x += UV_STRIDE)
            {
                DECOMPRESS (decompress_blue, ptr, compressed_block);
            }
        prev_dc = 0;
        for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
            for (int x = 0; x < w; x += UV_STRIDE)
            {
                DECOMPRESS (decompress_red, ptr, compressed_block);
            }

        // push to finished jobs
        thread_pool_push (&finished_uv_jobs, data, destination);
    }

    free (compressed_block);
    free (block_coefs);
    free (block_coefs_transformed);
    return NULL;
}

int jpeg_decompress (unsigned char* data, size_t size, unsigned char* destination)
{
    thread_args args;

    // push new jobs to decompress threads
    thread_pool_push (&y_jobs, data, destination);
    thread_pool_push (&uv_jobs, data, destination);

    // do something ?
    // this function is blocking but we could make it buffer.

    // pop finished job
    thread_pool_pop (&finished_y_jobs, &args);
    thread_pool_pop (&finished_uv_jobs, &args);
    return 0;
}

int jpeg_decompress_to_texture (uint8_t* data, size_t size, GLuint tex)
{
    jpeg_decompress (data, size, buffer);
    load_texture (buffer, width, height);
    return 0;
}

int jpeg_compress (uint8_t* source, uint8_t* destination)
{
    // not implemented
    return 0;
}


int jpeg_init (int w, int h, GLuint tex)
{
    width  = w;
    height = h;

    buffer = malloc (width * height * 2);

    huffman_init ();

    thread_pool_init (&y_jobs);
    thread_pool_init (&uv_jobs);
    thread_pool_init (&finished_y_jobs);
    thread_pool_init (&finished_uv_jobs);

    // start threads
    pthread_create (&thread_y,  NULL, decompress_y, NULL);
    pthread_create (&thread_uv, NULL, decompress_uv, NULL);

    return 0;
}

void jpeg_deinit ()
{
    running = 0;
    thread_pool_destroy (&y_jobs);
    thread_pool_destroy (&uv_jobs);
    thread_pool_destroy (&finished_y_jobs);
    thread_pool_destroy (&finished_uv_jobs);
    pthread_join (thread_y,  NULL);
    pthread_join (thread_uv, NULL);
    huffman_deinit ();
    free (buffer);
}


#endif /* ifdef MULTITHREAD */
