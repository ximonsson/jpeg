#include "jpeg/jpeg.h"
#include "ui.h"
#include "gpu.h"
#include "huffman.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <CL/cl.h>


#define BLOCK_TSIZE 64
#define EOB         0 // End of Block

#define DECODE_HUFFMAN_AC(buffer, p) decode_huffman_value (huffman_ac_tree, buffer, p)
#define DECODE_HUFFMAN_DC(buffer, p) decode_huffman_value (huffman_dc_tree, buffer, p)

#ifdef JPEG_HW__USE_OPENCL
    typedef int16_t DATATYPE;
#elif JPEG_HW__USE_GLSL
    typedef int16_t DATATYPE;
#endif


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

static int       width,
                 height;
static DATATYPE* blocks;
static int16_t   prev_dc;


/**
 *  Perform entropy encoding on a block of data.
 */
static inline void encode (DATATYPE* block, uint8_t* destination, int* p)
{
    uint8_t run_length = 0;
    uint16_t amplitude;

    // encode DC coefficient
    amplitude = block[0] - prev_dc;
    prev_dc   = block[0];
    // write code for symbol 2 (amplitude)
    encode_huffman_dc_value (amplitude, destination, p);

    // encode AC coefficients
    for (int i = 1; i < BLOCK_TSIZE; i ++)
    {
        amplitude = block[(zigzag[i][1] * width) + zigzag[i][0]];
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
static inline void decode (uint8_t* data, int* p, DATATYPE* block)
{
    // decode DC coefficient
    uint8_t  symbol    = DECODE_HUFFMAN_DC (data, p);
    uint16_t amplitude = read_value (data, p, symbol);
    // check if it was a negative value
    if ((amplitude & (1 << (symbol - 1))) == 0)
        amplitude = ~((~amplitude) & ~(0xFFFF << symbol)) + 1;

    prev_dc += amplitude;
    block[0] = (prev_dc << 3) + 1024; // magic: DC x 8 + 1024 (quantize and normalize here, difficult to do on gpu)

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
            block[(zigzag[i][1] * width) + zigzag[i][0]] = (int16_t) amplitude;
            i ++;
        }
    }
}


static int encode_blocks (uint8_t* destination)
{
    int ptr = 0;
    int w = width >> 1;
    int16_t* blockp = blocks;
    prev_dc = 0;
    // Y
    for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (int x = 0; x < width; x += JPEG_BLOCK_SIZE)
        {
            encode (blockp, destination, &ptr);
            blockp += JPEG_BLOCK_SIZE;
        }
        blockp += (JPEG_BLOCK_SIZE - 1) * width;
    }
    prev_dc = 0;
    blockp = blocks + height * width;
    // U
    for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (int x = 0; x < w; x += JPEG_BLOCK_SIZE)
        {
            encode (blockp, destination, &ptr);
            blockp += JPEG_BLOCK_SIZE;
        }
        blockp += (JPEG_BLOCK_SIZE) * width - w;
    }
    prev_dc = 0;
    blockp = blocks + height * width + w;
    // V
    for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (int x = 0; x < w; x += JPEG_BLOCK_SIZE)
        {
            encode (blockp, destination, &ptr);
            blockp += JPEG_BLOCK_SIZE;
        }
        blockp += (JPEG_BLOCK_SIZE) * width - w;
    }

    return ptr / 8 + 1;
}


static void decode_blocks (uint8_t* data)
{
    int ptr = 0;
    memset (blocks, 0, width * height * 2 * sizeof (DATATYPE));
    DATATYPE* block_ptr = blocks;
    int w = width >> 1;
    prev_dc = 0;

    // decode blocks
    // y blocks
    for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (int x = 0; x < width; x += JPEG_BLOCK_SIZE)
        {
            decode (data, &ptr, block_ptr);
            block_ptr += JPEG_BLOCK_SIZE;
        }
        block_ptr += 7 * width;
    }

    // u blocks
    prev_dc = 0;
    for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (int x = 0; x < w; x += JPEG_BLOCK_SIZE)
        {
            decode (data, &ptr, block_ptr);
            block_ptr += JPEG_BLOCK_SIZE;
        }
        block_ptr += JPEG_BLOCK_SIZE * width - w;
    }

    // v blocks
    prev_dc = 0;
    block_ptr = blocks + height * width + w;
    for (int y = 0; y < height; y += JPEG_BLOCK_SIZE)
    {
        for (int x = 0; x < w; x += JPEG_BLOCK_SIZE)
        {
            decode (data, &ptr, block_ptr);
            block_ptr += JPEG_BLOCK_SIZE;
        }
        block_ptr += JPEG_BLOCK_SIZE * width - w;
    }
}


static void init_huffman ()
{
    // init huffman trees for decoding
    huffman_ac_tree = malloc (sizeof (struct huffman_tree_node));
    huffman_dc_tree = malloc (sizeof (struct huffman_tree_node));
    huffman_ac_tree->leaf = 0;
    huffman_dc_tree->leaf = 0;
    huffman_ac_tree->children[0] = huffman_ac_tree->children[1] = NULL;
    huffman_dc_tree->children[0] = huffman_dc_tree->children[1] = NULL;
    create_huffman_ac_tree (huffman_ac_tree);
    create_huffman_dc_tree (huffman_dc_tree);
}


static void deinit_huffman ()
{
    // free huffman trees
    destroy_huffman_tree (huffman_ac_tree);
    destroy_huffman_tree (huffman_dc_tree);
}


int jpeg_init (int w, int h, GLuint texbuf)
{
    width = w; height = h;
    blocks = malloc (width * height * 2 * sizeof (DATATYPE));
    init_huffman ();
#ifdef JPEG_HW__USE_OPENCL
    if (init_opencl (width, height, texbuf) != 0)
    {
        fprintf (stderr, "error initializing OpenCL\n");
        return 1;
    }
#elif JPEG_HW__USE_GLSL
    if (init_compute_shader (width, height) != 0)
    {
        fprintf (stderr, "error initializing compute shader\n");
        return 1;
    }
#endif
    return 0;
}


void jpeg_deinit ()
{
    free (blocks);
    deinit_huffman ();
    deinit_opencl ();
    deinit_compute_shader ();
}


int jpeg_compress (uint8_t* data, uint8_t* destination)
{
    memset (blocks, 0, width * height * 2 * sizeof (DATATYPE));

    DATATYPE* yblocks   = blocks;
    DATATYPE* ublocks   = blocks + width * height;
    DATATYPE* vblocks   = blocks + width * height + (width >> 1);
    uint8_t*  datap     = data;

    // unpack blocks
    for (int y = 0; y < height; y ++)
    {
        for (int x = 0; x < (width >> 1); x ++)
        {
            yblocks[(x << 1)]     = datap[1];
            yblocks[(x << 1) + 1] = datap[3];
            ublocks[x]            = datap[0];
            vblocks[x]            = datap[2];

            datap += 4;
        }
        ublocks += width;
        vblocks += width;
        yblocks += width;
    }

    // compress
    compress_blocks (blocks);

    // encode
    return encode_blocks (destination);
}


int jpeg_decompress (unsigned char* data, size_t size, unsigned char* destination)
{
    decode_blocks (data);
    // send to gpu and decompress
    decompress_blocks (blocks, destination);
    return 0;
}


int jpeg_decompress_to_texture (uint8_t* data, size_t size, GLuint texture)
{
    decode_blocks (data);
    decompress_blocks_to_texture (blocks, texture);
    return 0;
}
