#include "huffman.h"
#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* size is computed by doing the binary logarithm of the value.
   the amplitude of negative values is the one complement of the two's complement. makes sense? */
#define SIZE(x) if ((x & 0x8000) != 0)\
                {\
                    x = ~x + 1;\
                    size = log2 (x) + 1;\
                    x = ~x & ~(0xFFFF << size);\
                }\
                else\
                    size = log2 (x) + 1;

// http://www.w3.org/Graphics/JPEG/itu-t81.pdf page 150
// usage: CODE(size, code) = HUFFMAN[RUNLEN][SIZE]
const uint16_t huffman_ac[MAX_RUN_LEN + 1][MAX_SIZE + 1][2] =
{
    { { 4, 0x0A },   { 2, 0x0 },     { 2, 0x1 },     { 3,  0x4 },     { 4, 0x0B },   { 5, 0x1A },     { 7, 0x78 },    { 8, 0xF8 },   { 10, 0x3F6 },  { 16, 0xFF82 }, { 16, 0xFF83 } },
    { { 0, 0 },      { 4, 0x0C },    { 5, 0x1B },    { 7,  0x79 },    { 9, 0x1F6 },  { 11, 0x7F6 },  { 16, 0xFF84 }, { 16, 0xFF85 }, { 16, 0xFF86 }, { 16, 0xFF87 }, { 16, 0xFF88 } },
    { { 0, 0 },      { 5, 0x1C },    { 8, 0xF9 },    { 10, 0x3F7 },  { 12, 0xFF4 },  { 16, 0xFF89 }, { 16, 0xFF8A }, { 16, 0xFF8B }, { 16, 0xFF8C }, { 16, 0xFF8D }, { 16, 0xFF8E } },
    { { 0, 0 },      { 6, 0x3A },    { 9, 0x1F7 },   { 12, 0xFF5 },  { 16, 0xFF8F }, { 16, 0xFF90 }, { 16, 0xFF91 }, { 16, 0xFF92 }, { 16, 0xFF93 }, { 16, 0xFF94 }, { 16, 0xFF95 } },
    { { 0, 0 },      { 6, 0x3B },    { 10, 0x3F8 },  { 16, 0xFF96 }, { 16, 0xFF97 }, { 16, 0xFF98 }, { 16, 0xFF99 }, { 16, 0xFF9A }, { 16, 0xFF9B }, { 16, 0xFF9C }, { 16, 0xFF9D } },
    { { 0, 0 },      { 7, 0x7A },    { 11, 0x7F7 },  { 16, 0xFF9E }, { 16, 0xFF9F }, { 16, 0xFFA0 }, { 16, 0xFFA1 }, { 16, 0xFFA2 }, { 16, 0xFFA3 }, { 16, 0xFFA4 }, { 16, 0xFFA5 } },
    { { 0, 0 },      { 7, 0x7B },    { 12, 0xFF6 },  { 16, 0xFFA6 }, { 16, 0xFFA7 }, { 16, 0xFFA8 }, { 16, 0xFFA9 }, { 16, 0xFFAA }, { 16, 0xFFAB }, { 16, 0xFFAC }, { 16, 0xFFAD } },
    { { 0, 0 },      { 8, 0xFA },    { 12, 0xFF7 },  { 16, 0xFFAE }, { 16, 0xFFAF }, { 16, 0xFFB0 }, { 16, 0xFFB1 }, { 16, 0xFFB2 }, { 16, 0xFFB3 }, { 16, 0xFFB4 }, { 16, 0xFFB5 } },
    { { 0, 0 },      { 9, 0x1F8 },   { 15, 0x7FC0 }, { 16, 0xFFB6 }, { 16, 0xFFB7 }, { 16, 0xFFB8 }, { 16, 0xFFB9 }, { 16, 0xFFBA }, { 16, 0xFFBB }, { 16, 0xFFBC }, { 16, 0xFFBD } },
    { { 0, 0 },      { 9, 0x1F9 },   { 16, 0xFFBE }, { 16, 0xFFBF }, { 16, 0xFFC0 }, { 16, 0xFFC1 }, { 16, 0xFFC2 }, { 16, 0xFFC3 }, { 16, 0xFFC4 }, { 16, 0xFFC5 }, { 16, 0xFFC6 } },
    { { 0, 0 },      { 9, 0x1FA },   { 16, 0xFFC7 }, { 16, 0xFFC8 }, { 16, 0xFFC9 }, { 16, 0xFFCA }, { 16, 0xFFCB }, { 16, 0xFFCC }, { 16, 0xFFCD }, { 16, 0xFFCE }, { 16, 0xFFCF } },
    { { 0, 0 },      { 10, 0x3F9 },  { 16, 0xFFD0 }, { 16, 0xFFD1 }, { 16, 0xFFD2 }, { 16, 0xFFD3 }, { 16, 0xFFD4 }, { 16, 0xFFD5 }, { 16, 0xFFD6 }, { 16, 0xFFD7 }, { 16, 0xFFD8 } },
    { { 0, 0 },      { 10, 0x3FA },  { 16, 0xFFD9 }, { 16, 0xFFDA }, { 16, 0xFFDB }, { 16, 0xFFDC }, { 16, 0xFFDD }, { 16, 0xFFDE }, { 16, 0xFFDF }, { 16, 0xFFE0 }, { 16, 0xFFE1 } },
    { { 0, 0 },      { 11, 0x7F8 },  { 16, 0xFFE2 }, { 16, 0xFFE3 }, { 16, 0xFFE4 }, { 16, 0xFFE5 }, { 16, 0xFFE6 }, { 16, 0xFFE7 }, { 16, 0xFFE8 }, { 16, 0xFFE9 }, { 16, 0xFFEA } },
    { { 0, 0 },      { 16, 0xFFEB }, { 16, 0xFFEC }, { 16, 0xFFED }, { 16, 0xFFEE }, { 16, 0xFFEF }, { 16, 0xFFF0 }, { 16, 0xFFF1 }, { 16, 0xFFF2 }, { 16, 0xFFF3 }, { 16, 0xFFF4 } },
    { { 11, 0x7F9 }, { 16, 0xFFF5 }, { 16, 0xFFF6 }, { 16, 0xFFF7 }, { 16, 0xFFF8 }, { 16, 0xFFF9 }, { 16, 0xFFFA }, { 16, 0xFFFB }, { 16, 0xFFFC }, { 16, 0xFFFD }, { 16, 0xFFFE } }
};

const uint16_t huffman_dc[12][2] =
{
    { 2, 0x0 }, { 3, 0x2 },  { 3, 0x3 },  { 3, 0x4 },  { 3, 0x5 },  { 3, 0x6 },
    { 4, 0xE }, { 5, 0x1E }, { 6, 0x3E }, { 7, 0x7E }, { 8, 0xFE }, { 9, 0x1FE }
};


void create_huffman_ac_tree (struct huffman_tree_node* root)
{
    uint16_t size, code;
    struct huffman_tree_node* node;
    for (int i = 0; i <= MAX_RUN_LEN; i ++)
    {
        for (int j = 0; j <= MAX_SIZE; j ++)
        {
            size = huffman_ac[i][j][0];
            code = huffman_ac[i][j][1];
            if (!size)
                continue;
            // create intermediate path through tree
            node = root;
            code <<= 16 - size;
            for (int x = 0; x < size - 1; x ++)
            {
                if ((code & 0x8000) != 0)
                {
                    if (node->children[1] == NULL)
                    {
                        node->children[1] = malloc (sizeof (struct huffman_tree_node));
                        node = node->children[1];
                        node->leaf  = 0;
                        node->children[0] = node->children[1] = NULL;
                    }
                    else
                        node = node->children[1];
                }
                else
                {
                    if (node->children[0] == NULL)
                    {
                        node->children[0] = malloc (sizeof (struct huffman_tree_node));
                        node = node->children[0];
                        node->leaf  = 0;
                        node->children[0] = node->children[1] = NULL;
                    }
                    else
                        node = node->children[0];
                        // node = node->left;
                }
                code <<= 1;
            }
            // create leaf node.
            if ((code & 0x8000) != 0)
            {
                if (node->children[1] != NULL)
                {
                    fprintf (stderr, "error in creating AC huffman tree\n");
                    fprintf (stderr, "  path %.4X (%d) (%d, %d) already exists in the tree.\n", code, size, i, j);
                    exit (1);
                }
                node->children[1] = malloc (sizeof (struct huffman_tree_node));
                node = node->children[1];
                node->leaf = 1;
                node->symbol = (i << 4) | j;
            }
            else
            {
                if (node->children[0] != NULL)
                {
                    fprintf (stderr, "error in creating AC huffman tree\n");
                    fprintf (stderr, "  path %.4X (%d) (%d, %d) already exists in the tree.\n", code, size, i, j);
                    exit (1);
                }
                node->children[0] = malloc (sizeof (struct huffman_tree_node));
                node = node->children[0];
                node->leaf = 1;
                node->symbol = (i << 4) | j;
            }
        }
    }
}


void create_huffman_dc_tree (struct huffman_tree_node* root)
{
    uint16_t size, code;
    struct huffman_tree_node* node;
    for (int i = 0; i < 12; i ++)
    {
        node = root;
        size = huffman_dc[i][0];
        code = huffman_dc[i][1];
        code <<= 16 - size;

        // create intermediate path through tree
        for (int x = 0; x < size - 1; x ++)
        {
            if (code & 0x8000)
            {
                if (node->children[1] == NULL)
                {
                    node->children[1] = malloc (sizeof (struct huffman_tree_node));
                    node = node->children[1];
                    node->leaf  = 0;
                    node->children[0] = node->children[1] = NULL;
                }
                else
                    node = node->children[1];
            }
            else
            {
                if (node->children[0] == NULL)
                {
                    node->children[0] = malloc (sizeof (struct huffman_tree_node));
                    node = node->children[0];
                    node->leaf  = 0;
                    node->children[0] = node->children[1] = NULL;
                }
                else
                    node = node->children[0];
            }
            code <<= 1;
        }
        // create leaf node.
        if (code & 0x8000)
        {
            if (node->children[1] != NULL)
            {
                fprintf (stderr, "oops!!\n");
                exit (1);
            }
            node->children[1] = malloc (sizeof (struct huffman_tree_node));
            node = node->children[1];
            node->leaf = 1;
            node->symbol = i;
        }
        else
        {
            if (node->children[0] != NULL)
            {
                fprintf (stderr, "oops!!\n");
                exit (1);
            }
            node->children[0] = malloc (sizeof (struct huffman_tree_node));
            node = node->children[0];
            node->leaf = 1;
            node->symbol = i;
        }
    }
}

/**
 *  Descend a Huffman tree and return the symbol in the leaf.
 */
uint8_t decode_huffman_value (struct huffman_tree_node* tree, uint8_t* buffer, int* p)
{
    struct huffman_tree_node* node = tree;
    uint8_t b = buffer[(*p) >> 3];
    b <<= (*p) & 7; // b <<= p - p % 8

    while (!node->leaf)
    {
        node = node->children[(b >> 7) & 1];
        b <<= 1;
        *p += 1;
        if (((*p) & 7) == 0) // mod 8
            b = buffer[(*p) >> 3]; // next byte
    }
    return node->symbol;
}


void encode_huffman_dc_value (uint16_t amplitude, uint8_t* destination, int* p)
{
    uint8_t size;
    SIZE (amplitude);
    write_bits (huffman_dc[size][1], huffman_dc[size][0], destination, p);
    if (size)
        write_bits (amplitude, size, destination, p);
}


void encode_huffman_ac_value (uint16_t amplitude, uint8_t run_length, uint8_t* destination, int* p)
{
    uint8_t size;
    SIZE (amplitude);
    write_bits (huffman_ac[run_length][size][1],
                huffman_ac[run_length][size][0],
                destination,
                p);
    if (size)
        write_bits (amplitude, size, destination, p);
}


void destroy_huffman_tree (struct huffman_tree_node* root)
{
    if (root->children[1] && root->children[1]->leaf)
    {
        free (root->children[1]);
        root->children[1] = NULL;
    }

    if (root->children[0] && root->children[0]->leaf)
    {
        free (root->children[0]);
        root->children[0] = NULL;
    }

    if (root->children[1])
        destroy_huffman_tree (root->children[1]);
    else if (root->children[0])
        destroy_huffman_tree (root->children[0]);
    free (root);
}
